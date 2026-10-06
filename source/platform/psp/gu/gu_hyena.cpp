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

// Copyright (C) 1996-1997 Id Software, Inc.
// Copyright (C) 2007 Peter Mackay and Chris Swindle.
// Copyright (C) 2008-2009 Crow_bar.

// libtxc_dxtn
// Version: 0.1b
// Fixed some bugs with dxt1 compression
// Copyright (C) 2004 Roland Scheidegger
// All Rights Reserved.
// Copyright (C) 2006-2008 Franck Charlet
// All Rights Reserved.
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

extern "C" {
#include "../../../nzportable_def.h"
#include "../../../render/r_texture.h"
}
#include "../../../render/r_dxt.h"
#include "../../../render/r_pixels.h"
#include "../clipping.hpp"
#include "../vram.hpp"
#include <malloc.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>
#include <vram.h>

static int current_vertex_mode;

void
Hyena_SetTextureMode(int texture_mode)
{
    switch (texture_mode) {
        case HYE_MODULATE:
            sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
            break;
        case HYE_REPLACE:
            sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
            break;
        default:
            Sys_Error("Received unknown texture mode [%d]\n", texture_mode);
            break;
    }
}

void
Hyena_SetColor(float red, float green, float blue, float alpha)
{
    sceGuColor(GU_COLOR(red, green, blue, alpha));
}

static int
Hyena_ResolveCapability(int capability)
{
    int gu_capability = -1;

    switch (capability) {
        case HYE_BLEND:
            gu_capability = GU_BLEND;
            break;
        case HYE_ALPHA_TEST: return GU_ALPHA_TEST;

        case HYE_DEPTH_TEST: return GU_DEPTH_TEST;

        case HYE_CULL_FACE:
            gu_capability = GU_CULL_FACE;
            break;
        case HYE_TEXTURE_2D:
            gu_capability = GU_TEXTURE_2D;
            break;
        default:
            Sys_Error("Received unknown capability [%d]\n", capability);
            break;
    }

    return gu_capability;
}

void
Hyena_EnableCapability(int capability)
{
    int gu_capability = Hyena_ResolveCapability(capability);

    sceGuEnable(gu_capability);
}

void
Hyena_DisableCapability(int capability)
{
    int gu_capability = Hyena_ResolveCapability(capability);

    sceGuDisable(gu_capability);
}

void
Hyena_DepthMask(qboolean value)
{
    // GU_TRUE masks depth writes.
    sceGuDepthMask(value ? GU_FALSE : GU_TRUE);
}

static int
Hyena_ResolveVertexMode(int mode)
{
    int gu_mode = -1;

    switch (mode) {
        case HYE_TRIANGLE_FAN:
            gu_mode = GU_TRIANGLE_FAN;
            break;
        case HYE_TRIANGLES:
            gu_mode = GU_TRIANGLES;
            break;
        case HYE_TRIANGLE_STRIP:
            gu_mode = GU_TRIANGLE_STRIP;
            break;
        default:
            Sys_Error("Received mode capability [%d]\n", mode);
            break;
    }

    return gu_mode;
}

void
Hyena_BeginVertices(int mode)
{
    int gu_mode = Hyena_ResolveVertexMode(mode);

    current_vertex_mode = gu_mode;
    sceGumPushMatrix();
}

void
Hyena_Translate(float x, float y, float z)
{
    const ScePspFVector3 translation = { x, y, z };

    sceGumTranslate(&translation);
}

void
Hyena_Scale(float x, float y, float z)
{
    const ScePspFVector3 scale = { x, y, z };

    sceGumScale(&scale);
}

void
Hyena_RotateXYZ(float x, float y, float z)
{
    const ScePspFVector3 rotation = { x, y, z };

    sceGumRotateXYZ(&rotation);
}

void
Hyena_RotateZYX(float z, float y, float x)
{
    const ScePspFVector3 rotation = { x, y, z };

    sceGumRotateZYX(&rotation);
}

void
Hyena_FlushMatrices(void)
{
    sceGumUpdateMatrix();
}

