/*
 * Copyright (C) 2023 NZ:P Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */
// r_types.h -- Type definitions for renderer abstraction layer.
#ifndef _RENDER_TYPES_H_
#define _RENDER_TYPES_H_

//
//
// MARK: Vertex Definition
//
//

typedef struct {
    float u, v;
} vertex_uv_t;

typedef struct {
    float x, y, z;
} vertex_xyz_t;

typedef struct {
    vertex_uv_t uv; 
    vertex_xyz_t xyz;
} vertex_t;

typedef	byte col_t[4];

//
//
// MARK: Booleans
//
//

#define GFX_FALSE                   false
#define GFX_TRUE                    true

//
//
// MARK: Texture Modes
//
//

#define GFX_REPLACE                 0
#define GFX_MODULATE                1
#define GFX_SRC_ALPHA               2
#define GFX_ONE_MINUS_SRC_ALPHA     3
#define GFX_ONE                     4
#define GFX_ONE_MINUS_SRC_COLOR     5

//
//
// MARK: Shade Modes
//
//

#define GFX_SMOOTH                  0
#define GFX_FLAT                    1

//
//
// MARK: Capabilities
//
//

#define GFX_BLEND                   0
#define GFX_TEXTURE_2D              1
#define GFX_CULL_FACE               2

//
//
// MARK: Triangle Primitives
//
//

#define GFX_QUADS                   0
#define GFX_TRIANGLE_FAN            1

//
//
// MARK: Vertex Precision
//
//

#define GFX_VERTEX_32BITFLOAT       0

//
//
// MARK: Texture Precision
//
//

#define GFX_TEXTURE_NOTEXTURE       0
#define GFX_TEXTURE_32BITFLOAT      1

//
//
// MARK: Math
//
//

#define GFX_PI                      3.141593f

#endif // _RENDER_TYPES_H_