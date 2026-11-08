#!/usr/bin/env python3
# Copyright (C) 2026 NZ:P Team
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

"""Embed a 128x128 RGBA PNG as an XPR title image in an nxdk XBE.

Uses only the Python standard library. The icon section is not preloaded,
so its texture does not occupy game RAM. XBE/XPR layouts are documented at
https://xboxdevwiki.net/Xbe and https://xboxdevwiki.net/XPR.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import zlib


def read_png(path):
    data = path.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Icon must be a PNG')
    compressed = bytearray()
    cursor = 8
    header = False
    ended = False
    while cursor + 12 <= len(data):
        size = struct.unpack_from('>I', data, cursor)[0]
        kind = data[cursor + 4:cursor + 8]
        chunk = data[cursor + 8:cursor + 8 + size]
        if cursor + size + 12 > len(data):
            raise ValueError('Truncated PNG chunk')
        crc = struct.unpack_from('>I', data, cursor + size + 8)[0]
        if zlib.crc32(kind + chunk) != crc:
            raise ValueError('Invalid PNG chunk CRC')
        if kind == b'IHDR':
            if header or chunk != struct.pack('>IIBBBBB', 128, 128, 8, 6, 0, 0, 0):
                raise ValueError('Icon must be 128x128, 8-bit RGBA, non-interlaced')
            header = True
        elif kind == b'IDAT':
            if not header:
                raise ValueError('PNG is missing IHDR')
            compressed.extend(chunk)
        elif kind == b'IEND':
            ended = True
            break
        elif not kind[0] & 32:
            raise ValueError('Unsupported critical PNG chunk')
        cursor += size + 12
    if not header or not ended:
        raise ValueError('Incomplete PNG')
    raw = zlib.decompress(compressed)
    stride = 128 * 4
    if len(raw) != 128 * (stride + 1):
        raise ValueError('Invalid PNG pixel data length')
    pixels = bytearray()
    previous = bytearray(stride)
    for y in range(128):
        start = y * (stride + 1)
        method = raw[start]
        row = bytearray(raw[start + 1:start + 1 + stride])
        if method > 4:
            raise ValueError('Invalid PNG row filter')
        for x in range(stride):
            left = row[x - 4] if x >= 4 else 0
            above = previous[x]
            corner = previous[x - 4] if x >= 4 else 0
            if method == 1:
                predictor = left
            elif method == 2:
                predictor = above
            elif method == 3:
                predictor = (left + above) // 2
            elif method == 4:
                estimate = left + above - corner
                distances = (abs(estimate - left), abs(estimate - above), abs(estimate - corner))
                predictor = (left, above, corner)[distances.index(min(distances))]
            else:
                predictor = 0
            row[x] = (row[x] + predictor) & 255
        pixels.extend(row)
        previous = row
    return pixels


def make_xpr(pixels):
    # A8R8G8B8 is BGRA in memory; interleave X/Y bits for NV2A swizzling.
    texture = bytearray(len(pixels))
    for y in range(128):
        for x in range(128):
            offset = sum(((x >> bit) & 1) << (bit * 2) |
                         ((y >> bit) & 1) << (bit * 2 + 1) for bit in range(7)) * 4
            r, g, b, a = pixels[(y * 128 + x) * 4:(y * 128 + x + 1) * 4]
            texture[offset:offset + 4] = bytes((b, g, r, a))
    header = bytearray(b'\xad' * 2048)
    struct.pack_into('<4s8I', header, 0, b'XPR0', len(header) + len(texture), len(header),
                     0x00040001, 0, 0, 0x07710629, 0, 0xffffffff)
    return header + texture


def embed_icon(xbe, icon):
    data = bytearray(xbe.read_bytes())
    if len(data) < 0x178 or data[:4] != b'XBEH':
        raise ValueError('Invalid XBE')
    base, headers, image_size = struct.unpack_from('<III', data, 0x104)
    count, table_address = struct.unpack_from('<II', data, 0x11c)
    table = table_address - base
    if not count or table < 0x178 or table + count * 56 > headers or headers > len(data):
        raise ValueError('Invalid XBE section table')
    sections = bytes(data[table:table + count * 56])
    first_raw = len(data)
    virtual_end = base + image_size
    for i in range(count):
        flags, address, size, raw, raw_size, name = struct.unpack_from('<6I', sections, i * 56)
        if raw_size:
            first_raw = min(first_raw, raw)
        if not base <= name < base + headers:
            raise ValueError('Invalid XBE section name')
        if data[name - base:name - base + 10] == b'$$XTIMAGE\0':
            raise ValueError('XBE already contains a title image; rebuild it with cxbe first')
        virtual_end = max(virtual_end, address + size)
    # Relocate only the table into unused header padding; existing pointers and
    # all executable sections remain unchanged. Refuse to move loaded sections.
    table = (headers + 3) & ~3
    references = table + (count + 1) * 56
    name = references + 4
    end = name + 10
    if end > first_raw or any(data[headers:end]):
        raise ValueError('Not enough unused XBE header space for the title image')
    raw = (len(data) + 4095) & ~4095
    address = (virtual_end + 4095) & ~4095
    digest = hashlib.sha1(struct.pack('<I', len(icon)) + icon).digest()
    # Inserted-file and read-only boundary flags; no preload flag.
    section = struct.pack('<9I20s', 0x38, address, len(icon), raw, len(icon),
                          base + name, 0, base + references, base + references + 2, digest)
    data[table:references] = sections + section
    data[references:end] = b'\0' * 4 + b'$$XTIMAGE\0'
    struct.pack_into('<II', data, 0x108, end, address - base + ((len(icon) + 4095) & ~4095))
    struct.pack_into('<II', data, 0x11c, count + 1, base + table)
    data.extend(bytes(raw - len(data)))
    data.extend(icon)
    temporary = xbe.with_name(xbe.name + '.tmp')
    temporary.write_bytes(data)
    temporary.replace(xbe)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('icon', type=Path)
    parser.add_argument('xbe', type=Path)
    args = parser.parse_args()
    try:
        embed_icon(args.xbe, make_xpr(read_png(args.icon)))
    except (OSError, ValueError, struct.error, zlib.error) as error:
        parser.exit(1, f'Icon packaging failed: {error}\n')
    print('Embedded 128x128 ARGB8888 title image with alpha.')


if __name__ == '__main__':
    main()
