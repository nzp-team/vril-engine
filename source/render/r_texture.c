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

// Shared Hyena texture entry points. Backends own the native texture
// representation; renderer code describes only the source pixels and use.

#include "../nzportable_def.h"
#include "r_texture.h"
#include "r_pixels.h"

// Optional compact colored lightmaps. Keep full precision by default.

cvar_t hyena_lightmap_16bit = { "hyena_lightmap_16bit", "1", true };

hyena_texture_t * hyena_textures;
static int hyena_texture_capacity;
static int hyena_texture_limit = HYENA_MAX_TEXTURES;
static qboolean hyena_override_filters;
cvar_t gl_max_size   = { "gl_max_size", "1024" };
cvar_t gl_picmip     = { "gl_picmip", "0" };
int hyena_filter_min = HYE_LINEAR_MIPMAP_NEAREST;
int hyena_filter_max = HYE_LINEAR;

void
Hyena_ResetTextureRegistry(void)
{
    if (hyena_textures)
        memset(hyena_textures, 0, hyena_texture_capacity * sizeof(*hyena_textures));
}

int
Hyena_CreateTexture(const hyena_texture_desc_t * desc)
{
    hyena_texture_t * record;
    int texture;

    if (!desc || !desc->identifier || !desc->identifier[0] || !desc->pixels ||
      desc->width <= 0 || desc->height <= 0 ||
      (desc->format != HYE_TEXTURE_INDEX8 && desc->format != HYE_TEXTURE_RGBA8) ||
      (desc->palette && desc->palette_bpp != 3 && desc->palette_bpp != 4))
        return -1;

    texture = Hyena_FindTexture(desc->identifier);
    if (texture >= 0 && !desc->update) {
        if (desc->keep)
            Hyena_KeepTexture(texture);
        return texture;
    }
    if (texture < 0) {
        for (texture = 0; texture < hyena_texture_capacity; ++texture)
            if (!hyena_textures[texture].used)
                break;
        if (texture == hyena_texture_capacity) {
            int capacity = hyena_texture_capacity ? hyena_texture_capacity * 2 : 128;
            if (capacity > hyena_texture_limit)
                capacity = hyena_texture_limit;
            if (capacity <= hyena_texture_capacity)
                Sys_Error("Hyena_CreateTexture: out of textures");
            hyena_texture_t * records = realloc(hyena_textures, capacity * sizeof(*records));
            if (!records)
                Sys_Error("Hyena: out of texture records");
            hyena_textures = records;
            memset(records + hyena_texture_capacity, 0, (capacity - hyena_texture_capacity) * sizeof(*records));
            hyena_texture_capacity = capacity;
        }
    }
    record = &hyena_textures[texture];
    // Keep ownership and original dimensions here; the backend reports stored dimensions.
    snprintf(record->identifier, sizeof(record->identifier), "%s", desc->identifier);
    record->original_width  = desc->width;
    record->original_height = desc->height;
    record->keep     |= desc->keep;
    record->filter    = desc->filter == HYE_FILTER_LINEAR ? HYE_LINEAR : HYE_NEAREST;
    record->lightmap  = desc->lightmap;
    record->luminance = desc->lightmap && desc->format == HYE_TEXTURE_INDEX8;
    Hyena_BackendUploadTexture(texture, desc);
    record->used = true;
    return texture;
} /* Hyena_CreateTexture */

int
Hyena_FindTexture(const char * identifier)
{
    int i;

    if (!identifier || !identifier[0])
        return -1;

    for (i = 0; i < hyena_texture_capacity; ++i) {
        if (hyena_textures[i].used && !strcmp(hyena_textures[i].identifier, identifier))
            return i;
    }
    return -1;
}

void
Hyena_GetTextureSize(int texture, int * width, int * height, int * original_width, int * original_height)
{
    if (texture < 0 || texture >= hyena_texture_capacity || !hyena_textures[texture].used) {
        *width = *height = *original_width = *original_height = 0;
        return;
    }
    *width           = hyena_textures[texture].width;
    *height          = hyena_textures[texture].height;
    *original_width  = hyena_textures[texture].original_width;
    *original_height = hyena_textures[texture].original_height;
}

void
Hyena_BindTexture(int texture)
{
    const hyena_texture_t * record;
    int min_filter, mag_filter;

    if (texture < 0)
        return;

    if (texture >= hyena_texture_capacity || !hyena_textures[texture].used)
        Sys_Error("Hyena_BindTexture: unused texture %d", texture);
    record     = &hyena_textures[texture];
    min_filter = mag_filter = record->filter;
    if (hyena_override_filters) {
        min_filter = record->mipmaps ? hyena_filter_min : hyena_filter_max;
        mag_filter = hyena_filter_max;
    }
    if (r_retro.value && (hyena_override_filters || !record->lightmap))
        min_filter = mag_filter = HYE_NEAREST;
    Hyena_BackendBindTexture(texture, min_filter, mag_filter);
}

