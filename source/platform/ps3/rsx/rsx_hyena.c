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
#include "rsx_main.h"
#include <math.h>
#include <string.h>

static void
PS3_MatrixIdentity(float * m)
{
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1;
}

static void
PS3_MatrixMultiply(float * out, const float * a, const float * b)
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
PS3_MatrixRotation(float * m, float degrees, float x, float y, float z)
{
    float length = sqrtf(x * x + y * y + z * z);
    float c      = cosf(degrees * 0.017453292519943295f);
    float s = sinf(degrees * 0.017453292519943295f), t = 1 - c;

    PS3_MatrixIdentity(m);

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

void
PS3_HyenaInit(void)
{
    PS3_MatrixIdentity(matrices[0][0]);
    PS3_MatrixIdentity(matrices[1][0]);
}

void
PS3_HyenaShutdown(void)
{
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
    PS3_MatrixIdentity(CurrentMatrix());
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

    PS3_MatrixIdentity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
    PS3_MatrixMultiply(CurrentMatrix(), CurrentMatrix(), m);
}

void
Hyena_MatrixScale(float x, float y, float z)
{
    float m[16];

    PS3_MatrixIdentity(m);
    m[0]  = x;
    m[5]  = y;
    m[10] = z;
    PS3_MatrixMultiply(CurrentMatrix(), CurrentMatrix(), m);
}

void
Hyena_MatrixRotate(float degrees, float x, float y, float z)
{
    float m[16];

    PS3_MatrixRotation(m, degrees, x, y, z);
    PS3_MatrixMultiply(CurrentMatrix(), CurrentMatrix(), m);
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
    PS3_MatrixMultiply(CurrentMatrix(), CurrentMatrix(), m);
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
            RSXR_Enable(RSXR_BLEND);
            break;
        case HYE_DEPTH_TEST:
            RSXR_Enable(RSXR_DEPTH_TEST);
            break;
        case HYE_CULL_FACE:
            RSXR_Enable(RSXR_CULL_FACE);
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
            RSXR_Disable(RSXR_BLEND);
            break;
        case HYE_DEPTH_TEST:
            RSXR_Disable(RSXR_DEPTH_TEST);
            break;
        case HYE_CULL_FACE:
            RSXR_Disable(RSXR_CULL_FACE);
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
            return RSXR_ZERO;

        case HYE_ONE:
            return RSXR_ONE;

        case HYE_SRC_ALPHA:
            return RSXR_SRC_ALPHA;

        case HYE_ONE_MINUS_SRC_ALPHA:
            return RSXR_ONE_MINUS_SRC_ALPHA;

        case HYE_DST_COLOR:
            return RSXR_DST_COLOR;

        case HYE_SRC_COLOR:
            return RSXR_SRC_COLOR;

        case HYE_ONE_MINUS_SRC_COLOR:
            return RSXR_ONE_MINUS_SRC_COLOR;

        default:
            Sys_Error("Hyena: invalid blend %d", blend);
    }
}

void
Hyena_SetBlendFunction(int src, int dst)
{
    hyena_blend_alpha = 1;
    RSXR_BlendFunc(Blend(src), Blend(dst));
}

void
Hyena_BlendConstant(float alpha)
{
    hyena_blend_alpha = alpha;
    RSXR_BlendFunc(RSXR_SRC_ALPHA, RSXR_ONE_MINUS_SRC_ALPHA);
}

void
Hyena_DepthMask(qboolean enabled)
{
    RSXR_DepthMask(enabled);
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

    RSXR_DepthRange(gldepthmin + n * range, gldepthmin + f * range);
}

void
Hyena_ResetDepthRange(void)
{
    Hyena_SetDepthRange(0.3f, 1);
}

void
Hyena_DepthFunction(int fn)
{
    RSXR_DepthFunc(fn == HYE_EQUAL ? RSXR_EQUAL :
      fn == HYE_GEQUAL || gldepthmin > gldepthmax ? RSXR_GEQUAL :
      RSXR_LEQUAL);
}

void
Hyena_SetDepthOffset(float offset)
{
    if (offset) {
        RSXR_Enable(RSXR_POLYGON_OFFSET_FILL);
        RSXR_PolygonOffset(offset, offset);
    } else
        RSXR_Disable(RSXR_POLYGON_OFFSET_FILL);
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

    PS3_MatrixIdentity(m);
    m[0]  = 2.0f / vid.width;
    m[5]  = -2.0f / vid.height;
    m[10] = -2.0f / 199998;
    m[12] = -1;
    m[13] = 1;
    Hyena_MatrixMode(HYE_PROJECTION);
    Hyena_LoadMatrix(m);
    Hyena_MatrixMode(HYE_MODELVIEW);
    Hyena_LoadIdentity();
    RSXR_Viewport(glx, gly, glwidth, glheight);
    RSXR_Disable(RSXR_DEPTH_TEST);
    RSXR_Disable(RSXR_CULL_FACE);
}

void
Hyena_LoadViewMatrix(const float * m)
{
    Hyena_LoadMatrix(m);
}

