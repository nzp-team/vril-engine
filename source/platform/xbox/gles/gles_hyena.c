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

// gles_hyena.c -- Xbox GLES Pbkit backend
#include "../../../nzportable_def.h"
#include "../../../render/r_texture.h"
#include "../../../render/r_pixels.h"
#include "../xbox_local.h"
#include <mmintrin.h>
#include <xmmintrin.h>

// Blargh, Windows (or minGW?)
#ifndef GL_UNSIGNED_SHORT_4_4_4_4
#define GL_UNSIGNED_SHORT_4_4_4_4 0x8033
#endif
#ifndef GL_UNSIGNED_SHORT_5_5_5_1
#define GL_UNSIGNED_SHORT_5_5_5_1 0x8034
#endif

static float hyena_blend_alpha = 1;
static qboolean hyena_read_front;
static int hyena_vertex_mode;
static vec3_t hyena_translation;
static vec3_t hyena_scale;
static hyena_colored_vertex_t * hyena_2d_vertices;
static vertex_t * hyena_vertices;
static int hyena_vertex_capacity;
static int hyena_2d_capacity;

static unsigned hyena_array_generation;
static void
Hyena_InvalidateArrays(void)
{
    ++hyena_array_generation;
}

#define HYENA_STREAM_BLOCK  (256u * 1024u)
#define HYENA_STREAM_BUDGET (2u * 1024u * 1024u)
#define HYENA_QUAD_BATCH    1024

typedef struct hyena_stream_s {
    GLuint                  object;
    size_t                  size, used;
    struct hyena_stream_s * next;
} hyena_stream_t;

static hyena_stream_t * hyena_streams;
static size_t hyena_stream_bytes;
static GLuint hyena_stream_object;
static unsigned short hyena_quad_indices[HYENA_QUAD_BATCH * 6];
static unsigned short hyena_quad_scratch[HYENA_QUAD_BATCH * 6];
static qboolean hyena_quad_indices_ready;

void
Hyena_XboxBeginFrame(void)
{
    hyena_stream_t * stream;

    // glFlipNV2A completes the preceding frame before any storage is reused.
    for (stream = hyena_streams; stream; stream = stream->next)
        stream->used = 0;
}

void
Hyena_XboxShutdown(void)
{
    while (hyena_streams) {
        hyena_stream_t * next = hyena_streams->next;
        glDeleteBuffers(1, &hyena_streams->object);
        free(hyena_streams);
        hyena_streams = next;
    }
    hyena_stream_bytes = 0;
    hyena_stream_object = 0;
    Hyena_InvalidateArrays();
    free(hyena_vertices);
    free(hyena_2d_vertices);
    hyena_vertices        = NULL;
    hyena_2d_vertices     = NULL;
    hyena_vertex_capacity = hyena_2d_capacity = 0;
}

static size_t
Hyena_StreamVertices(const void * data, size_t size)
{
    hyena_stream_t ** link = &hyena_streams;
    size_t aligned         = (size + 15) & ~(size_t) 15;

    while (*link) {
        if ((*link)->size - (*link)->used >= aligned) break;
        link = &(*link)->next;
    }
    if (!*link) {
        size_t capacity = max(HYENA_STREAM_BLOCK, aligned);
        // Keep transient memory bounded. The exceptional overflow path waits
        // before reusing bytes that an earlier draw may still be reading.
        if (hyena_streams && hyena_stream_bytes + capacity > HYENA_STREAM_BUDGET) {
            hyena_stream_t * available;
            glFinish();
            Hyena_XboxBeginFrame();
            for (available = hyena_streams; available; available = available->next)
                if (available->size >= aligned) return Hyena_StreamVertices(data, size);

            // Existing blocks may all be too small for this batch. Retire only
            // GPU storage; the caller may be borrowing our CPU scratch arrays.
            while (hyena_streams) {
                hyena_stream_t * next = hyena_streams->next;
                glDeleteBuffers(1, &hyena_streams->object);
                free(hyena_streams);
                hyena_streams = next;
            }
            hyena_stream_bytes = 0;
            link = &hyena_streams;
        }
        *link = calloc(1, sizeof(**link));
        if (!*link) Sys_Error("Hyena: out of stream metadata");
        glGenBuffers(1, &(*link)->object);
        glBindBuffer(GL_ARRAY_BUFFER, (*link)->object);
        glBufferData(GL_ARRAY_BUFFER, capacity, NULL, GL_DYNAMIC_DRAW);
        if (glGetError() != GL_NO_ERROR) Sys_Error("Hyena: out of GPU stream memory");
        (*link)->size       = capacity;
        hyena_stream_bytes += capacity;
    }
    hyena_stream_t * stream = *link;
    size_t offset = stream->used;
    glBindBuffer(GL_ARRAY_BUFFER, stream->object);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
    hyena_stream_object = stream->object;
    stream->used += aligned;
    return offset;
} /* Hyena_StreamVertices */

