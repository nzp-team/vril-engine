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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "../../../nzportable_def.h"
#include "../../../render/r_texture.h"
#include "gnm_main.h"
#include "../ps4_local.h"
#include <gnm/drawcommandbuffer.h>
#include <gnm/gpuaddr/gpuaddr.h>
#include <gnm/platform.h>
#include <gnm_helpers.h>
#include <gnmdriver.h>
#include <pm4/amdgfxregs.h>
#include <orbis/libkernel.h>
#include <orbis/SystemService.h>
#include "displayctx.h"
#include "misc.h"
#include "memalloc.h"

#define MAX_TEXTURES    8192
#define TRANSIENT_BYTES (32u * 1024 * 1024)
#define COMMAND_BYTES   (1024u * 1024)

typedef struct {
    GnmTexture image;
    GnmSampler sampler;
    uint8_t    pad[16];
    GnmBuffer  constants;
} ps_table_t;

_Static_assert(sizeof(ps_table_t) == 80 && offsetof(ps_table_t, constants) == 64,
  "Native PS descriptor ABI");
_Static_assert(sizeof(gnmr_uniforms_t) == 192, "std140 draw state ABI");

typedef struct {
    GnmDirectMemory memory;
    GnmTexture      image;
    int             used, width, height, min_filter, mag_filter, mipmaps_requested, mip_count;
    int             dynamic;
    uint8_t *       staging;
    uint64_t        revision, snapshot_revision, snapshot_serial;
    void *          snapshot;
} gnmr_texture_t;

typedef struct {
    int      blend, depth, cull, depth_write, offset;
    unsigned src, dst, depth_func, face;
    float    near_value, far_value, offset_factor, offset_units;
    int      x, y, w, h;
} render_state_t;

static gnmr_texture_t textures[MAX_TEXTURES];
static unsigned bound_texture;
static render_state_t state;
static MemoryAllocator permanent;
static DisplayContext display;
static GnmRenderTarget scene, output[2];
static GnmDepthRenderTarget depth_target;
static GnmTexture scene_texture;
static GnmVsShader * game_vs, * present_vs;
static GnmPsShader * game_ps, * present_ps, * clear_ps;
static void * game_fetch, * command_memory;
static uint8_t * transient;
static size_t transient_used;
static GnmCommandBuffer command;
static volatile uint64_t * completion;
static uint64_t serial;
static unsigned output_index, frames;
static int initialized, active, has_commands;
static float clear_color[4], clear_depth = 1;
static unsigned last_error;
static char graphics_error[192];

static void
Context(unsigned reg, uint32_t value)
{
    *command.cmdptr++ = 0xc0016900u; // SET_CONTEXT_REG, offset + one register.
    *command.cmdptr++ = (reg - 0x28000u) / 4;
    *command.cmdptr++ = value;
}

static uint32_t
FloatBits(float f)
{
    uint32_t value;

    memcpy(&value, &f, 4);

    return value;
}

static void
ResetCommands(void)
{
    command = gnmCmdInit(command_memory, COMMAND_BYTES, NULL, NULL);
    sceGnmDrawCmdInitDefaultHardwareState(&command);
    // Invalidate everything!!!
    *command.cmdptr++ = 0xc0055800u; // ACQUIRE_MEM
    *command.cmdptr++ =
      S_0301F0_TCL1_ACTION_ENA(1) | S_0301F0_TC_ACTION_ENA(1) | S_0301F0_SH_KCACHE_ACTION_ENA(1);
    *command.cmdptr++ = 0xffffffffu;
    *command.cmdptr++ = 0xffu;
    *command.cmdptr++ = 0;
    *command.cmdptr++ = 0;
    *command.cmdptr++ = 10;

    Context(0x28810, 1u << 19); // Explicit DX 0..w clip depth; VS converts GL clip Z.
    has_commands   = 0;
    transient_used = 0;
}

static void
SubmitCommands(void)
{
    if (!has_commands)
        return;

    uint64_t token = ++serial;
    uint32_t * fence = command.cmdptr;

    gnmDrawCmdEventWriteEop(&command, GNM_CACHE_FLUSH_TS, (uint64_t) completion, GNM_DATA_SEL_SEND_DATA64, token);
    fence[3] &= ~(3u << 24);

    void * dcb = command.beginptr, * ccb = NULL;
    uint32_t bytes = (command.cmdptr - command.beginptr) * 4, zero = 0;

    __sync_synchronize();

    int result = sceGnmSubmitCommandBuffers(1, &dcb, &bytes, &ccb, &zero);

    if (result < 0)
        Sys_Error("GNM submit failed: 0x%08x", result);

    unsigned waits = 0;

    while (*completion != token && waits++ < 5000) {
        sceKernelUsleep(1000);
    }

    if (*completion != token) {
        Sys_Error("GNM GPU completion timeout");
    }

    sceGnmSubmitDone();
    ResetCommands();
}

void
GNMR_Finish(void)
{
    if (initialized)
        SubmitCommands();
}

static void *
Allocate(size_t size, size_t alignment)
{
    size_t offset = (transient_used + alignment - 1) & ~(alignment - 1);

    if (offset + size > TRANSIENT_BYTES)
        Sys_Error("GNM draw exceeds transient arena");

    void * result = transient + offset;

    transient_used = offset + size;

    return result;
}

static void
BindSlots(GnmShaderStage stage, const GnmInputUsageSlot * slots, unsigned count,
  void * fetch, void * vertices, void * table)
{
    for (unsigned i = 0; i < count; ++i) {
        void * address = NULL;

        switch (slots[i].usagetype) {
            case GNM_SHINPUTUSAGE_SUBPTR_FETCHSHADER:
                address = fetch;
                break;
            case GNM_SHINPUTUSAGE_PTR_VERTEXBUFFERTABLE:
                address = vertices;
                break;
            case GNM_SHINPUTUSAGE_PTR_INDIRECTRESOURCETABLE:
                address = table;
                break;
            default:
                Sys_Error("GNM unsupported shader resource: %x", slots[i].usagetype);
        }

        if (!address)
            Sys_Error("GNM unbound shader resource");

        gnmDrawCmdSetPointerUserData(&command, stage, slots[i].startregister, address);
    }
}

static void
SetShaderIdentity(const GnmShaderCommonData * common, const void * registers)
{
    uint32_t address[2];
    uint32_t size = gnmShaderCommonCodeSize(common);

    memcpy(address, registers, sizeof(address));

    uint8_t * code = (uint8_t *) (((uint64_t) address[0] << 8) | ((uint64_t) address[1] << 40));

    if (size < sizeof(GnmShaderBinaryInfo))
        Sys_Error("GNM shader binary info missing");

    GnmShaderBinaryInfo * info = (GnmShaderBinaryInfo *) (code + size - sizeof(*info));

    if (memcmp(info->signature, "OrbShdr", 7) || info->length > size - sizeof(*info))
        Sys_Error("GNM shader binary info invalid");

    if (info->shaderhash0 || info->shaderhash1)
        return;

    uint64_t hash = UINT64_C(14695981039346656037);

    for (uint32_t i = 0; i < info->length; ++i) {
        hash ^= code[i];
        hash *= UINT64_C(1099511628211);
    }

    info->shaderhash0 = (uint32_t) hash;
    info->shaderhash1 = (uint32_t) (hash >> 32);
}

static void
BindProgram(GnmVsShader * vs, GnmPsShader * ps, void * fetch, GnmBuffer * vertices,
  GnmBuffer * vs_table, ps_table_t * ps_table)
{
    for (unsigned r = 0; r < 16; r += 2) {
        gnmDrawCmdSetPointerUserData(&command, GNM_STAGE_VS, r, NULL);
        gnmDrawCmdSetPointerUserData(&command, GNM_STAGE_PS, r, NULL);
    }

    gnmDrawCmdSetVsShader(&command, &vs->registers, 0);
    gnmDrawCmdSetPsShader(&command, &ps->registers);
    BindSlots(GNM_STAGE_VS, gnmVsShaderInputUsageSlotTable(vs), vs->common.numinputusageslots,
      fetch, vertices, vs_table);

    if (fetch)
        gnmDrawCmdSetPointerUserData(&command, GNM_STAGE_VS, 0, fetch);

    BindSlots(GNM_STAGE_PS, gnmPsShaderInputUsageSlotTable(ps), ps->common.numinputusageslots, NULL,
      NULL, ps_table);
    gnmDrawCmdSetPsInputUsage(&command, gnmVsShaderExportSemanticTable(vs), vs->numexportsemantics,
      gnmPsShaderInputSemanticTable(ps), ps->numinputsemantics);
}