void
Hyena_Viewport(int x, int y, int w, int h)
{
    RSXR_Viewport(x, y, w, h);
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
    RSXR_CullFace(back ? RSXR_BACK : RSXR_FRONT);
}

void
Hyena_ClearColor(float r, float g, float b, float a)
{
    RSXR_ClearColor(r, g, b, a);
}

void
Hyena_Clear(qboolean color, qboolean depth, qboolean stencil)
{
    RSXR_Clear((color ? RSXR_COLOR_BUFFER_BIT : 0) | (depth ? RSXR_DEPTH_BUFFER_BIT : 0)
      | (stencil ? RSXR_STENCIL_BUFFER_BIT : 0));
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
    RSXR_SelectReadBuffer(front);
}

void
Hyena_Finish(void)
{
    RSXR_Finish();
}

qboolean
Hyena_ReadPixels(int x, int y, int w, int h, byte * rgba)
{
    return RSXR_ReadPixels(x, y, w, h, RSXR_RGBA, RSXR_UNSIGNED_BYTE, rgba);
}

void
Hyena_TextureFilter(int filter)
{
    int mode = filter == HYE_FILTER_LINEAR ? RSXR_LINEAR : RSXR_NEAREST;

    RSXR_TextureFilter(mode, mode);
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
    static const int filters[] = { RSXR_NEAREST,
                                   RSXR_LINEAR,
                                   RSXR_NEAREST_MIPMAP_NEAREST,
                                   RSXR_LINEAR_MIPMAP_NEAREST,
                                   RSXR_NEAREST_MIPMAP_LINEAR,
                                   RSXR_LINEAR_MIPMAP_LINEAR };

    return filters[filter];
}

void
Hyena_BackendTextureFilter(int texture, int min_filter, int mag_filter)
{
    unsigned object = hyena_texture_objects[texture];

    if (current_texture_object != object) {
        RSXR_BindTexture(object);
        current_texture_object = object;
    }

    RSXR_TextureFilter(Hyena_NativeFilter(min_filter), Hyena_NativeFilter(mag_filter));
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
    RSXR_DestroyTexture(hyena_texture_objects[texture]);
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
Hyena_BackendUploadLevel(int texture, int level, int storage, int width, int height, int x, int y,
  qboolean replace, const void * pixels)
{
    RSXR_BindTexture(hyena_texture_objects[texture]);
    current_texture_object = hyena_texture_objects[texture];
    RSXR_Upload(level, storage, width, height, x, y, replace, pixels);
}

void
Hyena_BackendUploadTexture(int texture, const hyena_texture_desc_t * desc)
{
    if (!hyena_textures[texture].used)
        hyena_texture_objects[texture] = RSXR_CreateTexture();

    RSXR_BindTexture(hyena_texture_objects[texture]);
    RSXR_Mipmaps(desc->mip_levels > 0 && !desc->lightmap);
    RSXR_DynamicTexture(desc->lightmap);
    Hyena_PrepareTextureUpload(texture, desc, HYE_PIXELS_RGBA5551, Hyena_BackendUploadLevel);
}

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

    if (buffer->owned && data && size)
        RSXR_UploadBuffer(buffer);
}

void
Hyena_DestroyBuffer(hyena_buffer_t * buffer)
{
    if (!buffer)
        return;

    RSXR_FreeBuffer(buffer);

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
    if (buffer->owned && buffer->size)
        RSXR_UploadBuffer(buffer);
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

static void
Submit(int mode, const hyena_arrays_t * arrays, int count, const unsigned short * indices)
{
    float mvp[16];

    if (count <= 0)
        return;

    PS3_MatrixMultiply(mvp, matrices[1][matrix_depth[1]], matrices[0][matrix_depth[0]]);
    rsxr_uniforms_t uniforms = { 0 };

    memcpy(uniforms.mvp, mvp, sizeof(mvp));
    memcpy(uniforms.modelview, matrices[0][matrix_depth[0]], sizeof(mvp));
    memcpy(uniforms.uv, texture_transform, sizeof(texture_transform));
    memcpy(uniforms.fog_color, fog_color, sizeof(fog_color));
    uniforms.params[0]       = fog_start;
    uniforms.params[1]       = fog_end;
    uniforms.params[2]       = alpha_test ? alpha_threshold : -1;
    uniforms.params[3]       = textured;
    uniforms.flags[0]        = modulate;
    uniforms.flags[1]        = fog_enabled;
    uniforms.vertex_state[0] = arrays->scale ? arrays->scale : 1;
    uniforms.vertex_state[1] = hyena_blend_alpha;
    RSXR_Submit(mode, arrays, count, indices, &uniforms, current_color, flat_shading);
}

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
        RSXR_Enable(RSXR_BLEND);
        RSXR_BlendFunc(RSXR_SRC_ALPHA, RSXR_ONE_MINUS_SRC_ALPHA);
    } else
        RSXR_Disable(RSXR_BLEND);

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
    RSXR_Disable(RSXR_BLEND);
}
