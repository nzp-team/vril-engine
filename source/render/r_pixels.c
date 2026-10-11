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

// Shared Hyena texture preparation.

#include "r_pixels.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int
Hyena_TextureDimension(int size, int round_down, int minimum, int maximum, int reduction)
{
    int dimension = 1;

    while (dimension < size && dimension <= INT_MAX / 2)
        dimension *= 2;
    if (round_down && dimension > size)
        dimension /= 2;
    while (reduction-- > 0 && dimension > 1)
        dimension /= 2;
    if (maximum > 0 && dimension > maximum)
        dimension = maximum;
    return dimension < minimum ? minimum : dimension;
}

void
Hyena_ResamplePixels(const unsigned char * source, int width, int height, unsigned char * dest, int out_width,
  int out_height, int bpp, int linear)
{
    int x, y, c;
    unsigned int step  = ((unsigned int) width << 16) / out_width;
    unsigned int ystep = ((unsigned int) height << 16) / out_height;

    if (width == out_width && height == out_height) {
        memcpy(dest, source, (size_t) width * height * bpp);
        return;
    }
    if (bpp == 1) {
        for (y = 0; y < out_height; ++y, dest += out_width) {
            const unsigned char * row = source + width * (y * height / out_height);
            unsigned int frac         = step >> 1;
            for (x = 0; x < out_width; ++x, frac += step)
                dest[x] = row[frac >> 16];
        }
        return;
    }
    if (!linear && bpp == 4) {
        for (y = 0; y < out_height; ++y, dest += out_width * 4) {
            const unsigned char * row = source + (size_t) width * (y * height / out_height) * 4;
            unsigned int frac         = step >> 1;
            for (x = 0; x < out_width; ++x, frac += step)
                memcpy(dest + x * 4, row + (frac >> 16) * 4, 4);
        }
        return;
    }
    for (y = 0; y < out_height; ++y) {
        unsigned int fy = (unsigned int) y * ystep;
        int sy = linear ? fy >> 16 : (unsigned int) y * height / out_height;
        int ny = sy + 1 < height ? sy + 1 : sy;
        unsigned int fx = linear ? 0 : step >> 1;
        for (x = 0; x < out_width; ++x) {
            int sx = fx >> 16;
            int nx = sx + 1 < width ? sx + 1 : sx;
            const unsigned char * a = source + ((size_t) sy * width + sx) * bpp;
            unsigned char * out     = dest + ((size_t) y * out_width + x) * bpp;
            if (!linear || bpp == 1) {
                if (bpp == 1)
                    *out = *a;
                else if (bpp == 4)
                    memcpy(out, a, 4);
                else
                    memcpy(out, a, bpp);
                fx += step;
                continue;
            }
            for (c = 0; c < bpp; ++c) {
                int top    = a[c] + ((source[((size_t) sy * width + nx) * bpp + c] - a[c]) * (int) (fx & 65535) >> 16);
                int bottom = source[((size_t) ny * width + sx) * bpp + c];
                bottom += ((source[((size_t) ny * width + nx) * bpp + c] - bottom) * (int) (fx & 65535)) >> 16;
                out[c]  = top + (((bottom - top) * (int) (fy & 65535)) >> 16);
            }
            fx += step;
        }
    }
} /* Hyena_ResamplePixels */

void
Hyena_ExpandPalette(
    const unsigned char * source, unsigned char * dest, size_t count, const unsigned char * palette, int palette_bpp)
{
    size_t i;

    for (i = 0; i < count; ++i) {
        const unsigned char * color = palette + source[i] * palette_bpp;
        memcpy(dest + i * 4, color, 3);
        dest[i * 4 + 3] = palette_bpp == 4 ? color[3] : 255;
    }
}

void
Hyena_PackRGBA4444(const unsigned char * source, unsigned short * dest, size_t count, int red_low)
{
    size_t i;

    for (i = 0; i < count; ++i, source += 4) {
        unsigned r = source[0] >> 4, g = source[1] >> 4;
        unsigned b = source[2] >> 4, a = source[3] >> 4;
        dest[i] = red_low ? r | (g << 4) | (b << 8) | (a << 12) : (r << 12) | (g << 8) | (b << 4) | a;
    }
}

void
Hyena_PackRGBA5551(const unsigned char * source, unsigned short * dest, size_t count)
{
    size_t i;

    for (i = 0; i < count; ++i, source += 4)
        dest[i] = ((source[0] >> 3) << 11) | ((source[1] >> 3) << 6)
          | ((source[2] >> 3) << 1) | (source[3] >> 7);
}

void
Hyena_PackRGB565(const unsigned char * source, unsigned short * dest, size_t count)
{
    size_t i;

    for (i = 0; i < count; ++i, source += 4)
        dest[i] = ((source[0] >> 3) << 11) | ((source[1] >> 2) << 5) | (source[2] >> 3);
}

