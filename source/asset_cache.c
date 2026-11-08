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

#include "nzportable_def.h"
#include "asset_cache.h"
#include <limits.h>

#define CACHE_HEADER_SIZE 180
#define CACHE_NAME_SIZE   128
#define CACHE_ENTRY_LIMIT (16u * 1024u * 1024u)
#define CACHE_VERSION     1

cvar_t asset_cache       = { "asset_cache", "1", CVAR_ARCHIVE };
cvar_t asset_cache_maxmb = { "asset_cache_maxmb", "512", CVAR_ARCHIVE };
static qboolean initialized;
static asset_cache_build_t build;
static unsigned hits, misses, invalid, writes, failures;
static uint32_t crc_table[256];
static qboolean crc_initialized;

static uint32_t
Cache_UpdateCRC(uint32_t crc, const void * data, size_t size)
{
    const byte * bytes = data;
    unsigned i, bit;

    if (!crc_initialized) {
        for (i = 0; i < 256; ++i) {
            uint32_t value = i;
            for (bit = 0; bit < 8; ++bit)
                value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0);
            crc_table[i] = value;
        }
        crc_initialized = true;
    }
    while (size--)
        crc = crc_table[(crc ^ *bytes++) & 255] ^ (crc >> 8);
    return crc;
}

uint32_t
AssetCache_CRC32(const void * data, size_t size)
{
    return Cache_UpdateCRC(0xffffffffu, data, size) ^ 0xffffffffu;
}

static void
Cache_PutWord(byte * bytes, uint32_t value)
{
    int i;

    for (i = 0; i < 4; ++i)
        bytes[i] = (byte) (value >> (i * 8));
}

static uint32_t
Cache_GetWord(const byte * bytes)
{
    return (uint32_t) bytes[0] | (uint32_t) bytes[1] << 8
           | (uint32_t) bytes[2] << 16 | (uint32_t) bytes[3] << 24;
}

uint32_t
AssetCache_HashWords(const uint32_t * words, size_t count)
{
    uint32_t crc = 0xffffffffu;
    byte encoded[4];

    while (count--) {
        Cache_PutWord(encoded, *words++);
        crc = Cache_UpdateCRC(crc, encoded, sizeof(encoded));
    }
    return crc ^ 0xffffffffu;
}

qboolean
AssetCache_FileCRC(FILE * file, size_t size, uint32_t * result)
{
    byte buffer[4096];
    uint32_t crc      = 0xffffffffu;
    long start        = ftell(file);
    qboolean complete = true;

    if (start < 0)
        return false;

    while (size) {
        size_t count = size < sizeof(buffer) ? size : sizeof(buffer);
        if (fread(buffer, 1, count, file) != count) {
            complete = false;
            break;
        }
        crc   = Cache_UpdateCRC(crc, buffer, count);
        size -= count;
    }
    if (fseek(file, start, SEEK_SET) != 0)
        return false;

    *result = crc ^ 0xffffffffu;
    return complete;
}

qboolean
AssetCache_Enabled(void)
{
    return initialized && asset_cache.value != 0 && com_gamedir[0];
}

static qboolean
Cache_Directory(char * directory, size_t size)
{
    int count = snprintf(directory, size, "%s/_cache/%s", com_gamedir, STRINGIFY_MACRO(PLATFORM_DIRECTORY));

    return count > 0 && (size_t) count < size;
}

static qboolean
Cache_Path(asset_cache_kind_t kind, const char * name, uint32_t variant, char * path, size_t size)
{
    char directory[MAX_OSPATH * 2];
    int count;

    if (!name || !name[0] || strlen(name) >= CACHE_NAME_SIZE ||
      kind < ASSET_CACHE_IMAGE || kind > ASSET_CACHE_MAP || !Cache_Directory(directory, sizeof(directory)))
        return false;

    count = snprintf(path, size, "%s/%i-%08x-%08x.bin", directory, kind,
        (unsigned) AssetCache_CRC32(name, strlen(name)), (unsigned) variant);
    return count > 0 && (size_t) count < size;
}

static qboolean
Cache_OwnedName(const char * name)
{
    int i;

    if (strlen(name) != 23 || name[0] < '1' || name[0] > '5' || name[1] != '-' ||
      name[10] != '-' || strcmp(name + 19, ".bin"))
        return false;

    for (i = 2; i < 19; ++i) {
        if (i == 10)
            continue;
        if (!((name[i] >= '0' && name[i] <= '9') || (name[i] >= 'a' && name[i] <= 'f')))
            return false;
    }
    return true;
}

#define CACHE_PACK_HEADER         128u
#define CACHE_SLOT_SIZE           64u
#define CACHE_INDEX_HEADER        32u
#define CACHE_FOOTER_SIZE         32u
#define CACHE_INDEX_ENTRY         (CACHE_HEADER_SIZE + 8u)
#define CACHE_INDEX_LIMIT         16384u
#define CACHE_CHECKPOINT_INTERVAL 32u

typedef struct {
    byte     header[CACHE_HEADER_SIZE];
    uint32_t offset, sequence;
} cache_index_t;

static struct {
    FILE *          file;
    char            directory[MAX_OSPATH * 2], path[MAX_OSPATH * 2];
    cache_index_t * entries;
    int *           slots;
    unsigned        count, capacity, slot_count, pending;
    uint32_t        tail, sequence, generation;
    size_t          physical, live, legacy;
    qboolean        writable, dirty;
} archive;
static size_t migration_credit;
static sys_directory_t * legacy_import;
static qboolean
Cache_Checkpoint(void);
static qboolean
Cache_CompactArchive(void);
static qboolean
Cache_ReadMemory(void * context, void * data, size_t size);

static qboolean
Cache_ArchivePath(char * path, size_t size, const char * name)
{
    return COM_BuildFilePath(path, size, archive.directory, name);
}

static size_t
Cache_Limit(void)
{
    float maximum = asset_cache_maxmb.value;

    if (!(maximum >= 0 && maximum <= 1024)) maximum = maximum > 1024 ? 1024 : 0;
    return (size_t) maximum * 1024u * 1024u;
}

static qboolean
Cache_ValidHeader(const byte * header)
{
    return !memcmp(header, "VRILAC01", 8) && Cache_GetWord(header + 8) == CACHE_VERSION &&
           Cache_GetWord(header + 12) >= ASSET_CACHE_IMAGE && Cache_GetWord(header + 12) <= ASSET_CACHE_MAP &&
           Cache_GetWord(header + 24) && Cache_GetWord(header + 24) <= CACHE_ENTRY_LIMIT &&
           Cache_GetWord(header + 176) == AssetCache_CRC32(header, 176) && header[48] &&
           memchr(header + 48, 0, CACHE_NAME_SIZE);
}

