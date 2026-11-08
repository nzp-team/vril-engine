/*
 * Copyright (C) 2026 NZ:P Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stddef.h>
#include <stdint.h>

typedef struct __attribute__((packed, may_alias)){
    uint32_t value;
} xbox_memory_word_t;

static void *
Sys_XboxCopyMemory(void * destination, const void * source, size_t size)
{
    unsigned char * out      = destination;
    const unsigned char * in = source;

    size_t words = size / 4;

    // These routines are also used by interrupt-level SDK code. Integer string
    // operations avoid touching the interrupted thread's floating-point state.
    __asm__ __volatile__ ("rep movsl" : "+D" (out), "+S" (in), "+c" (words) : : "memory");
    size &= 3;
    while (size--) *out++ = *in++;
    return destination;
}

void *
memcpy(void * restrict destination, const void * restrict source, size_t size)
{
    return Sys_XboxCopyMemory(destination, source, size);
}

void *
memmove(void * destination, const void * source, size_t size)
{
    unsigned char * out      = destination;
    const unsigned char * in = source;

    if (out == in || !size) return destination;

    if ((uintptr_t) out < (uintptr_t) in || (uintptr_t) out - (uintptr_t) in >= size)
        return Sys_XboxCopyMemory(destination, source, size);

    out += size;
    in  += size;
    // Load each complete block before writing it, including overlapping ranges.
    while (size >= 4) {
        out -= 4;
        in  -= 4;
        ((xbox_memory_word_t *) out)->value = ((const xbox_memory_word_t *) in)->value;
        size -= 4;
    }
    while (size--) *--out = *--in;
    return destination;
}

void *
memset(void * destination, int value, size_t size)
{
    unsigned char * out = destination;
    uint32_t word       = (unsigned char) value * 0x01010101u;

    while (size && ((uintptr_t) out & 3)) {
        *out++ = value;
        --size;
    }
    size_t words = size / 4;
    __asm__ __volatile__ ("rep stosl" : "+D" (out), "+c" (words) : "a" (word) : "memory");
    size &= 3;
    while (size--) *out++ = value;
    return destination;
}
