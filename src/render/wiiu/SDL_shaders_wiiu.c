/*
  Simple DirectMedia Layer
  Copyright (C) 2022 GaryOderNichts <garyodernichts@gmail.com>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#if SDL_VIDEO_RENDER_WIIU

#include "SDL_shaders_wiiu.h"
#include "SDL_render.h"
#include "../SDL_sysrender.h"

#include <malloc.h>
#include <gfd.h>
#include <gx2/utils.h>
#include <gx2/mem.h>

#include "shaders/colorShader.inc"
#include "shaders/textureShader.inc"

static WiiU_ShaderGroup shaderGroups[NUM_SHADERS];
static int shaderRefCount = 0;

static GX2VertexShader* WiiU_LoadGFDVertexShader(uint32_t index, const void* file)
{
    uint32_t headerSize, programSize;
    GX2VertexShader* shader = NULL;
    void* program = NULL;

    if (index >= GFDGetVertexShaderCount(file)) {
        return NULL;
    }

    if ((headerSize = GFDGetVertexShaderHeaderSize(index, file)) == 0) {
        return NULL;
    }

    if ((programSize = GFDGetVertexShaderProgramSize(index, file)) == 0) {
        return NULL;
    }

    if (!(shader = memalign(0x40, headerSize))) {
        return NULL;
    }

    shader->gx2rBuffer.flags = GX2R_RESOURCE_BIND_SHADER_PROGRAM |
                                GX2R_RESOURCE_USAGE_CPU_READ |
                                GX2R_RESOURCE_USAGE_CPU_WRITE |
                                GX2R_RESOURCE_USAGE_GPU_READ;
    shader->gx2rBuffer.elemSize = programSize;
    shader->gx2rBuffer.elemCount = 1;
    shader->gx2rBuffer.buffer = NULL;
    if (!GX2RCreateBuffer(&shader->gx2rBuffer)) {
        free(shader);
        return NULL;
    }

    program = GX2RLockBufferEx(&shader->gx2rBuffer, GX2R_RESOURCE_BIND_NONE);
    if (!GFDGetVertexShader(shader, program, index, file)) {
        GX2RUnlockBufferEx(&shader->gx2rBuffer, GX2R_RESOURCE_DISABLE_CPU_INVALIDATE |
                                                GX2R_RESOURCE_DISABLE_GPU_INVALIDATE);
        GX2RDestroyBufferEx(&shader->gx2rBuffer, GX2R_RESOURCE_BIND_NONE);
        free(shader);
        return NULL;
    }

    GX2RUnlockBufferEx(&shader->gx2rBuffer, 0);
    // according to wut this needs to be invalidated again for some reason
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, shader->program, shader->size);
    return shader;
}

static GX2PixelShader* WiiU_LoadGFDPixelShader(uint32_t index, const void* file)
{
    uint32_t headerSize, programSize;
    GX2PixelShader* shader = NULL;
    void* program = NULL;

    if (index >= GFDGetPixelShaderCount(file)) {
        return NULL;
    }

    if ((headerSize = GFDGetPixelShaderHeaderSize(index, file)) == 0) {
        return NULL;
    }

    if ((programSize = GFDGetPixelShaderProgramSize(index, file)) == 0) {
        return NULL;
    }

    if (!(shader = memalign(0x40, headerSize))) {
        return NULL;
    }

    shader->gx2rBuffer.flags = GX2R_RESOURCE_BIND_SHADER_PROGRAM |
                                GX2R_RESOURCE_USAGE_CPU_READ |
                                GX2R_RESOURCE_USAGE_CPU_WRITE |
                                GX2R_RESOURCE_USAGE_GPU_READ;
    shader->gx2rBuffer.elemSize = programSize;
    shader->gx2rBuffer.elemCount = 1;
    shader->gx2rBuffer.buffer = NULL;
    if (!GX2RCreateBuffer(&shader->gx2rBuffer)) {
        free(shader);
        return NULL;
    }

    program = GX2RLockBufferEx(&shader->gx2rBuffer, GX2R_RESOURCE_BIND_NONE);
    if (!GFDGetPixelShader(shader, program, index, file)) {
        GX2RUnlockBufferEx(&shader->gx2rBuffer, GX2R_RESOURCE_DISABLE_CPU_INVALIDATE |
                                                GX2R_RESOURCE_DISABLE_GPU_INVALIDATE);
        GX2RDestroyBufferEx(&shader->gx2rBuffer, GX2R_RESOURCE_BIND_NONE);
        free(shader);
        return NULL;
    }

    GX2RUnlockBufferEx(&shader->gx2rBuffer, 0);
    // according to wut this needs to be invalidated again for some reason
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, shader->program, shader->size);
    return shader;
}

static int WiiU_LoadGFDShaderGroup(WiiU_ShaderGroup* group, uint32_t index, const void* file)
{
    group->vertexShader = WiiU_LoadGFDVertexShader(index, file);
    if (!group->vertexShader) {
        return -1;
    }

    group->pixelShader = WiiU_LoadGFDPixelShader(index, file);
    if (!group->pixelShader) {
        return -1;
    }

    return 0;
}

static int WiiU_CreateFetchShader(WiiU_ShaderGroup* group, GX2AttribStream* attributes, uint32_t numAttributes)
{
    uint32_t size = GX2CalcFetchShaderSizeEx(numAttributes, GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
    if (!size) {
        return -1;
    }

    group->fetchShaderProgram = memalign(GX2_SHADER_PROGRAM_ALIGNMENT, size);
    if (!group->fetchShaderProgram) {
        return -1;
    }

    GX2InitFetchShaderEx(&group->fetchShader,
                         group->fetchShaderProgram,
                         numAttributes,
                         attributes,
                         GX2_FETCH_SHADER_TESSELLATION_NONE,
                         GX2_TESSELLATION_MODE_DISCRETE);

    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, group->fetchShaderProgram, size);
    return 0;
}

static void WiiU_FreeShaderGroup(WiiU_ShaderGroup* group)
{
    GX2RDestroyBufferEx(&group->vertexShader->gx2rBuffer, GX2R_RESOURCE_BIND_NONE);
    free(group->vertexShader);

    GX2RDestroyBufferEx(&group->pixelShader->gx2rBuffer, GX2R_RESOURCE_BIND_NONE);
    free(group->pixelShader);

    free(group->fetchShaderProgram);
}

void WIIU_SDL_CreateShaders(void)
{
    if (!shaderRefCount++) {
        GX2AttribStream attributes[3];

        // a_position
        attributes[0] = (GX2AttribStream) { 
            0, 0, offsetof(SDL_VertexSolid, position),
            GX2_ATTRIB_FORMAT_FLOAT_32_32, GX2_ATTRIB_INDEX_PER_VERTEX, 0,
            GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_0, GX2_SQ_SEL_1),
            GX2_ENDIAN_SWAP_DEFAULT
        };
        // a_color
        attributes[1] = (GX2AttribStream) { 
            1, 0, offsetof(SDL_VertexSolid, color),
            GX2_ATTRIB_FORMAT_UNORM_8_8_8_8, GX2_ATTRIB_INDEX_PER_VERTEX, 0,
            GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_Z, GX2_SQ_SEL_W),
            GX2_ENDIAN_SWAP_DEFAULT
        };
        WiiU_LoadGFDShaderGroup(&shaderGroups[SHADER_COLOR], 0, colorShader_gsh);
        WiiU_CreateFetchShader(&shaderGroups[SHADER_COLOR], attributes, 2);

        // a_position
        attributes[0] = (GX2AttribStream) { 
            0, 0, offsetof(SDL_Vertex, position),
            GX2_ATTRIB_FORMAT_FLOAT_32_32, GX2_ATTRIB_INDEX_PER_VERTEX, 0,
            GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_0, GX2_SQ_SEL_1),
            GX2_ENDIAN_SWAP_DEFAULT
        };
        // a_color
        attributes[1] = (GX2AttribStream) { 
            1, 0, offsetof(SDL_Vertex, color),
            GX2_ATTRIB_FORMAT_UNORM_8_8_8_8, GX2_ATTRIB_INDEX_PER_VERTEX, 0,
            GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_Z, GX2_SQ_SEL_W),
            GX2_ENDIAN_SWAP_DEFAULT
        };
        // a_texcoord
        attributes[2] = (GX2AttribStream) { 
            2, 0, offsetof(SDL_Vertex, tex_coord),
            GX2_ATTRIB_FORMAT_FLOAT_32_32, GX2_ATTRIB_INDEX_PER_VERTEX, 0,
            GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_0, GX2_SQ_SEL_1),
            GX2_ENDIAN_SWAP_DEFAULT
        };
        WiiU_LoadGFDShaderGroup(&shaderGroups[SHADER_TEXTURE], 0, textureShader_gsh);
        WiiU_CreateFetchShader(&shaderGroups[SHADER_TEXTURE], attributes, 3);
    }
}

void WIIU_SDL_DestroyShaders(void)
{
    if (!--shaderRefCount) {
        WiiU_FreeShaderGroup(&shaderGroups[SHADER_COLOR]);
        WiiU_FreeShaderGroup(&shaderGroups[SHADER_TEXTURE]);
    }
}

void WIIU_SDL_SelectShader(WIIU_ShaderType shader)
{
    WiiU_ShaderGroup* shaderGroup = &shaderGroups[shader];

    GX2SetFetchShader(&shaderGroup->fetchShader);
    GX2SetVertexShader(shaderGroup->vertexShader);
    GX2SetPixelShader(shaderGroup->pixelShader);
}

WiiU_ShaderGroup* WIIU_SDL_GetShaderGroup(WIIU_ShaderType shader)
{
    return &shaderGroups[shader];
}

#endif //SDL_VIDEO_RENDER_WIIU