static void
Hyena_QuadIndices(void)
{
    int quad;

    if (hyena_quad_indices_ready) return;

    for (quad = 0; quad < HYENA_QUAD_BATCH; ++quad) {
        unsigned short * out = hyena_quad_indices + quad * 6;
        out[0] = out[3] = quad * 4;
        out[1] = quad * 4 + 1;
        out[2] = out[4] = quad * 4 + 2;
        out[5] = quad * 4 + 3;
    }
    hyena_quad_indices_ready = true;
}

void
Hyena_SetColor(float r, float g, float b, float a)
{
    glColor4f(r, g, b, a * hyena_blend_alpha);
}

void
Hyena_BeginVertices(int mode)
{
    hyena_vertex_mode = mode;
    VectorClear(hyena_translation);
    hyena_scale[0] = hyena_scale[1] = hyena_scale[2] = 1.0f;
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
    (void) x;
    (void) y;
    (void) z;
}

void
Hyena_RotateZYX(float z, float y, float x)
{
    (void) x;
    (void) y;
    (void) z;
}

void
Hyena_FlushMatrices(void)
{
}

vertex_t *
Hyena_AllocateMemoryForVertices(int count)
{
    if (count > hyena_vertex_capacity) {
        int capacity      = max(count, max(64, hyena_vertex_capacity * 2));
        vertex_t * memory = realloc(hyena_vertices, capacity * sizeof(*memory));
        if (!memory) Sys_Error("Hyena: out of vertex scratch memory");
        hyena_vertices        = memory;
        hyena_vertex_capacity = capacity;
    }
    return hyena_vertices;
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
    Hyena_InvalidateArrays();
    GLenum mode = hyena_vertex_mode == HYE_TRIANGLES ? GL_TRIANGLES :
      hyena_vertex_mode == HYE_TRIANGLE_STRIP ? GL_TRIANGLE_STRIP : GL_TRIANGLE_FAN;
    qboolean textured  = texture_precision != HYE_TEXTURE_NOTEXTURE;
    qboolean transform = hyena_translation[0] || hyena_translation[1] || hyena_translation[2] ||
      hyena_scale[0] != 1 || hyena_scale[1] != 1 || hyena_scale[2] != 1;

    (void) vertex_precision;
    if (count <= 0) return;

    size_t offset = Hyena_StreamVertices(vertices, count * sizeof(*vertices));
    glEnableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    if (!textured) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(*vertices), (const void *) (offset + offsetof(vertex_t, xyz)));
    if (textured) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, sizeof(*vertices), (const void *) (offset + offsetof(vertex_t, uv)));
    }
    if (transform) {
        glPushMatrix();
        glTranslatef(hyena_translation[0], hyena_translation[1], hyena_translation[2]);
        glScalef(hyena_scale[0], hyena_scale[1], hyena_scale[2]);
    }
    glDrawArrays(mode, 0, count);
    if (transform) glPopMatrix();
    if (textured) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void
Hyena_EndVertices(void)
{
}

static void
Hyena_Reserve2DVertices(int count)
{
    if (count <= hyena_2d_capacity) return;

    int capacity = max(count, max(256, hyena_2d_capacity * 2));
    hyena_colored_vertex_t * memory = realloc(hyena_2d_vertices, capacity * sizeof(*memory));
    if (!memory) Sys_Error("Hyena: out of 2D scratch memory");
    hyena_2d_vertices = memory;
    hyena_2d_capacity = capacity;
}

void
Hyena_Draw2DQuads(const hyena_2d_quad_t * quads, int count, int texture, unsigned int flags)
{
    Hyena_InvalidateArrays();
    int i, vertex_count = count * 4;

    if (count <= 0) return;

    Hyena_Reserve2DVertices(vertex_count);
    Hyena_QuadIndices();
    for (i = 0; i < vertex_count; ++i) {
        const hyena_2d_quad_t * q       = &quads[i / 4];
        hyena_colored_vertex_t * vertex = &hyena_2d_vertices[i];
        int corner = i & 3;
        vertex->xyz[0]   = corner == 0 || corner == 3 ? q->x0 : q->x1;
        vertex->xyz[1]   = corner < 2 ? q->y0 : q->y1;
        vertex->xyz[2]   = 0;
        vertex->uv[0]    = corner == 0 || corner == 3 ? q->u0 : q->u1;
        vertex->uv[1]    = corner < 2 ? q->v0 : q->v1;
        vertex->color[0] = q->r;
        vertex->color[1] = q->g;
        vertex->color[2] = q->b;
        vertex->color[3] = q->a;
    }
    if (flags & HYE_2D_TEXTURED) {
        Hyena_BindTexture(texture);
        glEnable(GL_TEXTURE_2D);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flags & HYE_2D_LINEAR ? GL_LINEAR : GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flags & HYE_2D_LINEAR ? GL_LINEAR : GL_NEAREST);
    } else {
        glDisable(GL_TEXTURE_2D);
    }
    if (flags & HYE_2D_BLEND) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
    glDisable(GL_ALPHA_TEST);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    if (flags & HYE_2D_TEXTURED) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    for (i = 0; i < count; i += HYENA_QUAD_BATCH) {
        int batch     = min(count - i, HYENA_QUAD_BATCH);
        size_t offset = Hyena_StreamVertices(hyena_2d_vertices + i * 4, batch * 4 * sizeof(*hyena_2d_vertices));
        glVertexPointer(3, GL_FLOAT, sizeof(*hyena_2d_vertices),
          (const void *) (offset + offsetof(hyena_colored_vertex_t, xyz)));
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(*hyena_2d_vertices),
          (const void *) (offset + offsetof(hyena_colored_vertex_t, color)));
        if (flags & HYE_2D_TEXTURED)
            glTexCoordPointer(2, GL_FLOAT, sizeof(*hyena_2d_vertices),
              (const void *) (offset + offsetof(hyena_colored_vertex_t, uv)));
        glDrawElements(GL_TRIANGLES, batch * 6, GL_UNSIGNED_SHORT, hyena_quad_indices);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (flags & HYE_2D_TEXTURED)
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glColor4f(1, 1, 1, 1);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);
} /* Hyena_Draw2DQuads */

