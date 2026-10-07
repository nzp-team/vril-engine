/*
 * Copyright (C) 1996-1997 Id Software, Inc.
 * Copyright (C) 2026 NZ:P Team
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
// r_surface.h -- Shared surface submission

#ifndef _R_SURFACE_H_
#define _R_SURFACE_H_

#include <stddef.h>

typedef struct {
    float uv[2];
    float lightmap_uv[2];
    float xyz[3];
} world_vertex_t;

// Describes the model loader's retained polygon layout, without changing its storage.
typedef struct {
    size_t   vertices_offset;
    int      stride, position_offset, uv_offset, lightmap_offset;
    size_t   clipped_vertices_offset, clipped_count_offset;
    int      unbatched_flags, unlit_flags;
    int      frustum_planes;
    qboolean alpha_test_all;
    cvar_t * lightmap_debug, * wireframe;
} r_world_layout_t;

#ifdef __cplusplus
extern "C" {
#endif

void
R_DrawSurfaceFan(const float * vertices, int count, int stride,
  int position_offset, int texture_offset, qboolean warp, double time);

void
R_ClearLightmapChains(void);
void
R_ChainLightmap(msurface_t * surface);
void
R_UpdateSurfaceLightmap(msurface_t * surface);
void
R_BlendLightmaps(void);
void
R_BuildWorldBatch(const r_world_layout_t * layout);
void
R_ClearWorldBatchCache(void);
void
R_WorldBatchStats(int * base_batches, int * lightmap_batches,
  int * base_indices, int * lightmap_indices);
int
R_UploadLightmap(int index);
void
R_MarkVisibleLeaves(model_t * model, mleaf_t * leaf, int frame, qboolean all_visible);
void
R_MarkLeaves(qboolean all_visible, qboolean mirror_view);

#ifdef __cplusplus
}
#endif

#endif
