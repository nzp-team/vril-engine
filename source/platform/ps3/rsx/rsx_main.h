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
} rsxr_vertex_t;

typedef struct {
    float mvp[16], modelview[16], uv[4], fog_color[4], params[4], flags[4], vertex_state[4];
} rsxr_uniforms_t;

void
RSXR_BeginFrame(void);

void
RSXR_Enable(unsigned cap);

void
RSXR_Disable(unsigned cap);

void
RSXR_BlendFunc(unsigned src, unsigned dst);

void
RSXR_DepthMask(int enabled);

void
RSXR_DepthFunc(unsigned function);

void
RSXR_DepthRange(float near_value, float far_value);

void
RSXR_PolygonOffset(float factor, float units);

void
RSXR_Viewport(int x, int y, int width, int height);

void
RSXR_CullFace(unsigned face);

void
RSXR_ClearColor(float r, float g, float b, float a);

void
RSXR_ClearDepth(float depth);

void
RSXR_Clear(unsigned mask);

void
RSXR_Finish(void);

void
RSXR_SelectReadBuffer(int front);

int
RSXR_ReadPixels(int x, int y, int width, int height, unsigned format, unsigned type,
  void * pixels);

unsigned
RSXR_GetError(void);

unsigned
RSXR_CreateTexture(void);

void
RSXR_BindTexture(unsigned texture);

void
RSXR_DestroyTexture(unsigned texture);

void
RSXR_TextureFilter(int min_filter, int mag_filter);

void
RSXR_Mipmaps(int enabled);

void
RSXR_Upload(int level, int storage, int width, int height, int x, int y, int replace,
  const void * pixels);

void
RSXR_DynamicTexture(int enabled);

enum {
    RSXR_BACK                   = 0x0405,
    RSXR_BLEND                  = 0x0BE2,
    RSXR_COLOR_BUFFER_BIT       = 0x00004000,
    RSXR_CULL_FACE              = 0x0B44,
    RSXR_DEPTH_BUFFER_BIT       = 0x00000100,
    RSXR_DEPTH_TEST             = 0x0B71,
    RSXR_DST_COLOR              = 0x0306,
    RSXR_EQUAL                  = 0x0202,
    RSXR_FRONT                  = 0x0404,
    RSXR_GEQUAL                 = 0x0206,
    RSXR_INVALID_OPERATION      = 0x0502,
    RSXR_INVALID_VALUE          = 0x0501,
    RSXR_LEQUAL                 = 0x0203,
    RSXR_LINEAR                 = 0x2601,
    RSXR_LINEAR_MIPMAP_LINEAR   = 0x2703,
    RSXR_LINEAR_MIPMAP_NEAREST  = 0x2701,
    RSXR_LINE_STRIP             = 0x0003,
    RSXR_NEAREST                = 0x2600,
    RSXR_NEAREST_MIPMAP_LINEAR  = 0x2702,
    RSXR_NEAREST_MIPMAP_NEAREST = 0x2700,
    RSXR_ONE                    = 1,
    RSXR_ONE_MINUS_SRC_ALPHA    = 0x0303,
    RSXR_ONE_MINUS_SRC_COLOR    = 0x0301,
    RSXR_OUT_OF_MEMORY          = 0x0505,
    RSXR_POLYGON_OFFSET_FILL    = 0x8037,
    RSXR_RGB                    = 0x1907,
    RSXR_RGBA                   = 0x1908,
    RSXR_SRC_ALPHA              = 0x0302,
    RSXR_SRC_COLOR              = 0x0300,
    RSXR_STENCIL_BUFFER_BIT     = 0x00000400,
    RSXR_TRIANGLES              = 0x0004,
    RSXR_TRIANGLE_FAN           = 0x0006,
    RSXR_TRIANGLE_STRIP         = 0x0005,
    RSXR_UNSIGNED_BYTE          = 0x1401,
    RSXR_ZERO                   = 0,
};

#define GL_RGB           RSXR_RGB
#define GL_UNSIGNED_BYTE RSXR_UNSIGNED_BYTE
#define glReadPixels     RSXR_ReadPixels
#define glGetError       RSXR_GetError
#include "../../../render/r_local.h"
void
PS3_HyenaInit(void);

void
PS3_HyenaShutdown(void);

struct hyena_buffer_s {
    int      target;
    size_t   size;
    void *   data;
    qboolean owned;
    void *   gpu;
    unsigned gpu_offset;
    size_t   gpu_capacity;
};

void
RSXR_UploadBuffer(hyena_buffer_t * buffer);
void
RSXR_FreeBuffer(hyena_buffer_t * buffer);
void
RSXR_Submit(int mode, const hyena_arrays_t * arrays, int count, const unsigned short * indices,
  const rsxr_uniforms_t * uniforms, const float color[4], int flat);

void
RSXR_ParticlesBegin(void);
void
RSXR_ParticleType(int half_resolution);
void
RSXR_ParticlesEnd(void);