static GnmBlendOp
NativeBlendFactor(unsigned b)
{
    switch (b) {
        case GNMR_ZERO:
            return GNM_BLEND_ZERO;

        case GNMR_ONE:
            return GNM_BLEND_ONE;

        case GNMR_SRC_ALPHA:
            return GNM_BLEND_SRC_ALPHA;

        case GNMR_ONE_MINUS_SRC_ALPHA:
            return GNM_BLEND_ONE_MINUS_SRC_ALPHA;

        case GNMR_SRC_COLOR:
            return GNM_BLEND_SRC_COLOR;

        case GNMR_ONE_MINUS_SRC_COLOR:
            return GNM_BLEND_ONE_MINUS_SRC_COLOR;

        case GNMR_DST_COLOR:
            return GNM_BLEND_DEST_COLOR;

        default:
            Sys_Error("GNM unsupported blend factor %u", b);
    }
}

static void
ApplyState(GnmRenderTarget * target, int with_depth)
{
    gnmDrawCmdSetRenderTarget(&command, 0, target);
    gnmDrawCmdSetRenderTargetMask(&command, 15);
    gnmDrawCmdSetDepthRenderTarget(&command, with_depth ? &depth_target : NULL);

    GnmDbRenderControl db = { 0 };

    gnmDrawCmdSetDbRenderControl(&command, &db);

    GnmDepthStencilControl d = { 0 };

    d.depthenable = with_depth && state.depth;
    d.zwrite      = state.depth_write;
    d.zfunc       = state.depth_func == GNMR_EQUAL ? GNM_DEPTH_COMPARE_EQUAL : state.depth_func ==
      GNMR_GEQUAL ? GNM_DEPTH_COMPARE_GREATEREQUAL : GNM_DEPTH_COMPARE_LESSEQUAL;

    gnmDrawCmdSetDepthStencilControl(&command, &d);
    GnmBlendControl b = { 0 };

    b.blendenabled = state.blend;
    b.colorfunc    = b.alphafunc = GNM_COMB_DST_PLUS_SRC;
    b.colorsrcmult = b.alphasrcmult = NativeBlendFactor(state.src);
    b.colordstmult = b.alphadstmult = NativeBlendFactor(state.dst);

    gnmDrawCmdSetBlendControl(&command, 0, &b);
    GnmPrimitiveSetup p = { 0 };

    p.cullmode        = !state.cull ? GNM_CULL_NONE : state.face == GNMR_FRONT ? GNM_CULL_FRONT : GNM_CULL_BACK;
    p.frontface       = GNM_FACE_CCW;
    p.frontmode       = p.backmode = GNM_FILL_SOLID;
    p.provokemode     = GNM_PROVOKINGVTX_LAST;
    p.frontoffsetmode = p.backoffsetmode = state.offset;

    gnmDrawCmdSetPrimitiveSetup(&command, &p);

    if (state.offset) {
        Context(0x28b80, FloatBits(state.offset_factor * 16));
        Context(0x28b84, FloatBits(state.offset_units));
        Context(0x28b88, FloatBits(state.offset_factor * 16));
        Context(0x28b8c, FloatBits(state.offset_units));
    }

    int top = 1080 - state.y - state.h;
    GnmSetViewportInfo v = {
        .dmin   = 0,
        .dmax   = 1,
        .scale  = { state.w * .5f,        -state.h * .5f,           state.far_value - state.near_value       },
        .offset = { state.x + state.w * .5f,top + state.h * .5f,      state.near_value                         } };

    gnmDrawCmdSetViewport(&command, 0, &v);
    GnmViewportTransformControl vt = { .scalex  = 1,
                                       .offsetx = 1,
                                       .scaley  = 1,
                                       .offsety = 1,
                                       .scalez  = 1,
                                       .offsetz = 1,
                                       .perspectivedividexy = 0,
                                       .perspectivedividez  = 0,
                                       .invertw             = 1 };

    gnmDrawCmdSetViewportTransformControl(&command, &vt);
    gnmDrawCmdSetScreenScissor(&command, 0, 0, 1920, 1080);
} /* ApplyState */

void
GNMR_BeginFrame(void)
{
    if (!active) {
        ResetCommands();
        active = 1;
    }
}

void
GNMR_Draw(unsigned primitive, const gnmr_vertex_t * vertices, size_t count,
  const gnmr_uniforms_t * uniforms)
{
    if (!count)
        return;

    GNMR_BeginFrame();

    gnmr_texture_t * t = &textures[bound_texture];

    if (!t->memory.allocated)
        t = &textures[0];

    size_t snapshot_bytes = t->dynamic ? t->memory.size + 256 : 0;
    size_t needed         = count * sizeof(*vertices) + sizeof(*uniforms) + 2048;

    if (needed + snapshot_bytes > TRANSIENT_BYTES)
        Sys_Error("GNM oversized draw");

    if (t->dynamic && (!t->snapshot || t->snapshot_serial != serial ||
      t->snapshot_revision != t->revision))
        needed += snapshot_bytes;

    if (transient_used + needed > TRANSIENT_BYTES || command.endptr - command.cmdptr < 2048)
        SubmitCommands();

    gnmr_vertex_t * data = Allocate(count * sizeof(*data), 256);

    memcpy(data, vertices, count * sizeof(*data));

    gnmr_uniforms_t * u = Allocate(sizeof(*u), 256);

    memcpy(u, uniforms, sizeof(*u));

    GnmBuffer * buffers = Allocate(3 * sizeof(*buffers), 256);

    buffers[0] = gnmCreateVertexBuffer(data, GNM_FMT_R32G32B32_FLOAT, sizeof(*data), count);
    buffers[1] =
      gnmCreateVertexBuffer((char *) data + 12, GNM_FMT_R32G32_FLOAT, sizeof(*data), count);
    buffers[2] =
      gnmCreateVertexBuffer((char *) data + 20, GNM_FMT_R32G32B32A32_FLOAT, sizeof(*data), count);

    GnmBuffer * vs_table = Allocate(sizeof(*vs_table), 256);

    *vs_table = gnmCreateConstBuffer(u, sizeof(*u));

    ps_table_t * ps_table = Allocate(sizeof(*ps_table), 256);

    memset(ps_table, 0, sizeof(*ps_table));
    ps_table->image = t->image;

    if (t->dynamic) {
        if (!t->snapshot || t->snapshot_serial != serial || t->snapshot_revision != t->revision) {
            t->snapshot = Allocate(t->memory.size, 256);
            memcpy(t->snapshot, t->staging, t->memory.size);
            t->snapshot_serial   = serial;
            t->snapshot_revision = t->revision;
        }

        gnmTexSetBaseAddress(&ps_table->image, t->snapshot);
    }

    ps_table->sampler.xyminfilter =
      (t->min_filter == GNMR_NEAREST || t->min_filter == GNMR_NEAREST_MIPMAP_NEAREST ||
      t->min_filter == GNMR_NEAREST_MIPMAP_LINEAR) ?
      GNM_FILTER_POINT :
      GNM_FILTER_BILINEAR;

    if (t->mip_count > 1 && t->min_filter != GNMR_NEAREST && t->min_filter != GNMR_LINEAR) {
        ps_table->sampler.mipfilter = (t->min_filter == GNMR_NEAREST_MIPMAP_LINEAR ||
          t->min_filter == GNMR_LINEAR_MIPMAP_LINEAR) ?
          GNM_MIPFILTER_LINEAR :
          GNM_MIPFILTER_POINT;
        ps_table->sampler.maxlod = (t->mip_count - 1) * 256;
    }

    ps_table->sampler.xymagfilter =
      t->mag_filter == GNMR_NEAREST ? GNM_FILTER_POINT : GNM_FILTER_BILINEAR;
    ps_table->constants = *vs_table;

    ApplyState(&scene, 1);
    BindProgram(game_vs, game_ps, game_fetch, buffers, vs_table, ps_table);
    GnmPrimitiveType type = primitive == GNMR_TRIANGLE_STRIP ? GNM_PT_TRISTRIP :
      primitive == GNMR_TRIANGLE_FAN ? GNM_PT_TRIFAN :
      primitive == GNMR_LINE_STRIP ? GNM_PT_LINESTRIP :
      GNM_PT_TRILIST;

    gnmDrawCmdSetPrimitiveType(&command, type);
    gnmDrawCmdDrawIndexAuto(&command, count);
    has_commands = 1;
} /* GNMR_Draw */

void
GNMR_Enable(unsigned cap)
{
    switch (cap) {
        case GNMR_BLEND:
            state.blend = 1;
            break;
        case GNMR_DEPTH_TEST:
            state.depth = 1;
            break;
        case GNMR_CULL_FACE:
            state.cull = 1;
            break;
        case GNMR_POLYGON_OFFSET_FILL:
            state.offset = 1;
            break;
        default:
            Sys_Error("GNM capability %u", cap);
    }
}