static void
Cache_MakeHeader(byte * header, asset_cache_kind_t kind, const char * name,
  uint32_t source_crc, uint32_t variant, const asset_cache_data_t * entry)
{
    int i;

    memset(header, 0, CACHE_HEADER_SIZE);
    memcpy(header, "VRILAC01", 8);
    Cache_PutWord(header + 8, CACHE_VERSION);
    Cache_PutWord(header + 12, kind);
    Cache_PutWord(header + 16, source_crc);
    Cache_PutWord(header + 20, variant);
    Cache_PutWord(header + 24, entry->size);
    for (i = 0; i < 4; ++i) Cache_PutWord(header + 32 + i * 4, entry->metadata[i]);
    Q_strlcpy((char *) header + 48, name, CACHE_NAME_SIZE);
}

static uint32_t
Cache_Key(const byte * header)
{
    uint32_t words[] = { Cache_GetWord(header + 12),   Cache_GetWord(header + 20),
                         AssetCache_CRC32(header + 48, strlen((const char *) header + 48)) };

    return AssetCache_HashWords(words, 3);
}

static qboolean
Cache_SameKey(const byte * a, const byte * b)
{
    return Cache_GetWord(a + 12) == Cache_GetWord(b + 12) &&
           Cache_GetWord(a + 20) == Cache_GetWord(b + 20) && !strcmp((const char *) a + 48, (const char *) b + 48);
}

static int
Cache_Find(const byte * header)
{
    unsigned slot;

    if (!archive.slot_count) return -1;

    slot = Cache_Key(header) & (archive.slot_count - 1);
    while (archive.slots[slot]) {
        int i = archive.slots[slot] - 1;
        if (Cache_SameKey(header, archive.entries[i].header)) return i;

        slot = (slot + 1) & (archive.slot_count - 1);
    }
    return -1;
}

static void
Cache_Rehash(void)
{
    unsigned i;

    memset(archive.slots, 0, archive.slot_count * sizeof(*archive.slots));
    for (i = 0; i < archive.count; ++i) {
        unsigned slot = Cache_Key(archive.entries[i].header) & (archive.slot_count - 1);
        while (archive.slots[slot]) slot = (slot + 1) & (archive.slot_count - 1);
        archive.slots[slot] = i + 1;
    }
}

static qboolean
Cache_Reserve(unsigned count)
{
    cache_index_t * entries;
    int * slots;
    unsigned capacity = archive.capacity ? archive.capacity : 64;

    if (count > CACHE_INDEX_LIMIT) return false;

    if (count <= archive.capacity) return true;

    while (capacity < count) capacity *= 2;
    entries = malloc(capacity * sizeof(*entries));
    slots   = calloc(capacity * 2, sizeof(*slots));
    if (!entries || !slots) { free(entries); free(slots); return false; }
    if (archive.count) memcpy(entries, archive.entries, archive.count * sizeof(*entries));
    free(archive.entries);
    free(archive.slots);
    archive.entries    = entries;
    archive.slots      = slots;
    archive.capacity   = capacity;
    archive.slot_count = capacity * 2;
    Cache_Rehash();
    return true;
}

static size_t
Cache_RecordBytes(const byte * header)
{
    return CACHE_HEADER_SIZE + Cache_GetWord(header + 24) + CACHE_FOOTER_SIZE;
}

static qboolean
Cache_Index(const byte * header, uint32_t offset, uint32_t sequence)
{
    int i = Cache_Find(header);

    if (i < 0) {
        unsigned slot;
        if (!Cache_Reserve(archive.count + 1)) return false;

        i    = archive.count++;
        slot = Cache_Key(header) & (archive.slot_count - 1);
        while (archive.slots[slot]) slot = (slot + 1) & (archive.slot_count - 1);
        archive.slots[slot] = i + 1;
    } else archive.live -= Cache_RecordBytes(archive.entries[i].header);
    memcpy(archive.entries[i].header, header, CACHE_HEADER_SIZE);
    archive.entries[i].offset   = offset;
    archive.entries[i].sequence = sequence;
    archive.live += Cache_RecordBytes(header);
    return true;
}

static void
Cache_Footer(byte * footer, uint32_t offset, uint32_t bytes,
  uint32_t sequence, uint32_t header_crc, uint32_t type)
{
    memset(footer, 0, CACHE_FOOTER_SIZE);
    memcpy(footer, "VRILEND1", 8);
    Cache_PutWord(footer + 8, bytes);
    Cache_PutWord(footer + 12, sequence);
    Cache_PutWord(footer + 16, header_crc);
    Cache_PutWord(footer + 20, offset);
    Cache_PutWord(footer + 24, type);
    Cache_PutWord(footer + 28, AssetCache_CRC32(footer, 28));
}

static qboolean
Cache_CheckFooter(const byte * footer, uint32_t offset, uint32_t bytes,
  uint32_t sequence, uint32_t header_crc, uint32_t type)
{
    byte expected[CACHE_FOOTER_SIZE];

    Cache_Footer(expected, offset, bytes, sequence, header_crc, type);
    return !memcmp(expected, footer, sizeof(expected));
}

static void
Cache_Superblock(byte * slot, uint32_t generation, uint32_t index,
  uint32_t bytes, uint32_t end, uint32_t sequence)
{
    memset(slot, 0, CACHE_SLOT_SIZE);
    memcpy(slot, "VRILPK01", 8);
    Cache_PutWord(slot + 8, 1);
    Cache_PutWord(slot + 12, generation);
    Cache_PutWord(slot + 16, index);
    Cache_PutWord(slot + 20, bytes);
    Cache_PutWord(slot + 24, end);
    Cache_PutWord(slot + 28, sequence);
    Cache_PutWord(slot + 60, AssetCache_CRC32(slot, 60));
}

static qboolean
Cache_ValidSlot(const byte * slot, size_t size)
{
    uint32_t end = Cache_GetWord(slot + 24), offset = Cache_GetWord(slot + 16), bytes = Cache_GetWord(slot + 20);

    return !memcmp(slot, "VRILPK01", 8) && Cache_GetWord(slot + 8) == 1 &&
           Cache_GetWord(slot + 60) == AssetCache_CRC32(slot, 60) && end >= CACHE_PACK_HEADER && end <= size &&
           ((!offset && !bytes && end == CACHE_PACK_HEADER) ||
           (offset >= CACHE_PACK_HEADER && bytes >= CACHE_INDEX_HEADER + CACHE_FOOTER_SIZE &&
           offset <= end && bytes == end - offset));
}