vertex_t *
Hyena_AllocateMemoryForVertices(int num_vertices)
{
    return (vertex_t *) (sceGuGetMemory(sizeof(vertex_t) * num_vertices));
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

static int
Hyena_ResolveTexturePrecision(int texture_precision)
{
    int gu_texture_precision = -1;

    switch (texture_precision) {
        case HYE_TEXTURE_NOTEXTURE:
            gu_texture_precision = 0;
            break;
        case HYE_TEXTURE_32BITFLOAT:
            gu_texture_precision = GU_TEXTURE_32BITF;
            break;
        default:
            Sys_Error("Received texture precision mode [%d]\n", texture_precision);
            break;
    }

    return gu_texture_precision;
}

static int
Hyena_ResolveVertexPrecision(int vertex_precision)
{
    int gu_vertex_precision = -1;

    switch (vertex_precision) {
        case HYE_VERTEX_32BITFLOAT:
            gu_vertex_precision = GU_VERTEX_32BITF;
            break;
        default:
            Sys_Error("Received vertex precision mode [%d]\n", vertex_precision);
            break;
    }

    return gu_vertex_precision;
}

void
Hyena_DrawVertices(vertex_t * vertices, int num_vertices, int texture_precision, int vertex_precision)
{
    int gu_texture_precision = Hyena_ResolveTexturePrecision(texture_precision);
    int gu_vertex_precision  = Hyena_ResolveVertexPrecision(vertex_precision);
    int i;

    // Untextured GU vertices contain positions without the leading UVs.
    if (!gu_texture_precision) {
        vertex_xyz_t * vertices_xyz = (vertex_xyz_t *) (sceGuGetMemory(sizeof(vertex_xyz_t) * num_vertices));

        for (i = 0; i < num_vertices; i++) {
            vertices_xyz[i].x = vertices[i].xyz.x;
            vertices_xyz[i].y = vertices[i].xyz.y;
            vertices_xyz[i].z = vertices[i].xyz.z;
        }

        sceGuDrawArray(current_vertex_mode, gu_vertex_precision, num_vertices, 0, vertices_xyz);
    } else {
        sceGuDrawArray(current_vertex_mode, gu_texture_precision | gu_vertex_precision, num_vertices, 0, vertices);
    }
}

void
Hyena_EndVertices(void)
{
    current_vertex_mode = -1;
    sceGumPopMatrix();
}

static int
Hyena_ResolveShadeMode(int shade_mode)
{
    int gu_shade_mode = -1;

    switch (shade_mode) {
        case HYE_SMOOTH:
            gu_shade_mode = GU_SMOOTH;
            break;
        case HYE_FLAT:
            gu_shade_mode = GU_FLAT;
            break;
        default:
            Sys_Error("Received shade mode [%d]\n", shade_mode);
            break;
    }

    return gu_shade_mode;
}

void
Hyena_SetShadeMode(int shade_mode)
{
    int gu_shade_mode = Hyena_ResolveShadeMode(shade_mode);

    sceGuShadeModel(gu_shade_mode);
}

static int
Hyena_ResolveBlendFunction(int blend_function)
{
    int gu_blend_function = -1;

    switch (blend_function) {
        case HYE_ONE_MINUS_SRC_ALPHA:
            gu_blend_function = GU_ONE_MINUS_SRC_ALPHA;
            break;
        case HYE_ONE:
            gu_blend_function = GU_FIX;
            break;
        case HYE_DST_COLOR: return GU_DST_COLOR;

        case HYE_SRC_COLOR: return GU_SRC_COLOR;

        case HYE_ONE_MINUS_SRC_COLOR:
            gu_blend_function = GU_ONE_MINUS_SRC_COLOR;
            break;
        case HYE_SRC_ALPHA:
            gu_blend_function = GU_SRC_ALPHA;
            break;
        default:
            Sys_Error("Received blend mode [%d]\n", blend_function);
            break;
    }

    return gu_blend_function;
}

void
Hyena_SetBlendFunction(int source_blend, int dest_blend)
{
    int gu_source_blend = Hyena_ResolveBlendFunction(source_blend);
    int gu_dest_blend   = Hyena_ResolveBlendFunction(dest_blend);

    sceGuBlendFunc(GU_ADD, gu_source_blend, gu_dest_blend, 0, 0xFFFFFFFF);
}

void
Hyena_SetDepthRange(float near, float far)
{
    sceGuDepthRange((int) (65535.0f * near), (int) (65535.0f * far));
}

void
Hyena_SetDepthOffset(float offset)
{
    sceGuDepthOffset((int) (offset * 256.0f));
}

void
Hyena_Set2D(void)
{
    sceGuViewport(glx, gly, glwidth, glheight);
    sceGuScissor(0, 0, glwidth, glheight);
    sceGuEnable(GU_BLEND);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
}

void
Hyena_Draw2DQuads(const hyena_2d_quad_t * quads, int count, int texture, unsigned int flags)
{
    int i;

    if (!count)
        return;

    if (flags & HYE_2D_BLEND) {
        sceGuEnable(GU_BLEND);
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    } else {
        sceGuDisable(GU_BLEND);
    }

    if (flags & HYE_2D_TEXTURED) {
        typedef struct {
            float        u, v;
            unsigned int color;
            float        x, y, z;
        } psp_2d_vertex_t;
        psp_2d_vertex_t * vertices;
        int width, height, original_width, original_height;

        Hyena_BindTexture(texture);
        Hyena_GetTextureSize(texture, &width, &height, &original_width, &original_height);
        (void) original_width;
        (void) original_height;
        sceGuEnable(GU_TEXTURE_2D);
        sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
        sceGuTexFilter(flags & HYE_2D_LINEAR ? GU_LINEAR : GU_NEAREST, flags & HYE_2D_LINEAR ? GU_LINEAR : GU_NEAREST);
        vertices = (psp_2d_vertex_t *) sceGuGetMemory(sizeof(*vertices) * count * 2);
        for (i = 0; i < count; ++i) {
            const hyena_2d_quad_t * q = &quads[i];
            unsigned int color        = GU_RGBA(q->r, q->g, q->b, q->a);
            vertices[i * 2].u         = q->u0 * width;
            vertices[i * 2].v         = q->v0 * height;
            vertices[i * 2].color     = color;
            vertices[i * 2].x         = q->x0;
            vertices[i * 2].y         = q->y0;
            vertices[i * 2].z         = 0;
            vertices[i * 2 + 1].u     = q->u1 * width;
            vertices[i * 2 + 1].v     = q->v1 * height;
            vertices[i * 2 + 1].color = color;
            vertices[i * 2 + 1].x     = q->x1;
            vertices[i * 2 + 1].y     = q->y1;
            vertices[i * 2 + 1].z     = 0;
        }
        sceGuDrawArray(
            GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, count * 2, 0, vertices);
    } else {
        typedef struct {
            unsigned int color;
            float        x, y, z;
        } psp_2d_color_vertex_t;
        psp_2d_color_vertex_t * vertices;

        sceGuDisable(GU_TEXTURE_2D);
        vertices = (psp_2d_color_vertex_t *) sceGuGetMemory(sizeof(*vertices) * count * 2);
        for (i = 0; i < count; ++i) {
            const hyena_2d_quad_t * q = &quads[i];
            unsigned int color        = GU_RGBA(q->r, q->g, q->b, q->a);
            vertices[i * 2].color     = color;
            vertices[i * 2].x         = q->x0;
            vertices[i * 2].y         = q->y0;
            vertices[i * 2].z         = 0;
            vertices[i * 2 + 1].color = color;
            vertices[i * 2 + 1].x     = q->x1;
            vertices[i * 2 + 1].y     = q->y1;
            vertices[i * 2 + 1].z     = 0;
        }
        sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, count * 2, 0, vertices);
    }

    sceGuColor(0xffffffff);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuDisable(GU_BLEND);
} // Hyena_Draw2DQuads

