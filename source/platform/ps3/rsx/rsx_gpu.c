/*
 * Copyright (C) 1996-1997 Id Software, Inc.
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

#include "../../../nzportable_def.h"
#include "rsx_main.h"
#include "rsx_layout.h"
#include "rsx_particles.h"
#include "../../../render/r_pixels.h"
#include "../../../render/r_texture.h"
#include "../ps3_local.h"
#include <malloc.h>
#include <rsx/rsx.h>
#include <sysutil/video.h>
#include <sys/systime.h>

#define WIDTH               1920
#define HEIGHT              1080
#define EFFECT_WIDTH        (WIDTH / 2)
#define EFFECT_HEIGHT       (HEIGHT / 2)
#define STREAM_BYTES        (8u * 1024 * 1024)
#define MAX_TEXTURES        4096
#define STATIC_GPU_BUDGET   (32u * 1024 * 1024)
#define LIGHTMAP_GPU_BUDGET (16u * 1024 * 1024)
#define LABEL               128
#define REMAP                                                                                      \
        GCM_TEXTURE_REMAP_MODE(                                                                        \
            GCM_TEXTURE_REMAP_ORDER_XYXY, GCM_TEXTURE_REMAP_COLOR_A, GCM_TEXTURE_REMAP_COLOR_R,        \
            GCM_TEXTURE_REMAP_COLOR_G, GCM_TEXTURE_REMAP_COLOR_B, GCM_TEXTURE_REMAP_TYPE_REMAP,        \
            GCM_TEXTURE_REMAP_TYPE_REMAP, GCM_TEXTURE_REMAP_TYPE_REMAP, GCM_TEXTURE_REMAP_TYPE_REMAP)

typedef struct {
    void *     pixels;
    gcmTexture image;
    int        used, min_filter, mag_filter, dynamic, mipmaps;
    size_t     bytes;
    u32        revision, snapshot_revision, snapshot_offset;
    uint64_t   snapshot_generation;
    void *     versions[2];
    u32        version_offset[2], version_revision[2];
    uint64_t   version_frame[2];
    size_t     version_bytes;
} ps3_texture_t;

typedef struct {
    rsxVertexProgram *   vp;
    rsxFragmentProgram * fp;
    void *               ucode, * fragment;
    u32                  offset;
    int                  position, texcoord, color;
} shader_t;

static gcmContextData * context;

static void * stream_host;
static u32 stream_offset;

static void * host, * scene, * depth, * output[2], * vertices[2];
static u32 scene_offset, depth_offset, depth_pitch = WIDTH * 4, output_offset[2], vertex_offset[2];
static int tiled_depth, zcull, color_tiling;
static unsigned tiled_colors;
static u32 color_pitch = WIDTH * 4;
static size_t color_bytes;

static u32 serial, completed[2];
static unsigned frame, bound_texture, last_error;
static size_t used;
static uint64_t generation, frame_id;
static int initialized, active, flip_pending, exiting, read_front;

static ps3_texture_t textures[MAX_TEXTURES];
static shader_t game[8], present, present_copy, effect_depth_shader, effect_composite;

static void * effect_color, * effect_depth;
static u32 effect_color_offset, effect_depth_offset, effect_pitch;
static int effect_available, effect_active, particle_scope, particle_type_half;
static unsigned effect_tiles, effect_blend_dst;
static int effect_bounds[4], effect_depth_bounds[4];

static unsigned effect_draws, effect_composites, effect_native_draws;
static uint64_t effect_depth_pixels;

static cvar_t ps3_halfres_particles    = { "ps3_halfres_particles", "1", true };
static cvar_t ps3_halfres_min_overdraw = { "ps3_halfres_min_overdraw", "4", true };

static int
BeginEffects(void);

static void
EnsureEffectsDepth(const int * bounds);

static void
EndEffects(void);

static shader_t * bound_shader;
static rsxProgramConst * constants[7];
static const char * constant_names[7] = { "mvp", "modelview", "uv", "fog_color",
                                          "params", "flags", "vertex_state" };
static int state_dirty = 1;
static unsigned constant_valid, attribute_generation;
static float constant_values[7][16];

static size_t stream_peak;
static unsigned draws, spills, snapshots, flushes, frame_samples;
static unsigned blend_draws, cutout_draws;
static unsigned indexed_draws, static_draws, texture_binds;
static size_t stream_bytes, index_bytes, static_bytes, lightmap_bytes;
static double submit_seconds, report_start;
static double flip_wait_seconds, scene_drain_seconds, present_drain_seconds, profile_start_seconds;

static cvar_t ps3_profile_sync = { "ps3_profile_sync", "0" };
static int sync_profile;
static double wait_seconds;

static cvar_t ps3_fxaa  = { "ps3_fxaa", "1", true };
static cvar_t ps3_zcull = { "ps3_zcull", "1", true };

static void
GPUStats(void)
{
    double now = Sys_FloatTime(), seconds = now - report_start;
    double frames = frame_samples ? frame_samples : 1;

    Con_SafePrintf("PS3: %.1f FPS; CPU submit %.2f, GPU timing unavailable, completion wait %.2f "
      "ms/frame\n",
      seconds > 0 ? frame_samples / seconds : 0, submit_seconds * 1000 / frames,
      wait_seconds * 1000 / frames);
    Con_SafePrintf("RSX: %.1f draws/frame (%.1f indexed, %.1f static), %.1f texture binds, %.1f "
      "flushes; %.1f KiB streamed/frame (%.1f indices), %u spills, %u snapshots, peak "
      "%u KiB; static %u KiB, lightmaps %u KiB; tiled depth %d, Zcull %d, color tiles "
      "%u, color pitch %u\n",
      draws / frames, indexed_draws / frames, static_draws / frames,
      texture_binds / frames, flushes / frames, stream_bytes / frames / 1024,
      index_bytes / frames / 1024, spills, snapshots, (unsigned) (stream_peak / 1024),
      (unsigned) (static_bytes / 1024), (unsigned) (lightmap_bytes / 1024), tiled_depth,
      zcull, tiled_colors, color_pitch);
    Con_SafePrintf("PS3 waits: flip %.2f, scene drain %.2f, FXAA/copy drain %.2f, profile start "
      "%.2f ms/frame; blended %.1f, cutout %.1f draws/frame; cl_maxfps %.0f, r_vsync "
      "%.0f, sync profiling %d\n",
      flip_wait_seconds * 1000 / frames, scene_drain_seconds * 1000 / frames,
      present_drain_seconds * 1000 / frames, profile_start_seconds * 1000 / frames,
      blend_draws / frames, cutout_draws / frames, Cvar_VariableValue("cl_maxfps"),
      Cvar_VariableValue("r_vsync"), sync_profile);
    Con_SafePrintf("PS3 particles: halfres %.0f, available %d, %.1f reduced draws/frame, %.1f "
      "composites/frame, %.1f K depth pixels/frame, %.1f sparse native draws/frame\n",
      ps3_halfres_particles.value, effect_available, effect_draws / frames,
      effect_composites / frames, effect_depth_pixels / frames / 1024,
      effect_native_draws / frames);

    effect_draws        = effect_composites = effect_native_draws = 0;
    effect_depth_pixels = 0;
    draws = spills = snapshots = flushes = frame_samples = indexed_draws = static_draws =
      texture_binds         = blend_draws = cutout_draws = 0;
    stream_bytes            = index_bytes = stream_peak = 0;
    submit_seconds          = wait_seconds = flip_wait_seconds = scene_drain_seconds =
      present_drain_seconds = profile_start_seconds = 0;
    report_start            = now;
}

static void
ProfileCommand(void)
{
    extern qboolean con_debuglog;
    extern char debuglogfile[1001];
    int mode = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 1;

    if (mode < 0 || mode > 2) {
        Con_Printf("Usage: ps3_profile [0|1|2]\n");
        return;
    }

    if (mode) {
        snprintf(debuglogfile, sizeof(debuglogfile), "%s/condebug.log", com_gamedir);
        con_debuglog = true;
    }

    Cvar_SetValue("r_profile", mode != 0);
    Cvar_SetValue("ps3_profile_sync", mode == 2);
    GPUStats();
    Con_SafePrintf("PS3 profiling mode %d; log %s; map %s; FXAA %.0f, Zcull %.0f\n", mode,
      mode ? debuglogfile : "unchanged", cl.worldmodel ? cl.worldmodel->name : "none",
      ps3_fxaa.value, ps3_zcull.value);

    if (mode == 2)
        Con_SafePrintf(
            "Synchronous drain measurements perturb FPS; use mode 1 for normal frame rate.\n");
}

static char error_text[192];
static float clear_depth = 1;
static u32 clear_color_value;
static int blend, depth_test, cull, depth_write = 1, polygon_offset;
static unsigned blend_src = GCM_SRC_ALPHA, blend_dst = GCM_ONE_MINUS_SRC_ALPHA;
static unsigned depth_func = GCM_LEQUAL, cull_face = GCM_CULL_FRONT;
static float depth_near, depth_far = 1, offset_factor, offset_units;
static int viewport[4] = { 0, 0, WIDTH, HEIGHT };

static void *
TryLocal(size_t size, u32 * offset)
{
    void * p = rsxMemalign(128, size);

    if (p && rsxAddressToOffset(p, offset) != 0) {
        rsxFree(p);
        return NULL;
    }

    return p;
}

static void *
Local(size_t size, u32 * offset)
{
    void * p = TryLocal(size, offset);

    if (!p)
        Sys_Error("PS3: RSX local allocation failed (%u bytes)", (unsigned) size);

    return p;
}

static void *
ColorSurface(unsigned tile, u32 * offset)
{
    void * p = rsxMemalign(GCM_TILE_ALIGN_SIZE, color_bytes);

    if (!p || rsxAddressToOffset(p, offset) != 0)
        Sys_Error("PS3: color surface allocation failed");

    if (color_tiling &&
      gcmSetTileInfo(tile, GCM_LOCATION_RSX, *offset, color_bytes, color_pitch,
      GCM_COMPMODE_DISABLED, 0, 0) == 0 &&
      gcmBindTile(tile) == 0)
        tiled_colors |= 1u << (tile - 1);

    return p;
}

static void
Wait(u32 value)
{
    if (!value)
        return;

    rsxFlushBuffer(context);
    double start = Sys_FloatTime(), deadline = start + 10;

    while ((s32) (*(volatile u32 *) gcmGetLabelAddress(LABEL) - value) < 0) {
        if (Sys_FloatTime() > deadline)
            Sys_Error("PS3: RSX completion timeout");

        sysUsleep(50);
    }

    wait_seconds += Sys_FloatTime() - start;
}

void
RSXR_Finish(void)
{
    if (!context || exiting)
        return;

    rsxSetWriteBackendLabel(context, LABEL, ++serial);
    Wait(serial);
}

static void
Surface(u32 color)
{
    gcmSurface surface = { 0 };
    unsigned i;

    surface.colorFormat = GCM_SURFACE_A8R8G8B8;
    surface.colorTarget = GCM_SURFACE_TARGET_0;

    for (i = 0; i < 4; ++i) {
        surface.colorLocation[i] = GCM_LOCATION_RSX;
        surface.colorPitch[i]    = 64;
    }

    surface.colorOffset[0] = color;
    surface.colorPitch[0]  = effect_active ? effect_pitch : color_pitch;
    surface.depthFormat    = GCM_SURFACE_ZETA_Z24S8;
    surface.depthLocation  = GCM_LOCATION_RSX;
    surface.depthOffset    = effect_active ? effect_depth_offset : depth_offset;
    surface.depthPitch     = effect_active ? effect_pitch : depth_pitch;
    surface.type      = GCM_SURFACE_TYPE_LINEAR;
    surface.antiAlias = GCM_SURFACE_CENTER_1;
    surface.width     = effect_active ? EFFECT_WIDTH : WIDTH;
    surface.height    = effect_active ? EFFECT_HEIGHT : HEIGHT;
    rsxSetSurface(context, &surface);
}

static gcmTexture
Image(u32 offset, unsigned w, unsigned h)
{
    gcmTexture t = { 0 };

    t.format    = GCM_TEXTURE_FORMAT_A8R8G8B8 | GCM_TEXTURE_FORMAT_LIN;
    t.mipmap    = 1;
    t.dimension = GCM_TEXTURE_DIMS_2D;
    t.remap     = REMAP;
    t.width     = w;
    t.height    = h;
    t.depth     = 1;
    t.location  = GCM_LOCATION_RSX;
    t.pitch     = w * 4;
    t.offset    = offset;
    return t;
}

static void
Texture(const gcmTexture * t, int min_filter, int mag_filter, int repeat)
{
    static gcmTexture cached;
    static int cached_min = -1, cached_mag = -1, cached_repeat = -1;

    if (cached_min == min_filter && cached_mag == mag_filter && cached_repeat == repeat &&
      !memcmp(&cached, t, sizeof(cached)))
        return;

    cached        = *t;
    cached_min    = min_filter;
    cached_mag    = mag_filter;
    cached_repeat = repeat;
    ++texture_binds;
    rsxLoadTexture(context, 0, t);
    rsxTextureControl(context, 0, GCM_TRUE, 0, (t->mipmap - 1) << 8, GCM_TEXTURE_MAX_ANISO_1);
    unsigned filter = min_filter == RSXR_NEAREST ? GCM_TEXTURE_NEAREST : GCM_TEXTURE_LINEAR;

    if (t->mipmap > 1) {
        if (min_filter == RSXR_NEAREST_MIPMAP_NEAREST)
            filter = GCM_TEXTURE_NEAREST_MIPMAP_NEAREST;

        if (min_filter == RSXR_LINEAR_MIPMAP_NEAREST)
            filter = GCM_TEXTURE_LINEAR_MIPMAP_NEAREST;

        if (min_filter == RSXR_NEAREST_MIPMAP_LINEAR)
            filter = GCM_TEXTURE_NEAREST_MIPMAP_LINEAR;

        if (min_filter == RSXR_LINEAR_MIPMAP_LINEAR)
            filter = GCM_TEXTURE_LINEAR_MIPMAP_LINEAR;
    }

    rsxTextureFilter(context, 0, 0, filter,
      mag_filter == RSXR_NEAREST ? GCM_TEXTURE_NEAREST : GCM_TEXTURE_LINEAR,
      GCM_TEXTURE_CONVOLUTION_QUINCUNX);
    unsigned wrap = repeat ? GCM_TEXTURE_REPEAT : GCM_TEXTURE_CLAMP_TO_EDGE;

    unsigned format = t->format & ~(GCM_TEXTURE_FORMAT_LIN | GCM_TEXTURE_FORMAT_UNRM);
    unsigned zfunc  =
      format == GCM_TEXTURE_FORMAT_DEPTH24_D8 ? GCM_TEXTURE_ZFUNC_NEVER : GCM_TEXTURE_ZFUNC_LESS;
    rsxTextureWrapMode(context, 0, wrap, wrap, wrap, 0, zfunc, 0);
}

void
RSXR_Viewport(int x, int y, int w, int h)
{
    if (exiting)
        return;

    viewport[0] = x;
    viewport[1] = y;
    viewport[2] = w;
    viewport[3] = h;
    int gy = HEIGHT - y - h;

    if (effect_active) {
        int right = (x + w + 1) / 2, bottom = (gy + h + 1) / 2;
        x  /= 2;
        gy /= 2;
        w   = right - x;
        h   = bottom - gy;
    }

    float scale[4]  = { w * .5f, -h * .5f, (depth_far - depth_near) * .5f, 0 };
    float offset[4] = { x + w * .5f, gy + h * .5f, (depth_far + depth_near) * .5f, 0 };
    rsxSetViewport(context, x, gy, w, h, depth_near, depth_far, scale, offset);
    rsxSetScissor(context, x, gy, w, h);
}

static void
State(void)
{
    if (!state_dirty)
        return;

    state_dirty = 0;
    rsxSetBlendEnable(context, blend);
    rsxSetDepthTestEnable(context, depth_test);
    rsxSetCullFaceEnable(context, cull);
    rsxSetCullFace(context, cull_face);
    rsxSetFrontFace(context, GCM_FRONTFACE_CCW);
    rsxSetDepthWriteEnable(context, effect_active ? 0 : depth_write);
    rsxSetDepthFunc(context, depth_func);
    rsxSetZCullEnable(
        context,
        !effect_active && zcull && ps3_zcull.value && depth_test && depth_func != GCM_GEQUAL, 0);
    rsxSetBlendFunc(context, blend_src, blend_dst, effect_active ? GCM_ZERO : blend_src, blend_dst);
    rsxSetBlendEquation(context, GCM_FUNC_ADD, GCM_FUNC_ADD);
    rsxSetPolygonOffsetFillEnable(context, polygon_offset);
    rsxSetPolygonOffset(context, offset_factor, offset_units);
    RSXR_Viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void
RSXR_BeginFrame(void)
{
    if (active || exiting)
        return;

    Wait(completed[frame]);
    sync_profile = r_profile.value && ps3_profile_sync.value;

    if (sync_profile) {
        double start = Sys_FloatTime();
        RSXR_Finish();
        profile_start_seconds += Sys_FloatTime() - start;
    }

    used = 0;
    ++generation;
    ++frame_id;
    active = 1;
    rsxInvalidateVertexCache(context);
    Surface(scene_offset);
    state_dirty = 1;
    State();
}

static void
BindShader(shader_t * shader)
{
    if (bound_shader == shader)
        return;

    if (!bound_shader || bound_shader->vp != shader->vp) {
        rsxLoadVertexProgram(context, shader->vp, shader->ucode);
        constant_valid = 0;
    }

    bound_shader = shader;
    rsxLoadFragmentProgramLocation(context, shader->fp, shader->offset, GCM_LOCATION_RSX);
}

static void
Constant(unsigned index, const float * value)
{
    size_t bytes = (index < 2 ? 16 : 4) * sizeof(float);

    if ((constant_valid & (1u << index)) && !memcmp(constant_values[index], value, bytes))
        return;

    memcpy(constant_values[index], value, bytes);
    constant_valid |= 1u << index;
    rsxProgramConst * c = constants[index];

    if (c)
        rsxSetVertexProgramParameter(context, game[0].vp, c, value);
}

static size_t
AllocateStream(size_t bytes)
{
    size_t aligned = (bytes + 127) & ~(size_t) 127;

    if (aligned > STREAM_BYTES)
        Sys_Error("PS3: allocation exceeds transient arena");

    if (used + aligned > STREAM_BYTES) {
        RSXR_Finish();
        used = 0;
        ++generation;
        ++spills;
        rsxInvalidateVertexCache(context);
    }

    size_t result = used;
    used         += aligned;
    stream_bytes += bytes;
    stream_peak   = max(stream_peak, used);
    return result;
}

static void
Draw(shader_t * shader, unsigned primitive, const rsxr_vertex_t * data, size_t count)
{
    ++attribute_generation;
    size_t bytes = count * sizeof(*data), allocation = AllocateStream(bytes);
    memcpy((byte *) vertices[frame] + allocation, data, bytes);

    __asm__ volatile ("sync" ::: "memory");
    u32 base = vertex_offset[frame] + allocation;
    int p = shader->position, t = shader->texcoord, c = shader->color;

    if (p >= 0)
        rsxBindVertexArrayAttrib(context, p, 0, base, sizeof(*data), 3, GCM_VERTEX_DATA_TYPE_F32,
          GCM_LOCATION_CELL);

    if (t >= 0)
        rsxBindVertexArrayAttrib(context, t, 0, base + 12, sizeof(*data), 2,
          GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_CELL);

    if (c >= 0)
        rsxBindVertexArrayAttrib(context, c, 0, base + 20, sizeof(*data), 4,
          GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_CELL);

    rsxDrawVertexArray(context, primitive, 0, count);
    ++draws;
}

void
RSXR_UploadBuffer(hyena_buffer_t * buffer)
{
    if (exiting || !buffer->size || !buffer->data)
        return;

    if (buffer->gpu)
        RSXR_Finish();

    if (buffer->gpu_capacity < buffer->size) {
        if (buffer->gpu)
            rsxFree(buffer->gpu);

        static_bytes        -= buffer->gpu_capacity;
        buffer->gpu          = NULL;
        buffer->gpu_capacity = 0;

        if (static_bytes + buffer->size > STATIC_GPU_BUDGET)
            return;

        buffer->gpu = TryLocal(buffer->size, &buffer->gpu_offset);

        if (!buffer->gpu)
            return;

        static_bytes        += buffer->size;
        buffer->gpu_capacity = buffer->size;
    }

    memcpy(buffer->gpu, buffer->data, buffer->size);
    __asm__ volatile ("sync" ::: "memory");
    rsxInvalidateVertexCache(context);
}

void
RSXR_FreeBuffer(hyena_buffer_t * buffer)
{
    if (buffer->gpu && !exiting) {
        RSXR_Finish();
        rsxFree(buffer->gpu);
    }

    static_bytes        -= buffer->gpu_capacity;
    buffer->gpu          = NULL;
    buffer->gpu_capacity = 0;
}

typedef struct {
    unsigned offset, stride, components, type, location;
    int      enabled;
} native_array_t;

typedef struct {
    uintptr_t base;
    size_t    span, bytes;
    unsigned  stride, offset;
} stream_group_t;

static const byte *
ArrayBytes(const hyena_array_t * a)
{
    return a->buffer ? (const byte *) a->buffer->data + (uintptr_t) a->data : a->data;
}

static unsigned
ComponentBytes(const hyena_array_t * a)
{
    return a->type == HYE_SHORT ? 2 : a->type == HYE_UBYTE ? 1 : 4;
}

static int
NativeFormat(const hyena_array_t * a, int color)
{
    return a->type == HYE_FLOAT || (color && a->type == HYE_UBYTE);
}

static size_t
ArraySpan(const hyena_array_t * a, unsigned components, unsigned count)
{
    size_t width  = components * ComponentBytes(a);
    size_t stride = a->stride ? (unsigned) a->stride : width;

    if (a->stride < 0 || stride < width)
        Sys_Error("PS3: invalid array stride");

    return (count - 1) * stride + width;
}

static void
BindArray(unsigned slot, const native_array_t * a, const float * constant)
{
    static native_array_t cached[16];
    static float cached_values[16][4];
    static unsigned valid, cached_generation;

    if (slot == ~0u)
        return;

    if (slot >= 16)
        Sys_Error("PS3: invalid vertex attribute slot");

    if (cached_generation != attribute_generation) {
        valid = 0;
        cached_generation = attribute_generation;
    }

    if ((valid & (1u << slot)) && !memcmp(cached + slot, a, sizeof(*a)) &&
      (a->enabled || !memcmp(cached_values[slot], constant, 4 * sizeof(float))))
        return;

    cached[slot] = *a;
    memcpy(cached_values[slot], constant, 4 * sizeof(float));
    valid |= 1u << slot;
    rsxBindVertexArrayAttrib(context, slot, 0, a->enabled ? a->offset : 0,
      a->enabled ? a->stride : 0, a->enabled ? a->components : 0,
      a->enabled ? a->type : GCM_VERTEX_DATA_TYPE_F32, a->location);

    if (!a->enabled)
        rsxDrawVertex4f(context, slot, constant);
}

void
RSXR_Submit(int mode, const hyena_arrays_t * arrays, int count, const unsigned short * indices,
  const rsxr_uniforms_t * u, const float color[4], int flat)
{
    if (exiting)
        return;

    const hyena_array_t * source[3] = { &arrays->position, &arrays->uv, &arrays->color };
    const unsigned components[3]    = { 3, 2, 4 };
    native_array_t attr[3]   = { { 0 } };
    stream_group_t groups[3] = { { 0 } };
    int membership[3] = { -1, -1, -1 }, group_count = 0;
    unsigned n = indices ? arrays->count : count, i, j;

    if (count <= 0)
        return;

    double submit_start = Sys_FloatTime();

    if ((indices && arrays->count <= 0) || (!indices && arrays->count > 0 && count > arrays->count))
        Sys_Error("PS3: invalid array count");

    if (!n || !ArrayBytes(source[0]))
        Sys_Error("PS3: missing vertex array");

    RSXR_BeginFrame();

    if (particle_scope && particle_type_half && ps3_halfres_particles.value && effect_available &&
      blend && blend_src == GCM_SRC_ALPHA &&
      (blend_dst == GCM_ONE || blend_dst == GCM_ONE_MINUS_SRC_ALPHA) && depth_test &&
      (depth_func == GCM_LESS || depth_func == GCM_LEQUAL))
    {
        int bounds[4] = { 0, 0, WIDTH, HEIGHT };
        float pixels  = 0;

        if (source[0]->type == HYE_FLOAT && source[0]->stride >= 0 &&
          (!arrays->scale || arrays->scale == 1))
        {
            unsigned stride = source[0]->stride ? source[0]->stride : 12;

            if (indices && mode == HYE_TRIANGLES && count % 3 == 0)
                PS3_IndexedParticleBounds(ArrayBytes(source[0]), stride, n, indices, count, u->mvp,
                  viewport, bounds, WIDTH, HEIGHT, &pixels);
            else
                PS3_ParticleBounds(ArrayBytes(source[0]), stride, n, u->mvp, viewport, bounds,
                  WIDTH, HEIGHT);
        }

        if (bounds[2] <= bounds[0] || bounds[3] <= bounds[1])
            return;

        float rectangle = (float) (bounds[2] - bounds[0]) * (bounds[3] - bounds[1]);

        if (ps3_halfres_min_overdraw.value > 0 &&
          pixels < rectangle * ps3_halfres_min_overdraw.value)
        {
            EndEffects();
            ++effect_native_draws;
        } else if (!effect_active)
            BeginEffects();

        if (effect_active) {
            EnsureEffectsDepth(bounds);
            effect_bounds[0] = min(effect_bounds[0], bounds[0]);
            effect_bounds[1] = min(effect_bounds[1], bounds[1]);
            effect_bounds[2] = max(effect_bounds[2], bounds[2]);
            effect_bounds[3] = max(effect_bounds[3], bounds[3]);
            ++effect_draws;
        }
    } else if (effect_active)
        EndEffects();

    size_t reserve = indices ? (((size_t) count * 2 + 127) & ~(size_t) 127) : 0;

    for (i = 0; i < 3; ++i) {
        const hyena_array_t * a = source[i];

        if (!a->data && !a->buffer)
            continue;

        size_t width = components[i] * ComponentBytes(a), span = ArraySpan(a, components[i], n);
        unsigned stride = a->stride ? (unsigned) a->stride : width;
        int native      = NativeFormat(a, i == 2) && stride <= 255;

        if (a->buffer &&
          ((uintptr_t) a->data > a->buffer->size || span > a->buffer->size - (uintptr_t) a->data))
            Sys_Error("PS3: array outside buffer");

        attr[i].enabled    = 1;
        attr[i].components = components[i];
        attr[i].type       = a->type == HYE_UBYTE ? GCM_VERTEX_DATA_TYPE_U8 : GCM_VERTEX_DATA_TYPE_F32;
        attr[i].stride     = stride;

        if (native && a->buffer && a->buffer->gpu) {
            attr[i].location = GCM_LOCATION_RSX;
            attr[i].offset   = a->buffer->gpu_offset + (uintptr_t) a->data;
            continue;
        }

        attr[i].location = GCM_LOCATION_CELL;

        if (native) {
            uintptr_t base = (uintptr_t) ArrayBytes(a);

            for (j = 0; j < (unsigned) group_count; ++j) {
                uintptr_t lo = min(base, groups[j].base);
                uintptr_t hi = max(base + width, groups[j].base + groups[j].span);

                if (stride == groups[j].stride && hi - lo <= stride) {
                    groups[j].base = lo;
                    groups[j].span = hi - lo;
                    break;
                }
            }

            if (j == (unsigned) group_count) {
                groups[j].base   = base;
                groups[j].span   = width;
                groups[j].stride = stride;
                ++group_count;
            }

            membership[i] = j;
        } else {
            attr[i].type   = GCM_VERTEX_DATA_TYPE_F32;
            attr[i].stride = components[i] * 4;
            reserve       += ((size_t) n * attr[i].stride + 127) & ~(size_t) 127;
            membership[i]  = -2;
        }
    }

    for (i = 0; i < (unsigned) group_count; ++i) {
        groups[i].bytes = (n - 1) * groups[i].stride + groups[i].span;
        reserve        += (groups[i].bytes + 127) & ~(size_t) 127;
    }

    ps3_texture_t * tex = &textures[bound_texture];

    if (tex->dynamic)
        reserve += (tex->bytes + 127) & ~(size_t) 127;

    if (reserve > STREAM_BYTES)
        Sys_Error("PS3: draw exceeds transient arena");

    if (used + reserve > STREAM_BYTES) {
        RSXR_Finish();
        used = 0;
        ++generation;
        ++spills;
        rsxInvalidateVertexCache(context);
    }

    for (i = 0; i < (unsigned) group_count; ++i) {
        size_t allocation = AllocateStream(groups[i].bytes);
        memcpy((byte *) vertices[frame] + allocation, (const void *) groups[i].base, groups[i].bytes);
        groups[i].offset = vertex_offset[frame] + allocation;
    }

    for (i = 0; i < 3; ++i) {
        if (membership[i] >= 0) {
            stream_group_t * group = &groups[membership[i]];
            attr[i].offset = group->offset + (uintptr_t) ArrayBytes(source[i]) - group->base;
        } else if (membership[i] == -2) {
            const hyena_array_t * a = source[i];
            size_t allocation       = AllocateStream((size_t) n * components[i] * 4);
            float * dest    = (float *) ((byte *) vertices[frame] + allocation);
            unsigned stride = a->stride ? (unsigned) a->stride : components[i] * ComponentBytes(a),
              v, c;

            for (v = 0; v < n; ++v)
                for (c = 0; c < components[i]; ++c) {
                    const byte * p = ArrayBytes(a) + (size_t) v * stride + c * ComponentBytes(a);
                    float value;

                    if (a->type == HYE_SHORT) {
                        short x;
                        memcpy(&x, p, 2);
                        value = i == 2 ? fmaxf(-1, x / 32767.0f) : x;
                    } else if (a->type == HYE_UBYTE)
                        value = i == 2 ? *p / 255.0f : *p;
                    else
                        memcpy(&value, p, 4);

                    *dest++ = value;
                }

            attr[i].offset = vertex_offset[frame] + allocation;
        }
    }

    unsigned index_offset = 0;

    if (indices) {
        size_t allocation = AllocateStream((size_t) count * 2);
        memcpy((byte *) vertices[frame] + allocation, indices, (size_t) count * 2);
        index_offset = vertex_offset[frame] + allocation;
    }

    gcmTexture image = tex->image;

    if (tex->dynamic) {
        if (!tex->versions[frame] && lightmap_bytes + tex->bytes <= LIGHTMAP_GPU_BUDGET) {
            tex->versions[frame] = TryLocal(tex->bytes, &tex->version_offset[frame]);

            if (tex->versions[frame]) {
                tex->version_bytes = tex->bytes;
                lightmap_bytes    += tex->bytes;
            }
        }

        if (tex->versions[frame] && (tex->version_frame[frame] != frame_id ||
          tex->version_revision[frame] == tex->revision))
        {
            if (tex->version_revision[frame] != tex->revision) {
                memcpy(tex->versions[frame], tex->pixels, tex->bytes);
                ++snapshots;
                tex->version_revision[frame] = tex->revision;
                rsxInvalidateTextureCache(context, GCM_INVALIDATE_TEXTURE);
            }

            tex->version_frame[frame] = frame_id;
            image.offset = tex->version_offset[frame];
        } else {
            if (tex->snapshot_generation != generation || tex->snapshot_revision != tex->revision) {
                size_t allocation = AllocateStream(tex->bytes);
                memcpy((byte *) vertices[frame] + allocation, tex->pixels, tex->bytes);
                ++snapshots;
                tex->snapshot_offset     = vertex_offset[frame] + allocation;
                tex->snapshot_generation = generation;
                tex->snapshot_revision   = tex->revision;
                rsxInvalidateTextureCache(context, GCM_INVALIDATE_TEXTURE);
            }

            image.offset   = tex->snapshot_offset;
            image.location = GCM_LOCATION_CELL;
        }
    }

    __asm__ volatile ("sync" ::: "memory");
    unsigned variant = (u->params[3] > 0.5f ? 1u : 0u) | (u->params[2] >= 0 ? 2u : 0u)
      | (u->flags[1] > 0.5f ? 4u : 0u);
    shader_t * shader = &game[variant];
    BindShader(shader);
    State();
    float mvp[16], mv[16];
    unsigned r, c;

    for (r = 0; r < 4; ++r)
        for (c = 0; c < 4; ++c) {
            mvp[r * 4 + c] = u->mvp[c * 4 + r];
            mv[r * 4 + c]  = u->modelview[c * 4 + r];
        }

    Constant(0, mvp);
    Constant(1, mv);
    Constant(2, u->uv);
    Constant(3, u->fog_color);
    Constant(4, u->params);
    Constant(5, u->flags);
    Constant(6, u->vertex_state);

    if (variant & 1)
        Texture(&image, tex->min_filter, tex->mag_filter, 1);

    const float zero[4] = { 0, 0, 0, 1 };
    BindArray(shader->position, &attr[0], zero);
    BindArray(shader->texcoord, &attr[1], zero);
    BindArray(shader->color, &attr[2], color);
    rsxSetShadeModel(context, flat ? GCM_SHADE_MODEL_FLAT : GCM_SHADE_MODEL_SMOOTH);
    unsigned primitive = mode == HYE_QUADS ? GCM_TYPE_QUADS :
      mode == HYE_TRIANGLE_FAN ? GCM_TYPE_TRIANGLE_FAN :
      mode == HYE_TRIANGLE_STRIP ? GCM_TYPE_TRIANGLE_STRIP :
      mode == HYE_LINE_STRIP ? GCM_TYPE_LINE_STRIP :
      GCM_TYPE_TRIANGLES;

    if (indices)
        rsxDrawIndexArray(context, primitive, index_offset, count, GCM_INDEX_TYPE_16B,
          GCM_LOCATION_CELL);
    else
        rsxDrawVertexArray(context, primitive, 0, count);

    ++draws;
    blend_draws  += blend != 0;
    cutout_draws += (variant & 2) != 0;

    if (indices) {
        ++indexed_draws;
        index_bytes += (size_t) count * 2;
    }

    if (arrays->position.buffer && arrays->position.buffer->gpu)
        ++static_draws;

    if ((draws & 15u) == 0) {
        rsxFlushBuffer(context);
        ++flushes;
    }

    submit_seconds += Sys_FloatTime() - submit_start;
}

void
RSXR_Enable(unsigned cap)
{
    int * value = cap == RSXR_BLEND ? &blend :
      cap == RSXR_DEPTH_TEST ? &depth_test :
      cap == RSXR_CULL_FACE ? &cull :
      cap == RSXR_POLYGON_OFFSET_FILL ? &polygon_offset :
      NULL;

    if (value && !*value) {
        *value      = 1;
        state_dirty = 1;
    }
}

void
RSXR_Disable(unsigned cap)
{
    int * value = cap == RSXR_BLEND ? &blend :
      cap == RSXR_DEPTH_TEST ? &depth_test :
      cap == RSXR_CULL_FACE ? &cull :
      cap == RSXR_POLYGON_OFFSET_FILL ? &polygon_offset :
      NULL;

    if (value && *value) {
        *value      = 0;
        state_dirty = 1;
    }
}

void
RSXR_BlendFunc(unsigned a, unsigned b)
{
    if (blend_src != a || blend_dst != b) {
        state_dirty = 1;
        blend_src   = a;
        blend_dst   = b;
    }
}

void
RSXR_DepthMask(int enabled)
{
    if (depth_write != enabled) {
        state_dirty = 1;
        depth_write = enabled;
    }
}

void
RSXR_DepthFunc(unsigned fn)
{
    if (depth_func != fn) {
        state_dirty = 1;
        depth_func  = fn;
    }
}

void
RSXR_DepthRange(float n, float f)
{
    if (depth_near != n || depth_far != f) {
        state_dirty = 1;
        depth_near  = n;
        depth_far   = f;
    }
}

void
RSXR_PolygonOffset(float f, float u)
{
    if (offset_factor != f || offset_units != u) {
        state_dirty   = 1;
        offset_factor = f;
        offset_units  = u;
    }
}

void
RSXR_CullFace(unsigned f)
{
    unsigned face = f == RSXR_FRONT ? GCM_CULL_FRONT : GCM_CULL_BACK;

    if (cull_face != face) {
        state_dirty = 1;
        cull_face   = face;
    }
}

void
RSXR_ClearColor(float r, float g, float b, float a)
{
    clear_color_value =
      ((u32) (a * 255) << 24) | ((u32) (r * 255) << 16) | ((u32) (g * 255) << 8) | (u32) (b * 255);

    if (!exiting)
        rsxSetClearColor(context, clear_color_value);
}

void
RSXR_ClearDepth(float d)
{
    clear_depth = d;
}

void
RSXR_Clear(unsigned mask)
{
    if (exiting)
        return;

    if (effect_active)
        RSXR_ParticlesEnd();

    RSXR_BeginFrame();
    unsigned clear = 0;

    if (mask & RSXR_COLOR_BUFFER_BIT)
        clear |= GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A;

    if (mask & RSXR_DEPTH_BUFFER_BIT)
        clear |= GCM_CLEAR_Z;

    if (mask & RSXR_STENCIL_BUFFER_BIT)
        clear |= GCM_CLEAR_S;

    rsxSetDepthWriteEnable(context, 1);
    state_dirty = 1;
    rsxSetClearDepthStencil(context, ((u32) (clear_depth * 16777215) << 8));

    if ((mask & RSXR_DEPTH_BUFFER_BIT) && zcull)
        rsxSetClearZCullSurface(context, 1, 0);

    rsxClearSurface(context, clear);
}

unsigned
RSXR_CreateTexture(void)
{
    unsigned i;

    for (i = 1; i < MAX_TEXTURES; ++i)
        if (!textures[i].used) {
            textures[i].used       = 1;
            textures[i].min_filter = textures[i].mag_filter = RSXR_LINEAR;
            return i;
        }

    Sys_Error("PS3: texture table exhausted");
    return 0;
}

void
RSXR_BindTexture(unsigned t)
{
    if (t >= MAX_TEXTURES)
        Sys_Error("PS3: invalid texture");

    bound_texture = t;
}

static void
FreeVersions(ps3_texture_t * texture)
{
    unsigned slot;

    for (slot = 0; slot < 2; ++slot)
        if (texture->versions[slot]) {
            rsxFree(texture->versions[slot]);
            texture->versions[slot] = NULL;
            lightmap_bytes -= texture->version_bytes;
        }

    texture->version_bytes = 0;
    memset(texture->version_revision, 0, sizeof(texture->version_revision));
    memset(texture->version_frame, 0, sizeof(texture->version_frame));
}

void
RSXR_DestroyTexture(unsigned t)
{
    if (exiting || t >= MAX_TEXTURES || !textures[t].used)
        return;

    RSXR_Finish();
    FreeVersions(&textures[t]);

    if (textures[t].dynamic)
        free(textures[t].pixels);
    else
        rsxFree(textures[t].pixels);

    memset(&textures[t], 0, sizeof(textures[t]));
}

void
RSXR_TextureFilter(int a, int b)
{
    textures[bound_texture].min_filter = a;
    textures[bound_texture].mag_filter = b;
}

void
RSXR_Mipmaps(int enabled)
{
    textures[bound_texture].mipmaps = enabled;
}

void
RSXR_DynamicTexture(int enabled)
{
    textures[bound_texture].dynamic = enabled;
}

void
RSXR_Upload(int level, int storage, int w, int h, int x, int y, int replace, const void * pixels)
{
    if (exiting)
        return;

    ps3_texture_t * t = &textures[bound_texture];

    if (!pixels || w <= 0 || h <= 0 || x < 0 || y < 0) {
        last_error = RSXR_INVALID_VALUE;
        return;
    }

    if (!t->dynamic)
        RSXR_Finish();

    if (replace)
        x = y = 0;

    if (replace && level == 0) {
        if (t->dynamic && t->version_bytes && t->version_bytes != (size_t) w * h * 4) {
            RSXR_Finish();
            FreeVersions(t);
        }

        if (t->pixels) {
            if (t->dynamic)
                free(t->pixels);
            else
                rsxFree(t->pixels);
        }

        u32 offset = 0;
        unsigned mw = w, mh = h, levels = 1;
        t->bytes = (size_t) w * h * 4;
        int swizzled = !t->dynamic && (w & (w - 1)) == 0 && (h & (h - 1)) == 0;

        if (swizzled && t->mipmaps)
            while (mw > 1 || mh > 1) {
                mw        = max(1, mw / 2);
                mh        = max(1, mh / 2);
                t->bytes += (size_t) mw * mh * 4;
                ++levels;
            }

        t->pixels = t->dynamic ? calloc(1, t->bytes) : Local(t->bytes, &offset);

        if (!t->pixels)
            Sys_Error("PS3: lightmap staging allocation failed");

        t->image = Image(offset, w, h);

        if (swizzled) {
            t->image.format = GCM_TEXTURE_FORMAT_A8R8G8B8;
            t->image.pitch  = 0;
            t->image.mipmap = levels;
        }
    }

    if (level < 0 || level >= t->image.mipmap)
        return;

    unsigned lw = t->image.width, lh = t->image.height, i;
    size_t base = 0;

    for (i = 0; i < (unsigned) level; ++i) {
        base += (size_t) lw * lh * 4;
        lw    = max(1, lw / 2);
        lh    = max(1, lh / 2);
    }

    if (!t->pixels || x + w > (int) lw || y + h > (int) lh) {
        last_error = RSXR_INVALID_VALUE;
        return;
    }

    const byte * src       = pixels;
    const uint16_t * words = pixels;
    unsigned row, col;

    for (row = 0; row < (unsigned) h; ++row)
        for (col = 0; col < (unsigned) w; ++col) {
            size_t index = (row + y) * lw + col + x;

            if (!(t->image.format & GCM_TEXTURE_FORMAT_LIN))
                index = PS3_SwizzleIndex(col + x, row + y, lw, lh);

            byte * dst = (byte *) t->pixels + base + index * 4;
            size_t i   = (size_t) row * w + col;

            if (storage == HYE_PIXELS_RGBA8) {
                dst[0] = src[4 * i + 3];
                dst[1] = src[4 * i];
                dst[2] = src[4 * i + 1];
                dst[3] = src[4 * i + 2];
            } else if (storage == HYE_PIXELS_L8) {
                dst[0] = 255;
                dst[1] = dst[2] = dst[3] = src[i];
            } else if (storage == HYE_PIXELS_RGBA5551) {
                uint16_t v = words[i];
                dst[0] = (v & 1) ? 255 : 0;
                dst[1] = ((v >> 11) & 31) * 255 / 31;
                dst[2] = ((v >> 6) & 31) * 255 / 31;
                dst[3] = ((v >> 1) & 31) * 255 / 31;
            } else if (storage == HYE_PIXELS_RGBA4444) {
                uint16_t v = words[i];
                dst[0] = (v & 15) * 17;
                dst[1] = ((v >> 12) & 15) * 17;
                dst[2] = ((v >> 8) & 15) * 17;
                dst[3] = ((v >> 4) & 15) * 17;
            } else
                Sys_Error("PS3: unsupported texture storage %d", storage);
        }

    __asm__ volatile ("sync" ::: "memory");
    rsxInvalidateTextureCache(context, GCM_INVALIDATE_TEXTURE);
    ++t->revision;
}

unsigned
RSXR_GetError(void)
{
    unsigned e = last_error;

    last_error = 0;
    return e;
}

void
RSXR_SelectReadBuffer(int front)
{
    read_front = front;
}

int
RSXR_ReadPixels(int x, int y, int w, int h, unsigned format, unsigned type, void * pixels)
{
    if (exiting)
        return 0;

    if (x < 0 || y < 0 || w < 0 || h < 0 || x + w > WIDTH || y + h > HEIGHT || !pixels ||
      type != RSXR_UNSIGNED_BYTE || (format != RSXR_RGB && format != RSXR_RGBA))
    {
        last_error = RSXR_INVALID_VALUE;
        return 0;
    }

    RSXR_Finish();
    unsigned row, col, channels = format == RSXR_RGB ? 3 : 4;
    byte * dst = pixels;
    const byte * source = read_front ? output[frame ^ 1] : scene;

    for (row = 0; row < (unsigned) h; ++row)
        for (col = 0; col < (unsigned) w; ++col) {
            const byte * src = source + (HEIGHT - 1 - y - row) * color_pitch + (x + col) * 4;
            *dst++ = src[1];
            *dst++ = src[2];
            *dst++ = src[3];

            if (channels == 4)
                *dst++ = src[0];
        }

    return 1;
}

static void *
Load(const char * name)
{
    char path[256];

    snprintf(path, sizeof(path), PS3_INSTALLDIR "/shaders/%s", name);
    FILE * f = fopen(path, "rb");

    if (!f)
        Sys_Error("PS3: missing shader %s", path);

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size <= 0 || size > 1024 * 1024)
        Sys_Error("PS3: invalid shader %s", path);

    void * p = memalign(16, size);

    if (!p || fread(p, 1, size, f) != (size_t) size)
        Sys_Error("PS3: reading shader %s", path);

    fclose(f);
    return p;
}

static void
Shader(shader_t * s, const char * vertex, const char * fragment)
{
    void * code;
    u32 size;

    s->vp = Load(vertex);
    s->fp = Load(fragment);
    rsxVertexProgramGetUCode(s->vp, &s->ucode, &size);
    rsxFragmentProgramGetUCode(s->fp, &code, &size);
    s->fragment = Local(size, &s->offset);
    memcpy(s->fragment, code, size);
    rsxProgramAttrib * a = rsxVertexProgramGetAttrib(s->vp, "position");
    s->position = a ? (int) a->index : -1;
    a = rsxVertexProgramGetAttrib(s->vp, "texcoord");
    s->texcoord = a ? (int) a->index : -1;
    a        = rsxVertexProgramGetAttrib(s->vp, "color");
    s->color = a ? (int) a->index : -1;
}

static void
PassViewport(unsigned w, unsigned h)
{
    float scale[4] = { w * .5f, -h * .5f, .5f, 0 }, offset[4] = { w * .5f, h * .5f, .5f, 0 };

    rsxSetViewport(context, 0, 0, w, h, 0, 1, scale, offset);
    rsxSetScissor(context, 0, 0, w, h);
}

static void
PassState(void)
{
    rsxSetBlendEnable(context, 0);
    rsxSetDepthTestEnable(context, 0);
    rsxSetDepthWriteEnable(context, 0);
    rsxSetZCullEnable(context, 0, 0);
    rsxSetCullFaceEnable(context, 0);
    rsxSetPolygonOffsetFillEnable(context, 0);
    rsxSetShadeModel(context, GCM_SHADE_MODEL_SMOOTH);
    rsxSetColorMask(context,
      GCM_COLOR_MASK_R | GCM_COLOR_MASK_G | GCM_COLOR_MASK_B | GCM_COLOR_MASK_A);
}

static void
PassTriangle(shader_t * shader)
{
    const rsxr_vertex_t quad[3] = {
        { { -1, -1, 0 }, { 0, 1 }, { 1, 1, 1, 1 } },
        { { 3, -1, 0 }, { 2, 1 }, { 1, 1, 1, 1 } },
        { { -1, 3, 0 }, { 0, -1 }, { 1, 1, 1, 1 } }
    };

    Draw(shader, GCM_TYPE_TRIANGLES, quad, 3);
}

static void
InitEffects(void)
{
    effect_pitch = gcmGetTiledPitchSize(EFFECT_WIDTH * 4);
    size_t bytes = PS3_SurfaceBytes(effect_pitch, EFFECT_HEIGHT);
    effect_color = rsxMemalign(GCM_TILE_ALIGN_SIZE, bytes);
    effect_depth = rsxMemalign(GCM_TILE_ALIGN_SIZE, bytes);

    if (!effect_pitch || !effect_color || !effect_depth ||
      rsxAddressToOffset(effect_color, &effect_color_offset) != 0 ||
      rsxAddressToOffset(effect_depth, &effect_depth_offset) != 0)
    {
        if (effect_color)
            rsxFree(effect_color);

        if (effect_depth)
            rsxFree(effect_depth);

        effect_color = effect_depth = NULL;
        Con_Printf("PS3: half-resolution particle buffers unavailable; using native resolution\n");
        return;
    }

    if (gcmSetTileInfo(4, GCM_LOCATION_RSX, effect_color_offset, bytes, effect_pitch,
      GCM_COMPMODE_DISABLED, 0, 0) == 0 &&
      gcmBindTile(4) == 0)
        effect_tiles |= 1;

    if (gcmSetTileInfo(5, GCM_LOCATION_RSX, effect_depth_offset, bytes, effect_pitch,
      GCM_COMPMODE_DISABLED, 0, 2) == 0 &&
      gcmBindTile(5) == 0)
        effect_tiles |= 2;

    Shader(&effect_depth_shader, "present.vpo", "particle_depth.fpo");
    Shader(&effect_composite, "present.vpo", "particle_composite.fpo");
    effect_available = 1;
}

static int
BeginEffects(void)
{
    if (!effect_available)
        return 0;

    rsxSetWaitForIdle(context);
    effect_active = 1;
    Surface(effect_color_offset);
    PassState();
    PassViewport(EFFECT_WIDTH, EFFECT_HEIGHT);
    rsxSetColorMask(context,
      GCM_COLOR_MASK_R | GCM_COLOR_MASK_G | GCM_COLOR_MASK_B | GCM_COLOR_MASK_A);
    rsxSetClearColor(context, 0xff000000);
    rsxClearSurface(context, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A);
    rsxSetClearColor(context, clear_color_value);
    effect_bounds[0] = WIDTH;
    effect_bounds[1] = HEIGHT;
    effect_bounds[2] = effect_bounds[3] = 0;
    effect_blend_dst = blend_dst;
    state_dirty      = 1;
    State();
    return 1;
}

static void
EnsureEffectsDepth(const int * bounds)
{
    int rects[4][4];
    int count = PS3_ParticleDepthRects(bounds, effect_depth_bounds, rects);

    if (!count)
        return;

    PassState();
    PassViewport(EFFECT_WIDTH, EFFECT_HEIGHT);
    rsxSetColorMask(context, 0);
    rsxSetDepthTestEnable(context, 1);
    rsxSetDepthFunc(context, GCM_ALWAYS);
    rsxSetDepthWriteEnable(context, 1);
    BindShader(&effect_depth_shader);
    gcmTexture image = Image(depth_offset, WIDTH, HEIGHT);
    image.pitch = depth_pitch;
    rsxInvalidateTextureCache(context, GCM_INVALIDATE_TEXTURE);
    Texture(&image, RSXR_NEAREST, RSXR_NEAREST, 0);

    for (int i = 0; i < count; ++i) {
        int w = rects[i][2] - rects[i][0], h = rects[i][3] - rects[i][1];
        rsxSetScissor(context, rects[i][0], rects[i][1], w, h);
        PassTriangle(&effect_depth_shader);
        effect_depth_pixels += (uint64_t) w * h;
    }

    rsxSetColorMask(context,
      GCM_COLOR_MASK_R | GCM_COLOR_MASK_G | GCM_COLOR_MASK_B | GCM_COLOR_MASK_A);
    state_dirty = 1;
    State();
}

static void
EndEffects(void)
{
    if (!effect_active)
        return;

    rsxSetWaitForIdle(context);
    effect_active = 0;
    Surface(scene_offset);
    PassState();
    PassViewport(WIDTH, HEIGHT);

    if (effect_bounds[2] > effect_bounds[0] && effect_bounds[3] > effect_bounds[1]) {
        rsxSetScissor(context, effect_bounds[0], effect_bounds[1],
          effect_bounds[2] - effect_bounds[0], effect_bounds[3] - effect_bounds[1]);
        BindShader(&effect_composite);
        rsxSetBlendEnable(context, 1);
        rsxSetBlendFunc(context, GCM_ONE, GCM_SRC_ALPHA, GCM_ZERO, GCM_ONE);
        rsxSetBlendEquation(context, GCM_FUNC_ADD, GCM_FUNC_ADD);
        gcmTexture image = Image(effect_color_offset, EFFECT_WIDTH, EFFECT_HEIGHT);
        image.pitch = effect_pitch;
        rsxInvalidateTextureCache(context, GCM_INVALIDATE_TEXTURE);
        Texture(&image, RSXR_LINEAR, RSXR_LINEAR, 0);
        PassTriangle(&effect_composite);
        ++effect_composites;
    }

    state_dirty = 1;
    State();
}

void
RSXR_ParticlesBegin(void)
{
    if (exiting)
        return;

    EndEffects();
    particle_scope     = 1;
    particle_type_half = 0;
    memset(effect_depth_bounds, 0, sizeof(effect_depth_bounds));
}

void
RSXR_ParticleType(int half_resolution)
{
    if (exiting)
        return;

    int supported = blend_src == GCM_SRC_ALPHA &&
      (blend_dst == GCM_ONE || blend_dst == GCM_ONE_MINUS_SRC_ALPHA);

    if (effect_active && (!half_resolution || !supported || effect_blend_dst != blend_dst))
        EndEffects();

    particle_type_half = particle_scope && half_resolution && supported;
}

void
RSXR_ParticlesEnd(void)
{
    if (exiting)
        return;

    EndEffects();
    particle_scope = particle_type_half = 0;
}

const char *
PS3_GraphicsError(void)
{
    return error_text;
}

int
PS3_GraphicsInit(void)
{
    if (initialized)
        return 1;

    host = memalign(1024 * 1024, 1024 * 1024);

    if (!host || rsxInit(&context, 256 * 1024, 1024 * 1024, host) != 0) {
        snprintf(error_text, sizeof(error_text), "rsxInit failed");
        return 0;
    }

    if (videoGetResolutionAvailability(VIDEO_PRIMARY, VIDEO_RESOLUTION_1080, VIDEO_ASPECT_16_9,
      0) != 1)
    {
        snprintf(error_text, sizeof(error_text),
          "1920x1080 output unavailable; enable 1080p in system settings");
        return 0;
    }

    color_tiling = !COM_CheckParm("-ps3_linear_color");
    color_pitch  = color_tiling ? gcmGetTiledPitchSize(WIDTH * 4) : WIDTH * 4;

    if (!color_pitch)
        Sys_Error("PS3: invalid color pitch");

    color_bytes = PS3_SurfaceBytes(color_pitch, HEIGHT);
    videoConfiguration config = { 0 };
    config.resolution = VIDEO_RESOLUTION_1080;
    config.format     = VIDEO_BUFFER_FORMAT_XRGB;
    config.aspect     = VIDEO_ASPECT_16_9;
    config.pitch      = color_pitch;

    if (videoConfigure(VIDEO_PRIMARY, &config, NULL, 0) != 0) {
        snprintf(error_text, sizeof(error_text), "1080p videoConfigure failed");
        return 0;
    }

    scene       = ColorSurface(1, &scene_offset);
    depth_pitch = gcmGetTiledPitchSize(WIDTH * 4);
    size_t depth_size = PS3_SurfaceBytes(depth_pitch, HEIGHT);
    depth = rsxMemalign(GCM_TILE_ALIGN_SIZE, depth_size);

    if (!depth || rsxAddressToOffset(depth, &depth_offset) != 0)
        Sys_Error("PS3: depth allocation failed");

    if (gcmSetTileInfo(0, GCM_LOCATION_RSX, depth_offset, depth_size, depth_pitch,
      GCM_COMPMODE_Z32_SEPSTENCIL, 0, 2) == 0 &&
      gcmBindTile(0) == 0)
    {
        tiled_depth = 1;
        zcull       = gcmBindZcull(0, depth_offset, (WIDTH + 63) & ~63, (HEIGHT + 63) & ~63, 0,
            GCM_ZCULL_Z24S8, GCM_SURFACE_CENTER_1, GCM_ZCULL_LESS, GCM_ZCULL_LONES,
            GCM_SCULL_SFUNC_LESS, 1, 0xff) == 0;
    }

    rsxSetZCullControl(context, GCM_ZCULL_LESS, GCM_ZCULL_LONES);

    stream_host = memalign(1024 * 1024, 2 * STREAM_BYTES);

    if (!stream_host || gcmMapMainMemory(stream_host, 2 * STREAM_BYTES, &stream_offset) != 0)
        Sys_Error("PS3: cannot map transient stream memory");

    unsigned i;

    for (i = 0; i < 2; ++i) {
        output[i]        = ColorSurface(i + 2, &output_offset[i]);
        vertices[i]      = (byte *) stream_host + i * STREAM_BYTES;
        vertex_offset[i] = stream_offset + i * STREAM_BYTES;

        if (gcmSetDisplayBuffer(i, output_offset[i], color_pitch, WIDTH, HEIGHT) != 0)
            Sys_Error("PS3: display buffer setup failed");
    }

    Shader(&game[0], "hyena.vpo", "hyena_0.fpo");

    for (i = 1; i < 8; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "hyena_%u.fpo", i);
        game[i]    = game[0];
        game[i].fp = Load(name);
        void * code;
        u32 size;
        rsxFragmentProgramGetUCode(game[i].fp, &code, &size);
        game[i].fragment = Local(size, &game[i].offset);
        memcpy(game[i].fragment, code, size);
    }

    Shader(&present, "present.vpo", "present.fpo");
    present_copy    = present;
    present_copy.fp = Load("present_copy.fpo");
    void * copy_code;
    u32 copy_size;
    rsxFragmentProgramGetUCode(present_copy.fp, &copy_code, &copy_size);
    present_copy.fragment = Local(copy_size, &present_copy.offset);
    memcpy(present_copy.fragment, copy_code, copy_size);
    InitEffects();

    for (i = 0; i < 7; ++i) {
        constants[i] = rsxVertexProgramGetConst(game[0].vp, constant_names[i]);

        if (!constants[i])
            Sys_Error("PS3: missing shader uniform %s", constant_names[i]);
    }

    Cmd_AddCommand("ps3_gpu_stats", GPUStats);
    Cmd_AddCommand("ps3_profile", ProfileCommand);
    Cvar_RegisterVariable(&ps3_profile_sync);
    Cvar_RegisterVariable(&ps3_halfres_particles);
    Cvar_RegisterVariable(&ps3_halfres_min_overdraw);
    Cvar_RegisterVariable(&ps3_fxaa);
    Cvar_RegisterVariable(&ps3_zcull);
    report_start = Sys_FloatTime();

    textures[0].used   = 1;
    textures[0].pixels = Local(128, &textures[0].image.offset);
    u32 white_offset = textures[0].image.offset;
    textures[0].image = Image(white_offset, 1, 1);
    memset(textures[0].pixels, 255, 4);
    rsxSetColorMask(context,
      GCM_COLOR_MASK_R | GCM_COLOR_MASK_G | GCM_COLOR_MASK_B | GCM_COLOR_MASK_A);
    rsxSetColorMaskMrt(context, 0);
    rsxSetZMinMaxControl(context, 0, 1, 1);

    for (i = 0; i < 8; ++i)
        rsxSetViewportClip(context, i, WIDTH, HEIGHT);

    *(volatile u32 *) gcmGetLabelAddress(LABEL) = 0;
    gcmSetFlipMode(GCM_FLIP_VSYNC);
    initialized = 1;
    return 1;
}

int
PS3_GraphicsSwap(int vsync)
{
    if (exiting)
        return 1;

    RSXR_ParticlesEnd();
    RSXR_BeginFrame();

    if (sync_profile) {
        double start = Sys_FloatTime();
        RSXR_Finish();
        scene_drain_seconds += Sys_FloatTime() - start;
    }

    Surface(output_offset[frame]);
    rsxSetDepthTestEnable(context, 0);
    rsxSetDepthWriteEnable(context, 0);
    rsxSetZCullEnable(context, 0, 0);
    rsxSetBlendEnable(context, 0);
    rsxSetCullFaceEnable(context, 0);
    rsxSetPolygonOffsetFillEnable(context, 0);
    int saved[4];
    memcpy(saved, viewport, sizeof(saved));
    float n = depth_near, f = depth_far;
    depth_near = 0;
    depth_far  = 1;
    RSXR_Viewport(0, 0, WIDTH, HEIGHT);
    shader_t * presenter = ps3_fxaa.value ? &present : &present_copy;
    BindShader(presenter);
    rsxInvalidateTextureCache(context, GCM_INVALIDATE_TEXTURE);
    gcmTexture image = Image(scene_offset, WIDTH, HEIGHT);
    image.pitch = color_pitch;
    Texture(&image, RSXR_LINEAR, RSXR_LINEAR, 0);
    const rsxr_vertex_t quad[3] = {
        { { -1, -1, 0 }, { 0, 1 }, { 1, 1, 1, 1 } },
        { { 3, -1, 0 }, { 2, 1 }, { 1, 1, 1, 1 } },
        { { -1, 3, 0 }, { 0, -1 }, { 1, 1, 1, 1 } }
    };
    Draw(presenter, GCM_TYPE_TRIANGLES, quad, 3);

    if (sync_profile) {
        double start = Sys_FloatTime();
        RSXR_Finish();
        present_drain_seconds += Sys_FloatTime() - start;
    }

    ++frame_samples;

    if (flip_pending) {
        double start = Sys_FloatTime(), deadline = start + 10;

        while (gcmGetFlipStatus() != 0) {
            if (Sys_FloatTime() > deadline)
                return 0;

            sysUsleep(100);
        }

        flip_wait_seconds += Sys_FloatTime() - start;
    }

    gcmResetFlipStatus();
    gcmSetFlipMode(vsync ? GCM_FLIP_VSYNC : GCM_FLIP_HSYNC);

    if (gcmSetFlip(context, frame) != 0)
        return 0;

    gcmSetWaitFlip(context);
    completed[frame] = ++serial;
    rsxSetWriteBackendLabel(context, LABEL, serial);
    rsxFlushBuffer(context);
    flip_pending = 1;
    frame       ^= 1;
    active       = 0;
    depth_near   = n;
    depth_far    = f;
    memcpy(viewport, saved, sizeof(saved));

    if (r_profile.value && Sys_FloatTime() - report_start >= 2)
        GPUStats();

    return 1;
}

void
PS3_GraphicsBeginExit(void)
{
    exiting = 1;
}

void
PS3_GraphicsShutdown(void)
{
    if (!initialized)
        return;

    if (exiting) {
        initialized = 0;
        Con_SafePrintf("PS3: GPU resources retained until process exit\n");
        return;
    }

    RSXR_ParticlesEnd();
    RSXR_Finish();
    unsigned i;

    for (i = 0; i < MAX_TEXTURES; ++i) {
        if (!textures[i].pixels)
            continue;

        FreeVersions(&textures[i]);

        if (textures[i].dynamic)
            free(textures[i].pixels);
        else
            rsxFree(textures[i].pixels);
    }

    if (stream_host && gcmUnmapEaIoAddress(stream_host) == 0)
        free(stream_host);

    stream_host = NULL;

    for (i = 0; i < 8; ++i) {
        rsxFree(game[i].fragment);
        free(game[i].fp);
    }

    rsxFree(present_copy.fragment);
    free(present_copy.fp);
    rsxFree(present.fragment);
    free(game[0].vp);
    free(present.vp);
    free(present.fp);

    if (effect_available) {
        if (effect_tiles & 1)
            gcmUnbindTile(4);

        if (effect_tiles & 2)
            gcmUnbindTile(5);

        rsxFree(effect_color);
        rsxFree(effect_depth);
        rsxFree(effect_depth_shader.fragment);
        free(effect_depth_shader.fp);
        free(effect_depth_shader.vp);
        rsxFree(effect_composite.fragment);
        free(effect_composite.fp);
        free(effect_composite.vp);
    }

    if (zcull)
        gcmUnbindZcull(0);

    for (i = 0; i < 3; ++i)
        if (tiled_colors & (1u << i))
            gcmUnbindTile(i + 1);

    if (tiled_depth)
        gcmUnbindTile(0);

    initialized = 0;
}