static void
Hyena_GLUnbindBuffers(void)
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void
Hyena_FogSet(bool is_world_geometry, float start, float end, float red, float green, float blue, float alpha)
{
    float color[4] = { is_world_geometry ? 0.5f : red * 0.01f,  is_world_geometry ? 0.5f : green * 0.01f,
                       is_world_geometry ? 0.5f : blue * 0.01f, alpha };

    glFogfv(GL_FOG_COLOR, color);
    glFogf(GL_FOG_START, start);
    glFogf(GL_FOG_END, end);
}

static GLuint hyena_texture_objects[HYENA_MAX_TEXTURES];
static GLuint current_gl_id;
static qboolean hyena_upload_lightmap, hyena_upload_alpha;

static int
Hyena_GLFilter(int filter)
{
    static const int filters[] = { GL_NEAREST,               GL_LINEAR,                GL_NEAREST_MIPMAP_NEAREST,
                                   GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR };

    return filters[filter];
}

void
Hyena_BackendTextureFilter(int texture, int min_filter, int mag_filter)
{
    GLuint object = hyena_texture_objects[texture];

    if (current_gl_id != object) {
        glBindTexture(GL_TEXTURE_2D, object);
        current_gl_id = object;
    }
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, Hyena_GLFilter(min_filter));
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, Hyena_GLFilter(mag_filter));
}

void
Hyena_BackendBindTexture(int texture, int min_filter, int mag_filter)
{
    if (current_gl_id == hyena_texture_objects[texture])
        return;

    Hyena_BackendTextureFilter(texture, min_filter, mag_filter);
}

void
Hyena_BackendDestroyTexture(int texture)
{
    glDeleteTextures(1, &hyena_texture_objects[texture]);
    hyena_texture_objects[texture] = 0;
    current_gl_id = (GLuint) - 1;
}

void
Hyena_BackendInitTextureResources(void)
{
    Hyena_ResetTextureRegistry();
    Hyena_InitTextureSettings("512", true, HYENA_MAX_TEXTURES);
}

