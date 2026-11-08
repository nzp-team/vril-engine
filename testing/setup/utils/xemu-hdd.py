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

"""Read test output from a standalone xemu QCOW2 HDD."""
import struct
import zlib


class Qcow:
    def __init__(self, path):
        self.file = open(path, "rb")
        header = self.file.read(72)
        if header[:4] != b"QFI\xfb" or struct.unpack_from(">Q", header, 8)[0]:
            raise ValueError("Expected a standalone QCOW2 image")
        self.cluster = 1 << struct.unpack_from(">I", header, 20)[0]
        count = struct.unpack_from(">I", header, 36)[0]
        self.file.seek(struct.unpack_from(">Q", header, 40)[0])
        self.l1 = struct.unpack(">" + "Q" * count, self.file.read(count * 8))

    def read(self, offset, size):
        result = bytearray()
        mask = ((1 << 56) - 1) & ~511
        while size:
            index, within = divmod(offset, self.cluster)
            first, second = divmod(index, self.cluster // 8)
            count = min(size, self.cluster - within)
            table = self.l1[first] & mask
            entry = 0
            if table:
                self.file.seek(table + second * 8)
                entry = struct.unpack(">Q", self.file.read(8))[0]
            physical = entry & mask
            if entry & (1 << 62):
                bits = self.cluster.bit_length() - 1
                shift = 62 - (bits - 8)
                sectors = ((entry >> shift) & ((1 << (bits - 8)) - 1)) + 1
                physical = entry & ((1 << shift) - 1)
                self.file.seek(physical)
                compressed = self.file.read(sectors * 512 - (physical & 511))
                block = zlib.decompress(compressed, -15)
                if len(block) != self.cluster:
                    raise ValueError("Invalid compressed QCOW2 cluster")
                result.extend(block[within:within + count])
            elif physical and not entry & 1:
                self.file.seek(physical + within)
                data = self.file.read(count)
                if len(data) != count:
                    raise ValueError("Truncated QCOW2 cluster")
                result.extend(data)
            else:
                result.extend(bytes(count))
            offset += count
            size -= count
        return bytes(result)


class Fatx:
    def __init__(self, disk):
        self.disk = disk
        self.offset = 0xABE80000
        header = disk.read(self.offset, 16)
        if header[:4] != b"FATX":
            raise ValueError("Missing Xbox E: FATX filesystem")
        self.cluster = struct.unpack_from("<I", header, 8)[0] * 512
        self.root = struct.unpack_from("<I", header, 12)[0]
        clusters = 0x131F00000 // self.cluster
        self.width = 4 if clusters >= 65525 else 2
        fat_size = ((clusters + 1) * self.width + 4095) & ~4095
        self.fat = disk.read(self.offset + 4096, fat_size)
        self.data = self.offset + 4096 + fat_size

    def chain(self, cluster):
        seen = set()
        limit = 0xFFFFFFF8 if self.width == 4 else 0xFFF8
        while 0 < cluster < limit:
            if cluster in seen or cluster * self.width + self.width > len(self.fat):
                raise ValueError("Invalid FATX cluster chain")
            seen.add(cluster)
            yield self.disk.read(self.data + (cluster - 1) * self.cluster, self.cluster)
            cluster = int.from_bytes(self.fat[cluster * self.width:(cluster + 1) * self.width], "little")

    def entries(self, cluster):
        for block in self.chain(cluster):
            for offset in range(0, len(block), 64):
                entry = block[offset:offset + 64]
                length = entry[0]
                if length in (0, 255):
                    return
                if length == 229:
                    continue
                if length > 42:
                    raise ValueError("Invalid FATX directory entry")
                name = entry[2:2 + length].decode("ascii")
                first, size = struct.unpack_from("<II", entry, 44)
                yield name, first, size

    def read(self, path):
        cluster = self.root
        size = 0
        for part in path.replace("\\", "/").strip("/").split("/"):
            _, cluster, size = next(entry for entry in self.entries(cluster)
                                    if entry[0].lower() == part.lower())
        result = bytearray()
        for block in self.chain(cluster):
            result.extend(block[:size - len(result)])
            if len(result) >= size:
                break
        if len(result) != size:
            raise ValueError("Truncated FATX file")
        return bytes(result)