static qboolean
Cache_WriteSnapshot(FILE * file, const cache_index_t * entries, unsigned count,
  uint32_t sequence, uint32_t * tail, uint32_t * generation)
{
    byte header[CACHE_INDEX_HEADER] = { 0 }, row[CACHE_INDEX_ENTRY], footer[CACHE_FOOTER_SIZE], slot[CACHE_SLOT_SIZE];
    uint32_t start = *tail, bytes = CACHE_INDEX_HEADER + count * CACHE_INDEX_ENTRY + CACHE_FOOTER_SIZE;
    uint32_t crc = 0xffffffffu;
    unsigned i;

    if (start > INT_MAX - bytes || fseek(file, start, SEEK_SET) || fwrite(header, 1, sizeof(header),
      file) != sizeof(header)) return false;

    for (i = 0; i < count; ++i) {
        memcpy(row, entries[i].header, CACHE_HEADER_SIZE);
        Cache_PutWord(row + CACHE_HEADER_SIZE, entries[i].offset);
        Cache_PutWord(row + CACHE_HEADER_SIZE + 4, entries[i].sequence);
        if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) return false;

        crc = Cache_UpdateCRC(crc, row, sizeof(row));
    }
    memcpy(header, "VRILIX01", 8);
    Cache_PutWord(header + 8, 1);
    Cache_PutWord(header + 12, count);
    Cache_PutWord(header + 16, sequence);
    Cache_PutWord(header + 20, count * CACHE_INDEX_ENTRY);
    Cache_PutWord(header + 24, crc ^ 0xffffffffu);
    Cache_PutWord(header + 28, AssetCache_CRC32(header, 28));
    Cache_Footer(footer, start, bytes, sequence, Cache_GetWord(header + 28), 2);
    if (fseek(file, start, SEEK_SET) || fwrite(header, 1, sizeof(header), file) != sizeof(header) ||
      fseek(file, start + bytes - sizeof(footer), SEEK_SET) || fwrite(footer, 1, sizeof(footer),
      file) != sizeof(footer) || fflush(file)) return false;

    *tail = start + bytes;
    Cache_Superblock(slot, *generation + 1, start, bytes, *tail, sequence);
    if (fseek(file, ((*generation + 1) & 1) * CACHE_SLOT_SIZE, SEEK_SET) ||
      fwrite(slot, 1, sizeof(slot), file) != sizeof(slot) || fflush(file)) return false;

    ++*generation;
    return true;
} /* Cache_WriteSnapshot */

static qboolean
Cache_Checkpoint(void)
{
    qboolean saved;

    if (!archive.file || !archive.dirty || !archive.writable || !asset_cache.value) return true;

    clearerr(archive.file);
    saved = Cache_WriteSnapshot(archive.file, archive.entries, archive.count, archive.sequence, &archive.tail,
        &archive.generation);
    archive.physical = max(archive.physical, archive.tail);
    if (saved) { archive.dirty = false; archive.pending = 0; }
    return saved;
}

static void
Cache_Close(qboolean save)
{
    if (save) Cache_Checkpoint();
    if (archive.file) fclose(archive.file);
    free(archive.entries);
    free(archive.slots);
    memset(&archive, 0, sizeof(archive));
}

void
AssetCache_Shutdown(void)
{
    AssetCache_CancelBuild();
    Cache_Close(true);
}

static qboolean
Cache_ReadIndex(const byte * slot)
{
    byte header[CACHE_INDEX_HEADER], row[CACHE_INDEX_ENTRY], footer[CACHE_FOOTER_SIZE];
    uint32_t start = Cache_GetWord(slot + 16), bytes = Cache_GetWord(slot + 20), crc = 0xffffffffu;
    unsigned i, count;

    archive.count = 0;
    archive.live  = 0;
    if (archive.slots) memset(archive.slots, 0, archive.slot_count * sizeof(*archive.slots));
    if (!start) return true;

    if (fseek(archive.file, start, SEEK_SET) || fread(header, 1, sizeof(header), archive.file) != sizeof(header) ||
      memcmp(header, "VRILIX01", 8) || Cache_GetWord(header + 8) != 1 ||
      Cache_GetWord(header + 28) != AssetCache_CRC32(header, 28)) return false;

    count = Cache_GetWord(header + 12);
    if (count > CACHE_INDEX_LIMIT || Cache_GetWord(header + 20) != count * CACHE_INDEX_ENTRY ||
      bytes != CACHE_INDEX_HEADER + count * CACHE_INDEX_ENTRY + CACHE_FOOTER_SIZE ||
      Cache_GetWord(header + 16) != Cache_GetWord(slot + 28) || !Cache_Reserve(count)) return false;

    for (i = 0; i < count; ++i) {
        uint32_t offset, sequence;
        if (fread(row, 1, sizeof(row), archive.file) != sizeof(row) || !Cache_ValidHeader(row)) return false;

        offset   = Cache_GetWord(row + CACHE_HEADER_SIZE);
        sequence = Cache_GetWord(row + CACHE_HEADER_SIZE + 4);
        if (offset < CACHE_PACK_HEADER || offset >= start || Cache_RecordBytes(row) > start - offset ||
          Cache_Find(row) >= 0) return false;

        crc = Cache_UpdateCRC(crc, row, sizeof(row));
        if (!Cache_Index(row, offset, sequence) || archive.live > start - CACHE_PACK_HEADER) return false;
    }
    return (crc ^ 0xffffffffu) == Cache_GetWord(header + 24) &&
           fread(footer, 1, sizeof(footer), archive.file) == sizeof(footer) &&
           Cache_CheckFooter(footer, start, bytes, Cache_GetWord(slot + 28), Cache_GetWord(header + 28), 2);
} /* Cache_ReadIndex */

static qboolean
Cache_ScanTail(void)
{
    byte header[CACHE_HEADER_SIZE], footer[CACHE_FOOTER_SIZE];
    uint32_t cursor = archive.tail;

    while (cursor <= archive.physical && archive.physical - cursor >= CACHE_INDEX_HEADER) {
        uint32_t bytes, sequence, header_crc, type;
        if (fseek(archive.file, cursor, SEEK_SET) || fread(header, 1, CACHE_INDEX_HEADER,
          archive.file) != CACHE_INDEX_HEADER) break;
        if (!memcmp(header, "VRILAC01", 8)) {
            if (archive.physical - cursor < CACHE_HEADER_SIZE ||
              fread(header + CACHE_INDEX_HEADER, 1, CACHE_HEADER_SIZE - CACHE_INDEX_HEADER,
              archive.file) != CACHE_HEADER_SIZE - CACHE_INDEX_HEADER ||
              !Cache_ValidHeader(header)) break;
            bytes      = Cache_RecordBytes(header);
            sequence   = archive.sequence + 1;
            header_crc = Cache_GetWord(header + 176);
            type       = 1;
        } else if (!memcmp(header, "VRILIX01", 8)) {
            unsigned count = Cache_GetWord(header + 12);
            if (Cache_GetWord(header + 8) != 1 || count > CACHE_INDEX_LIMIT ||
              Cache_GetWord(header + 20) != count * CACHE_INDEX_ENTRY ||
              Cache_GetWord(header + 28) != AssetCache_CRC32(header,
              28) || Cache_GetWord(header + 16) != archive.sequence) break;
            bytes      = CACHE_INDEX_HEADER + count * CACHE_INDEX_ENTRY + CACHE_FOOTER_SIZE;
            sequence   = archive.sequence;
            header_crc = Cache_GetWord(header + 28);
            type       = 2;
        } else break;
        if (bytes > archive.physical - cursor || fseek(archive.file, cursor + bytes - sizeof(footer), SEEK_SET) ||
          fread(footer, 1, sizeof(footer), archive.file) != sizeof(footer) ||
          !Cache_CheckFooter(footer, cursor, bytes, sequence, header_crc, type)) break;
        if (type == 1) {
            if (!Cache_Index(header, cursor, sequence)) return false;

            archive.sequence = sequence;
        }
        cursor       += bytes;
        archive.dirty = true;
    }
    archive.tail = cursor;
    clearerr(archive.file);
    return true;
} /* Cache_ScanTail */

