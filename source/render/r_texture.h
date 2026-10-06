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

// r_texture.h -- Texture registry/backend interface

#ifndef _R_TEXTURE_H_
#define _R_TEXTURE_H_

#define HYENA_MAX_TEXTURES 1024

typedef struct {
    char     identifier[MAX_QPATH];
    int      width, height, original_width, original_height;
    int      bpp, mipmaps;
    qboolean used, keep, luminance, lightmap;
    int      filter;
} hyena_texture_t;

// Storage formats are independent of the native API's enum values.
enum { HYE_PIXELS_RGBA8, HYE_PIXELS_L8, HYE_PIXELS_RGBA5551, HYE_PIXELS_RGBA4444 };
enum { HYE_NEAREST, HYE_LINEAR, HYE_NEAREST_MIPMAP_NEAREST,
       HYE_LINEAR_MIPMAP_NEAREST, HYE_NEAREST_MIPMAP_LINEAR, HYE_LINEAR_MIPMAP_LINEAR };

extern hyena_texture_t * hyena_textures;
extern cvar_t hyena_lightmap_16bit, gl_max_size, gl_picmip;
extern int hyena_filter_min, hyena_filter_max;

void
Hyena_ResetTextureRegistry(void);
void
Hyena_InitTextureSettings(const char * maximum, qboolean mip_filtering, int limit);
typedef void (*hyena_upload_level_fn)(int texture, int level, int format, int width, int height,
  int x, int y, qboolean replace, const void * pixels);
void
Hyena_PrepareTextureUpload(int texture, const hyena_texture_desc_t * desc, int compact_format,
  hyena_upload_level_fn upload);
void
Hyena_BackendInitTextureResources(void);
void
Hyena_BackendUploadTexture(int texture, const hyena_texture_desc_t * desc);
void
Hyena_BackendDestroyTexture(int texture);
void
Hyena_BackendBindTexture(int texture, int min_filter, int mag_filter);
void
Hyena_BackendTextureFilter(int texture, int min_filter, int mag_filter);
void
Hyena_BackendUploadLevel(int texture, int level, int format, int width, int height,
  int x, int y, qboolean replace, const void * pixels);

#endif // _R_TEXTURE_H_
