/*
 * Copyright (C) 2023 Shpuld and NZP Team.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
// r_color_quantization.h -- Entity fragment header
#ifndef _COLOR_QUANTIZER_H_
#define _COLOR_QUANTIZER_H_

void
convert_8bpp_to_4bpp(const byte * indata, const byte * inpal, int inpal_bpp, int width, int height, byte * outdata, byte * outpal);

void
convert_8bpp_to_4bpp_with_hint(const byte * indata, const byte * inpal, int inpal_bpp, int width, int height, byte * outdata, const byte * palhint);

#endif // _COLOR_QUANTIZER_H_