static qboolean
Cache_ReadPack(void)
{
    byte slots[CACHE_PACK_HEADER];
    int first = 0, second = 1;
    qboolean a, b, loaded = false;
    long size;

    if (fseek(archive.file, 0, SEEK_END) || (size = ftell(archive.file)) < (long) CACHE_PACK_HEADER || size > INT_MAX ||
      fseek(archive.file, 0, SEEK_SET) || fread(slots, 1, sizeof(slots), archive.file) != sizeof(slots)) return false;

    archive.physical = size;
    a = Cache_ValidSlot(slots, size);
    b = Cache_ValidSlot(slots + CACHE_SLOT_SIZE, size);
    if (b && (!a || (int32_t) (Cache_GetWord(slots + CACHE_SLOT_SIZE + 12) - Cache_GetWord(slots + 12)) > 0)) {
        first  = 1;
        second = 0;
    }
    if ((first ? b : a) && Cache_ReadIndex(slots + first * CACHE_SLOT_SIZE)) {
        const byte * slot = slots + first * CACHE_SLOT_SIZE;
        archive.tail       = Cache_GetWord(slot + 24);
        archive.sequence   = Cache_GetWord(slot + 28);
        archive.generation = Cache_GetWord(slot + 12);
        loaded = true;
    } else if ((second ? b : a) && Cache_ReadIndex(slots + second * CACHE_SLOT_SIZE)) {
        const byte * slot = slots + second * CACHE_SLOT_SIZE;
        archive.tail       = Cache_GetWord(slot + 24);
        archive.sequence   = Cache_GetWord(slot + 28);
        archive.generation = Cache_GetWord(slot + 12);
        loaded = true;
    }
    if (!loaded) {
        if (memcmp(slots, "VRILPK01", 8) && memcmp(slots + CACHE_SLOT_SIZE, "VRILPK01", 8)) return false;

        archive.count    = 0;
        archive.live     = 0;
        archive.sequence = archive.generation = 0;
        archive.tail     = CACHE_PACK_HEADER;
        if (archive.slots) memset(archive.slots, 0, archive.slot_count * sizeof(*archive.slots));
        archive.dirty = true;
    }
    return Cache_ScanTail();
} /* Cache_ReadPack */

static size_t
Cache_FileSize(const char * path)
{
    FILE * file = fopen(path, "rb");
    long size   = -1;

    if (file) { if (!fseek(file, 0, SEEK_END)) size = ftell(file); fclose(file); }
    return size < 0 ? 0 : (size_t) size;
}

static void
Cache_LegacyInventory(qboolean clear)
{
    sys_directory_t * directory = Sys_OpenDirectory(archive.directory);
    char name[MAX_OSPATH], path[MAX_OSPATH * 2];

    archive.legacy = 0;
    if (!directory) return;

    while (Sys_ReadDirectory(directory, name, sizeof(name))) {
        if (!Cache_OwnedName(name)) continue;
        if (snprintf(path, sizeof(path), "%s/%s", archive.directory, name) >= (int) sizeof(path)) continue;
        if (!clear || remove(path)) archive.legacy += Cache_FileSize(path);
    }
    Sys_CloseDirectory(directory);
}

static void
Cache_RemoveSpareFiles(void)
{
    char path[MAX_OSPATH * 2];
    const char * names[] = { "assets.tmp", "assets.bak" };
    unsigned i;

    for (i = 0; i < 2; ++i)
        if (Cache_ArchivePath(path, sizeof(path), names[i]) && strcmp(path, archive.path)) remove(path);
}

static qboolean
Cache_Ensure(qboolean create)
{
    char directory[MAX_OSPATH * 2], path[MAX_OSPATH * 2];
    const char * names[] = { "assets.cache", "assets.tmp", "assets.bak" };
    unsigned i;

    if (!Cache_Directory(directory, sizeof(directory))) return false;

    if (strcmp(directory, archive.directory)) {
        Cache_Close(true);
        Q_strlcpy(archive.directory, directory, sizeof(archive.directory));
        Cache_LegacyInventory(false);
        for (i = 0; i < 3; ++i) {
            if (!Cache_ArchivePath(path, sizeof(path), names[i])) continue;
            archive.file     = fopen(path, "r+b");
            archive.writable = archive.file != NULL;
            if (!archive.file) archive.file = fopen(path, "rb");
            if (!archive.file) continue;
            if (Cache_ReadPack()) {
                if (!i) {
                    Q_strlcpy(archive.path, path, sizeof(archive.path));
                    Cache_RemoveSpareFiles();
                    return true;
                }
                fclose(archive.file);
                archive.file = NULL;
                if (i) {
                    char main_path[MAX_OSPATH * 2];
                    if (!Cache_ArchivePath(main_path, sizeof(main_path), "assets.cache")) return false;

                    remove(main_path);
                    if (!rename(path, main_path)) Q_strlcpy(path, main_path, sizeof(path));
                }
                Q_strlcpy(archive.path, path, sizeof(archive.path));
                archive.file     = fopen(path, "r+b");
                archive.writable = archive.file != NULL;
                if (!archive.file) archive.file = fopen(path, "rb");
                if (archive.file && Cache_ArchivePath(path, sizeof(path), "assets.cache") && !strcmp(path,
                  archive.path))
                    Cache_RemoveSpareFiles();
                return archive.file != NULL;
            }
            fclose(archive.file);
            archive.file  = NULL;
            archive.count = 0;
            archive.live  = 0;
            if (archive.slots) memset(archive.slots, 0, archive.slot_count * sizeof(*archive.slots));
        }
        if (!Cache_ArchivePath(archive.path, sizeof(archive.path), "assets.cache")) return false;
        // An unknown/corrupt main file is disposable, but do not destroy it on a read.
    }
    if (archive.file || !create) return true;

    if (!Cache_Limit()) return false;

    COM_CreatePath(archive.path);
    archive.file = fopen(archive.path, "wb");
    if (archive.file) {
        byte slots[CACHE_PACK_HEADER];
        Cache_Superblock(slots, 0, 0, 0, CACHE_PACK_HEADER, 0);
        memcpy(slots + CACHE_SLOT_SIZE, slots, CACHE_SLOT_SIZE);
        qboolean saved = fwrite(slots, 1, sizeof(slots), archive.file) == sizeof(slots) && !fflush(archive.file);
        if (fclose(archive.file)) saved = false;
        archive.file = saved ? fopen(archive.path, "r+b") : NULL;
    }
    archive.writable = archive.file != NULL;
    archive.tail     = CACHE_PACK_HEADER;
    archive.physical = archive.file ? CACHE_PACK_HEADER : Cache_FileSize(archive.path);
    archive.sequence = archive.generation = 0;
    archive.dirty    = false;
    return archive.file != NULL;
} /* Cache_Ensure */

