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


#ifndef ASSET_CACHE_H
#define ASSET_CACHE_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ASSET_CACHE_IMAGE   = 1,
    ASSET_CACHE_TEXTURE = 2,
    ASSET_CACHE_SOUND   = 3,
    ASSET_CACHE_MODEL   = 4,
    ASSET_CACHE_MAP     = 5
} asset_cache_kind_t;

typedef struct {
    void *   data;
    size_t   size;
    uint32_t metadata[4];
} asset_cache_data_t;

// Fill exactly size bytes. Returning false abandons and removes the entry.
typedef qboolean (*asset_cache_read_fn)(void * context, void * data, size_t size);

typedef struct {
    unsigned hits, misses, invalid, writes, failures;
    size_t   stored_bytes;
} asset_cache_stats_t;

typedef struct {
    qboolean active, scanning;
    unsigned checked, skipped, cached, total;
    char     current[MAX_QPATH];
    char     status[96];
} asset_cache_build_t;

extern cvar_t asset_cache, asset_cache_maxmb;
void
AssetCache_Init(void);
qboolean
AssetCache_Enabled(void);
uint32_t
AssetCache_CRC32(const void * data, size_t size);
uint32_t
AssetCache_HashWords(const uint32_t * words, size_t count);
qboolean
AssetCache_FileCRC(FILE * file, size_t size, uint32_t * crc);
qboolean
AssetCache_Load(asset_cache_kind_t kind, const char * name, uint32_t source_crc,
  uint32_t variant, asset_cache_data_t * result);
// Verify the complete entry and return its metadata with result->data == NULL.
qboolean
AssetCache_Validate(asset_cache_kind_t kind, const char * name, uint32_t source_crc,
  uint32_t variant, asset_cache_data_t * result);
qboolean
AssetCache_Store(asset_cache_kind_t kind, const char * name, uint32_t source_crc,
  uint32_t variant, const asset_cache_data_t * entry);
qboolean
AssetCache_StoreStream(asset_cache_kind_t kind, const char * name, uint32_t source_crc,
  uint32_t variant, const asset_cache_data_t * entry, asset_cache_read_fn read, void * context);
void
AssetCache_Clear(void);
void
AssetCache_Compact(void);
void
AssetCache_Shutdown(void);
void
AssetCache_GetStats(asset_cache_stats_t * stats);
void
AssetCache_Flush(void);
void
AssetCache_BeginBuild(void);
void
AssetCache_CancelBuild(void);
void
AssetCache_BuildFrame(void);
const asset_cache_build_t *
AssetCache_GetBuildState(void);

#ifdef __cplusplus
}
#endif
#endif // ifndef ASSET_CACHE_H