void
Hyena_FogSet(bool is_world_geometry, float start, float end, float red, float green, float blue, float alpha)
{
    unsigned int color = is_world_geometry ? GU_COLOR(0.5f, 0.5f, 0.5f, alpha) :
      GU_COLOR(red * 0.01f, green * 0.01f, blue * 0.01f, alpha);

    sceGuFog(start, end, color);
}

void
Hyena_FogEnable(void)
{
    sceGuEnable(GU_FOG);
}

void
Hyena_FogDisable(void)
{
    sceGuDisable(GU_FOG);
}

void
Hyena_FogInit(void)
{
}

static void
Convert_DXT5(unsigned char * data, unsigned int size)
{
    unsigned short * src = (unsigned short *) data;
    int i;
    int j;
    unsigned short converted[8];

    for (j = 0; size >= 16; size -= 16, j++) {
        converted[4] = src[1];
        converted[5] = src[2];
        converted[6] = src[3];
        converted[7] = src[0];

        converted[0] = src[6];
        converted[1] = src[7];
        converted[2] = src[4];
        converted[3] = src[5];
        for (i = 0; i < 8; i++)
            src[i] = converted[i];
        src += 8;
    }
}

static void
Convert_DXT3(unsigned char * data, unsigned int size)
{
    unsigned short * src = (unsigned short *) data;
    int i;
    int j;
    unsigned short converted[8];

    for (j = 0; size >= 16; size -= 16, j++) {
        converted[4] = src[0];
        converted[5] = src[1];
        converted[6] = src[2];
        converted[7] = src[3];

        converted[0] = src[6];
        converted[1] = src[7];
        converted[2] = src[4];
        converted[3] = src[5];
        for (i = 0; i < 8; i++)
            src[i] = converted[i];
        src += 8;
    }
}

