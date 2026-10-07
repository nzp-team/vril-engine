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

// r_hyena_soft.c -- Nspire software Hyena backend
#include "../../nzportable_def.h"
#include "../../render/r_texture.h"
#include "../../render/r_pixels.h"
#include "d_local.h"

#define HYENA_NEAR_CLIP 8.0f

typedef struct {
    float x, y, z, u, v;
} hyena_viewvert_t;

typedef struct {
    float x, y, inv_z, u_over_z, v_over_z;
} hyena_screenvert_t;

cachepic_t cachepics[MAX_CACHED_PICS];
int numcachepics;

static float hyena_matrix[16];
static float hyena_color[4] = { 1, 1, 1, 1 };
static float hyena_depth_near;
static float hyena_depth_far = 1.0f;
static float hyena_depth_offset;
static int hyena_vertex_mode;
static int hyena_texture_mode      = HYE_REPLACE;
static int hyena_texture           = -1;
static int hyena_destination_blend = HYE_ONE_MINUS_SRC_ALPHA;
static qboolean hyena_blend;
static qboolean hyena_texture_enabled = qtrue;
static qboolean hyena_depth_write     = qtrue;
static qboolean hyena_cull;
static qboolean hyena_draw_textured;
static qboolean hyena_fog;
static float hyena_fog_start, hyena_fog_end;
static float hyena_fog_color[3];

void
Hyena_BackendUploadTexture(int texture_index, const hyena_texture_desc_t * desc)
{
    cachepic_t * texture;
    byte * pixels;
    int count = desc->width * desc->height;
    int i;

    // The software rasterizer retains its legacy cache view of the owned pixels.
    if (hyena_textures[texture_index].used)
        return;

    if (texture_index >= MAX_CACHED_PICS)
        Sys_Error("Hyena: software texture cache is full");
    hyena_textures[texture_index].width  = desc->width;
    hyena_textures[texture_index].height = desc->height;
    texture = &cachepics[texture_index];
    pixels  = Hunk_Alloc(count);
    if (desc->format == HYE_TEXTURE_INDEX8 && !desc->palette) {
        memcpy(pixels, desc->pixels, count);
    } else if (desc->format == HYE_TEXTURE_INDEX8) {
        byte colors[256 * 4], indices[256], mapped[256];
        for (i = 0; i < 256; ++i)
            indices[i] = i;
        Hyena_ExpandPalette(indices, colors, 256, desc->palette, desc->palette_bpp);
        for (i = 0; i < 256; ++i)
            mapped[i] = findclosestpalmatch(colors[i * 4], colors[i * 4 + 1], colors[i * 4 + 2], colors[i * 4 + 3]);
        const byte * source = desc->pixels;
        for (i = 0; i < count; ++i)
            pixels[i] = mapped[source[i]];
    } else {
        const byte * source = desc->pixels;
        for (i = 0; i < count; ++i, source += 4)
            pixels[i] = findclosestpalmatch(source[0], source[1], source[2], source[3]);
    }

    memset(texture, 0, sizeof(*texture));
    strncpy(texture->name, desc->identifier, sizeof(texture->name) - 1);
    texture->width  = desc->width;
    texture->height = desc->height;
    texture->used   = qtrue;
    texture->transparent_color = 255;
    texture->data = pixels;
    if (texture_index >= numcachepics)
        numcachepics = texture_index + 1;
} /* Hyena_BackendUploadTexture */

static void
Hyena_Identity(float * matrix)
{
    int i;

    for (i = 0; i < 16; ++i)
        matrix[i] = 0.0f;
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
}

static void
Hyena_Multiply(float * left, const float * right)
{
    float result[16];
    int row, column, i;

    for (column = 0; column < 4; ++column)
        for (row = 0; row < 4; ++row) {
            result[column * 4 + row] = 0.0f;
            for (i = 0; i < 4; ++i)
                result[column * 4 + row] += left[i * 4 + row] * right[column * 4 + i];
        }
    memcpy(left, result, sizeof(result));
}