static qboolean
Cache_WriteRecord(FILE * file, uint32_t start, uint32_t sequence, byte * header,
  asset_cache_read_fn read, void * context)
{
    byte buffer[4096], footer[CACHE_FOOTER_SIZE], empty[CACHE_HEADER_SIZE] = { 0 };
    size_t remaining = Cache_GetWord(header + 24), bytes = CACHE_HEADER_SIZE + remaining + CACHE_FOOTER_SIZE;
    uint32_t crc = 0xffffffffu;

    if (start > INT_MAX - bytes) return false;

    clearerr(file);
    if (fseek(file, start, SEEK_SET) || fwrite(empty, 1, sizeof(empty), file) != sizeof(empty)) return false;

    while (remaining) {
        size_t count = min(remaining, sizeof(buffer));
        if (!read(context, buffer, count) || fwrite(buffer, 1, count, file) != count) return false;

        crc        = Cache_UpdateCRC(crc, buffer, count);
        remaining -= count;
    }
    Cache_PutWord(header + 28, crc ^ 0xffffffffu);
    Cache_PutWord(header + 176, AssetCache_CRC32(header, 176));
    Cache_Footer(footer, start, bytes, sequence, Cache_GetWord(header + 176), 1);
    return !fseek(file, start, SEEK_SET) && fwrite(header, 1, CACHE_HEADER_SIZE, file) == CACHE_HEADER_SIZE &&
           !fseek(file, start + bytes - sizeof(footer), SEEK_SET) && fwrite(footer, 1, sizeof(footer),
             file) == sizeof(footer) && !fflush(file);
}

static qboolean
Cache_ReadPayload(const cache_index_t * item, asset_cache_data_t * result, qboolean allocate)
{
    byte buffer[4096], footer[CACHE_FOOTER_SIZE];
    size_t size = Cache_GetWord(item->header + 24), offset = 0;
    uint32_t crc = 0xffffffffu;
    int i;

    memset(result, 0, sizeof(*result));
    if (allocate && !(result->data = malloc(size))) return false;

    clearerr(archive.file);
    if (fseek(archive.file, item->offset + CACHE_HEADER_SIZE, SEEK_SET)) goto bad;
    while (offset < size) {
        size_t count = min(size - offset, sizeof(buffer));
        void * dest  = allocate ? (byte *) result->data + offset : buffer;
        if (fread(dest, 1, count, archive.file) != count) goto bad;
        crc     = Cache_UpdateCRC(crc, dest, count);
        offset += count;
    }
    if ((crc ^ 0xffffffffu) != Cache_GetWord(item->header + 28) ||
      fread(footer, 1, sizeof(footer), archive.file) != sizeof(footer) ||
      !Cache_CheckFooter(footer, item->offset, Cache_RecordBytes(item->header), item->sequence,
      Cache_GetWord(item->header + 176), 1)) goto bad;
    result->size = size;
    for (i = 0; i < 4; ++i) result->metadata[i] = Cache_GetWord(item->header + 32 + i * 4);
    return true;

bad:
    free(result->data);
    result->data = NULL;
    return false;
}

static qboolean
Cache_ReadFile(void * context, void * data, size_t size)
{
    return fread(data, 1, size, context) == size;
}

static qboolean
Cache_LoadLegacy(asset_cache_kind_t kind, const char * name, uint32_t source_crc,
  uint32_t variant, asset_cache_data_t * result, qboolean allocate)
{
    char path[MAX_OSPATH * 2];
    byte header[CACHE_HEADER_SIZE];
    FILE * file;
    uint32_t crc;
    int i;
    qboolean valid = false;

    if (!archive.legacy || !Cache_Path(kind, name, variant, path,
      sizeof(path)) || !(file = fopen(path, "rb")))
        return false;

    if (fread(header, 1, sizeof(header), file) != sizeof(header) || !Cache_ValidHeader(header) ||
      Cache_GetWord(header + 16) != source_crc || Cache_GetWord(header + 12) != (uint32_t) kind ||
      Cache_GetWord(header + 20) != variant || strcmp((const char *) header + 48, name)) goto done;
    result->size = Cache_GetWord(header + 24);
    if (allocate) {
        if (!(result->data = malloc(result->size)) || fread(result->data, 1, result->size, file) != result->size ||
          AssetCache_CRC32(result->data, result->size) != Cache_GetWord(header + 28)) goto done;
    } else if (!AssetCache_FileCRC(file, result->size, &crc) || crc != Cache_GetWord(header + 28) || fseek(file,
      result->size, SEEK_CUR)) goto done;
    if (fgetc(file) != EOF) goto done;
    for (i = 0; i < 4; ++i) result->metadata[i] = Cache_GetWord(header + 32 + i * 4);
    valid = true;
    if (asset_cache.value && !fseek(file, CACHE_HEADER_SIZE, SEEK_SET)) {
        migration_credit = Cache_FileSize(path);
        qboolean imported = AssetCache_StoreStream(kind, name, source_crc, variant, result, Cache_ReadFile, file);
        size_t credit     = migration_credit;
        migration_credit = 0;
        fclose(file);
        file = NULL;
        if (imported && !remove(path)) archive.legacy -= min(archive.legacy, credit);
    }
done:
    if (file) fclose(file);
    if (!valid) { free(result->data); memset(result, 0, sizeof(*result)); }
    return valid;
} /* Cache_LoadLegacy */