static void
Hyena_UploadPixels(int texture, int level, int storage, int width, int height,
  int x, int y, qboolean replace, const void * pixels)
{
    GLenum error;
    int format = storage == HYE_PIXELS_L8 ? GL_LUMINANCE : GL_RGBA;
    int type   = storage == HYE_PIXELS_RGBA5551 ? GL_UNSIGNED_SHORT_5_5_5_1 :
      storage == HYE_PIXELS_RGBA4444 ? GL_UNSIGNED_SHORT_4_4_4_4 : GL_UNSIGNED_BYTE;
    GLint alignment;

    glGetError();
    glBindTexture(GL_TEXTURE_2D, hyena_texture_objects[texture]);
    current_gl_id = hyena_texture_objects[texture];

    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (replace)
        glTexImage2D(GL_TEXTURE_2D, level, format, width, height, 0, format, type, pixels);
    else
        glTexSubImage2D(GL_TEXTURE_2D, level, x, y, width, height, format, type, pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    error = glGetError();
    if (error != GL_NO_ERROR)
        Sys_Error("Xbox texture %s level %d (%dx%d): GLES error 0x%x",
          hyena_textures[texture].identifier, level, width, height, error);
} /* Hyena_UploadPixels */

void
Hyena_BackendUploadLevel(int texture, int level, int storage, int width, int height,
  int x, int y, qboolean replace, const void * pixels)
{
    unsigned short * packed = NULL;

    if (storage == HYE_PIXELS_RGBA8 && !hyena_upload_lightmap) {
        packed = malloc((size_t) width * height * sizeof(*packed));
        if (!packed)
            Sys_Error("Hyena: out of texture conversion memory");
        storage = hyena_upload_alpha ? HYE_PIXELS_RGBA4444 : HYE_PIXELS_RGBA5551;
        if (hyena_upload_alpha)
            Hyena_PackRGBA4444(pixels, packed, (size_t) width * height, false);
        else
            Hyena_PackRGBA5551(pixels, packed, (size_t) width * height);
        pixels = packed;
    }
    if (storage == HYE_PIXELS_RGBA4444 || storage == HYE_PIXELS_RGBA5551) {
        size_t i, count = (size_t) width * height;
        if (!packed) {
            packed = malloc(count * sizeof(*packed));
            if (!packed)
                Sys_Error("Hyena: out of packed texture memory");
            memcpy(packed, pixels, count * sizeof(*packed));
        }
        // nxdk-gles11 stores packed texels directly in the NV2A's ARGB layout.
        for (i = 0; i < count; ++i) {
            unsigned value = packed[i];
            packed[i] = storage == HYE_PIXELS_RGBA4444 ?
              (value >> 4) | (value << 12) : (value >> 1) | (value << 15);
        }
        pixels = packed;
    }
    Hyena_UploadPixels(texture, level, storage, width, height, x, y, replace, pixels);
    free(packed);
} /* Hyena_BackendUploadLevel */

static void
Hyena_AlignLightmapUpdate(hyena_texture_desc_t * desc)
{
    int right, bottom;

    if (!desc->lightmap || !desc->update || desc->width < 8 || desc->height < 8 ||
      (desc->width & (desc->width - 1)) || (desc->height & (desc->height - 1)) ||
      desc->update_x < 0 || desc->update_y < 0 || desc->update_width <= 0 || desc->update_height <= 0 ||
      desc->update_x > desc->width || desc->update_y > desc->height ||
      desc->update_width > desc->width - desc->update_x ||
      desc->update_height > desc->height - desc->update_y)
        return;

    // A complete Morton tile uploads as sequential writes to GPU memory.
    // The CPU atlas also holds the unchanged neighbors, so no GPU readback is needed.
    right = (desc->update_x + desc->update_width + 7) & ~7;
    bottom = (desc->update_y + desc->update_height + 7) & ~7;
    desc->update_x &= ~7;
    desc->update_y &= ~7;
    desc->update_width = right - desc->update_x;
    desc->update_height = bottom - desc->update_y;
}

static void
Hyena_UploadLightmap(int texture, const hyena_texture_desc_t * desc)
{
    hyena_texture_t * record = &hyena_textures[texture];
    int bpp = hyena_lightmap_16bit.value ? 2 : 4;
    qboolean replace = !record->used || record->width != desc->width ||
      record->height != desc->height || record->bpp != bpp;
    int x = replace || !desc->update_width ? 0 : desc->update_x;
    int y = replace || !desc->update_width ? 0 : desc->update_y;
    int width = replace || !desc->update_width ? desc->width : desc->update_width;
    int height = replace || !desc->update_width ? desc->height : desc->update_height;
    int stride = desc->row_stride ? desc->row_stride : desc->width * 4;
    byte * pixels = malloc((size_t)width * height * bpp);
    int row, column;

    if (!pixels)
        Sys_Error("Hyena: out of lightmap upload memory");
    // Read the strided CPU atlas once and produce the GPU's native layout.
    // This avoids a region copy, GL-format packing and a second ARGB conversion.
    for (row = 0; row < height; ++row) {
        const byte * source = (const byte *)desc->pixels + (size_t)(y + row) * stride + x * 4;
        if (bpp == 2) {
            unsigned short * out = (unsigned short *)pixels + (size_t)row * width;
            for (column = 0; column < width; ++column, source += 4)
                out[column] = ((source[3] >> 7) << 15) | ((source[0] >> 3) << 10) |
                  ((source[1] >> 3) << 5) | (source[2] >> 3);
        } else
            memcpy(pixels + (size_t)row * width * 4, source, width * 4);
    }
    Hyena_UploadPixels(texture, 0, bpp == 2 ? HYE_PIXELS_RGBA5551 : HYE_PIXELS_RGBA8,
      width, height, x, y, replace, pixels);
    free(pixels);
    record->width = desc->width;
    record->height = desc->height;
    record->bpp = bpp;
    record->mipmaps = 0;
    int filter = desc->filter == HYE_FILTER_LINEAR ? HYE_LINEAR : HYE_NEAREST;
    Hyena_BackendTextureFilter(texture, filter, filter);
} /* Hyena_UploadLightmap */

void
Hyena_BackendUploadTexture(int texture, const hyena_texture_desc_t * desc)
{
    hyena_upload_lightmap = desc->lightmap;
    hyena_upload_alpha    = desc->alpha;
    if (!hyena_textures[texture].used)
        glGenTextures(1, &hyena_texture_objects[texture]);
    hyena_texture_desc_t upload = *desc;
    Hyena_AlignLightmapUpdate(&upload);
    if (upload.lightmap && upload.format == HYE_TEXTURE_RGBA8 && !upload.mip_levels)
        Hyena_UploadLightmap(texture, &upload);
    else
        Hyena_PrepareTextureUpload(texture, &upload, HYE_PIXELS_RGBA5551, Hyena_BackendUploadLevel);
    if (!desc->lightmap)
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

static GLenum
Hyena_ResolveCapability(int capability)
{
    switch (capability) {
        case HYE_BLEND:
            return GL_BLEND;

        case HYE_TEXTURE_2D:
            return GL_TEXTURE_2D;

        case HYE_ALPHA_TEST:
            return GL_ALPHA_TEST;

        case HYE_DEPTH_TEST:
            return GL_DEPTH_TEST;

        case HYE_CULL_FACE:
            return GL_CULL_FACE;

        default:
            Sys_Error("Hyena: unknown capability %d", capability);
    }
    return 0;
}

void
Hyena_SetTextureMode(int mode)
{
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode == HYE_MODULATE ? GL_MODULATE : GL_REPLACE);
}

void
Hyena_EnableCapability(int capability)
{
    glEnable(Hyena_ResolveCapability(capability));
}

void
Hyena_DisableCapability(int capability)
{
    glDisable(Hyena_ResolveCapability(capability));
}

void
Hyena_DepthMask(qboolean value)
{
    glDepthMask(value ? GL_TRUE : GL_FALSE);
}

void
Hyena_SetShadeMode(int mode)
{
    glShadeModel(mode == HYE_FLAT ? GL_FLAT : GL_SMOOTH);
}

static GLenum
Hyena_ResolveBlend(int blend)
{
    switch (blend) {
        case HYE_ZERO: return GL_ZERO;

        case HYE_SRC_ALPHA:
            return GL_SRC_ALPHA;

        case HYE_ONE_MINUS_SRC_ALPHA:
            return GL_ONE_MINUS_SRC_ALPHA;

        case HYE_ONE:
            return GL_ONE;

        case HYE_DST_COLOR:
            return GL_DST_COLOR;

        case HYE_SRC_COLOR:
            return GL_SRC_COLOR;

        case HYE_ONE_MINUS_SRC_COLOR:
            return GL_ONE_MINUS_SRC_COLOR;

        default:
            Sys_Error("Hyena: unknown blend function %d", blend);
    }
    return GL_ONE;
}

void
Hyena_SetBlendFunction(int source, int destination)
{
    hyena_blend_alpha = 1;
    glBlendFunc(Hyena_ResolveBlend(source), Hyena_ResolveBlend(destination));
}

void
Hyena_SetDepthRange(float near_value, float far_value)
{
    double range = gldepthmax - gldepthmin;

    glDepthRangef(gldepthmin + near_value * range, gldepthmin + far_value * range);
}

void
Hyena_ResetDepthRange(void)
{
    glDepthRangef(gldepthmin + 0.3f * (gldepthmax - gldepthmin), gldepthmax);
}

void
Hyena_SetDepthOffset(float offset)
{
    if (offset != 0.0f) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(offset, offset);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
}

void
Hyena_Set2D(void)
{
    glViewport(glx, gly, glwidth, glheight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, vid.width, vid.height, 0, -99999, 99999);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void
Hyena_FogEnable(void)
{
    glEnable(GL_FOG);
}

void
Hyena_FogDisable(void)
{
    glDisable(GL_FOG);
}

void
Hyena_FogInit(void)
{
    glFogf(GL_FOG_MODE, GL_LINEAR);
}

void
Hyena_DrawIndexedTriangles(const hyena_colored_vertex_t * vertices, int vertex_count, const unsigned short * indices,
  int index_count, qboolean textured)
{
    Hyena_InvalidateArrays();
    if (index_count <= 0) return;

    if (vertex_count <= 0 || vertex_count > 1024) Sys_Error("Hyena: invalid indexed batch size");
    size_t offset = Hyena_StreamVertices(vertices, vertex_count * sizeof(*vertices));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(*vertices), (const void *) (offset + offsetof(hyena_colored_vertex_t, xyz)));
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(*vertices),
      (const void *) (offset + offsetof(hyena_colored_vertex_t, color)));
    if (textured) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, sizeof(*vertices),
          (const void *) (offset + offsetof(hyena_colored_vertex_t, uv)));
    }
    glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, indices);
    if (textured) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glColor4f(1, 1, 1, 1);
} /* Hyena_DrawIndexedTriangles */