void
Hyena_TilePixels(
    const unsigned char * source, unsigned char * dest, int row_bytes, int height, int tile_bytes, int tile_height)
{
    int x, y, row;

    if (tile_bytes == 16 && tile_height == 8) {
        for (y = 0; y < height; y += 8)
            for (x = 0; x < row_bytes; x += 16)
                for (row = 0; row < 8; ++row) {
                    memcpy(dest, source + (size_t) (y + row) * row_bytes + x, 16);
                    dest += 16;
                }
        return;
    }
    for (y = 0; y < height; y += tile_height)
        for (x = 0; x < row_bytes; x += tile_bytes)
            for (row = 0; row < tile_height; ++row) {
                memcpy(dest, source + (size_t) (y + row) * row_bytes + x, tile_bytes);
                dest += tile_bytes;
            }
}

static void
Hyena_ReducePixels(const hyena_mip_t * in, hyena_mip_t * out, int bpp)
{
    int x, y, c;

    for (y = 0; y < out->height; ++y)
        for (x = 0; x < out->width; ++x) {
            int nx = x * 2 + (in->width > 1);
            int ny = y * 2 + (in->height > 1);
            for (c = 0; c < bpp; ++c) {
                unsigned sum = in->pixels[((size_t) y * 2 * in->width + x * 2) * bpp + c];
                sum += in->pixels[((size_t) y * 2 * in->width + nx) * bpp + c];
                sum += in->pixels[((size_t) ny * in->width + x * 2) * bpp + c];
                sum += in->pixels[((size_t) ny * in->width + nx) * bpp + c];
                out->pixels[((size_t) y * out->width + x) * bpp + c] = sum >> 2;
            }
        }
}

int
Hyena_TextureMipCount(int width, int height, int mip_levels, int min_width, int min_height)
{
    int count = 1;

    while (count <= mip_levels && count < HYENA_MAX_MIP_LEVELS && (width > 1 || height > 1)) {
        width  = width > 1 ? width / 2 : 1;
        height = height > 1 ? height / 2 : 1;
        if (width < min_width || height < min_height)
            break;
        ++count;
    }
    return count;
}

void
Hyena_PreparePixelLevel(unsigned char * dest, const void * source, int width, int height, int out_width, int out_height,
  int bpp, int stretch, int linear)
{
    int x, y;

    if (stretch || out_width < width || out_height < height) {
        Hyena_ResamplePixels(source, width, height, dest, out_width, out_height, bpp, linear);
        return;
    }
    for (y = 0; y < out_height; ++y) {
        int sy = y < height ? y : height - 1;
        unsigned char * row = dest + (size_t) y * out_width * bpp;
        memcpy(row, (const unsigned char *) source + (size_t) sy * width * bpp, (size_t) width * bpp);
        for (x = width; x < out_width; ++x)
            memcpy(row + x * bpp, row + (width - 1) * bpp, bpp);
    }
}

int
Hyena_PreparePixels(hyena_pixels_t * result, const void * source, int width, int height, int out_width, int out_height,
  int bpp, int stretch, int linear, int mip_levels, int min_mip_width, int min_mip_height)
{
    int i;

    memset(result, 0, sizeof(*result));
    if (!source || width <= 0 || height <= 0 || out_width <= 0 || out_height <= 0 || bpp < 1 || bpp > 4 ||
      min_mip_width < 1 || min_mip_height < 1 || width > 32768 || height > 32768 || out_width > 32768 ||
      out_height > 32768)
        return 0;

    result->count = Hyena_TextureMipCount(out_width, out_height, mip_levels, min_mip_width, min_mip_height);
    for (i = 0; i < result->count; ++i) {
        hyena_mip_t * level = &result->levels[i];
        if ((size_t) out_width > SIZE_MAX / bpp / out_height)
            return 0;

        level->width  = out_width;
        level->height = out_height;
        level->size   = (size_t) out_width * out_height * bpp;
        if (level->size > SIZE_MAX - result->size)
            return 0;

        result->size += level->size;
        out_width     = out_width > 1 ? out_width / 2 : 1;
        out_height    = out_height > 1 ? out_height / 2 : 1;
    }
    result->storage = malloc(result->size);
    if (!result->storage)
        return 0;

    for (i = 0; i < result->count; ++i) {
        hyena_mip_t * level = &result->levels[i];
        level->pixels = i ? result->levels[i - 1].pixels + result->levels[i - 1].size : result->storage;
        if (i && bpp != 1) {
            Hyena_ReducePixels(&result->levels[i - 1], level, bpp);
        } else {
            Hyena_PreparePixelLevel(
                level->pixels, source, width, height, level->width, level->height, bpp, i || stretch, linear);
        }
    }
    return 1;
} /* Hyena_PreparePixels */

void
Hyena_FreePixels(hyena_pixels_t * pixels)
{
    free(pixels->storage);
    memset(pixels, 0, sizeof(*pixels));
}