static void
Convert_DXT1(unsigned char * data, unsigned int size)
{
    unsigned short * src = (unsigned short *) data;
    int i;
    int j;
    unsigned short converted[4];

    for (j = 0; size >= 8; size -= 8, j++) {
        converted[0] = src[2];
        converted[1] = src[3];
        converted[2] = src[0];
        converted[3] = src[1];
        for (i = 0; i < 4; i++)
            src[i] = converted[i];
        src += 4;
    }
}

static int
Hyena_CompressGUTexture(
    int srccomps, int width, int height, const unsigned char * source, unsigned int format, unsigned char * dest)
{
    int shared_format = format == GU_PSM_DXT1 ? HYE_DXT1 : format == GU_PSM_DXT3 ? HYE_DXT3 : HYE_DXT5;
    int size = Hyena_CompressDXT(srccomps, width, height, source, shared_format, dest);

    if (format == GU_PSM_DXT1)
        Convert_DXT1(dest, size);
    else if (format == GU_PSM_DXT3)
        Convert_DXT3(dest, size);
    else
        Convert_DXT5(dest, size);
    return size;
}

int zombie_skins[2][2];

typedef struct {
    int    format, swizzle;
    byte * palette, * ram, * vram;
} hyena_gu_texture_t;
static hyena_gu_texture_t hyena_gu_textures[HYENA_MAX_TEXTURES];
static const unsigned int * hyena_current_palette;

void
VID_SetPalette4(unsigned char * clut4pal);

void
Hyena_InitTextureStorage(void)
{
    Hyena_ResetTextureRegistry();
    memset(hyena_gu_textures, 0, sizeof(hyena_gu_textures));
}

int
GL_GetTexSize(int format, int w, int h, int bpp)
{
    int size = 0;

    if (bpp == 0 && (format != -1)) {
        switch (format) {
            case GU_PSM_T4:
            case GU_PSM_DXT1:
                size = w * h / 2;
                break;
            case GU_PSM_T8:
            case GU_PSM_DXT3:
            case GU_PSM_DXT5:
                size = w * h;
                break;
            case GU_PSM_5650:
            case GU_PSM_5551:
            case GU_PSM_4444:
                size = w * h * 2;
                break;
            case GU_PSM_8888:
                size = w * h * 4;
                break;
        }
    } else {
        size = w * h * bpp;
    }
    return size;
}

void
VID_SetPaletteTX();
extern qboolean last_palette_wasnt_tx;
void
Hyena_BackendBindTexture(int texture_index, int min_filter, int mag_filter)
{
    if (currenttexture == texture_index) {
        return;
    }

    currenttexture = texture_index;

    const hyena_gu_texture_t & texture = hyena_gu_textures[texture_index];
    hyena_texture_t &record = hyena_textures[texture_index];

    sceGuTexMode(texture.format, record.mipmaps, 0, texture.swizzle);

    if (record.luminance) {
        extern unsigned int d_8to24tableLM[256];
        if (hyena_current_palette != d_8to24tableLM)
            Hyena_SetPalette(d_8to24tableLM, 256);
    } else if (texture.format == GU_PSM_T8 && last_palette_wasnt_tx) {
        VID_SetPaletteTX();
    } else if (texture.format == GU_PSM_T4) {
        VID_SetPalette4(texture.palette);
    }

    sceGuTexFilter(min_filter & 1 ? GU_LINEAR : GU_NEAREST, mag_filter & 1 ? GU_LINEAR : GU_NEAREST);

    const void * const texture_memory = texture.vram ? texture.vram : texture.ram;
    sceGuTexImage(0, record.width, record.height, record.width, texture_memory);
}

