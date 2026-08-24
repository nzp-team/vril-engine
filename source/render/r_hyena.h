// r_hyena.h -- Platform rendering interface

#ifndef _HYENA_H_
#define _HYENA_H_

#include "r_hyena_types.h"

/** @brief Selects how texture and vertex colors are combined. */
void
Hyena_SetTextureMode(int texture_mode);
/** @brief Sets the current RGBA vertex color. */
void
Hyena_SetColor(float red, float green, float blue, float alpha);
/** @brief Enables a rendering capability. */
void
Hyena_EnableCapability(int capability);
/** @brief Disables a rendering capability. */
void
Hyena_DisableCapability(int capability);
/** @brief Enables or disables depth-buffer writes. */
void
Hyena_DepthMask(qboolean enabled);
/** @brief Starts a vertex batch and resets its transform. */
void
Hyena_BeginVertices(int mode);
/** @brief Applies a translation to the current transform. */
void
Hyena_Translate(float x, float y, float z);
/** @brief Applies a scale to the current transform. */
void
Hyena_Scale(float x, float y, float z);
/** @brief Applies rotations in X, Y, Z order. */
void
Hyena_RotateXYZ(float x, float y, float z);
/** @brief Applies rotations in Z, Y, X order. */
void
Hyena_RotateZYX(float z, float y, float x);
/** @brief Submits pending transform changes to the backend. */
void
Hyena_FlushMatrices(void);
/** @brief Allocates transient storage for a vertex batch. */
vertex_t *
Hyena_AllocateMemoryForVertices(int num_vertices);
/** @brief Sets a vertex texture coordinate. */
void
Hyena_2DTextureCoord(vertex_t * vertex, float u, float v);
/** @brief Sets a vertex position. */
void
Hyena_VertexXYZ(vertex_t * vertex, float x, float y, float z);
/** @brief Draws and releases the current vertex batch. */
void
Hyena_DrawVertices(vertex_t * vertices, int num_vertices, int texture_precision, int vertex_precision);
/** @brief Ends the current vertex batch. */
void
Hyena_EndVertices(void);
/** @brief Selects flat or smooth shading. */
void
Hyena_SetShadeMode(int shade_mode);
/** @brief Selects source and destination blend factors. */
void
Hyena_SetBlendFunction(int source_blend, int destination_blend);
/** @brief Maps depth values into the requested range. */
void
Hyena_SetDepthRange(float near_value, float far_value);
/** @brief Applies a depth offset to subsequent draws. */
void
Hyena_SetDepthOffset(float offset);
/** @brief Binds a renderer texture handle. */
void
Hyena_BindTexture(int texture);

/** @brief Initializes platform fog state. */
void
Hyena_FogInit(void);
/** @brief Enables fog rendering. */
void
Hyena_FogEnable(void);
/** @brief Disables fog rendering. */
void
Hyena_FogDisable(void);
/** @brief Updates platform fog parameters. */
void
Hyena_FogSet(bool is_world_geometry, float start, float end,
  float red, float green, float blue, float alpha);

#endif // _HYENA_H_
