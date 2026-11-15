/*
 * Copyright (C) 2026 NZ:P Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "../../../nzportable_def.h"
#include "../ps3_local.h"
#include "rsx_main.h"

unsigned d_8to24table[256];
unsigned char d_15to8table[65536];
int texture_mode = RSXR_LINEAR;
double gldepthmin, gldepthmax;
qboolean isPermedia    = true;
static float vid_gamma = 1.0f;
extern cvar_t r_vsync;

static void
MainFramebufferSize(int * width, int * height)
{
    *width  = 1920;
    *height = 1080;
}

void
GL_BeginRendering(int * x, int * y, int * width, int * height)
{
    RSXR_BeginFrame();
    *x = *y = 0;
    MainFramebufferSize(width, height);
}

void
GL_EndRendering(void)
{
    Draw_Flush();
    vid.vsync_rate = r_vsync.value != 0 ? 60 : 0;

    if (!PS3_GraphicsSwap(r_vsync.value != 0))
        Sys_Error("PS3: RSX presentation failed");
}

void
VID_SetPalette(unsigned char * palette)
{
    unsigned i;

    for (i = 0; i < 256; ++i)
        d_8to24table[i] = ((unsigned) palette[i * 3] << 24) | (palette[i * 3 + 1] << 16)
          | (palette[i * 3 + 2] << 8) | 255u;

    d_8to24table[255] &= 0xffffff00;
}

void
VID_ShiftPalette(unsigned char * palette)
{
    (void) palette;
}

static void
Check_Gamma(unsigned char * pal)
{
    unsigned char adjusted[768];
    int i, parameter = COM_CheckParm("-gamma");

    vid_gamma = parameter && parameter + 1 < com_argc ? Q_atof(com_argv[parameter + 1]) : 0.7f;

    for (i = 0; i < 768; ++i) {
        float value = powf((pal[i] + 1) / 256.0f, vid_gamma) * 255.0f + 0.5f;

        adjusted[i] = (unsigned char) (value < 0 ? 0 : value > 255 ? 255 : value);
    }

    memcpy(pal, adjusted, sizeof(adjusted));
}

void
VID_Init(unsigned char * palette)
{
    if (!PS3_GraphicsInit())
        Sys_Error("PS3: %s", PS3_GraphicsError());

    PS3_HyenaInit();
    vid.maxwarpwidth  = vid.width = 1920;
    vid.maxwarpheight = vid.height = 1080;
    vid.colormap      = host_colormap;
    vid.fullbright    = 256 - LittleLong(*((int *) vid.colormap + 2048));
    vid.conwidth      = 640;
    vid.conheight     = 360;
    vid.aspect        = ((float) vid.height / vid.width) * (320.0f / 240.0f);
    vid.scale         = ((float) vid.height / STD_UI_HEIGHT) * 0.875f;
    vid.numpages      = 2;
    vid.vsync_rate    = 60;
    RSXR_ClearDepth(1.0f);
    RSXR_ClearColor(0, 0, 0, 1);
    RSXR_CullFace(RSXR_FRONT);
    RSXR_BlendFunc(RSXR_SRC_ALPHA, RSXR_ONE_MINUS_SRC_ALPHA);
    Check_Gamma(palette);
    VID_SetPalette(palette);
    vid.recalc_refdef = true;
    Con_SafePrintf("PS3: 1920x1080 native RSX with FXAA initialized.\n");
}

void
VID_Shutdown(void)
{
    PS3_HyenaShutdown();
    PS3_GraphicsShutdown();
}