void
Hyena_BindTextureLod(int texture_index, int lod_mode, float bias, float slope, qboolean mipmapped, qboolean nearest)
{
    if (currenttexture == texture_index) {
        return;
    }

    currenttexture = texture_index;

    const hyena_gu_texture_t & texture = hyena_gu_textures[texture_index];
    hyena_texture_t &record = hyena_textures[texture_index];

    sceGuTexMode(texture.format, record.mipmaps, 0, texture.swizzle);

    if (nearest) {
        sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    } else {
        if (record.mipmaps > 0 && mipmapped) {
            sceGuTexSlope(slope); // the near from 0 slope is the lower (=best detailed) mipmap it uses
            sceGuTexFilter(GU_LINEAR_MIPMAP_LINEAR, GU_LINEAR_MIPMAP_LINEAR);
            sceGuTexLevelMode(lod_mode, bias); // manual slope setting
        } else {
            sceGuTexFilter(record.filter & 1 ? GU_LINEAR : GU_NEAREST, record.filter & 1 ? GU_LINEAR : GU_NEAREST);
        }
    }

    const void * texture_memory = texture.vram ? texture.vram : texture.ram;
    sceGuTexImage(0, record.width, record.height, record.width, texture_memory);

    if (record.mipmaps > 0 && mipmapped) {
        int size   = (record.width * record.height * record.bpp);
        int offset = size;
        int div    = 2;

        for (int i = 1; i <= record.mipmaps; i++) {
            const void * texture_memory2 = ((const byte *) texture_memory) + offset;
            sceGuTexImage(i, record.width / div, record.height / div, record.width / div, texture_memory2);
            offset += size / (div * div);
            div    *= 2;
        }
    }
} // Hyena_BindTextureLod

void
Hyena_BackendInitTextureResources(void)
{
    Hyena_InitTextureSettings("512", false, HYENA_MAX_TEXTURES);
}

void
Hyena_BackendTextureFilter(int texture, int min_filter, int mag_filter)
{
    hyena_textures[texture].filter = mag_filter;
    currenttexture = -1;
    Hyena_BackendBindTexture(texture, min_filter, mag_filter);
}

void
Hyena_BackendDestroyTexture(int texture_index)
{
    hyena_gu_texture_t & texture = hyena_gu_textures[texture_index];

    free(texture.palette);
    free(texture.ram);
    if (texture.vram)
        vfree(texture.vram);
    memset(&texture, 0, sizeof(texture));
    currenttexture = -1;
}

static int
Hyena_GUTextureFormat(const hyena_texture_desc_t * desc)
{
    if (desc->format == HYE_TEXTURE_INDEX8)
        return desc->palette ? GU_PSM_T4 : GU_PSM_T8;

    if (desc->lightmap)
        return hyena_lightmap_16bit.value ? GU_PSM_4444 : GU_PSM_8888;

    switch ((int) r_texcompr.value) {
        case 0:
            return GU_PSM_8888;

        case 1:
            return GU_PSM_DXT1;

        case 3:
            return GU_PSM_DXT3;

        case 16:
            return GU_PSM_4444;

        default:
            return GU_PSM_DXT5;
    }
}

int total_overbudget_texturemem;