void
GNMR_Disable(unsigned cap)
{
    switch (cap) {
        case GNMR_BLEND:
            state.blend = 0;
            break;
        case GNMR_DEPTH_TEST:
            state.depth = 0;
            break;
        case GNMR_CULL_FACE:
            state.cull = 0;
            break;
        case GNMR_POLYGON_OFFSET_FILL:
            state.offset = 0;
            break;
        default:
            Sys_Error("GNM capability %u", cap);
    }
}

void
GNMR_BlendFunc(unsigned src, unsigned dst)
{
    state.src = src;
    state.dst = dst;
}

void
GNMR_DepthMask(int enabled)
{
    state.depth_write = enabled;
}

void
GNMR_DepthFunc(unsigned fn)
{
    state.depth_func = fn;
}

void
GNMR_DepthRange(float n, float f)
{
    state.near_value = n;
    state.far_value  = f;
}

void
GNMR_PolygonOffset(float f, float u)
{
    state.offset_factor = f;
    state.offset_units  = u;
}

void
GNMR_Viewport(int x, int y, int w, int h)
{
    state.x = x;
    state.y = y;
    state.w = w;
    state.h = h;
}

void
GNMR_CullFace(unsigned face)
{
    state.face = face;
}

void
GNMR_ClearColor(float r, float g, float b, float a)
{
    clear_color[0] = r;
    clear_color[1] = g;
    clear_color[2] = b;
    clear_color[3] = a;
}

void
GNMR_ClearDepth(float d)
{
    clear_depth = d;
}

void
GNMR_Clear(unsigned mask)
{
    GNMR_BeginFrame();

    if (command.endptr - command.cmdptr < 4096 || transient_used + 4096 > TRANSIENT_BYTES)
        SubmitCommands();

    if (mask & (GNMR_DEPTH_BUFFER_BIT | GNMR_STENCIL_BUFFER_BIT)) {
        GnmDbRenderControl db = { 0 };

        db.depthclearenable   = !!(mask & GNMR_DEPTH_BUFFER_BIT);
        db.stencilclearenable = !!(mask & GNMR_STENCIL_BUFFER_BIT);
        gnmDrawCmdSetDbRenderControl(&command, &db);
        GnmDepthStencilControl ds = {
            .zwrite = true, .zfunc = GNM_DEPTH_COMPARE_ALWAYS, .depthenable = true
        };

        gnmDrawCmdSetDepthStencilControl(&command, &ds);
        gnmDrawCmdSetDepthRenderTarget(&command, &depth_target);
        gnmDrawCmdSetDepthClearValue(&command, clear_depth);
        Context(0x28028, 0); // DB_STENCIL_CLEAR.
        gnmDrawCmdSetRenderTargetMask(&command, 0);
        gnmDrawCmdSetEmbeddedVsShader(&command, GNM_EMBEDDED_VSH_FULLSCREEN, 0);
        gnmDrawCmdSetPsShader(&command, &clear_ps->registers);
        setupviewport(&command, 0, 0, 1920, 1080, .5f, .5f);
        gnmDrawCmdSetPrimitiveType(&command, GNM_PT_RECTLIST);
        gnmDrawCmdDrawIndexAuto(&command, 3);
        has_commands = 1;
    }

    if (mask & GNMR_COLOR_BUFFER_BIT) {
        render_state_t saved = state;

        state.blend = state.depth = state.cull = state.offset = 0;
        state.x     = state.y = 0;
        state.w     = 1920;
        state.h     = 1080;
        gnmr_uniforms_t u = { 0 };

        for (int i = 0; i < 4; ++i)
            u.mvp[i * 5] = u.modelview[i * 5] = 1;

        u.uv[0]     = u.uv[1] = 1;
        u.params[2] = -1;
        gnmr_vertex_t v[6]   = { 0 };
        const float xy[6][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, -1 }, { 1, 1 }, { -1, 1 } };

        for (int i = 0; i < 6; ++i) {
            v[i].position[0] = xy[i][0];
            v[i].position[1] = xy[i][1];
            memcpy(v[i].color, clear_color, 16);
        }

        GNMR_Draw(GNMR_TRIANGLES, v, 6, &u);
        state = saved;
    }
} /* GNMR_Clear */

unsigned
GNMR_CreateTexture(void)
{
    for (unsigned i = 1; i < MAX_TEXTURES; ++i)
        if (!textures[i].used) {
            textures[i].used       = 1;
            textures[i].min_filter = textures[i].mag_filter = GNMR_LINEAR;

            return i;
        }

    Sys_Error("GNM texture registry full");
}

void
GNMR_BindTexture(unsigned t)
{
    if (t >= MAX_TEXTURES)
        Sys_Error("GNM invalid texture %u", t);

    bound_texture = t;
}

void
GNMR_DestroyTexture(unsigned t)
{
    if (!t || t >= MAX_TEXTURES)
        return;

    GNMR_Finish();
    free(textures[t].staging);
    sceGnmDirectMemoryRelease(&textures[t].memory);
    memset(&textures[t], 0, sizeof(textures[t]));

    if (bound_texture == t)
        bound_texture = 0;
}

void
GNMR_TextureFilter(int min_filter, int mag_filter)
{
    textures[bound_texture].min_filter = min_filter;
    textures[bound_texture].mag_filter = mag_filter;
}

void
GNMR_DynamicTexture(int enabled)
{
    textures[bound_texture].dynamic = enabled;
}

void
GNMR_Mipmaps(int enabled)
{
    textures[bound_texture].mipmaps_requested = enabled;
}

void
GNMR_Upload(int level, int storage, int w, int h, int x, int y, int replace,
  const void * pixels)
{
    if (w < 1 || h < 1 || w > 8192 || h > 8192 || !pixels)
        Sys_Error("GNM invalid texture upload");

    // Full image replacements ignore dirty-region offsets, as glTexImage2D does.
    if (replace)
        x = y = 0;

    gnmr_texture_t * t = &textures[bound_texture];

    // Cached CPU lightmaps feed immutable GPU snapshots; never read back WC memory.
    if (!t->dynamic)
        GNMR_Finish();

    if (replace && !level) {
        free(t->staging);
        t->staging = NULL;
        sceGnmDirectMemoryRelease(&t->memory);
        t->width     = w;
        t->height    = h;
        t->mip_count = 1;

        if (t->mipmaps_requested)
            for (int mw = w, mh = h; mw > 1 || mh > 1;) {
                mw = mw > 1 ? mw / 2 : 1;
                mh = mh > 1 ? mh / 2 : 1;
                ++t->mip_count;
            }

        GnmTextureCreateInfo ci = { .texturetype  = GNM_TEXTURE_2D,
                                    .width        = w,
                                    .height       = h,
                                    .depth        = 1,
                                    .pitch        = w,
                                    .nummiplevels = t->mip_count,
                                    .numslices    = 1,
                                    .format       = GNM_FMT_R8G8B8A8_UNORM,
                                    .tilemodehint = GNM_TM_DISPLAY_LINEAR_ALIGNED,
                                    .mingpumode   = GNM_GPU_BASE,
                                    .numfragments = 1 };

        if (gnmCreateTexture(&t->image, &ci) != GNM_ERROR_OK)
            Sys_Error("GNM texture layout failed %dx%d", w, h);

        uint64_t size      = 0;
        uint32_t alignment = 0;
        GnmError layout    = gnmTexCalcByteSize(&size, &alignment, &t->image);

        if (layout != GNM_ERROR_OK || !size || size > 256u * 1024 * 1024)
            Sys_Error("GNM texture size failed %dx%d: %x (%llu bytes)", w, h, layout,
              (unsigned long long) size);

        GnmError allocation = sceGnmDirectMemoryAllocate(
            &t->memory, size, 65536, GNM_DIRECT_MEMORY_TYPE_WC_GARLIC, GNM_PROT_CPU_GPU_RW);

        if (allocation != GNM_ERROR_OK)
            Sys_Error("GNM texture allocation failed %dx%d: %x (%llu bytes)", w, h, allocation,
              (unsigned long long) size);

        gnmTexSetBaseAddress(&t->image, t->memory.mapped);

        if (t->dynamic) {
            t->staging = calloc(1, t->memory.size);

            if (!t->staging)
                Sys_Error("GNM lightmap staging allocation failed");
        }
    }

    if (!t->memory.allocated || level < 0 || level >= t->mip_count)
        Sys_Error("GNM texture mip outside allocation");

    int level_width = t->width >> level, level_height = t->height >> level;

    if (level_width < 1)
        level_width = 1;

    if (level_height < 1)
        level_height = 1;

    if (x < 0 || y < 0 || x + w > level_width || y + h > level_height)
        Sys_Error("GNM texture update outside allocation");

    GpaTextureInfo info = gnmTexBuildInfo(&t->image);
    GpaTilingParams tp;
    GpaSurfaceInfo surface;
    uint64_t offset, size;

    if (gpaTpInit(&tp, &info, level, 0) != GPA_ERR_OK ||
      gpaComputeSurfaceInfo(&surface, &tp) != GPA_ERR_OK ||
      gpaComputeSurfaceSizeOffset(&size, &offset, &info, level, 0) != GPA_ERR_OK ||
      offset + size > t->memory.size)
        Sys_Error("GNM mip layout calculation failed");

    const uint8_t * bytes  = pixels;
    const uint16_t * words = pixels;

    for (int row = 0; row < h; ++row)
        for (int col = 0; col < w; ++col) {
            size_t i    = (size_t) row * w + col;
            uint8_t * p = (t->staging ? t->staging : (uint8_t *) t->memory.mapped) + offset
              + ((size_t) (y + row) * surface.pitch + x + col) * 4;

            if (storage == HYE_PIXELS_RGBA8)
                memcpy(p, bytes + 4 * i, 4);
            else if (storage == HYE_PIXELS_L8) {
                p[0] = p[1] = p[2] = bytes[i];
                p[3] = 255;
            } else if (storage == HYE_PIXELS_RGBA5551) {
                uint16_t v = words[i];
                p[0] = ((v >> 11) & 31) * 255 / 31;
                p[1] = ((v >> 6) & 31) * 255 / 31;
                p[2] = ((v >> 1) & 31) * 255 / 31;
                p[3] = (v & 1) ? 255 : 0;
            } else if (storage == HYE_PIXELS_RGBA4444) {
                uint16_t v = words[i];
                p[0] = ((v >> 12) & 15) * 17;
                p[1] = ((v >> 8) & 15) * 17;
                p[2] = ((v >> 4) & 15) * 17;
                p[3] = (v & 15) * 17;
            } else
                Sys_Error("GNM unsupported pixel storage %d", storage);
        }

    ++t->revision;
} /* GNMR_Upload */