void
Hyena_KeepTexture(int texture)
{
    if (texture < 0 || texture >= hyena_texture_capacity || !hyena_textures[texture].used)
        Sys_Error("Hyena_KeepTexture: invalid texture %d\n", texture);
    hyena_textures[texture].keep = true;
}

void
Hyena_DestroyTexture(int texture)
{
    if (texture < 0 || texture >= hyena_texture_capacity || !hyena_textures[texture].used ||
      hyena_textures[texture].keep)
        return;

    Hyena_BackendDestroyTexture(texture);
    memset(&hyena_textures[texture], 0, sizeof(hyena_textures[texture]));
}

void
Hyena_DestroyTextures(void)
{
    int i;

    for (i = 0; i < hyena_texture_capacity; ++i)
        Hyena_DestroyTexture(i);
}

void
Hyena_InitTextures(void)
{
    Cvar_RegisterVariable(&hyena_lightmap_16bit);
    Hyena_BackendInitTextureResources();
}

static const char * hyena_filter_names[] = {
    "GL_NEAREST",               "GL_LINEAR",                "GL_NEAREST_MIPMAP_NEAREST",
    "GL_LINEAR_MIPMAP_NEAREST", "GL_NEAREST_MIPMAP_LINEAR", "GL_LINEAR_MIPMAP_LINEAR"
};

static void
Hyena_TextureMode_f(void)
{
    int mode, texture;

    if (Cmd_Argc() == 1) {
        Con_Printf("%s\n", hyena_filter_names[hyena_filter_min]);
        return;
    }
    for (mode = 0; mode < 6; ++mode)
        if (!Q_strcasecmp((char *) hyena_filter_names[mode], Cmd_Argv(1)))
            break;
    if (mode == 6) {
        Con_Printf("bad filter name\n");
        return;
    }
    hyena_filter_min = mode;
    hyena_filter_max = mode & 1;
    for (texture = 0; texture < hyena_texture_capacity; ++texture) {
        if (hyena_textures[texture].used && hyena_textures[texture].mipmaps)
            Hyena_BackendTextureFilter(texture, hyena_filter_min, hyena_filter_max);
    }
}

void
Hyena_InitTextureSettings(const char * maximum, qboolean mip_filtering, int limit)
{
    hyena_texture_limit    = limit;
    hyena_override_filters = mip_filtering;
    gl_max_size.string     = (char *) maximum;
    Cvar_RegisterVariable(&gl_max_size);
    Cvar_RegisterVariable(&gl_picmip);
    if (mip_filtering)
        Cmd_AddCommand("gl_texturemode", Hyena_TextureMode_f);
}

void
Hyena_PrepareTextureUpload(int texture, const hyena_texture_desc_t * desc, int compact_format,
  hyena_upload_level_fn upload)
{
    hyena_texture_t * record = &hyena_textures[texture];
    const void * pixels      = desc->pixels;
    byte * expanded = NULL, * region = NULL;
    unsigned short * packed = NULL;
    int width = desc->width, height = desc->height;
    int storage = HYE_PIXELS_RGBA8, bpp = 4;
    int min_filter = desc->filter == HYE_FILTER_LINEAR ? HYE_LINEAR : HYE_NEAREST;
    int mag_filter = min_filter;

    if (desc->lightmap) {
        int source_bpp = desc->format == HYE_TEXTURE_INDEX8 ? 1 : 4;
        qboolean replace;
        storage = source_bpp == 1 ? HYE_PIXELS_L8 :
          hyena_lightmap_16bit.value ? compact_format : HYE_PIXELS_RGBA8;
        bpp     = storage == HYE_PIXELS_L8 ? 1 : storage == HYE_PIXELS_RGBA8 ? 4 : 2;
        replace = !record->used || record->width != width || record->height != height || record->bpp != bpp;
        if (!replace && desc->update_width) {
            const byte * source = (const byte *) pixels + (size_t) desc->update_y * desc->row_stride
              + desc->update_x * source_bpp;
            int row;
            width  = desc->update_width;
            height = desc->update_height;
            region = malloc((size_t) width * height * source_bpp);
            if (!region)
                Sys_Error("Hyena: out of texture update memory");
            for (row = 0; row < height; ++row)
                memcpy(region + (size_t) row * width * source_bpp,
                  source + (size_t) row * desc->row_stride, width * source_bpp);
            pixels = region;
        }
        if (bpp == 2) {
            packed = malloc((size_t) width * height * sizeof(*packed));
            if (!packed)
                Sys_Error("Hyena: out of texture conversion memory");
            if (storage == HYE_PIXELS_RGBA5551)
                Hyena_PackRGBA5551(pixels, packed, width * height);
            else
                Hyena_PackRGBA4444(pixels, packed, width * height, false);
            pixels = packed;
        }
        upload(texture, 0, storage, width, height, desc->update_x, desc->update_y, replace, pixels);
        record->width   = desc->width;
        record->height  = desc->height;
        record->mipmaps = 0;
    } else {
        hyena_pixels_t prepared;
        int i;
        if (desc->format == HYE_TEXTURE_INDEX8) {
            expanded = malloc((size_t) width * height * 4);
            if (!expanded)
                Sys_Error("Hyena: out of palette expansion memory");
            Hyena_ExpandPalette(pixels, expanded, (size_t) width * height,
              desc->palette ? desc->palette : (const byte *) d_8to24table,
              desc->palette ? desc->palette_bpp : 4);
            pixels = expanded;
        }
        width  = Hyena_TextureDimension(width, false, 4, (int) gl_max_size.value, (int) gl_picmip.value);
        height = Hyena_TextureDimension(height, false, 4, (int) gl_max_size.value, (int) gl_picmip.value);
        // These backends consume a box-filtered chain; native tiled storage can prepare its own levels.
        if (!Hyena_PreparePixels(&prepared, pixels, desc->width, desc->height, width, height, 4,
          true, false, desc->mip_levels ? HYENA_MAX_MIP_LEVELS - 1 : 0, 1, 1))
            Sys_Error("Hyena: could not prepare texture");
        for (i = 0; i < prepared.count; ++i) {
            const hyena_mip_t * level = &prepared.levels[i];
            upload(texture, i, storage, level->width, level->height, 0, 0, true, level->pixels);
        }
        record->mipmaps = prepared.count - 1;
        record->width   = width;
        record->height  = height;
        Hyena_FreePixels(&prepared);
    }
    record->bpp = bpp;
    if (desc->mip_levels > 0)
        min_filter = desc->filter == HYE_FILTER_LINEAR ? hyena_filter_min : HYE_NEAREST_MIPMAP_NEAREST;
    Hyena_BackendTextureFilter(texture, min_filter, mag_filter);
    free(expanded);
    free(region);
    free(packed);
} /* Hyena_PrepareTextureUpload */