void
Hyena_BackendUploadTexture(int texture_index, const hyena_texture_desc_t * desc)
{
    byte * scratch;
    int format, bpp, minimum, maximum, width, height;
    int mip_levels, mip_count, i;
    size_t buffer_size = 0;
    byte * destination;

    format = Hyena_GUTextureFormat(desc);
    if (!desc->lightmap && desc->format == HYE_TEXTURE_RGBA8 && desc->smooth_alpha)
        format = GU_PSM_DXT5;
    bpp     = desc->format == HYE_TEXTURE_INDEX8 ? 1 : 4;
    minimum = desc->lightmap ? 16 : 32;
    maximum = !desc->lightmap && bpp == 4 && psp_system_model == PSP_MODEL_PHAT ? 128 : 512;
    if (gl_max_size.value > 0 && maximum > (int) gl_max_size.value)
        maximum = (int) gl_max_size.value;
    int stretch = desc->stretch_to_power_of_two || format == GU_PSM_T4;
    int down    = r_tex_scale_down.value && stretch;
    width  = Hyena_TextureDimension(desc->width, down, minimum, maximum, (int) gl_picmip.value);
    height = Hyena_TextureDimension(desc->height, down, format == GU_PSM_T4 ? 16 : minimum, maximum,
        (int) gl_picmip.value);
    mip_levels = desc->lightmap || format == GU_PSM_T4 || format >= GU_PSM_DXT1 ? 0 : desc->mip_levels;
    mip_count  = Hyena_TextureMipCount(width, height, mip_levels, 32, 32);
    for (i = 0; i < mip_count; ++i)
        buffer_size += GL_GetTexSize(format, width >> i, height >> i, 0);

    hyena_gu_texture_t & texture = hyena_gu_textures[texture_index];
    hyena_texture_t &record      = hyena_textures[texture_index];
    if (record.width != width || record.height != height || texture.format != format ||
      record.mipmaps != mip_count - 1)
    {
        free(texture.ram);
        if (texture.vram)
            vfree(texture.vram);
        texture.ram  = NULL;
        texture.vram = NULL;
    }
    if (!texture.ram && !texture.vram) {
        texture.vram = static_cast<byte *>(vramalloc(buffer_size));
        if (!texture.vram) {
            texture.ram = static_cast<byte *>(memalign(16, buffer_size));
            if (!texture.ram)
                Sys_Error("Hyena: out of texture memory\n");
            total_overbudget_texturemem += buffer_size / 1024;
        }
    }
    destination = texture.vram ? texture.vram : texture.ram;
    byte * out = destination;
    scratch = static_cast<byte *>(malloc((size_t) width * height * bpp));
    if (!scratch)
        Sys_Error("Hyena: out of texture preparation memory");
    for (i = 0; i < mip_count; ++i) {
        hyena_mip_t level;
        level.width  = width >> i;
        level.height = height >> i;
        level.size   = (size_t) level.width * level.height * bpp;
        level.pixels = scratch;
        Hyena_PreparePixelLevel(scratch, desc->pixels, desc->width, desc->height, level.width, level.height, bpp,
          i || stretch, bpp == 4 && r_restexf.value);
        if (format >= GU_PSM_DXT1) {
            Hyena_CompressGUTexture(bpp, level.width, level.height, level.pixels, format, out);
        } else if (format == GU_PSM_T4) {
            if (!texture.palette)
                texture.palette = static_cast<byte *>(memalign(16, 64));
            byte * packed = static_cast<byte *>(malloc(level.size / 2));
            if (!texture.palette || !packed)
                Sys_Error("Hyena: out of palette memory\n");
            if (desc->palette_hint) {
                memcpy(texture.palette, desc->palette_hint, 64);
                convert_8bpp_to_4bpp_with_hint(level.pixels, static_cast<const byte *>(desc->palette),
                  desc->palette_bpp, level.width, level.height, packed,
                  static_cast<const byte *>(desc->palette_hint));
            } else {
                convert_8bpp_to_4bpp(level.pixels, static_cast<const byte *>(desc->palette), desc->palette_bpp,
                  level.width, level.height, packed, texture.palette);
            }
            Hyena_TilePixels(packed, out, level.width / 2, level.height, 16, 8);
            free(packed);
            sceKernelDcacheWritebackRange(texture.palette, 64);
        } else {
            int stored_bpp = format == GU_PSM_4444 ? 2 : bpp;
            if (format == GU_PSM_4444) {
                Hyena_PackRGBA4444(level.pixels, reinterpret_cast<unsigned short *>(level.pixels),
                  (size_t) level.width * level.height, true);
            }
            Hyena_TilePixels(level.pixels, out, level.width * stored_bpp, level.height, 16, 8);
        }
        out += GL_GetTexSize(format, level.width, level.height, 0);
    }
    sceKernelDcacheWritebackRange(destination, buffer_size);
    record.width    = width;
    record.height   = height;
    record.bpp      = format == GU_PSM_4444 ? 2 : bpp;
    texture.format  = format;
    texture.swizzle = format < GU_PSM_DXT1;
    record.mipmaps  = mip_count - 1;
    free(scratch);
    currenttexture = -1;
} // Hyena_BackendUploadTexture