unsigned
GNMR_GetError(void)
{
    unsigned error = last_error;

    last_error = 0;

    return error;
}

int
GNMR_ReadPixels(int x, int y, int w, int h, unsigned f, unsigned t, void * pixels)
{
    if (!pixels || w < 1 || h < 1 || x < 0 || y < 0 || x + w > 1920 || y + h > 1080 ||
      f != GNMR_RGBA || t != GNMR_UNSIGNED_BYTE)
    {
        last_error = GNMR_INVALID_VALUE;

        return 0;
    }

    GNMR_Finish();
    GpaTextureInfo source = gnmTexBuildInfo(&scene_texture), linear = source;

    linear.tm = GNM_TM_DISPLAY_LINEAR_GENERAL;
    uint64_t size = 0, offset = 0, source_size = 0;
    uint32_t alignment = 0;

    if (gpaComputeSurfaceSizeOffset(&size, &offset, &linear, 0, 0) != GPA_ERR_OK ||
      gnmTexCalcByteSize(&source_size, &alignment, &scene_texture) != GNM_ERROR_OK)
    {
        last_error = GNMR_INVALID_OPERATION;

        return 0;
    }

    uint8_t * buffer = malloc(size);

    if (!buffer) {
        last_error = GNMR_OUT_OF_MEMORY;

        return 0;
    }

    if (sceGpaTileTextureIndexed(gnmRtGetBaseAddr(&scene), source_size, buffer, size, &source,
      linear.tm, 0, 0) != GPA_ERR_OK)
    {
        free(buffer);
        last_error = GNMR_INVALID_OPERATION;

        return 0;
    }

    GpaTilingParams tp;
    GpaSurfaceInfo surface;

    if (gpaTpInit(&tp, &linear, 0, 0) != GPA_ERR_OK ||
      gpaComputeSurfaceInfo(&surface, &tp) != GPA_ERR_OK)
    {
        free(buffer);
        last_error = GNMR_INVALID_OPERATION;

        return 0;
    }

    for (int row = 0; row < h; ++row)
        memcpy((uint8_t *) pixels + (size_t) row * w * 4,
          buffer + ((size_t) (1079 - y - row) * surface.pitch + x) * 4, (size_t) w * 4);

    free(buffer);

    return 1;
} /* GNMR_ReadPixels */

const char *
PS4_GraphicsError(void)
{
    return graphics_error;
}

int
PS4_GraphicsInit(void)
{
    if (initialized)
        return 1;

    char path[256];
    const char * sandbox = sceKernelGetFsSandboxRandomWord();
    int start = 0;

    if (!sandbox) {
        snprintf(graphics_error, sizeof(graphics_error), "missing sandbox");

        return 0;
    }

    snprintf(path, sizeof(path), "/%s/common/lib/libSceGnmDriver.sprx", sandbox);
    int driver = sceKernelLoadStartModule(path, 0, NULL, 0, NULL, &start);

    // Emulators may provide GNM directly without a physical system-module file.
    // Keep the console load path and require an operational driver on fallback.
    if (((driver < 0 || start < 0) && sceGnmAreSubmitsAllowed() <= 0) ||
      PS4_LoadModule(0x80000022) < 0 || PS4_LoadModule(0x80000010) < 0)
    {
        snprintf(graphics_error, sizeof(graphics_error), "GNM system module load failed");

        return 0;
    }

    permanent = memalloc_init(128 * 1024 * 1024, GNM_PROT_CPU_GPU_RW, ORBIS_KERNEL_WC_GARLIC);

    if (!displayctx_init(&display))
        Sys_Error("GNM VideoOut initialization failed");

    // Render at 1080p independently of the display mode reported by VideoOut.
    if (!initcolortarget(&scene, &permanent, 1920, 1080, GNM_FMT_R8G8B8A8_UNORM, gnmGpuMode()) ||
      !initcolortarget(&output[0], &permanent, 1920, 1080, GNM_FMT_R8G8B8A8_UNORM,
      gnmGpuMode()) ||
      !initcolortarget(&output[1], &permanent, 1920, 1080, GNM_FMT_R8G8B8A8_UNORM,
      gnmGpuMode()) ||
      !initdepthtarget(&depth_target, &permanent, 1920, 1080, gnmRtGetPitch(&scene), 1, 1,
      GNM_Z_32_FLOAT, GNM_STENCIL_8, gnmGpuMode()))
        Sys_Error("GNM framebuffer allocation failed");

    if (!displayctx_setrts(&display, output, 2))
        Sys_Error("GNM display buffer registration failed");

    GnmTextureCreateInfo ti = { .texturetype  = GNM_TEXTURE_2D,
                                .width        = 1920,
                                .height       = 1080,
                                .depth        = 1,
                                .pitch        = gnmRtGetPitch(&scene),
                                .nummiplevels = 1,
                                .numslices    = 1,
                                .format       = GNM_FMT_R8G8B8A8_UNORM,
                                .tilemodehint = scene.attrib.tilemode_index,
                                .mingpumode   = gnmGpuMode(),
                                .numfragments = 1 };

    if (gnmCreateTexture(&scene_texture, &ti) != GNM_ERROR_OK)
        Sys_Error("GNM FXAA source layout failed");

    gnmTexSetBaseAddress(&scene_texture, gnmRtGetBaseAddr(&scene));

    if (!loadvshader(&game_vs, &permanent, "/app0/shaders/hyena.vert.sb") ||
      !loadpshader(&game_ps, &permanent, "/app0/shaders/hyena.frag.sb") ||
      !loadvshader(&present_vs, &permanent, "/app0/shaders/present.vert.sb") ||
      !loadpshader(&present_ps, &permanent, "/app0/shaders/present.frag.sb") ||
      !loadpshader(&clear_ps, &permanent, "/app0/shaders/clear.frag.sb"))
        Sys_Error("GNM compiled shader load failed");

    SetShaderIdentity(&game_vs->common, &game_vs->registers);
    SetShaderIdentity(&game_ps->common, &game_ps->registers);
    SetShaderIdentity(&present_vs->common, &present_vs->registers);
    SetShaderIdentity(&present_ps->common, &present_ps->registers);
    SetShaderIdentity(&clear_ps->common, &clear_ps->registers);

    GnmFetchShaderCreateInfo fi = { .regs           = &game_vs->registers,
                                    .vtxinputs      = gnmVsShaderInputSemanticTable(game_vs),
                                    .numvtxinputs   = game_vs->numinputsemantics,
                                    .inputusages    = gnmVsShaderInputUsageSlotTable(game_vs),
                                    .numinputusages = game_vs->common.numinputusageslots };
    uint32_t fetch_size      = 0;
    GnmFetchShaderResults fr = { 0 };

    if (gnmFetchShaderCalcSize(&fetch_size, &fi) != GNM_ERROR_OK)
        Sys_Error("GNM game fetch shader size failed");

    game_fetch = memalloc_alloc(&permanent, fetch_size, GNM_ALIGNMENT_FETCHSHADER_BYTES);

    if (!game_fetch || gnmCreateFetchShader(game_fetch, fetch_size, &fi, &fr) != GNM_ERROR_OK)
        Sys_Error("GNM game fetch shader creation failed");

    gnmVsRegsSetFetchShaderModifier(&game_vs->registers, &fr);
    command_memory = memalloc_alloc(&permanent, COMMAND_BYTES, 256);
    transient      = memalloc_alloc(&permanent, TRANSIENT_BYTES, 256);
    completion     = memalloc_alloc(&permanent, 8, 8);

    if (!command_memory || !transient || !completion)
        Sys_Error("GNM command allocation failed");

    *completion = 0;
    state       = (render_state_t){ .depth_write = 1,
                                    .src         = GNMR_SRC_ALPHA,
                                    .dst         = GNMR_ONE_MINUS_SRC_ALPHA,
                                    .depth_func  = GNMR_LEQUAL,
                                    .face        = GNMR_FRONT,
                                    .far_value   = 1,
                                    .w = 1920,
                                    .h = 1080 };
    initialized = 1;
    ResetCommands();
    textures[0].used = 1;
    uint32_t white = 0xffffffffu;

    GNMR_Upload(0, HYE_PIXELS_RGBA8, 1, 1, 0, 0, 1, &white);
    GNMR_TextureFilter(GNMR_LINEAR, GNMR_LINEAR);

    return 1;
} /* PS4_GraphicsInit */

