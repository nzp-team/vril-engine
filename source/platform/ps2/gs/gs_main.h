/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef VRIL_PS2_GS_MAIN_H
#define VRIL_PS2_GS_MAIN_H

#include <gsKit.h>
#include <packet2.h>
#include <string.h>
#include "gs_vu1_layout.h"

typedef render_sprite_vertex_t ps2_gs_vertex_t;

int Hyena_ClipPolygon(const ps2_gs_vertex_t *vertices, int count,
    ps2_gs_vertex_t *output, int capacity);

typedef struct ps2_gs_static_s ps2_gs_static_t;

#define PS2_GS_ALIAS_VERTICES VU_VERTICES
#define PS2_GS_ALIAS_TRIANGLES VU_TRIANGLES

typedef struct {
    unsigned int indices[PS2_GS_ALIAS_TRIANGLES];
    float uvs[PS2_GS_ALIAS_VERTICES][2];
    unsigned short vertices[PS2_GS_ALIAS_VERTICES];
    int vertex_count, triangle_count, strip_count;
} __attribute__((aligned(64))) ps2_gs_alias_batch_t;

int Hyena_BuildAliasBatch(ps2_gs_alias_batch_t *batch, const unsigned short *indices,
    const float *uvs, int count);

struct ps2_gs_static_s {
    packet2_t *packet;
    unsigned int (*counts)[4];
};

typedef struct {
    const float (*positions)[4];
    const float (*uvs)[2];
    const unsigned int *indices;
    int vertex_count, triangle_count;
} ps2_gs_world_batch_t;

typedef struct {
    ps2_gs_world_batch_t batch;
    float positions[84][4] __attribute__((aligned(16)));
    float uvs[84][2];
    unsigned int indices[64];
} ps2_gs_world_scratch_t;

void Hyena_WorldFrame(void);
void Hyena_InvalidateWorld(const void *data, size_t size);
const ps2_gs_world_batch_t *Hyena_CacheWorldBatch(ps2_gs_world_scratch_t *scratch,
    const unsigned char *positions, int position_stride, const unsigned char *uvs,
    int uv_stride, const unsigned short *indices, int first, int count, int vertex_limit);

static inline int Hyena_BuildWorldBatch(ps2_gs_world_scratch_t *scratch,
    const unsigned char *positions, int position_stride, const unsigned char *uvs,
    int uv_stride, const unsigned short *indices, int first, int count, int vertex_limit)
{
    ps2_gs_world_batch_t *batch = &scratch->batch;
    unsigned short keys[256];
    unsigned char values[256];
    int i, corner;
    batch->positions = scratch->positions;
    batch->uvs = scratch->uvs;
    batch->indices = scratch->indices;
    memset(values, 0, sizeof(values));
    batch->vertex_count = batch->triangle_count = 0;
    for (i = 0; i < count && i < 64 && batch->vertex_count <= vertex_limit - 3; ++i) {
        unsigned int packed = 0;
        for (corner = 0; corner < 3; ++corner) {
            unsigned short source = indices[(first + i) * 3 + corner];
            unsigned int slot = (source * 0x9e37u >> 8) & 255;
            int vertex;
            while (values[slot] && keys[slot] != source)
                slot = (slot + 1) & 255;
            if (!values[slot]) {
                const float *position = (const float *)(positions + source * position_stride);
                const float *uv = (const float *)(uvs + source * uv_stride);
                vertex = batch->vertex_count++;
                keys[slot] = source;
                values[slot] = vertex + 1;
                memcpy(scratch->positions[vertex], position, 3 * sizeof(float));
                scratch->positions[vertex][3] = 1;
                memcpy(scratch->uvs[vertex], uv, 2 * sizeof(float));
            } else {
                vertex = values[slot] - 1;
            }
            packed |= (unsigned int)(vertex * 3) << (corner * 8);
        }
        scratch->indices[i] = packed;
    }
    batch->triangle_count = i;
    if (batch->vertex_count & 1)
        memset(scratch->uvs[batch->vertex_count], 0, 2 * sizeof(float));
    while (i & 3)
        scratch->indices[i++] = 0;
    return batch->triangle_count;
}

void Hyena_GSInit(void);
void Hyena_GSBeginFrame(void);
void Hyena_GSFlush(void);
void Hyena_GSAddPackingTime(double seconds);
void Hyena_GSAddEntityLightTime(double seconds);
void Hyena_GSEndFrame(int vsync);
GSGLOBAL *Hyena_GSGlobal(void);
void Hyena_GSSetFog(int world, float start, float end, float r, float g, float b);
void Hyena_GSSetFogEnabled(int enabled);
void Hyena_GSSetClearColor(float r, float g, float b, float a);
void Hyena_GSClear(int color, int depth);
void Hyena_GSSetColor(float r, float g, float b, float a);
void Hyena_GSGetColor(float rgba[4]);
void Hyena_GSSetBlend(int enabled);
void Hyena_GSSetBlendEquation(int a, int b, int c, int d, int fix);
void Hyena_GSSetAlphaTest(int enabled, float threshold);
void Hyena_GSSetDepthTest(int enabled);
void Hyena_GSSetDepthWrite(int enabled);
void Hyena_GSSetDepthRange(float near_value, float far_value);
void Hyena_GSSetTexturing(int enabled);
void Hyena_GSSetReplaceTexture(int enabled);
void Hyena_GSBindTexture(GSTEXTURE *texture);
void Hyena_GSInvalidateLightmap(GSTEXTURE *texture);
void Hyena_GSBindLightmap(GSTEXTURE *red, GSTEXTURE *green, GSTEXTURE *blue);
void Hyena_GSSetLightmapBlend(int enabled);
void Hyena_GSSetLightmapAlpha(int enabled);
void Hyena_GSSetLightmapCoherent(int coherent);
void Hyena_GSDraw2DQuad(float x0, float y0, float x1, float y1,
    float u0, float v0, float u1, float v1, const float color[4], int textured);