struct hyena_buffer_s {
    int      target;
    size_t   size;
    void *   data;
    qboolean mapped;
    GLuint   object;
};
static int hyena_arrays_mask;

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
    if (buffer->target != HYE_ARRAY_BUFFER) {
        buffer->data = (void *) data;
        buffer->size = size;
        return;
    }
    if (!buffer->object) glGenBuffers(1, &buffer->object);
    glBindBuffer(GL_ARRAY_BUFFER, buffer->object);
    if (buffer->size != size || !buffer->size) {
        glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW);
        Hyena_InvalidateArrays();
    }
    if (data && size) glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    buffer->size = size;
}

void
Hyena_DestroyBuffer(hyena_buffer_t * buffer)
{
    if (!buffer) return;

    if (buffer->object) glDeleteBuffers(1, &buffer->object);
    free(buffer);
}

static int
Hyena_ArrayType(int type)
{
    return type == HYE_SHORT ? GL_SHORT : type == HYE_UBYTE ? GL_UNSIGNED_BYTE : GL_FLOAT;
}

static int
Hyena_ArrayMode(int mode)
{
    switch (mode) {
        case HYE_QUADS: return GL_TRIANGLE_FAN;

        case HYE_TRIANGLE_FAN: return GL_TRIANGLE_FAN;

        case HYE_TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;

        case HYE_LINE_STRIP: return GL_LINE_STRIP;

        default: return GL_TRIANGLES;
    }
}

