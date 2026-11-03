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

#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
    float position[3], uv[2], color[4];
} gnmr_vertex_t;

typedef struct {
    float mvp[16], modelview[16], uv[4], fog_color[4], params[4], flags[4];
} gnmr_uniforms_t;

void
GNMR_BeginFrame(void);

void
GNMR_Draw(unsigned primitive, const gnmr_vertex_t *, size_t count, const gnmr_uniforms_t *);

void
GNMR_Enable(unsigned cap);

void
GNMR_Disable(unsigned cap);

void
GNMR_BlendFunc(unsigned src, unsigned dst);

void
GNMR_DepthMask(int enabled);

void
GNMR_DepthFunc(unsigned function);

void
GNMR_DepthRange(float near_value, float far_value);

void
GNMR_PolygonOffset(float factor, float units);

void
GNMR_Viewport(int x, int y, int width, int height);

void
GNMR_CullFace(unsigned face);

void
GNMR_ClearColor(float r, float g, float b, float a);

void
GNMR_ClearDepth(float depth);

void
GNMR_Clear(unsigned mask);

void
GNMR_Finish(void);

int
GNMR_ReadPixels(int x, int y, int width, int height, unsigned format, unsigned type,
  void * pixels);

unsigned
GNMR_GetError(void);

unsigned
GNMR_CreateTexture(void);

void
GNMR_BindTexture(unsigned texture);

void
GNMR_DestroyTexture(unsigned texture);

void
GNMR_TextureFilter(int min_filter, int mag_filter);

void
GNMR_Mipmaps(int enabled);

void
GNMR_Upload(int level, int storage, int width, int height, int x, int y, int replace,
  const void * pixels);

void
GNMR_DynamicTexture(int enabled);

// Values retained for the unchanged common screenshot call. No GL library is used.
enum {
    GNMR_BACK                   = 0x0405,
    GNMR_BLEND                  = 0x0BE2,
    GNMR_COLOR_BUFFER_BIT       = 0x00004000,
    GNMR_CULL_FACE              = 0x0B44,
    GNMR_DEPTH_BUFFER_BIT       = 0x00000100,
    GNMR_DEPTH_TEST             = 0x0B71,
    GNMR_DST_COLOR              = 0x0306,
    GNMR_EQUAL                  = 0x0202,
    GNMR_FRONT                  = 0x0404,
    GNMR_GEQUAL                 = 0x0206,
    GNMR_INVALID_OPERATION      = 0x0502,
    GNMR_INVALID_VALUE          = 0x0501,
    GNMR_LEQUAL                 = 0x0203,
    GNMR_LINEAR                 = 0x2601,
    GNMR_LINEAR_MIPMAP_LINEAR   = 0x2703,
    GNMR_LINEAR_MIPMAP_NEAREST  = 0x2701,
    GNMR_LINE_STRIP             = 0x0003,
    GNMR_NEAREST                = 0x2600,
    GNMR_NEAREST_MIPMAP_LINEAR  = 0x2702,
    GNMR_NEAREST_MIPMAP_NEAREST = 0x2700,
    GNMR_ONE                    = 1,
    GNMR_ONE_MINUS_SRC_ALPHA    = 0x0303,
    GNMR_ONE_MINUS_SRC_COLOR    = 0x0301,
    GNMR_OUT_OF_MEMORY          = 0x0505,
    GNMR_POLYGON_OFFSET_FILL    = 0x8037,
    GNMR_RGB                    = 0x1907,
    GNMR_RGBA                   = 0x1908,
    GNMR_SRC_ALPHA              = 0x0302,
    GNMR_SRC_COLOR              = 0x0300,
    GNMR_STENCIL_BUFFER_BIT     = 0x00000400,
    GNMR_TRIANGLES              = 0x0004,
    GNMR_TRIANGLE_FAN           = 0x0006,
    GNMR_TRIANGLE_STRIP         = 0x0005,
    GNMR_UNSIGNED_BYTE          = 0x1401,
    GNMR_ZERO                   = 0,
};
#define GL_RGB           GNMR_RGB
#define GL_UNSIGNED_BYTE GNMR_UNSIGNED_BYTE
#define glReadPixels     GNMR_ReadPixels
#define glGetError       GNMR_GetError
#include "../../../render/r_local.h"
void
PS4_HyenaInit(void);

void
PS4_HyenaShutdown(void);
