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

// Shared CPU texture preparation; no graphics API dependencies.

#ifndef R_PIXELS_H
#define R_PIXELS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HYENA_MAX_MIP_LEVELS 16

int
Hyena_TextureMipCount(int width, int height, int mip_levels, int min_width, int min_height);
void
Hyena_PreparePixelLevel(unsigned char * dest, const void * source, int width, int height, int out_width,
  int out_height, int bpp, int stretch, int linear);

typedef struct {
    unsigned char * pixels;
    int             width, height;
    size_t          size;
} hyena_mip_t;

typedef struct {
    unsigned char * storage;
    size_t          size;
    int             count;
    hyena_mip_t     levels[HYENA_MAX_MIP_LEVELS];
} hyena_pixels_t;

int
Hyena_TextureDimension(int size, int round_down, int minimum, int maximum, int reduction);
void
Hyena_ResamplePixels(const unsigned char * source, int width, int height, unsigned char * dest, int out_width,
  int out_height, int bpp, int linear);
void
Hyena_ExpandPalette(
    const unsigned char * source, unsigned char * dest, size_t count, const unsigned char * palette, int palette_bpp);
void
Hyena_PackRGBA4444(const unsigned char * source, unsigned short * dest, size_t count, int red_low);
void
Hyena_PackRGBA5551(const unsigned char * source, unsigned short * dest, size_t count);
void
Hyena_PackRGB565(const unsigned char * source, unsigned short * dest, size_t count);
// Dimensions must be multiples of the tile dimensions; buffers must not overlap.

void
Hyena_TilePixels(
    const unsigned char * source, unsigned char * dest, int row_bytes, int height, int tile_bytes, int tile_height);
// Indexed mips use nearest sampling; color mips average channels. Padding
// extends edge texels. The returned storage is owned by the caller.

int
Hyena_PreparePixels(hyena_pixels_t * result, const void * source, int width, int height, int out_width,
  int out_height, int bpp, int stretch, int linear, int mip_levels, int min_mip_width, int min_mip_height);
void
Hyena_FreePixels(hyena_pixels_t * pixels);

#ifdef __cplusplus
}
#endif
#endif // ifndef R_PIXELS_H