int
PS4_GraphicsSwap(int vsync)
{
    GNMR_BeginFrame();

    if (command.endptr - command.cmdptr < 4096 || transient_used + 2048 > TRANSIENT_BYTES)
        SubmitCommands();

    gnmDrawCmdWaitGraphicsWrite(&command, GNM_ACQUIRE_TARGET_CB0 | GNM_ACQUIRE_TARGET_DB);
    render_state_t saved = state;

    state.blend      = state.depth = state.cull = state.offset = 0;
    state.x          = state.y = 0;
    state.w          = 1920;
    state.h          = 1080;
    state.near_value = 0;
    state.far_value  = 1;
    ApplyState(&output[output_index], 0);
    ps_table_t * table = Allocate(sizeof(*table), 256);

    memset(table, 0, sizeof(*table));
    table->image = scene_texture;
    table->sampler.xyminfilter = table->sampler.xymagfilter = GNM_FILTER_BILINEAR;
    GnmBuffer * dummy = Allocate(sizeof(*dummy), 256);

    memset(dummy, 0, sizeof(*dummy));
    BindProgram(present_vs, present_ps, NULL, NULL, dummy, table);
    gnmDrawCmdSetPrimitiveType(&command, GNM_PT_TRILIST);
    gnmDrawCmdDrawIndexAuto(&command, 3);
    has_commands = 1;
    state        = saved;
    SubmitCommands();
    int result = displayctx_flip(&display, output_index);

    output_index ^= 1;
    active        = 0;

    if (++frames == 1) {
        sceSystemServiceHideSplashScreen();
    }

    (void) vsync;

    return result;
} /* PS4_GraphicsSwap */

void
PS4_GraphicsShutdown(void)
{
    if (!initialized)
        return;

    GNMR_Finish();

    for (unsigned i = 0; i < MAX_TEXTURES; ++i) {
        free(textures[i].staging);
        textures[i].staging = NULL;
        sceGnmDirectMemoryRelease(&textures[i].memory);
    }

    displayctx_destroy(&display);
    memalloc_destroy(&permanent);
    initialized = 0;
    active      = 0;
}

#include <math.h>
#include <string.h>

// Column-major matrices, with the same post-multiplication order as Hyena's GL backend.
static void
PS4_MatrixIdentity(float * m)
{
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1;
}

static void
PS4_MatrixMultiply(float * out, const float * a, const float * b)
{
    float result[16];
    int row, column, k;

    for (column = 0; column < 4; ++column)
        for (row = 0; row < 4; ++row) {
            float value = 0;

            for (k = 0; k < 4; ++k)
                value += a[k * 4 + row] * b[column * 4 + k];

            result[column * 4 + row] = value;
        }

    memcpy(out, result, sizeof(result));
}

static void
PS4_MatrixRotation(float * m, float degrees, float x, float y, float z)
{
    float length = sqrtf(x * x + y * y + z * z);
    float c      = cosf(degrees * 0.017453292519943295f);
    float s = sinf(degrees * 0.017453292519943295f), t = 1 - c;

    PS4_MatrixIdentity(m);

    if (length == 0)
        return;

    x    /= length;
    y    /= length;
    z    /= length;
    m[0]  = t * x * x + c;
    m[4]  = t * x * y - s * z;
    m[8]  = t * x * z + s * y;
    m[1]  = t * x * y + s * z;
    m[5]  = t * y * y + c;
    m[9]  = t * y * z - s * x;
    m[2]  = t * x * z - s * y;
    m[6]  = t * y * z + s * x;
    m[10] = t * z * z + c;
}

#include "../../../nzportable_def.h"
#include "../../../render/r_texture.h"
#include "../../../render/r_pixels.h"

#define MATRIX_DEPTH 32
static float matrices[2][MATRIX_DEPTH][16];
static unsigned matrix_depth[2];
static int matrix_mode;
static float current_color[4]     = { 1, 1, 1, 1 };
static float texture_transform[4] = { 1, 1, 0, 0 };
static float fog_color[4]         = { 0.5f, 0.5f, 0.5f, 1 };
static float fog_start, fog_end = 1, alpha_threshold = 0.666f;
static float hyena_blend_alpha = 1;
static qboolean textured = true, alpha_test = true, fog_enabled, modulate = true;
static qboolean flat_shading = true;
static int hyena_vertex_mode;
static vec3_t hyena_translation, hyena_scale;
static gnmr_vertex_t * stream;
static size_t stream_capacity;

void
PS4_HyenaInit(void)
{
    PS4_MatrixIdentity(matrices[0][0]);
    PS4_MatrixIdentity(matrices[1][0]);
}

void
PS4_HyenaShutdown(void)
{
    free(stream);
    stream = NULL;
    stream_capacity = 0;
}

static float *
CurrentMatrix(void)
{
    return matrices[matrix_mode][matrix_depth[matrix_mode]];
}

void
Hyena_MatrixMode(int mode)
{
    matrix_mode = mode == HYE_PROJECTION ? 1 : 0;
}

void
Hyena_LoadIdentity(void)
{
    PS4_MatrixIdentity(CurrentMatrix());
}

void
Hyena_LoadMatrix(const float * m)
{
    memcpy(CurrentMatrix(), m, 16 * sizeof(float));
}

void
Hyena_GetMatrix(float * m)
{
    memcpy(m, CurrentMatrix(), 16 * sizeof(float));
}

void
Hyena_PushMatrix(void)
{
    unsigned depth = matrix_depth[matrix_mode];

    if (depth + 1 >= MATRIX_DEPTH)
        Sys_Error("Hyena: matrix stack overflow");

    memcpy(matrices[matrix_mode][depth + 1], CurrentMatrix(), 16 * sizeof(float));
    ++matrix_depth[matrix_mode];
}

void
Hyena_PopMatrix(void)
{
    if (!matrix_depth[matrix_mode])
        Sys_Error("Hyena: matrix stack underflow");

    --matrix_depth[matrix_mode];
}

void
Hyena_MatrixTranslate(float x, float y, float z)
{
    float m[16];

    PS4_MatrixIdentity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
    PS4_MatrixMultiply(CurrentMatrix(), CurrentMatrix(), m);
}

void
Hyena_MatrixScale(float x, float y, float z)
{
    float m[16];

    PS4_MatrixIdentity(m);
    m[0]  = x;
    m[5]  = y;
    m[10] = z;
    PS4_MatrixMultiply(CurrentMatrix(), CurrentMatrix(), m);
}

void
Hyena_MatrixRotate(float degrees, float x, float y, float z)
{
    float m[16];

    PS4_MatrixRotation(m, degrees, x, y, z);
    PS4_MatrixMultiply(CurrentMatrix(), CurrentMatrix(), m);
}

