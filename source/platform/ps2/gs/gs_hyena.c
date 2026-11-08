// Copyright (C) 1996-1997 Id Software, Inc.
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

#include <screenshot.h>
#include "../../../nzportable_def.h"
#include "../../../render/r_texture.h"
#include "../../../render/r_pixels.h"
#include <malloc.h>
#include <sifcmd.h>

#include <dmaKit.h>
#include <gsInline.h>
#include <kernel.h>
#include <timer.h>
#include <math3d.h>
#include <math.h>
#include <stdint.h>

static void Hyena_AliasStrips(ps2_gs_alias_batch_t *batch)
{
    int first, count, runs = 0;
    for (first = 0; first < batch->triangle_count; first += count) {
        int lengths[2], kind;
        for (kind = 0; kind < 2; kind++) {
            unsigned int previous = batch->indices[first];
            for (count = 1; count < 48 && first + count < batch->triangle_count; count++) {
                unsigned int next = batch->indices[first + count];
                unsigned int a = kind || !(count & 1) ? previous & 255 : (previous >> 16) & 255;
                unsigned int b = kind || !(count & 1) ? (previous >> 16) & 255 : (previous >> 8) & 255;
                if ((next & 255) != a || ((next >> 8) & 255) != b)
                    break;
                previous = next;
            }
            lengths[kind] = count;
        }
        kind = lengths[1] > lengths[0];
        count = lengths[kind];
        batch->indices[first] |= (unsigned int)(count | (kind << 7)) << 24;
        runs++;
    }
    if ((batch->triangle_count + 2 * runs) * 3 + runs < batch->triangle_count * 9)
        batch->strip_count = runs;
}

int Hyena_BuildAliasBatch(ps2_gs_alias_batch_t *batch, const unsigned short *indices,
    const float *uvs, int count)
{
    int i, vertex, triangle_vertices = 0;
    if (count < 3 || count > PS2_GS_ALIAS_TRIANGLES * 3 || count % 3)
        return 0;
    memset(batch, 0, sizeof(*batch));
    for (i = 0; i < count; ++i) {
        if (!(i % 3))
            triangle_vertices = batch->vertex_count;
        for (vertex = 0; vertex < batch->vertex_count; ++vertex)
            if (batch->vertices[vertex] == indices[i] &&
                batch->uvs[vertex][0] == uvs[i * 2] && batch->uvs[vertex][1] == uvs[i * 2 + 1])
                break;
        if (vertex == batch->vertex_count) {
            if (vertex == PS2_GS_ALIAS_VERTICES) {
                batch->vertex_count = triangle_vertices;
                batch->indices[i / 3] = 0;
                batch->triangle_count = i / 3;
                memset(batch->uvs + triangle_vertices, 0,
                    sizeof(batch->uvs) - triangle_vertices * sizeof(batch->uvs[0]));
                Hyena_AliasStrips(batch);
                return batch->vertex_count;
            }
            batch->vertices[vertex] = indices[i];
            batch->uvs[vertex][0] = uvs[i * 2];
            batch->uvs[vertex][1] = uvs[i * 2 + 1];
            ++batch->vertex_count;
        }
        batch->indices[i / 3] |= (unsigned int)(vertex * 3) << ((i % 3) * 8);
    }
    batch->triangle_count = count / 3;
    Hyena_AliasStrips(batch);
    return batch->vertex_count;
}

#define PS2_GS_CLIP_VERTICES 32

static float Hyena_ClipDistance(const ps2_gs_vertex_t *vertex, int plane)
{
    const float *p = vertex->position;
    switch (plane) {
        case 0: return p[3] + p[0];
        case 1: return p[3] - p[0];
        case 2: return p[3] + p[1];
        case 3: return p[3] - p[1];
        case 4: return p[3] + p[2];
        case 5: return p[3] - p[2];
        default: return p[3] - 0.0001f;
    }
}

static void Hyena_ClipLerp(ps2_gs_vertex_t *output,
    const ps2_gs_vertex_t *a, const ps2_gs_vertex_t *b, float fraction)
{
    int i;
    for (i = 0; i < 4; ++i) {
        output->position[i] = a->position[i] + fraction * (b->position[i] - a->position[i]);
        output->color[i] = a->color[i] + fraction * (b->color[i] - a->color[i]);
    }
    for (i = 0; i < 2; ++i)
        output->uv[i] = a->uv[i] + fraction * (b->uv[i] - a->uv[i]);
}

int Hyena_ClipPolygon(const ps2_gs_vertex_t *vertices, int count,
    ps2_gs_vertex_t *output, int capacity)
{
    ps2_gs_vertex_t work[2][PS2_GS_CLIP_VERTICES];
    const ps2_gs_vertex_t *input;
    ps2_gs_vertex_t *destination;
    int source_count = count;
    int mask = 0;
    int source_buffer = 0;
    int plane, i;

    if (!vertices || !output || count < 3 || count > PS2_GS_CLIP_VERTICES || capacity < 3)
        return 0;
    for (plane = 0; plane < 7; ++plane) {
        for (i = 0; i < count; ++i) {
            if (Hyena_ClipDistance(&vertices[i], plane) < 0.0f) {
                mask |= (1 << plane);
                break;
            }
        }
    }
    if (!mask) {
        if (count > capacity)
            return 0;
        memcpy(output, vertices, (size_t)count * sizeof(*output));
        return count;
    }

    memcpy(work[0], vertices, (size_t)count * sizeof(*vertices));
    for (plane = 0; plane < 7; ++plane) {
        int written = 0;
        if (!(mask & (1 << plane)))
            continue;
        input = work[source_buffer];
        destination = work[source_buffer ^ 1];
        for (i = 0; i < source_count; ++i) {
            const ps2_gs_vertex_t *a = &input[i];
            const ps2_gs_vertex_t *b = &input[(i + 1) % source_count];
            float da = Hyena_ClipDistance(a, plane);
            float db = Hyena_ClipDistance(b, plane);
            int a_inside = da >= 0.0f;
            int b_inside = db >= 0.0f;
            if (a_inside) {
                if (written == PS2_GS_CLIP_VERTICES)
                    return 0;
                destination[written++] = *a;
            }
            if (a_inside != b_inside) {
                if (written == PS2_GS_CLIP_VERTICES)
                    return 0;
                Hyena_ClipLerp(&destination[written++], a, b, da / (da - db));
            }
        }
        source_count = written;
        if (source_count < 3)
            return 0;
        source_buffer ^= 1;
    }
    input = work[source_buffer];
    if (source_count > capacity)
        return 0;
    memcpy(output, input, (size_t)source_count * sizeof(*output));
    return source_count;
}

typedef struct world_cache_s {
    ps2_gs_world_batch_t batch;
    const void *positions, *uvs;
    int position_stride, uv_stride, count, vertex_limit;
    unsigned int hash, used, frame;
    unsigned short *indices;
    size_t size;
    struct world_cache_s *next, *hash_next;
} world_cache_t;

static world_cache_t *world_cache, *world_buckets[256];
static unsigned int world_used, world_count, world_frame, world_full_frame, world_bytes;

void Hyena_WorldFrame(void)
{
    world_frame++;
}

static void Hyena_UnlinkWorld(world_cache_t *entry)
{
    world_cache_t **link = &world_buckets[entry->hash & 255];
    while (*link != entry)
        link = &(*link)->hash_next;
    *link = entry->hash_next;
}

void Hyena_InvalidateWorld(const void *data, size_t size)
{
    world_cache_t **link = &world_cache, *entry;
    uintptr_t start = (uintptr_t)data, end = start + size;
    while ((entry = *link)) {
        if (!data || ((uintptr_t)entry->positions >= start && (uintptr_t)entry->positions < end) ||
            ((uintptr_t)entry->uvs >= start && (uintptr_t)entry->uvs < end)) {
            if (entry->frame == world_frame)
                VID_VU1WaitDMA();
            *link = entry->next;
            Hyena_UnlinkWorld(entry);
            world_bytes -= entry->size;
            free(entry);
            world_count--;
        } else {
            link = &entry->next;
        }
    }
}

const ps2_gs_world_batch_t *Hyena_CacheWorldBatch(ps2_gs_world_scratch_t *scratch,
    const unsigned char *positions, int position_stride, const unsigned char *uvs,
    int uv_stride, const unsigned short *indices, int first, int count, int vertex_limit)
{
    world_cache_t *entry, *oldest = NULL;
    unsigned int hash = 2166136261u;
    int i;
    size_t size, offset, positions_size, uvs_size, indices_size;
    if (count > 64)
        count = 64;
    indices += first * 3;
    for (i = 0; i < count * 3; ++i)
        hash = (hash ^ indices[i]) * 16777619u;
    ++world_used;
    for (entry = world_buckets[hash & 255]; entry; entry = entry->hash_next) {
        if (entry->hash == hash && entry->positions == positions && entry->uvs == uvs &&
            entry->position_stride == position_stride && entry->uv_stride == uv_stride &&
            entry->count == count && entry->vertex_limit == vertex_limit && !memcmp(entry->indices, indices, count * 6)) {
            entry->used = world_used;
            entry->frame = world_frame;
            return &entry->batch;
        }
    }
    Hyena_BuildWorldBatch(scratch, positions, position_stride, uvs, uv_stride, indices, 0, count, vertex_limit);
    positions_size = scratch->batch.vertex_count * 16;
    uvs_size = (scratch->batch.vertex_count * 8 + 15) & ~15;
    indices_size = (scratch->batch.triangle_count * 4 + 15) & ~15;
    offset = (sizeof(*entry) + count * 6 + 15) & ~15;
    size = (offset + positions_size + uvs_size + indices_size + 63) & ~63;
    while (world_bytes + size > 192 * 1024 || world_count >= 256) {
        if (world_full_frame == world_frame)
            return &scratch->batch;
        oldest = NULL;
        for (entry = world_cache; entry; entry = entry->next)
            if (world_frame - entry->frame > 1 && (!oldest || entry->used < oldest->used))
                oldest = entry;
        if (!oldest) {
            world_full_frame = world_frame;
            return &scratch->batch;
        }
        world_cache_t **link = &world_cache;
        while (*link != oldest)
            link = &(*link)->next;
        *link = oldest->next;
        Hyena_UnlinkWorld(oldest);
        world_bytes -= oldest->size;
        world_count--;
        free(oldest);
    }
    entry = memalign(64, size);
    if (!entry)
        return &scratch->batch;
    entry->size = size;
    entry->next = world_cache;
    world_cache = entry;
    world_count++;
    world_bytes += size;
    entry->indices = (unsigned short *)(entry + 1);
    entry->batch = scratch->batch;
    entry->batch.positions = (void *)((unsigned char *)entry + offset);
    entry->batch.uvs = (void *)((unsigned char *)entry + offset + positions_size);
    entry->batch.indices = (void *)((unsigned char *)entry + offset + positions_size + uvs_size);
    memcpy((void *)entry->batch.positions, scratch->positions, positions_size);
    memcpy((void *)entry->batch.uvs, scratch->uvs, uvs_size);
    memcpy((void *)entry->batch.indices, scratch->indices, indices_size);
    entry->positions = positions;
    entry->uvs = uvs;
    entry->position_stride = position_stride;
    entry->uv_stride = uv_stride;
    entry->count = count;
    entry->vertex_limit = vertex_limit;
    entry->hash = hash;
    entry->used = world_used;
    entry->frame = world_frame;
    entry->hash_next = world_buckets[hash & 255];
    world_buckets[hash & 255] = entry;
    memcpy(entry->indices, indices, count * 6);
    SifWriteBackDCache(entry, size);
    return &entry->batch;
}

#define PS2_GS_STACK_LIMIT 32
#define PS2_GS_BATCH_LIMIT 768

static GSGLOBAL *ps2_gs;
extern cvar_t gl_cull;
cvar_t ps2_async_flip = {"ps2_async_flip", "1"};
cvar_t ps2_alias_strips = {"ps2_alias_strips", "1"};
cvar_t ps2_vu1 = {"ps2_vu1", "1"};
cvar_t ps2_packet_cache = {"ps2_packet_cache", "1"};
cvar_t ps2_dma_cache = {"ps2_dma_cache", "1"};
cvar_t ps2_vu_batch = {"ps2_vu_batch", "1"};
cvar_t ps2_world_indexed = {"ps2_world_indexed", "1"};
cvar_t ps2_world_cache = {"ps2_world_cache", "1"};
cvar_t ps2_world_stream = {"ps2_world_stream", "1"};
cvar_t ps2_alias_cull = {"ps2_alias_cull", "1"};
cvar_t ps2_matrix_vu0 = {"ps2_matrix_vu0", "1"};
static int fog_enabled, fog_world;
static float fog_start, fog_end, fog_color[3];
static double ps2_frame_begin_time, ps2_present_wait;
static float projection[PS2_GS_STACK_LIMIT][16] __attribute__((aligned(16)));
static float model[PS2_GS_STACK_LIMIT][16] __attribute__((aligned(16)));
static float combined_matrix[16] __attribute__((aligned(16)));
static int matrix_dirty = 1;
static const ps2_gs_stream_t *current_stream;
static int projection_depth, model_depth;
static float texture_transform[4] = {1, 1, 0, 0};
static float current_color[4] = {1, 1, 1, 1};
static float clear_color[4];
static float depth_near = 0, depth_far = 1;
static GSTEXTURE *current_texture;
static GSTEXTURE *last_vu_texture;
static u64 last_vu_tex0;
static int last_vu_filter, last_vu_context;
static GSTEXTURE *current_lightmap[3];
static GSTEXTURE *resident_lightmap[3];
static unsigned int lightmap_vram, lightmap_plane_size, lightmap_slot_size;
static int lightmap_blending, lightmap_alpha = 1, lightmap_coherent;
static int blending, alpha_testing, depth_testing = 1;
static int depth_writes = 1, texturing = 1, texture_replace;
static int alpha_reference;
static int alpha_bit[2] = {-1, -1};
static GSPRIMSTQPOINT textured_batch[PS2_GS_BATCH_LIMIT];
static GSPRIMPOINT plain_batch[PS2_GS_BATCH_LIMIT];
static int batch_count;
static int batch_textured;
static unsigned int profile_vertices, profile_triangles, profile_batches, profile_lightmap_batches;
static unsigned int profile_static_batches, profile_static_vertices;
static double profile_static_seconds;
static double profile_geometry_seconds, profile_packing_seconds, profile_entity_light_seconds;
static unsigned long long profile_vif_wait_ticks, profile_gif_wait_ticks, profile_queue_ticks;

int Hyena_GSPacketCache(void)
{
    return ps2_packet_cache.value != 0;
}

int Hyena_GSRangeWriteback(void)
{
    return ps2_dma_cache.value != 0;
}

int Hyena_GSProfiling(void)
{
    return r_profile.value != 0;
}

void Hyena_GSAddGIFWaitTicks(unsigned int ticks)
{
    profile_gif_wait_ticks += ticks;
}

void Hyena_GSAddVIFWaitTicks(unsigned int ticks)
{
    profile_vif_wait_ticks += ticks;
}

void Hyena_GSAddEntityLightTime(double seconds)
{
    profile_entity_light_seconds += seconds;
}

void Hyena_GSAddPackingTime(double seconds)
{
    profile_packing_seconds += seconds;
}