static void
Hyena_ApplyRotation(float x, float y, float z, qboolean zyx)
{
    float rx[16], ry[16], rz[16];

    Hyena_Identity(rx);
    Hyena_Identity(ry);
    Hyena_Identity(rz);
    rx[5]  = cosf(x);
    rx[9]  = -sinf(x);
    rx[6]  = sinf(x);
    rx[10] = cosf(x);
    ry[0]  = cosf(y);
    ry[8]  = sinf(y);
    ry[2]  = -sinf(y);
    ry[10] = cosf(y);
    rz[0]  = cosf(z);
    rz[4]  = -sinf(z);
    rz[1]  = sinf(z);
    rz[5]  = cosf(z);
    if (zyx) {
        Hyena_Multiply(hyena_matrix, rz);
        Hyena_Multiply(hyena_matrix, ry);
        Hyena_Multiply(hyena_matrix, rx);
    } else {
        Hyena_Multiply(hyena_matrix, rx);
        Hyena_Multiply(hyena_matrix, ry);
        Hyena_Multiply(hyena_matrix, rz);
    }
}

static void
Hyena_TransformVertex(const vertex_t * input, hyena_viewvert_t * output)
{
    vec3_t world, local;

    world[0] = hyena_matrix[0] * input->xyz.x + hyena_matrix[4] * input->xyz.y + hyena_matrix[8] * input->xyz.z
      + hyena_matrix[12];
    world[1] = hyena_matrix[1] * input->xyz.x + hyena_matrix[5] * input->xyz.y + hyena_matrix[9] * input->xyz.z
      + hyena_matrix[13];
    world[2] = hyena_matrix[2] * input->xyz.x + hyena_matrix[6] * input->xyz.y + hyena_matrix[10] * input->xyz.z
      + hyena_matrix[14];
    VectorSubtract(world, r_origin, local);
    output->x = DotProduct(local, vright);
    output->y = DotProduct(local, vup);
    output->z = DotProduct(local, vpn);
    output->u = input->uv.u;
    output->v = input->uv.v;
}

static hyena_viewvert_t
Hyena_IntersectNear(const hyena_viewvert_t * inside, const hyena_viewvert_t * outside)
{
    hyena_viewvert_t result;
    float fraction = (HYENA_NEAR_CLIP - inside->z) / (outside->z - inside->z);

    result.x = inside->x + fraction * (outside->x - inside->x);
    result.y = inside->y + fraction * (outside->y - inside->y);
    result.z = HYENA_NEAR_CLIP;
    result.u = inside->u + fraction * (outside->u - inside->u);
    result.v = inside->v + fraction * (outside->v - inside->v);
    return result;
}

static int
Hyena_ClipNear(const hyena_viewvert_t * input, int count, hyena_viewvert_t * output)
{
    int i, output_count = 0;

    for (i = 0; i < count; ++i) {
        const hyena_viewvert_t * current  = &input[i];
        const hyena_viewvert_t * previous = &input[(i + count - 1) % count];
        qboolean current_inside  = current->z >= HYENA_NEAR_CLIP;
        qboolean previous_inside = previous->z >= HYENA_NEAR_CLIP;
        if (current_inside != previous_inside) {
            output[output_count++] =
              current_inside ? Hyena_IntersectNear(current, previous) : Hyena_IntersectNear(previous, current);
        }
        if (current_inside)
            output[output_count++] = *current;
    }
    return output_count;
}

static byte
Hyena_ShadeTexel(byte texel)
{
    const byte * rgb;

    if (hyena_texture_mode == HYE_REPLACE)
        return texel;

    rgb = (const byte *) &d_8to24table[texel];
    return findclosestpalmatch(
        (byte) (rgb[0] * hyena_color[0]), (byte) (rgb[1] * hyena_color[1]), (byte) (rgb[2] * hyena_color[2]), 255);
}