void
Hyena_Frustum(float l, float r, float b, float t, float n, float f)
{
    float m[16] = { 0 };

    if (r == l || t == b || f == n || n <= 0 || f <= 0)
        Sys_Error("Hyena: invalid frustum");

    m[0]  = 2 * n / (r - l);
    m[5]  = 2 * n / (t - b);
    m[8]  = (r + l) / (r - l);
    m[9]  = (t + b) / (t - b);
    m[10] = -(f + n) / (f - n);
    m[11] = -1;
    m[14] = -2 * f * n / (f - n);
    PS4_MatrixMultiply(CurrentMatrix(), CurrentMatrix(), m);
}

void
Hyena_UpdateMatrices(void)
{
}

void
Hyena_SetColor(float r, float g, float b, float a)
{
    current_color[0] = r;
    current_color[1] = g;
    current_color[2] = b;
    current_color[3] = a;
}

void
Hyena_SetCurrentColor(float r, float g, float b, float a)
{
    Hyena_SetColor(r, g, b, a);
}

void
Hyena_SetTextureMode(int mode)
{
    modulate = mode == HYE_MODULATE;
}

void
Hyena_SetShadeMode(int mode)
{
    flat_shading = mode == HYE_FLAT;
}

void
Hyena_EnableCapability(int cap)
{
    switch (cap) {
        case HYE_TEXTURE_2D:
            textured = true;
            break;
        case HYE_ALPHA_TEST:
            alpha_test = true;
            break;
        case HYE_BLEND:
            GNMR_Enable(GNMR_BLEND);
            break;
        case HYE_DEPTH_TEST:
            GNMR_Enable(GNMR_DEPTH_TEST);
            break;
        case HYE_CULL_FACE:
            GNMR_Enable(GNMR_CULL_FACE);
            break;
        default:
            Sys_Error("Hyena: unknown capability %d", cap);
    }
}

void
Hyena_DisableCapability(int cap)
{
    switch (cap) {
        case HYE_TEXTURE_2D:
            textured = false;
            break;
        case HYE_ALPHA_TEST:
            alpha_test = false;
            break;
        case HYE_BLEND:
            GNMR_Disable(GNMR_BLEND);
            break;
        case HYE_DEPTH_TEST:
            GNMR_Disable(GNMR_DEPTH_TEST);
            break;
        case HYE_CULL_FACE:
            GNMR_Disable(GNMR_CULL_FACE);
            break;
        default:
            Sys_Error("Hyena: unknown capability %d", cap);
    }
}

static unsigned
Blend(int blend)
{
    switch (blend) {
        case HYE_ZERO:
            return GNMR_ZERO;

        case HYE_ONE:
            return GNMR_ONE;

        case HYE_SRC_ALPHA:
            return GNMR_SRC_ALPHA;

        case HYE_ONE_MINUS_SRC_ALPHA:
            return GNMR_ONE_MINUS_SRC_ALPHA;

        case HYE_DST_COLOR:
            return GNMR_DST_COLOR;

        case HYE_SRC_COLOR:
            return GNMR_SRC_COLOR;

        case HYE_ONE_MINUS_SRC_COLOR:
            return GNMR_ONE_MINUS_SRC_COLOR;

        default:
            Sys_Error("Hyena: invalid blend %d", blend);
    }
}

void
Hyena_SetBlendFunction(int src, int dst)
{
    hyena_blend_alpha = 1;
    GNMR_BlendFunc(Blend(src), Blend(dst));
}

void
Hyena_BlendConstant(float alpha)
{
    hyena_blend_alpha = alpha;
    GNMR_BlendFunc(GNMR_SRC_ALPHA, GNMR_ONE_MINUS_SRC_ALPHA);
}

void
Hyena_DepthMask(qboolean enabled)
{
    GNMR_DepthMask(enabled);
}

void
Hyena_DepthBounds(float n, float f)
{
    gldepthmin = n;
    gldepthmax = f;
}

void
Hyena_SetDepthRange(float n, float f)
{
    float range = gldepthmax - gldepthmin;

    GNMR_DepthRange(gldepthmin + n * range, gldepthmin + f * range);
}

void
Hyena_ResetDepthRange(void)
{
    Hyena_SetDepthRange(0.3f, 1);
}

void
Hyena_DepthFunction(int fn)
{
    GNMR_DepthFunc(fn == HYE_EQUAL ? GNMR_EQUAL :
      fn == HYE_GEQUAL || gldepthmin > gldepthmax ? GNMR_GEQUAL :
      GNMR_LEQUAL);
}

void
Hyena_SetDepthOffset(float offset)
{
    if (offset) {
        GNMR_Enable(GNMR_POLYGON_OFFSET_FILL);
        GNMR_PolygonOffset(offset, offset);
    } else
        GNMR_Disable(GNMR_POLYGON_OFFSET_FILL);
}

void
Hyena_AlphaFunction(int fn, float value)
{
    (void) fn;
    alpha_threshold = value;
}

void
Hyena_FogInit(void)
{
}

void
Hyena_FogEnable(void)
{
    fog_enabled = true;
}

void
Hyena_FogDisable(void)
{
    fog_enabled = false;
}

void
Hyena_FogSet(bool world, float start, float end, float r, float g, float b, float a)
{
    fog_start    = start;
    fog_end      = end;
    fog_color[0] = world ? 0.5f : r * 0.01f;
    fog_color[1] = world ? 0.5f : g * 0.01f;
    fog_color[2] = world ? 0.5f : b * 0.01f;
    fog_color[3] = a;
}

void
Hyena_Set2D(void)
{
    float m[16];

    PS4_MatrixIdentity(m);
    m[0]  = 2.0f / vid.width;
    m[5]  = -2.0f / vid.height;
    m[10] = -2.0f / 199998;
    m[12] = -1;
    m[13] = 1;
    Hyena_MatrixMode(HYE_PROJECTION);
    Hyena_LoadMatrix(m);
    Hyena_MatrixMode(HYE_MODELVIEW);
    Hyena_LoadIdentity();
    GNMR_Viewport(glx, gly, glwidth, glheight);
    GNMR_Disable(GNMR_DEPTH_TEST);
    GNMR_Disable(GNMR_CULL_FACE);
}

void
Hyena_LoadViewMatrix(const float * m)
{
    Hyena_LoadMatrix(m);
}

void
Hyena_Viewport(int x, int y, int w, int h)
{
    GNMR_Viewport(x, y, w, h);
}

void
Hyena_ClipFrustum(float fovy, float aspect, float near_value)
{
    (void) fovy;
    (void) aspect;
    (void) near_value;
}

void
Hyena_CullFace(qboolean back)
{
    GNMR_CullFace(back ? GNMR_BACK : GNMR_FRONT);
}

void
Hyena_ClearColor(float r, float g, float b, float a)
{
    GNMR_ClearColor(r, g, b, a);
}

void
Hyena_Clear(qboolean color, qboolean depth, qboolean stencil)
{
    GNMR_Clear((color ? GNMR_COLOR_BUFFER_BIT : 0) | (depth ? GNMR_DEPTH_BUFFER_BIT : 0)
      | (stencil ? GNMR_STENCIL_BUFFER_BIT : 0));
}

void
Hyena_PerspectiveCorrection(qboolean enabled)
{
    (void) enabled;
}

void
Hyena_SelectDrawBuffer(qboolean front)
{
    (void) front;
}

void
Hyena_SelectReadBuffer(qboolean front)
{
    (void) front;
}

void
Hyena_Finish(void)
{
    GNMR_Finish();
}

qboolean
Hyena_ReadPixels(int x, int y, int w, int h, byte * rgba)
{
    return GNMR_ReadPixels(x, y, w, h, GNMR_RGBA, GNMR_UNSIGNED_BYTE, rgba);
}

void
Hyena_TextureFilter(int filter)
{
    int mode = filter == HYE_FILTER_LINEAR ? GNMR_LINEAR : GNMR_NEAREST;

    GNMR_TextureFilter(mode, mode);
}

void
Hyena_BeginArrays(int mask)
{
    (void) mask;
}

void
Hyena_EndArrays(void)
{
}

static unsigned hyena_texture_objects[HYENA_MAX_TEXTURES];
static unsigned current_texture_object;

void
Hyena_InvalidateTextureBinding(void)
{
    current_texture_object = (unsigned) -1;
}

static int
Hyena_NativeFilter(int filter)
{
    static const int filters[] = { GNMR_NEAREST,
                                   GNMR_LINEAR,
                                   GNMR_NEAREST_MIPMAP_NEAREST,
                                   GNMR_LINEAR_MIPMAP_NEAREST,
                                   GNMR_NEAREST_MIPMAP_LINEAR,
                                   GNMR_LINEAR_MIPMAP_LINEAR };

    return filters[filter];
}

