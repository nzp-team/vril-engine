/*
Copyright (C) 1996-1997 Id Software, Inc.

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

/* *INDENT-OFF* */

#ifndef QUAKEASM_H
#define QUAKEASM_H

//
// quakeasm.h: general asm header file
//

// !!! must be kept the same as in d_iface.h !!!
#define TRANSPARENT_COLOR 255

.extern C(snd_scaletable)
.extern C(paintbuffer)
.extern C(snd_linear_count)
.extern C(snd_p)
.extern C(snd_vol)
.extern C(snd_out)
.extern C(vright)
.extern C(vup)
.extern C(vpn)
.extern C(BOPS_Error)
