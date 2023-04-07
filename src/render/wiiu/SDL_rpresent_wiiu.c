/*
  Simple DirectMedia Layer
  Copyright (C) 2018-2019 Ash Logan <ash@heyquark.com>
  Copyright (C) 2018-2019 Roberto Van Eeden <r.r.qwertyuiop.r.r@gmail.com>
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

#include "../SDL_sysrender.h"
#include "SDL_render_wiiu.h"

#include <gx2/registers.h>
#include <gx2/state.h>
#include <gx2/draw.h>
#include <gx2/swap.h>
#include <gx2/display.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>

static SDL_bool tvDrcEnabled = SDL_FALSE;

void WIIU_SDL_RenderPresent(SDL_Renderer * renderer)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    WIIU_TextureData *tdata = (WIIU_TextureData *) data->windowTex.driverdata;
    Uint32 flags = SDL_GetWindowFlags(renderer->window);

    /* Only render to TV if the window is *not* drc-only */
    if (!(flags & SDL_WINDOW_WIIU_GAMEPAD_ONLY)) {
        GX2CopyColorBufferToScanBuffer(&tdata->cbuf, GX2_SCAN_TARGET_TV);
    }

    if (!(flags & SDL_WINDOW_WIIU_TV_ONLY)) {
        GX2CopyColorBufferToScanBuffer(&tdata->cbuf, GX2_SCAN_TARGET_DRC);
    }

    /* Swap buffers */
    GX2SwapScanBuffers();
    GX2Flush();

    /* Restore SDL context state */
    GX2SetContextState(data->ctx);

    /* TV and DRC can now be enabled after the first frame was drawn */
    if (!tvDrcEnabled) {
        GX2SetTVEnable(TRUE);
        GX2SetDRCEnable(TRUE);
        tvDrcEnabled = SDL_TRUE;
    }

    /* Wait for vsync */
    if (renderer->info.flags & SDL_RENDERER_PRESENTVSYNC) {
        uint32_t swap_count, flip_count;
        OSTime last_flip, last_vsync;
        uint32_t wait_count = 0;

        while (true) {
            GX2GetSwapStatus(&swap_count, &flip_count, &last_flip, &last_vsync);

            if (flip_count >= swap_count) {
                break;
            }

            if (wait_count >= 10) {
                /* GPU timed out */
                break;
            }

            wait_count++;
            GX2WaitForVsync();
        }
    }

    /* Free the list of render and draw data */
    WIIU_FreeRenderData(data);
    WIIU_TextureDoneRendering(data);
}

#endif /* SDL_VIDEO_RENDER_WIIU */