static const void *
Hyena_ArrayPointer(const hyena_array_t * array)
{
    glBindBuffer(GL_ARRAY_BUFFER, array->buffer ? array->buffer->object : 0);
    return array->data;
}

void
Hyena_BeginArrays(int mask)
{
    Hyena_GLUnbindBuffers();
    hyena_arrays_mask = mask;
    glEnableClientState(GL_VERTEX_ARRAY);
    // Set the complete array state when switching from colored UI submissions.
    if (mask & HYE_ARRAY_UV)
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    else
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    if (mask & HYE_ARRAY_COLOR)
        glEnableClientState(GL_COLOR_ARRAY);
    else
        glDisableClientState(GL_COLOR_ARRAY);
}

void
Hyena_EndArrays(void)
{
    Hyena_GLUnbindBuffers();
    if (hyena_arrays_mask & HYE_ARRAY_COLOR)
        glDisableClientState(GL_COLOR_ARRAY);
    if (hyena_arrays_mask & HYE_ARRAY_UV)
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    hyena_arrays_mask = 0;
}

static void
Hyena_SetArrays(const hyena_arrays_t * arrays)
{
    static struct {
        GLuint       object;
        const void * data;
        int          type, stride;
        unsigned     generation;
        qboolean     valid;
    } cached[3];
    const hyena_array_t * attributes[] = { &arrays->position, &arrays->uv, &arrays->color };
    int i;
    for (i = 0; i < 3; ++i) {
        const hyena_array_t * attribute = attributes[i];
        if (i && !attribute->data && !attribute->buffer) continue;
        GLuint object = attribute->buffer ? attribute->buffer->object : 0;
        if (cached[i].valid && cached[i].generation == hyena_array_generation &&
          cached[i].object == object && cached[i].data == attribute->data &&
          cached[i].type == attribute->type && cached[i].stride == attribute->stride) continue;
        const void * ptr = Hyena_ArrayPointer(attribute);
        int type         = Hyena_ArrayType(attribute->type);
        if (i == 0) glVertexPointer(3, type, attribute->stride, ptr);
        else if (i == 1) glTexCoordPointer(2, type, attribute->stride, ptr);
        else glColorPointer(4, type, attribute->stride, ptr);
        cached[i].object     = object;
        cached[i].data       = attribute->data;
        cached[i].type       = attribute->type;
        cached[i].stride     = attribute->stride;
        cached[i].generation = hyena_array_generation;
        cached[i].valid      = true;
    }
}

static int
Hyena_ArrayMask(const hyena_arrays_t * arrays)
{
    return HYE_ARRAY_POSITION | ((arrays->uv.data || arrays->uv.buffer) ? HYE_ARRAY_UV : 0)
           | ((arrays->color.data || arrays->color.buffer) ? HYE_ARRAY_COLOR : 0);
}

void
Hyena_DrawElements(int mode, const hyena_arrays_t * arrays, int count,
  hyena_buffer_t * indices, size_t offset)
{
    qboolean own_arrays = !hyena_arrays_mask;
    const void * elements;

    if (!count)
        return;

    // Animated aliases already have GPU UVs. Stream just their changing positions
    // so GLES can draw the existing index list without scanning and staging it.
    hyena_arrays_t streamed_arrays;
    hyena_buffer_t streamed_positions = {0};
    if (!arrays->position.buffer && arrays->position.data && arrays->uv.buffer &&
      !arrays->color.data && !arrays->color.buffer && arrays->count > 0) {
        int component = arrays->position.type == HYE_SHORT ? sizeof(short) :
          arrays->position.type == HYE_UBYTE ? sizeof(byte) : sizeof(float);
        int stride = arrays->position.stride ? arrays->position.stride : component * 3;
        size_t bytes = (size_t)(arrays->count - 1) * stride + component * 3;
        size_t position_offset = Hyena_StreamVertices(arrays->position.data, bytes);
        streamed_positions.object = hyena_stream_object;
        streamed_arrays = *arrays;
        streamed_arrays.position.buffer = &streamed_positions;
        streamed_arrays.position.data = (const void *)(uintptr_t)position_offset;
        arrays = &streamed_arrays;
    }

    if (own_arrays)
        Hyena_BeginArrays(Hyena_ArrayMask(arrays));
    Hyena_SetArrays(arrays);
    if (arrays->scale != 0 && arrays->scale != 1) {
        glPushMatrix();
        glScalef(arrays->scale, arrays->scale, arrays->scale);
    }
    elements = (const byte *) indices->data + offset;
    if (mode == HYE_QUADS) {
        int first;
        const unsigned short * source = elements;
        for (first = 0; first + 3 < count;) {
            int batch = min((count - first) / 4, HYENA_QUAD_BATCH), quad;
            for (quad = 0; quad < batch; ++quad) {
                const unsigned short * in = source + first + quad * 4;
                unsigned short * out      = hyena_quad_scratch + quad * 6;
                out[0] = out[3] = in[0];
                out[1] = in[1];
                out[2] = out[4] = in[2];
                out[5] = in[3];
            }
            glDrawElements(GL_TRIANGLES, batch * 6, GL_UNSIGNED_SHORT, hyena_quad_scratch);
            first += batch * 4;
        }
    } else {
        glDrawElements(Hyena_ArrayMode(mode), count, GL_UNSIGNED_SHORT, elements);
    }
    if (arrays->scale != 0 && arrays->scale != 1)
        glPopMatrix();
    if (own_arrays)
        Hyena_EndArrays();
} /* Hyena_DrawElements */

