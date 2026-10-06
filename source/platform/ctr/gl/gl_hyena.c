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

// gl_hyena.c -- Nintendo 3DS OpenGL Hyena backend
#include "../../../nzportable_def.h"
#include "../../../render/r_texture.h"
#include "../../../render/r_pixels.h"

#include <3ds.h>

static int hyena_vertex_mode;
static vec3_t hyena_translation;
static vec3_t hyena_scale;
static float hyena_color[4] = { 1, 1, 1, 1 };
static float * hyena_2d_positions;
static float * hyena_2d_texcoords;
static byte * hyena_2d_colors;
static int hyena_2d_capacity;

void
Hyena_SetColor(float r, float g, float b, float a)
{
    hyena_color[0] = r;
    hyena_color[1] = g;
    hyena_color[2] = b;
    hyena_color[3] = a;
    glColor4f(r, g, b, a);
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

static void
Hyena_SubmitVertex(const vertex_t * vertex, int textured)
{
    if (textured)
        glTexCoord2f(vertex->uv.u, vertex->uv.v);
    glVertex3f(vertex->xyz.x * hyena_scale[0] + hyena_translation[0],
      vertex->xyz.y * hyena_scale[1] + hyena_translation[1], vertex->xyz.z * hyena_scale[2] + hyena_translation[2]);
}

void
Hyena_DrawVertices(vertex_t * vertices, int count, int texture_precision, int vertex_precision)
{
    int i;
    int textured = texture_precision != HYE_TEXTURE_NOTEXTURE;

    (void) vertex_precision;

    // Expand fans for backends without reliable fan support.
    if (hyena_vertex_mode == HYE_TRIANGLE_FAN && count >= 3) {
        glBegin(GL_TRIANGLES);
        for (i = 1; i + 1 < count; ++i) {
            Hyena_SubmitVertex(&vertices[0], textured);
            Hyena_SubmitVertex(&vertices[i], textured);
            Hyena_SubmitVertex(&vertices[i + 1], textured);
        }
        glEnd();
    }
    free(vertices);
}

void
Hyena_EndVertices(void)
{
}

static void
Hyena_Reserve2DVertices(int count)
{
    if (count <= hyena_2d_capacity)
        return;

    hyena_2d_positions = realloc(hyena_2d_positions, count * 3 * sizeof(*hyena_2d_positions));
    hyena_2d_texcoords = realloc(hyena_2d_texcoords, count * 2 * sizeof(*hyena_2d_texcoords));
    hyena_2d_colors    = realloc(hyena_2d_colors, count * 4 * sizeof(*hyena_2d_colors));
    if (!hyena_2d_positions || !hyena_2d_texcoords || !hyena_2d_colors)
        Sys_Error("Hyena_Reserve2DVertices: out of memory");
    hyena_2d_capacity = count;
}

void
Hyena_Draw2DQuads(const hyena_2d_quad_t * quads, int count, int texture, unsigned int flags)
{
    static const int corners[6] = { 0, 1, 2, 0, 2, 3 };
    int i, vertex_count = count * 6;

    if (!count)
        return;

    Hyena_Reserve2DVertices(vertex_count);
    for (i = 0; i < vertex_count; ++i) {
        const hyena_2d_quad_t * q = &quads[i / 6];
        int corner = corners[i % 6];
        hyena_2d_positions[i * 3]     = (corner == 0 || corner == 3) ? q->x0 : q->x1;
        hyena_2d_positions[i * 3 + 1] = corner < 2 ? q->y0 : q->y1;
        hyena_2d_positions[i * 3 + 2] = 0;
        hyena_2d_texcoords[i * 2]     = (corner == 0 || corner == 3) ? q->u0 : q->u1;
        hyena_2d_texcoords[i * 2 + 1] = corner < 2 ? q->v0 : q->v1;
        hyena_2d_colors[i * 4]        = q->r;
        hyena_2d_colors[i * 4 + 1]    = q->g;
        hyena_2d_colors[i * 4 + 2]    = q->b;
        hyena_2d_colors[i * 4 + 3]    = q->a;
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
    glVertexPointer(3, GL_FLOAT, 0, hyena_2d_positions);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, hyena_2d_colors);
    if (flags & HYE_2D_TEXTURED) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, hyena_2d_texcoords);
    }
    glDrawArrays(GL_TRIANGLES, 0, vertex_count);
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
}