static void Hyena_GSExecuteQueue(void)
{
    unsigned int start;
    if (VID_QueueEnabled()) {
        GSQUEUE *queue = ps2_gs->CurQueue;
        if (queue->tag_size) {
            *(u64 *)queue->dma_tag = DMA_TAG(queue->tag_size, 0, DMA_END, 0, 0, 0);
            if (queue->last_type != GIF_AD)
                *(u64 *)queue->last_tag |= queue->same_obj;
            __asm__ volatile("sync.l" ::: "memory");
            VID_QueueGIF(queue->pool[queue->dbuf], NULL, 0);
            gsKit_queue_reset(queue);
        }
        ps2_gs->FirstFrame = GS_SETTING_OFF;
        return;
    }
    VID_VU1Finish();
    start = cpu_ticks();
    if (ps2_gs->CurQueue->tag_size && !ps2_gs->FirstFrame) {
        unsigned int polls = 0;
        while (!(GS_CSR_FINISH)) {
            if (!(++polls & 1023) && cpu_ticks() - start > 2 * kBUSCLK) {
                Sys_PS2GPUError("GS FINISH");
            }
        }
    }
    gsKit_queue_exec(ps2_gs);
    if (r_profile.value)
        profile_queue_ticks += (unsigned int)(cpu_ticks() - start);
}

void Hyena_GSFlush(void)
{
    if (!ps2_gs)
        return;
    Hyena_GSExecuteQueue();
    VID_VU1Finish();
}

static void Hyena_GSReserveQueue(size_t bytes)
{
    if ((byte *)ps2_gs->CurQueue->pool_cur + bytes >=
        (byte *)ps2_gs->CurQueue->pool_max[ps2_gs->CurQueue->dbuf])
        Hyena_GSExecuteQueue();
}

static void Hyena_GSFrameMask(unsigned int mask)
{
    u64 *data;

    Hyena_GSReserveQueue(32);
    data = gsKit_heap_alloc(ps2_gs, 1, 16, GIF_AD);
    *data++ = GIF_TAG_AD(1);
    *data++ = GIF_AD;
    *data++ = GS_SETREG_FRAME(ps2_gs->ScreenBuffer[ps2_gs->ActiveBuffer & 1] / 8192,
        ps2_gs->Width / 64, ps2_gs->PSM, mask);
    *data++ = GS_FRAME_1 + ps2_gs->PrimContext;
}

static void Hyena_GSDepthWrite(int enabled)
{
    u64 *data;

    if (!ps2_gs)
        return;
    Hyena_GSReserveQueue(32);
    data = gsKit_heap_alloc(ps2_gs, 1, 16, GIF_AD);
    *data++ = GIF_TAG_AD(1);
    *data++ = GIF_AD;
    *data++ = GS_SETREG_ZBUF(ps2_gs->ZBuffer / 8192, ps2_gs->PSMZ, !enabled);
    *data++ = GS_ZBUF_1 + ps2_gs->PrimContext;
}

static void Hyena_GSAlphaBit(int enabled)
{
    u64 *data;
    int context;
    if (!ps2_gs)
        return;
    context = ps2_gs->PrimContext;
    enabled = !!enabled;
    if (alpha_bit[context] == enabled)
        return;
    Hyena_GSReserveQueue(32);
    data = gsKit_heap_alloc(ps2_gs, 1, 16, GIF_AD);
    *data++ = GIF_TAG_AD(1);
    *data++ = GIF_AD;
    *data++ = enabled;
    *data++ = GS_FBA_1 + context;
    alpha_bit[context] = enabled;
}

static void Hyena_GSIdentity(float m[16])
{
    int i;
    memset(m, 0, 16 * sizeof(float));
    for (i = 0; i < 4; ++i)
        m[i * 5] = 1;
}

static void Hyena_GSMultiply(float result[16], const float b[16])
{
    float a[16], c[16];
    int row, column, index;
    if (ps2_matrix_vu0.value) {
        matrix_multiply(result, (float *)b, result);
        return;
    }
    memcpy(a, result, sizeof(a));
    for (column = 0; column < 4; ++column)
        for (row = 0; row < 4; ++row) {
            float v = 0;
            for (index = 0; index < 4; ++index)
                v += a[index * 4 + row] * b[column * 4 + index];
            c[column * 4 + row] = v;
        }
    memcpy(result, c, sizeof(c));
}

static void Hyena_GSScale(float m[16], float x, float y, float z)
{
    int i;
    for (i = 0; i < 4; ++i) {
        m[i] *= x;
        m[4 + i] *= y;
        m[8 + i] *= z;
    }
}

static void Hyena_GSTranslate(float m[16], float x, float y, float z)
{
    int i;
    for (i = 0; i < 4; ++i)
        m[12 + i] += m[i] * x + m[4 + i] * y + m[8 + i] * z;
}

static void Hyena_GSRotate(float m[16], float angle, float x, float y, float z)
{
    float t[16] __attribute__((aligned(16)));
    float c, s, inv;
    float length;
    int first = -1, second = -1, i;
    if (angle == 0)
        return;
    if ((x == 1 || x == -1) && y == 0 && z == 0) {
        first = 1;
        second = 2;
        angle *= x;
    } else if ((y == 1 || y == -1) && x == 0 && z == 0) {
        first = 2;
        second = 0;
        angle *= y;
    } else if ((z == 1 || z == -1) && x == 0 && y == 0) {
        first = 0;
        second = 1;
        angle *= z;
    }
    if (first >= 0) {
        c = cosf(angle * (float)M_PI / 180.0f);
        s = sinf(angle * (float)M_PI / 180.0f);
        for (i = 0; i < 4; ++i) {
            float a = m[first * 4 + i], b = m[second * 4 + i];
            m[first * 4 + i] = a * c + b * s;
            m[second * 4 + i] = b * c - a * s;
        }
        return;
    }
    length = sqrtf(x*x + y*y + z*z);
    if (length < 0.00001f)
        return;
    x /= length;
    y /= length;
    z /= length;
    c = cosf(angle * (float)M_PI / 180.0f);
    s = sinf(angle * (float)M_PI / 180.0f);
    inv = 1.0f - c;
    Hyena_GSIdentity(t);
    t[0] = x*x*inv+c;
    t[1] = y*x*inv+z*s;
    t[2] = z*x*inv-y*s;
    t[4] = x*y*inv-z*s;
    t[5] = y*y*inv+c;
    t[6] = z*y*inv+x*s;
    t[8] = x*z*inv+y*s;
    t[9] = y*z*inv-x*s;
    t[10] = z*z*inv+c;
    Hyena_GSMultiply(m, t);
}

static float Hyena_GSLimit(float n)
{
    return n < 0 ? 0 : (n > 1 ? 1 : n);
}

static unsigned int Hyena_GSRGBA(const float color[4], int textured)
{
    float scale = textured ? 128.0f : 255.0f;
    int r = (int)(Hyena_GSLimit(color[0]) * scale + 0.5f);
    int g = (int)(Hyena_GSLimit(color[1]) * scale + 0.5f);
    int b = (int)(Hyena_GSLimit(color[2]) * scale + 0.5f);
    int a = (int)(Hyena_GSLimit(color[3]) * 128 + 0.5f);
    return GS_SETREG_RGBA(r, g, b, a);
}

static void Hyena_GSTransformVertex(const ps2_gs_vertex_t *input, ps2_gs_vertex_t *output)
{
    float clip[4];
    int i, j;
    for (i = 0; i < 4; ++i) {
        clip[i] = 0;
        for (j = 0; j < 4; ++j)
            clip[i] += combined_matrix[j * 4 + i] * input->position[j];
    }
    *output = *input;
    memcpy(output->position, clip, sizeof(clip));
    output->uv[0] = input->uv[0] * texture_transform[0] + texture_transform[2];
    output->uv[1] = input->uv[1] * texture_transform[1] + texture_transform[3];
    if (texture_replace && texturing && current_texture) {
        output->color[0] = output->color[1] = output->color[2] = 1.0f;
    }
}

static void Hyena_GSFlushBatch(void)
{
    size_t queue_bytes;
    int channel;
    if (!ps2_gs || !batch_count)
        return;
    last_vu_texture = NULL;
    if (r_profile.value) {
        profile_batches++;
        if (batch_textured && current_lightmap[0] && blending && lightmap_blending)
            profile_lightmap_batches++;
    }
    queue_bytes = (size_t)batch_count * (batch_textured ? sizeof(GSPRIMSTQPOINT) : sizeof(GSPRIMPOINT)) + 4096;
    Hyena_GSReserveQueue(queue_bytes);
    if (batch_textured && current_lightmap[0] && blending && lightmap_blending) {
        for (channel = 0; channel < 3; channel++) {
            Hyena_GSReserveQueue(queue_bytes);
            Hyena_GSFrameMask(~(0xffu << (channel * 8)));
            gsKit_TexManager_bind(ps2_gs, current_lightmap[channel]);
            gsKit_prim_list_triangle_goraud_texture_stq_3d(ps2_gs, current_lightmap[channel],
                batch_count, textured_batch);
        }
        Hyena_GSFrameMask(0);
    } else if (batch_textured && current_texture) {
        gsKit_TexManager_bind(ps2_gs, current_texture);
        gsKit_prim_list_triangle_goraud_texture_stq_3d(ps2_gs, current_texture,
            batch_count, textured_batch);
    } else {
        gsKit_prim_list_triangle_gouraud_3d(ps2_gs, batch_count, plain_batch);
    }
    batch_count = 0;
}

static void Hyena_GSClipTriangle(const ps2_gs_vertex_t original[3])
{
    ps2_gs_vertex_t clipped[32];
    const ps2_gs_vertex_t *vertices = original;
    int count = 3;
    int i, j, all_inside = 1;
    if (r_profile.value)
        profile_triangles++;
    for (i = 0; i < 3; ++i) {
        const float *p = original[i].position;
        float w = p[3];
        if (w <= 0.0001f || p[0] < -w || p[0] > w ||
            p[1] < -w || p[1] > w || p[2] < -w || p[2] > w) {
            all_inside = 0;
            break;
        }
    }
    if (!all_inside) {
        count = Hyena_ClipPolygon(original, 3, clipped, 32);
        vertices = clipped;
    }
    if (!count)
        return;
    for (i = 1; i + 1 < count; ++i) {
        const ps2_gs_vertex_t *verts[3] = {&vertices[0], &vertices[i], &vertices[i + 1]};
        if (batch_count + 3 > PS2_GS_BATCH_LIMIT)
            Hyena_GSFlushBatch();
        for (j = 0; j < 3; ++j) {
            const ps2_gs_vertex_t *vert = verts[j];
            float inverse_w = 1.0f / vert->position[3];
            float x = (vert->position[0] * inverse_w * 0.5f + 0.5f) * ps2_gs->Width;
            float y = (0.5f - vert->position[1] * inverse_w * 0.5f) * ps2_gs->Height;
            float z_normalized = 1.0f - (depth_near +
                (depth_far - depth_near) * (vert->position[2] * inverse_w * 0.5f + 0.5f));
            int z = (int)(z_normalized * 16777215.0f);
            unsigned int rgba = Hyena_GSRGBA(vert->color, batch_textured);
            if (batch_textured) {
                GSPRIMSTQPOINT *output = &textured_batch[batch_count + j];
                output->rgbaq = rgba_to_RGBAQ(rgba, inverse_w);
                output->stq = vertex_to_STQ(vert->uv[0] * inverse_w, vert->uv[1] * inverse_w);
                output->xyz2 = vertex_to_XYZ2(ps2_gs, x, y, z);
            } else {
                GSPRIMPOINT *output = &plain_batch[batch_count + j];
                output->rgbaq = rgba_to_RGBAQ(rgba, 0);
                output->xyz2 = vertex_to_XYZ2(ps2_gs, x, y, z);
            }
        }
        batch_count += 3;
    }
}

static float Hyena_GSClipDistance(const ps2_gs_vertex_t *v, int plane)
{
    const float *p = v->position;
    switch (plane) {
        case 0: return p[3] + p[0];
        case 1: return p[3] - p[0];
        case 2: return p[3] + p[1];
        case 3: return p[3] - p[1];
        case 4: return p[3] + p[2];
        case 5: return p[3] - p[2];
        default: return p[3] - 0.0001f;
    }
}

static void Hyena_GSDrawLine(const ps2_gs_vertex_t *start, const ps2_gs_vertex_t *end)
{
    ps2_gs_vertex_t clipped[2] = {*start, *end};
    float xy[2][2];
    int z[2], plane, i, j;
    for (plane = 0; plane < 7; ++plane) {
        float a = Hyena_GSClipDistance(&clipped[0], plane);
        float b = Hyena_GSClipDistance(&clipped[1], plane);
        if (a < 0 && b < 0)
            return;
        if ((a < 0) != (b < 0)) {
            float fraction = a / (a - b);
            ps2_gs_vertex_t interpolated;
            for (j = 0; j < 4; ++j) {
                interpolated.position[j] = clipped[0].position[j] +
                    fraction * (clipped[1].position[j] - clipped[0].position[j]);
                interpolated.color[j] = clipped[0].color[j] +
                    fraction * (clipped[1].color[j] - clipped[0].color[j]);
            }
            clipped[a < 0 ? 0 : 1] = interpolated;
        }
    }
    for (i = 0; i < 2; ++i) {
        float w = 1.0f / clipped[i].position[3];
        float depth = clipped[i].position[2] * w * 0.5f + 0.5f;
        xy[i][0] = (clipped[i].position[0] * w * 0.5f + 0.5f) * ps2_gs->Width;
        xy[i][1] = (0.5f - clipped[i].position[1] * w * 0.5f) * ps2_gs->Height;
        z[i] = (int)((1.0f - depth_near - (depth_far - depth_near) * depth) * 16777215.0f);
    }
    gsKit_prim_line_goraud_3d(ps2_gs, xy[0][0], xy[0][1], z[0],
        xy[1][0], xy[1][1], z[1], Hyena_GSRGBA(clipped[0].color, 0), Hyena_GSRGBA(clipped[1].color, 0));
}

static u64 Hyena_GSTextureRegister(GSTEXTURE *texture)
{
    int tw = 0, th = 0;
    while ((1 << tw) < texture->Width) tw++;
    while ((1 << th) < texture->Height) th++;
    return GS_SETREG_TEX0(texture->Vram / 256, texture->TBW, texture->PSM,
        tw, th, ps2_gs->PrimAlphaEnable, 0, texture->VramClut / 256,
        texture->ClutPSM, texture->ClutStorageMode, 0,
        texture->VramClut ? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD);
}

static void Hyena_GSTextureState(u64 tex0)
{
    u64 *data;
    Hyena_GSReserveQueue(32);
    data = gsKit_heap_alloc(ps2_gs, 1, 16, GIF_AD);
    *data++ = GIF_TAG_AD(1);
    *data++ = GIF_AD;
    *data++ = tex0;
    *data++ = GS_TEX0_1 + ps2_gs->PrimContext;
}

static void Hyena_GSVUTexture(GSTEXTURE *texture)
{
    u64 tex0 = Hyena_GSTextureRegister(texture);
    if (last_vu_texture == texture && texture->Vram && last_vu_tex0 == tex0 &&
        last_vu_filter == texture->Filter && last_vu_context == ps2_gs->PrimContext)
        return;
    Hyena_GSReserveQueue(4096);
    gsKit_TexManager_bind(ps2_gs, texture);
    gsKit_set_texfilter(ps2_gs, texture->Filter);
    last_vu_tex0 = Hyena_GSTextureRegister(texture);
    Hyena_GSTextureState(last_vu_tex0);
    last_vu_texture = texture;
    last_vu_filter = texture->Filter;
    last_vu_context = ps2_gs->PrimContext;
}