int Hyena_GSSubmitAlias(const unsigned char *pose1, const unsigned char *pose2,
    const ps2_gs_alias_batch_t *batches, int batch_count, int count, float blend, const unsigned int *packed, const ps2_gs_static_t *commands);
int Hyena_GSSubmitAliasPair(const unsigned int *first, const unsigned int *second,
    const ps2_gs_alias_batch_t *batches, int batch_count, int count, float blend);
int Hyena_GSSubmitStaticAlias(const unsigned int *packed, const ps2_gs_alias_batch_t *batches,
    int batch_count, int count, const ps2_gs_static_t *commands);
int Hyena_GSSubmitIndexed(const void *positions, int position_stride, const void *uvs,
    int uv_stride, const unsigned short *indices, int count, int cache);
void Hyena_GSSubmit(int mode, const ps2_gs_vertex_t *vertices, int count, int textured);
void Hyena_GSProjectionIdentity(void);
void Hyena_GSProjectionPush(void);
void Hyena_GSProjectionPop(void);
void Hyena_GSProjectionScale(float x, float y, float z);
void Hyena_GSProjectionTranslate(float x, float y, float z);
void Hyena_GSProjectionFrustum(float left, float right, float bottom, float top, float near_value, float far_value);
void Hyena_GSProjectionOrtho(float left, float right, float bottom, float top, float near_value, float far_value);
void Hyena_GSModelIdentity(void);
void Hyena_GSModelLoad(const float m[16]);
void Hyena_GSModelGet(float m[16]);
void Hyena_GSModelPush(void);
void Hyena_GSModelPop(void);
void Hyena_GSModelTranslate(float x, float y, float z);
void Hyena_GSModelScale(float x, float y, float z);
void Hyena_GSModelRotate(float degrees, float x, float y, float z);
void Hyena_GSSetTextureTransform(float sx, float sy, float ox, float oy);

enum {
    PS2_VU1_TRIANGLES,
    PS2_VU1_QUADS,
    PS2_VU1_FAN,
    PS2_VU1_STRIP
};

enum {
    PS2_VU1_PASS_LIGHTMAP,
    PS2_VU1_PASS_MASK,
    PS2_VU1_PASS_FOG,
    PS2_VU1_PASS_RESTORE,
    PS2_VU1_PASS_STATE
};

typedef struct {
    unsigned int kind[4];
    unsigned long long packet[6][2];
} ps2_vu1_pass_t;

typedef struct {
    const unsigned char *pose1, *pose2;
    const unsigned int *packed, *packed_second;
    const ps2_gs_static_t *commands;
    const ps2_gs_alias_batch_t *batches;
    int batch_count;
    float blend;
    int cull, batch, single, strips;
    float color[4];
} ps2_gs_alias_t;

typedef struct {
    const unsigned char *positions, *uvs;
    const unsigned short *indices;
    int position_stride, uv_stride;
    int indexed, batch, cache;
    float color[4];
} ps2_gs_stream_t;

size_t VID_VU1StaticSize(int batch_count);
ps2_gs_static_t *VID_VU1CreateAliasChain(const unsigned int *poses, const ps2_gs_alias_batch_t *batches, int batch_count, int single);
ps2_gs_static_t *VID_VU1CreateStatic(const unsigned int *poses, const ps2_gs_alias_batch_t *batches, int batch_count);
void VID_VU1FreeStatic(ps2_gs_static_t *commands);
int VID_VU1Init(void);
void VID_VU1Shutdown(void);
int VID_VU1Submit(GSGLOBAL *gs, const float matrix[16],
    const ps2_gs_vertex_t *vertices, int count, int mode, int textured,
    int blending, int replace, const float uv[4], float depth_near, float depth_far,
    int fog_mode, float fog_start, float fog_end, const float fog_color[3],
    const ps2_vu1_pass_t *passes, int pass_count, const ps2_gs_alias_t *alias, const ps2_gs_stream_t *stream);
void VID_VU1Finish(void);
void VID_VU1KickFinish(void);
void VID_VU1WaitIdle(void);
void VID_VU1SendRaw(const void *chain);
void VID_VU1StartRaw(const void *chain);

void VID_VU1Writeback(const void *start, const void *end);
void VID_VU1WaitDMA(void);
void VID_VU1WaitGif(void);

typedef struct {
    const void *data;
    size_t size;
} ps2_gs_dma_range_t;

void VID_QueueBegin(void);
int VID_QueueEnabled(void);
void VID_QueueVIF(packet2_t *packet, const ps2_gs_dma_range_t *ranges, int count);
void VID_QueueGIF(const void *chain, const ps2_gs_dma_range_t *ranges, int count);
void VID_QueueFlush(void);
void VID_QueueWait(void);
void VID_QueuePoll(void);
void VID_QueueShutdown(void);

void VID_PresentBegin(GSGLOBAL *gs);
void VID_PresentImmediate(GSGLOBAL *gs);
int VID_PresentQueue(GSGLOBAL *gs);
void VID_PresentShutdown(void);

void VID_SetDepthRange(float near_value, float far_value);

void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);

extern	int		texture_mode;

extern	double	gldepthmin, gldepthmax;

extern	int glx, gly, glwidth, glheight;

#include "../../../render/r_local.h"

#endif
