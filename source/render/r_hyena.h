// Copyright (C) 2026 NZ:P Team
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

// r_hyena.h -- Platform graphics interface

#ifndef _HYENA_H_
#define _HYENA_H_

#include "r_types.h"

void Hyena_SelectDrawBuffer(qboolean front);
void Hyena_SelectReadBuffer(qboolean front);

void
Hyena_SetTextureMode(int texture_mode);
void
Hyena_SetColor(float red, float green, float blue, float alpha);
void
Hyena_SetCurrentColor(float red, float green, float blue, float alpha);
void
Hyena_EnableCapability(int capability);
void
Hyena_DisableCapability(int capability);
void
Hyena_DepthMask(qboolean enabled);
// BeginVertices resets the batch transform. DrawVertices consumes the allocation;
// callers must not reuse it after submission.

void
Hyena_BeginVertices(int mode);
void
Hyena_Translate(float x, float y, float z);
void
Hyena_Scale(float x, float y, float z);
void
Hyena_RotateXYZ(float x, float y, float z);
void
Hyena_RotateZYX(float z, float y, float x);
// Submits pending transform changes to the backend.

void
Hyena_FlushMatrices(void);
// Allocates transient storage for a vertex batch.

vertex_t *
Hyena_AllocateMemoryForVertices(int num_vertices);
void
Hyena_2DTextureCoord(vertex_t * vertex, float u, float v);
void
Hyena_VertexXYZ(vertex_t * vertex, float x, float y, float z);
void
Hyena_DrawVertices(vertex_t * vertices, int num_vertices, int texture_precision, int vertex_precision);

// Indexed draws borrow client memory for this call. Deferred backends copy it.
// At most 1,024 vertices; indices are unsigned 16-bit triangle lists.
void
Hyena_DrawIndexedTriangles(const hyena_colored_vertex_t * vertices, int vertex_count, const unsigned short * indices,
  int index_count, qboolean textured);

void
Hyena_EndVertices(void);
void
Hyena_SetShadeMode(int shade_mode);
void
Hyena_SetBlendFunction(int source_blend, int destination_blend);
void
Hyena_SetDepthRange(float near_value, float far_value);
void
Hyena_ResetDepthRange(void);
void
Hyena_SetDepthOffset(float offset);
void
Hyena_BindTexture(int texture);
// Creates or updates a texture from a platform-neutral description.

int
Hyena_CreateTexture(const hyena_texture_desc_t * desc);
// Convenience entry point for ordinary renderer-owned textures.

int
Hyena_LoadTexture(const char * identifier, int width, int height, const void * pixels,
  hyena_texture_format_t format, hyena_texture_filter_t filter, int mip_levels, qboolean alpha, qboolean keep,
  qboolean stretch_to_power_of_two);
// Loads indexed pixels with an explicit RGB or RGBA palette.

int
Hyena_LoadPalettedTexture(const char * identifier, int width, int height, const void * pixels, const void * palette,
  int palette_bpp, const void * palette_hint, hyena_texture_filter_t filter, int mip_levels, qboolean keep,
  qboolean stretch_to_power_of_two);
// Creates or updates a renderer lightmap.

int
Hyena_LoadLightmap(const char * identifier, int width, int height, const void * pixels,
  hyena_texture_format_t format, qboolean update);
int
Hyena_UpdateLightmap(const char * identifier, int width, int height, const void * pixels,
  hyena_texture_format_t format, int x, int y, int update_width, int update_height, int row_stride);
int
Hyena_FindTexture(const char * identifier);
// Prevents a texture from being released by bulk cleanup.

void
Hyena_KeepTexture(int texture);
void
Hyena_DestroyTexture(int texture);
void
Hyena_DestroyTextures(void);

void
Hyena_Set2D(void);
void
Hyena_InitTextures(void);
// Draws compatible axis-aligned quads in one backend submission.

void
Hyena_Draw2DQuads(const hyena_2d_quad_t * quads, int count, int texture, unsigned int flags);
void
Hyena_GetTextureSize(int texture, int * width, int * height, int * original_width, int * original_height);

void
Hyena_FogInit(void);
void
Hyena_FogEnable(void);
void
Hyena_FogDisable(void);
void
Hyena_FogSet(bool is_world_geometry, float start, float end, float red, float green, float blue, float alpha);

