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
#include "../xbox_local.h"
#include <hal/video.h>
#include <pbkit/pbkit.h>

unsigned d_8to24table[256];
unsigned char d_15to8table[65536];
int texture_mode = GL_LINEAR;
double gldepthmin, gldepthmax;
qboolean isPermedia = true;
static int screen_width, screen_height, screen_refresh;
static qboolean video_initialized;
static const byte * front_buffer;
extern cvar_t vid_widescreen;
cvar_t vid_overscan = { "vid_overscan", "0", CVAR_ARCHIVE };

void
VID_SetPalette(unsigned char * palette)
{
    int i;

    for (i = 0; i < 256; ++i)
        d_8to24table[i] = 0xff000000u | palette[i * 3]
          | (palette[i * 3 + 1] << 8) | (palette[i * 3 + 2] << 16);
    d_8to24table[255] &= 0x00ffffff;
}

void VID_ShiftPalette(unsigned char * palette){ (void) palette; }

void
VID_Init(unsigned char * palette)
{
    int parameter = COM_CheckParm("-width");
    int width     = parameter && parameter + 1 < com_argc ? Q_atoi(com_argv[parameter + 1]) : 1280;
    VIDEO_MODE mode;

    if (width != 640 && width != 1280)
        Sys_Error("Xbox video width must be 640 or 1280");
    if (width == 640 || !XVideoSetMode(1280, 720, 32, REFRESH_60HZ)) {
        if (!XVideoSetMode(640, 480, 32, REFRESH_DEFAULT))
            Sys_Error("Could not select an Xbox video mode");
    }
    mode                = XVideoGetMode();
    screen_width        = mode.width;
    screen_height       = mode.height;
    screen_refresh      = mode.refresh;
    vid_overscan.string = screen_height <= 480 ? "5" : "0";
    Cvar_RegisterVariable(&vid_overscan);
    // SCR_Init registers this shared cvar after VID_Init.
    vid_widescreen.string = screen_width == 1280 ||
      (XVideoGetEncoderSettings() & VIDEO_WIDESCREEN) ? "1" : "0";
    glContextInit(screen_width, screen_height);
    video_initialized = true;
    glSwapInterval(r_vsync.value != 0);
    glClearDepthf(1);
    glClearColor(0, 0, 0, 1);
    glCullFace(GL_FRONT);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.666f);
    glShadeModel(GL_FLAT);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    vid.width         = screen_width;
    vid.height        = screen_height;
    vid.maxwarpwidth  = screen_width;
    vid.maxwarpheight = screen_height;
    vid.colormap      = host_colormap;
    vid.fullbright    = 256 - LittleLong(*((int *) vid.colormap + 2048));
    vid.aspect        = ((float) vid.height / vid.width) * (320.0f / 240.0f);
    vid.scale         = ((float) vid.height / STD_UI_HEIGHT) * 0.875f;
    if (vid.scale < 1)
        vid.scale = 1;
    vid.conwidth  = vid.width;
    vid.conheight = vid.height;
    vid.numpages  = 2;
    Cvar_SetValue("cl_maxfps", 60);
    VID_SetPalette(palette);
} /* VID_Init */

void
GL_BeginRendering(int * x, int * y, int * width, int * height)
{
    int logical_width, viewport_width, inset_x, inset_y;
    GLfloat clear_color[4];


    Hyena_XboxBeginFrame();

    // 720p is a widescreen signal. Pillarbox 4:3; 480-line widescreen is anamorphic.
    viewport_width = screen_width == 1280 && !vid_widescreen.value ? 960 : screen_width;
    logical_width  = screen_width == 640 && vid_widescreen.value ?
      (int) (screen_height * (16.0f / 9.0f) + 0.5f) : viewport_width;
    if (vid.width != logical_width) {
        vid.width         = vid.conwidth = logical_width;
        vid.aspect        = ((float) vid.height / vid.width) * (320.0f / 240.0f);
        vid.recalc_refdef = true;
    }
    inset_x = (int) (viewport_width * bound(0, vid_overscan.value, 10) * 0.01f + 0.5f);
    inset_y = (int) (screen_height * bound(0, vid_overscan.value, 10) * 0.01f + 0.5f);
    *x      = (screen_width - viewport_width) / 2 + inset_x;
    *y      = inset_y;
    *width  = viewport_width - inset_x * 2;
    *height = screen_height - inset_y * 2;
    // Clear the entire buffer, including the pillars when screen mode changes.
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
    glScissor(*x, *y, *width, *height);
    glEnable(GL_SCISSOR_TEST);
} /* GL_BeginRendering */

void
GL_EndRendering(void)
{
    Draw_Flush();
    glSwapInterval(r_vsync.value != 0);
    front_buffer = (const byte *) pb_back_buffer();
    glFlipNV2A();
    vid.vsync_rate = r_vsync.value ? screen_refresh : 0;
}

qboolean
VID_XboxReadFrontPixels(int x, int y, int width, int height, byte * rgba)
{
    int row, col;
    unsigned pitch = pb_back_buffer_pitch();

    if (!front_buffer || x < 0 || y < 0 || width < 0 || height < 0 ||
      x > screen_width - width || y > screen_height - height)
        return false;

    glFinish();
    for (row = 0; row < height; ++row) {
        const byte * source = front_buffer + (screen_height - y - row - 1) * pitch + x * 4;
        byte * target       = rgba + row * width * 4;

        for (col = 0; col < width; ++col) {
            target[col * 4]     = source[col * 4 + 2];
            target[col * 4 + 1] = source[col * 4 + 1];
            target[col * 4 + 2] = source[col * 4];
            target[col * 4 + 3] = source[col * 4 + 3];
        }
    }
    return true;
}

void
VID_Shutdown(void)
{
    if (!video_initialized)
        return;

    glFinish();
    Hyena_XboxShutdown();
    pb_kill();
    video_initialized = false;
    vid.vsync_rate = 0;
}