static void Hyena_GSLoadLightmaps(int shared_clut, u64 tex0[3])
{
    int channel, uploaded = 0;
    last_vu_texture = NULL;
    for (channel = 0; channel < 3; ++channel) {
        GSTEXTURE *texture = current_lightmap[channel];
        unsigned int bytes = gsKit_texture_size_ee(texture->Width, texture->Height, texture->PSM);
        if (gsKit_texture_size(texture->Width, texture->Height, texture->PSM) > lightmap_plane_size)
            Sys_Error("Lightmap exceeds reserved VRAM");
        texture->Vram = lightmap_vram + channel * lightmap_slot_size;
        texture->VramClut = lightmap_vram + (shared_clut ? 0 : channel) * lightmap_slot_size + lightmap_plane_size;
        gsKit_setup_tbw(texture);
        if (resident_lightmap[channel] != texture) {
            Hyena_GSReserveQueue(4096);
            if (!lightmap_coherent)
                SyncDCache(texture->Mem, (byte *)texture->Mem + bytes);
            gsKit_texture_send_inline(ps2_gs, texture->Mem, texture->Width, texture->Height,
                texture->Vram, texture->PSM, texture->TBW, GS_CLUT_TEXTURE);
            if (!shared_clut || !channel) {
                if (!lightmap_coherent)
                    SyncDCache(texture->Clut, (byte *)texture->Clut + 1024);
                gsKit_texture_send_inline(ps2_gs, texture->Clut, 16, 16,
                    texture->VramClut, texture->ClutPSM, 1, GS_CLUT_PALLETE);
            }
            resident_lightmap[channel] = texture;
            uploaded = 1;
        }
        tex0[channel] = Hyena_GSTextureRegister(texture);
    }
    if (uploaded) {
        u64 *data;
        Hyena_GSReserveQueue(32);
        data = gsKit_heap_alloc(ps2_gs, 1, 16, GIF_AD);
        *data++ = GIF_TAG_AD(1);
        *data++ = GIF_AD;
        *data++ = 0;
        *data++ = GS_TEXFLUSH;
    }
    gsKit_set_texfilter(ps2_gs, current_lightmap[0]->Filter);
}

static void Hyena_GSVUDraw(int mode, const ps2_gs_vertex_t *input, int count, int textured, int fog_mode)
{
    if (ps2_gs->CurQueue->tag_size)
        Hyena_GSExecuteQueue();
    if (!VID_QueueEnabled())
        VID_VU1WaitGif();
    if (!VID_VU1Submit(ps2_gs, combined_matrix, input, count, mode, textured,
        blending, texture_replace, texture_transform, depth_near, depth_far,
        fog_mode, fog_start, fog_end, fog_color, NULL, 0, NULL, current_stream))
        Sys_Error("VU1 submission failed");
}

static void Hyena_GSVUPass(ps2_vu1_pass_t *pass, int kind, unsigned int mask,
    u64 test, u64 alpha, u64 tex0, int textured, int blend)
{
    unsigned int primitive = 5 | (1 << 3) | (textured << 4) |
        (blend << 6) | (ps2_gs->PrimContext << 9);
    memset(pass, 0, sizeof(*pass));
    pass->kind[0] = kind;
    pass->packet[0][0] = GIF_TAG(4, 0, 0, 0, GSKIT_GIF_FLG_PACKED, 1);
    pass->packet[0][1] = GIF_AD;
    pass->packet[1][0] = GS_SETREG_FRAME(ps2_gs->ScreenBuffer[ps2_gs->ActiveBuffer & 1] / 8192,
        ps2_gs->Width / 64, ps2_gs->PSM, mask);
    pass->packet[1][1] = GS_FRAME_1 + ps2_gs->PrimContext;
    pass->packet[2][0] = test;
    pass->packet[2][1] = GS_TEST_1 + ps2_gs->PrimContext;
    pass->packet[3][0] = alpha;
    pass->packet[3][1] = GS_ALPHA_1 + ps2_gs->PrimContext;
    pass->packet[4][0] = tex0;
    pass->packet[4][1] = GS_TEX0_1 + ps2_gs->PrimContext;
    pass->packet[5][0] = (1ULL << 46) | ((u64)primitive << 47) | (3ULL << 60);
    pass->packet[5][1] = 0x412;
}

static void Hyena_GSVULightmap(int mode, const ps2_gs_vertex_t *input, int count, int fog)
{
    ps2_vu1_pass_t passes[7];
    ps2_gs_stream_t stream;
    const ps2_gs_stream_t *submission = current_stream;
    u64 tex0[3], test;
    int channel, pass_count = 0;
    int shared_clut = current_lightmap[0]->Clut == current_lightmap[1]->Clut &&
        current_lightmap[0]->Clut == current_lightmap[2]->Clut;

    Hyena_GSAlphaBit(0);
    Hyena_GSLoadLightmaps(shared_clut, tex0);
    if (shared_clut) {
        Hyena_GSTextureState(tex0[0]);
        for (channel = 0; channel < 3; ++channel)
            tex0[channel] &= ~(7ULL << 61);
    }
    if (depth_testing && lightmap_alpha)
        Hyena_GSVUPass(&passes[pass_count++], PS2_VU1_PASS_MASK, 0x00ffffff,
            GS_SETREG_TEST(0, 1, 0, 0, 0, 0, 1, 3), 0, tex0[0] & ~(7ULL << 61), 0, 0);
    test = GS_SETREG_TEST(0, 1, 0, 0, depth_testing && lightmap_alpha, 1, 1, depth_testing ? 2 : 1);
    for (channel = 0; channel < 3; ++channel)
        Hyena_GSVUPass(&passes[pass_count++], PS2_VU1_PASS_LIGHTMAP, ~(0xffu << (channel * 8)),
            test, GS_SETREG_ALPHA(1, 2, 0, 2, 0), tex0[channel], 1, 1);
    if (fog)
        Hyena_GSVUPass(&passes[pass_count++], PS2_VU1_PASS_FOG, 0xff000000,
            test, GS_SETREG_ALPHA(0, 2, 0, 1, 0), tex0[2] & ~(7ULL << 61), 0, 1);
    if (depth_testing && lightmap_alpha)
        Hyena_GSVUPass(&passes[pass_count++], PS2_VU1_PASS_RESTORE, 0x00ffffff,
            GS_SETREG_TEST(0, 1, 0, 0, 0, 0, 1, 1), 0, tex0[2] & ~(7ULL << 61), 0, 0);
    Hyena_GSVUPass(&passes[pass_count], PS2_VU1_PASS_STATE, 0,
        GS_SETREG_TEST(ps2_gs->Test->ATE, ps2_gs->Test->ATST,
            ps2_gs->Test->AREF, ps2_gs->Test->AFAIL, ps2_gs->Test->DATE,
            ps2_gs->Test->DATM, ps2_gs->Test->ZTE, ps2_gs->Test->ZTST),
        ps2_gs->PrimAlpha, tex0[2] & ~(7ULL << 61), 0, 0);
    passes[pass_count++].packet[5][0] |= 0x8000;
    if (current_stream) {
        stream = *current_stream;
        stream.indexed = stream.indexed && stream.batch && !lightmap_alpha;
        stream.batch = stream.batch && !lightmap_alpha;
        submission = &stream;
        if (stream.batch)
            for (channel = 0; channel < pass_count; ++channel)
                passes[channel].packet[5][0] = (passes[channel].packet[5][0] & ~(7ULL << 47)) | (3ULL << 47);
    }
    if (ps2_gs->CurQueue->tag_size)
        Hyena_GSExecuteQueue();
    if (!VID_QueueEnabled())
        VID_VU1WaitGif();
    if (!VID_VU1Submit(ps2_gs, combined_matrix, input, count, mode, 1,
        1, texture_replace, texture_transform, depth_near, depth_far,
        fog ? 2 : 0, fog_start, fog_end, fog_color, passes, pass_count, NULL, submission))
        Sys_Error("VU1 lightmap submission failed");
}

static void Hyena_GSVUSubmit(int mode, const ps2_gs_vertex_t *input, int count, int textured)
{
    int fog = fog_enabled && fog_end > fog_start && fog_start > 0;
    int vu_mode = mode == HYE_TRIANGLES ? PS2_VU1_TRIANGLES :
        mode == HYE_QUADS ? PS2_VU1_QUADS :
        mode == HYE_TRIANGLE_FAN ? PS2_VU1_FAN : PS2_VU1_STRIP;
    if (textured && current_lightmap[0] && blending && lightmap_blending) {
        Hyena_GSVULightmap(vu_mode, input, count, fog);
    } else {
        if (textured)
            Hyena_GSVUTexture(current_texture);
        Hyena_GSVUDraw(vu_mode, input, count, textured, fog && !fog_world ? 1 : 0);
    }
    if (r_profile.value) {
        profile_triangles += mode == HYE_TRIANGLES ? count / 3 :
            mode == HYE_QUADS ? count / 4 * 2 : count - 2;
        profile_batches++;
        if (textured && current_lightmap[0] && blending && lightmap_blending)
            profile_lightmap_batches++;
    }
}

static int Hyena_GSSubmitAliasData(const unsigned char *pose1, const unsigned char *pose2,
    const ps2_gs_alias_batch_t *batches, int batch_count, int count, float blend, const unsigned int *packed, int single, const unsigned int *packed_second, const ps2_gs_static_t *commands)
{
    ps2_gs_alias_t alias;
    double start = r_profile.value ? Sys_FloatTime() : 0;
    int textured = texturing && current_texture && current_texture->Mem;
    int fog = fog_enabled && !fog_world && fog_end > fog_start && fog_start > 0;

    if (!ps2_gs || !ps2_vu1.value)
        return 0;
    alias.commands = commands;
    alias.packed = packed;
    alias.packed_second = packed_second;
    alias.pose1 = pose1;
    alias.pose2 = pose2 ? pose2 : pose1;
    alias.batches = batches;
    alias.batch_count = batch_count;
    alias.single = single;
    alias.strips = ps2_alias_strips.value != 0;
    alias.blend = pose2 || packed_second ? blend : 0;
    alias.batch = ps2_vu_batch.value != 0;
    alias.cull = gl_cull.value && ps2_alias_cull.value;
    memcpy(alias.color, current_color, sizeof(alias.color));
    if (matrix_dirty) {
        memcpy(combined_matrix, projection[projection_depth], sizeof(combined_matrix));
        Hyena_GSMultiply(combined_matrix, model[model_depth]);
        matrix_dirty = 0;
    }
    if (fog) {
        float farthest = combined_matrix[15];
        int axis;
        for (axis = 0; axis < 3; ++axis)
            if (combined_matrix[axis * 4 + 3] > 0)
                farthest += combined_matrix[axis * 4 + 3] * 255;
        if (farthest <= fog_start)
            fog = 0;
    }
    if (textured)
        Hyena_GSVUTexture(current_texture);
    if (ps2_gs->CurQueue->tag_size)
        Hyena_GSExecuteQueue();
    if (!VID_QueueEnabled())
        VID_VU1WaitGif();
    if (!VID_VU1Submit(ps2_gs, combined_matrix, NULL, count, PS2_VU1_TRIANGLES,
        textured, blending, texture_replace, texture_transform, depth_near, depth_far,
        fog ? 1 : 0, fog_start, fog_end, fog_color, NULL, 0, &alias, NULL))
        Sys_Error("VU1 alias submission failed");
    if (r_profile.value) {
        profile_vertices += count;
        profile_triangles += count / 3;
        profile_batches++;
        profile_geometry_seconds += Sys_FloatTime() - start;
        if (single) {
            profile_static_batches++;
            profile_static_vertices += count;
            profile_static_seconds += Sys_FloatTime() - start;
        }
    }
    return 1;
}

int Hyena_GSSubmitAlias(const unsigned char *pose1, const unsigned char *pose2,
    const ps2_gs_alias_batch_t *batches, int batch_count, int count, float blend, const unsigned int *packed, const ps2_gs_static_t *commands)
{
    return Hyena_GSSubmitAliasData(pose1, pose2, batches, batch_count, count, blend, packed, 0, NULL, commands);
}

int Hyena_GSSubmitAliasPair(const unsigned int *first, const unsigned int *second,
    const ps2_gs_alias_batch_t *batches, int batch_count, int count, float blend)
{
    return Hyena_GSSubmitAliasData(NULL, NULL, batches, batch_count, count, blend, first, 0, second, NULL);
}

int Hyena_GSSubmitStaticAlias(const unsigned int *packed, const ps2_gs_alias_batch_t *batches,
    int batch_count, int count, const ps2_gs_static_t *commands)
{
    if (!packed)
        return 0;
    return Hyena_GSSubmitAliasData(NULL, NULL, batches, batch_count, count, 0, packed, 1, NULL, commands);
}

int Hyena_GSSubmitIndexed(const void *positions, int position_stride, const void *uvs,
    int uv_stride, const unsigned short *indices, int count, int cache)
{
    ps2_gs_stream_t stream;
    double start = r_profile.value ? Sys_FloatTime() : 0;
    int textured = texturing && current_texture && current_texture->Mem;

    if (!ps2_gs || !ps2_vu1.value || !ps2_world_stream.value || !positions || !uvs || count < 3 || count % 3)
        return 0;
    stream.positions = positions;
    stream.uvs = uvs;
    stream.indices = indices;
    stream.batch = ps2_vu_batch.value != 0;
    stream.indexed = indices && ps2_world_indexed.value;
    stream.cache = cache && ps2_world_cache.value;
    stream.position_stride = position_stride;
    stream.uv_stride = uv_stride;
    memcpy(stream.color, current_color, sizeof(stream.color));
    if (matrix_dirty) {
        memcpy(combined_matrix, projection[projection_depth], sizeof(combined_matrix));
        Hyena_GSMultiply(combined_matrix, model[model_depth]);
        matrix_dirty = 0;
    }
    current_stream = &stream;
    Hyena_GSVUSubmit(HYE_TRIANGLES, NULL, count, textured);
    current_stream = NULL;
    if (r_profile.value) {
        profile_vertices += count;
        profile_geometry_seconds += Sys_FloatTime() - start;
    }
    return 1;
}

void Hyena_GSSubmit(int mode, const ps2_gs_vertex_t *input, int count, int textured)
{
    ps2_gs_vertex_t first, previous = {0}, previous2 = {0}, current, triangle[3];
    double profile_start = r_profile.value ? Sys_FloatTime() : 0;
    int i;
    if (!ps2_gs || !input || count < (mode == HYE_LINE_STRIP ? 2 : 3))
        return;
    if (matrix_dirty) {
        memcpy(combined_matrix, projection[projection_depth], sizeof(combined_matrix));
        Hyena_GSMultiply(combined_matrix, model[model_depth]);
        matrix_dirty = 0;
    }
    if (r_profile.value)
        profile_vertices += count;
    if (mode == HYE_LINE_STRIP) {
        Hyena_GSTransformVertex(&input[0], &previous);
        for (i = 1; i < count; ++i) {
            Hyena_GSTransformVertex(&input[i], &current);
            Hyena_GSDrawLine(&previous, &current);
            previous = current;
        }
        return;
    }
    batch_count = 0;
    batch_textured = textured && texturing && current_texture && current_texture->Mem;
    if (ps2_vu1.value) {
        Hyena_GSVUSubmit(mode, input, count, batch_textured);
        if (r_profile.value)
            profile_geometry_seconds += Sys_FloatTime() - profile_start;
        return;
    }
    for (i = 0; i < count; ++i) {
        Hyena_GSTransformVertex(&input[i], &current);
        if (i == 0 || (mode == HYE_QUADS && i % 4 == 0))
            first = current;
        if (mode == HYE_TRIANGLES && i % 3 == 2) {
            triangle[0] = previous2;
            triangle[1] = previous;
            triangle[2] = current;
            Hyena_GSClipTriangle(triangle);
        } else if ((mode == HYE_TRIANGLE_FAN || mode == HYE_QUADS) && i >= 2) {
            if (mode == HYE_QUADS) {
                if (i % 4 == 2) {
                    triangle[0] = previous2;
                    triangle[1] = previous;
                    triangle[2] = current;
                    Hyena_GSClipTriangle(triangle);
                } else if (i % 4 == 3) {
                    triangle[0] = first;
                    triangle[1] = previous;
                    triangle[2] = current;
                    Hyena_GSClipTriangle(triangle);
                }
            } else {
                triangle[0] = first;
                triangle[1] = previous;
                triangle[2] = current;
                Hyena_GSClipTriangle(triangle);
            }
        } else if (mode == HYE_TRIANGLE_STRIP && i >= 2) {
            triangle[0] = i & 1 ? previous : previous2;
            triangle[1] = i & 1 ? previous2 : previous;
            triangle[2] = current;
            Hyena_GSClipTriangle(triangle);
        }
        previous2 = previous;
        previous = current;
    }
    Hyena_GSFlushBatch();
    if (r_profile.value)
        profile_geometry_seconds += Sys_FloatTime() - profile_start;
}