static byte
Hyena_BlendPixel(byte source, byte destination)
{
    const byte * src = (const byte *) &d_8to24table[source];
    const byte * dst = (const byte *) &d_8to24table[destination];
    float alpha      = hyena_color[3];
    int r, g, b;

    if (!hyena_blend)
        return source;

    if (hyena_destination_blend == HYE_ONE) {
        r = (int) (src[0] * alpha + dst[0]);
        g = (int) (src[1] * alpha + dst[1]);
        b = (int) (src[2] * alpha + dst[2]);
    } else if (hyena_destination_blend == HYE_ONE_MINUS_SRC_COLOR) {
        r = (int) (src[0] * alpha + dst[0] * (1.0f - src[0] / 255.0f));
        g = (int) (src[1] * alpha + dst[1] * (1.0f - src[1] / 255.0f));
        b = (int) (src[2] * alpha + dst[2] * (1.0f - src[2] / 255.0f));
    } else {
        r = (int) (src[0] * alpha + dst[0] * (1.0f - alpha));
        g = (int) (src[1] * alpha + dst[1] * (1.0f - alpha));
        b = (int) (src[2] * alpha + dst[2] * (1.0f - alpha));
    }
    return findclosestpalmatch((byte) bound(0, r, 255), (byte) bound(0, g, 255), (byte) bound(0, b, 255), 255);
}

static byte
Hyena_FogPixel(byte source, float inv_z)
{
    const byte * src;
    float factor;

    if (!hyena_fog || hyena_fog_end <= hyena_fog_start)
        return source;

    factor = bound(0.0f, ((1.0f / inv_z) - hyena_fog_start) / (hyena_fog_end - hyena_fog_start), 1.0f);
    if (factor <= 0.0f)
        return source;

    src = (const byte *) &d_8to24table[source];
    return findclosestpalmatch((byte) (src[0] + factor * (hyena_fog_color[0] - src[0])),
             (byte) (src[1] + factor * (hyena_fog_color[1] - src[1])),
             (byte) (src[2] + factor * (hyena_fog_color[2] - src[2])), 255);
}