static void
Cache_ImportLegacyFile(const char * name)
{
    char path[MAX_OSPATH * 2];
    byte header[CACHE_HEADER_SIZE];
    asset_cache_data_t result;
    FILE * file;
    int index;

    if (!Cache_OwnedName(name) || !Cache_ArchivePath(path, sizeof(path), name) || !(file = fopen(path, "rb"))) return;

    qboolean valid = fread(header, 1, sizeof(header), file) == sizeof(header) && Cache_ValidHeader(header);
    fclose(file);
    if (!valid) {
        size_t bytes = Cache_FileSize(path);
        if (!remove(path)) archive.legacy -= min(archive.legacy, bytes);
        return;
    }

    Q_strlcpy(build.current, (const char *) header + 48, sizeof(build.current));
    index = Cache_Find(header);
    if (index >= 0 && Cache_ReadPayload(&archive.entries[index], &result, false)) {
        size_t bytes = Cache_FileSize(path);
        if (!remove(path)) archive.legacy -= min(archive.legacy, bytes);
        return;
    }
    Cache_LoadLegacy(Cache_GetWord(header + 12), (const char *) header + 48,
      Cache_GetWord(header + 16), Cache_GetWord(header + 20), &result, false);
}

static qboolean
Cache_Load(asset_cache_kind_t kind, const char * name, uint32_t source_crc,
  uint32_t variant, asset_cache_data_t * result, qboolean allocate)
{
    byte key[CACHE_HEADER_SIZE] = { 0 };
    int index;

    memset(result, 0, sizeof(*result));
    if (!AssetCache_Enabled() || !name || !name[0] || strlen(name) >= CACHE_NAME_SIZE ||
      kind < ASSET_CACHE_IMAGE || kind > ASSET_CACHE_MAP || !Cache_Ensure(false)) return false;

    Cache_PutWord(key + 12, kind);
    Cache_PutWord(key + 20, variant);
    Q_strlcpy((char *) key + 48, name, CACHE_NAME_SIZE);
    index = Cache_Find(key);
    if (index >= 0) {
        if (Cache_GetWord(archive.entries[index].header + 16) == source_crc &&
          Cache_ReadPayload(&archive.entries[index], result, allocate)) { ++hits; return true; }
        ++invalid;
    }
    if (Cache_LoadLegacy(kind, name, source_crc, variant, result, allocate)) { ++hits; return true; }
    ++misses;
    return false;
}

qboolean
AssetCache_Load(asset_cache_kind_t kind, const char * name, uint32_t source_crc, uint32_t variant,
  asset_cache_data_t * result)
{
    return Cache_Load(kind, name, source_crc, variant, result, true);
}

qboolean
AssetCache_Validate(asset_cache_kind_t kind, const char * name, uint32_t source_crc, uint32_t variant,
  asset_cache_data_t * result)
{
    return Cache_Load(kind, name, source_crc, variant, result, false);
}

qboolean
AssetCache_StoreStream(asset_cache_kind_t kind, const char * name, uint32_t source_crc,
  uint32_t variant, const asset_cache_data_t * entry, asset_cache_read_fn read, void * context)
{
    byte header[CACHE_HEADER_SIZE];
    unsigned count;
    size_t limit, projected;

    if (!AssetCache_Enabled() || !read || !name || !name[0] || strlen(name) >= CACHE_NAME_SIZE ||
      kind < ASSET_CACHE_IMAGE || kind > ASSET_CACHE_MAP || !entry->size || entry->size > CACHE_ENTRY_LIMIT ||
      !(limit = Cache_Limit()) || !Cache_Ensure(true) || !archive.writable) return false;

    Cache_MakeHeader(header, kind, name, source_crc, variant, entry);
    count     = archive.count + (Cache_Find(header) < 0);
    projected = max(archive.physical,
        archive.tail + Cache_RecordBytes(header) + CACHE_INDEX_HEADER + count * CACHE_INDEX_ENTRY + CACHE_FOOTER_SIZE);
    if (projected + archive.legacy - min(archive.legacy, migration_credit) > limit) {
        size_t compacted = CACHE_PACK_HEADER + archive.live + Cache_RecordBytes(header)
          + 2 * (CACHE_INDEX_HEADER + count * CACHE_INDEX_ENTRY + CACHE_FOOTER_SIZE);
        if (compacted + archive.legacy - min(archive.legacy,
          migration_credit) > limit || !Cache_CompactArchive())
            return false;
    }
    if (!Cache_Reserve(count)) { ++failures; return false; }
    if (!Cache_WriteRecord(archive.file, archive.tail, archive.sequence + 1, header, read, context)) {
        long end;
        clearerr(archive.file);
        if (!fseek(archive.file, 0,
          SEEK_END) && (end = ftell(archive.file)) >= 0) archive.physical = max(archive.physical, (size_t) end);
        ++failures;
        return false;
    }
    Cache_Index(header, archive.tail, ++archive.sequence);
    archive.tail    += Cache_RecordBytes(header);
    archive.physical = max(archive.physical, archive.tail);
    archive.dirty    = true;
    ++archive.pending;
    ++writes;
    if (archive.pending >= CACHE_CHECKPOINT_INTERVAL) Cache_Checkpoint();
    return true;
} /* AssetCache_StoreStream */

static qboolean
Cache_ReadMemory(void * context, void * data, size_t size)
{
    const byte ** source = context;

    memcpy(data, *source, size);
    *source += size;
    return true;
}

qboolean
AssetCache_Store(asset_cache_kind_t kind, const char * name, uint32_t source_crc, uint32_t variant,
  const asset_cache_data_t * entry)
{
    const byte * source = entry->data;

    return source && AssetCache_StoreStream(kind, name, source_crc, variant, entry, Cache_ReadMemory, &source);
}