void
Hyena_FogSet(bool is_world_geometry, float start, float end, float red, float green, float blue, float alpha)
{
    float color[4];

    (void) is_world_geometry;
    color[0] = red * 0.01f;
    color[1] = green * 0.01f;
    color[2] = blue * 0.01f;
    color[3] = alpha;
    glFogfv(GL_FOG_COLOR, color);
    glFogf(GL_FOG_START, start);
    glFogf(GL_FOG_END, end);
}

static GLuint hyena_texture_objects[HYENA_MAX_TEXTURES];
static GLuint current_gl_id;

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
    Hyena_InitTextureSettings("256", true, HYENA_MAX_TEXTURES);
}

void
Hyena_BackendUploadLevel(int texture, int level, int storage, int width, int height,
  int x, int y, qboolean replace, const void * pixels)
{
    int format = storage == HYE_PIXELS_L8 ? GL_LUMINANCE : GL_RGBA;
    int type   = GL_UNSIGNED_BYTE;
    GLint alignment;

    glBindTexture(GL_TEXTURE_2D, hyena_texture_objects[texture]);
    current_gl_id = hyena_texture_objects[texture];

    // Narrow luminance atlases are byte-aligned, regardless of the previous upload.
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (replace)
        glTexImage2D(GL_TEXTURE_2D, level, format, width, height, 0, format, type, pixels);
    else
        glTexSubImage2D(GL_TEXTURE_2D, level, x, y, width, height, format, type, pixels);

    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
}

void
Hyena_BackendUploadTexture(int texture, const hyena_texture_desc_t * desc)
{
    if (!hyena_textures[texture].used)
        glGenTextures(1, &hyena_texture_objects[texture]);

    Hyena_PrepareTextureUpload(texture, desc, HYE_PIXELS_RGBA8, Hyena_BackendUploadLevel);

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
    glBlendFunc(Hyena_ResolveBlend(source), Hyena_ResolveBlend(destination));
}

void
Hyena_SetDepthRange(float near_value, float far_value)
{
    glDepthRange(near_value, far_value);
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
    glOrtho(0, vid.width, vid.height, 0, -99999, 99999);
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
    glFogi(GL_FOG_MODE, GL_LINEAR);
}

void
Hyena_DrawIndexedTriangles(const hyena_colored_vertex_t * vertices, int vertex_count, const unsigned short * indices,
  int index_count, qboolean textured)
{
    static float positions[1024 * 3];
    static float texcoords[1024 * 2];
    static byte colors[1024 * 4];
    int i;

    if (!index_count)
        return;

    if (vertex_count > 1024)
        Sys_Error("Hyena: indexed batch is too large");
    for (i = 0; i < vertex_count; ++i) {
        memcpy(positions + i * 3, vertices[i].xyz, sizeof(vertices[i].xyz));
        memcpy(colors + i * 4, vertices[i].color, sizeof(vertices[i].color));
        if (textured)
            memcpy(texcoords + i * 2, vertices[i].uv, sizeof(vertices[i].uv));
    }
    Hyena_GLUnbindBuffers();
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, positions);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
    if (textured) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
    }
    glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, indices);
    if (textured)
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glColor4f(1, 1, 1, 1);
} /* Hyena_DrawIndexedTriangles */

struct hyena_buffer_s {
    int      target;
    size_t   size;
    void *   data;
    qboolean owned;
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
    return true;
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
        linearFree(buffer->data);

