/*
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
#include "../xbox360_local.h"
#include "xenos_main.h"
#include "xenos_shaders.h"
#include <xenos/xe.h>
#include <xenos/xenos.h>
#include <xenos/edram.h>
#include <console/console.h>
#include <time/time.h>
#include <ppc/cache.h>

_Static_assert(sizeof(xenr_vertex_t) == 36 && offsetof(xenr_vertex_t, color) == 20, "Xenos vertex layout");
_Static_assert(sizeof(xenr_uniforms_t) == 192, "Xenos uniform layout");

#define MAX_TEXTURES 8192
#define VERTEX_BYTES (8 * 1024 * 1024)
#define INDEX_BYTES (512 * 1024)
#define WIDTH 1280
#define HEIGHT 720

typedef struct {
    struct XenosSurface * image;
    int used, min_filter, mag_filter, referenced, mipmaps, mip_count, lightmap, reports;
    size_t mip_offset[16];
    unsigned mip_pitch[16];
} xenr_texture_t;

typedef struct retired_image_s {
    struct XenosSurface * image;
    struct retired_image_s * next;
} retired_image_t;

typedef struct {
    int blend, depth, cull, depth_write, offset;
    unsigned src, dst, depth_func, face;
    float near_value, far_value, offset_factor, offset_units;
    int x, y, w, h;
} render_state_t;

struct xenr_mesh_s {
    struct XenosVertexBuffer * vertices;
    size_t count;
};

extern cvar_t vid_overscan;

static struct XenosDevice device;
static struct XenosSurface * scene, * output[2];
static struct XenosShader * game_vs, * game_ps, * present_vs, * present_ps;
static struct XenosShader * texture_ps, * solid_ps;
static struct XenosShader * solid_fog_ps;
static struct XenosShader * game_alpha_ps, * texture_alpha_ps;
static struct XenosVertexBuffer * vertices;
static struct XenosIndexBuffer * index_buffer;
static xenr_texture_t textures[MAX_TEXTURES];
static retired_image_t * retired_images;
static size_t retired_bytes;
static render_state_t state;
static size_t vertex_used;
static size_t index_used;
static unsigned bound_texture, output_index, last_error;
static int initialized, active;
static int state_valid, constants_valid;
static render_state_t applied_state;
static xenr_uniforms_t vertex_constants, pixel_constants;
static struct XenosSurface * sampled_image;
static int sampler_valid, sampled_min, sampled_mag, sampled_mips;
static unsigned frame_draws, frame_vertices, frame_uploads, frame_copies, frame_syncs;
static unsigned frame_indexed, report_indexed;
static double frame_start, frame_wait;
static double report_start, report_cpu, report_present, report_wait, report_vblank;
static unsigned report_frames, report_draws, report_vertices, report_uploads, report_copies, report_syncs, reports;
static float clear_color[4], clear_depth = 1;
static const struct XenosVBFFormat vertex_format = {
    3, { { XE_USAGE_POSITION, 0, XE_TYPE_FLOAT3 },
         { XE_USAGE_TEXCOORD, 0, XE_TYPE_FLOAT2 },
         { XE_USAGE_COLOR, 0, XE_TYPE_FLOAT4 } }
};

static void
ShaderConstants(const xenr_shader_t * shader, int pixel)
{
    unsigned i;

    for (i = 0; i < shader->count; ++i) {
        const xenr_constant_t * constant = &shader->constants[i];

        if (pixel)
            Xe_SetPixelShaderConstantF(&device, constant->index, constant->value, 1);
        else
            Xe_SetVertexShaderConstantF(&device, constant->index, constant->value, 1);
    }
}

static struct XenosShader *
LoadShader(const xenr_shader_t * source, int pixel)
{
    struct XenosShader * shader = Xe_LoadShaderFromMemory(&device, (void *) source->data);

    if (!shader)
        Sys_Error("Xenos: cannot load shader");
    Xe_InstantiateShader(&device, shader, 0);
    if (!pixel)
        Xe_ShaderApplyVFetchPatches(&device, shader, 0, &vertex_format);
    return shader;
}

void
XENR_Finish(void)
{
    unsigned i;
    double start;

    if (!initialized)
        return;
    start = Sys_FloatTime();
    Xe_Sync(&device);
    frame_wait += Sys_FloatTime() - start;
    ++frame_syncs;
    while (retired_images) {
        retired_image_t * entry = retired_images;

        retired_images = entry->next;
        Xe_DestroyTexture(&device, entry->image);
        free(entry);
    }
    retired_bytes = 0;
    for (i = 0; i < MAX_TEXTURES; ++i)
        textures[i].referenced = 0;
}

static void
RetireImage(struct XenosSurface * image, int referenced)
{
    retired_image_t * entry;

    if (!image)
        return;
    if (!referenced) {
        Xe_DestroyTexture(&device, image);
        return;
    }
    if (retired_bytes + (size_t) image->wpitch * image->hpitch > 32 * 1024 * 1024) {
        XENR_Finish();
        Xe_DestroyTexture(&device, image);
        return;
    }
    entry = malloc(sizeof(*entry));
    if (!entry)
        Sys_Error("Xenos: texture retirement allocation failed");
    entry->image = image;
    entry->next = retired_images;
    retired_images = entry;
    retired_bytes += (size_t) image->wpitch * image->hpitch;
}

static int
Blend(unsigned mode)
{
    switch (mode) {
    case XENR_ZERO: return XE_BLEND_ZERO;
    case XENR_ONE: return XE_BLEND_ONE;
    case XENR_SRC_ALPHA: return XE_BLEND_SRCALPHA;
    case XENR_ONE_MINUS_SRC_ALPHA: return XE_BLEND_INVSRCALPHA;
    case XENR_SRC_COLOR: return XE_BLEND_SRCCOLOR;
    case XENR_ONE_MINUS_SRC_COLOR: return XE_BLEND_INVSRCCOLOR;
    case XENR_DST_COLOR: return XE_BLEND_DESTCOLOR;
    default: return XE_BLEND_ONE;
    }
}

static int
Depth(unsigned function)
{
    switch (function) {
    case XENR_EQUAL: return XE_CMP_EQUAL;
    case XENR_GEQUAL: return XE_CMP_GREATEREQUAL;
    default: return XE_CMP_LESSEQUAL;
    }
}

static void
ApplyState(void)
{
    if (state_valid && !memcmp(&state, &applied_state, sizeof(state)))
        return;
    Xe_SetZEnable(&device, state.depth);
    Xe_SetZWrite(&device, state.depth_write);
    Xe_SetZFunc(&device, Depth(state.depth_func));
    Xe_SetCullMode(&device, state.cull ? (state.face == XENR_FRONT ? XE_CULL_CCW : XE_CULL_CW) : XE_CULL_NONE);
    Xe_SetBlendControl(&device, state.blend ? Blend(state.src) : XE_BLEND_ONE, XE_BLENDOP_ADD,
      state.blend ? Blend(state.dst) : XE_BLEND_ZERO, XE_BLEND_ONE, XE_BLENDOP_ADD,
      state.blend ? Blend(state.dst) : XE_BLEND_ZERO);
    Xe_SetAlphaTestEnable(&device, 0);
    Xe_SetScissor(&device, 1, state.x, HEIGHT - state.y - state.h,
      state.x + state.w, HEIGHT - state.y);
    applied_state = state;
    state_valid = 1;
}

static void
ResetVertices(void)
{
    Xe_VB_Lock(&device, vertices, 0, VERTEX_BYTES, XE_LOCK_WRITE);
    Xe_VB_Unlock(&device, vertices);
    vertex_used = 0;
}

static void
ResetIndices(void)
{
    Xe_IB_Lock(&device, index_buffer, 0, INDEX_BYTES, XE_LOCK_WRITE);
    Xe_IB_Unlock(&device, index_buffer);
    index_used = 0;
}

static void
RecordDraw(size_t count)
{
    ++frame_draws;
    frame_vertices += count;
    if (!(frame_draws & 15))
        Xe_Execute(&device);
}

void
XENR_BeginFrame(void)
{
    if (active)
        return;
    Xe_InvalidateState(&device);
    Xe_SetRenderTarget(&device, scene);
    ResetVertices();
    ResetIndices();
    state_valid = constants_valid = 0;
    sampler_valid = 0;
    frame_draws = frame_vertices = frame_uploads = frame_copies = frame_syncs = 0;
    frame_indexed = 0;
    frame_wait = 0;
    frame_start = Sys_FloatTime();
    active = 1;
}

static void
Draw(unsigned primitive, const xenr_vertex_t * data, size_t count)
{
    size_t bytes = count * sizeof(*data);
    void * target;
    int type, primitives;

    if (bytes > VERTEX_BYTES)
        Sys_Error("Xenos: oversized vertex batch");
    if (vertex_used + bytes > VERTEX_BYTES) {
        XENR_Finish();
        ResetVertices();
    }
    target = (byte *) vertices->base + vertex_used;
    memcpy(target, data, bytes);
    memdcbst(target, bytes);
    Xe_SetStreamSource(&device, 0, vertices, vertex_used, sizeof(*data));
    type = primitive == XENR_TRIANGLE_STRIP ? XE_PRIMTYPE_TRIANGLESTRIP :
      primitive == XENR_TRIANGLE_FAN ? XE_PRIMTYPE_TRIANGLEFAN :
      primitive == XENR_LINE_STRIP ? XE_PRIMTYPE_LINESTRIP : XE_PRIMTYPE_TRIANGLELIST;
    primitives = type == XE_PRIMTYPE_TRIANGLELIST ? count / 3 :
      type == XE_PRIMTYPE_LINESTRIP ? count - 1 : count - 2;
    if (primitives > 0)
        Xe_DrawPrimitive(&device, type, 0, primitives);
    vertex_used = (vertex_used + bytes + 255) & ~(size_t) 255;
    RecordDraw(count);
}

static size_t
TiledOffset(unsigned x, unsigned y, unsigned pitch)
{
    unsigned blocks = (((y >> 5) * (pitch >> 5) + (x >> 5)) << 6) |
      (((y >> 1) & 7) << 3) | (x & 7);
    unsigned bytes = blocks << 2;
    unsigned bank = (y >> 4) & 1;
    unsigned pipe = ((x >> 3) & 3) ^ (((y >> 3) & 1) << 1);

    return ((y & 1) << 4) | (pipe << 6) | (bank << 11) | (bytes & 15) |
      (((bytes >> 4) & 1) << 5) | (((bytes >> 5) & 7) << 8) | ((bytes >> 8) << 12);
}

static void
BindTexture(struct XenosSurface * image, int min_filter, int mag_filter, int mip_count)
{
    if (sampler_valid && sampled_image == image && sampled_min == min_filter &&
      sampled_mag == mag_filter && sampled_mips == mip_count)
        return;
    unsigned min_linear = min_filter == XENR_LINEAR || min_filter == XENR_LINEAR_MIPMAP_NEAREST ||
      min_filter == XENR_LINEAR_MIPMAP_LINEAR;
    unsigned mip_linear = min_filter == XENR_LINEAR_MIPMAP_LINEAR || min_filter == XENR_NEAREST_MIPMAP_LINEAR;
    unsigned levels = min_filter == XENR_LINEAR || min_filter == XENR_NEAREST || COM_CheckParm("-nomipmaps") ? 1 : mip_count;
    unsigned mip_filter = levels > 1 ? mip_linear : 2;

    Xe_SetTexture(&device, 0, image);
    device.fetch_constants[3] = (device.fetch_constants[3] & ~(511u << 19)) |
      ((mag_filter != XENR_NEAREST) << 19) | (min_linear << 21) | (mip_filter << 23);
    device.fetch_constants[4] = (levels - 1) << 6;
    device.fetch_constants[5] &= ~0x800u;
    Xe_DirtyFetch(&device, 0, 3);
    sampled_image = image;
    sampled_min = min_filter;
    sampled_mag = mag_filter;
    sampled_mips = mip_count;
    sampler_valid = 1;
}

static xenr_texture_t *
PrepareDraw(const xenr_uniforms_t * uniforms)
{
    float viewport[4], depth[4];
    xenr_uniforms_t transformed = *uniforms;
    int column;
    xenr_texture_t * texture = &textures[bound_texture];
    int alpha = uniforms->params[2] >= 0;
    struct XenosShader * fragment = uniforms->flags[1] ?
      (uniforms->params[3] ? (alpha ? game_alpha_ps : game_ps) : solid_fog_ps) :
      uniforms->params[3] ? (alpha ? texture_alpha_ps : texture_ps) : solid_ps;
    const xenr_shader_t * fragment_source = uniforms->flags[1] ?
      (uniforms->params[3] ? (alpha ? &hyena_fog_alpha_frag : &hyena_frag) : &hyena_solid_fog_frag) :
      uniforms->params[3] ? (alpha ? &hyena_alpha_frag : &hyena_texture_frag) : &hyena_solid_frag;

    XENR_BeginFrame();
    ApplyState();
    if (device.ps != fragment)
        constants_valid = 0;
    Xe_SetShader(&device, SHADER_TYPE_VERTEX, game_vs, 0);
    Xe_SetShader(&device, SHADER_TYPE_PIXEL, fragment, 0);
    if (!constants_valid) {
        ShaderConstants(&hyena_vert, 0);
        ShaderConstants(fragment_source, 1);
    }
    if (!constants_valid || memcmp(pixel_constants.fog_color, uniforms->fog_color, 12 * sizeof(float))) {
        Xe_SetPixelShaderConstantF(&device, 9, uniforms->fog_color, 3);
        pixel_constants = *uniforms;
    }
    viewport[0] = (float) state.w / WIDTH;
    viewport[1] = (float) state.h / HEIGHT;
    viewport[2] = (float) (2 * state.x + state.w) / WIDTH - 1;
    viewport[3] = (float) (2 * state.y + state.h) / HEIGHT - 1;
    depth[0] = state.near_value;
    depth[1] = state.far_value - state.near_value;
    depth[2] = state.offset ? state.offset_units / 16777216.0f + state.offset_factor * 0.00001f : 0;
    depth[3] = 0;
    for (column = 0; column < 4; ++column) {
        const float * source = uniforms->mvp + column * 4;
        float * target = transformed.mvp + column * 4;

        target[0] = source[0] * viewport[0] + source[3] * (viewport[2] - 1.0f / WIDTH);
        target[1] = source[1] * viewport[1] + source[3] * (viewport[3] + 1.0f / HEIGHT);
        target[2] = (source[2] + source[3]) * 0.5f * depth[1] + source[3] * (depth[0] + depth[2]);
    }
    if (!constants_valid || memcmp(&vertex_constants, &transformed, 36 * sizeof(float))) {
        Xe_SetVertexShaderConstantF(&device, 0, (const float *) &transformed, 9);
        vertex_constants = transformed;
    }
    constants_valid = 1;
    if (!texture->image)
        texture = &textures[0];
    BindTexture(texture->image, texture->min_filter, texture->mag_filter, texture->mip_count);
    return texture;
}

void
XENR_Draw(unsigned primitive, const xenr_vertex_t * data, size_t count, const xenr_uniforms_t * uniforms)
{
    xenr_texture_t * texture;

    if (count < 2)
        return;
    texture = PrepareDraw(uniforms);
    Draw(primitive, data, count);
    texture->referenced = 1;
}

xenr_mesh_t *
XENR_CreateMesh(const xenr_vertex_t * data, size_t count)
{
    xenr_mesh_t * mesh = calloc(1, sizeof(*mesh));
    size_t bytes = count * sizeof(*data);
    void * target;

    if (!mesh || !count || count > 65536)
        Sys_Error("Xenos: invalid mesh allocation");
    mesh->vertices = Xe_CreateVertexBuffer(&device, (bytes + 4095) & ~(size_t) 4095);
    mesh->count = count;
    target = Xe_VB_Lock(&device, mesh->vertices, 0, bytes, XE_LOCK_WRITE);
    memcpy(target, data, bytes);
    Xe_VB_Unlock(&device, mesh->vertices);
    return mesh;
}

void
XENR_DestroyMesh(xenr_mesh_t * mesh)
{
    if (!mesh)
        return;
    XENR_Finish();
    Xe_DestroyVertexBuffer(&device, mesh->vertices);
    free(mesh);
}

static void
DrawIndexed(struct XenosVertexBuffer * buffer, size_t offset, size_t vertex_count,
  const unsigned short * indices, size_t count)
{
    while (count) {
        size_t chunk = min(count, (size_t) 65535);
        size_t bytes = chunk * sizeof(*indices);
        void * target;

        if (index_used + bytes > INDEX_BYTES) {
            XENR_Finish();
            ResetIndices();
        }
        target = (byte *) index_buffer->base + index_used;
        memcpy(target, indices, bytes);
        memdcbst(target, bytes);
        Xe_SetStreamSource(&device, 0, buffer, offset, sizeof(xenr_vertex_t));
        Xe_SetIndices(&device, index_buffer);
        Xe_DrawIndexedPrimitive(&device, XE_PRIMTYPE_TRIANGLELIST, 0, 0, vertex_count,
          index_used / sizeof(*indices), chunk / 3);
        index_used = (index_used + bytes + 255) & ~(size_t) 255;
        frame_indexed += chunk;
        RecordDraw(chunk);
        indices += chunk;
        count -= chunk;
    }
}

void
XENR_DrawMesh(xenr_mesh_t * mesh, const unsigned short * indices, size_t count, const xenr_uniforms_t * uniforms)
{
    xenr_texture_t * texture;

    if (!mesh || count < 3)
        return;
    texture = PrepareDraw(uniforms);
    DrawIndexed(mesh->vertices, 0, mesh->count, indices, count - count % 3);
    texture->referenced = 1;
}

void
XENR_DrawIndexed(const xenr_vertex_t * data, size_t vertex_count, const unsigned short * indices,
  size_t count, const xenr_uniforms_t * uniforms)
{
    xenr_texture_t * texture;
    size_t bytes = vertex_count * sizeof(*data);
    size_t offset;
    void * target;

    if (!vertex_count || count < 3)
        return;
    if (bytes > VERTEX_BYTES)
        Sys_Error("Xenos: oversized indexed vertex batch");
    texture = PrepareDraw(uniforms);
    if (vertex_used + bytes > VERTEX_BYTES) {
        XENR_Finish();
        ResetVertices();
    }
    offset = vertex_used;
    target = (byte *) vertices->base + offset;
    memcpy(target, data, bytes);
    memdcbst(target, bytes);
    vertex_used = (vertex_used + bytes + 255) & ~(size_t) 255;
    DrawIndexed(vertices, offset, vertex_count, indices, count - count % 3);
    texture->referenced = 1;
}

void
XENR_Enable(unsigned cap)
{
    switch (cap) {
    case XENR_BLEND: state.blend = 1; break;
    case XENR_DEPTH_TEST: state.depth = 1; break;
    case XENR_CULL_FACE: state.cull = 1; break;
    case XENR_POLYGON_OFFSET_FILL: state.offset = 1; break;
    }
}

void
XENR_Disable(unsigned cap)
{
    switch (cap) {
    case XENR_BLEND: state.blend = 0; break;
    case XENR_DEPTH_TEST: state.depth = 0; break;
    case XENR_CULL_FACE: state.cull = 0; break;
    case XENR_POLYGON_OFFSET_FILL: state.offset = 0; break;
    }
}

void
XENR_BlendFunc(unsigned src, unsigned dst)
{
    state.src = src;
    state.dst = dst;
}

void
XENR_DepthMask(int enabled)
{
    state.depth_write = enabled;
}

void
XENR_DepthFunc(unsigned function)
{
    state.depth_func = function;
}

void
XENR_DepthRange(float near_value, float far_value)
{
    state.near_value = near_value;
    state.far_value = far_value;
}

void
XENR_PolygonOffset(float factor, float units)
{
    state.offset_factor = factor;
    state.offset_units = units;
}

void
XENR_Viewport(int x, int y, int width, int height)
{
    state.x = x;
    state.y = y;
    state.w = width;
    state.h = height;
}

void
XENR_CullFace(unsigned face)
{
    state.face = face;
}

void
XENR_ClearColor(float r, float g, float b, float a)
{
    clear_color[0] = bound(0, r, 1);
    clear_color[1] = bound(0, g, 1);
    clear_color[2] = bound(0, b, 1);
    clear_color[3] = bound(0, a, 1);
}

void
XENR_ClearDepth(float depth)
{
    clear_depth = bound(0, depth, 1);
}

void
XENR_Clear(unsigned mask)
{
    XENR_BeginFrame();
    Xe_SetClearColor(&device, ((unsigned) (clear_color[3] * 255) << 24) |
      ((unsigned) (clear_color[0] * 255) << 16) | ((unsigned) (clear_color[1] * 255) << 8) |
      (unsigned) (clear_color[2] * 255));
    device.clear_stencil_z = (unsigned) (clear_depth * 16777215) << 8;
    Xe_Clear(&device, (mask & XENR_COLOR_BUFFER_BIT ? XE_CLEAR_COLOR : 0) |
      (mask & (XENR_DEPTH_BUFFER_BIT | XENR_STENCIL_BUFFER_BIT) ? XE_CLEAR_DS : 0));
    state_valid = constants_valid = 0;
    sampler_valid = 0;
}

unsigned
XENR_CreateTexture(void)
{
    unsigned i;

    for (i = 1; i < MAX_TEXTURES; ++i) {
        if (!textures[i].used) {
            textures[i].used = 1;
            textures[i].min_filter = textures[i].mag_filter = XENR_LINEAR;
            return i;
        }
    }
    Sys_Error("Xenos: texture limit reached");
    return 0;
}

void
XENR_BindTexture(unsigned texture)
{
    if (texture >= MAX_TEXTURES) {
        last_error = XENR_INVALID_VALUE;
        return;
    }
    bound_texture = texture;
}

void
XENR_DestroyTexture(unsigned texture)
{
    sampler_valid = 0;
    if (!texture || texture >= MAX_TEXTURES)
        return;
    RetireImage(textures[texture].image, textures[texture].referenced);
    memset(&textures[texture], 0, sizeof(textures[texture]));
    if (bound_texture == texture)
        bound_texture = 0;
}

void
XENR_TextureFilter(int min_filter, int mag_filter)
{
    textures[bound_texture].min_filter = min_filter;
    textures[bound_texture].mag_filter = mag_filter;
}

void
XENR_Mipmaps(int enabled)
{
    textures[bound_texture].mipmaps = enabled;
}

void
XENR_Lightmap(int enabled)
{
    textures[bound_texture].lightmap = enabled;
}

static unsigned
PowerOfTwo(unsigned value)
{
    unsigned result = 1;

    while (result < value)
        result <<= 1;
    return result;
}

static struct XenosSurface *
CreateImage(xenr_texture_t * texture, unsigned width, unsigned height)
{
    unsigned level, count = 1;
    unsigned pitch = (width + 63) & ~63u;
    size_t bytes = (size_t) pitch * ((height + 31) & ~31u) * 4;
    struct XenosSurface * image;

    memset(texture->mip_offset, 0, sizeof(texture->mip_offset));
    memset(texture->mip_pitch, 0, sizeof(texture->mip_pitch));
    texture->mip_offset[0] = 0;
    texture->mip_pitch[0] = pitch;
    if (texture->mipmaps) {
        unsigned w = PowerOfTwo(width), h = PowerOfTwo(height);

        for (level = 1; (w > 1 || h > 1) && level < 16; ++level) {
            w = max(1, w >> 1);
            h = max(1, h >> 1);
            bytes = (bytes + 4095) & ~(size_t) 4095;
            texture->mip_offset[level] = bytes;
            texture->mip_pitch[level] = (w + 63) & ~63u;
            bytes += (size_t) texture->mip_pitch[level] * ((h + 31) & ~31u) * 4;
            ++count;
        }
    }
    image = Xe_CreateTexture(&device, pitch, (bytes + pitch * 4 - 1) / (pitch * 4), 1,
      XE_FMT_8888 | XE_FMT_ARGB, 0);
    {
        void * pixels = Xe_Surface_LockRect(&device, image, 0, 0, 0, 0, XE_LOCK_WRITE);

        memset(pixels, 0, (size_t) image->wpitch * image->hpitch);
        Xe_Surface_Unlock(&device, image);
    }
    image->width = width;
    image->height = height;
    image->ptr_mip = count > 1 ? image->ptr + texture->mip_offset[1] : 0;
    texture->mip_count = 1;
    return image;
}

void
XENR_Upload(int level, int storage, int width, int height, int x, int y, int replace, const void * pixels)
{
    xenr_texture_t * texture = &textures[bound_texture];
    byte * target;
    int row, column, rows = height, columns = width;
    unsigned peak = 0, lit = 0;

    sampler_valid = 0;
    if (replace)
        x = y = 0;
    if (level < 0 || level >= 16 || width < 1 || height < 1 || width > 4096 || height > 4096 || x < 0 || y < 0) {
        last_error = XENR_INVALID_VALUE;
        return;
    }
    if (level && (!texture->mipmaps || !texture->image || !texture->mip_offset[level])) {
        last_error = XENR_INVALID_VALUE;
        return;
    }
    if (texture->image && (!replace || level) &&
      (x + width > max(1, texture->image->width >> level) || y + height > max(1, texture->image->height >> level))) {
        last_error = XENR_INVALID_VALUE;
        return;
    }
    if (!level && replace) {
        struct XenosSurface * previous = texture->image;
        int referenced = texture->referenced;

        texture->image = CreateImage(texture, width, height);
        texture->referenced = 0;
        RetireImage(previous, referenced);
    } else if (texture->referenced) {
        struct XenosSurface * previous = texture->image;
        int referenced = texture->referenced;
        void * copy;

        texture->image = Xe_CreateTexture(&device, previous->wpitch / 4, previous->hpitch, 1,
          previous->format, previous->tiled);
        texture->image->width = previous->width;
        texture->image->height = previous->height;
        texture->image->ptr_mip = previous->ptr_mip ? texture->image->ptr + texture->mip_offset[1] : 0;
        copy = Xe_Surface_LockRect(&device, texture->image, 0, 0, 0, 0, XE_LOCK_WRITE);
        memcpy(copy, previous->base, (size_t) previous->wpitch * previous->hpitch);
        frame_copies += previous->wpitch * previous->hpitch;
        Xe_Surface_Unlock(&device, texture->image);
        texture->referenced = 0;
        RetireImage(previous, referenced);
    }
    if (!texture->image) {
        last_error = XENR_INVALID_OPERATION;
        return;
    }
    if (x + width == max(1, texture->image->width >> level))
        columns = texture->mip_pitch[level] - x;
    if (y + height == max(1, texture->image->height >> level)) {
        unsigned storage_height = level ? max(1, PowerOfTwo(texture->image->height) >> level) : texture->image->height;

        rows = ((storage_height + 31) & ~31u) - y;
    }
    if (!level && texture->image->height == texture->image->hpitch) {
        target = Xe_Surface_LockRect(&device, texture->image, 0, y, texture->image->width, rows, XE_LOCK_WRITE);
        target -= (size_t) y * texture->image->wpitch;
    } else
        target = Xe_Surface_LockRect(&device, texture->image, 0, 0, 0, 0, XE_LOCK_WRITE);
    if (storage == HYE_PIXELS_RGBA8 && pixels) {
        for (row = 0; row < rows; ++row) {
            const byte * source = (const byte *) pixels + (size_t) min(row, height - 1) * width * 4;
            uint32_t * dest = (uint32_t *) (target + texture->mip_offset[level]) +
              (size_t) (y + row) * texture->mip_pitch[level] + x;

            for (column = 0; column < width; ++column) {
                uint32_t value;

                memcpy(&value, source + column * 4, sizeof(value));
                #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                dest[column] = (value >> 8) | (value << 24);
                #else
                dest[column] = (value & 0xff00ff00u) | ((value & 255) << 16) | ((value >> 16) & 255);
                #endif
            }
            for (; column < columns; ++column)
                dest[column] = dest[width - 1];
        }
    } else {
        for (row = 0; row < rows; ++row) {
            for (column = 0; column < columns; ++column) {
                size_t index = (size_t) min(row, height - 1) * width + min(column, width - 1);
                const byte * source = pixels;
                unsigned r = 255, g = 255, b = 255, a = 255;

                if (!pixels)
                    r = g = b = a = 0;
                else if (storage == HYE_PIXELS_RGBA8) {
                    source += index * 4;
                    r = source[0]; g = source[1]; b = source[2]; a = source[3];
                } else if (storage == HYE_PIXELS_L8)
                    r = g = b = source[index];
                else if (storage == HYE_PIXELS_RGBA5551) {
                    unsigned value = ((const unsigned short *) pixels)[index];

                    r = (value & 31) * 255 / 31;
                    g = ((value >> 5) & 31) * 255 / 31;
                    b = ((value >> 10) & 31) * 255 / 31;
                    a = value & 32768 ? 255 : 0;
                } else if (storage == HYE_PIXELS_RGBA4444) {
                    unsigned value = ((const unsigned short *) pixels)[index];

                    r = (value >> 12) * 17;
                    g = ((value >> 8) & 15) * 17;
                    b = ((value >> 4) & 15) * 17;
                    a = (value & 15) * 17;
                } else {
                    Sys_Error("Xenos: unsupported pixel storage %d", storage);
                }
                size_t offset = texture->mip_offset[level] +
                  ((size_t) (y + row) * texture->mip_pitch[level] + x + column) * 4;

                *(uint32_t *) (target + offset) = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
    if (texture->lightmap && texture->reports < 3)
        for (row = 0; row < height; ++row)
            for (column = 0; column < width; ++column) {
                uint32_t value = ((uint32_t *) (target + texture->mip_offset[level]))[
                  (size_t) (y + row) * texture->mip_pitch[level] + x + column];

                peak = max(peak, max((value >> 16) & 255, max((value >> 8) & 255, value & 255)));
                lit += (value & 0xffffffu) != 0;
            }
    Xe_Surface_Unlock(&device, texture->image);
    frame_uploads += width * height * 4;
    texture->mip_count = max(texture->mip_count, level + 1);
    if (texture->lightmap && texture->reports < 3) {
        Sys_Printf("lightmap %u: %dx%d at %d,%d replace=%d lit=%u/%u peak=%u\n",
          bound_texture, width, height, x, y, replace, lit, width * height, peak);
        ++texture->reports;
    }
}

unsigned
XENR_GetError(void)
{
    unsigned error = last_error;

    last_error = 0;
    return error;
}

int
XENR_ReadPixels(int x, int y, int width, int height, unsigned format, unsigned type, void * pixels)
{
    int row, column, channels = format == XENR_RGBA ? 4 : 3;
    const byte * source;
    byte * target = pixels;

    if (!initialized || !pixels || x < 0 || y < 0 || width < 1 || height < 1 ||
      x + width > WIDTH || y + height > HEIGHT || type != XENR_UNSIGNED_BYTE ||
      (format != XENR_RGB && format != XENR_RGBA)) {
        last_error = XENR_INVALID_VALUE;
        return 0;
    }
    if (active) {
        Xe_ResolveInto(&device, scene, XE_SOURCE_COLOR, 0);
        state_valid = constants_valid = sampler_valid = 0;
    }
    XENR_Finish();
    source = Xe_Surface_LockRect(&device, scene, 0, 0, 0, 0, XE_LOCK_READ);
    for (row = 0; row < height; ++row) {
        for (column = 0; column < width; ++column) {
            size_t offset = TiledOffset(x + column, HEIGHT - 1 - y - row, scene->wpitch / 4);
            uint32_t value = *(const uint32_t *) (source + offset);

            *target++ = value >> 16;
            *target++ = value >> 8;
            *target++ = value;
            if (channels == 4)
                *target++ = value >> 24;
        }
    }
    Xe_Surface_Unlock(&device, scene);
    return 1;
}

const char *
X360_GraphicsError(void)
{
    return "1280x720 HDMI graphics initialization failed";
}

int
X360_GraphicsInit(void)
{
    if (initialized)
        return 1;
    xenos_init(VIDEO_MODE_HDMI_720P);
    console_init();
    console_close();
    Xe_Init(&device);
    scene = Xe_CreateTexture(&device, WIDTH, HEIGHT, 1, XE_FMT_8888 | XE_FMT_ARGB, 1);
    output[0] = Xe_CreateTexture(&device, WIDTH, HEIGHT, 1, XE_FMT_8888 | XE_FMT_BGRA, 1);
    output[1] = Xe_CreateTexture(&device, WIDTH, HEIGHT, 1, XE_FMT_8888 | XE_FMT_BGRA, 1);
    vertices = Xe_CreateVertexBuffer(&device, VERTEX_BYTES);
    index_buffer = Xe_CreateIndexBuffer(&device, INDEX_BYTES, XE_FMT_INDEX16);
    if (!scene || !output[0] || !output[1] || !vertices || !index_buffer)
        return 0;
    game_vs = LoadShader(&hyena_vert, 0);
    game_ps = LoadShader(&hyena_frag, 1);
    texture_ps = LoadShader(&hyena_texture_frag, 1);
    solid_ps = LoadShader(&hyena_solid_frag, 1);
    solid_fog_ps = LoadShader(&hyena_solid_fog_frag, 1);
    game_alpha_ps = LoadShader(&hyena_fog_alpha_frag, 1);
    texture_alpha_ps = LoadShader(&hyena_alpha_frag, 1);
    present_vs = LoadShader(&present_vert, 0);
    present_ps = LoadShader(&present_frag, 1);
    Xe_SetRenderTarget(&device, scene);
    edram_init(&device);
    initialized = 1;
    state.w = WIDTH;
    state.h = HEIGHT;
    state.far_value = 1;
    state.depth_write = 1;
    state.depth_func = XENR_LEQUAL;
    textures[0].used = 1;
    textures[0].mag_filter = XENR_NEAREST;
    {
        byte white[] = { 255, 255, 255, 255 };

        XENR_Upload(0, HYE_PIXELS_RGBA8, 1, 1, 0, 0, 1, white);
    }
    XENR_Clear(XENR_COLOR_BUFFER_BIT | XENR_DEPTH_BUFFER_BIT);
    return 1;
}

int
X360_GraphicsSwap(int vsync)
{
    xenr_vertex_t quad[] = {
        { { -1, 1, 0 }, { 0, 0 }, { 1, 1, 1, 1 } },
        { { 1, 1, 0 }, { 1, 0 }, { 1, 1, 1, 1 } },
        { { -1, -1, 0 }, { 0, 1 }, { 1, 1, 1, 1 } },
        { { 1, -1, 0 }, { 1, 1 }, { 1, 1, 1, 1 } }
    };

    float scale = 1 - 2 * bound(0, vid_overscan.value, 10) * 0.01f;
    unsigned i;
    double cpu_done, present_done, wait_done, done;

    for (i = 0; i < 4; ++i) {
        quad[i].position[0] *= scale;
        quad[i].position[1] *= scale;
    }
    XENR_BeginFrame();
    cpu_done = Sys_FloatTime();
    Xe_ResolveInto(&device, scene, XE_SOURCE_COLOR, 0);
    sampler_valid = 0;
    Xe_InvalidateState(&device);
    Xe_SetRenderTarget(&device, output[output_index]);
    Xe_SetClearColor(&device, 0xff000000);
    Xe_Clear(&device, XE_CLEAR_COLOR);
    Xe_SetZEnable(&device, 0);
    Xe_SetZWrite(&device, 0);
    Xe_SetCullMode(&device, XE_CULL_NONE);
    Xe_SetBlendControl(&device, XE_BLEND_ONE, XE_BLENDOP_ADD, XE_BLEND_ZERO,
      XE_BLEND_ONE, XE_BLENDOP_ADD, XE_BLEND_ZERO);
    Xe_SetScissor(&device, 0, 0, 0, WIDTH, HEIGHT);
    Xe_SetShader(&device, SHADER_TYPE_VERTEX, present_vs, 0);
    Xe_SetShader(&device, SHADER_TYPE_PIXEL, present_ps, 0);
    ShaderConstants(&present_vert, 0);
    ShaderConstants(&present_frag, 1);
    scene->u_addressing = scene->v_addressing = XE_TEXADDR_CLAMP;
    BindTexture(scene, XENR_LINEAR, XENR_LINEAR, 1);
    Draw(XENR_TRIANGLE_STRIP, quad, 4);
    Xe_Resolve(&device);
    present_done = Sys_FloatTime();
    XENR_Finish();
    wait_done = Sys_FloatTime();
    if (vsync) {
        while (!Xe_IsVBlank(&device))
            udelay(50);
    }
    Xe_SetFrameBufferSurface(&device, output[output_index]);
    output_index ^= 1;
    active = 0;
    done = Sys_FloatTime();
    if (!report_start)
        report_start = done;
    else {
        ++report_frames;
        report_cpu += cpu_done - frame_start;
        report_present += present_done - cpu_done;
        report_wait += frame_wait;
        report_vblank += done - wait_done;
        report_draws += frame_draws;
        report_vertices += frame_vertices;
        report_indexed += frame_indexed;
        report_uploads += frame_uploads;
        report_copies += frame_copies;
        report_syncs += frame_syncs;
        if (reports < 60 && done - report_start >= 5) {
            double ms = 1000.0 / report_frames;

            Sys_Printf("render: fps=%.1f cpu=%.2fms present=%.2fms gpu_wait=%.2fms vblank=%.2fms draws=%u vertices=%u indexed=%u upload=%uKB copy=%uKB syncs=%u\n",
              report_frames / (done - report_start), report_cpu * ms, report_present * ms,
              report_wait * ms, report_vblank * ms, report_draws / report_frames,
              report_vertices / report_frames, report_indexed / report_frames, report_uploads / report_frames / 1024,
              report_copies / report_frames / 1024, report_syncs / report_frames);
            report_start = done;
            report_frames = report_draws = report_vertices = report_uploads = report_copies = report_syncs = 0;
            report_indexed = 0;
            report_cpu = report_present = report_wait = report_vblank = 0;
            ++reports;
        }
    }
    return 1;
}

void
X360_GraphicsShowError(const char * error)
{
    if (initialized)
        XENR_Finish();
    if (!xenos_is_initialized())
        xenos_init(VIDEO_MODE_AUTO);
    console_init();
    printf("%s\n", error);
}

void
X360_GraphicsShutdown(void)
{
    unsigned i;

    if (!initialized)
        return;
    XENR_Finish();
    for (i = 0; i < MAX_TEXTURES; ++i) {
        if (textures[i].image)
            Xe_DestroyTexture(&device, textures[i].image);
    }
    Xe_DestroyVertexBuffer(&device, vertices);
    Xe_DestroyIndexBuffer(&device, index_buffer);
    Xe_DestroyTexture(&device, scene);
    initialized = active = 0;
}
