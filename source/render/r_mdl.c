// Copyright (C) 1996-1997 Id Software, Inc.
// Copyright (C) 2026 NZ:P Team
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

// r_mdl.c -- Shared alias-model batching

#include "../nzportable_def.h"

extern entity_t * currententity;

static short * r_alias_positions;
static int r_alias_vertex_capacity;

typedef struct alias_topology_s {
    const void *              key;
    const int *               commands;
    float *                   uvs;
    unsigned short *          indices;
    int                       num_vertices;
    int                       num_indices;
    struct alias_topology_s * next;
} alias_topology_t;

static alias_topology_t * r_alias_topologies;

typedef struct alias_pose_s {
    const trivertx_t *    source;
    hyena_buffer_t *      vertices;
    struct alias_pose_s * next;
} alias_pose_t;
typedef struct alias_arrays_s {
    const int *             commands;
    hyena_buffer_t *        uvs, * indices;
    alias_pose_t *          poses;
    struct alias_arrays_s * next;
} alias_arrays_t;
static alias_arrays_t * alias_arrays;
static size_t alias_pose_bytes;
static hyena_buffer_t * alias_client_indices;

static void
R_ClearAliasArrays(void)
{
    alias_arrays_t * cache, * next;

    for (cache = alias_arrays; cache; cache = next) {
        alias_pose_t * pose, * next_pose;
        next = cache->next;
        for (pose = cache->poses; pose; pose = next_pose) {
            next_pose = pose->next;
            Hyena_DestroyBuffer(pose->vertices);
            free(pose);
        }
        Hyena_DestroyBuffer(cache->uvs);
        Hyena_DestroyBuffer(cache->indices);
        free(cache);
    }
    Hyena_DestroyBuffer(alias_client_indices);
    alias_client_indices = NULL;
    alias_arrays         = NULL;
    alias_pose_bytes     = 0;
}