void
Hyena_DrawArrays(int mode, const hyena_arrays_t * arrays, int count)
{
    qboolean own_arrays = !hyena_arrays_mask;

    if (count <= 0) return;

    if (own_arrays) Hyena_BeginArrays(Hyena_ArrayMask(arrays));
    if (arrays->scale != 0 && arrays->scale != 1) {
        glPushMatrix();
        glScalef(arrays->scale, arrays->scale, arrays->scale);
    }
    if (mode == HYE_QUADS) {
        int first;
        Hyena_QuadIndices();
        for (first = 0; first + 3 < count;) {
            int batch = min((count - first) / 4, HYENA_QUAD_BATCH), i;
            hyena_arrays_t segment       = *arrays;
            hyena_array_t * attributes[] = { &segment.position, &segment.uv, &segment.color };
            const int components[]       = { 3, 2, 4 };
            for (i = 0; i < 3; ++i) {
                hyena_array_t * attribute = attributes[i];
                if (!attribute->buffer && !attribute->data) continue;
                int size   = attribute->type == HYE_SHORT ? 2 : attribute->type == HYE_UBYTE ? 1 : 4;
                int stride = attribute->stride ? attribute->stride : components[i] * size;
                attribute->data = (const void *) ((uintptr_t) attribute->data + (size_t) first * stride);
            }
            Hyena_SetArrays(&segment);
            glDrawElements(GL_TRIANGLES, batch * 6, GL_UNSIGNED_SHORT, hyena_quad_indices);
            first += batch * 4;
        }
    } else {
        Hyena_SetArrays(arrays);
        glDrawArrays(Hyena_ArrayMode(mode), 0, count);
    }
    if (arrays->scale != 0 && arrays->scale != 1) glPopMatrix();
    if (own_arrays) Hyena_EndArrays();
} /* Hyena_DrawArrays */

void
Hyena_DepthFunction(int function)
{
    glDepthFunc(function == HYE_EQUAL ? GL_EQUAL : function == HYE_GEQUAL ||
      gldepthmin > gldepthmax ? GL_GEQUAL : GL_LEQUAL);
}

void
Hyena_AlphaFunction(int function, float value)
{
    (void) function;
    glAlphaFunc(GL_GREATER, value);
}

void
Hyena_TextureFilter(int filter)
{
    int mode = filter == HYE_FILTER_LINEAR ? GL_LINEAR : GL_NEAREST;

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mode);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mode);
}

void
Hyena_LoadViewMatrix(const float * matrix)
{
    glLoadMatrixf(matrix);
}

void
Hyena_InvalidateTextureBinding(void)
{
    current_gl_id = (GLuint) - 1;
}

void
Hyena_SetCurrentColor(float r, float g, float b, float a)
{
    glColor4f(r, g, b, a * hyena_blend_alpha);
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
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(uoffset, voffset, 0);
    glScalef(uscale, vscale, 1);
    glMatrixMode(GL_MODELVIEW);
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
    if (!buffer->object) return buffer->data;

    glBindBuffer(GL_ARRAY_BUFFER, buffer->object);
    void * data = glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES);
    if (buffer->size && !data) Sys_Error("Hyena: could not map GPU buffer");
    buffer->mapped = true;
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return data;
}

void
Hyena_UnmapBuffer(hyena_buffer_t * buffer)
{
    if (buffer->mapped) {
        glBindBuffer(GL_ARRAY_BUFFER, buffer->object);
        if (!glUnmapBufferOES(GL_ARRAY_BUFFER)) Sys_Error("Hyena: could not unmap GPU buffer");
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        buffer->mapped = false;
    }
}

void
Hyena_SelectDrawBuffer(qboolean front)
{
    (void) front;
}

void
Hyena_SelectReadBuffer(qboolean front)
{
    hyena_read_front = front;
}

static int hyena_matrix_mode;
void
Hyena_MatrixMode(int mode)
{
    hyena_matrix_mode = mode;
    glMatrixMode(mode == HYE_PROJECTION ? GL_PROJECTION : GL_MODELVIEW);
}

void Hyena_LoadIdentity(void){ glLoadIdentity(); }

void Hyena_LoadMatrix(const float * matrix){ glLoadMatrixf(matrix); }

void
Hyena_GetMatrix(float * matrix)
{
    glGetFloatv(hyena_matrix_mode == HYE_PROJECTION ? GL_PROJECTION_MATRIX : GL_MODELVIEW_MATRIX, matrix);
}

void Hyena_PushMatrix(void){ glPushMatrix(); }