GSGLOBAL *Hyena_GSGlobal(void)
{
    return ps2_gs;
}

void Hyena_GSInit(void)
{
    if (ps2_gs)
        return;
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
        D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    ps2_gs = gsKit_init_global_custom(64 * 1024, 16 * 1024);
    if (!ps2_gs)
        return;
    ps2_gs->Mode = GS_MODE_NTSC;
    ps2_gs->Interlace = GS_INTERLACED;
    ps2_gs->Field = GS_FIELD;
    ps2_gs->Width = 640;
    ps2_gs->Height = 448;
    ps2_gs->PSM = GS_PSM_CT32;
    ps2_gs->PSMZ = GS_PSMZ_24;
    ps2_gs->DoubleBuffering = GS_SETTING_ON;
    ps2_gs->ZBuffering = GS_SETTING_ON;
    gsKit_init_screen(ps2_gs);
    lightmap_plane_size = gsKit_texture_size(128, 128, GS_PSM_T8);
    lightmap_slot_size = lightmap_plane_size + gsKit_texture_size(16, 16, GS_PSM_CT32);
    lightmap_vram = gsKit_vram_alloc(ps2_gs, lightmap_slot_size * 3, GSKIT_ALLOC_USERBUFFER);
    if (lightmap_vram == GSKIT_ALLOC_ERROR)
        Sys_Error("Unable to reserve lightmap VRAM");
    gsKit_TexManager_init(ps2_gs);
    gsKit_mode_switch(ps2_gs, GS_ONESHOT);
    gsKit_set_test(ps2_gs, GS_ZTEST_ON);
    gsKit_set_clamp(ps2_gs, GS_CMODE_REPEAT);
    gsKit_set_primalpha(ps2_gs, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
    Hyena_GSIdentity(projection[0]);
    Hyena_GSIdentity(model[0]);
}

void Hyena_GSBeginFrame(void)
{
    double start = r_profile.value ? Sys_FloatTime() : 0;
    VID_PresentBegin(ps2_gs);
    ps2_present_wait = r_profile.value ? Sys_FloatTime() - start : 0;
    VID_QueueBegin();
    Hyena_WorldFrame();
    if (r_profile.value)
        ps2_frame_begin_time = Sys_FloatTime();
}

void Hyena_GSEndFrame(int vsync)
{
    static int profile_frames;
    static double total_cpu, total_submit, total_flip, last_report;
    double start = 0, submitted = 0, finished = 0;
    int async = 0;
    if (!ps2_gs)
        return;
    if (r_profile.value)
        start = Sys_FloatTime();
    Hyena_GSExecuteQueue();
    if (VID_QueueEnabled()) {
        GS_SETREG_CSR_FINISH(1);
        if (vsync && ps2_async_flip.value)
            async = VID_PresentQueue(ps2_gs);
        gsKit_set_finish(ps2_gs);
        Hyena_GSExecuteQueue();
        if (async)
            VID_VU1KickFinish();
        else {
            VID_VU1Finish();
            gsKit_finish();
        }
    }
    if (!vsync && !VID_QueueEnabled()) {
        VID_VU1WaitGif();
        GS_SETREG_CSR_FINISH(1);
        gsKit_set_finish(ps2_gs);
        gsKit_queue_exec(ps2_gs);
        gsKit_finish();
    }
    if (r_profile.value)
        submitted = Sys_FloatTime();
    if (!async) {
        if (vsync)
            gsKit_sync_flip(ps2_gs);
        else
            VID_PresentImmediate(ps2_gs);
    }
    if (r_profile.value)
        finished = Sys_FloatTime();
    gsKit_TexManager_nextFrame(ps2_gs);
    if (r_profile.value) {
        total_cpu += start - ps2_frame_begin_time;
        total_submit += submitted - start;
        total_flip += finished - submitted + ps2_present_wait;
        profile_frames++;
        if (finished - last_report >= 2.0) {
            Con_SafePrintf("PS2 frame: CPU render %.2f ms, GS submit %.2f ms, flip wait %.2f ms, %d samples\n",
                1000.0 * total_cpu / profile_frames,
                1000.0 * total_submit / profile_frames,
                1000.0 * total_flip / profile_frames, profile_frames);
            Con_SafePrintf("PS2 geometry/frame: %u vertices, %u triangles, %u batches (%u RGB lightmap batches)\n",
                profile_vertices / profile_frames, profile_triangles / profile_frames,
                profile_batches / profile_frames, profile_lightmap_batches / profile_frames);
            Con_SafePrintf("PS2 geometry: packing %.2f ms, native %.2f ms/frame\n",
                1000.0 * profile_packing_seconds / profile_frames,
                1000.0 * profile_geometry_seconds / profile_frames);
            Con_SafePrintf("PS2 entity lighting: %.2f ms/frame\n",
                1000.0 * profile_entity_light_seconds / profile_frames);
            Con_SafePrintf("PS2 VIF wait: %.2f ms/frame\n", (double)profile_vif_wait_ticks * (1000.0 / (2 * kBUSCLK)) / profile_frames);
            Con_SafePrintf("PS2 GS waits: queue %.2f ms, GIF %.2f ms/frame\n",
                (double)profile_queue_ticks * (1000.0 / (2 * kBUSCLK)) / profile_frames,
                (double)profile_gif_wait_ticks * (1000.0 / (2 * kBUSCLK)) / profile_frames);
            Con_SafePrintf("PS2 single poses: %u draws, %u vertices, %.2f ms/frame\n",
                profile_static_batches / profile_frames, profile_static_vertices / profile_frames,
                1000.0 * profile_static_seconds / profile_frames);
            profile_static_batches = profile_static_vertices = 0;
            profile_static_seconds = 0;
            profile_entity_light_seconds = 0;
            profile_frames = 0;
            total_cpu = total_submit = total_flip = 0;
            profile_vertices = profile_triangles = profile_batches = profile_lightmap_batches = 0;
            profile_queue_ticks = profile_gif_wait_ticks = 0;
            profile_vif_wait_ticks = 0;
            profile_geometry_seconds = profile_packing_seconds = 0;
            last_report = finished;
        }
    }
    (void)vsync;
}

void Hyena_GSSetClearColor(float r, float g, float b, float a)
{
    clear_color[0] = r;
    clear_color[1] = g;
    clear_color[2] = b;
    clear_color[3] = a;
}

void Hyena_GSClear(int color, int depth)
{
    if (ps2_gs && (color || depth)) {
        int alpha = ps2_gs->PrimAlphaEnable;
        int alpha_test = ps2_gs->Test->ATE;
        int destination_test = ps2_gs->Test->DATE;

        ps2_gs->PrimAlphaEnable = 0;
        ps2_gs->Test->ATE = ps2_gs->Test->DATE = 0;
        gsKit_set_test(ps2_gs, 0);
        Hyena_GSDepthWrite(depth);
        Hyena_GSAlphaBit(0);
        gsKit_clear(ps2_gs, (u64)Hyena_GSRGBA(clear_color, 0));
        Hyena_GSDepthWrite(depth_writes && depth_testing);
        ps2_gs->Test->ATE = alpha_test;
        ps2_gs->Test->DATE = destination_test;
        gsKit_set_test(ps2_gs, 0);
        ps2_gs->PrimAlphaEnable = alpha;
        Hyena_GSAlphaBit(alpha_testing && depth_writes && depth_testing);
    }
}

void Hyena_GSSetColor(float r, float g, float b, float a)
{
    current_color[0] = r;
    current_color[1] = g;
    current_color[2] = b;
    current_color[3] = a;
}

void Hyena_GSGetColor(float rgba[4])
{
    memcpy(rgba, current_color, sizeof(current_color));
}

void Hyena_GSSetBlend(int enabled)
{
    blending = !!enabled;
    if (ps2_gs)
        ps2_gs->PrimAlphaEnable = blending || alpha_testing;
}

void Hyena_GSSetBlendEquation(int a, int b, int c, int d, int fix)
{
    if (ps2_gs)
        gsKit_set_primalpha(ps2_gs, GS_SETREG_ALPHA(a, b, c, d, fix), 0);
}

void Hyena_GSSetAlphaTest(int enabled, float threshold)
{
    alpha_testing = !!enabled;
    alpha_reference = (int)(Hyena_GSLimit(threshold) * 128.0f);
    if (ps2_gs) {
        ps2_gs->PrimAlphaEnable = blending || alpha_testing;
        ps2_gs->Test->ATST = 6;
        ps2_gs->Test->AREF = alpha_reference;
        gsKit_set_test(ps2_gs, enabled ? GS_ATEST_ON : GS_ATEST_OFF);
        Hyena_GSAlphaBit(alpha_testing && depth_writes && depth_testing);
    }
}

void Hyena_GSSetDepthTest(int enabled)
{
    depth_testing = !!enabled;
    if (ps2_gs)
        gsKit_set_test(ps2_gs, depth_testing ? GS_ZTEST_ON : GS_ZTEST_OFF);
    Hyena_GSDepthWrite(depth_writes && depth_testing);
    Hyena_GSAlphaBit(alpha_testing && depth_writes && depth_testing);
}

void Hyena_GSSetDepthWrite(int enabled)
{
    depth_writes = !!enabled;
    Hyena_GSDepthWrite(depth_writes && depth_testing);
    Hyena_GSAlphaBit(alpha_testing && depth_writes && depth_testing);
}

void Hyena_GSSetDepthRange(float near_value, float far_value)
{
    depth_near = near_value;
    depth_far = far_value;
}

void Hyena_GSSetTexturing(int enabled)
{
    texturing = !!enabled;
}

void Hyena_GSSetReplaceTexture(int enabled)
{
    texture_replace = !!enabled;
}

void Hyena_GSBindTexture(GSTEXTURE *texture)
{
    current_texture = texture;
    current_lightmap[0] = NULL;
}

void Hyena_GSBindLightmap(GSTEXTURE *red, GSTEXTURE *green, GSTEXTURE *blue)
{
    lightmap_coherent = 0;
    current_texture = current_lightmap[0] = red;
    current_lightmap[1] = green;
    current_lightmap[2] = blue;
}

void Hyena_GSSetLightmapCoherent(int coherent)
{
    lightmap_coherent = coherent;
}

void Hyena_GSSetLightmapAlpha(int enabled)
{
    lightmap_alpha = !!enabled;
}

void Hyena_GSSetLightmapBlend(int enabled)
{
    lightmap_blending = !!enabled;
}

void Hyena_GSDraw2DQuad(float x0, float y0, float x1, float y1,
    float u0, float v0, float u1, float v1, const float color[4], int textured)
{
    if (!ps2_gs)
        return;
    if (vid.width) {
        x0 *= (float)ps2_gs->Width / vid.width;
        x1 *= (float)ps2_gs->Width / vid.width;
    }
    last_vu_texture = NULL;
    Hyena_GSReserveQueue(4096);
    if (textured && current_texture && current_texture->Mem) {
        gsKit_TexManager_bind(ps2_gs, current_texture);
        gsKit_set_texfilter(ps2_gs, current_texture->Filter);
        gsKit_prim_sprite_texture_3d(ps2_gs, current_texture,
            x0, y0, 0, u0 * current_texture->Width, v0 * current_texture->Height,
            x1, y1, 0, u1 * current_texture->Width, v1 * current_texture->Height,
            (u64)Hyena_GSRGBA(color, 1));
    } else {
        gsKit_prim_sprite(ps2_gs, x0, y0, x1, y1, 0, (u64)Hyena_GSRGBA(color, 0));
    }
}

void Hyena_GSProjectionIdentity(void)
{
    matrix_dirty = 1;
    Hyena_GSIdentity(projection[projection_depth]);
}

void Hyena_GSProjectionPush(void)
{
    if (projection_depth + 1 < PS2_GS_STACK_LIMIT) {
        memcpy(projection[projection_depth + 1], projection[projection_depth], sizeof(projection[0]));
        projection_depth++;
    }
}

void Hyena_GSProjectionPop(void)
{
    matrix_dirty = 1;
    if (projection_depth)
        projection_depth--;
}

void Hyena_GSProjectionScale(float x, float y, float z)
{
    matrix_dirty = 1;
    Hyena_GSScale(projection[projection_depth], x, y, z);
}

void Hyena_GSProjectionTranslate(float x, float y, float z)
{
    matrix_dirty = 1;
    Hyena_GSTranslate(projection[projection_depth], x, y, z);
}

void Hyena_GSProjectionFrustum(float left, float right, float bottom, float top, float near_value, float far_value)
{
    matrix_dirty = 1;
    float m[16] __attribute__((aligned(16))) = {0};
    m[0] = 2 * near_value / (right - left);
    m[5] = 2 * near_value / (top - bottom);
    m[8] = (right + left) / (right - left);
    m[9] = (top + bottom) / (top - bottom);
    m[10] = -(far_value + near_value) / (far_value - near_value);
    m[11] = -1;
    m[14] = -2 * far_value * near_value / (far_value - near_value);
    Hyena_GSMultiply(projection[projection_depth], m);
}

void Hyena_GSProjectionOrtho(float left, float right, float bottom, float top, float near_value, float far_value)
{
    matrix_dirty = 1;
    float m[16] __attribute__((aligned(16)));
    Hyena_GSIdentity(m);
    m[0] = 2 / (right - left);
    m[5] = 2 / (top - bottom);
    m[10] = -2 / (far_value - near_value);
    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
    m[14] = -(far_value + near_value) / (far_value - near_value);
    Hyena_GSMultiply(projection[projection_depth], m);
}

void Hyena_GSModelIdentity(void)
{
    matrix_dirty = 1;
    Hyena_GSIdentity(model[model_depth]);
}

void Hyena_GSModelLoad(const float m[16])
{
    matrix_dirty = 1;
    memcpy(model[model_depth], m, sizeof(model[0]));
}

void Hyena_GSModelGet(float m[16])
{
    memcpy(m, model[model_depth], sizeof(model[0]));
}

void Hyena_GSModelPush(void)
{
    if (model_depth + 1 < PS2_GS_STACK_LIMIT) {
        memcpy(model[model_depth + 1], model[model_depth], sizeof(model[0]));
        model_depth++;
    }
}

void Hyena_GSModelPop(void)
{
    matrix_dirty = 1;
    if (model_depth)
        model_depth--;
}

void Hyena_GSModelTranslate(float x, float y, float z)
{
    matrix_dirty = 1;
    Hyena_GSTranslate(model[model_depth], x, y, z);
}

void Hyena_GSModelScale(float x, float y, float z)
{
    matrix_dirty = 1;
    Hyena_GSScale(model[model_depth], x, y, z);
}

void Hyena_GSModelRotate(float degrees, float x, float y, float z)
{
    matrix_dirty = 1;
    Hyena_GSRotate(model[model_depth], degrees, x, y, z);
}

void Hyena_GSSetTextureTransform(float sx, float sy, float ox, float oy)
{
    texture_transform[0] = sx;
    texture_transform[1] = sy;
    texture_transform[2] = ox;
    texture_transform[3] = oy;
}

void Hyena_GSSetFog(int world, float start, float end, float r, float g, float b)
{
    u64 *data;
    fog_world = world;
    fog_start = start;
    fog_end = end;
    fog_color[0] = Hyena_GSLimit(r);
    fog_color[1] = Hyena_GSLimit(g);
    fog_color[2] = Hyena_GSLimit(b);
    if (!ps2_gs)
        return;
    Hyena_GSReserveQueue(32);
    data = gsKit_heap_alloc(ps2_gs, 1, 16, GIF_AD);
    *data++ = GIF_TAG_AD(1);
    *data++ = GIF_AD;
    *data++ = (unsigned int)(fog_color[0] * 255) |
        ((unsigned int)(fog_color[1] * 255) << 8) |
        ((unsigned int)(fog_color[2] * 255) << 16);
    *data++ = GS_FOGCOL;
}

void Hyena_GSSetFogEnabled(int enabled)
{
    fog_enabled = enabled;
}

void Hyena_GSInvalidateLightmap(GSTEXTURE *texture)
{
    int channel;
    for (channel = 0; channel < 3; ++channel) {
        if (resident_lightmap[channel] == texture) {
            memset(resident_lightmap, 0, sizeof(resident_lightmap));
            break;
        }
    }
}

extern vec3_t lightcolor;

extern cvar_t ps2_vu1;
extern cvar_t ps2_packet_cache;
cvar_t ps2_alias_vu1 = {"ps2_alias_vu1", "1"};
cvar_t ps2_alias_static = {"ps2_alias_static", "1"};
cvar_t ps2_alias_batch = {"ps2_alias_batch", "64"};

typedef struct gs_alias_s {
    const void *key;
    ps2_gs_alias_batch_t *batches;
    unsigned int *single;
    ps2_gs_static_t *commands;
    int count, batch_count;
    size_t bytes, single_pose_bytes;
    struct gs_alias_s *next;
} gs_alias_t;

static gs_alias_t *alias_cache;
static size_t alias_cache_bytes;

typedef struct gs_alias_pose_s {
    const void *key;
    int pose, frame;
    size_t bytes;
    unsigned int *vertices;
    struct gs_alias_pose_s *next;
} gs_alias_pose_t;

static gs_alias_pose_t *alias_pose_cache;
static size_t alias_packed_bytes;

static void Hyena_ClearAliasPoses(void)
{
    gs_alias_pose_t *pose, *next;
    VID_VU1WaitDMA();
    for (pose = alias_pose_cache; pose; pose = next) {
        next = pose->next;
        free(pose->vertices);
        free(pose);
    }
    alias_pose_cache = NULL;
    alias_packed_bytes = 0;
}

typedef struct { byte v[3], normal; } hyena_morph_vertex_t;

static const gs_alias_pose_t *Hyena_AliasPose(const gs_alias_t *geometry, const hyena_morph_mesh_t *header,
    const hyena_morph_vertex_t *source)
{
    const hyena_morph_vertex_t *base = (const hyena_morph_vertex_t *)header->poses;
    gs_alias_pose_t *pose, *previous = NULL, *oldest, *oldest_previous;
    int index = (source - base) / header->vertices;
    size_t bytes = sizeof(*pose) + geometry->single_pose_bytes, offset = 0;
    int batch, vertex;

    if (!ps2_packet_cache.value) {
        if (alias_pose_cache)
            Hyena_ClearAliasPoses();
        return NULL;
    }
    if (bytes > 256 * 1024)
        return NULL;
    for (pose = alias_pose_cache; pose; previous = pose, pose = pose->next) {
        if (pose->key == geometry->key && pose->pose == index) {
            if (previous) {
                previous->next = pose->next;
                pose->next = alias_pose_cache;
                alias_pose_cache = pose;
            }
            pose->frame = host_framecount;
            return pose;
        }
    }
    while (alias_pose_cache && alias_packed_bytes + bytes > 256 * 1024) {
        previous = oldest = oldest_previous = NULL;
        for (pose = alias_pose_cache; pose; previous = pose, pose = pose->next)
            if (pose->frame != host_framecount) {
                oldest = pose;
                oldest_previous = previous;
            }
        if (!oldest)
            return NULL;
        if (oldest_previous)
            oldest_previous->next = oldest->next;
        else
            alias_pose_cache = oldest->next;
        alias_packed_bytes -= oldest->bytes;
        free(oldest->vertices);
        free(oldest);
    }
    pose = malloc(sizeof(*pose));
    if (!pose)
        return NULL;
    pose->vertices = memalign(64, geometry->single_pose_bytes);
    if (!pose->vertices) {
        free(pose);
        return NULL;
    }
    memset(pose->vertices, 0, geometry->single_pose_bytes);
    for (batch = 0; batch < geometry->batch_count; ++batch) {
        const ps2_gs_alias_batch_t *layout = &geometry->batches[batch];
        unsigned int *dest = (unsigned int *)((byte *)pose->vertices + offset);
        for (vertex = 0; vertex < layout->vertex_count; ++vertex)
            memcpy(dest + vertex, source + layout->vertices[vertex], sizeof(*source));
        offset += (layout->vertex_count * 4 + 15) & ~15;
    }
    SifWriteBackDCache(pose->vertices, geometry->single_pose_bytes);
    pose->key = geometry->key;
    pose->pose = index;
    pose->frame = host_framecount;
    pose->bytes = bytes;
    pose->next = alias_pose_cache;
    alias_pose_cache = pose;
    alias_packed_bytes += bytes;
    return pose;
}

size_t Hyena_AliasCacheSize(void)
{
    return alias_cache_bytes + alias_packed_bytes;
}

void Hyena_ClearAliasCache(void)
{
    gs_alias_t *cache, *next;

    Hyena_ClearAliasPoses();
    for (cache = alias_cache; cache; cache = next) {
        next = cache->next;
        VID_VU1FreeStatic(cache->commands);
        free(cache->single);
        free(cache->batches);
        free(cache);
    }
    alias_cache = NULL;
    alias_cache_bytes = 0;
}

typedef struct {
    const int *commands;
    const unsigned short *remap;
    int vertices, remaining, count, triangle, fan, pending, limit, done;
    unsigned short indices[PS2_GS_ALIAS_TRIANGLES * 3];
    float uvs[PS2_GS_ALIAS_TRIANGLES * 3][2];
} gs_alias_source_t;

static void Hyena_AliasSource(gs_alias_source_t *source, const hyena_morph_mesh_t *header, int limit)
{
    memset(source, 0, sizeof(*source));
    source->commands = header->commands;
    source->remap = header->remap;
    source->remaining = header->command_vertices;
    source->vertices = header->vertices;
    source->limit = limit;
}

static int Hyena_ReadAliasBatch(gs_alias_source_t *source, ps2_gs_alias_batch_t *batch)
{
    int used;
    while (source->pending < source->limit * 3) {
        int corner;
        if (source->done)
            break;
        if (!source->count) {
            source->count = *source->commands++;
            if (!source->count) {
                source->done = 1;
                if (source->remaining)
                    Sys_Error("Invalid alias topology");
                break;
            }
            source->fan = source->count < 0;
            if (source->fan)
                source->count = -source->count;
            if (source->count < 3 || source->count > source->remaining)
                Sys_Error("Invalid alias commands");
            source->remaining -= source->count;
            source->triangle = 0;
        }
        for (corner = 0; corner < 3; corner++) {
            int index = source->fan ? corner ? source->triangle + corner : 0 :
                source->triangle + (corner == 2 ? 2 : corner ^ (source->triangle & 1));
            if (source->remap[index] >= source->vertices)
                Sys_Error("Invalid alias vertex");
            source->indices[source->pending] = source->remap[index];
            memcpy(source->uvs[source->pending], source->commands + index * 2, 2 * sizeof(float));
            source->pending++;
        }
        if (++source->triangle == source->count - 2) {
            source->commands += source->count * 2;
            source->remap += source->count;
            source->count = 0;
        }
    }
    if (!source->pending)
        return 0;
    if (!Hyena_BuildAliasBatch(batch, source->indices, &source->uvs[0][0], source->pending))
        Sys_Error("Invalid alias vertex batch");
    used = batch->triangle_count * 3;
    source->pending -= used;
    memmove(source->indices, source->indices + used, source->pending * sizeof(*source->indices));
    memmove(source->uvs, source->uvs + used, source->pending * sizeof(*source->uvs));
    return used;
}

static int Hyena_EvictAliasGeometry(void)
{
    gs_alias_t *cache = alias_cache, *previous = NULL;
    if (!cache)
        return 0;
    Hyena_ClearAliasPoses();
    while (cache->next) {
        previous = cache;
        cache = cache->next;
    }
    if (previous)
        previous->next = NULL;
    else
        alias_cache = NULL;
    alias_cache_bytes -= cache->bytes;
    VID_VU1FreeStatic(cache->commands);
    free(cache->single);
    free(cache->batches);
    free(cache);
    return 1;
}

static gs_alias_t *Hyena_AliasGeometry(const hyena_morph_mesh_t *header)
{
    static gs_alias_source_t source;
    static ps2_gs_alias_batch_t scratch;
    static int cached_limit;
    gs_alias_t *cache, *previous = NULL;
    size_t bytes, single_bytes = 0, single_offset = 0;
    int count, indices = 0, batch, batch_count = 0;
    int limit = bound(1, (int)ps2_alias_batch.value, PS2_GS_ALIAS_TRIANGLES);

    if (cached_limit != limit) {
        Hyena_ClearAliasCache();
        cached_limit = limit;
    }
    for (cache = alias_cache; cache; previous = cache, cache = cache->next) {
        if (cache->key == header->key) {
            if (previous) {
                previous->next = cache->next;
                cache->next = alias_cache;
                alias_cache = cache;
            }
            return cache;
        }
    }
    Hyena_AliasSource(&source, header, limit);
    while ((count = Hyena_ReadAliasBatch(&source, &scratch)) != 0) {
        indices += count;
        if (header->poses_count <= 1)
            single_bytes += (scratch.vertex_count * 4 + 15) & ~15;
        batch_count++;
    }
    if (!indices)
        Sys_Error("Invalid alias topology");
    bytes = sizeof(*cache) + batch_count * sizeof(ps2_gs_alias_batch_t) + single_bytes;
    if (single_bytes)
        bytes += VID_VU1StaticSize(batch_count);
    if (bytes > 1024 * 1024)
        return NULL;
    while (alias_cache && alias_cache_bytes + bytes > 1024 * 1024)
        Hyena_EvictAliasGeometry();
    for (;;) {
        cache = calloc(1, sizeof(*cache));
        if (cache) {
            cache->batches = memalign(64, batch_count * sizeof(*cache->batches));
            if (cache->batches)
                break;
            free(cache);
        }
        if (!Hyena_EvictAliasGeometry())
            return NULL;
    }
    cache->batch_count = batch_count;
    if (single_bytes) {
        cache->single = memalign(64, single_bytes);
        if (cache->single)
            memset(cache->single, 0, single_bytes);
        else
            bytes -= single_bytes + VID_VU1StaticSize(batch_count);
    }
    cache->bytes = bytes;
    cache->key = header->key;
    cache->count = indices;
    Hyena_AliasSource(&source, header, limit);
    for (batch = 0; batch < batch_count; batch++) {
        if (!Hyena_ReadAliasBatch(&source, &cache->batches[batch]))
            Sys_Error("Invalid alias vertex batch");
        cache->single_pose_bytes += (cache->batches[batch].vertex_count * 4 + 15) & ~15;
        if (cache->single) {
            const unsigned int *pose = (const unsigned int *)header->poses;
            unsigned int *out = (unsigned int *)((byte *)cache->single + single_offset);
            int i;
            for (i = 0; i < cache->batches[batch].vertex_count; i++)
                out[i] = pose[cache->batches[batch].vertices[i]];
            single_offset += (cache->batches[batch].vertex_count * 4 + 15) & ~15;
        }
    }
    if (cache->single)
        SifWriteBackDCache(cache->single, single_bytes);
    SyncDCache(cache->batches, cache->batches + cache->batch_count);
    if (cache->single) {
        cache->commands = VID_VU1CreateStatic(cache->single, cache->batches, cache->batch_count);
        if (!cache->commands) {
            bytes -= VID_VU1StaticSize(batch_count);
            cache->bytes = bytes;
        }
    }
    alias_cache_bytes += bytes;
    cache->next = alias_cache;
    alias_cache = cache;
    return cache;
}

static void Hyena_DrawAliasStream(const hyena_morph_mesh_t *header, const hyena_morph_vertex_t *pose1,
    const hyena_morph_vertex_t *pose2, float blend, int vu1)
{
    static gs_alias_source_t source;
    static ps2_gs_alias_batch_t batch;
    static ps2_gs_vertex_t vertices[PS2_GS_ALIAS_TRIANGLES * 3];
    int count, triangle, corner;
    float color[4];
    Hyena_GSGetColor(color);
    Hyena_AliasSource(&source, header, bound(1, (int)ps2_alias_batch.value, PS2_GS_ALIAS_TRIANGLES));
    while ((count = Hyena_ReadAliasBatch(&source, &batch)) != 0) {
        SyncDCache(&batch, &batch + 1);
        if (vu1 && Hyena_GSSubmitAlias((const byte *)pose1, (const byte *)pose2,
            &batch, 1, count, blend, NULL, NULL)) {
            VID_VU1WaitDMA();
            continue;
        }
        for (triangle = 0; triangle < batch.triangle_count; triangle++) {
            for (corner = 0; corner < 3; corner++) {
                int vertex = ((batch.indices[triangle] >> (corner * 8)) & 255) / 3;
                int index = batch.vertices[vertex], axis;
                ps2_gs_vertex_t *out = &vertices[triangle * 3 + corner];
                for (axis = 0; axis < 3; axis++)
                    out->position[axis] = pose2 ? pose1[index].v[axis] +
                        blend * (pose2[index].v[axis] - pose1[index].v[axis]) : pose1[index].v[axis];
                out->position[3] = 1;
                memcpy(out->uv, batch.uvs[vertex], 2 * sizeof(float));
                memcpy(out->color, color, sizeof(color));
            }
        }
        Hyena_GSSubmit(HYE_TRIANGLES, vertices, count, 1);
        VID_VU1WaitDMA();
    }
}


qboolean Hyena_DrawMorphMesh(const hyena_morph_mesh_t *header, const byte *first_pose, const byte *second_pose, float blend)
{
    const hyena_morph_vertex_t *pose1 = (const hyena_morph_vertex_t *)first_pose;
    const hyena_morph_vertex_t *pose2 = (const hyena_morph_vertex_t *)second_pose;
    if (ps2_vu1.value && ps2_alias_vu1.value) {
        if (pose2 && (pose1 == pose2 || blend == 0))
            pose2 = NULL;
        else if (pose2 && blend == 1) {
            pose1 = pose2;
            pose2 = NULL;
        }
        gs_alias_t *cache = Hyena_AliasGeometry(header);
        if (!cache) {
            Hyena_DrawAliasStream(header, pose1, pose2, blend, 1);
            return true;
        }
        if (cache->single && ps2_alias_static.value &&
            Hyena_GSSubmitStaticAlias(cache->single, cache->batches, cache->batch_count, cache->count, cache->commands))
            return true;
        const gs_alias_pose_t *first = Hyena_AliasPose(cache, header, pose1);
        const gs_alias_pose_t *second = pose2 ? Hyena_AliasPose(cache, header, pose2) : NULL;
        if (first && !pose2 && ps2_alias_static.value &&
            Hyena_GSSubmitStaticAlias(first->vertices, cache->batches, cache->batch_count, cache->count, NULL))
            return true;
        if (first && (!pose2 || second) && Hyena_GSSubmitAliasPair(first->vertices,
            second ? second->vertices : first->vertices,
            cache->batches, cache->batch_count, cache->count, blend))
            return true;
        if (Hyena_GSSubmitAlias((const byte *)pose1, (const byte *)pose2,
            cache->batches, cache->batch_count, cache->count, blend, NULL, NULL))
            return true;
    }
    Hyena_DrawAliasStream(header, pose1, pose2, blend, 0);
    return true;
}

static byte * hyena_texture_pixels[HYENA_MAX_TEXTURES];
static int hyena_texture_width[HYENA_MAX_TEXTURES];
static int hyena_texture_height[HYENA_MAX_TEXTURES];
static qboolean hyena_texture_16bit[HYENA_MAX_TEXTURES];

extern cvar_t r_profile;
static int hyena_vertex_mode;
static ps2_gs_vertex_t *hyena_vertex_scratch;
static size_t hyena_vertex_capacity;

static ps2_gs_vertex_t *
Hyena_ScratchVertices(size_t count)
{
    if (count > hyena_vertex_capacity) {
        ps2_gs_vertex_t *resized = memalign(64, count * sizeof(*resized));
        if (!resized)
            Sys_Error("Hyena: out of vertex scratch memory");
        free(hyena_vertex_scratch);
        hyena_vertex_scratch = resized;
        hyena_vertex_capacity = count;
    }
    return hyena_vertex_scratch;
}

void
Hyena_SetColor(float r, float g, float b, float a)
{
    Hyena_GSSetColor(r, g, b, a);
}

void
Hyena_BeginVertices(int mode)
{
    hyena_vertex_mode = mode;
    Hyena_GSModelPush();
}

void
Hyena_Translate(float x, float y, float z)
{
    Hyena_GSModelTranslate(x, y, z);
}

void
Hyena_Scale(float x, float y, float z)
{
    Hyena_GSModelScale(x, y, z);
}

void
Hyena_RotateXYZ(float x, float y, float z)
{
    Hyena_GSModelRotate(x * (180.0f / M_PI), 1, 0, 0);
    Hyena_GSModelRotate(y * (180.0f / M_PI), 0, 1, 0);
    Hyena_GSModelRotate(z * (180.0f / M_PI), 0, 0, 1);
}

void
Hyena_RotateZYX(float z, float y, float x)
{
    Hyena_GSModelRotate(z * (180.0f / M_PI), 0, 0, 1);
    Hyena_GSModelRotate(y * (180.0f / M_PI), 0, 1, 0);
    Hyena_GSModelRotate(x * (180.0f / M_PI), 1, 0, 0);
}

void
Hyena_FlushMatrices(void)
{
}

vertex_t *
Hyena_AllocateMemoryForVertices(int count)
{
    return (vertex_t *) malloc(sizeof(vertex_t) * count);
}

void
Hyena_2DTextureCoord(vertex_t * vertex, float u, float v)
{
    vertex->uv.u = u;
    vertex->uv.v = v;
}

void
Hyena_VertexXYZ(vertex_t * vertex, float x, float y, float z)
{
    vertex->xyz.x = x;
    vertex->xyz.y = y;
    vertex->xyz.z = z;
}

void
Hyena_DrawVertices(vertex_t * vertices, int count, int texture_precision, int vertex_precision)
{
    int i;
    int textured = texture_precision != HYE_TEXTURE_NOTEXTURE;
    ps2_gs_vertex_t *scratch = Hyena_ScratchVertices(count);
    float rgba[4];

    (void) vertex_precision;
    Hyena_GSGetColor(rgba);
    for (i = 0; i < count; ++i) {
        scratch[i].position[0] = vertices[i].xyz.x;
        scratch[i].position[1] = vertices[i].xyz.y;
        scratch[i].position[2] = vertices[i].xyz.z;
        scratch[i].position[3] = 1;
        scratch[i].uv[0] = vertices[i].uv.u;
        scratch[i].uv[1] = vertices[i].uv.v;
        memcpy(scratch[i].color, rgba, sizeof(rgba));
    }
    Hyena_GSSubmit(hyena_vertex_mode, scratch, count, textured);
    free(vertices);
}

void
Hyena_EndVertices(void)
{
    Hyena_GSModelPop();
}

void
Hyena_Draw2DQuads(const hyena_2d_quad_t *quads, int count, int texture, unsigned int flags)
{
    int i;
    if (!count)
        return;
    Hyena_GSSetDepthTest(0);
    if (flags & HYE_2D_TEXTURED) {
        int filter = flags & HYE_2D_LINEAR ? HYE_FILTER_LINEAR : HYE_FILTER_NEAREST;

        if (quads[0].u1 - quads[0].u0 < 1.0f && quads[0].v1 - quads[0].v0 < 1.0f)
            filter = hyena_textures[texture].filter;
        Hyena_BindTexture(texture);
        Hyena_TextureFilter(filter);
    }
    Hyena_GSSetBlend((flags & HYE_2D_BLEND) != 0);
    Hyena_GSSetAlphaTest(0, 0);
    for (i = 0; i < count; ++i) {
        const hyena_2d_quad_t *q = &quads[i];
        float color[4] = {q->r / 255.0f, q->g / 255.0f, q->b / 255.0f, q->a / 255.0f};
        Hyena_GSDraw2DQuad(q->x0, q->y0, q->x1, q->y1,
            q->u0, q->v0, q->u1, q->v1, color, (flags & HYE_2D_TEXTURED) != 0);
    }
    Hyena_GSSetBlend(0);
    Hyena_GSSetAlphaTest(1, 0.5f);
}

void
Hyena_FogSet(bool is_world_geometry, float start, float end, float red, float green, float blue, float alpha)
{
    Hyena_GSSetFog(is_world_geometry && !r_fullbright.value, start, end, red * 0.01f, green * 0.01f, blue * 0.01f);
    (void) alpha;
}

static GSTEXTURE hyena_gs_textures[HYENA_MAX_TEXTURES];
static GSTEXTURE hyena_gs_lightmaps[HYENA_MAX_TEXTURES][2];
static unsigned int hyena_lightmap_palette[256] __attribute__((aligned(64)));
static int current_gs_texture = -1;
static byte hyena_lightmap_alpha[HYENA_MAX_TEXTURES];
static byte hyena_lightmap_borrowed[HYENA_MAX_TEXTURES];
static byte hyena_lightmap_dirty[HYENA_MAX_TEXTURES];



static int
Hyena_GSFilter(int filter)
{
    return filter == HYE_NEAREST || filter == HYE_NEAREST_MIPMAP_NEAREST ||
      filter == HYE_NEAREST_MIPMAP_LINEAR ? GS_FILTER_NEAREST : GS_FILTER_LINEAR;
}

void
Hyena_BackendTextureFilter(int texture, int min_filter, int mag_filter)
{
    (void)mag_filter;
    hyena_gs_textures[texture].Filter = Hyena_GSFilter(min_filter);
    if (hyena_textures[texture].lightmap) {
        if (hyena_lightmap_dirty[texture]) {
            size_t bytes = gsKit_texture_size(hyena_texture_width[texture], hyena_texture_height[texture], GS_PSM_T8) * 3;
            SifWriteBackDCache(hyena_texture_pixels[texture], bytes);
            hyena_lightmap_dirty[texture] = 0;
        }
        hyena_gs_lightmaps[texture][0].Filter = hyena_gs_textures[texture].Filter;
        hyena_gs_lightmaps[texture][1].Filter = hyena_gs_textures[texture].Filter;
        Hyena_GSBindLightmap(&hyena_gs_textures[texture],
            &hyena_gs_lightmaps[texture][0], &hyena_gs_lightmaps[texture][1]);
        Hyena_GSSetLightmapAlpha(hyena_lightmap_alpha[texture]);
        Hyena_GSSetLightmapCoherent(1);
    } else
        Hyena_GSBindTexture(&hyena_gs_textures[texture]);
    current_gs_texture = texture;
}

void
Hyena_BackendBindTexture(int texture, int min_filter, int mag_filter)
{
    Hyena_BackendTextureFilter(texture, min_filter, mag_filter);
}

void
Hyena_BackendDestroyTexture(int texture)
{
    int i;

    Hyena_GSFlush();

    Hyena_GSInvalidateLightmap(&hyena_gs_textures[texture]);
    for (i = 0; i < 2; i++) {
        if (hyena_gs_lightmaps[texture][i].Mem && Hyena_GSGlobal())
            gsKit_TexManager_free(Hyena_GSGlobal(), &hyena_gs_lightmaps[texture][i]);
        memset(&hyena_gs_lightmaps[texture][i], 0, sizeof(GSTEXTURE));
    }
    if (hyena_gs_textures[texture].Mem && Hyena_GSGlobal())
        gsKit_TexManager_free(Hyena_GSGlobal(), &hyena_gs_textures[texture]);
    if (!hyena_lightmap_borrowed[texture])
        free(hyena_texture_pixels[texture]);
    hyena_lightmap_dirty[texture] = 0;
    hyena_lightmap_borrowed[texture] = 0;
    hyena_lightmap_alpha[texture] = 1;
    hyena_texture_pixels[texture] = NULL;
    hyena_texture_16bit[texture] = false;
    memset(&hyena_gs_textures[texture], 0, sizeof(hyena_gs_textures[texture]));
    if (current_gs_texture == texture) {
        Hyena_GSBindTexture(NULL);
        current_gs_texture = -1;
    }
}

void
Hyena_BackendInitTextureResources(void)
{
    int i, index;

    for (i = 0; i < 256; i++) {
        index = (i & ~0x18) | ((i & 8) << 1) | ((i & 16) >> 1);
        hyena_lightmap_palette[index] = (unsigned int)i << 24;
    }
    SifWriteBackDCache(hyena_lightmap_palette, sizeof(hyena_lightmap_palette));

    Hyena_ResetTextureRegistry();
    Hyena_InitTextureSettings("128", false, HYENA_MAX_TEXTURES);
}

void
Hyena_BackendUploadLevel(int texture, int level, int storage, int width, int height,
  int x, int y, qboolean replace, const void * pixels)
{
    byte * target;
    const byte * source = pixels;
    int row, column, target_width;
    int bpp = hyena_texture_16bit[texture] ? 2 : 4;

    if (level)
        return;
    if (hyena_gs_textures[texture].Mem)
        Hyena_GSFlush();
    if (replace) {
        if (hyena_gs_textures[texture].Mem && Hyena_GSGlobal())
            gsKit_TexManager_free(Hyena_GSGlobal(), &hyena_gs_textures[texture]);
        free(hyena_texture_pixels[texture]);
        hyena_gs_textures[texture].Clut = NULL;
        hyena_gs_textures[texture].VramClut = 0;
        size_t bytes = (size_t)width * height * bpp;
        size_t allocation = (bytes + 15) & ~(size_t)15;
        hyena_texture_pixels[texture] = memalign(64, allocation);
        if (!hyena_texture_pixels[texture])
            Sys_Error("Hyena: out of texture memory");
        memset(hyena_texture_pixels[texture], 0, allocation);
        hyena_texture_width[texture] = width;
        hyena_texture_height[texture] = height;
        x = y = 0;
    }
    target_width = hyena_texture_width[texture];
    for (row = 0; row < height; ++row) {
        target = hyena_texture_pixels[texture] + ((y + row) * target_width + x) * bpp;
        for (column = 0; column < width; ++column, target += bpp) {
            if (hyena_texture_16bit[texture]) {
                unsigned short color = ((unsigned short)(source[2] >> 3) << 10) |
                  ((unsigned short)(source[1] >> 3) << 5) |
                  ((unsigned short)(source[0] >> 3)) | ((source[3] >= 128) << 15);
                memcpy(target, &color, sizeof(color));
                source += 4;
            } else if (storage == HYE_PIXELS_L8) {
                target[0] = target[1] = target[2] = *source++;
                target[3] = 128;
            } else {
                memcpy(target, source, 3);
                target[3] = (source[3] + 1) >> 1;
                source += 4;
            }
        }
    }
    hyena_gs_textures[texture].Mem = (u32 *)hyena_texture_pixels[texture];
    hyena_gs_textures[texture].Width = target_width;
    hyena_gs_textures[texture].Height = hyena_texture_height[texture];
    hyena_gs_textures[texture].PSM = hyena_texture_16bit[texture] ? GS_PSM_CT16 : GS_PSM_CT32;
    hyena_gs_textures[texture].Delayed = 1;
    hyena_gs_textures[texture].Filter = Hyena_GSFilter(hyena_textures[texture].filter);
    if (!replace && Hyena_GSGlobal())
        gsKit_TexManager_invalidate(Hyena_GSGlobal(), &hyena_gs_textures[texture]);
}

static qboolean
Hyena_UploadIndexedTexture(int texture, const hyena_texture_desc_t *desc)
{
    GSTEXTURE *image = &hyena_gs_textures[texture];
    unsigned int keys[1024];
    unsigned short slots[1024] = {0};
    const byte *source = desc->pixels;
    const byte *palette = desc->palette ? desc->palette : (const byte *)d_8to24table;
    int palette_bpp = desc->palette ? desc->palette_bpp : 4;
    int width = Hyena_TextureDimension(desc->width, false, 4, gl_max_size.value, gl_picmip.value);
    int height = Hyena_TextureDimension(desc->height, false, 4, gl_max_size.value, gl_picmip.value);
    size_t plane_size = (gsKit_texture_size(width, height, GS_PSM_T8) + 63) & ~(size_t)63;
    byte *pixels;
    unsigned int *clut;
    int x, y, count = 0;

    if (width * height < 1024)
        return false;
    pixels = memalign(64, plane_size + 1024);
    if (!pixels)
        Sys_Error("Hyena: out of indexed texture memory");
    memset(pixels, 0, plane_size + 1024);
    clut = (unsigned int *)(pixels + plane_size);
    asset_cache_data_t cached = {0};
    uint32_t source_crc = 0;
    const uint32_t fields[] = { 0x50533201u, width, height, plane_size };
    uint32_t variant = AssetCache_HashWords(fields, sizeof(fields) / sizeof(fields[0]));
    qboolean use_cache = AssetCache_Enabled() && !desc->update;
    qboolean restored = false;
    if (use_cache) {
        source_crc = Hyena_TextureSourceCRC(desc);
        if (AssetCache_Load(ASSET_CACHE_TEXTURE, desc->identifier, source_crc, variant, &cached)) {
            if (cached.size == plane_size + 1024) {
                memcpy(pixels, cached.data, cached.size);
                restored = true;
            }
            free(cached.data);
        }
    }
    if (!restored) {
        for (y = 0; y < height; ++y) {
            int sy = y * desc->height / height;
            for (x = 0; x < width; ++x) {
                int sx = (2 * x + 1) * desc->width / (2 * width);
                int index = sy * desc->width + sx;
                const byte *color = desc->format == HYE_TEXTURE_INDEX8 ?
                    palette + source[index] * palette_bpp : source + index * 4;
                unsigned int alpha = desc->format == HYE_TEXTURE_INDEX8 && palette_bpp == 3 ? 255 : color[3];
                unsigned int rgba = color[0] | (color[1] << 8) | (color[2] << 16) | (((alpha + 1) >> 1) << 24);
                unsigned int hash = (rgba * 2654435761u) >> 22;

                while (slots[hash] && keys[hash] != rgba)
                    hash = (hash + 1) & 1023;
                if (!slots[hash]) {
                    int swizzled;
                    if (count == 256) {
                        free(pixels);
                        return false;
                    }
                    swizzled = (count & ~0x18) | ((count & 8) << 1) | ((count & 16) >> 1);
                    clut[swizzled] = rgba;
                    slots[hash] = ++count;
                    keys[hash] = rgba;
                }
                pixels[y * width + x] = slots[hash] - 1;
            }
        }
        if (use_cache) {
            memset(&cached, 0, sizeof(cached));
            cached.data = pixels;
            cached.size = plane_size + 1024;
            AssetCache_Store(ASSET_CACHE_TEXTURE, desc->identifier, source_crc, variant, &cached);
        }
    }
    if (image->Mem && Hyena_GSGlobal())
        gsKit_TexManager_free(Hyena_GSGlobal(), image);
    free(hyena_texture_pixels[texture]);
    memset(image, 0, sizeof(*image));
    hyena_texture_pixels[texture] = pixels;
    hyena_texture_width[texture] = width;
    hyena_texture_height[texture] = height;
    image->Mem = (u32 *)pixels;
    image->Width = width;
    image->Height = height;
    image->PSM = GS_PSM_T8;
    image->Clut = clut;
    image->ClutPSM = GS_PSM_CT32;
    image->Delayed = 1;
    hyena_textures[texture].width = width;
    hyena_textures[texture].height = height;
    hyena_textures[texture].bpp = 1;
    hyena_textures[texture].mipmaps = 0;
    Hyena_BackendTextureFilter(texture, desc->filter == HYE_FILTER_LINEAR ? HYE_LINEAR : HYE_NEAREST,
        desc->filter == HYE_FILTER_LINEAR ? HYE_LINEAR : HYE_NEAREST);
    return true;
}

void
Hyena_BackendUploadTexture(int texture, const hyena_texture_desc_t * desc)
{
    hyena_texture_desc_t upload = *desc;

    upload.mip_levels = 0;
    if (upload.lightmap) {
        int channel, x, y, left, top, right, bottom, stride;
        const byte * source = upload.pixels;
        byte * pixels;
        size_t plane_size = gsKit_texture_size(upload.width, upload.height, GS_PSM_T8);
        qboolean replace = !hyena_texture_pixels[texture] ||
          hyena_texture_width[texture] != upload.width ||
          hyena_texture_height[texture] != upload.height;

        Hyena_GSInvalidateLightmap(&hyena_gs_textures[texture]);
        if (replace) {
            Hyena_BackendDestroyTexture(texture);
            pixels = upload.format == HYE_TEXTURE_PLANAR_RGB8 ? (byte *)upload.pixels : memalign(64, plane_size * 3);
            if (!pixels)
                Sys_Error("Hyena: out of lightmap memory");
            if (upload.format != HYE_TEXTURE_PLANAR_RGB8)
                memset(pixels, 0, plane_size * 3);
            hyena_lightmap_borrowed[texture] = upload.format == HYE_TEXTURE_PLANAR_RGB8;
            hyena_texture_pixels[texture] = pixels;
            hyena_texture_width[texture] = upload.width;
            hyena_texture_height[texture] = upload.height;
        }
        hyena_lightmap_alpha[texture] = upload.alpha;
        pixels = hyena_texture_pixels[texture];
        left = !replace && upload.update_width ? upload.update_x : 0;
        top = !replace && upload.update_height ? upload.update_y : 0;
        right = !replace && upload.update_width ? left + upload.update_width : upload.width;
        bottom = !replace && upload.update_height ? top + upload.update_height : upload.height;
        stride = upload.row_stride ? upload.row_stride : upload.width * (upload.format == HYE_TEXTURE_INDEX8 ? 1 : 4);
        for (channel = 0; channel < 3; channel++) {
            GSTEXTURE * image = channel ? &hyena_gs_lightmaps[texture][channel - 1] : &hyena_gs_textures[texture];

            for (y = top; !hyena_lightmap_borrowed[texture] && y < bottom; ++y) {
                byte *dest = pixels + channel * plane_size + y * upload.width;
                const byte *row = source + y * stride;
                for (x = left; x < right; ++x)
                    dest[x] = upload.format == HYE_TEXTURE_INDEX8 ? row[x] : row[x * 4 + channel];
            }
            image->Mem = (u32 *)(pixels + channel * plane_size);
            image->Width = upload.width;
            image->Height = upload.height;
            image->PSM = GS_PSM_T8;
            image->Clut = hyena_lightmap_palette;
            image->ClutPSM = GS_PSM_CT32;
            image->ClutStorageMode = 0;
            image->Delayed = 1;
            if (!replace && Hyena_GSGlobal())
                gsKit_TexManager_invalidate(Hyena_GSGlobal(), image);
        }
        hyena_textures[texture].width = upload.width;
        hyena_textures[texture].height = upload.height;
        hyena_textures[texture].bpp = 3;
        hyena_textures[texture].mipmaps = 0;
        hyena_lightmap_dirty[texture] = 1;
        Hyena_BackendTextureFilter(texture, HYE_LINEAR, HYE_LINEAR);
    } else if (!Hyena_UploadIndexedTexture(texture, desc)) {
        int i;
        const byte * pixels = desc->pixels;

        hyena_texture_16bit[texture] = !desc->alpha;
        if (desc->format == HYE_TEXTURE_RGBA8) {
            hyena_texture_16bit[texture] = true;
            for (i = 0; i < desc->width * desc->height; i++) {
                if (pixels[i * 4 + 3] != 255) {
                    hyena_texture_16bit[texture] = false;
                    break;
                }
            }
        }
        Hyena_PrepareTextureUpload(texture, &upload, HYE_PIXELS_RGBA8, Hyena_BackendUploadLevel);
    }
    if (!desc->lightmap)
        Hyena_GSSetReplaceTexture(0);
}

static float hyena_alpha_reference = 0.5f;

void
Hyena_SetTextureMode(int mode)
{
    Hyena_GSSetReplaceTexture(mode != HYE_MODULATE);
}

void
Hyena_EnableCapability(int capability)
{
    switch (capability) {
        case HYE_BLEND: Hyena_GSSetBlend(1); break;
        case HYE_TEXTURE_2D: Hyena_GSSetTexturing(1); break;
        case HYE_ALPHA_TEST: Hyena_GSSetAlphaTest(1, hyena_alpha_reference); break;
        case HYE_DEPTH_TEST: Hyena_GSSetDepthTest(1); break;
        case HYE_CULL_FACE: break;
        default: Sys_Error("Hyena: unknown capability %d", capability);
    }
}

void
Hyena_DisableCapability(int capability)
{
    switch (capability) {
        case HYE_BLEND: Hyena_GSSetBlend(0); break;
        case HYE_TEXTURE_2D: Hyena_GSSetTexturing(0); break;
        case HYE_ALPHA_TEST: Hyena_GSSetAlphaTest(0, hyena_alpha_reference); break;
        case HYE_DEPTH_TEST: Hyena_GSSetDepthTest(0); break;
        case HYE_CULL_FACE: break;
        default: Sys_Error("Hyena: unknown capability %d", capability);
    }
}

void
Hyena_DepthMask(qboolean value)
{
    Hyena_GSSetDepthWrite(value);
}

void
Hyena_SetShadeMode(int mode)
{
    (void)mode;
}

void
Hyena_SetBlendFunction(int source, int destination)
{
    Hyena_GSSetLightmapBlend(source == HYE_DST_COLOR && destination == HYE_SRC_COLOR);
    if (source == HYE_DST_COLOR && destination == HYE_SRC_COLOR)
        Hyena_GSSetBlendEquation(1, 2, 0, 2, 0);
    else if (source == HYE_ZERO && destination == HYE_SRC_COLOR)
        Hyena_GSSetBlendEquation(1, 2, 0, 2, 0);
    else if (source == HYE_ONE && destination == HYE_ONE)
        Hyena_GSSetBlendEquation(0, 2, 2, 1, 128);
    else if (source == HYE_SRC_ALPHA && destination == HYE_ONE)
        Hyena_GSSetBlendEquation(0, 2, 0, 1, 0);
    else
        Hyena_GSSetBlendEquation(0, 1, 0, 1, 0);
}

void
Hyena_SetDepthRange(float near_value, float far_value)
{
    Hyena_GSSetDepthRange(near_value, far_value);
}

void
Hyena_ResetDepthRange(void)
{
    Hyena_SetDepthRange(0, 1);
}

void
Hyena_SetDepthOffset(float offset)
{
    (void) offset;
}

void
Hyena_Set2D(void)
{
    Hyena_GSProjectionIdentity();
    Hyena_GSProjectionOrtho(0, vid.width, vid.height, 0, -99999, 99999);
    Hyena_GSModelIdentity();
    Hyena_GSSetDepthTest(0);
}

void
Hyena_FogEnable(void)
{
    Hyena_GSSetFogEnabled(1);
}

void
Hyena_FogDisable(void)
{
    Hyena_GSSetFogEnabled(0);
}

void
Hyena_FogInit(void)
{
}

void
Hyena_DrawIndexedTriangles(const hyena_colored_vertex_t * vertices, int vertex_count, const unsigned short * indices,
  int index_count, qboolean textured)
{
    int first;
    ps2_gs_vertex_t *scratch = Hyena_ScratchVertices(768);

    for (first = 0; first < index_count; first += 768) {
        int i, count = index_count - first;
        if (count > 768)
            count = 768;
        for (i = 0; i < count; ++i) {
            const hyena_colored_vertex_t *vertex;
            ps2_gs_vertex_t *v = &scratch[i];
            unsigned short index = indices[first + i];
            if (index >= vertex_count)
                Sys_Error("Hyena: vertex index out of range");
            vertex = &vertices[index];
            v->position[0] = vertex->xyz[0];
            v->position[1] = vertex->xyz[1];
            v->position[2] = vertex->xyz[2];
            v->position[3] = 1;
            v->uv[0] = vertex->uv[0];
            v->uv[1] = vertex->uv[1];
            v->color[0] = vertex->color[0] / 255.0f;
            v->color[1] = vertex->color[1] / 255.0f;
            v->color[2] = vertex->color[2] / 255.0f;
            v->color[3] = vertex->color[3] / 255.0f;
        }
        Hyena_GSSubmit(HYE_TRIANGLES, scratch, count, textured);
    }
    Hyena_GSSetColor(1, 1, 1, 1);
} /* Hyena_DrawIndexedTriangles */

struct hyena_buffer_s {
    int          target;
    size_t       size, capacity;
    void *       data;
};
static int hyena_arrays_mask;
static hyena_arrays_t hyena_arrays;

size_t
Hyena_StaticVertexBudget(void)
{
    return 1024 * 1024;
}

qboolean
Hyena_NeedsColorArray(void)
{
    return false;
}

hyena_buffer_t *
Hyena_CreateBuffer(int target, const void * data, size_t size)
{
    hyena_buffer_t * buffer = calloc(1, sizeof(*buffer));

    if (!buffer)
        Sys_Error("Hyena_CreateBuffer: out of memory");
    buffer->target = target;
    if (size)
        Hyena_UpdateBuffer(buffer, data, size);
    return buffer;
}

void
Hyena_UpdateBuffer(hyena_buffer_t * buffer, const void * data, size_t size)
{
    void * resized;

    if (buffer->target == HYE_CLIENT_INDEX_BUFFER) {
        buffer->data = (void *) data;
        buffer->size = size;
        return;
    }
    if (buffer->data)
        Hyena_InvalidateWorld(buffer->data, buffer->size);
    if (!size) {
        free(buffer->data);
        buffer->data = NULL;
        buffer->capacity = 0;
    } else {
        if (size > buffer->capacity) {
            resized = realloc(buffer->data, size);
            if (!resized)
                Sys_Error("Hyena_UpdateBuffer: out of memory");
            buffer->data = resized;
            buffer->capacity = size;
        }
        if (data)
            memcpy(buffer->data, data, size);
    }
    buffer->size = size;
}

void
Hyena_DestroyBuffer(hyena_buffer_t * buffer)
{
    if (!buffer)
        return;
    if (buffer->target != HYE_CLIENT_INDEX_BUFFER) {
        if (buffer->data)
            Hyena_InvalidateWorld(buffer->data, buffer->size);
        free(buffer->data);
    }
    free(buffer);
}

static const void *
Hyena_ArrayPointer(const hyena_array_t * array, int index, int components)
{
    const byte * base = array->buffer ? (const byte *) array->buffer->data + (size_t) array->data : array->data;
    int size = array->type == HYE_FLOAT ? sizeof(float) : array->type == HYE_SHORT ? sizeof(short) : 1;

    return base + index * (array->stride ? array->stride : components * size);
}

static void
Hyena_ArrayValues(const hyena_array_t * array, int index, float * value, int count)
{
    const void * data = Hyena_ArrayPointer(array, index, count);
    int i;

    for (i = 0; i < count; ++i) {
        if (array->type == HYE_SHORT)
            value[i] = ((const short *) data)[i];
        else if (array->type == HYE_UBYTE)
            value[i] = ((const byte *) data)[i] / 255.0f;
        else
            value[i] = ((const float *) data)[i];
    }
}

void
Hyena_BeginArrays(int mask)
{
    hyena_arrays_mask = mask;
}

void
Hyena_EndArrays(void)
{
    hyena_arrays_mask = 0;
}

void
Hyena_SetArrays(const hyena_arrays_t * arrays)
{
    hyena_arrays = *arrays;
}

static void
Hyena_ArrayVertex(const hyena_arrays_t * arrays, int index, ps2_gs_vertex_t *vertex)
{
    float value[4];

    vertex->uv[0] = vertex->uv[1] = 0;
    if (arrays->uv.data || arrays->uv.buffer) {
        Hyena_ArrayValues(&arrays->uv, index, value, 2);
        vertex->uv[0] = value[0];
        vertex->uv[1] = value[1];
    }
    if (arrays->color.data || arrays->color.buffer) {
        Hyena_ArrayValues(&arrays->color, index, vertex->color, 4);
    } else
        Hyena_GSGetColor(vertex->color);
    Hyena_ArrayValues(&arrays->position, index, value, 3);
    vertex->position[0] = value[0];
    vertex->position[1] = value[1];
    vertex->position[2] = value[2];
    vertex->position[3] = 1;
}

static void
Hyena_DrawArrayRange(int mode, const hyena_arrays_t * arrays, int count, const unsigned short * indices)
{
    int first, i, packed;
    int position_stride = 0, uv_stride = 0;
    const byte *positions = NULL, *uvs = NULL;
    float color[4];
    ps2_gs_vertex_t *scratch;

    if (!arrays)
        arrays = &hyena_arrays;
    if (arrays->scale != 0 && arrays->scale != 1) {
        Hyena_GSModelPush();
        Hyena_GSModelScale(arrays->scale, arrays->scale, arrays->scale);
    }
    if (!indices && arrays->position.type == HYE_FLOAT && arrays->uv.type == HYE_FLOAT && arrays->color.type == HYE_FLOAT &&
        arrays->position.stride == sizeof(ps2_gs_vertex_t) && arrays->uv.stride == sizeof(ps2_gs_vertex_t) &&
        arrays->color.stride == sizeof(ps2_gs_vertex_t) && arrays->position.data &&
        (const byte *)arrays->uv.data == (const byte *)arrays->position.data + offsetof(ps2_gs_vertex_t, uv) &&
        (const byte *)arrays->color.data == (const byte *)arrays->position.data + offsetof(ps2_gs_vertex_t, color)) {
        Hyena_GSSubmit(mode, arrays->position.data, count, 1);
        if (arrays->scale != 0 && arrays->scale != 1)
            Hyena_GSModelPop();
        return;
    }
    packed = (arrays->position.type == HYE_FLOAT || arrays->position.type == HYE_SHORT) &&
        arrays->uv.type == HYE_FLOAT &&
        (arrays->uv.data || arrays->uv.buffer) && !arrays->color.data && !arrays->color.buffer;
    if (packed) {
        positions = Hyena_ArrayPointer(&arrays->position, 0, 3);
        uvs = Hyena_ArrayPointer(&arrays->uv, 0, 2);
        position_stride = arrays->position.stride ? arrays->position.stride :
            3 * (arrays->position.type == HYE_SHORT ? sizeof(short) : sizeof(float));
        uv_stride = arrays->uv.stride ? arrays->uv.stride : 2 * sizeof(float);
        Hyena_GSGetColor(color);
    }
    if (packed && arrays->position.type == HYE_FLOAT && mode == HYE_TRIANGLES &&
        Hyena_GSSubmitIndexed(positions, position_stride, uvs, uv_stride, indices, count,
            arrays->position.buffer && arrays->uv.buffer)) {
        if (arrays->scale != 0 && arrays->scale != 1)
            Hyena_GSModelPop();
        return;
    }
    scratch = Hyena_ScratchVertices(mode == HYE_TRIANGLES ? 768 : count);
    for (first = 0; first < count; ) {
        double pack_start = r_profile.value ? Sys_FloatTime() : 0;
        int batch_count = mode == HYE_TRIANGLES && count - first > 768 ? 768 : count - first;
        if (packed && arrays->position.type == HYE_SHORT) {
            for (i = 0; i < batch_count; ++i) {
                int index = indices ? indices[first + i] : first + i;
                const short *position = (const short *)(positions + index * position_stride);
                const float *uv = (const float *)(uvs + index * uv_stride);
                scratch[i].position[0] = position[0];
                scratch[i].position[1] = position[1];
                scratch[i].position[2] = position[2];
                scratch[i].position[3] = 1;
                scratch[i].uv[0] = uv[0];
                scratch[i].uv[1] = uv[1];
                memcpy(scratch[i].color, color, sizeof(color));
            }
        } else if (packed) {
            for (i = 0; i < batch_count; ++i) {
                int index = indices ? indices[first + i] : first + i;
                const float *position = (const float *)(positions + index * position_stride);
                const float *uv = (const float *)(uvs + index * uv_stride);
                scratch[i].position[0] = position[0];
                scratch[i].position[1] = position[1];
                scratch[i].position[2] = position[2];
                scratch[i].position[3] = 1;
                scratch[i].uv[0] = uv[0];
                scratch[i].uv[1] = uv[1];
                memcpy(scratch[i].color, color, sizeof(color));
            }
        } else {
            for (i = 0; i < batch_count; ++i)
                Hyena_ArrayVertex(arrays, indices ? indices[first + i] : first + i, &scratch[i]);
        }
        if (r_profile.value)
            Hyena_GSAddPackingTime(Sys_FloatTime() - pack_start);
        Hyena_GSSubmit(mode, scratch, batch_count, arrays->uv.data || arrays->uv.buffer);
        first += batch_count;
    }
    if (arrays->scale != 0 && arrays->scale != 1)
        Hyena_GSModelPop();
}

void
Hyena_DrawElements(int mode, const hyena_arrays_t * arrays, int count,
  hyena_buffer_t * indices, size_t offset)
{
    Hyena_DrawArrayRange(mode, arrays, count, (const unsigned short *) ((const byte *) indices->data + offset));
}

void
Hyena_DrawArrays(int mode, const hyena_arrays_t * arrays, int count)
{
    Hyena_DrawArrayRange(mode, arrays, count, NULL);
}

void
Hyena_DepthFunction(int function)
{
    (void) function;
}

void
Hyena_AlphaFunction(int function, float value)
{
    (void) function;
    hyena_alpha_reference = value;
    Hyena_GSSetAlphaTest(1, value);
}

void
Hyena_TextureFilter(int filter)
{
    if (current_gs_texture >= 0) {
        int mode = filter == HYE_FILTER_LINEAR ? GS_FILTER_LINEAR : GS_FILTER_NEAREST;

        hyena_gs_textures[current_gs_texture].Filter = mode;
        hyena_gs_lightmaps[current_gs_texture][0].Filter = mode;
        hyena_gs_lightmaps[current_gs_texture][1].Filter = mode;
    }
}

void
Hyena_LoadViewMatrix(const float * matrix)
{
    Hyena_GSModelLoad(matrix);
}

void
Hyena_InvalidateTextureBinding(void)
{
    current_gs_texture = -1;
    Hyena_GSBindTexture(NULL);
}

void
Hyena_SetCurrentColor(float r, float g, float b, float a)
{
    Hyena_GSSetColor(r, g, b, a);
}

qboolean
Hyena_PackedArraysSupported(void)
{
    return false;
}

qboolean
Hyena_SeparateArrays(void)
{
    return true;
}

qboolean
Hyena_HardwareClipping(void)
{
    return true;
}

qboolean
Hyena_SeparateViewMatrix(void)
{
    return false;
}

void
Hyena_TextureTransform(float uscale, float vscale, float uoffset, float voffset)
{
    Hyena_GSSetTextureTransform(uscale, vscale, uoffset, voffset);
}

void
Hyena_TextureAlpha(float alpha)
{
    Hyena_SetCurrentColor(1, 1, 1, alpha);
    Hyena_SetTextureMode(HYE_MODULATE);
}

void
Hyena_SetPalette(const unsigned int * rgba, int count)
{
    (void) rgba;
    (void) count;
}

void
Hyena_MorphWeight(int index, float weight)
{
    Sys_Error("Hyena: packed morphs unavailable");
}

void *
Hyena_AllocateTransient(size_t size)
{
    Sys_Error("Hyena: packed allocation unavailable");
    return NULL;
}

void
Hyena_DrawPacked(int mode, int format, int morphs, int count, const void * vertices,
  const unsigned short * indices, unsigned int flags)
{
    Sys_Error("Hyena: packed arrays unavailable");
}

void
Hyena_RotationMatrix(float * matrix, float x, float y, float z)
{
    Sys_Error("Hyena: separate view matrix unavailable");
}

void *
Hyena_MapBuffer(hyena_buffer_t * buffer)
{
    if (buffer->data)
        Hyena_InvalidateWorld(buffer->data, buffer->size);
    return buffer->data;
}

void
Hyena_UnmapBuffer(hyena_buffer_t * buffer)
{
    (void) buffer;
}

void
Hyena_PS2PrintMemory(void)
{
    size_t textures = 0;
    int i;

    for (i = 0; i < HYENA_MAX_TEXTURES; ++i) {
        if (!hyena_texture_pixels[i])
            continue;
        textures += hyena_texture_width[i] * hyena_texture_height[i] *
            (hyena_textures[i].lightmap ? 3 : hyena_gs_textures[i].PSM == GS_PSM_T8 ? 1 : hyena_texture_16bit[i] ? 2 : 4);
        if (!hyena_textures[i].lightmap && hyena_gs_textures[i].PSM == GS_PSM_T8)
            textures += 1024;
    }
    Con_Printf("PS2 texture pixels: %u KB, vertex scratch: %u KB\n",
        (unsigned)textures / 1024, (unsigned)(hyena_vertex_capacity * sizeof(*hyena_vertex_scratch)) / 1024);
}

static int hyena_matrix_mode;
void Hyena_MatrixMode(int mode) { hyena_matrix_mode = mode; }
void Hyena_LoadIdentity(void)
{
    if (hyena_matrix_mode == HYE_PROJECTION) Hyena_GSProjectionIdentity();
    else Hyena_GSModelIdentity();
}
void Hyena_LoadMatrix(const float *matrix)
{
    if (hyena_matrix_mode == HYE_PROJECTION) {
        memcpy(projection[projection_depth], matrix, sizeof(projection[0]));
        matrix_dirty = 1;
    } else Hyena_GSModelLoad(matrix);
}
void Hyena_GetMatrix(float *matrix)
{
    if (hyena_matrix_mode == HYE_PROJECTION) memcpy(matrix, projection[projection_depth], sizeof(projection[0]));
    else Hyena_GSModelGet(matrix);
}
void Hyena_PushMatrix(void)
{
    if (hyena_matrix_mode == HYE_PROJECTION) Hyena_GSProjectionPush();
    else Hyena_GSModelPush();
}
void Hyena_PopMatrix(void)
{
    if (hyena_matrix_mode == HYE_PROJECTION) Hyena_GSProjectionPop();
    else Hyena_GSModelPop();
}
void Hyena_MatrixTranslate(float x, float y, float z)
{
    if (hyena_matrix_mode == HYE_PROJECTION) Hyena_GSProjectionTranslate(x, y, z);
    else Hyena_GSModelTranslate(x, y, z);
}
void Hyena_MatrixScale(float x, float y, float z)
{
    if (hyena_matrix_mode == HYE_PROJECTION) Hyena_GSProjectionScale(x, y, z);
    else Hyena_GSModelScale(x, y, z);
}
void Hyena_MatrixRotate(float degrees, float x, float y, float z)
{
    if (hyena_matrix_mode == HYE_PROJECTION) {
        Hyena_GSRotate(projection[projection_depth], degrees, x, y, z);
        matrix_dirty = 1;
    } else Hyena_GSModelRotate(degrees, x, y, z);
}
void Hyena_UpdateMatrices(void) { }
void Hyena_Frustum(float left, float right, float bottom, float top, float near_value, float far_value)
{
    Hyena_GSProjectionFrustum(left, right, bottom, top, near_value, far_value);
}
void Hyena_Viewport(int x, int y, int width, int height)
{
    Hyena_GSProjectionTranslate((2.0f * x + width) / glwidth - 1, (2.0f * y + height) / glheight - 1, 0);
    Hyena_GSProjectionScale((float)width / glwidth, (float)height / glheight, 1);
}
void Hyena_ClipFrustum(float fovy, float aspect, float near_value) { (void)fovy; (void)aspect; (void)near_value; }
void Hyena_CullFace(qboolean back) { (void)back; }
void Hyena_ClearColor(float red, float green, float blue, float alpha) { Hyena_GSSetClearColor(red, green, blue, alpha); }
void Hyena_Clear(qboolean color, qboolean depth, qboolean stencil) { (void)stencil; Hyena_GSClear(color, depth); }
void Hyena_DepthBounds(float near_value, float far_value)
{
    gldepthmin = near_value;
    gldepthmax = far_value;
    VID_SetDepthRange(near_value, far_value);
}
void Hyena_PerspectiveCorrection(qboolean enabled) { (void)enabled; }
void Hyena_BlendConstant(float alpha) { Hyena_GSSetBlendEquation(0, 1, 2, 1, (int)(bound(0, alpha, 1) * 128)); }
void Hyena_Finish(void) { VID_VU1Finish(); }
qboolean Hyena_ReadPixels(int x, int y, int width, int height, byte *rgba)
{
    byte *row;
    int i;
    if (!ps2_gs || ps2_gs->PSM != GS_PSM_CT32 || x < 0 || y < 0 ||
        width < 1 || height < 1 || x + width > ps2_gs->Width || y + height > ps2_gs->Height)
        return false;
    row = memalign(64, ps2_gs->Width * 4);
    if (!row) return false;
    VID_VU1Finish();
    VID_PresentBegin(ps2_gs);
    for (i = 0; i < height; i++) {
        int buffer = ps2_gs->ActiveBuffer ^ (ps2_gs->DoubleBuffering == GS_SETTING_ON);
        if (!ps2_screenshot(row, ps2_gs->ScreenBuffer[buffer] / 256, 0,
            ps2_gs->Height - y - i - 1, ps2_gs->Width, 1, ps2_gs->PSM)) {
            free(row);
            return false;
        }
        memcpy(rgba + i * width * 4, row + x * 4, width * 4);
    }
    free(row);
    return true;
}

void Hyena_AddLightTime(double seconds) { Hyena_GSAddEntityLightTime(seconds); }

void Hyena_InitTextureStorage(void) { }
size_t Hyena_TextureBudget(void) { return 0; }
int Hyena_PreferredPaletteBits(void) { return 8; }

void Hyena_SelectDrawBuffer(qboolean front) { (void)front; }
void Hyena_SelectReadBuffer(qboolean front) { (void)front; }

qboolean Hyena_MorphMeshesSupported(void) { return true; }

static byte hyena_pixel_arena[128 * 128 * 128] __attribute__((aligned(64)));
static size_t hyena_pixel_used;
static int hyena_pixel_allocations;
void *Hyena_AllocateTexturePixels(size_t bytes)
{
    bytes = (bytes + 63) & ~(size_t)63;
    if (bytes <= sizeof(hyena_pixel_arena) - hyena_pixel_used) {
        byte *pixels = hyena_pixel_arena + hyena_pixel_used;
        hyena_pixel_used += bytes;
        hyena_pixel_allocations++;
        return pixels;
    }
    return memalign(64, bytes);
}
void Hyena_FreeTexturePixels(void *pixels)
{
    if ((uintptr_t)pixels >= (uintptr_t)hyena_pixel_arena && (uintptr_t)pixels < (uintptr_t)(hyena_pixel_arena + sizeof(hyena_pixel_arena))) {
        if (!--hyena_pixel_allocations)
            hyena_pixel_used = 0;
    } else free(pixels);
}
int Hyena_PreferredLightmapSize(void) { return 128; }

hyena_texture_format_t Hyena_PreferredLightmapFormat(void) { return HYE_TEXTURE_PLANAR_RGB8; }
unsigned *Hyena_LightmapScratch(int count) { return count <= 4096 ? (unsigned *)0x70000000 : NULL; }
extern void Hyena_GSScaleLightmap(unsigned *dest, const byte *source, int count, unsigned scale);
extern void Hyena_GSAddLightmap(unsigned *dest, const byte *source, int count, unsigned scale);
qboolean Hyena_AccumulateLightmap(unsigned *dest, const byte *source, int count, unsigned scale, qboolean add)
{
    if (scale > 32767 || count < 16) return false;
    if (add) Hyena_GSAddLightmap(dest, source, count, scale);
    else Hyena_GSScaleLightmap(dest, source, count, scale);
    return true;
}
void Hyena_PrepareTextureUpdate(int texture)
{
    (void)texture;
    if (VID_QueueEnabled()) VID_VU1WaitDMA();
}
void Hyena_ClipArrays(const hyena_arrays_t *source, hyena_arrays_t *clipped) { *clipped = *source; }
void Hyena_BeginModelClipping(void) { }
void Hyena_EndModelClipping(void) { }

void Hyena_ClearMorphMeshes(void) { Hyena_ClearAliasCache(); }