static void
R_DrawAliasBatch(const alias_batch_t * batch)
{
    if (batch->command_words) {
        if (!Hyena_PackedArraysSupported())
            Sys_Error("R_DrawAliasBatch: unsupported packed vertex format");
        typedef struct { int uv, xyz;
        } packed_vertex_t;
        const int * commands = batch->commands;
        const trivertx_t * pose1 = batch->pose1, * pose2 = batch->pose2;
        packed_vertex_t * output = NULL;
        int count, output_index = 0;
        // Packed static poses are already GU vertices; animated pairs need display-list storage.
        if (!batch->packed_static)
            output = Hyena_AllocateTransient(sizeof(*output) * batch->command_words * (pose2 ? 2 : 1));
        while ((count = *commands++) != 0) {
            int mode = count < 0 ? HYE_TRIANGLE_FAN : HYE_TRIANGLE_STRIP;
            int i;
            if (count < 0)
                count = -count;
            if (batch->packed_static) {
                Hyena_DrawPacked(mode, HYE_PACKED_UV_SHORT | HYE_PACKED_XYZ_BYTE, 1, count, commands, NULL, 0);
                commands += count * 2;
                continue;
            }
            for (i = 0; i < count; ++i) {
                output[output_index].uv    = *commands++;
                output[output_index++].xyz = ((const int *) pose1[i].v)[0];
                if (pose2) {
                    output[output_index].uv    = commands[-1];
                    output[output_index++].xyz = ((const int *) pose2[i].v)[0];
                }
            }
            if (pose2) {
                Hyena_MorphWeight(0, 1.0f - batch->blend);
                Hyena_MorphWeight(1, batch->blend);
            }
            Hyena_DrawPacked(mode, HYE_PACKED_UV_SHORT | HYE_PACKED_XYZ_BYTE, pose2 ? 2 : 1, count,
              &output[output_index - count * (pose2 ? 2 : 1)], NULL, 0);
            pose1 += count;
            if (pose2)
                pose2 += count;
        }
        return;
    }
    hyena_arrays_t arrays = { 0 };
    hyena_buffer_t * indices;
    alias_arrays_t * cache = NULL;
    size_t budget = Hyena_StaticVertexBudget();
    if (!batch->num_indices)
        return;

    arrays.count         = batch->num_vertices;
    arrays.scale         = 1.0f / 128.0f;
    arrays.position.type = HYE_SHORT;
    // The backend supplies a memory limit; the renderer owns pose keys and eviction.
    if (budget) {
        for (cache = alias_arrays; cache; cache = cache->next)
            if (cache->commands == batch->commands)
                break;
        if (!cache) {
            cache = calloc(1, sizeof(*cache));
            if (!cache)
                Sys_Error("R_DrawAliasBatch: out of memory");
            cache->commands = batch->commands;
            cache->uvs      = Hyena_CreateBuffer(HYE_ARRAY_BUFFER, batch->uvs, batch->num_vertices * 2 * sizeof(float));
            cache->indices  = Hyena_CreateBuffer(HYE_ARRAY_BUFFER, batch->indices,
                batch->num_indices * sizeof(unsigned short));
            cache->next  = alias_arrays;
            alias_arrays = cache;
        }
        arrays.uv.buffer = cache->uvs;
        indices = cache->indices;
    } else {
        if (!alias_client_indices)
            alias_client_indices = Hyena_CreateBuffer(HYE_CLIENT_INDEX_BUFFER, NULL, 0);
        Hyena_UpdateBuffer(alias_client_indices, batch->indices, batch->num_indices * sizeof(unsigned short));
        arrays.uv.data = batch->uvs;
        indices        = alias_client_indices;
    }
    arrays.position.data = batch->positions;
    if (!batch->positions && cache) {
        alias_pose_t * pose;
        size_t size = batch->num_vertices * 3 * sizeof(short);
        for (pose = cache->poses; pose; pose = pose->next)
            if (pose->source == batch->pose1)
                break;
        if (!pose) {
            short * positions;
            if (batch->num_vertices > r_alias_vertex_capacity) {
                r_alias_positions = realloc(r_alias_positions, size);
                if (!r_alias_positions)
                    Sys_Error("R_DrawAliasBatch: out of pose memory");
                r_alias_vertex_capacity = batch->num_vertices;
            }
            positions = r_alias_positions;
            int i;
            if (!positions)
                Sys_Error("R_DrawAliasBatch: out of pose memory");
            for (i = 0; i < batch->num_vertices; ++i) {
                positions[i * 3]     = batch->pose1[i].v[0] * 128;
                positions[i * 3 + 1] = batch->pose1[i].v[1] * 128;
                positions[i * 3 + 2] = batch->pose1[i].v[2] * 128;
            }
            // Once linear memory is full, keep using the same interpolation scratch.
            if (size <= budget - alias_pose_bytes) {
                pose = calloc(1, sizeof(*pose));
                if (!pose)
                    Sys_Error("R_DrawAliasBatch: out of pose memory");
                pose->source      = batch->pose1;
                pose->vertices    = Hyena_CreateBuffer(HYE_ARRAY_BUFFER, positions, size);
                pose->next        = cache->poses;
                cache->poses      = pose;
                alias_pose_bytes += size;
            } else {
                arrays.position.data = positions;
                Hyena_DrawElements(HYE_TRIANGLES, &arrays, batch->num_indices, indices, 0);
                return;
            }
        }
        arrays.position.buffer = pose->vertices;
    }
    Hyena_DrawElements(HYE_TRIANGLES, &arrays, batch->num_indices, indices, 0);
} /* R_DrawAliasBatch */

void
R_ClearAliasTopologyCache(void)
{
    alias_topology_t * topology, * next;

    R_ClearAliasArrays();
    for (topology = r_alias_topologies; topology; topology = next) {
        next = topology->next;
        free(topology->uvs);
        free(topology->indices);
        free(topology);
    }
    r_alias_topologies = NULL;
}