void Hyena_PopMatrix(void){ glPopMatrix(); }

void Hyena_MatrixTranslate(float x, float y, float z){ glTranslatef(x, y, z); }

void Hyena_MatrixScale(float x, float y, float z){ glScalef(x, y, z); }

void Hyena_MatrixRotate(float degrees, float x, float y, float z){ glRotatef(degrees, x, y, z); }

void Hyena_UpdateMatrices(void){ }

void
Hyena_Frustum(float left, float right, float bottom, float top, float near_value, float far_value)
{
    glFrustumf(left, right, bottom, top, near_value, far_value);
}

void Hyena_Viewport(int x, int y, int width, int height){ glViewport(x, y, width, height); }

void Hyena_ClipFrustum(float fovy, float aspect, float near_value){ (void) fovy; (void) aspect; (void) near_value; }

void Hyena_CullFace(qboolean back){ glCullFace(back ? GL_BACK : GL_FRONT); }

void Hyena_ClearColor(float red, float green, float blue, float alpha){ glClearColor(red, green, blue, alpha); }

void
Hyena_Clear(qboolean color, qboolean depth, qboolean stencil)
{
    glClear((color ? GL_COLOR_BUFFER_BIT : 0) | (depth ? GL_DEPTH_BUFFER_BIT : 0)
      | (stencil ? GL_STENCIL_BUFFER_BIT : 0));
}

void
Hyena_DepthBounds(float near_value, float far_value)
{
    gldepthmin = near_value;
    gldepthmax = far_value;
}

void
Hyena_PerspectiveCorrection(qboolean enabled)
{
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, enabled ? GL_NICEST : GL_FASTEST);
}

void
Hyena_BlendConstant(float alpha)
{
    hyena_blend_alpha = alpha;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Hyena_Finish(void){ glFinish(); }

qboolean
Hyena_ReadPixels(int x, int y, int width, int height, byte * rgba)
{
    if (hyena_read_front)
        return VID_XboxReadFrontPixels(x, y, width, height, rgba);

    glGetError();
    glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    return glGetError() == GL_NO_ERROR;
}

qboolean
Hyena_DrawMorphMesh(const hyena_morph_mesh_t * mesh, const byte * first, const byte * second, float blend)
{
    (void) mesh;
    (void) first;
    (void) second;
    (void) blend;
    return false;
}

void Hyena_AddLightTime(double seconds){ (void) seconds; }

void Hyena_InitTextureStorage(void){ }

size_t Hyena_TextureBudget(void){ return 0; }

int Hyena_PreferredPaletteBits(void){ return 8; }

qboolean Hyena_MorphMeshesSupported(void){ return false; }

void * Hyena_AllocateTexturePixels(size_t bytes){ return malloc(bytes); }

void Hyena_FreeTexturePixels(void * pixels){ free(pixels); }

int Hyena_PreferredLightmapSize(void){ return 256; }

hyena_texture_format_t Hyena_PreferredLightmapFormat(void){ return HYE_TEXTURE_RGBA8; }

unsigned * Hyena_LightmapScratch(int count){ (void) count; return NULL; }

qboolean
Hyena_AccumulateLightmap(unsigned * dest, const byte * source, int count, unsigned scale, qboolean add)
{
    typedef struct __attribute__((packed, may_alias)){ __m64 value;
    } unaligned_mmx_t;
    typedef struct __attribute__((packed, may_alias)){ unsigned value;
    } unaligned_word_t;
    int i;
    if (scale > 65535 || count < 16) return false;

    __m64 zero   = _mm_setzero_si64();
    __m64 factor = _mm_set1_pi16((short) scale);
    for (i = 0; i + 3 < count; i += 4) {
        __m64 samples = _mm_cvtsi32_si64(((const unaligned_word_t *) (source + i))->value);
        __m64 words   = _mm_unpacklo_pi8(samples, zero);
        __m64 low     = _mm_mullo_pi16(words, factor);
        __m64 high    = _mm_mulhi_pu16(words, factor);
        __m64 first   = _mm_unpacklo_pi16(low, high);
        __m64 second  = _mm_unpackhi_pi16(low, high);
        if (add) {
            first  = _mm_add_pi32(first, ((const unaligned_mmx_t *) (dest + i))->value);
            second = _mm_add_pi32(second, ((const unaligned_mmx_t *) (dest + i + 2))->value);
        }
        ((unaligned_mmx_t *) (dest + i))->value     = first;
        ((unaligned_mmx_t *) (dest + i + 2))->value = second;
    }
    _mm_empty();
    for (; i < count; ++i) {
        unsigned value = source[i] * scale;
        dest[i] = add ? dest[i] + value : value;
    }
    return true;
} /* Hyena_AccumulateLightmap */

void Hyena_PrepareTextureUpdate(int texture){ (void) texture; }

void Hyena_ClipArrays(const hyena_arrays_t * source, hyena_arrays_t * clipped){ *clipped = *source; }

void Hyena_BeginModelClipping(void){ }

void Hyena_EndModelClipping(void){ }

void Hyena_ClearMorphMeshes(void){  }