// ARRAY_BUFFER copies bytes into owned storage. INDEX_BUFFER streams indices,
// borrowing client memory on backends without index buffers. CLIENT_INDEX_BUFFER
// always borrows memory. Borrowed storage must survive deferred GPU execution.
hyena_buffer_t *
Hyena_CreateBuffer(int target, const void * data, size_t size);
void
Hyena_DestroyBuffer(hyena_buffer_t * buffer);
void *
Hyena_MapBuffer(hyena_buffer_t * buffer);
void
Hyena_UnmapBuffer(hyena_buffer_t * buffer);
void
Hyena_SetPalette(const unsigned int * rgba, int count);
void
Hyena_UpdateBuffer(hyena_buffer_t * buffer, const void * data, size_t size);
size_t
Hyena_StaticVertexBudget(void);
qboolean
Hyena_NeedsColorArray(void);
void
Hyena_BeginArrays(int mask);
void
Hyena_EndArrays(void);
void
Hyena_DrawArrays(int mode, const hyena_arrays_t * arrays, int count);
void
Hyena_DrawElements(int mode, const hyena_arrays_t * arrays, int count,
  hyena_buffer_t * indices, size_t offset);

// Optional packed-array operations. Check support before using native packed data.
void *
Hyena_AllocateTransient(size_t size);
void
Hyena_DrawPacked(int mode, int format, int morphs, int count,
  const void * vertices, const unsigned short * indices, unsigned int flags);
qboolean
Hyena_PackedArraysSupported(void);
qboolean
Hyena_SeparateArrays(void);
qboolean
Hyena_HardwareClipping(void);
qboolean
Hyena_SeparateViewMatrix(void);
void
Hyena_TextureAlpha(float alpha);
void
Hyena_MorphWeight(int index, float weight);
void
Hyena_TextureTransform(float uscale, float vscale, float uoffset, float voffset);
void
Hyena_DepthFunction(int function);
void
Hyena_AlphaFunction(int function, float value);
void
Hyena_TextureFilter(int filter);
void
Hyena_LoadViewMatrix(const float * matrix);
void
Hyena_InvalidateTextureBinding(void);
void
Hyena_RotationMatrix(float * matrix, float x, float y, float z);

void Hyena_InitTextureStorage(void);
size_t Hyena_TextureBudget(void);
int Hyena_PreferredPaletteBits(void);
void *Hyena_AllocateTexturePixels(size_t bytes);
void Hyena_FreeTexturePixels(void *pixels);
int Hyena_PreferredLightmapSize(void);
hyena_texture_format_t Hyena_PreferredLightmapFormat(void);
unsigned *Hyena_LightmapScratch(int count);
qboolean Hyena_AccumulateLightmap(unsigned *dest, const byte *source, int count, unsigned scale, qboolean add);
void Hyena_PrepareTextureUpdate(int texture);
void Hyena_ClipArrays(const hyena_arrays_t *source, hyena_arrays_t *clipped);
void Hyena_BeginModelClipping(void);
void Hyena_EndModelClipping(void);
void Hyena_MatrixMode(int mode);
void Hyena_LoadIdentity(void);
void Hyena_LoadMatrix(const float *matrix);
void Hyena_GetMatrix(float *matrix);
void Hyena_PushMatrix(void);
void Hyena_PopMatrix(void);
void Hyena_MatrixTranslate(float x, float y, float z);
void Hyena_MatrixScale(float x, float y, float z);
void Hyena_MatrixRotate(float degrees, float x, float y, float z);
void Hyena_UpdateMatrices(void);
void Hyena_Frustum(float left, float right, float bottom, float top, float near_value, float far_value);
void Hyena_Viewport(int x, int y, int width, int height);
void Hyena_ClipFrustum(float fovy, float aspect, float near_value);
void Hyena_CullFace(qboolean back);
void Hyena_ClearColor(float red, float green, float blue, float alpha);
void Hyena_Clear(qboolean color, qboolean depth, qboolean stencil);
void Hyena_DepthBounds(float near_value, float far_value);
void Hyena_PerspectiveCorrection(qboolean enabled);
void Hyena_BlendConstant(float alpha);
void Hyena_Finish(void);
qboolean Hyena_ReadPixels(int x, int y, int width, int height, byte *rgba);
void Hyena_ClearMorphMeshes(void);
qboolean Hyena_MorphMeshesSupported(void);
qboolean Hyena_DrawMorphMesh(const hyena_morph_mesh_t *mesh, const byte *first, const byte *second, float blend);
void Hyena_AddLightTime(double seconds);

#endif // _HYENA_H_
