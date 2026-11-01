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
// r_mdl.h -- Shared alias-model batching

#ifndef _R_MDL_H_
#define _R_MDL_H_

typedef struct {
    short *                positions;
    const void *           key;
    const unsigned short * remap;
    const float *          uvs;
    const unsigned short * indices;
    int                    num_vertices;
    int                    num_indices;
    const int *            commands;
    const trivertx_t *     pose1;
    const trivertx_t *     pose2;
    float                  blend;
    qboolean               packed_static;
    int                    command_words;
} alias_batch_t;

#ifdef __cplusplus
extern "C" {
#endif

void
R_DrawAliasCommands(const int * commands, const trivertx_t * pose1,
  const trivertx_t * pose2, float blend, qboolean packed_static,
  int command_words);
int R_CompactAliasVertices(const int *commands, const int *order, int count,
    unsigned short *remap, int *compact);
int R_StoreAliasPoses(const void *header, const trivertx_t * const *poses,
    int pose_count, int vertex_count, const int *order, int *posemap);
trivertx_t *R_AliasPoseData(const void *header, int offset, int stride, int map, int pose);
#define R_AliasPose(header, pose) R_AliasPoseData(header, (header)->posedata, (header)->poseverts, (header)->posemap, pose)
void R_DrawAliasGeometry(aliashdr_t *header, const trivertx_t *first, const trivertx_t *second, float blend);
void R_DrawAliasShadowGeometry(aliashdr_t *header, const trivertx_t *pose, const float *direction, float height);
void R_DrawAliasMesh(const model_t *model, const int *commands, const unsigned short *remap,
    int vertices, const trivertx_t *first, const trivertx_t *second, float blend);
void R_DrawAliasShadowMesh(const void *key, const int *commands, const unsigned short *remap,
    int vertices, const trivertx_t *pose, const float *scale, const float *origin,
    const float *direction, float height);
void
R_ClearAliasTopologyCache(void);

#ifdef __cplusplus
}
#endif

#endif // ifndef _R_MDL_H_
