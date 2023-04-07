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

#ifndef SDL_shaders_wiiu_h
#define SDL_shaders_wiiu_h

#if SDL_VIDEO_RENDER_WIIU

#include <gx2/shaders.h>

typedef enum {
    SHADER_INVALID = -1,
    SHADER_COLOR,
    SHADER_TEXTURE,
    NUM_SHADERS
} WIIU_ShaderType;

typedef struct {
   GX2FetchShader fetchShader;
   void* fetchShaderProgram;
   GX2VertexShader* vertexShader;
   GX2PixelShader* pixelShader;
} WiiU_ShaderGroup;

void WIIU_SDL_CreateShaders(void);
void WIIU_SDL_DestroyShaders(void);
void WIIU_SDL_SelectShader(WIIU_ShaderType shader);
WiiU_ShaderGroup* WIIU_SDL_GetShaderGroup(WIIU_ShaderType shader);

#endif /* SDL_VIDEO_RENDER_WIIU */

#endif /* SDL_shaders_wiiu_h */
