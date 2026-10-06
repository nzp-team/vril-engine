// Copyright (C) 2023 NZ:P Team
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

// r_types.h -- Hyena rendering types
#ifndef _R_TYPES_H_
#define _R_TYPES_H_

// Two-dimensional texture coordinate.

typedef struct {
    float u, v;
} vertex_uv_t;

// Three-dimensional object-space position.

typedef struct {
    float x, y, z;
} vertex_xyz_t;

// Vertex data shared by Hyena backends.

typedef struct {
    vertex_uv_t  uv;
    vertex_xyz_t xyz;
} vertex_t;

typedef struct {
    float uv[2];
    byte  color[4];
    float xyz[3];
} hyena_colored_vertex_t;


// An axis-aligned quad consumed by the platform 2D backend.

typedef struct {
    float x0, y0, x1, y1;
    float u0, v0, u1, v1;
    byte  r, g, b, a;
} hyena_2d_quad_t;

typedef enum { HYE_TEXTURE_INDEX8, HYE_TEXTURE_RGBA8 } hyena_texture_format_t;

typedef enum { HYE_FILTER_NEAREST, HYE_FILTER_LINEAR } hyena_texture_filter_t;

typedef struct {
    const char *           identifier;
    const void *           pixels;
    const void *           palette;
    const void *           palette_hint; // optional 16-entry RGBA reduction palette

    int                    width;
    int                    height;
    int                    update_x;
    int                    update_y;
    int                    update_width;
    int                    update_height;
    int                    row_stride;
    hyena_texture_format_t format;
    hyena_texture_filter_t filter;
    int                    mip_levels;
    int                    palette_bpp; // bytes per palette entry: 3 (RGB) or 4 (RGBA)

    qboolean               alpha;
    qboolean               smooth_alpha;
    qboolean               keep;
    qboolean               stretch_to_power_of_two;
    qboolean               lightmap;
    qboolean               update;
} hyena_texture_desc_t;

// Client arrays may refer to host memory or an offset in a buffer.
typedef struct hyena_buffer_s hyena_buffer_t;
typedef struct {
    hyena_buffer_t * buffer;
    const void *     data;
    int              type;
    int              stride;
} hyena_array_t;

typedef struct {
    hyena_array_t position, uv, color;
    int           count;
    float         scale;
    unsigned int  flags;
} hyena_arrays_t;

#define HYE_FLOAT               0
#define HYE_SHORT               1
#define HYE_UBYTE               2
#define HYE_ARRAY_BUFFER        0
#define HYE_INDEX_BUFFER        1
#define HYE_CLIENT_INDEX_BUFFER 2
#define HYE_ARRAY_POSITION      1
#define HYE_ARRAY_UV            2
#define HYE_ARRAY_COLOR         4
#define HYE_ALPHA_TEST          3
#define HYE_DEPTH_TEST          4
#define HYE_DST_COLOR           6
#define HYE_SRC_COLOR           7
#define HYE_EQUAL               0
#define HYE_LEQUAL              1
#define HYE_GREATER             2
#define HYE_LINE_STRIP          4
#define HYE_COPY_VERTICES       1
#define HYE_CLIP_POLYGON        2
#define HYE_PACKED_UV_FLOAT     1
#define HYE_PACKED_UV_SHORT     2
#define HYE_PACKED_XYZ_FLOAT    4
#define HYE_PACKED_XYZ_BYTE     8
#define HYE_PACKED_COLOR        16

typedef byte col_t[4];

#define HYE_FALSE               false
#define HYE_TRUE                true

#define HYE_REPLACE             0
#define HYE_MODULATE            1
#define HYE_SRC_ALPHA           2
#define HYE_ONE_MINUS_SRC_ALPHA 3
#define HYE_ONE                 4
#define HYE_ONE_MINUS_SRC_COLOR 5

#define HYE_SMOOTH              0
#define HYE_FLAT                1

#define HYE_BLEND               0
#define HYE_TEXTURE_2D          1
#define HYE_CULL_FACE           2

#define HYE_QUADS               0
#define HYE_TRIANGLE_FAN        1
#define HYE_TRIANGLES           2
#define HYE_TRIANGLE_STRIP      3

#define HYE_VERTEX_32BITFLOAT   0

#define HYE_TEXTURE_NOTEXTURE   0
#define HYE_TEXTURE_32BITFLOAT  1

#define HYE_2D_TEXTURED         (1 << 0)
#define HYE_2D_BLEND            (1 << 1)
#define HYE_2D_LINEAR           (1 << 2)

#define HYE_PI                  3.141593f

#endif // _R_TYPES_H_