void
Hyena_BackendTextureFilter(int texture, int min_filter, int mag_filter)
{
    unsigned object = hyena_texture_objects[texture];

    if (current_texture_object != object) {
        GNMR_BindTexture(object);
        current_texture_object = object;
    }

    GNMR_TextureFilter(Hyena_NativeFilter(min_filter), Hyena_NativeFilter(mag_filter));
}

void
Hyena_BackendBindTexture(int texture, int min_filter, int mag_filter)
{
    if (current_texture_object == hyena_texture_objects[texture])
        return;

    Hyena_BackendTextureFilter(texture, min_filter, mag_filter);
}

void
Hyena_BackendDestroyTexture(int texture)
{
    GNMR_DestroyTexture(hyena_texture_objects[texture]);
    hyena_texture_objects[texture] = 0;
    current_texture_object         = (unsigned) -1;
}

void
Hyena_BackendInitTextureResources(void)
{
    Hyena_ResetTextureRegistry();
    Hyena_InitTextureSettings("1024", true, HYENA_MAX_TEXTURES);
}

void
Hyena_BackendUploadLevel(int texture, int level, int storage, int width, int height, int x,
  int y, qboolean replace, const void * pixels)
{
    GNMR_BindTexture(hyena_texture_objects[texture]);
    current_texture_object = hyena_texture_objects[texture];
    GNMR_Upload(level, storage, width, height, x, y, replace, pixels);
}

void
Hyena_BackendUploadTexture(int texture, const hyena_texture_desc_t * desc)
{
    if (!hyena_textures[texture].used)
        hyena_texture_objects[texture] = GNMR_CreateTexture();

    GNMR_BindTexture(hyena_texture_objects[texture]);
    GNMR_Mipmaps(desc->mip_levels > 0 && !desc->lightmap);
    GNMR_DynamicTexture(desc->lightmap);
    Hyena_PrepareTextureUpload(texture, desc, HYE_PIXELS_RGBA5551, Hyena_BackendUploadLevel);
}

struct hyena_buffer_s {
    int      target;
    size_t   size;
    void *   data;
    qboolean owned;
};

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
    if (buffer->owned)
        free(buffer->data);

    // Static storage owns its bytes; streamed indices keep the caller's frame allocation.
    buffer->owned = buffer->target == HYE_ARRAY_BUFFER;

    if (buffer->owned) {
        buffer->data = size ? malloc(size) : NULL;

        if (size && !buffer->data)
            Sys_Error("Hyena_UpdateBuffer: out of memory");

        if (size && data)
            memcpy(buffer->data, data, size);
    } else {
        buffer->data = (void *) data;
    }

    buffer->size = size;
}

void
Hyena_DestroyBuffer(hyena_buffer_t * buffer)
{
    if (!buffer)
        return;

    if (buffer->owned)
        free(buffer->data);

    free(buffer);
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
    texture_transform[0] = uscale;
    texture_transform[1] = vscale;
    texture_transform[2] = uoffset;
    texture_transform[3] = voffset;
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
    return buffer->data;
}

void
Hyena_UnmapBuffer(hyena_buffer_t * buffer)
{
    (void) buffer;
}

qboolean
Hyena_DrawMorphMesh(const hyena_morph_mesh_t * mesh, const byte * first, const byte * second,
  float blend)
{
    (void) mesh;
    (void) first;
    (void) second;
    (void) blend;

    return false;
}

void
Hyena_AddLightTime(double seconds)
{
    (void) seconds;
}

void
Hyena_InitTextureStorage(void)
{
}

size_t
Hyena_TextureBudget(void)
{
    return 0;
}

int
Hyena_PreferredPaletteBits(void)
{
    return 8;
}

qboolean
Hyena_MorphMeshesSupported(void)
{
    return false;
}

void *
Hyena_AllocateTexturePixels(size_t bytes)
{
    return malloc(bytes);
}

void
Hyena_FreeTexturePixels(void * pixels)
{
    free(pixels);
}

int
Hyena_PreferredLightmapSize(void)
{
    return 256;
}

hyena_texture_format_t
Hyena_PreferredLightmapFormat(void)
{
    return HYE_TEXTURE_RGBA8;
}

unsigned *
Hyena_LightmapScratch(int count)
{
    (void) count;

    return NULL;
}

qboolean
Hyena_AccumulateLightmap(unsigned * dest, const byte * source, int count, unsigned scale,
  qboolean add)
{
    (void) dest;
    (void) source;
    (void) count;
    (void) scale;
    (void) add;

    return false;
}

void
Hyena_PrepareTextureUpdate(int texture)
{
    (void) texture;
}

void
Hyena_ClipArrays(const hyena_arrays_t * source, hyena_arrays_t * clipped)
{
    *clipped = *source;
}

void
Hyena_BeginModelClipping(void)
{
}

void
Hyena_EndModelClipping(void)
{
}

void
Hyena_ClearMorphMeshes(void)
{
}

static const void *
ArrayPointer(const hyena_array_t * array)
{
    return array->buffer ? (const byte *) array->buffer->data + (size_t) array->data : array->data;
}

static float
ArrayValue(const hyena_array_t * array, int index, int component, int components,
  qboolean normalize)
{
    int size       = array->type == HYE_SHORT ? 2 : array->type == HYE_UBYTE ? 1 : 4;
    const byte * p = (const byte *) ArrayPointer(array)
      + (size_t) index * (array->stride ? array->stride : size * components)
      + component * size;

    if (array->type == HYE_UBYTE)
        return normalize ? *p / 255.0f : *p;

    if (array->type == HYE_SHORT) {
        short v;

        memcpy(&v, p, sizeof(v));

        return normalize ? fmaxf(-1, v / 32767.0f) : v;
    }

    {
        float v;

        memcpy(&v, p, sizeof(v));

        return v;
    }
}

static void
ReadVertex(gnmr_vertex_t * v, const hyena_arrays_t * arrays, int index)
{
    int i;

    if (index < 0 || (arrays->count > 0 && index >= arrays->count))
        Sys_Error("Hyena: vertex index out of range");

    for (i = 0; i < 3; ++i)
        v->position[i] =
          ArrayValue(&arrays->position, index, i, 3, false) * (arrays->scale ? arrays->scale : 1);

    for (i = 0; i < 2; ++i)
        v->uv[i] = (arrays->uv.data || arrays->uv.buffer) ?
          ArrayValue(&arrays->uv, index, i, 2, false) :
          0;

    for (i = 0; i < 4; ++i)
        v->color[i] = (arrays->color.data || arrays->color.buffer) ?
          ArrayValue(&arrays->color, index, i, 4, true) :
          current_color[i];

    v->color[3] *= hyena_blend_alpha;
}

static void
Submit(int mode, const hyena_arrays_t * arrays, int count, const unsigned short * indices)
{
    size_t output_count, i;
    int triangles = 0;
    qboolean expand;
    unsigned primitive;
    float mvp[16];

    if (count <= 0)
        return;

    expand = mode == HYE_QUADS || (flat_shading && mode != HYE_LINE_STRIP);

    if (expand) {
        triangles = mode == HYE_QUADS ? (count / 4) * 2 :
          mode == HYE_TRIANGLES ? count / 3 :
          count >= 3 ? count - 2 :
          0;
        output_count = (size_t) triangles * 3;
        primitive    = GNMR_TRIANGLES;
    } else {
        output_count = count;
        primitive    = mode == HYE_TRIANGLE_FAN ? GNMR_TRIANGLE_FAN :
          mode == HYE_TRIANGLE_STRIP ? GNMR_TRIANGLE_STRIP :
          mode == HYE_LINE_STRIP ? GNMR_LINE_STRIP :
          GNMR_TRIANGLES;
    }

    if (!output_count)
        return;

    if (output_count > stream_capacity) {
        void * memory = realloc(stream, output_count * sizeof(*stream));

        if (!memory)
            Sys_Error("Hyena: GNM stream allocation failed");

        stream = memory;
        stream_capacity = output_count;
    }

    if (expand) {
        for (i = 0; i < (size_t) triangles; ++i) {
            int a, b, c, provoking, j, verts[3];

            if (mode == HYE_QUADS) {
                int base = (i / 2) * 4;

                a         = base;
                b         = base + (i % 2 ? 2 : 1);
                c         = base + (i % 2 ? 3 : 2);
                provoking = base + 3;
            } else if (mode == HYE_TRIANGLES) {
                a         = i * 3;
                b         = a + 1;
                c         = a + 2;
                provoking = c;
            } else if (mode == HYE_TRIANGLE_STRIP) {
                a         = i + (i % 2);
                b         = i + 1 - (i % 2);
                c         = i + 2;
                provoking = c;
            } else {
                a         = 0;
                b         = i + 1;
                c         = i + 2;
                provoking = c;
            }

            verts[0] = a;
            verts[1] = b;
            verts[2] = c;

            for (j = 0; j < 3; ++j)
                ReadVertex(&stream[i * 3 + j], arrays, indices ? indices[verts[j]] : verts[j]);

            if (flat_shading) {
                gnmr_vertex_t last;

                ReadVertex(&last, arrays, indices ? indices[provoking] : provoking);

                for (j = 0; j < 3; ++j)
                    memcpy(stream[i * 3 + j].color, last.color, sizeof(last.color));
            }
        }
    } else
        for (i = 0; i < output_count; ++i)
            ReadVertex(&stream[i], arrays, indices ? indices[i] : i);

    PS4_MatrixMultiply(mvp, matrices[1][matrix_depth[1]], matrices[0][matrix_depth[0]]);
    gnmr_uniforms_t uniforms = { 0 };

    memcpy(uniforms.mvp, mvp, sizeof(mvp));
    memcpy(uniforms.modelview, matrices[0][matrix_depth[0]], sizeof(mvp));
    memcpy(uniforms.uv, texture_transform, sizeof(texture_transform));
    memcpy(uniforms.fog_color, fog_color, sizeof(fog_color));
    uniforms.params[0] = fog_start;
    uniforms.params[1] = fog_end;
    uniforms.params[2] = alpha_test ? alpha_threshold : -1;
    uniforms.params[3] = textured;
    uniforms.flags[0]  = modulate;
    uniforms.flags[1]  = fog_enabled;
    GNMR_Draw(primitive, stream, output_count, &uniforms);
} /* Submit */

