/*
 * Copyright (C) 1996-2001 Id Software, Inc.
 * Copyright (C) 2002-2009 John Fitzgibbons and others
 * Copyright (C) 2010-2014 QuakeSpasm developers
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
// r_main.h -- Agnostic render header
#ifndef _RENDER_MAIN_H_
#define _RENDER_MAIN_H_

#include "r_types.h"
#include "r_color_quantization.h"
#include "r_entity_fragments.h"
#include "r_light.h"
#include "r_fog.h"

void Hyena_Graphics_SetTextureMode(int texture_mode);
void Hyena_Graphics_SetColor(float red, float green, float blue, float alpha);
void Hyena_Graphics_EnableCapability(int capability);
void Hyena_Graphics_DisableCapability(int capability);
void Hyena_Graphics_DepthMask(bool value);
void Hyena_Graphics_BeginVertices(int mode);
void Hyena_Graphics_Translate(float x, float y, float z);
void Hyena_Graphics_Scale(float x, float y, float z);
void Hyena_Graphics_RotateXYZ(float x, float y, float z);
void Hyena_Graphics_RotateZYX(float z, float y, float x);
void Hyena_Graphics_FlushMatrices(void);
vertex_t *Hyena_Graphics_AllocateMemoryForVertices(int num_vertices);
void Hyena_Graphics_2DTextureCoord(vertex_t *vertex, float u, float v);
void Hyena_Graphics_VertexXYZ(vertex_t *vertex, float x, float y, float z);
void Hyena_Graphics_DrawVertices(vertex_t* vertices, int num_vertices, int texture_precision, int vertex_precision);
void Hyena_Graphics_EndVertices(void);
void Hyena_Graphics_SetShadeMode(int shade_mode);
void Hyena_Graphics_SetBlendFunction(int source_blend, int dest_blend);
void Hyena_Graphics_SetDepthRange(float near, float far);

#endif // _RENDER_MAIN_H_