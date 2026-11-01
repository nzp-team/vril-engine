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
#include <stdarg.h>
#include <stdio.h>
#include <kernel.h>
#include <unistd.h>

#include "../../../nzportable_def.h"
#include "../../../common.h"
#include <packet2_vif.h>
#include <packet2_chain.h>
#define vertex_t ps2_sdk_vertex_t
#define texinfo_t ps2_sdk_texinfo_t
#include <packet2_utils.h>
#undef texinfo_t
#undef vertex_t
#include <timer.h>
#include <vif_registers.h>
#include <ee_regs.h>
#include <sifcmd.h>
#include <malloc.h>
#include <stdint.h>

size_t VID_VU1StaticSize(int batch_count)
{
    return sizeof(ps2_gs_static_t) + sizeof(packet2_t) + (batch_count * 16 + 2) * 16 + batch_count * 16;
}

void VID_VU1FreeStatic(ps2_gs_static_t *commands)
{
    if (!commands)
        return;
    VID_VU1WaitDMA();
    if (commands->packet)
        packet2_free(commands->packet);
    free(commands->counts);
    free(commands);
}

ps2_gs_static_t *VID_VU1CreateAliasChain(const unsigned int *poses, const ps2_gs_alias_batch_t *batches, int batch_count, int single)
{
    ps2_gs_static_t *commands = calloc(1, sizeof(*commands));
    packet2_t *packet;
    size_t offset = 0;
    int i;
    if (!commands)
        return NULL;
    commands->packet = packet2_create(batch_count * 16 + 2, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    commands->counts = memalign(64, batch_count * sizeof(*commands->counts));
    if (!commands->packet || !commands->counts) {
        VID_VU1FreeStatic(commands);
        return NULL;
    }
    packet = commands->packet;
    memset(commands->counts, 0, batch_count * sizeof(*commands->counts));
    for (i = 0; i < batch_count; ++i) {
        const ps2_gs_alias_batch_t *batch = &batches[i];
        commands->counts[i][0] = batch->triangle_count;
        commands->counts[i][1] = batch->vertex_count;
        commands->counts[i][2] = batch->strip_count;
        packet2_chain_open_cnt(packet, 0, 0, 0);
        packet2_vif_flushe(packet, 0);
        packet2_vif_nop(packet, 0);
        packet2_chain_close_tag(packet);
        packet2_utils_vu_add_unpack_data(packet, VU_BATCH_COUNTS, commands->counts[i], 1, 0);
        packet2_chain_ref(packet, batch->indices, (batch->triangle_count * 4 + 15) / 16, 0, 0, 0);
        packet2_vif_stcycl(packet, 1, 1, 0);
        packet2_vif_open_unpack(packet, P2_UNPACK_V4_8, VU_INDICES, 0, 0, 1, 0);
        packet2_vif_close_unpack_manual(packet, batch->triangle_count);
        packet2_chain_ref(packet, (const unsigned char *)poses + offset, (batch->vertex_count * (single ? 4 : 8) + 15) / 16, 0, 0, 0);
        packet2_vif_stcycl(packet, single ? 1 : 2, 3, 0);
        packet2_vif_open_unpack(packet, P2_UNPACK_V4_8, VU_INPUT, 0, 0, 1, 0);
        packet2_vif_close_unpack_manual(packet, batch->vertex_count * (single ? 1 : 2));
        packet2_chain_ref(packet, batch->uvs, (batch->vertex_count * 8 + 15) / 16, 0, 0, 0);
        packet2_vif_stcycl(packet, 1, 3, 0);
        packet2_vif_open_unpack(packet, P2_UNPACK_V2_32, VU_INPUT + 2, 0, 0, 1, 0);
        packet2_vif_close_unpack_manual(packet, batch->vertex_count);
        packet2_utils_vu_add_start_program(packet, 0);
        offset += (batch->vertex_count * (single ? 4 : 8) + 15) & ~15;
    }
    packet2_chain_ret(packet, 0, 0);
    packet2_vif_nop(packet, 0);
    packet2_vif_nop(packet, 0);
    packet2_chain_close_tag(packet);
    VID_VU1Writeback(commands->counts, commands->counts + batch_count);
    VID_VU1Writeback(packet->base, packet->next);
    return commands;
}

ps2_gs_static_t *VID_VU1CreateStatic(const unsigned int *poses, const ps2_gs_alias_batch_t *batches, int batch_count)
{
    return VID_VU1CreateAliasChain(poses, batches, batch_count, 1);
}

#define VIF_MASK_DMA_TAG_MISMATCH (1 << 1)
#define PS2_VU1_FINISH_MARK 0x5652

typedef struct {
    float matrix[16];
    float scale[4];
    float offset[4];
    unsigned int count[4];
    unsigned long long giftag[2];
    float fog[4];
    float uv[4];
    float color_scale[4];
    float planes[6][4];
    float fog_color[4];
    float rounding[4];
    ps2_vu1_pass_t passes[7];
} ps2_vu1_control_t;

extern u32 VU1_CodeStart __attribute__((section(".vudata")));
extern u32 VU1_CodeEnd __attribute__((section(".vudata")));

static ps2_vu1_control_t vu1_control[2] __attribute__((aligned(64)));
static union {
    ps2_gs_vertex_t vertices[VU_VERTICES];
    ps2_gs_world_scratch_t world;
} vu1_input[2] __attribute__((aligned(64)));
static unsigned int alias_poses[2][VU_VERTICES * 2] __attribute__((aligned(64)));
static unsigned long long batch_giftag[2][2] __attribute__((aligned(64)));
static float alias_control[2][20] __attribute__((aligned(64)));
static packet2_t *program_packet;
static packet2_t *submit_packet[2];
static packet2_t *finish_packet;
static struct {
    packet2_t *packet;
    dma_tag_t *state, *indices, *poses, *poses_second, *uvs;
    vif_code_t *poses_cycle, *state_unpack, *indices_unpack, *poses_unpack, *poses_second_unpack, *uvs_unpack;
} alias_packets[2];
static struct {
    packet2_t *packet;
    dma_tag_t *state, *indices, *positions, *uvs;
    vif_code_t *state_unpack, *indices_unpack, *positions_unpack, *uvs_unpack;
} world_packets[2];
static struct {
    packet2_t *packet;
    dma_tag_t *call;
} static_packets[2];
static int vu1_ready;
static int vu1_submitted;
static int vu1_buffer;
static unsigned int transient_start, transient_end;

extern int Hyena_GSPacketCache(void);
extern int Hyena_GSProfiling(void);
extern int Hyena_GSRangeWriteback(void);
extern void Hyena_GSAddVIFWaitTicks(unsigned int ticks);
extern void Hyena_GSAddGIFWaitTicks(unsigned int ticks);

void VID_VU1WaitIdle(void)
{
    int profile = Hyena_GSProfiling();
    unsigned int start = cpu_ticks(), polls = 0;
    while (*R_EE_D1_CHCR & 0x100) {
        if (!(++polls & 1023) && cpu_ticks() - start > 2 * kBUSCLK) {
            Sys_PS2GPUError("VIF DMA");
        }
    }
    if (profile)
        Hyena_GSAddVIFWaitTicks(cpu_ticks() - start);
}

void VID_VU1WaitDMA(void)
{
    VID_QueueFlush();
    VID_QueueWait();
}

void VID_VU1Writeback(const void *start, const void *end)
{
    if (Hyena_GSRangeWriteback())
        SifWriteBackDCache((void *)start, (const unsigned char *)end - (const unsigned char *)start);
    else
        SyncDCache((void *)start, (void *)end);
}

void VID_VU1StartRaw(const void *chain)
{
    *R_EE_D_STAT = 1 << DMA_CHANNEL_VIF1;
    *R_EE_D1_QWC = 0;
    *R_EE_D1_MADR = 0;
    *R_EE_D1_TADR = (unsigned int)chain & 0x0fffffff;
    __asm__ volatile("sync.l" ::: "memory");
    *R_EE_D1_CHCR = DMA_SET_CHCR(1, 1, 0, 1, 1, 1, 0);
}

void VID_VU1SendRaw(const void *chain)
{
    VID_VU1WaitGif();
    VID_VU1StartRaw(chain);
}

static void VID_VU1Send(packet2_t *packet, const void *vertices, size_t size)
{
    if (VID_QueueEnabled()) {
        ps2_gs_dma_range_t ranges[] = {
            {(const void *)transient_start, transient_end - transient_start}, {vertices, size}
        };
        VID_QueueVIF(packet, ranges, sizeof(ranges) / sizeof(ranges[0]));
    } else
        VID_VU1SendRaw(packet->base);
}

static inline vif_code_t *VID_VU1Unpack(packet2_t *packet, const void *data,
    int qwords, int format, int address, int width, int length, int count)
{
    vif_code_t *unpack;
    packet2_chain_ref(packet, data, qwords, 0, 0, 0);
    packet2_vif_stcycl(packet, width, length, 0);
    packet2_vif_open_unpack(packet, format, address, 0, 0, 1, 0);
    unpack = packet->vif_code_opened_at;
    packet2_vif_close_unpack_manual(packet, count);
    return unpack;
}

static void VID_VU1BuildAliasPacket(int slot)
{
    packet2_t *packet = alias_packets[slot].packet;

    packet2_chain_open_cnt(packet, 0, 0, 0);
    packet2_vif_flushe(packet, 0);
    packet2_vif_nop(packet, 0);
    packet2_chain_close_tag(packet);
    alias_packets[slot].state = (dma_tag_t *)packet->next;
    alias_packets[slot].state_unpack = VID_VU1Unpack(packet, &vu1_control[slot],
        VU_PASSES, P2_UNPACK_V4_32, VU_MATRIX, 1, 1, VU_PASSES);
    packet2_utils_vu_add_unpack_data(packet, VU_ALIAS_PARAMS, alias_control[slot], 3, 0);
    packet2_utils_vu_add_unpack_data(packet, VU_BATCH_COUNTS, alias_control[slot] + 12, 1, 0);
    packet2_utils_vu_add_unpack_data(packet, VU_BATCH_GIFTAG, batch_giftag[slot], 1, 0);
    packet2_utils_vu_add_unpack_data(packet, VU_STRIP_GIFTAG, alias_control[slot] + 16, 1, 0);
    alias_packets[slot].indices = (dma_tag_t *)packet->next;
    alias_packets[slot].indices_unpack = VID_VU1Unpack(packet, &vu1_control[slot],
        1, P2_UNPACK_V4_8, VU_INDICES, 1, 1, 1);
    alias_packets[slot].poses = (dma_tag_t *)packet->next;
    packet2_chain_ref(packet, alias_poses[slot], 1, 0, 0, 0);
    alias_packets[slot].poses_cycle = (vif_code_t *)packet->next;
    packet2_vif_stcycl(packet, 2, 3, 0);
    packet2_vif_open_unpack(packet, P2_UNPACK_V4_8, VU_INPUT, 0, 0, 1, 0);
    alias_packets[slot].poses_unpack = packet->vif_code_opened_at;
    packet2_vif_close_unpack_manual(packet, 2);
    alias_packets[slot].poses_second = (dma_tag_t *)packet->next;
    alias_packets[slot].poses_second_unpack = VID_VU1Unpack(packet, alias_poses[slot],
        1, P2_UNPACK_V4_8, VU_INPUT + 1, 1, 3, 1);
    alias_packets[slot].uvs = (dma_tag_t *)packet->next;
    alias_packets[slot].uvs_unpack = VID_VU1Unpack(packet, &vu1_control[slot],
        1, P2_UNPACK_V2_32, VU_INPUT + 2, 1, 3, 1);
    packet2_utils_vu_add_start_program(packet, 0);
    packet2_utils_vu_add_end_tag(packet);
}

static void VID_VU1BuildWorldPacket(int slot)
{
    packet2_t *packet = world_packets[slot].packet;
    packet2_chain_open_cnt(packet, 0, 0, 0);
    packet2_vif_flushe(packet, 0);
    packet2_vif_nop(packet, 0);
    packet2_chain_close_tag(packet);
    world_packets[slot].state = (dma_tag_t *)packet->next;
    world_packets[slot].state_unpack = VID_VU1Unpack(packet, &vu1_control[slot],
        VU_PASSES, P2_UNPACK_V4_32, 0, 1, 1, VU_PASSES);
    packet2_utils_vu_add_unpack_data(packet, VU_BATCH_GIFTAG, batch_giftag[slot], 1, 0);
    packet2_utils_vu_add_unpack_data(packet, VU_ALIAS_PARAMS, alias_control[slot], 3, 0);
    world_packets[slot].indices = (dma_tag_t *)packet->next;
    world_packets[slot].indices_unpack = VID_VU1Unpack(packet, &vu1_control[slot],
        1, P2_UNPACK_V4_8, VU_INDICES, 1, 1, 1);
    world_packets[slot].positions = (dma_tag_t *)packet->next;
    world_packets[slot].positions_unpack = VID_VU1Unpack(packet, &vu1_control[slot],
        1, P2_UNPACK_V4_32, VU_INPUT, 1, 3, 1);
    world_packets[slot].uvs = (dma_tag_t *)packet->next;
    world_packets[slot].uvs_unpack = VID_VU1Unpack(packet, &vu1_control[slot],
        1, P2_UNPACK_V2_32, VU_INPUT + 2, 1, 3, 1);
    packet2_utils_vu_add_start_program(packet, 0);
    packet2_utils_vu_add_end_tag(packet);
}

static void VID_VU1BuildStaticPacket(int slot)
{
    packet2_t *packet = static_packets[slot].packet;
    packet2_chain_open_cnt(packet, 0, 0, 0);
    packet2_vif_flushe(packet, 0);
    packet2_vif_nop(packet, 0);
    packet2_chain_close_tag(packet);
    packet2_utils_vu_add_unpack_data(packet, 0, &vu1_control[slot], VU_PASSES, 0);
    packet2_utils_vu_add_unpack_data(packet, VU_ALIAS_PARAMS, alias_control[slot], 3, 0);
    packet2_utils_vu_add_unpack_data(packet, VU_BATCH_GIFTAG, batch_giftag[slot], 1, 0);
    packet2_utils_vu_add_unpack_data(packet, VU_STRIP_GIFTAG, alias_control[slot] + 16, 1, 0);
    static_packets[slot].call = (dma_tag_t *)packet->next;
    packet2_chain_call(packet, &vu1_control[slot], 0, 0, 0);
    packet2_vif_nop(packet, 0);
    packet2_vif_nop(packet, 0);
    packet2_chain_close_tag(packet);
    packet2_utils_vu_add_end_tag(packet);
}

int VID_VU1Init(void)
{
    unsigned int size;
    int i;
    if (vu1_ready)
        return 1;
    dmaKit_chan_init(DMA_CHANNEL_VIF1);
    VIF1_ERR = VIF_MASK_DMA_TAG_MISMATCH;
    size = packet2_utils_get_packet_size_for_program(&VU1_CodeStart,
        &VU1_CodeEnd) + 2;
    program_packet = packet2_create(size, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    submit_packet[0] = packet2_create(32, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    submit_packet[1] = packet2_create(32, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    finish_packet = packet2_create(4, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    for (i = 0; i < 2; ++i) {
        alias_packets[i].packet = packet2_create(32, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
        static_packets[i].packet = packet2_create(16, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
        world_packets[i].packet = packet2_create(32, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);
    }
    if (!program_packet || !submit_packet[0] || !submit_packet[1] || !finish_packet ||
        !alias_packets[0].packet || !alias_packets[1].packet) {
        VID_VU1Shutdown();
        return 0;
    }
    packet2_vif_add_micro_program(program_packet, 0,
        &VU1_CodeStart, &VU1_CodeEnd);
    packet2_utils_vu_add_end_tag(program_packet);
    VID_VU1Writeback(program_packet->base, program_packet->next);
    VID_VU1SendRaw(program_packet->base);
    VID_VU1WaitDMA();
    packet2_chain_open_cnt(finish_packet, 0, 0, 0);
    packet2_vif_flusha(finish_packet, 0);
    packet2_vif_mark(finish_packet, PS2_VU1_FINISH_MARK, 0);
    packet2_chain_close_tag(finish_packet);
    packet2_utils_vu_add_end_tag(finish_packet);
    VID_VU1Writeback(finish_packet->base, finish_packet->next);
    for (i = 0; i < 2; ++i) {
        int plane;
        VID_VU1BuildAliasPacket(i);
        if (world_packets[i].packet)
            VID_VU1BuildWorldPacket(i);
        if (static_packets[i].packet)
            VID_VU1BuildStaticPacket(i);
        vu1_control[i].scale[3] = 255;
        for (plane = 0; plane < 4; ++plane)
            vu1_control[i].rounding[plane] = 0.5f;
        for (plane = 0; plane < 6; ++plane) {
            vu1_control[i].planes[plane][plane / 2] = plane & 1 ? -1 : 1;
            vu1_control[i].planes[plane][3] = 1;
        }
    }
    {
        const ps2_gs_dma_range_t ranges[] = {
            {vu1_control, sizeof(vu1_control)}, {vu1_input, sizeof(vu1_input)},
            {alias_control, sizeof(alias_control)}, {alias_poses, sizeof(alias_poses)},
            {batch_giftag, sizeof(batch_giftag)}
        };
        transient_start = (unsigned int)vu1_control;
        transient_end = transient_start + sizeof(vu1_control);
        for (i = 0; i < sizeof(ranges) / sizeof(ranges[0]); i++) {
            unsigned int start = (unsigned int)ranges[i].data;
            if (start < transient_start)
                transient_start = start;
            if (start + ranges[i].size > transient_end)
                transient_end = start + ranges[i].size;
        }
    }
    vu1_ready = 1;
    return 1;
}

void VID_VU1Shutdown(void)
{
    if (vu1_ready)
        VID_VU1Finish();
    if (program_packet) packet2_free(program_packet);
    if (submit_packet[0]) packet2_free(submit_packet[0]);
    if (submit_packet[1]) packet2_free(submit_packet[1]);
    if (finish_packet) packet2_free(finish_packet);
    if (alias_packets[0].packet) packet2_free(alias_packets[0].packet);
    if (alias_packets[1].packet) packet2_free(alias_packets[1].packet);
    if (static_packets[0].packet) packet2_free(static_packets[0].packet);
    if (static_packets[1].packet) packet2_free(static_packets[1].packet);
    if (world_packets[0].packet) packet2_free(world_packets[0].packet);
    if (world_packets[1].packet) packet2_free(world_packets[1].packet);
    memset(world_packets, 0, sizeof(world_packets));
    memset(static_packets, 0, sizeof(static_packets));
    memset(alias_packets, 0, sizeof(alias_packets));
    program_packet = submit_packet[0] = submit_packet[1] = finish_packet = NULL;
    VID_QueueShutdown();
    vu1_submitted = vu1_ready = 0;
}

int VID_VU1Submit(GSGLOBAL *gs, const float matrix[16],
    const ps2_gs_vertex_t *vertices, int count, int mode, int textured,
    int blending, int replace, const float uv[4], float depth_near, float depth_far,
    int fog_mode, float fog_start, float fog_end, const float fog_color[3],
    const ps2_vu1_pass_t *passes, int pass_count, const ps2_gs_alias_t *alias, const ps2_gs_stream_t *stream)
{
    int first, index, triangles, triangle_count, prepared = 0;
    int indexed = stream && stream->indexed;
    int batching = alias ? alias->batch : stream && stream->batch && (indexed || pass_count);
    size_t pose_offset = 0;
    unsigned int primitive;
    size_t state_size = sizeof(vu1_control[0]) - sizeof(vu1_control[0].passes) + pass_count * sizeof(*passes);
    if (!gs || (!vertices && !alias && !stream) || count < 3 || !VID_VU1Init())
        return 0;
    primitive = 5 | (1 << 3) | (textured ? 1 << 4 : 0) |
        (fog_mode == 1 ? 1 << 5 : 0) | (blending ? 1 << 6 : 0) |
        (gs->PrimContext << 9);
    triangle_count = mode == PS2_VU1_TRIANGLES ? count / 3 :
        mode == PS2_VU1_QUADS ? count / 4 * 2 : count - 2;
    for (first = 0; first < (alias ? alias->batch_count : triangle_count);
        first += alias ? 1 : triangles) {
        ps2_vu1_control_t *state = &vu1_control[vu1_buffer];
        packet2_t *packet = submit_packet[vu1_buffer];
        const ps2_gs_alias_batch_t *batch = alias ? &alias->batches[first] : NULL;
        int full_state = !first || !Hyena_GSPacketCache();
        int pose_stride = alias && (alias->single || alias->packed_second) ? 4 : 8;
        int retained_list = !first && alias && alias->commands &&
            Hyena_GSPacketCache() && static_packets[vu1_buffer].packet;
        triangles = alias ? batch->triangle_count : triangle_count - first;
        const ps2_gs_vertex_t *data = vertices;
        const ps2_gs_world_batch_t *world = NULL;
        if (indexed) {
            int limit = pass_count && triangles > 24 ? 24 : triangles;
            int vertex_limit = pass_count ? VU_WORLD_PASS_VERTICES : VU_VERTICES;
            world = stream->cache ? Hyena_CacheWorldBatch(&vu1_input[vu1_buffer].world, stream->positions,
                stream->position_stride, stream->uvs, stream->uv_stride, stream->indices, first, limit, vertex_limit) : &vu1_input[vu1_buffer].world.batch;
            if (!stream->cache)
                Hyena_BuildWorldBatch(&vu1_input[vu1_buffer].world, stream->positions,
                    stream->position_stride, stream->uvs, stream->uv_stride, stream->indices, first, limit, vertex_limit);
            triangles = world->triangle_count;
        }
        if (!alias && !indexed && triangles > VU_VERTICES / 3)
            triangles = VU_VERTICES / 3;
        state->count[0] = triangles;
        if (!Hyena_GSPacketCache() || !(prepared & (1 << vu1_buffer))) {
            memcpy(state->matrix, matrix, sizeof(state->matrix));
            state->scale[0] = gs->Width * 0.5f;
            state->scale[1] = -gs->Height * 0.5f;
            state->scale[2] = -0.5f * (depth_far - depth_near) * 16777215.0f;
            state->offset[0] = (float)gs->OffsetX / 16.0f + gs->Width * 0.5f;
            state->offset[1] = (float)gs->OffsetY / 16.0f + gs->Height * 0.5f;
            state->offset[2] = (1.0f - depth_near - 0.5f * (depth_far - depth_near)) * 16777215.0f;
            state->count[1] = fog_mode;
            state->count[2] = pass_count;
            prepared |= 1 << vu1_buffer;
            state->count[3] = (alias ? VU_FLAG_ALIAS : (indexed ? VU_FLAG_INDEXED : 0) | (stream ? VU_FLAG_STREAM : 0)) |
                (batching ? VU_FLAG_BATCH : 0) | (alias && alias->single ? VU_FLAG_SINGLE : 0) | (alias && alias->strips && batching ? VU_FLAG_STRIPS : 0) | (pass_count && fog_mode);
            if (pass_count)
                memcpy(state->passes, passes, pass_count * sizeof(*passes));
            state->giftag[0] = (1ULL << 46) |
                ((unsigned long long)primitive << 47) | (3ULL << 60);
            if (alias || indexed)
                state->giftag[0] |= 0x8003;
            state->giftag[1] = 0x412;
            memcpy(batch_giftag[vu1_buffer], state->giftag, sizeof(state->giftag));
            if (batching)
                batch_giftag[vu1_buffer][0] = (batch_giftag[vu1_buffer][0] & ~(7ULL << 47)) | (3ULL << 47);
            state->fog[0] = fog_mode && fog_end > fog_start ? 1.0f / (fog_end - fog_start) : 0;
            state->fog[1] = 0;
            state->fog[2] = fog_mode ? fog_end : 0;
            state->fog[3] = 1;
            memcpy(state->uv, uv, sizeof(state->uv));
            state->color_scale[0] = state->color_scale[1] = state->color_scale[2] =
                replace && textured ? 0 : textured ? 128 : 255;
            for (index = 0; index < 3; ++index)
                state->rounding[index] = replace && textured ? 128.5f : 0.5f;
            state->color_scale[3] = 128;
            for (index = 0; index < 3; ++index)
                state->fog_color[index] = fog_color[index] * 255;
            state->fog_color[3] = 128;
        }
        if (alias) {
            const unsigned int *pose1 = (const unsigned int *)alias->pose1;
            const unsigned int *pose2 = (const unsigned int *)alias->pose2;
            if (!alias->packed) {
                for (index = 0; index < batch->vertex_count; ++index) {
                    unsigned short vertex = batch->vertices[index];
                    alias_poses[vu1_buffer][index * 2] = pose1[vertex];
                    alias_poses[vu1_buffer][index * 2 + 1] = pose2[vertex];
                }
                if (batch->vertex_count & 1) {
                    alias_poses[vu1_buffer][batch->vertex_count * 2] = 0;
                    alias_poses[vu1_buffer][batch->vertex_count * 2 + 1] = 0;
                }
            }
            ((unsigned int *)alias_control[vu1_buffer])[12] = triangles;
            ((unsigned int *)alias_control[vu1_buffer])[13] = batch->vertex_count;
            ((unsigned int *)alias_control[vu1_buffer])[14] = batch->strip_count;
            ((unsigned long long *)alias_control[vu1_buffer])[8] = (state->giftag[0] & ~(7ULL << 47)) | (4ULL << 47);
            ((unsigned long long *)alias_control[vu1_buffer])[9] = state->giftag[1];
            alias_control[vu1_buffer][0] = alias->blend;
            alias_control[vu1_buffer][1] = 1.0f - alias->blend;
            if (alias->single)
                ((unsigned int *)alias_control[vu1_buffer])[1] = triangles;
            ((unsigned int *)alias_control[vu1_buffer])[2] = 0;
            ((unsigned int *)alias_control[vu1_buffer])[3] = batch->vertex_count;
            memcpy(alias_control[vu1_buffer] + 4, alias->color, sizeof(alias->color));
            ((unsigned int *)alias_control[vu1_buffer])[8] = alias->cull;
        } else if (indexed) {
            ((unsigned int *)alias_control[vu1_buffer])[3] = world->vertex_count;
            ((unsigned int *)alias_control[vu1_buffer])[8] = 0;
            memcpy(alias_control[vu1_buffer] + 4, stream->color, sizeof(stream->color));
        } else if (stream) {
            float *positions = (float *)vu1_input[vu1_buffer].vertices;
            float *uvs = positions + VU_VERTICES * 4;
            for (index = 0; index < triangles * 3; ++index) {
                int vertex = stream->indices ? stream->indices[first * 3 + index] : first * 3 + index;
                const float *position = (const float *)(stream->positions + vertex * stream->position_stride);
                const float *uv = (const float *)(stream->uvs + vertex * stream->uv_stride);
                positions[index * 4] = position[0];
                positions[index * 4 + 1] = position[1];
                positions[index * 4 + 2] = position[2];
                positions[index * 4 + 3] = 1;
                uvs[index * 2] = uv[0];
                uvs[index * 2 + 1] = uv[1];
            }
            if (triangles & 1)
                uvs[triangles * 6] = uvs[triangles * 6 + 1] = 0;
            memcpy(alias_control[vu1_buffer] + 4, stream->color, sizeof(stream->color));
        } else if (mode == PS2_VU1_TRIANGLES) {
            data += first * 3;
        } else {
            for (index = 0; index < triangles * 3; ++index) {
                int triangle = first + index / 3, corner = index % 3;
                int source_index;
                if (mode == PS2_VU1_QUADS)
                    source_index = triangle / 2 * 4 + (corner ? corner + (triangle & 1) : 0);
                else if (mode == PS2_VU1_FAN)
                    source_index = corner ? triangle + corner : 0;
                else
                    source_index = triangle + (corner == 2 ? 2 : corner ^ (triangle & 1));
                vu1_input[vu1_buffer].vertices[index] = vertices[source_index];
            }
            data = vu1_input[vu1_buffer].vertices;
        }
        if (!VID_QueueEnabled())
            VID_VU1WaitDMA();
        if (retained_list) {
            packet = static_packets[vu1_buffer].packet;
            static_packets[vu1_buffer].call->ADDR = (unsigned int)alias->commands->packet->base;
            if (!VID_QueueEnabled())
                VID_VU1Writeback(alias_control[vu1_buffer], alias_control[vu1_buffer] + 20);
        } else if (alias && Hyena_GSPacketCache()) {
            packet = alias_packets[vu1_buffer].packet;
            alias_packets[vu1_buffer].state->ADDR = (unsigned int)(full_state ? (void *)state : (void *)state->count);
            alias_packets[vu1_buffer].state->QWC = full_state ? VU_PASSES : 1;
            alias_packets[vu1_buffer].state_unpack->num = full_state ? VU_PASSES : 1;
            alias_packets[vu1_buffer].state_unpack->immediate = 0x4000 | (full_state ? 0 : VU_COUNTS);
            alias_packets[vu1_buffer].indices->ADDR = (unsigned int)batch->indices;
            alias_packets[vu1_buffer].indices->QWC = (triangles * 4 + 15) / 16;
            alias_packets[vu1_buffer].indices_unpack->num = triangles;
            alias_packets[vu1_buffer].poses->ADDR = (unsigned int)(alias->packed ?
                (const unsigned char *)alias->packed + pose_offset : (const unsigned char *)alias_poses[vu1_buffer]);
            alias_packets[vu1_buffer].poses->QWC = (batch->vertex_count * pose_stride + 15) / 16;
            alias_packets[vu1_buffer].poses_cycle->immediate = 3 | ((pose_stride / 4) << 8);
            alias_packets[vu1_buffer].poses_unpack->num = batch->vertex_count * (pose_stride / 4);
            alias_packets[vu1_buffer].poses_second->ADDR = (unsigned int)(alias->packed_second ?
                (const unsigned char *)alias->packed_second + pose_offset : (const unsigned char *)alias_poses[vu1_buffer]);
            alias_packets[vu1_buffer].poses_second->QWC = alias->packed_second ? (batch->vertex_count * 4 + 15) / 16 : 0;
            alias_packets[vu1_buffer].poses_second_unpack->cmd = alias->packed_second ? P2_UNPACK_V4_8 | 0x60 : 0;
            alias_packets[vu1_buffer].poses_second_unpack->num = alias->packed_second ? batch->vertex_count : 0;
            alias_packets[vu1_buffer].uvs->ADDR = (unsigned int)batch->uvs;
            alias_packets[vu1_buffer].uvs->QWC = (batch->vertex_count * 8 + 15) / 16;
            alias_packets[vu1_buffer].uvs_unpack->num = batch->vertex_count;
            if (!VID_QueueEnabled())
                VID_VU1Writeback(alias_control[vu1_buffer], alias_control[vu1_buffer] + 20);
            if (!alias->packed && !VID_QueueEnabled())
                VID_VU1Writeback(alias_poses[vu1_buffer], (unsigned char *)alias_poses[vu1_buffer] + batch->vertex_count * 8);
        } else if (indexed && Hyena_GSPacketCache() && world_packets[vu1_buffer].packet) {
            packet = world_packets[vu1_buffer].packet;
            world_packets[vu1_buffer].state->ADDR = (unsigned int)(full_state ? (void *)state : (void *)state->count);
            world_packets[vu1_buffer].state->QWC = full_state ? state_size / 16 : 1;
            world_packets[vu1_buffer].state_unpack->num = full_state ? state_size / 16 : 1;
            world_packets[vu1_buffer].state_unpack->immediate = 0x4000 | (full_state ? 0 : VU_COUNTS);
            world_packets[vu1_buffer].indices->ADDR = (unsigned int)world->indices;
            world_packets[vu1_buffer].indices->QWC = (triangles * 4 + 15) / 16;
            world_packets[vu1_buffer].indices_unpack->num = triangles;
            world_packets[vu1_buffer].indices_unpack->immediate = 0x4000 | (pass_count ? VU_LIGHTMAP_INDICES : VU_INDICES);
            world_packets[vu1_buffer].positions->ADDR = (unsigned int)world->positions;
            world_packets[vu1_buffer].positions->QWC = world->vertex_count;
            world_packets[vu1_buffer].positions_unpack->num = world->vertex_count;
            world_packets[vu1_buffer].uvs->ADDR = (unsigned int)world->uvs;
            world_packets[vu1_buffer].uvs->QWC = (world->vertex_count * 8 + 15) / 16;
            world_packets[vu1_buffer].uvs_unpack->num = world->vertex_count;
            if (!VID_QueueEnabled()) {
                VID_VU1Writeback(alias_control[vu1_buffer], alias_control[vu1_buffer] + 20);
                if (world == &vu1_input[vu1_buffer].world.batch)
                    VID_VU1Writeback(&vu1_input[vu1_buffer].world, &vu1_input[vu1_buffer].world + 1);
            }
        } else {
            packet2_reset(packet, 0);
            packet2_chain_open_cnt(packet, 0, 0, 0);
            packet2_vif_flushe(packet, 0);
            packet2_vif_nop(packet, 0);
            packet2_chain_close_tag(packet);
            packet2_utils_vu_add_unpack_data(packet, full_state ? 0 : VU_COUNTS,
                full_state ? (void *)state : (void *)state->count, full_state ? state_size / 16 : 1, 0);
            if (alias || indexed)
                packet2_utils_vu_add_unpack_data(packet, VU_BATCH_GIFTAG, batch_giftag[vu1_buffer], 1, 0);
            if (alias) {
                packet2_utils_vu_add_unpack_data(packet, VU_ALIAS_PARAMS, alias_control[vu1_buffer], 3, 0);
                packet2_utils_vu_add_unpack_data(packet, VU_BATCH_COUNTS, alias_control[vu1_buffer] + 12, 1, 0);
                packet2_utils_vu_add_unpack_data(packet, VU_STRIP_GIFTAG, alias_control[vu1_buffer] + 16, 1, 0);
                packet2_chain_ref(packet, (void *)batch->indices, (triangles * 4 + 15) / 16, 0, 0, 0);
                packet2_vif_stcycl(packet, 1, 1, 0);
                packet2_vif_open_unpack(packet, P2_UNPACK_V4_8, VU_INDICES, 0, 0, 1, 0);
                packet2_vif_close_unpack_manual(packet, triangles);
                packet2_chain_ref(packet, alias->packed ? (const unsigned char *)alias->packed + pose_offset :
                    (const unsigned char *)alias_poses[vu1_buffer], (batch->vertex_count * pose_stride + 15) / 16, 0, 0, 0);
                packet2_vif_stcycl(packet, pose_stride / 4, 3, 0);
                packet2_vif_open_unpack(packet, P2_UNPACK_V4_8, VU_INPUT, 0, 0, 1, 0);
                packet2_vif_close_unpack_manual(packet, batch->vertex_count * (pose_stride / 4));
                if (alias->packed_second) {
                    packet2_chain_ref(packet, (const unsigned char *)alias->packed_second + pose_offset,
                        (batch->vertex_count * 4 + 15) / 16, 0, 0, 0);
                    packet2_vif_stcycl(packet, 1, 3, 0);
                    packet2_vif_open_unpack(packet, P2_UNPACK_V4_8, VU_INPUT + 1, 0, 0, 1, 0);
                    packet2_vif_close_unpack_manual(packet, batch->vertex_count);
                }
                packet2_chain_ref(packet, (void *)batch->uvs, (batch->vertex_count * 8 + 15) / 16, 0, 0, 0);
                packet2_vif_stcycl(packet, 1, 3, 0);
                packet2_vif_open_unpack(packet, P2_UNPACK_V2_32, VU_INPUT + 2, 0, 0, 1, 0);
                packet2_vif_close_unpack_manual(packet, batch->vertex_count);
                if (!VID_QueueEnabled())
                    VID_VU1Writeback(alias_control[vu1_buffer], alias_control[vu1_buffer] + 20);
                if (!alias->packed && !VID_QueueEnabled())
                    VID_VU1Writeback(alias_poses[vu1_buffer], (unsigned char *)alias_poses[vu1_buffer] + batch->vertex_count * 8);
            } else if (indexed) {
                packet2_utils_vu_add_unpack_data(packet, VU_ALIAS_PARAMS, alias_control[vu1_buffer], 3, 0);
                packet2_chain_ref(packet, world->indices, (triangles * 4 + 15) / 16, 0, 0, 0);
                packet2_vif_stcycl(packet, 1, 1, 0);
                packet2_vif_open_unpack(packet, P2_UNPACK_V4_8, pass_count ? VU_LIGHTMAP_INDICES : VU_INDICES, 0, 0, 1, 0);
                packet2_vif_close_unpack_manual(packet, triangles);
                packet2_chain_ref(packet, world->positions, world->vertex_count, 0, 0, 0);
                packet2_vif_stcycl(packet, 1, 3, 0);
                packet2_vif_open_unpack(packet, P2_UNPACK_V4_32, VU_INPUT, 0, 0, 1, 0);
                packet2_vif_close_unpack_manual(packet, world->vertex_count);
                packet2_chain_ref(packet, world->uvs, (world->vertex_count * 8 + 15) / 16, 0, 0, 0);
                packet2_vif_stcycl(packet, 1, 3, 0);
                packet2_vif_open_unpack(packet, P2_UNPACK_V2_32, VU_INPUT + 2, 0, 0, 1, 0);
                packet2_vif_close_unpack_manual(packet, world->vertex_count);
                if (!VID_QueueEnabled())
                    VID_VU1Writeback(alias_control[vu1_buffer], alias_control[vu1_buffer] + 20);
                if (world == &vu1_input[vu1_buffer].world.batch && !VID_QueueEnabled())
                    VID_VU1Writeback(&vu1_input[vu1_buffer].world, &vu1_input[vu1_buffer].world + 1);
            } else if (stream) {
                float *positions = (float *)vu1_input[vu1_buffer].vertices;
                float *uvs = positions + VU_VERTICES * 4;
                packet2_utils_vu_add_unpack_data(packet, VU_COLOR, alias_control[vu1_buffer] + 4, 1, 0);
                packet2_chain_ref(packet, positions, triangles * 3, 0, 0, 0);
                packet2_vif_stcycl(packet, 1, 3, 0);
                packet2_vif_open_unpack(packet, P2_UNPACK_V4_32, VU_INPUT, 0, 0, 1, 0);
                packet2_vif_close_unpack_manual(packet, triangles * 3);
                packet2_chain_ref(packet, uvs, (triangles * 24 + 15) / 16, 0, 0, 0);
                packet2_vif_stcycl(packet, 1, 3, 0);
                packet2_vif_open_unpack(packet, P2_UNPACK_V2_32, VU_INPUT + 1, 0, 0, 1, 0);
                packet2_vif_close_unpack_manual(packet, triangles * 3);
                if (!VID_QueueEnabled())
                    VID_VU1Writeback(alias_control[vu1_buffer] + 4, alias_control[vu1_buffer] + 8);
                if (!VID_QueueEnabled())
                    VID_VU1Writeback(positions, uvs + triangles * 6);
            } else {
                packet2_utils_vu_add_unpack_data(packet, VU_INPUT, (void *)data, triangles * 9, 0);
                if (!VID_QueueEnabled())
                    VID_VU1Writeback((void *)data, (void *)(data + triangles * 3));
            }
            packet2_utils_vu_add_start_program(packet, 0);
            packet2_utils_vu_add_end_tag(packet);
        }
        if (!VID_QueueEnabled())
            VID_VU1Writeback(batch_giftag[vu1_buffer], batch_giftag[vu1_buffer] + 2);
        if (!VID_QueueEnabled()) {
            if (full_state)
                VID_VU1Writeback(state, (char *)state + state_size);
            else
                VID_VU1Writeback(state->count, state->count + 4);
        }
        if (!VID_QueueEnabled())
            VID_VU1Writeback(packet->base, packet->next);
        VID_VU1Send(packet, vertices, count * sizeof(*vertices));
        vu1_submitted = 1;
        if (alias)
            pose_offset += (batch->vertex_count * pose_stride + 15) & ~15;
        vu1_buffer ^= 1;
        if (retained_list)
            break;
    }
    if (!VID_QueueEnabled() && !alias && (!indexed || !stream->cache))
        VID_VU1WaitDMA();
    return 1;
}

void VID_VU1KickFinish(void)
{
    if (vu1_ready && vu1_submitted) {
        VIF1_MARK = 0;
        __asm__ volatile("sync.l" ::: "memory");
        VID_VU1Send(finish_packet, NULL, 0);
    }
    VID_QueueFlush();
    vu1_submitted = 0;
}

void VID_VU1Finish(void)
{
    if (!vu1_ready) {
        VID_QueueFlush();
        VID_QueueWait();
        return;
    }
    if (!vu1_submitted) {
        VID_QueueFlush();
        VID_QueueWait();
        return;
    }
    if (!VID_QueueEnabled())
        VID_VU1WaitDMA();
    VIF1_MARK = 0;
    __asm__ volatile("sync.l" ::: "memory");
    VID_VU1Send(finish_packet, NULL, 0);
    VID_VU1WaitDMA();
    {
        int profile = Hyena_GSProfiling();
        unsigned int start = cpu_ticks(), polls = 0;
        while (VIF1_MARK != PS2_VU1_FINISH_MARK) {
            if (!(++polls & 1023) && cpu_ticks() - start > 2 * kBUSCLK) {
                Sys_PS2GPUError("VIF FLUSHA/MARK");
            }
        }
        if (profile)
            Hyena_GSAddVIFWaitTicks(cpu_ticks() - start);
    }
    vu1_submitted = 0;
}

void VID_VU1WaitGif(void)
{
    int profile = Hyena_GSProfiling();
    unsigned int start = cpu_ticks(), polls = 0;
    while (*R_EE_D2_CHCR & 0x100) {
        if (!(++polls & 1023) && cpu_ticks() - start > 2 * kBUSCLK) {
            Sys_PS2GPUError("GIF DMA");
        }
    }
    if (profile)
        Hyena_GSAddGIFWaitTicks(cpu_ticks() - start);
}

void dmaKit_wait_fast(void)
{
    __asm__ volatile("sync.l; sync.p" ::: "memory");
    VID_VU1WaitGif();
}

#define GS_DMA_QUEUE_BYTES (128 * 1024)
#define GS_DMA_SNAPSHOT_BYTES (8 * 1024)

typedef struct {
    unsigned long long control, codes;
} gs_dma_tag_t;

cvar_t ps2_dma_queue = {"ps2_dma_queue", "1"};
cvar_t ps2_dma_batch = {"ps2_dma_batch", "128"};
static byte *queue_data[2];
static byte queue_snapshot[GS_DMA_SNAPSHOT_BYTES] __attribute__((aligned(64)));
static size_t queue_used, queue_target;
static int queue_buffer, queue_active, queue_failed;
static int queue_handler = -1, queue_handler_failed;
static const void *volatile queue_current, *volatile queue_pending;

static void *VID_DMAAddress(unsigned long long tag)
{
    return (void *)(uintptr_t)((tag >> 32) & 0x0fffffff);
}

static int VID_DMACopy(unsigned long long tag, const ps2_gs_dma_range_t *ranges, int count)
{
    uintptr_t address = (tag >> 32) & 0x0fffffff;
    int i;
    for (i = 0; i < count; i++) {
        uintptr_t start = (uintptr_t)ranges[i].data & 0x0fffffff;
        if (!ranges[i].data)
            continue;
        if (address >= start && address < start + ranges[i].size)
            return 1;
    }
    return 0;
}

int VID_QueueEnabled(void)
{
    return queue_active;
}

static void VID_DMAAdvance(void)
{
    *R_EE_D_STAT = 1 << 1;
    if (!(*R_EE_D1_CHCR & 0x100)) {
        queue_current = NULL;
        if (queue_pending) {
            queue_current = queue_pending;
            VID_VU1StartRaw(queue_current);
            queue_pending = NULL;
        }
    }
}

static int VID_DMAComplete(int channel)
{
    (void)channel;
    VID_DMAAdvance();
    ExitHandler();
    return 0;
}

void VID_QueuePoll(void)
{
    int enabled = DIntr();
    VID_DMAAdvance();
    if (enabled)
        EIntr();
}

static void VID_DMAWait(void)
{
    VID_VU1WaitIdle();
    VID_QueuePoll();
}

void VID_QueueWait(void)
{
    do {
        VID_DMAWait();
    } while (queue_pending || (*R_EE_D1_CHCR & 0x100));
}

static void VID_DMASubmit(const void *chain)
{
    int enabled;
    if (queue_handler < 0) {
        VID_VU1WaitIdle();
        VID_VU1SendRaw(chain);
        return;
    }
    while (queue_pending)
        VID_DMAWait();
    enabled = DIntr();
    if (*R_EE_D1_CHCR & 0x100)
        queue_pending = chain;
    else {
        *R_EE_D_STAT = 1 << 1;
        queue_current = chain;
        VID_VU1SendRaw(chain);
    }
    if (enabled)
        EIntr();
}

void VID_QueueFlush(void)
{
    gs_dma_tag_t *end;
    if (!queue_used)
        return;
    end = (gs_dma_tag_t *)(queue_data[queue_buffer] + queue_used);
    end->control = (7ULL << 28) | (queue_handler >= 0 ? 1ULL << 31 : 0);
    end->codes = 0;
    VID_VU1Writeback(queue_data[queue_buffer], end + 1);
    VID_DMASubmit(queue_data[queue_buffer]);
    queue_buffer ^= 1;
    queue_used = 0;
}

static gs_dma_tag_t *VID_DMAReserve(size_t bytes)
{
    gs_dma_tag_t *result;
    if (bytes + 16 > GS_DMA_QUEUE_BYTES)
        Sys_Error("GS DMA queue packet exceeds capacity");
    if (queue_used + bytes + 16 > GS_DMA_QUEUE_BYTES)
        VID_QueueFlush();
    while (queue_pending == queue_data[queue_buffer] ||
        (queue_current == queue_data[queue_buffer] && (*R_EE_D1_CHCR & 0x100)))
        VID_DMAWait();
    result = (gs_dma_tag_t *)(queue_data[queue_buffer] + queue_used);
    queue_used += bytes;
    return result;
}

void VID_QueueBegin(void)
{
    queue_target = bound(8, (int)ps2_dma_batch.value, 128) * 1024;
    if (!ps2_dma_queue.value) {
        VID_QueueFlush();
        VID_QueueWait();
        queue_active = 0;
        return;
    }
    if (!queue_data[0] && !queue_failed) {
        queue_data[0] = memalign(64, GS_DMA_QUEUE_BYTES);
        queue_data[1] = memalign(64, GS_DMA_QUEUE_BYTES);
        if (!queue_data[0] || !queue_data[1]) {
            free(queue_data[0]);
            free(queue_data[1]);
            queue_data[0] = queue_data[1] = NULL;
            queue_failed = 1;
        }
    }
    queue_active = queue_data[0] != NULL && VID_VU1Init();
    if (queue_active && queue_handler < 0 && !queue_handler_failed) {
        queue_handler = AddDmacHandler(1, VID_DMAComplete, 0);
        if (queue_handler >= 0 && EnableDmac(1) < 0) {
            RemoveDmacHandler(1, queue_handler);
            queue_handler = -1;
        }
        if (queue_handler < 0)
            queue_handler_failed = 1;
    }
}

void VID_QueueVIF(packet2_t *packet, const ps2_gs_dma_range_t *ranges, int count)
{
    const gs_dma_tag_t *source = (const gs_dma_tag_t *)packet->base;
    size_t first = queue_used, bytes = 0;
    int snapshot = queue_used + GS_DMA_SNAPSHOT_BYTES + 16 > GS_DMA_QUEUE_BYTES;
    gs_dma_tag_t *dest = snapshot ? (gs_dma_tag_t *)queue_snapshot : VID_DMAReserve(GS_DMA_SNAPSHOT_BYTES);
    for (;;) {
        unsigned int qwc = source->control & 65535;
        unsigned int id = (source->control >> 28) & 7;
        int copy = id == 3 && VID_DMACopy(source->control, ranges, count);
        size_t size = 16 + ((id == 1 || id == 7 || copy) ? qwc * 16 : 0);
        if (bytes + size > GS_DMA_SNAPSHOT_BYTES)
            Sys_Error("VIF DMA snapshot exceeds capacity");
        *dest = *source;
        if (id == 7 || copy)
            dest->control = qwc | (1ULL << 28);
        if (copy)
            memcpy(dest + 1, VID_DMAAddress(source->control), qwc * 16);
        else if (id == 1 || id == 7)
            memcpy(dest + 1, source + 1, qwc * 16);
        dest = (gs_dma_tag_t *)((byte *)dest + size);
        bytes += size;
        if (id == 7)
            break;
        if (id != 1 && id != 3 && id != 5)
            Sys_Error("Unsupported VIF DMA tag %u", id);
        source += id == 1 ? qwc + 1 : 1;
    }
    if (snapshot) {
        dest = VID_DMAReserve(bytes);
        memcpy(dest, queue_snapshot, bytes);
    } else
        queue_used = first + bytes;
    if (queue_used >= queue_target)
        VID_QueueFlush();
}

void VID_QueueGIF(const void *chain, const ps2_gs_dma_range_t *ranges, int count)
{
    const gs_dma_tag_t *source = chain;
    gs_dma_tag_t *dest = VID_DMAReserve(16);
    dest->control = 1ULL << 28;
    dest->codes = 0x13000000;
    for (;;) {
        unsigned int qwc = source->control & 65535;
        unsigned int id = (source->control >> 28) & 7;
        const byte *data = id == 3 ? VID_DMAAddress(source->control) : (const void *)(source + 1);
        if (id == 3 && !VID_DMACopy(source->control, ranges, count)) {
            dest = VID_DMAReserve(16);
            *dest = *source;
            dest->codes = (0x50000000ULL | qwc) << 32;
            qwc = 0;
        }
        while (qwc) {
            unsigned int size = qwc > 2048 ? 2048 : qwc;
            dest = VID_DMAReserve((size + 1) * 16);
            dest->control = size | (1ULL << 28);
            dest->codes = (0x50000000ULL | size) << 32;
            memcpy(dest + 1, data, size * 16);
            data += size * 16;
            qwc -= size;
            if (queue_used >= queue_target)
                VID_QueueFlush();
        }
        if (id == 7)
            break;
        if (id != 1 && id != 3)
            Sys_Error("Unsupported GIF DMA tag %u", id);
        source += id == 1 ? (source->control & 65535) + 1 : 1;
    }
}

void VID_QueueShutdown(void)
{
    VID_QueueFlush();
    VID_QueueWait();
    if (queue_handler >= 0) {
        DisableDmac(1);
        RemoveDmacHandler(1, queue_handler);
    }
    queue_handler = -1;
    queue_handler_failed = 0;
    queue_current = queue_pending = NULL;
    free(queue_data[0]);
    free(queue_data[1]);
    queue_data[0] = queue_data[1] = NULL;
    queue_active = queue_failed = queue_buffer = 0;
}

static GSGLOBAL *present_gs;
static int present_sema = -1, present_handler = -1;
static volatile int present_pending, present_ready, present_buffer;


static void VID_DisplayBuffer(GSGLOBAL *gs, int buffer)
{
    GS_SET_DISPFB2(gs->ScreenBuffer[buffer] / 8192,
        gs->Width / 64, gs->PSM, 0, 0);
}

void VID_PresentImmediate(GSGLOBAL *gs)
{
    if (gs->DoubleBuffering == GS_SETTING_ON) {
        VID_DisplayBuffer(gs, gs->ActiveBuffer & 1);
        gs->ActiveBuffer ^= 1;
    }
    gsKit_setactive(gs);
    gs->FirstFrame = GS_SETTING_OFF;
}

static int VID_PresentVBlank(int cause)
{
    (void)cause;
    if (present_pending && GS_CSR_FINISH) {
        VID_DisplayBuffer(present_gs, present_buffer);
        present_ready = 1;
        present_pending = 0;
        iSignalSema(present_sema);
    }
    ExitHandler();
    return 0;
}

void VID_PresentBegin(GSGLOBAL *gs)
{
    unsigned int start = cpu_ticks();
    while (present_pending) {
        VID_QueuePoll();
        if (cpu_ticks() - start > 2 * kBUSCLK)
            Sys_PS2GPUError("Frame presentation");
        if (PollSema(present_sema) < 0)
            usleep(1000);
    }
    if (present_ready) {
        gs->ActiveBuffer = present_buffer ^ 1;
        gsKit_setactive(gs);
        present_ready = 0;
    }
}

int VID_PresentQueue(GSGLOBAL *gs)
{
    if (present_handler < 0) {
        ee_sema_t sema;
        sema.init_count = 0;
        sema.max_count = 1;
        sema.attr = 0;
        sema.option = 0;
        present_sema = CreateSema(&sema);
        if (present_sema < 0)
            return 0;
        present_handler = gsKit_add_vsync_handler(VID_PresentVBlank);
        if (present_handler < 0) {
            DeleteSema(present_sema);
            present_sema = -1;
            return 0;
        }
    }
    present_gs = gs;
    present_buffer = gs->ActiveBuffer & 1;
    present_pending = 1;
    return 1;
}

void VID_PresentShutdown(void)
{
    if (present_gs)
        VID_PresentBegin(present_gs);
    if (present_handler >= 0)
        gsKit_remove_vsync_handler(present_handler);
    if (present_sema >= 0)
        DeleteSema(present_sema);
    present_handler = present_sema = -1;
    present_gs = NULL;
}

#ifdef PLATFORM_USES_PROJECTION_WIDESCREEN
extern cvar_t vid_widescreen;
#endif
extern cvar_t vid_vsync;
extern cvar_t ps2_alias_strips;
extern cvar_t ps2_dma_queue;
extern cvar_t ps2_dma_batch;
extern cvar_t ps2_async_flip;
extern cvar_t r_profile;
extern cvar_t ps2_vu1;
extern cvar_t ps2_alias_vu1;
extern cvar_t ps2_alias_static;
extern cvar_t ps2_alias_batch;
extern cvar_t r_entity_sort;
extern cvar_t ps2_matrix_vu0;
extern cvar_t ps2_alias_cull;
extern cvar_t ps2_world_stream;
extern cvar_t ps2_world_cache;
extern cvar_t r_sprite_batch;
extern cvar_t ps2_dma_cache;
extern cvar_t ps2_packet_cache;
extern cvar_t ps2_world_indexed;
extern cvar_t ps2_vu_batch;
extern void Sys_PS2Memory_f(void);
unsigned short	d_8to16table[256];
unsigned	d_8to24table[256];
unsigned char d_15to8table[65536];
cvar_t gl_fog = {"gl_fog", "0", true};

int num_shades=32;

int	d_con_indirect = 0;

int		svgalib_inited=0;
int		UseMouse = 1;
int		UseKeyboard = 1;

cvar_t vid_mode = {"vid_mode", "5", true};
cvar_t vid_redrawfull = {"vid_redrawfull", "0", true};
cvar_t vid_waitforrefresh = {"vid_waitforrefresh", "1", true};
cvar_t gl_outline = {"gl_outline", "0", true};
cvar_t gl_mipmap = {"gl_mipmap", "1", true};

int gl_ssaa = 1;

extern cvar_t scr_conscale;
extern cvar_t scr_conwidth;

signed char *framebuffer_ptr;

int     mouse_buttons;
int     mouse_buttonstate;
int     mouse_oldbuttonstate;
float   mouse_x, mouse_y;
float	old_mouse_x, old_mouse_y;
int		mx, my;

int scr_width = 640, scr_height = 448;

/*-----------------------------------------------------------------------*/

int		texture_mode = HYE_FILTER_LINEAR;

double		gldepthmin, gldepthmax;

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

static float vid_gamma = 1.0;

bool isPermedia = false;

float sintablef[17] = {
	 0.000000f, 0.382683f, 0.707107f,
	 0.923879f, 1.000000f, 0.923879f,
	 0.707107f, 0.382683f, 0.000000f,
	-0.382683f,-0.707107f,-0.923879f,
	-1.000000f,-0.923879f,-0.707107f,
	-0.382683f, 0.000000f
};
	
float costablef[17] = {
	 1.000000f, 0.923879f, 0.707107f,
	 0.382683f, 0.000000f,-0.382683f,
	-0.707107f,-0.923879f,-1.000000f,
	-0.923879f,-0.707107f,-0.382683f,
	 0.000000f, 0.382683f, 0.707107f,
	 0.923879f, 1.000000f
};

/*-----------------------------------------------------------------------*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}

void VID_Shutdown(void)
{
	Hyena_GSFlush();
	VID_PresentShutdown();
	VID_VU1Shutdown();
}

void VID_ShiftPalette(unsigned char *p)
{
}

void	VID_SetPalette (unsigned char *palette)
{
	byte	*pal;
	unsigned r,g,b;
	unsigned v;
	int     r1,g1,b1;
	int		k;
	unsigned short i;
	unsigned	*table;
	int dist, bestdist;

//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to24table;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;
		
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		*table++ = v;
		
	}
	d_8to24table[255] &= 0xffffff;	// 255 is transparent
	
	// JACK: 3D distance calcs - k is last closest, l is the distance.
	for (i=0; i < (1<<15); i++) {
		/* Maps
		000000000000000
		000000000011111 = Red  = 0x1F
		000001111100000 = Blue = 0x03E0
		111110000000000 = Grn  = 0x7C00
		*/
		r = ((i & 0x1F) << 3)+4;
		g = ((i & 0x03E0) >> 2)+4;
		b = ((i & 0x7C00) >> 7)+4;
		pal = (unsigned char *)d_8to24table;
		for (v=0,k=0,bestdist=10000*10000; v<256; v++,pal+=4) {
			r1 = (int)r - (int)pal[0];
			g1 = (int)g - (int)pal[1];
			b1 = (int)b - (int)pal[2];
			dist = (r1*r1)+(g1*g1)+(b1*b1);
			if (dist < bestdist) {
				k=v;
				bestdist = dist;
			}
		}
		d_15to8table[i]=k;
	}
	
}

/*
===============
GL_Init
===============
*/
static void VID_InitRenderer(void)
{
	gl_vendor = "ps2dev";
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = "gsKit";
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = "GS native";
	Con_Printf ("GL_VERSION: %s\n", gl_version);

	Hyena_GSSetClearColor(0, 0, 0, 0);

	Cvar_RegisterVariable (&gl_fog);
	
	Hyena_GSSetBlendEquation(0, 1, 0, 1, 0);
	Hyena_GSSetReplaceTexture(0);

	Cvar_RegisterVariable(&vid_vsync);
	Cvar_RegisterVariable(&ps2_alias_strips);
	Cvar_RegisterVariable(&ps2_dma_queue);
	Cvar_RegisterVariable(&ps2_dma_batch);
	Cvar_RegisterVariable(&ps2_async_flip);
	Cvar_RegisterVariable(&ps2_vu1);
	Cvar_RegisterVariable(&ps2_alias_vu1);
	Cvar_RegisterVariable(&ps2_alias_static);
	Cvar_RegisterVariable(&ps2_alias_batch);
	Cvar_RegisterVariable(&ps2_matrix_vu0);
	Cvar_RegisterVariable(&ps2_alias_cull);
	Cvar_RegisterVariable(&ps2_world_stream);
	Cvar_RegisterVariable(&ps2_world_cache);
	Cvar_RegisterVariable(&ps2_dma_cache);
	Cvar_RegisterVariable(&ps2_packet_cache);
	Cvar_RegisterVariable(&ps2_world_indexed);
	Cvar_RegisterVariable(&ps2_vu_batch);
	Cmd_AddCommand("ps2_mem", Sys_PS2Memory_f);
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
#ifdef PLATFORM_USES_PROJECTION_WIDESCREEN
	int width_value = scr_width;
	if (vid_widescreen.value) {
		GSGLOBAL *gs = Hyena_GSGlobal();
		float aspect = 16.0f / 9.0f;
		if (gs && (gs->Mode == GS_MODE_NTSC || gs->Mode == GS_MODE_PAL)) {
			float lines = gs->Mode == GS_MODE_PAL ? 576.0f : 480.0f;
			if (gs->Interlace == GS_NONINTERLACED)
				lines *= 0.5f;
			aspect *= gs->DW * lines / (2880.0f * gs->DH);
		}
		width_value = (int)(scr_height * aspect + 0.5f);
	}
	if (vid.width != width_value) {
		vid.width = width_value;
		vid.recalc_refdef = 1;
	}
#endif
	*x = *y = 0;
	*width = scr_width;
	*height = scr_height;
	Hyena_GSBeginFrame();
}

void GL_EndRendering (void)
{
	Draw_Flush();
	Hyena_GSEndFrame(vid_vsync.value != 0);
}

void VID_SetDepthRange(float near_value, float far_value)
{
    Hyena_GSSetDepthRange(near_value, far_value);
}

static void Check_Gamma (unsigned char *pal)
{
	float	f, inf;
	unsigned char	palette[768];
	int		i;

	if ((i = COM_CheckParm("-gamma")) == 0) {
		vid_gamma = 0.7; // default to 0.7 on non-3dfx hardware
	} else
		vid_gamma = atof(com_argv[i+1]);

	for (i=0 ; i<768 ; i++)
	{
		f = pow ( (pal[i]+1)/256.0 , vid_gamma );
		inf = f*255 + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		palette[i] = inf;
	}

	memcpy (pal, palette, sizeof(palette));
}

static void VID_InitBuffers(void)
{
	Sys_PS2BeginVideo();
	Hyena_GSInit();
}

void VID_Init(unsigned char *palette)
{
	int width = scr_width, height = scr_height;
	
	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&vid_redrawfull);
	Cvar_RegisterVariable (&vid_waitforrefresh);
	Cvar_RegisterVariable (&gl_outline);
	Cvar_RegisterVariable (&gl_mipmap);
	
	vid.maxwarpwidth = width;
	vid.maxwarpheight = height;
	vid.colormap = host_colormap;
	vid.fullbright = 0xFFFF;
	vid.aspect = (float) width / (float) height;
	vid.numpages = 2;
	vid.rowbytes = 2 * width;
	vid.width = width;
	vid.height = height;
	vid.scale = 1.75f;

	vid.conwidth = 400;
	vid.conheight = 240;
	
	VID_InitBuffers();
	VID_InitRenderer();

	Check_Gamma(palette);
	VID_SetPalette(palette);


	Con_SafePrintf ("Video mode %dx%d initialized.\n", width, height);

	vid.recalc_refdef = 1;				// force a surface cache flush
}

void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}