static qboolean
Cache_CompactArchive(void)
{
    char temp[MAX_OSPATH * 2], backup[MAX_OSPATH * 2], main_path[MAX_OSPATH * 2];
    byte slots[CACHE_PACK_HEADER];
    cache_index_t * entries;
    FILE * file;
    unsigned i, count = 0;
    uint32_t tail = CACHE_PACK_HEADER, sequence = 0, generation = archive.generation;
    qboolean saved = false;

    if (!archive.file || !archive.writable) return false;

    entries = malloc(max(archive.count, 1) * sizeof(*entries));
    if (!entries) return false;

    if (!Cache_ArchivePath(temp, sizeof(temp), "assets.tmp") || !Cache_ArchivePath(backup, sizeof(backup),
      "assets.bak"))
    {
        free(entries);
        return false;
    }
    Q_strlcpy(main_path, archive.path, sizeof(main_path));
    file = fopen(temp, "wb");
    if (!file) { free(entries); return false; }
    Cache_Superblock(slots, 0, 0, 0, CACHE_PACK_HEADER, 0);
    memcpy(slots + CACHE_SLOT_SIZE, slots, CACHE_SLOT_SIZE);
    if (fwrite(slots, 1, sizeof(slots), file) != sizeof(slots)) goto done;
    for (i = 0; i < archive.count; ++i) {
        asset_cache_data_t checked;
        cache_index_t item = archive.entries[i];
        if (!Cache_ReadPayload(&item, &checked, false)) {
            if (ferror(archive.file)) goto done;
            continue;
        }
        uint32_t expected_crc = Cache_GetWord(item.header + 28);
        if (fseek(archive.file, item.offset + CACHE_HEADER_SIZE, SEEK_SET) ||
          !Cache_WriteRecord(file, tail, sequence + 1, item.header, Cache_ReadFile, archive.file) ||
          Cache_GetWord(item.header + 28) != expected_crc) goto done;
        item.offset      = tail;
        item.sequence    = ++sequence;
        entries[count++] = item;
        tail += Cache_RecordBytes(item.header);
    }
    saved = Cache_WriteSnapshot(file, entries, count, sequence, &tail, &generation);
done:
    if (fclose(file)) saved = false;
    free(entries);
    if (!saved) { remove(temp); return false; }
    // Keep a recoverable old archive until the replacement has been installed.
    fclose(archive.file);
    archive.file = NULL;
    remove(backup);
    saved = !rename(main_path, backup);
    if (saved && rename(temp, main_path)) { rename(backup, main_path); saved = false; }
    if (saved) remove(backup);
    Cache_Close(false);
    Cache_Ensure(false);
    return saved;
} /* Cache_CompactArchive */

void
AssetCache_Compact(void)
{
    AssetCache_CancelBuild();
    qboolean saved = AssetCache_Enabled() && Cache_Ensure(false) && Cache_CompactArchive();
    Q_strlcpy(build.status, saved ? "Unused cache space reclaimed." : "Unable to compact asset cache.",
      sizeof(build.status));
    Con_Printf("%s\n", build.status);
}

void
AssetCache_Clear(void)
{
    char directory[MAX_OSPATH * 2], path[MAX_OSPATH * 2];
    const char * names[] = { "assets.cache", "assets.tmp", "assets.bak" };
    unsigned i;

    AssetCache_CancelBuild();
    Cache_Close(false);
    memset(&build, 0, sizeof(build));
    if (Cache_Directory(directory, sizeof(directory))) {
        Q_strlcpy(archive.directory, directory, sizeof(archive.directory));
        Cache_LegacyInventory(true);
        for (i = 0; i < 3; ++i) if (Cache_ArchivePath(path, sizeof(path), names[i])) remove(path);
        Cache_Close(false);
    }
    Q_strlcpy(build.status, "Asset cache cleared.", sizeof(build.status));
    Con_Printf("Asset cache cleared.\n");
}

void
AssetCache_GetStats(asset_cache_stats_t * stats)
{
    Cache_Ensure(false);
    stats->hits         = hits;
    stats->misses       = misses;
    stats->invalid      = invalid;
    stats->writes       = writes;
    stats->failures     = failures;
    stats->stored_bytes = archive.physical + archive.legacy;
}

void
AssetCache_Flush(void)
{
    Cache_Checkpoint();
}

static void
Cache_Stats_f(void)
{
    asset_cache_stats_t stats;

    AssetCache_GetStats(&stats);
    Con_Printf("Asset cache: %u hits, %u misses, %u invalid, %u writes, %u write failures; %.1f MB stored.\n",
      stats.hits, stats.misses, stats.invalid, stats.writes, stats.failures,
      stats.stored_bytes / (1024.0 * 1024.0));
    size_t packed = CACHE_PACK_HEADER + archive.live + CACHE_INDEX_HEADER + archive.count * CACHE_INDEX_ENTRY
      + CACHE_FOOTER_SIZE;
    Con_Printf("Archive: %u indexed entries, %.1f MiB reclaimable.\n", archive.count,
      (archive.physical - min(archive.physical, packed)) / (1024.0 * 1024.0));
}

static void
Cache_ReadConfiguration(void)
{
    char path[MAX_OSPATH * 2], line[512];
    FILE * file;
    int i;

    snprintf(path, sizeof(path), "%s/%sconfig.cfg%s", com_gamedir, FILE_SPECIAL_PREFIX, FILE_SPECIAL_SUFFIX);
    file = fopen(path, "rb");
    if (file) {
        while (fgets(line, sizeof(line), file)) {
            char * value     = COM_Parse(line);
            cvar_t * setting = !strcmp(com_token, "asset_cache") ? &asset_cache :
              !strcmp(com_token, "asset_cache_maxmb") ? &asset_cache_maxmb : NULL;
            if (setting && value) {
                COM_Parse(value);
                if (com_token[0])
                    Cvar_Set(setting->name, com_token);
            }
        }
        fclose(file);
    }
    for (i = 1; i + 1 < com_argc; ++i) {
        if (!strcmp(com_argv[i], "+asset_cache"))
            Cvar_Set("asset_cache", com_argv[i + 1]);
        else if (!strcmp(com_argv[i], "+asset_cache_maxmb"))
            Cvar_Set("asset_cache_maxmb", com_argv[i + 1]);
    }
}

// Keep traversal bounded and prepare at most one asset per frame. No full asset
// list or live renderer/sound resources are retained during a manual build.
#define CACHE_BUILD_DEPTH 16
static searchpath_t * build_search;
static searchpath_t * build_search_root;
static char build_game[MAX_OSPATH];
static int build_pack_index, build_depth;
static char (*build_items)[MAX_QPATH];
static size_t build_capacity;
static struct {
    sys_directory_t * directory;
    char              relative[MAX_QPATH];
} build_folders[CACHE_BUILD_DEPTH];

const asset_cache_build_t *
AssetCache_GetBuildState(void)
{
    return &build;
}

void
AssetCache_CancelBuild(void)
{
    if (legacy_import) { Sys_CloseDirectory(legacy_import); legacy_import = NULL; }
    while (build_depth)
        Sys_CloseDirectory(build_folders[--build_depth].directory);
    if (build.active)
        Q_strlcpy(build.status, "Cache build cancelled.", sizeof(build.status));
    build.active = false;
    build_search = NULL;
    free(build_items);
    build_items    = NULL;
    build_capacity = 0;
}

void
AssetCache_BeginBuild(void)
{
    AssetCache_CancelBuild();
    memset(&build, 0, sizeof(build));
    if (!AssetCache_Enabled()) {
        Q_strlcpy(build.status, "Enable Asset Cache before building.", sizeof(build.status));
        return;
    }
    build.active     = true;
    build.scanning   = true;
    build_search     = build_search_root = com_searchpaths;
    build_pack_index = 0;
    Q_strlcpy(build_game, com_gamedir, sizeof(build_game));
    Q_strlcpy(build.status, "Counting cacheable assets...", sizeof(build.status));
    if (Cache_Ensure(false) && archive.legacy && (legacy_import = Sys_OpenDirectory(archive.directory)))
        Q_strlcpy(build.status, "Importing existing cache entries.", sizeof(build.status));
}

