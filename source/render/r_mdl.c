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
// r_mdl.c -- Shared alias-model batching

#include "../nzportable_def.h"

static alias_vertex_t *r_alias_vertices;
static unsigned short *r_alias_indices;
static int r_alias_vertex_capacity;
static int r_alias_index_capacity;

static void
R_BuildAliasBatch(const int *commands, const trivertx_t *pose1,
  const trivertx_t *pose2, float blend, alias_batch_t *batch)
{
    const int *scan = commands;
    int count, vertex_count = 0, index_count = 0;
    int vertex_base = 0, index = 0;

    while ((count = *scan++) != 0) {
        if (count < 0)
            count = -count;
        vertex_count += count;
        index_count += 3 * (count - 2);
        scan += count * 2;
    }
    if (!vertex_count)
        return;
    if (vertex_count > r_alias_vertex_capacity) {
        r_alias_vertices = realloc(r_alias_vertices,
          sizeof(*r_alias_vertices) * vertex_count);
        if (!r_alias_vertices)
            Sys_Error("R_BuildAliasBatch: out of memory");
        r_alias_vertex_capacity = vertex_count;
    }
    if (index_count > r_alias_index_capacity) {
        r_alias_indices = realloc(r_alias_indices,
          sizeof(*r_alias_indices) * index_count);
        if (!r_alias_indices)
            Sys_Error("R_BuildAliasBatch: out of memory");
        r_alias_index_capacity = index_count;
    }
    batch->vertices = r_alias_vertices;
    batch->indices = r_alias_indices;
    batch->num_vertices = vertex_count;
    batch->num_indices = index_count;

    while ((count = *commands++) != 0) {
        qboolean fan = count < 0;
        int i;
        if (fan)
            count = -count;
        for (i = 0; i < count; ++i) {
            alias_vertex_t *out = &batch->vertices[vertex_base + i];
            memcpy(&out->uv[0], &commands[i * 2], sizeof(float));
            memcpy(&out->uv[1], &commands[i * 2 + 1], sizeof(float));
            out->xyz[0] = (short)((pose1[i].v[0] + (pose2 ? blend * (pose2[i].v[0] - pose1[i].v[0]) : 0)) * 128.0f);
            out->xyz[1] = (short)((pose1[i].v[1] + (pose2 ? blend * (pose2[i].v[1] - pose1[i].v[1]) : 0)) * 128.0f);
            out->xyz[2] = (short)((pose1[i].v[2] + (pose2 ? blend * (pose2[i].v[2] - pose1[i].v[2]) : 0)) * 128.0f);
            out->pad = 0;
        }
        for (i = 0; i < count - 2; ++i) {
            if (fan) {
                batch->indices[index++] = vertex_base;
                batch->indices[index++] = vertex_base + i + 1;
            } else {
                batch->indices[index++] = vertex_base + i + (i & 1);
                batch->indices[index++] = vertex_base + i + 1 - (i & 1);
            }
            batch->indices[index++] = vertex_base + i + 2;
        }
        commands += count * 2;
        pose1 += count;
        if (pose2)
            pose2 += count;
        vertex_base += count;
    }
}

void
R_DrawAliasCommands(const int *commands, const trivertx_t *pose1,
  const trivertx_t *pose2, float blend, qboolean packed_static,
  int command_words)
{
    alias_batch_t batch;

    memset(&batch, 0, sizeof(batch));
    batch.commands = commands;
    batch.pose1 = pose1;
    batch.pose2 = pose2;
    batch.blend = blend;
    batch.packed_static = packed_static;
    batch.command_words = command_words;
    if (!command_words)
        R_BuildAliasBatch(commands, pose1, pose2, blend, &batch);
    Hyena_DrawAliasBatch(&batch);
}