    // Static storage owns its bytes; streamed indices keep the caller's frame allocation.
    buffer->owned = buffer->target == HYE_ARRAY_BUFFER;
    if (buffer->owned) {
        buffer->data = linearAlloc(size);
        if (!buffer->data)
            Sys_Error("Hyena_UpdateBuffer: out of memory");
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
        linearFree(buffer->data);
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
        case HYE_QUADS: return GL_QUADS;

        case HYE_TRIANGLE_FAN: return GL_TRIANGLE_FAN;

        case HYE_TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;

        case HYE_LINE_STRIP: return GL_LINE_STRIP;

        default: return GL_TRIANGLES;
    }
}

static const void *
Hyena_ArrayPointer(const hyena_array_t * array)
{
    return array->buffer ? (const byte *) array->buffer->data + (size_t) array->data : array->data;
}

void
Hyena_BeginArrays(int mask)
{
    Hyena_GLUnbindBuffers();
    hyena_arrays_mask = mask;
    glEnableClientState(GL_VERTEX_ARRAY);

    if (mask & HYE_ARRAY_UV)
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glEnableClientState(GL_COLOR_ARRAY);
}

void
Hyena_EndArrays(void)
{
    Hyena_GLUnbindBuffers();
    glDisableClientState(GL_COLOR_ARRAY);

    if (hyena_arrays_mask & HYE_ARRAY_UV)
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    glDisableClientState(GL_VERTEX_ARRAY);
    hyena_arrays_mask = 0;
}

static void
Hyena_SetArrays(const hyena_arrays_t * arrays)
{
    const void * ptr;

    if (arrays->uv.data || arrays->uv.buffer) {
        ptr = Hyena_ArrayPointer(&arrays->uv);
        glTexCoordPointer(2, Hyena_ArrayType(arrays->uv.type), arrays->uv.stride, ptr);
    }
    if (arrays->color.data || arrays->color.buffer) {
        ptr = Hyena_ArrayPointer(&arrays->color);
        glColorPointer(4, Hyena_ArrayType(arrays->color.type), arrays->color.stride, ptr);
    } else {
        static byte * colors;
        static int capacity;
        byte color[4];
        int i;
        if (arrays->count > capacity) {
            colors = realloc(colors, arrays->count * 4);
            if (!colors)
                Sys_Error("Hyena: out of color array memory");
            capacity = arrays->count;
        }
        for (i = 0; i < 4; ++i)
            color[i] = (byte) bound(0, hyena_color[i] * 255, 255);
        for (i = 0; i < arrays->count; ++i)
            memcpy(colors + i * 4, color, 4);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
    }
    ptr = Hyena_ArrayPointer(&arrays->position);
    glVertexPointer(3, Hyena_ArrayType(arrays->position.type), arrays->position.stride, ptr);
}

static int
Hyena_ArrayMask(const hyena_arrays_t * arrays)
{
    return HYE_ARRAY_POSITION | ((arrays->uv.data || arrays->uv.buffer) ? HYE_ARRAY_UV : 0) | ((arrays->color.data ||
           arrays->color.buffer) ? HYE_ARRAY_COLOR : 0);
}

void
Hyena_DrawElements(int mode, const hyena_arrays_t * arrays, int count,
  hyena_buffer_t * indices, size_t offset)
{
    qboolean own_arrays = !hyena_arrays_mask;
    const void * elements;

    if (!count)
        return;

    if (own_arrays)
        Hyena_BeginArrays(Hyena_ArrayMask(arrays));

    Hyena_SetArrays(arrays);

    if (arrays->scale != 0 && arrays->scale != 1) {
        glPushMatrix();
        glScalef(arrays->scale, arrays->scale, arrays->scale);
    }

    elements = (const byte *) indices->data + offset;
    glDrawRangeElements(Hyena_ArrayMode(mode), 0, arrays->count, count, GL_UNSIGNED_SHORT, elements);

    if (arrays->scale != 0 && arrays->scale != 1)
        glPopMatrix();

    if (own_arrays)
        Hyena_EndArrays();
}

