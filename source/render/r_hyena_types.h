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
// r_hyena_types.h -- Hyena rendering types
#ifndef _R_HYENA_TYPES_H_
#define _R_HYENA_TYPES_H_

// MARK: Vertex Definition

/** @brief Two-dimensional texture coordinate. */
typedef struct {
    float u, v;
} vertex_uv_t;

/** @brief Three-dimensional object-space position. */
typedef struct {
    float x, y, z;
} vertex_xyz_t;

/** @brief Vertex data shared by Hyena backends. */
typedef struct {
    vertex_uv_t  uv;
    vertex_xyz_t xyz;
} vertex_t;

typedef byte col_t[4];

// MARK: Booleans

#define HYE_FALSE false
#define HYE_TRUE  true

// MARK: Texture Modes

#define HYE_REPLACE             0
#define HYE_MODULATE            1
#define HYE_SRC_ALPHA           2
#define HYE_ONE_MINUS_SRC_ALPHA 3
#define HYE_ONE                 4
#define HYE_ONE_MINUS_SRC_COLOR 5

// MARK: Shade Modes

#define HYE_SMOOTH 0
#define HYE_FLAT   1

// MARK: Capabilities

#define HYE_BLEND      0
#define HYE_TEXTURE_2D 1
#define HYE_CULL_FACE  2

// MARK: Triangle Primitives

#define HYE_QUADS        0
#define HYE_TRIANGLE_FAN 1

// MARK: Vertex Precision

#define HYE_VERTEX_32BITFLOAT 0

// MARK: Texture Precision

#define HYE_TEXTURE_NOTEXTURE  0
#define HYE_TEXTURE_32BITFLOAT 1

// MARK: Math

#define HYE_PI 3.141593f

#endif // _R_HYENA_TYPES_H_
