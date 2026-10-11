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

// Shared BC1/BC2/BC3 encoder. Output uses standard block ordering.
// Width and height must be multiples of four; source pixels are RGBA8.

#ifndef R_DXT_H
#define R_DXT_H
#ifdef __cplusplus
extern "C" {
#endif
enum { HYE_DXT1 = 1, HYE_DXT3 = 3, HYE_DXT5 = 5 };
int
Hyena_CompressDXT(
    int srccomps, int width, int height, const unsigned char * source, unsigned int format, unsigned char * dest);
#ifdef __cplusplus
}
#endif
#endif // ifndef R_DXT_H
