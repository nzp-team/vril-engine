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
#include "../xbox360_local.h"
#include "xenos_main.h"

unsigned d_8to24table[256];
unsigned char d_15to8table[65536];
int texture_mode = XENR_LINEAR;
double gldepthmin, gldepthmax;
qboolean isPermedia    = true;
static float vid_gamma = 1.0f;
extern cvar_t r_vsync;
cvar_t vid_overscan = { "vid_overscan", "5", CVAR_ARCHIVE };

static void
MainFramebufferSize(int * width, int * height)
{
    *width  = 1280;
    *height = 720;
}

void
GL_BeginRendering(int * x, int * y, int * width, int * height)
{
    XENR_BeginFrame();
    *x = *y = 0;
    MainFramebufferSize(width, height);
}

void
GL_EndRendering(void)
{
    Draw_Flush();
    X360_AudioUpdate();
    vid.vsync_rate = r_vsync.value ? 60 : 0;

    if (!X360_GraphicsSwap(r_vsync.value != 0))
        Sys_Error("X360: Xenos presentation failed");
    X360_AudioUpdate();
}

void
VID_SetPalette(unsigned char * palette)
{
    unsigned i;

    for (i = 0; i < 256; ++i)
        d_8to24table[i] =
          ((unsigned) palette[i * 3] << 24) | (palette[i * 3 + 1] << 16) | (palette[i * 3 + 2] << 8) | 255u;

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
    if (!X360_GraphicsInit())
        Sys_Error("X360: %s", X360_GraphicsError());

    Cvar_RegisterVariable(&vid_overscan);
    X360_HyenaInit();
    vid.maxwarpwidth  = vid.width = 1280;
    vid.maxwarpheight = vid.height = 720;
    vid.colormap      = host_colormap;
    vid.fullbright    = 256 - LittleLong(*((int *) vid.colormap + 2048));
    vid.conwidth      = 640;
    vid.conheight     = 360;
    vid.aspect        = ((float) vid.height / vid.width) * (320.0f / 240.0f);
    vid.scale         = ((float) vid.height / STD_UI_HEIGHT) * 0.875f;
    vid.numpages      = 2;
    vid.vsync_rate    = 60;
    XENR_ClearDepth(1.0f);
    XENR_ClearColor(0, 0, 0, 1);
    XENR_CullFace(XENR_FRONT);
    XENR_BlendFunc(XENR_SRC_ALPHA, XENR_ONE_MINUS_SRC_ALPHA);
    Check_Gamma(palette);
    VID_SetPalette(palette);
    vid.recalc_refdef = true;
    Con_SafePrintf("X360: 1280x720 native Xenos initialized.\n");
}

void
VID_Shutdown(void)
{
    X360_HyenaShutdown();
    X360_GraphicsShutdown();
}