static alias_topology_t *
R_AliasTopology(const int * commands)
{
    alias_topology_t * topology;
    const void * key = currententity && currententity->model ?
      (const void *) currententity->model : (const void *) commands;
    const int * scan;
    int count, vertex_base = 0, index = 0;

    for (topology = r_alias_topologies; topology; topology = topology->next) {
        if (topology->key != key)
            continue;
        if (topology->commands == commands)
            return topology;

        free(topology->uvs);
        free(topology->indices);
        topology->uvs          = NULL;
        topology->indices      = NULL;
        topology->num_vertices = 0;
        topology->num_indices  = 0;
        break;
    }
    if (!topology) {
        topology = calloc(1, sizeof(*topology));
        if (!topology)
            Sys_Error("R_AliasTopology: out of memory");
        topology->key      = key;
        topology->next     = r_alias_topologies;
        r_alias_topologies = topology;
    }
    topology->commands = commands;
    for (scan = commands; (count = *scan++) != 0; scan += count * 2) {
        if (count < 0)
            count = -count;
        topology->num_vertices += count;
        topology->num_indices  += 3 * (count - 2);
    }
    if (topology->num_vertices) {
        topology->uvs     = malloc(topology->num_vertices * 2 * sizeof(*topology->uvs));
        topology->indices = malloc(topology->num_indices * sizeof(*topology->indices));
        if (!topology->uvs || !topology->indices)
            Sys_Error("R_AliasTopology: out of memory");
    }
    while ((count = *commands++) != 0) {
        qboolean fan = count < 0;
        int i;

        if (fan)
            count = -count;
        memcpy(topology->uvs + vertex_base * 2, commands,
          count * 2 * sizeof(*topology->uvs));
        for (i = 0; i < count - 2; ++i) {
            if (fan) {
                topology->indices[index++] = vertex_base;
                topology->indices[index++] = vertex_base + i + 1;
            } else {
                topology->indices[index++] = vertex_base + i + (i & 1);
                topology->indices[index++] = vertex_base + i + 1 - (i & 1);
            }
            topology->indices[index++] = vertex_base + i + 2;
        }
        commands    += count * 2;
        vertex_base += count;
    }
    return topology;
} /* R_AliasTopology */

static void
R_BuildAliasBatch(
    const int * commands, const trivertx_t * pose1, const trivertx_t * pose2, float blend, alias_batch_t * batch)
{
    alias_topology_t * topology = R_AliasTopology(commands);
    int i;

    if (!topology->num_vertices)
        return;

    batch->positions    = NULL;
    batch->uvs          = topology->uvs;
    batch->indices      = topology->indices;
    batch->num_vertices = topology->num_vertices;
    batch->num_indices  = topology->num_indices;
    if (!pose2 && (Hyena_StaticVertexBudget() != 0))
        return;

    if (topology->num_vertices > r_alias_vertex_capacity) {
        r_alias_positions = realloc(r_alias_positions,
            topology->num_vertices * 3 * sizeof(*r_alias_positions));
        if (!r_alias_positions)
            Sys_Error("R_BuildAliasBatch: out of memory");
        r_alias_vertex_capacity = topology->num_vertices;
    }
    batch->positions = r_alias_positions;
    for (i = 0; i < topology->num_vertices; ++i) {
        short * out = batch->positions + i * 3;

        if (pose2) {
            out[0] = (short) ((pose1[i].v[0] + blend * (pose2[i].v[0] - pose1[i].v[0])) * 128.0f);
            out[1] = (short) ((pose1[i].v[1] + blend * (pose2[i].v[1] - pose1[i].v[1])) * 128.0f);
            out[2] = (short) ((pose1[i].v[2] + blend * (pose2[i].v[2] - pose1[i].v[2])) * 128.0f);
        } else {
            out[0] = pose1[i].v[0] * 128;
            out[1] = pose1[i].v[1] * 128;
            out[2] = pose1[i].v[2] * 128;
        }
    }
} /* R_BuildAliasBatch */

void
R_DrawAliasCommands(const int * commands, const trivertx_t * pose1, const trivertx_t * pose2, float blend,
  qboolean packed_static, int command_words)
{
    alias_batch_t batch;

    memset(&batch, 0, sizeof(batch));
    batch.commands      = commands;
    batch.pose1         = pose1;
    batch.pose2         = pose2;
    batch.blend         = blend;
    batch.packed_static = packed_static;
    batch.command_words = command_words;
    if (!command_words)
        R_BuildAliasBatch(commands, pose1, pose2, blend, &batch);
    R_DrawAliasBatch(&batch);
}