void
Hyena_DrawIndexedTriangles(const hyena_colored_vertex_t * vertices, int vertex_count, const unsigned short * indices,
  int index_count, qboolean textured)
{
    unsigned short * gu_indices;
    void * gu_vertices;
    int format = GU_COLOR_8888 | GU_VERTEX_32BITF | GU_INDEX_16BIT;

    if (!index_count)
        return;

    gu_indices = (unsigned short *) sceGuGetMemory(index_count * sizeof(*gu_indices));
    memcpy(gu_indices, indices, index_count * sizeof(*gu_indices));
    if (textured) {
        gu_vertices = sceGuGetMemory(vertex_count * sizeof(*vertices));
        memcpy(gu_vertices, vertices, vertex_count * sizeof(*vertices));
        format |= GU_TEXTURE_32BITF;
    } else {
        typedef struct {
            byte  color[4];
            float xyz[3];
        } gu_colored_vertex_t;
        gu_colored_vertex_t * out = (gu_colored_vertex_t *) sceGuGetMemory(vertex_count * sizeof(*out));
        for (int i = 0; i < vertex_count; ++i) {
            memcpy(out[i].color, vertices[i].color, sizeof(out[i].color));
            memcpy(out[i].xyz, vertices[i].xyz, sizeof(out[i].xyz));
        }
        gu_vertices = out;
    }
    sceGumUpdateMatrix();
    sceGuDrawArray(GU_TRIANGLES, format, index_count, gu_indices, gu_vertices);
    sceGuColor(0xffffffff);
}

struct hyena_buffer_s {
    const void * data;
    qboolean     owned;
    int          target;
    size_t       size;
};

hyena_buffer_t *
Hyena_CreateBuffer(int target, const void * data, size_t size)
{
    hyena_buffer_t * buffer = (hyena_buffer_t *) calloc(1, sizeof(*buffer));

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
        free((void *) buffer->data);
    buffer->size  = size;
    buffer->owned = buffer->target == HYE_ARRAY_BUFFER;
    if (buffer->owned) {
        void * copy = memalign(16, size);
        if (!copy)
            Sys_Error("Hyena_UpdateBuffer: out of memory");
        if (data)
            memcpy(copy, data, size);
        buffer->data = copy;
    } else {
        buffer->data = data;
    }
    if (data)
        sceKernelDcacheWritebackRange((void *) buffer->data, size);
}

void
Hyena_DestroyBuffer(hyena_buffer_t * buffer)
{
    if (!buffer)
        return;

    if (buffer->owned)
        free((void *) buffer->data);
    free(buffer);
}

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

void *
Hyena_AllocateTransient(size_t size)
{
    return sceGuGetMemory(size);
}

void
Hyena_MorphWeight(int index, float weight)
{
    sceGuMorphWeight(index, weight);
}

void
Hyena_DrawPacked(int mode, int format, int morphs, int count,
  const void * vertices, const unsigned short * indices, unsigned int flags)
{
    int native_format = 0, stride = 0;
    int native_mode = mode == HYE_LINE_STRIP ? GU_LINE_STRIP : mode ==
      HYE_QUADS ? GU_TRIANGLE_FAN : Hyena_ResolveVertexMode(mode);

    if (format & HYE_PACKED_UV_FLOAT) {
        native_format |= GU_TEXTURE_32BITF;
        stride        += 8;
    }
    if (format & HYE_PACKED_UV_SHORT) {
        native_format |= GU_TEXTURE_16BIT;
        stride        += 4;
    }
    if (format & HYE_PACKED_COLOR) {
        native_format |= GU_COLOR_8888;
        stride        += 4;
    }
    if (format & HYE_PACKED_XYZ_FLOAT) {
        native_format |= GU_VERTEX_32BITF;
        stride        += 12;
    }
    if (format & HYE_PACKED_XYZ_BYTE) {
        native_format |= GU_VERTEX_8BIT;
        stride        += 4;
    }
    if (morphs > 1)
        native_format |= GU_VERTICES(morphs);
    if (indices)
        native_format |= GU_INDEX_16BIT;
    if (flags & HYE_CLIP_POLYGON) {
        const glvert_t * clipped = (const glvert_t *) vertices;
        size_t clipped_count     = count;
        if (quake::clipping::is_clipping_required(clipped, clipped_count))
            quake::clipping::clip(clipped, clipped_count, &clipped, &clipped_count);
        vertices    = clipped;
        count       = clipped_count;
        native_mode = GU_TRIANGLE_FAN;
        mode        = HYE_TRIANGLE_FAN;
    }
    if (!count)
        return;

    if (flags & HYE_COPY_VERTICES) {
        void * copy = sceGuGetMemory(count * stride * morphs);
        memcpy(copy, vertices, count * stride * morphs);
        vertices = copy;
    }
    if (mode == HYE_QUADS) {
        for (int i = 0; i < count; i += 4)
            sceGuDrawArray(GU_TRIANGLE_FAN, native_format, 4, 0, (const byte *) vertices + i * stride);
    } else {
        sceGuDrawArray(native_mode, native_format, count, indices, vertices);
    }
} // Hyena_DrawPacked

