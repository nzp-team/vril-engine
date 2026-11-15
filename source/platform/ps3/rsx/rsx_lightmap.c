/*
 * Copyright (C) 1996-1997 Id Software, Inc.
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
 *
 */

#include "../../../nzportable_def.h"
#if defined(__ALTIVEC__)
#include <altivec.h>
#endif

qboolean
Hyena_AccumulateLightmap(unsigned * dest, const byte * source, int count, unsigned scale,
  qboolean add)
{
    int i = 0;

    if (count < 0 || scale > 65535)
        return false;

    #if defined(__ALTIVEC__)

    for (; i < count && ((uintptr_t) (dest + i) & 15); ++i) {
        unsigned value = source[i] * scale;
        dest[i] = add ? dest[i] + value : value;
    }

    __vector unsigned char zero    = vec_splats((unsigned char) 0);
    __vector unsigned short factor = vec_splats((unsigned short) scale);

    for (; i + 16 <= count; i += 16) {
        const byte * p = source + i;
        __vector unsigned char bytes   = vec_perm(vec_ld(0, p), vec_ld(15, p), vec_lvsl(0, p));
        __vector unsigned short first  = (__vector unsigned short) vec_mergeh(zero, bytes);
        __vector unsigned short second = (__vector unsigned short) vec_mergel(zero, bytes);
        __vector unsigned int even = vec_mule(first, factor), odd = vec_mulo(first, factor);
        __vector unsigned int a = vec_mergeh(even, odd), b = vec_mergel(even, odd);
        even = vec_mule(second, factor);
        odd  = vec_mulo(second, factor);
        __vector unsigned int c = vec_mergeh(even, odd), d = vec_mergel(even, odd);

        if (add) {
            a = vec_add(a, vec_ld(0, dest + i));
            b = vec_add(b, vec_ld(16, dest + i));
            c = vec_add(c, vec_ld(32, dest + i));
            d = vec_add(d, vec_ld(48, dest + i));
        }

        vec_st(a, 0, dest + i);
        vec_st(b, 16, dest + i);
        vec_st(c, 32, dest + i);
        vec_st(d, 48, dest + i);
    }

    #endif

    for (; i < count; ++i) {
        unsigned value = source[i] * scale;
        dest[i] = add ? dest[i] + value : value;
    }

    return true;
}