static qboolean
Cache_BuildOpenDirectory(const char * relative)
{
    char path[MAX_OSPATH * 2];
    sys_directory_t * directory;
    int count;

    if (build_depth == CACHE_BUILD_DEPTH)
        return false;

    count = snprintf(path, sizeof(path), "%s/%s", build_search->filename, relative);
    if (count < 0 || (size_t) count >= sizeof(path) || !(directory = Sys_OpenDirectory(path)))
        return false;

    build_folders[build_depth].directory = directory;
    Q_strlcpy(build_folders[build_depth++].relative, relative, MAX_QPATH);
    return true;
}

static asset_cache_kind_t
Cache_BuildKind(const char * name)
{
    const char * dot = strrchr(name, '.');
    char extension[8];

    if (!dot || strlen(dot + 1) >= sizeof(extension)) return 0;

    Q_strlcpy(extension, dot + 1, sizeof(extension));
    if (!Q_strcasecmp(extension, "wav")) return ASSET_CACHE_SOUND;

    if (!Q_strcasecmp(extension, "mdl")) return ASSET_CACHE_MODEL;

    if (!Q_strcasecmp(extension, "pcx") || !Q_strcasecmp(extension, "tga") ||
      !Q_strcasecmp(extension, "png") || !Q_strcasecmp(extension, "jpg") ||
      !Q_strcasecmp(extension, "jpeg")) return ASSET_CACHE_IMAGE;

    return 0;
}

static void
Cache_BuildAdd(const char * name)
{
    unsigned i;

    if (!Cache_BuildKind(name) || strlen(name) >= MAX_QPATH) return;

    for (i = 0; i < build.total; ++i)
        if (!strcmp(build_items[i], name)) return;

    if (build.total == build_capacity) {
        size_t capacity = build_capacity + 128;
        void * items    = realloc(build_items, capacity * sizeof(*build_items));
        if (!items) {
            AssetCache_CancelBuild();
            Q_strlcpy(build.status, "Not enough memory to scan assets.", sizeof(build.status));
            return;
        }
        build_items    = items;
        build_capacity = capacity;
    }
    Q_strlcpy(build_items[build.total++], name, MAX_QPATH);
}

static void
Cache_BuildAsset(const char * name)
{
    qboolean prepared = false;
    unsigned old_hits = hits, old_writes = writes;

    Q_strlcpy(build.current, name, sizeof(build.current));
    switch (Cache_BuildKind(name)) {
        case ASSET_CACHE_IMAGE: prepared = Image_BuildCache(name);
            break;
        case ASSET_CACHE_SOUND: prepared = S_BuildSoundCache(name);
            break;
        case ASSET_CACHE_MODEL: prepared = Mod_BuildAliasCache(name);
            break;
        default: break;
    }
    ++build.checked;
    if (prepared && (hits > old_hits || writes > old_writes)) ++build.cached;
    else ++build.skipped;
}

void
AssetCache_BuildFrame(void)
{
    int work;

    if (!build.active)
        return;

    if (!AssetCache_Enabled() || strcmp(build_game, com_gamedir) || build_search_root != com_searchpaths) {
        AssetCache_CancelBuild();
        return;
    }
    if (legacy_import) {
        char name[MAX_OSPATH];
        for (work = 0; work < 32; ++work) {
            if (!Sys_ReadDirectory(legacy_import, name, sizeof(name))) {
                Sys_CloseDirectory(legacy_import);
                legacy_import = NULL;
                Cache_Checkpoint();
                Q_strlcpy(build.status, "Counting cacheable assets...", sizeof(build.status));
                break;
            }
            if (Cache_OwnedName(name)) { Cache_ImportLegacyFile(name); return; }
        }
        if (legacy_import) return;
    }
    if (!build.scanning) {
        if (build.checked < build.total)
            Cache_BuildAsset(build_items[build.checked]);
        if (build.checked == build.total) {
            Cache_Checkpoint();
            build.active = false;
            snprintf(build.status, sizeof(build.status), "Cache build finished: %u/%u cached, %u skipped.",
              build.cached, build.total, build.skipped);
            free(build_items);
            build_items    = NULL;
            build_capacity = 0;
            Con_Printf("%s\n", build.status);
        }
        return;
    }
    for (work = 0; work < 32 && build_search; ++work) {
        if (build_search->pack) {
            pack_t * pack = build_search->pack;
            if (build_pack_index < pack->numfiles) {
                const char * name = pack->files[build_pack_index++].name;
                if (name[0] != '.' && strncmp(name, "_cache/", 7))
                    Cache_BuildAdd(name);

                continue;
            }
        } else {
            char name[MAX_QPATH], relative[MAX_QPATH];
            if (!build_depth && !Cache_BuildOpenDirectory(""))
                goto next_search;
            if (Sys_ReadDirectory(build_folders[build_depth - 1].directory, name, sizeof(name))) {
                const char * parent = build_folders[build_depth - 1].relative;
                if (name[0] == '.' || !strcmp(name, "_cache"))
                    continue;
                if (strlen(parent) + strlen(name) + 2 > sizeof(relative))
                    continue;
                Q_strlcpy(relative, parent, sizeof(relative));
                if (parent[0])
                    Q_strlcat(relative, "/", sizeof(relative));
                Q_strlcat(relative, name, sizeof(relative));
                if (Cache_BuildOpenDirectory(relative))
                    continue;
                Cache_BuildAdd(relative);

                continue;
            }
            Sys_CloseDirectory(build_folders[--build_depth].directory);
            if (build_depth)
                continue;
        }
next_search:
        build_search     = build_search->next;
        build_pack_index = 0;
    }
    if (build.active && !build_search) {
        build.scanning = false;
        Q_strlcpy(build.status, "Preparing installed assets...", sizeof(build.status));
    }
} /* AssetCache_BuildFrame */

void
AssetCache_Init(void)
{
    Cvar_RegisterVariable(&asset_cache);
    Cvar_RegisterVariable(&asset_cache_maxmb);
    Cmd_AddCommand("asset_cache_clear", AssetCache_Clear);
    Cmd_AddCommand("asset_cache_compact", AssetCache_Compact);
    Cmd_AddCommand("asset_cache_build", AssetCache_BeginBuild);
    Cmd_AddCommand("asset_cache_cancel", AssetCache_CancelBuild);
    Cmd_AddCommand("asset_cache_stats", Cache_Stats_f);
    Cache_ReadConfiguration();
    initialized = true;
}