void
Hyena_DrawArrays(int mode, const hyena_arrays_t * arrays, int count)
{
    qboolean own_arrays = !hyena_arrays_mask;

    if (!count)
        return;

    if (arrays->scale != 0 && arrays->scale != 1) {
        glPushMatrix();
        glScalef(arrays->scale, arrays->scale, arrays->scale);
    }

    // picaGL can't take quads as client arrays. Keep its immediate path :(
    // not sure why
    if (mode == HYE_QUADS) {
        const byte * xyz   = Hyena_ArrayPointer(&arrays->position);
        const byte * uv    = (arrays->uv.data || arrays->uv.buffer) ? Hyena_ArrayPointer(&arrays->uv) : NULL;
        const byte * color = (arrays->color.data || arrays->color.buffer) ? Hyena_ArrayPointer(&arrays->color) : NULL;
        int i, j;
        for (i = 0; i < count; i += 4) {
            glBegin(GL_QUADS);
            for (j = i; j < i + 4; ++j) {
                if (uv)
                    glTexCoord2fv((const float *) (uv + j
                      * (arrays->uv.stride ? arrays->uv.stride : 2 * sizeof(float))));
                if (color)
                    glColor4ubv(color + j * (arrays->color.stride ? arrays->color.stride : 4));
                glVertex3fv((const float *) (xyz + j
                  * (arrays->position.stride ? arrays->position.stride : 3 * sizeof(float))));
            }
            glEnd();
        }
    } else if (mode == HYE_TRIANGLE_FAN) {
        static float * positions, * texcoords, * colors;
        static unsigned short * indices;
        static int capacity;
        const byte * xyz = Hyena_ArrayPointer(&arrays->position);
        const byte * uv  = Hyena_ArrayPointer(&arrays->uv);
        int i;

        if (count > capacity) {
            positions = realloc(positions, count * 3 * sizeof(float));
            texcoords = realloc(texcoords, count * 2 * sizeof(float));
            colors    = realloc(colors, count * 4 * sizeof(float));
            indices   = realloc(indices, (count - 2) * 3 * sizeof(*indices));
            if (!positions || !texcoords || !colors || !indices)
                Sys_Error("Hyena: out of array memory");
            capacity = count;
        }

        for (i = 0; i < count; ++i) {
            if (arrays->position.stride)
                memcpy(positions + i * 3, xyz + i * arrays->position.stride, 3 * sizeof(float));
            if (arrays->uv.stride)
                memcpy(texcoords + i * 2, uv + i * arrays->uv.stride, 2 * sizeof(float));
            memcpy(colors + i * 4, hyena_color, 4 * sizeof(float));
        }

        for (i = 0; i < count - 2; ++i) {
            indices[i * 3]     = 0;
            indices[i * 3 + 1] = i + 1;
            indices[i * 3 + 2] = i + 2;
        }

        if (own_arrays)
            Hyena_BeginArrays(HYE_ARRAY_POSITION | HYE_ARRAY_UV);

        glVertexPointer(3, GL_FLOAT, 0, arrays->position.stride ? (const void *) positions : xyz);
        glTexCoordPointer(2, GL_FLOAT, 0, arrays->uv.stride ? (const void *) texcoords : uv);
        glColorPointer(4, GL_FLOAT, 0, colors);
        glDrawRangeElements(GL_TRIANGLES, 0, count, (count - 2) * 3, GL_UNSIGNED_SHORT, indices);

        if (own_arrays)
            Hyena_EndArrays();
    } else {
        if (own_arrays)
            Hyena_BeginArrays(Hyena_ArrayMask(arrays));

        Hyena_SetArrays(arrays);
        glDrawArrays(Hyena_ArrayMode(mode), 0, count);

        if (own_arrays)
            Hyena_EndArrays();
    }

    if (arrays->scale != 0 && arrays->scale != 1)
        glPopMatrix();
} /* Hyena_DrawArrays */

void
Hyena_DepthFunction(int function)
{
    glDepthFunc(function == HYE_EQUAL ? GL_EQUAL : GL_LEQUAL);
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
    glColor4f(r, g, b, a);
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
    return buffer->data;
}

void
Hyena_UnmapBuffer(hyena_buffer_t * buffer)
{
    (void) buffer;
}