void
Hyena_DrawArrays(int mode, const hyena_arrays_t * arrays, int count)
{
    Submit(mode, arrays, count, NULL);
}

void
Hyena_DrawElements(int mode, const hyena_arrays_t * arrays, int count, hyena_buffer_t * indices,
  size_t offset)
{
    if (count < 0 || !indices || offset > indices->size ||
      (size_t) count > (indices->size - offset) / sizeof(unsigned short))
        Sys_Error("Hyena: index buffer out of range");

    Submit(mode, arrays, count, (const unsigned short *) ((const byte *) indices->data + offset));
}

void
Hyena_BeginVertices(int mode)
{
    hyena_vertex_mode = mode;
    VectorClear(hyena_translation);
    hyena_scale[0] = hyena_scale[1] = hyena_scale[2] = 1;
}

void
Hyena_Translate(float x, float y, float z)
{
    hyena_translation[0] = x;
    hyena_translation[1] = y;
    hyena_translation[2] = z;
}

void
Hyena_Scale(float x, float y, float z)
{
    hyena_scale[0] = x;
    hyena_scale[1] = y;
    hyena_scale[2] = z;
}

void
Hyena_RotateXYZ(float x, float y, float z)
{
    Hyena_MatrixRotate(x, 1, 0, 0);
    Hyena_MatrixRotate(y, 0, 1, 0);
    Hyena_MatrixRotate(z, 0, 0, 1);
}

void
Hyena_RotateZYX(float z, float y, float x)
{
    Hyena_MatrixRotate(z, 0, 0, 1);
    Hyena_MatrixRotate(y, 0, 1, 0);
    Hyena_MatrixRotate(x, 1, 0, 0);
}

void
Hyena_FlushMatrices(void)
{
}

vertex_t *
Hyena_AllocateMemoryForVertices(int count)
{
    vertex_t * v = count > 0 ? malloc((size_t) count * sizeof(*v)) : NULL;

    if (count > 0 && !v)
        Sys_Error("Hyena: vertex allocation failed");

    return v;
}

void
Hyena_2DTextureCoord(vertex_t * v, float u, float t)
{
    v->uv.u = u;
    v->uv.v = t;
}

void
Hyena_VertexXYZ(vertex_t * v, float x, float y, float z)
{
    v->xyz.x = x;
    v->xyz.y = y;
    v->xyz.z = z;
}

void
Hyena_DrawVertices(vertex_t * vertices, int count, int texture_precision, int vertex_precision)
{
    hyena_arrays_t arrays = { 0 };
    qboolean saved        = textured;
    int i;

    (void) vertex_precision;

    if (count <= 0) {
        free(vertices);

        return;
    }

    for (i = 0; i < count; ++i) {
        vertices[i].xyz.x = vertices[i].xyz.x * hyena_scale[0] + hyena_translation[0];
        vertices[i].xyz.y = vertices[i].xyz.y * hyena_scale[1] + hyena_translation[1];
        vertices[i].xyz.z = vertices[i].xyz.z * hyena_scale[2] + hyena_translation[2];
    }

    arrays.position.data   = &vertices[0].xyz;
    arrays.position.stride = sizeof(*vertices);
    arrays.uv.data         = &vertices[0].uv;
    arrays.uv.stride       = sizeof(*vertices);
    arrays.count = count;
    textured     = texture_precision != HYE_TEXTURE_NOTEXTURE;
    Submit(hyena_vertex_mode, &arrays, count, NULL);
    textured = saved;
    free(vertices);
}

void
Hyena_EndVertices(void)
{
}

void
Hyena_DrawIndexedTriangles(const hyena_colored_vertex_t * vertices, int vertex_count,
  const unsigned short * indices, int index_count, qboolean texture)
{
    hyena_arrays_t arrays = { 0 };
    qboolean saved        = textured;

    if (!index_count)
        return;

    arrays.position.data   = vertices[0].xyz;
    arrays.position.stride = sizeof(*vertices);
    arrays.uv.data         = vertices[0].uv;
    arrays.uv.stride       = sizeof(*vertices);
    arrays.color.data      = vertices[0].color;
    arrays.color.type      = HYE_UBYTE;
    arrays.color.stride    = sizeof(*vertices);
    arrays.count = vertex_count;
    textured     = texture;
    Submit(HYE_TRIANGLES, &arrays, index_count, indices);
    textured = saved;
    Hyena_SetColor(1, 1, 1, 1);
}

void
Hyena_Draw2DQuads(const hyena_2d_quad_t * quads, int count, int texture, unsigned flags)
{
    static const int corners[] = { 0, 1, 2, 0, 2, 3 };
    hyena_colored_vertex_t * vertices;
    hyena_arrays_t arrays = { 0 };
    int i;
    qboolean saved_flat = flat_shading;

    if (count <= 0)
        return;

    vertices = malloc((size_t) count * 6 * sizeof(*vertices));

    if (!vertices)
        Sys_Error("Hyena: 2D allocation failed");

    for (i = 0; i < count * 6; ++i) {
        const hyena_2d_quad_t * q = &quads[i / 6];
        int c = corners[i % 6];

        vertices[i].xyz[0]   = c == 0 || c == 3 ? q->x0 : q->x1;
        vertices[i].xyz[1]   = c < 2 ? q->y0 : q->y1;
        vertices[i].xyz[2]   = 0;
        vertices[i].uv[0]    = c == 0 || c == 3 ? q->u0 : q->u1;
        vertices[i].uv[1]    = c < 2 ? q->v0 : q->v1;
        vertices[i].color[0] = q->r;
        vertices[i].color[1] = q->g;
        vertices[i].color[2] = q->b;
        vertices[i].color[3] = q->a;
    }

    textured     = (flags & HYE_2D_TEXTURED) != 0;
    alpha_test   = false;
    modulate     = true;
    flat_shading = false;

    if (textured) {
        Hyena_BindTexture(texture);
        Hyena_TextureFilter(flags & HYE_2D_LINEAR ? HYE_FILTER_LINEAR : HYE_FILTER_NEAREST);
    }

    if (flags & HYE_2D_BLEND) {
        GNMR_Enable(GNMR_BLEND);
        GNMR_BlendFunc(GNMR_SRC_ALPHA, GNMR_ONE_MINUS_SRC_ALPHA);
    } else
        GNMR_Disable(GNMR_BLEND);

    arrays.position.data   = vertices[0].xyz;
    arrays.position.stride = sizeof(*vertices);
    arrays.uv.data         = vertices[0].uv;
    arrays.uv.stride       = sizeof(*vertices);
    arrays.color.data      = vertices[0].color;
    arrays.color.stride    = sizeof(*vertices);
    arrays.color.type      = HYE_UBYTE;
    arrays.count = count * 6;
    Submit(HYE_TRIANGLES, &arrays, count * 6, NULL);
    free(vertices);
    flat_shading = saved_flat;
    textured     = true;
    alpha_test   = true;
    Hyena_SetColor(1, 1, 1, 1);
    GNMR_Disable(GNMR_BLEND);
} /* Hyena_Draw2DQuads */
