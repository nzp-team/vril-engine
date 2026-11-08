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
static float *r_alias_shadow_positions;
static int r_alias_shadow_capacity;

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
    size_t               bytes;
    hyena_buffer_t *      vertices;
    struct alias_pose_s * next;
} alias_pose_t;
typedef struct alias_arrays_s {
    const void *            key;
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
            if (cache->key == batch->key && cache->commands == batch->commands)
                break;
        if (!cache) {
            cache = calloc(1, sizeof(*cache));
            if (!cache)
                Sys_Error("R_DrawAliasBatch: out of memory");
            cache->key = batch->key;
            cache->commands = batch->commands;
            cache->uvs      = Hyena_CreateBuffer(HYE_ARRAY_BUFFER, batch->uvs, batch->num_vertices * 2 * sizeof(float));
            cache->indices  = Hyena_CreateBuffer(HYE_CLIENT_INDEX_BUFFER, batch->indices,
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
                pose->bytes       = size;
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

static void R_InvalidateAliasArrays(const void *key)
{
    alias_arrays_t **link = &alias_arrays;
    while (*link) {
        alias_arrays_t *cache = *link;
        alias_pose_t *pose, *next;
        if (cache->key != key) {
            link = &cache->next;
            continue;
        }
        *link = cache->next;
        for (pose = cache->poses; pose; pose = next) {
            next = pose->next;
            alias_pose_bytes -= pose->bytes;
            Hyena_DestroyBuffer(pose->vertices);
            free(pose);
        }
        Hyena_DestroyBuffer(cache->uvs);
        Hyena_DestroyBuffer(cache->indices);
        free(cache);
    }
}

static alias_topology_t *
R_AliasTopology(const void *key, const int *commands, const unsigned short *remap, int vertices)
{
    alias_topology_t * topology;
    const int * scan;
    int count, vertex_base = 0, index = 0;

    for (topology = r_alias_topologies; topology; topology = topology->next) {
        if (topology->key != key)
            continue;
        if (topology->commands == commands)
            return topology;

        R_InvalidateAliasArrays(key);
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
    if (remap)
        topology->num_vertices = vertices;
    if (!topology->num_vertices)
        return topology;
    topology->uvs     = malloc(topology->num_vertices * 2 * sizeof(*topology->uvs));
    topology->indices = malloc(topology->num_indices * sizeof(*topology->indices));
    if (!topology->uvs || !topology->indices)
        Sys_Error("R_AliasTopology: out of memory");
    while ((count = *commands++) != 0) {
        qboolean fan = count < 0;
        int i;

        if (fan)
            count = -count;
        for (i = 0; i < count; i++) {
            int vertex = remap ? remap[vertex_base + i] : vertex_base + i;
            memcpy(topology->uvs + vertex * 2, commands + i * 2, 2 * sizeof(*topology->uvs));
        }
        for (i = 0; i < count - 2; ++i) {
            int first, second, third = vertex_base + i + 2;
            if (fan) {
                first = vertex_base;
                second = vertex_base + i + 1;
            } else {
                first = vertex_base + i + (i & 1);
                second = vertex_base + i + 1 - (i & 1);
            }
            topology->indices[index++] = remap ? remap[first] : first;
            topology->indices[index++] = remap ? remap[second] : second;
            topology->indices[index++] = remap ? remap[third] : third;
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
    alias_topology_t * topology = R_AliasTopology(batch->key, commands, batch->remap, batch->num_vertices);
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
    batch.key           = commands;
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

int R_CompactAliasVertices(const int *commands, const int *order, int count,
    unsigned short *remap, int *compact)
{
    const int *scan = commands;
    int capacity = 1, vertices = 0, index = 0, primitive, i;
    int *storage, *slots, *uvs;
    while (capacity < count * 2)
        capacity *= 2;
    storage = malloc((capacity + count * 2) * sizeof(*storage));
    if (!storage)
        return 0;
    slots = storage;
    uvs = slots + capacity;
    memset(slots, 255, capacity * sizeof(*slots));
    while ((primitive = *scan++) != 0) {
        if (primitive < 0)
            primitive = -primitive;
        if (primitive < 3 || primitive > count - index)
            Sys_Error("Invalid alias commands");
        for (i = 0; i < primitive; i++, index++, scan += 2) {
            unsigned int hash = (unsigned int)order[index] * 16777619u;
            int slot, previous;
            hash = (hash ^ (unsigned int)scan[0]) * 16777619u;
            hash = (hash ^ (unsigned int)scan[1]) * 16777619u;
            slot = hash & (capacity - 1);
            while ((previous = slots[slot]) != -1) {
                if (order[previous] == order[index] && uvs[previous * 2] == scan[0] &&
                    uvs[previous * 2 + 1] == scan[1])
                    break;
                slot = (slot + 1) & (capacity - 1);
            }
            if (previous == -1) {
                slots[slot] = index;
                compact[vertices] = order[index];
                remap[index] = vertices++;
            } else
                remap[index] = remap[previous];
            memcpy(uvs + index * 2, scan, 2 * sizeof(*uvs));
        }
    }
    if (index != count)
        Sys_Error("Invalid alias vertex count");
    free(storage);
    return vertices;
}

static int R_EqualAliasPoses(const trivertx_t *first, const trivertx_t *second,
    const int *order, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        int vertex = order ? order[i] : i;
        if (memcmp(first + vertex, second + vertex, sizeof(*first)))
            return 0;
    }
    return 1;
}

int R_StoreAliasPoses(const void *header, const trivertx_t * const *poses,
    int pose_count, int vertex_count, const int *order, int *posemap)
{
    unsigned int *hashes = NULL;
    unsigned short *unique = NULL, *mapping = NULL, *slots = NULL;
    int capacity = 1, physical = pose_count, pose, vertex;
    trivertx_t *data;
    *posemap = 0;
    if (pose_count > 1 && pose_count < 65535) {
        while (capacity < pose_count * 2)
            capacity *= 2;
        hashes = malloc(pose_count * (sizeof(*hashes) + 4) + capacity * 2);
        if (hashes) {
            unique = (unsigned short *)(hashes + pose_count);
            mapping = unique + pose_count;
            slots = mapping + pose_count;
            memset(slots, 255, capacity * sizeof(*slots));
            physical = 0;
            for (pose = 0; pose < pose_count; pose++) {
                unsigned int hash = 2166136261u;
                int slot;
                for (vertex = 0; vertex < vertex_count; vertex++) {
                    const byte *bytes = (const byte *)&poses[pose][order ? order[vertex] : vertex];
                    size_t i;
                    for (i = 0; i < sizeof(trivertx_t); i++)
                        hash = (hash ^ bytes[i]) * 16777619u;
                }
                slot = hash & (capacity - 1);
                while (slots[slot] != 65535) {
                    int match = slots[slot];
                    if (hashes[match] == hash && R_EqualAliasPoses(poses[pose], poses[unique[match]], order, vertex_count))
                        break;
                    slot = (slot + 1) & (capacity - 1);
                }
                if (slots[slot] == 65535) {
                    hashes[physical] = hash;
                    unique[physical] = pose;
                    slots[slot] = physical++;
                }
                mapping[pose] = slots[slot];
            }
            if ((size_t)(pose_count - physical) * vertex_count * sizeof(trivertx_t) <=
                pose_count * sizeof(*mapping) + 32)
                physical = pose_count;
        }
    }
    if (physical < pose_count) {
        unsigned short *stored = Hunk_Alloc(pose_count * sizeof(*stored));
        memcpy(stored, mapping, pose_count * sizeof(*stored));
        *posemap = (byte *)stored - (const byte *)header;
    }
    data = Hunk_Alloc(physical * vertex_count * sizeof(*data));
    for (pose = 0; pose < physical; pose++) {
        const trivertx_t *source = poses[physical < pose_count ? unique[pose] : pose];
        for (vertex = 0; vertex < vertex_count; vertex++)
            data[pose * vertex_count + vertex] = source[order ? order[vertex] : vertex];
    }
    free(hashes);
    return (byte *)data - (const byte *)header;
}

trivertx_t *R_AliasPoseData(const void *header, int offset, int stride, int map, int pose)
{
    if (map)
        pose = ((const unsigned short *)((const byte *)header + map))[pose];
    return (trivertx_t *)((const byte *)header + offset) + pose * stride;
}

void R_DrawAliasMesh(const model_t *model, const int *commands, const unsigned short *remap,
    int vertices, const trivertx_t *first, const trivertx_t *second, float blend)
{
    alias_batch_t batch = {0};
    if (second && blend == 1) {
        first = second;
        second = NULL;
    } else if (second && (first == second || blend == 0))
        second = NULL;
    batch.key = model;
    batch.commands = commands;
    batch.remap = remap;
    batch.num_vertices = vertices;
    batch.pose1 = first;
    batch.pose2 = second;
    batch.blend = blend;
    R_BuildAliasBatch(commands, first, second, blend, &batch);
    R_DrawAliasBatch(&batch);
}

void R_DrawAliasShadowMesh(const void *key, const int *commands, const unsigned short *remap,
    int vertices, const trivertx_t *pose, const float *scale, const float *origin,
    const float *direction, float height)
{
    alias_topology_t *topology = R_AliasTopology(key, commands, remap, vertices);
    hyena_arrays_t arrays = {0};
    int i;
    if (!topology->num_indices)
        return;
    if (topology->num_vertices > r_alias_shadow_capacity) {
        float *positions = realloc(r_alias_shadow_positions, topology->num_vertices * 3 * sizeof(*positions));
        if (!positions)
            Sys_Error("Out of alias shadow memory");
        r_alias_shadow_positions = positions;
        r_alias_shadow_capacity = topology->num_vertices;
    }
    for (i = 0; i < topology->num_vertices; ++i) {
        float *out = r_alias_shadow_positions + i * 3;
        float z = pose[i].v[2] * scale[2] + origin[2] + height;
        out[0] = pose[i].v[0] * scale[0] + origin[0] - direction[0] * z;
        out[1] = pose[i].v[1] * scale[1] + origin[1] - direction[1] * z;
        out[2] = 1 - height;
    }
    arrays.position.data = r_alias_shadow_positions;
    arrays.position.type = HYE_FLOAT;
    arrays.position.stride = 3 * sizeof(float);
    arrays.count = topology->num_vertices;
    arrays.scale = 1;
    arrays.flags = HYE_COPY_VERTICES;
    if (!alias_client_indices)
        alias_client_indices = Hyena_CreateBuffer(HYE_CLIENT_INDEX_BUFFER, NULL, 0);
    Hyena_UpdateBuffer(alias_client_indices, topology->indices, topology->num_indices * sizeof(*topology->indices));
    Hyena_DrawElements(HYE_TRIANGLES, &arrays, topology->num_indices, alias_client_indices, 0);
}