static const void *
Hyena_ArrayPointer(const hyena_array_t * array)
{
    return array->buffer ? (const byte *) array->buffer->data + (size_t) array->data : array->data;
}

static void
Hyena_SubmitArrays(int mode, const hyena_arrays_t * arrays, int count, const unsigned short * indices)
{
    const void * vertices;
    int format = HYE_PACKED_XYZ_FLOAT;

    if (arrays->uv.data || arrays->uv.buffer) {
        format  |= HYE_PACKED_UV_FLOAT;
        vertices = Hyena_ArrayPointer(&arrays->uv);
    } else if (arrays->color.data || arrays->color.buffer) {
        format  |= HYE_PACKED_COLOR;
        vertices = Hyena_ArrayPointer(&arrays->color);
    } else {
        vertices = Hyena_ArrayPointer(&arrays->position);
    }
    Hyena_DrawPacked(mode, format, 1, count, vertices, indices, arrays->flags);
}

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
Hyena_DepthFunction(int function)
{
    sceGuDepthFunc(function == HYE_EQUAL ? GU_EQUAL : GU_LEQUAL);
}

void
Hyena_AlphaFunction(int function, float value)
{
    (void) function;
    sceGuAlphaFunc(GU_GREATER, (int) (value * 255.0f + 0.5f), 0xff);
}

void
Hyena_TextureFilter(int filter)
{
    int mode = filter == HYE_FILTER_LINEAR ? GU_LINEAR : GU_NEAREST;

    sceGuTexFilter(mode, mode);
}

void
Hyena_TextureTransform(float uscale, float vscale, float uoffset, float voffset)
{
    sceGuTexScale(uscale, vscale);
    sceGuTexOffset(uoffset, voffset);
}

void
Hyena_InvalidateTextureBinding(void)
{
    currenttexture = -1;
}

void
Hyena_LoadViewMatrix(const float * matrix)
{
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadMatrix((const ScePspFMatrix4 *) matrix);
    sceGumUpdateMatrix();
    sceGumMatrixMode(GU_MODEL);
}

static void
Hyena_BlendConstant(float alpha)
{
    sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, GU_COLOR(alpha, alpha, alpha, alpha),
      GU_COLOR(1 - alpha, 1 - alpha, 1 - alpha, 1 - alpha));
}

void
Hyena_RotationMatrix(float * matrix, float x, float y, float z)
{
    const ScePspFVector3 rotation = { x, y, z };

    gumLoadIdentity((ScePspFMatrix4 *) matrix);
    gumRotateZYX((ScePspFMatrix4 *) matrix, &rotation);
}

void
Hyena_SetCurrentColor(float r, float g, float b, float a)
{
    sceGuColor(GU_COLOR(r, g, b, a));
}

void *
Hyena_MapBuffer(hyena_buffer_t * buffer)
{
    return (void *) buffer->data;
}

void
Hyena_UnmapBuffer(hyena_buffer_t * buffer)
{
    // GU reads RAM asynchronously; publish writes before submitting the buffer.
    sceKernelDcacheWritebackRange((void *) buffer->data, buffer->size);
}

void
Hyena_SetPalette(const unsigned int * rgba, int count)
{
    if (!rgba) {
        if (hyena_current_palette == d_8to24table)
            return;

        rgba  = d_8to24table;
        count = 256;
    }
    hyena_current_palette = rgba;
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceKernelDcacheWritebackRange((void *) rgba, count * sizeof(*rgba));
    sceGuClutLoad(count / 8, rgba);
    reloaded_pallete = 1;
    // Legacy indexed texture binds restore the default palette when needed.
    last_palette_wasnt_tx = rgba != d_8to24table;
}

qboolean
Hyena_PackedArraysSupported(void)
{
    return true;
}

qboolean
Hyena_SeparateArrays(void)
{
    return false;
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
    Hyena_SetTextureMode(HYE_REPLACE);
    if (alpha == 1)
        Hyena_SetBlendFunction(HYE_SRC_ALPHA, HYE_ONE_MINUS_SRC_ALPHA);
    else
        Hyena_BlendConstant(alpha);
}