int
Hyena_LoadTexture(const char * identifier, int width, int height, const void * pixels, hyena_texture_format_t format,
  hyena_texture_filter_t filter, int mip_levels, qboolean alpha, qboolean keep, qboolean stretch_to_power_of_two)
{
    hyena_texture_desc_t desc = { 0 };

    desc.identifier = identifier;
    desc.pixels     = pixels;
    desc.width      = width;
    desc.height     = height;
    desc.format     = format;
    desc.filter     = filter;
    desc.mip_levels = mip_levels;
    desc.alpha      = alpha;
    desc.keep       = keep;
    desc.stretch_to_power_of_two = stretch_to_power_of_two;
    return Hyena_CreateTexture(&desc);
}

int
Hyena_LoadPalettedTexture(const char * identifier, int width, int height, const void * pixels, const void * palette,
  int palette_bpp, const void * palette_hint, hyena_texture_filter_t filter, int mip_levels, qboolean keep,
  qboolean stretch_to_power_of_two)
{
    hyena_texture_desc_t desc = { 0 };

    desc.identifier   = identifier;
    desc.pixels       = pixels;
    desc.palette      = palette;
    desc.palette_hint = palette_hint;
    desc.width        = width;
    desc.height       = height;
    desc.format       = HYE_TEXTURE_INDEX8;
    desc.filter       = filter;
    desc.mip_levels   = mip_levels;
    desc.palette_bpp  = palette_bpp;
    desc.alpha        = palette_bpp == 4;
    desc.keep         = keep;
    desc.stretch_to_power_of_two = stretch_to_power_of_two;
    return Hyena_CreateTexture(&desc);
}

int
Hyena_LoadLightmap(
    const char * identifier, int width, int height, const void * pixels, hyena_texture_format_t format, qboolean update)
{
    hyena_texture_desc_t desc = { 0 };

    desc.identifier = identifier;
    desc.pixels     = pixels;
    desc.width      = width;
    desc.height     = height;
    desc.format     = format;
    desc.filter     = HYE_FILTER_LINEAR;
    desc.lightmap   = true;
    desc.update     = update;
    return Hyena_CreateTexture(&desc);
}

int
Hyena_UpdateLightmap(const char * identifier, int width, int height, const void * pixels,
  hyena_texture_format_t format, int x, int y, int update_width, int update_height, int row_stride)
{
    hyena_texture_desc_t desc = { 0 };

    desc.identifier    = identifier;
    desc.pixels        = pixels;
    desc.width         = width;
    desc.height        = height;
    desc.update_x      = x;
    desc.update_y      = y;
    desc.update_width  = update_width;
    desc.update_height = update_height;
    desc.row_stride    = row_stride;
    desc.format        = format;
    desc.filter        = HYE_FILTER_LINEAR;
    desc.lightmap      = true;
    desc.update        = true;
    return Hyena_CreateTexture(&desc);
}
