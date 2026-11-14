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
} xenr_vertex_t;

typedef struct {
    float mvp[16], modelview[16], uv[4], fog_color[4], params[4], flags[4];
} xenr_uniforms_t;

typedef struct xenr_mesh_s xenr_mesh_t;

xenr_mesh_t *
XENR_CreateMesh(const xenr_vertex_t * vertices, size_t count);

void
XENR_DestroyMesh(xenr_mesh_t * mesh);

void
XENR_DrawMesh(xenr_mesh_t * mesh, const unsigned short * indices, size_t count, const xenr_uniforms_t * uniforms);

void
XENR_DrawIndexed(const xenr_vertex_t * vertices, size_t vertex_count, const unsigned short * indices,
  size_t count, const xenr_uniforms_t * uniforms);

void
XENR_BeginFrame(void);

void
XENR_Draw(unsigned primitive, const xenr_vertex_t *, size_t count, const xenr_uniforms_t *);

void
XENR_Enable(unsigned cap);

void
XENR_Disable(unsigned cap);

void
XENR_BlendFunc(unsigned src, unsigned dst);

void
XENR_DepthMask(int enabled);

void
XENR_DepthFunc(unsigned function);

void
XENR_DepthRange(float near_value, float far_value);

void
XENR_PolygonOffset(float factor, float units);

void
XENR_Viewport(int x, int y, int width, int height);

void
XENR_CullFace(unsigned face);

void
XENR_ClearColor(float r, float g, float b, float a);

void
XENR_ClearDepth(float depth);

void
XENR_Clear(unsigned mask);

void
XENR_Finish(void);

int
XENR_ReadPixels(int x, int y, int width, int height, unsigned format, unsigned type,
  void * pixels);

unsigned
XENR_GetError(void);

unsigned
XENR_CreateTexture(void);

void
XENR_BindTexture(unsigned texture);

void
XENR_DestroyTexture(unsigned texture);

void
XENR_TextureFilter(int min_filter, int mag_filter);

void
XENR_Mipmaps(int enabled);

void
XENR_Lightmap(int enabled);

void
XENR_Upload(int level, int storage, int width, int height, int x, int y, int replace,
  const void * pixels);

// Values retained for the unchanged common screenshot call. No GL library is used.
enum {
    XENR_BACK                   = 0x0405,
    XENR_BLEND                  = 0x0BE2,
    XENR_COLOR_BUFFER_BIT       = 0x00004000,
    XENR_CULL_FACE              = 0x0B44,
    XENR_DEPTH_BUFFER_BIT       = 0x00000100,
    XENR_DEPTH_TEST             = 0x0B71,
    XENR_DST_COLOR              = 0x0306,
    XENR_EQUAL                  = 0x0202,
    XENR_FRONT                  = 0x0404,
    XENR_GEQUAL                 = 0x0206,
    XENR_INVALID_OPERATION      = 0x0502,
    XENR_INVALID_VALUE          = 0x0501,
    XENR_LEQUAL                 = 0x0203,
    XENR_LINEAR                 = 0x2601,
    XENR_LINEAR_MIPMAP_LINEAR   = 0x2703,
    XENR_LINEAR_MIPMAP_NEAREST  = 0x2701,
    XENR_LINE_STRIP             = 0x0003,
    XENR_NEAREST                = 0x2600,
    XENR_NEAREST_MIPMAP_LINEAR  = 0x2702,
    XENR_NEAREST_MIPMAP_NEAREST = 0x2700,
    XENR_ONE                    = 1,
    XENR_ONE_MINUS_SRC_ALPHA    = 0x0303,
    XENR_ONE_MINUS_SRC_COLOR    = 0x0301,
    XENR_OUT_OF_MEMORY          = 0x0505,
    XENR_POLYGON_OFFSET_FILL    = 0x8037,
    XENR_RGB                    = 0x1907,
    XENR_RGBA                   = 0x1908,
    XENR_SRC_ALPHA              = 0x0302,
    XENR_SRC_COLOR              = 0x0300,
    XENR_STENCIL_BUFFER_BIT     = 0x00000400,
    XENR_TRIANGLES              = 0x0004,
    XENR_TRIANGLE_FAN           = 0x0006,
    XENR_TRIANGLE_STRIP         = 0x0005,
    XENR_UNSIGNED_BYTE          = 0x1401,
    XENR_ZERO                   = 0,
};
#define GL_RGB           XENR_RGB
#define GL_UNSIGNED_BYTE XENR_UNSIGNED_BYTE
#define glReadPixels     XENR_ReadPixels
#define glGetError       XENR_GetError
#include "../../../render/r_local.h"
void
X360_HyenaInit(void);

void
X360_HyenaShutdown(void);