static float
Hyena_Edge(float ax, float ay, float bx, float by, float px, float py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static void
Hyena_RasterizeTriangle(const hyena_viewvert_t * view)
{
    hyena_screenvert_t vertex[3];
    cachepic_t * texture = NULL;
    float area;
    int i, x, y, min_x, max_x, min_y, max_y;

    if (hyena_draw_textured && hyena_texture_enabled && hyena_texture >= 0 && hyena_texture < numcachepics &&
      cachepics[hyena_texture].used)
        texture = &cachepics[hyena_texture];
    for (i = 0; i < 3; ++i) {
        vertex[i].inv_z    = 1.0f / view[i].z;
        vertex[i].x        = xcenter + xscale * view[i].x * vertex[i].inv_z;
        vertex[i].y        = ycenter - yscale * view[i].y * vertex[i].inv_z;
        vertex[i].u_over_z = view[i].u * vertex[i].inv_z;
        vertex[i].v_over_z = view[i].v * vertex[i].inv_z;
    }
    area = Hyena_Edge(vertex[0].x, vertex[0].y, vertex[1].x, vertex[1].y, vertex[2].x, vertex[2].y);
    if (fabsf(area) < 0.0001f)
        return;

    if (hyena_cull && area <= 0.0f)
        return;

    min_x = max(r_refdef.vrect.x, (int) floorf(min(vertex[0].x, min(vertex[1].x, vertex[2].x))));
    max_x = min(r_refdef.vrectright - 1, (int) ceilf(max(vertex[0].x, max(vertex[1].x, vertex[2].x))));
    min_y = max(r_refdef.vrect.y, (int) floorf(min(vertex[0].y, min(vertex[1].y, vertex[2].y))));
    max_y = min(r_refdef.vrectbottom - 1, (int) ceilf(max(vertex[0].y, max(vertex[1].y, vertex[2].y))));
    for (y = min_y; y <= max_y; ++y)
        for (x = min_x; x <= max_x; ++x) {
            float w0 = Hyena_Edge(vertex[1].x, vertex[1].y, vertex[2].x, vertex[2].y, x + 0.5f, y + 0.5f);
            float w1 = Hyena_Edge(vertex[2].x, vertex[2].y, vertex[0].x, vertex[0].y, x + 0.5f, y + 0.5f);
            float w2 = Hyena_Edge(vertex[0].x, vertex[0].y, vertex[1].x, vertex[1].y, x + 0.5f, y + 0.5f);
            float inv_z, mapped_z;
            int depth, offset;
            byte source;
            if ((area > 0 && (w0 < 0 || w1 < 0 || w2 < 0)) || (area < 0 && (w0 > 0 || w1 > 0 || w2 > 0)))
                continue;
            w0      /= area;
            w1      /= area;
            w2      /= area;
            inv_z    = w0 * vertex[0].inv_z + w1 * vertex[1].inv_z + w2 * vertex[2].inv_z;
            mapped_z = 32767.0f * (1.0f - hyena_depth_near)
              - (32767.0f - inv_z * 32768.0f) * (hyena_depth_far - hyena_depth_near);
            depth  = (int) mapped_z + (int) hyena_depth_offset;
            offset = y * d_zwidth + x;
            if (depth < d_pzbuffer[offset])
                continue;
            if (texture) {
                float u = (w0 * vertex[0].u_over_z + w1 * vertex[1].u_over_z + w2 * vertex[2].u_over_z) / inv_z;
                float v = (w0 * vertex[0].v_over_z + w1 * vertex[1].v_over_z + w2 * vertex[2].v_over_z) / inv_z;
                int tx  = ((int) floorf(u * texture->width) % texture->width + texture->width) % texture->width;
                int ty  = ((int) floorf(v * texture->height) % texture->height + texture->height) % texture->height;
                source = texture->data[ty * texture->width + tx];
                if (source == texture->transparent_color)
                    continue;
                source = Hyena_ShadeTexel(source);
            } else {
                source = findclosestpalmatch((byte) (hyena_color[0] * 255.0f), (byte) (hyena_color[1] * 255.0f),
                    (byte) (hyena_color[2] * 255.0f), 255);
            }
            source = Hyena_FogPixel(source, inv_z);
            d_viewbuffer[d_scantable[y] + x] = Hyena_BlendPixel(source, d_viewbuffer[d_scantable[y] + x]);
            if (hyena_depth_write)
                d_pzbuffer[offset] = (short) bound(-32768, depth, 32767);
        }
} /* Hyena_RasterizeTriangle */

void
Hyena_SetTextureMode(int mode)
{
    hyena_texture_mode = mode;
}

void
Hyena_SetColor(float r, float g, float b, float a)
{
    hyena_color[0] = r;
    hyena_color[1] = g;
    hyena_color[2] = b;
    hyena_color[3] = a;
}

void
Hyena_EnableCapability(int capability)
{
    if (capability == HYE_BLEND)
        hyena_blend = qtrue;
    else if (capability == HYE_TEXTURE_2D)
        hyena_texture_enabled = qtrue;
    else if (capability == HYE_CULL_FACE)
        hyena_cull = qtrue;
}

void
Hyena_DisableCapability(int capability)
{
    if (capability == HYE_BLEND)
        hyena_blend = qfalse;
    else if (capability == HYE_TEXTURE_2D)
        hyena_texture_enabled = qfalse;
    else if (capability == HYE_CULL_FACE)
        hyena_cull = qfalse;
}

void
Hyena_DepthMask(qboolean enabled)
{
    hyena_depth_write = enabled;
}

void
Hyena_BeginVertices(int mode)
{
    hyena_vertex_mode = mode;
    Hyena_Identity(hyena_matrix);
}

void
Hyena_Translate(float x, float y, float z)
{
    float m[16];

    Hyena_Identity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
    Hyena_Multiply(hyena_matrix, m);
}

void
Hyena_Scale(float x, float y, float z)
{
    float m[16];

    Hyena_Identity(m);
    m[0]  = x;
    m[5]  = y;
    m[10] = z;
    Hyena_Multiply(hyena_matrix, m);
}

void
Hyena_RotateXYZ(float x, float y, float z)
{
    Hyena_ApplyRotation(x, y, z, qfalse);
}

void
Hyena_RotateZYX(float z, float y, float x)
{
    Hyena_ApplyRotation(x, y, z, qtrue);
}

void
Hyena_FlushMatrices(void)
{
}

vertex_t *
Hyena_AllocateMemoryForVertices(int count)
{
    return malloc(sizeof(vertex_t) * count);
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
    int i, first, step;

    (void) vertex_precision;
    hyena_draw_textured = texture_precision != HYE_TEXTURE_NOTEXTURE;
    first = hyena_vertex_mode == HYE_TRIANGLES ? 0 : 1;
    step  = hyena_vertex_mode == HYE_TRIANGLES ? 3 : 1;
    if (count >= 3) {
        for (i = first; i + 1 < count; i += step) {
            hyena_viewvert_t triangle[3], clipped[4];
            int clipped_count, j;
            int strip_odd = hyena_vertex_mode == HYE_TRIANGLE_STRIP && ((i - 1) & 1);
            int i0        = hyena_vertex_mode == HYE_TRIANGLES ?
              i :
              (hyena_vertex_mode == HYE_TRIANGLE_STRIP ? (strip_odd ? i : i - 1) : 0);
            int i1 = strip_odd ? i - 1 : i;
            int i2 = i + 1;
            Hyena_TransformVertex(&vertices[i0], &triangle[0]);
            Hyena_TransformVertex(&vertices[i1], &triangle[1]);
            Hyena_TransformVertex(&vertices[i2], &triangle[2]);
            clipped_count = Hyena_ClipNear(triangle, 3, clipped);
            for (j = 1; j + 1 < clipped_count; ++j) {
                hyena_viewvert_t clipped_triangle[3] = { clipped[0], clipped[j], clipped[j + 1] };
                Hyena_RasterizeTriangle(clipped_triangle);
            }
        }
    }
    free(vertices);
}

void
Hyena_EndVertices(void)
{
}

void
Hyena_SetShadeMode(int mode)
{
    (void) mode;
}

void
Hyena_SetBlendFunction(int source, int destination)
{
    (void) source;
    hyena_destination_blend = destination;
}

void
Hyena_SetDepthRange(float near_value, float far_value)
{
    hyena_depth_near = near_value;
    hyena_depth_far  = far_value;
}

void
Hyena_SetDepthOffset(float offset)
{
    hyena_depth_offset = offset;
}

void
Hyena_BackendBindTexture(int texture, int min_filter, int mag_filter)
{
    (void) min_filter;
    (void) mag_filter;
    hyena_texture = texture;
}

void
Hyena_BackendDestroyTexture(int texture)
{
    if (texture < 0 || texture >= numcachepics || !cachepics[texture].used)
        return;

    cachepics[texture].used    = qfalse;
    cachepics[texture].name[0] = '\0';
    cachepics[texture].data    = NULL;
}

void
Hyena_BackendInitTextureResources(void)
{
    Hyena_ResetTextureRegistry();
    Hyena_InitTextureSettings("1024", false, MAX_CACHED_PICS);
}

void
Hyena_BackendTextureFilter(int texture, int min_filter, int mag_filter)
{
    (void) texture;
    (void) min_filter;
    (void) mag_filter;
}

void
Hyena_Set2D(void)
{
}

static byte
Hyena_2DModulate(byte texel, const hyena_2d_quad_t * quad)
{
    if (quad->r == 255 && quad->g == 255 && quad->b == 255)
        return texel;

    return findclosestpalmatch((byte) (host_basepal[texel * 3] * quad->r / 255),
             (byte) (host_basepal[texel * 3 + 1] * quad->g / 255), (byte) (host_basepal[texel * 3 + 2] * quad->b / 255),
             255);
}

static void
Hyena_2DWritePixel(int x, int y, byte color)
{
    if (r_pixbytes == 1)
        vid.buffer[y * vid.rowbytes + x] = color;
    else
        ((unsigned short *) (vid.buffer + y * vid.rowbytes))[x] = d_8to16table[color];
}

void
Hyena_Draw2DQuads(const hyena_2d_quad_t * quads, int count, int texture_index, unsigned int flags)
{
    static const byte dither[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
    cachepic_t * texture         = NULL;
    int i;

    if ((flags & HYE_2D_TEXTURED) && texture_index >= 0 && texture_index < numcachepics &&
      cachepics[texture_index].used)
        texture = &cachepics[texture_index];
    if ((flags & HYE_2D_TEXTURED) && !texture)
        return;

    for (i = 0; i < count; ++i) {
        const hyena_2d_quad_t * quad = &quads[i];
        int x0, y0, x1, y1;
        int x, y;

        if (quad->x1 <= quad->x0 || quad->y1 <= quad->y0)
            continue;
        x0 = max(0, (int) quad->x0);
        y0 = max(0, (int) quad->y0);
        x1 = min((int) vid.width, (int) quad->x1);
        y1 = min((int) vid.height, (int) quad->y1);

        for (y = y0; y < y1; ++y) {
            for (x = x0; x < x1; ++x) {
                byte color;
                if ((flags & HYE_2D_BLEND) && quad->a < 255 && (quad->a >> 4) <= dither[((y & 3) << 2) | (x & 3)])
                    continue;
                if (texture) {
                    float s = (x + 0.5f - quad->x0) / (quad->x1 - quad->x0);
                    float t = (y + 0.5f - quad->y0) / (quad->y1 - quad->y0);
                    int u   =
                      bound(0, (int) ((quad->u0 + (quad->u1 - quad->u0) * s) * texture->width), texture->width - 1);
                    int v = bound(
                        0, (int) ((quad->v0 + (quad->v1 - quad->v0) * t) * texture->height), texture->height - 1);
                    color = texture->data[v * texture->width + u];
                    if (color == texture->transparent_color)
                        continue;
                    color = Hyena_2DModulate(color, quad);
                } else {
                    color = findclosestpalmatch(quad->r, quad->g, quad->b, 255);
                }
                Hyena_2DWritePixel(x, y, color);
            }
        }
    }
} /* Hyena_Draw2DQuads */

void
Hyena_FogInit(void)
{
    hyena_fog = qfalse;
}

void
Hyena_FogEnable(void)
{
    hyena_fog = qtrue;
}

void
Hyena_FogDisable(void)
{
    hyena_fog = qfalse;
}

void
Hyena_FogSet(bool is_world_geometry, float start, float end, float red, float green, float blue, float alpha)
{
    (void) is_world_geometry;
    (void) alpha;
    hyena_fog_start    = start;
    hyena_fog_end      = end;
    hyena_fog_color[0] = red * 2.55f;
    hyena_fog_color[1] = green * 2.55f;
    hyena_fog_color[2] = blue * 2.55f;
}

void
Hyena_DrawIndexedTriangles(const hyena_colored_vertex_t * vertices, int vertex_count, const unsigned short * indices,
  int index_count, qboolean textured)
{
    int i, j;

    (void) vertex_count;
    Hyena_BeginVertices(HYE_TRIANGLES);
    for (i = 0; i < index_count; i += 3) {
        const byte * color  = vertices[indices[i]].color;
        vertex_t * triangle = Hyena_AllocateMemoryForVertices(3);
        Hyena_SetColor(color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f, color[3] / 255.0f);
        for (j = 0; j < 3; ++j) {
            const hyena_colored_vertex_t * source = &vertices[indices[i + j]];
            Hyena_2DTextureCoord(&triangle[j], source->uv[0], source->uv[1]);
            Hyena_VertexXYZ(&triangle[j], source->xyz[0], source->xyz[1], source->xyz[2]);
        }
        Hyena_DrawVertices(
            triangle, 3, textured ? HYE_TEXTURE_32BITFLOAT : HYE_TEXTURE_NOTEXTURE, HYE_VERTEX_32BITFLOAT);
    }
    Hyena_EndVertices();
    Hyena_SetColor(1, 1, 1, 1);
}

struct hyena_buffer_s {
    void *   data;
    int      target;
    qboolean owned;
};
size_t
Hyena_StaticVertexBudget(void)
{
    return 0;
}

qboolean
Hyena_NeedsColorArray(void)
{
    return false;
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
        buffer->data = malloc(size);
        if (!buffer->data)
            Sys_Error("Hyena_UpdateBuffer: out of memory");
        if (data)
            memcpy(buffer->data, data, size);
    } else {
        buffer->data = (void *) data;
    }
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

static const void *
Hyena_ArrayPointer(const hyena_array_t * array)
{
    return array->buffer ? (const byte *) array->buffer->data + (size_t) array->data : array->data;
}

static void
Hyena_SubmitArrays(int mode, const hyena_arrays_t * arrays, int count, const unsigned short * indices)
{
    const byte * positions = Hyena_ArrayPointer(&arrays->position);
    const byte * uvs       = Hyena_ArrayPointer(&arrays->uv);
    int width   = arrays->position.type == HYE_SHORT ? 2 : arrays->position.type == HYE_UBYTE ? 1 : 4;
    int stride  = arrays->position.stride ? arrays->position.stride : width * 3;
    float scale = arrays->scale ? arrays->scale : 1;
    int i;
    vertex_t * vertices = Hyena_AllocateMemoryForVertices(count);

    for (i = 0; i < count; ++i) {
        int index      = indices ? indices[i] : i;
        const byte * p = positions + index * stride;
        float x, y, z;
        if (width == 2) {
            const short * v = (const short *) p;
            x = v[0];
            y = v[1];
            z = v[2];
        } else if (width == 1) {
            x = p[0];
            y = p[1];
            z = p[2];
        } else {
            const float * v = (const float *) p;
            x = v[0];
            y = v[1];
            z = v[2];
        }
        Hyena_VertexXYZ(vertices + i, x * scale, y * scale, z * scale);
        if (uvs) {
            const float * uv = (const float *) (uvs + index * (arrays->uv.stride ? arrays->uv.stride : 8));
            Hyena_2DTextureCoord(vertices + i, uv[0], uv[1]);
        }
    }
    Hyena_BeginVertices(mode);
    Hyena_DrawVertices(vertices, count, uvs ? HYE_TEXTURE_32BITFLOAT : HYE_TEXTURE_NOTEXTURE, HYE_VERTEX_32BITFLOAT);
    Hyena_EndVertices();
} /* Hyena_SubmitArrays */

void
Hyena_DrawArrays(int mode, const hyena_arrays_t * arrays, int count)
{
    Hyena_SubmitArrays(mode, arrays, count, NULL);
}

void
Hyena_DrawElements(int mode, const hyena_arrays_t * arrays, int count, hyena_buffer_t * indices, size_t offset)
{
    Hyena_SubmitArrays(mode, arrays, count, (const unsigned short *) ((const byte *) indices->data + offset));
}

void
Hyena_RotationMatrix(float * matrix, float x, float y, float z)
{
    float saved[16];

    memcpy(saved, hyena_matrix, sizeof(saved));
    Hyena_Identity(hyena_matrix);
    Hyena_ApplyRotation(x, y, z, qtrue);
    memcpy(matrix, hyena_matrix, sizeof(saved));
    memcpy(hyena_matrix, saved, sizeof(saved));
}

void
Hyena_SetCurrentColor(float r, float g, float b, float a)
{
    Hyena_SetColor(r, g, b, a);
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
    return false;
}

qboolean
Hyena_SeparateViewMatrix(void)
{
    return true;
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
