/*
NZ:P Universal Image loading
Copyright (C) 2025

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef _IMAGES_H_
#define _IMAGES_H_

#include "render/r_types.h"

void tex_filebase (char *in, char *out);
byte* Image_LoadPixels (char* filename, int image_format);
int Image_LoadImage(char* filename, int image_format, int filter, bool keep, bool mipmap);
int Image_LoadImageWithIdentifier(char *filename, char *identifier, int image_format,
  int filter, bool keep, bool mipmap);
int loadrgbafrompal (char* name, int width, int height, byte* data);
int loadpcxas4bpp (char* filename, int filter);
#define IMAGE_PCX   1
#define IMAGE_TGA   2
#define IMAGE_PNG   4
#define IMAGE_JPG   8

typedef int image_t;

int Image_LoadTexture(const char *identifier, int width, int height, const void *pixels, hyena_texture_format_t format, hyena_texture_filter_t filter, int mip_levels, qboolean alpha, qboolean keep, qboolean stretch_to_power_of_two);

#endif
