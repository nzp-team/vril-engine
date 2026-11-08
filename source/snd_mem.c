/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2026 NZ:P Team.

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
// snd_mem.c: sound caching

#include "nzportable_def.h"
#include <limits.h>

int			cache_full_cycle;

byte *S_Alloc (int size);

/*
================
ResampleSfx
================
*/
static void S_ResampleSfx (sfxcache_t *sc, int inrate, int inwidth, byte *data)
{
	int		outcount;
	int		srcsample;
	float	stepscale;
	int		i;
	int		sample;
	uint64_t samplefrac, fracstep;

	stepscale = (float)inrate / shm->speed;	// this is usually 0.5, 1, or 2

	outcount = sc->length / stepscale;
	sc->length = outcount;
	if (sc->loopstart != -1)
		sc->loopstart = sc->loopstart / stepscale;

	sc->speed = shm->speed;
	if (loadas8bit.value)
		sc->width = 1;
	else
		sc->width = inwidth;
	sc->stereo = 0;

// resample / decimate to the current source rate

	if (stepscale == 1 && inwidth == 1 && sc->width == 1)
	{
// fast special case
		for (i=0 ; i<outcount ; i++)
			((signed char *)sc->data)[i]
			= (int)( (unsigned char)(data[i]) - 128);
	}
	else
	{
// general case
		samplefrac = 0;
		fracstep = stepscale*256;
		for (i=0 ; i<outcount ; i++)
		{
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if (inwidth == 2)
				sample = LittleShort ( ((short *)data)[srcsample] );
			else
				sample = (int)( (unsigned char)(data[srcsample]) - 128) * 256;
			if (sc->width == 2)
				((short *)sc->data)[i] = sample;
			else
				((signed char *)sc->data)[i] = sample >> 8;
		}
	}
}

void ResampleSfx (sfx_t *sfx, int inrate, int inwidth, byte *data)
{
	sfxcache_t *sc = Cache_Check(&sfx->cache);
	if (sc)
		S_ResampleSfx(sc, inrate, inwidth, data);
}

//=============================================================================

/*
==============
S_LoadSound
==============
*/
sfxcache_t *S_LoadSound (sfx_t *s)
{
    char	namebuffer[256];
	byte	*data;
	wavinfo_t	info;
	int		len, length;
	FILE	*file;
	qboolean	allocated = false;
	float	stepscale;
	sfxcache_t	*sc;
	asset_cache_data_t cached = {0};
	uint32_t source_crc = 0, variant;
	uint32_t settings[4];
	qboolean use_cache;
	byte	stackbuf[1*1024];		// avoid dirtying the cache heap

// see if still in memory
	if ((sc = Cache_Check (&s->cache)))
		return sc;

	if (!shm || shm->speed <= 0)
		return NULL;
	settings[0] = 0x534e4401u; // Prepared mono PCM, version 1.
	settings[1] = shm->speed;
	settings[2] = loadas8bit.value != 0;
	settings[3] = LittleShort(1) == 1;
	variant = AssetCache_HashWords(settings, 4);

//Con_Printf ("S_LoadSound: %x\n", (int)stackbuf);
// load it in
    Q_strcpy(namebuffer, "");
    Q_strcat(namebuffer, s->name);

//	Con_Printf ("loading %s\n",namebuffer);

	length = COM_FOpenFile(namebuffer, &file);
	if (!file || length <= 0)
	{
		if (file)
			fclose(file);
		Con_Printf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}
	use_cache = AssetCache_Enabled() && AssetCache_FileCRC(file, length, &source_crc);
	if (use_cache && AssetCache_Load(ASSET_CACHE_SOUND, s->name, source_crc, variant, &cached)) {
		unsigned samples = cached.metadata[0], loop = cached.metadata[1];
		unsigned width = cached.metadata[3];
		if (samples && samples <= 16u * 1024u * 1024u && (width == 1 || width == 2) &&
			cached.metadata[2] == (unsigned)shm->speed && loop <= samples &&
			cached.size == (size_t)samples * width) {
			sc = Cache_Alloc(&s->cache, sizeof(*sc) + cached.size, s->name);
			if (sc) {
				sc->length = samples;
				sc->loopstart = (int)loop - 1;
				sc->speed = shm->speed;
				sc->width = width;
				sc->stereo = 0;
				memcpy(sc->data, cached.data, cached.size);
			}
			free(cached.data);
			fclose(file);
			return sc;
		}
		free(cached.data);
	}
	data = length + 1 <= sizeof(stackbuf) ? stackbuf : malloc(length + 1);
	if (data)
	{
		allocated = data != stackbuf;
		if (fread(data, 1, length, file) != length) {
			fclose(file);
			if (allocated)
				free(data);
			return NULL;
		}
		data[length] = 0;
		fclose(file);
	}
	else
	{
		fclose(file);
		data = COM_LoadStackFile(namebuffer, stackbuf, sizeof(stackbuf));
		if (!data)
			return NULL;
	}

	info = GetWavinfo (s->name, data, length);
	if (info.channels != 1 || info.rate <= 0 || (info.width != 1 && info.width != 2) ||
		info.samples <= 0 || info.dataofs < 0 || info.dataofs > length ||
		info.samples > (length - info.dataofs) / info.width)
	{
		Con_DPrintf("Unsupported or invalid WAV: %s\n", s->name);
		if (allocated)
			free(data);
		return NULL;
	}

	stepscale = (float)info.rate / shm->speed;
	if ((double)info.samples / (double)stepscale < 1 || (double)info.samples / (double)stepscale >
		(INT_MAX - (int)sizeof(sfxcache_t)) /
		(loadas8bit.value ? 1 : info.width)) {
		if (allocated)
			free(data);
		return NULL;
	}
	len = info.samples / stepscale;

	len = len * (loadas8bit.value ? 1 : info.width) * info.channels;

	sc = Cache_Alloc(&s->cache, len + sizeof(sfxcache_t), s->name);
	if (!sc)
	{
		if (allocated)
			free(data);
		return NULL;
	}

	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	S_ResampleSfx(sc, sc->speed, sc->width, data + info.dataofs);
	if (use_cache && sc->length > 0) {
		memset(&cached, 0, sizeof(cached));
		cached.data = sc->data;
		cached.size = (size_t)sc->length * sc->width;
		cached.metadata[0] = sc->length;
		cached.metadata[1] = sc->loopstart + 1;
		cached.metadata[2] = sc->speed;
		cached.metadata[3] = sc->width;
		AssetCache_Store(ASSET_CACHE_SOUND, s->name, source_crc, variant, &cached);
	}
	if (allocated)
		free(data);

	return sc;
}

// MARK: Sound Cache

typedef struct {
    FILE *    file;
    long      base;
    wavinfo_t info;
    int       width;
    size_t    window_begin, window_size;
    uint64_t  phase, step;
    byte      window[4096];
} sound_cache_stream_t;

static uint32_t
S_WavWord(const byte * bytes)
{
    return (uint32_t) bytes[0] | (uint32_t) bytes[1] << 8
           | (uint32_t) bytes[2] << 16 | (uint32_t) bytes[3] << 24;
}

static qboolean
S_ReadWavAt(FILE * file, long base, int length, int offset, void * data, size_t size)
{
    return offset >= 0 && offset <= length && size <= (size_t) (length - offset) &&
           !fseek(file, base + offset, SEEK_SET) && fread(data, 1, size, file) == size;
}

static qboolean
S_ReadWavCacheInfo(FILE * file, long base, int length, wavinfo_t * info)
{
    byte header[32];
    int offset = 12, end, data_bytes = -1, loop_length = -1;
    qboolean format_found = false, cue_found = false, list_found = false;

    memset(info, 0, sizeof(*info));
    info->loopstart = -1;
    if (!S_ReadWavAt(file, base, length, 0, header, 12) || memcmp(header, "RIFF", 4) ||
      memcmp(header + 8, "WAVE", 4) || S_WavWord(header + 4) < 4 ||
      S_WavWord(header + 4) > (uint32_t) (length - 8)) return false;

    end = S_WavWord(header + 4) + 8;
    while (offset <= end - 8) {
        int bytes;
        if (!S_ReadWavAt(file, base, end, offset, header, 8) ||
          S_WavWord(header + 4) > (uint32_t) (end - offset - 8)) return false;

        bytes = S_WavWord(header + 4);
        if (!memcmp(header, "fmt ", 4) && !format_found) {
            if (bytes < 16 || !S_ReadWavAt(file, base, end, offset + 8, header, 16) ||
              header[0] != 1 || header[1] || header[2] != 1 || header[3] ||
              header[15] || (header[14] != 8 && header[14] != 16) ||
              !S_WavWord(header + 4) || S_WavWord(header + 4) > INT_MAX) return false;

            info->rate     = S_WavWord(header + 4);
            info->width    = header[14] / 8;
            info->channels = 1;
            format_found   = true;
        } else if (!memcmp(header, "cue ", 4) && !cue_found) {
            cue_found = true;
            if (bytes >= 28) {
                if (!S_ReadWavAt(file, base, end, offset + 8, header, 28) ||
                  S_WavWord(header + 24) > INT_MAX) return false;

                info->loopstart = S_WavWord(header + 24);
            }
        } else if (!memcmp(header, "LIST", 4) && cue_found && !list_found) {
            list_found = true;
            if (bytes >= 24) {
                if (!S_ReadWavAt(file, base, end, offset + 8, header, 24)) return false;

                if (!memcmp(header + 20, "mark", 4)) {
                    if (S_WavWord(header + 16) > INT_MAX) return false;

                    loop_length = S_WavWord(header + 16);
                }
            }
        } else if (!memcmp(header, "data", 4) && data_bytes < 0) {
            info->dataofs = offset + 8;
            data_bytes    = bytes;
        }
        offset += 8 + bytes;
        if ((bytes & 1) && offset < end) ++offset;
    }
    if (!format_found || data_bytes < 0 || data_bytes % info->width) return false;

    info->samples = data_bytes / info->width;
    if (loop_length >= 0) {
        if (info->loopstart < 0 || loop_length > INT_MAX - info->loopstart) return false;

        if (info->loopstart + loop_length)
            info->samples = info->loopstart + loop_length;
    }
    return info->samples > 0 && info->samples <= data_bytes / info->width &&
           info->loopstart < info->samples;
} /* S_ReadWavCacheInfo */

static qboolean
S_ReadPreparedSound(void * context, void * data, size_t size)
{
    sound_cache_stream_t * stream = context;
    byte * output = data;
    size_t i;

    if (size % stream->width) return false;

    for (i = 0; i < size; i += stream->width) {
        size_t index = stream->phase >> 8, offset;
        int sample;
        if (index >= (size_t) stream->info.samples) return false;

        offset = index * stream->info.width;
        if (offset < stream->window_begin || offset + stream->info.width > stream->window_begin + stream->window_size) {
            size_t bytes = (size_t) stream->info.samples * stream->info.width;
            stream->window_begin = offset & ~(sizeof(stream->window) - 1);
            stream->window_size  = min(sizeof(stream->window), bytes - stream->window_begin);
            if (fseek(stream->file, stream->base + stream->info.dataofs + stream->window_begin, SEEK_SET) ||
              fread(stream->window, 1, stream->window_size, stream->file) != stream->window_size) return false;
        }
        offset -= stream->window_begin;
        if (stream->info.width == 2)
            sample = (short) ((unsigned) stream->window[offset] | (unsigned) stream->window[offset + 1] << 8);
        else
            sample = ((int) stream->window[offset] - 128) * 256;
        if (stream->width == 2) {
            short value = sample;
            memcpy(output + i, &value, sizeof(value));
        } else output[i] = (byte) (sample >> 8);
        stream->phase += stream->step;
    }
    return true;
} /* S_ReadPreparedSound */

qboolean
S_BuildSoundCache(const char * name)
{
    sound_cache_stream_t stream = { 0 };
    asset_cache_data_t entry = { 0 }, cached;
    uint32_t crc, variant, settings[4];
    float stepscale;
    int length, samples, loop;
    qboolean saved;

    if (!AssetCache_Enabled() || !shm || shm->speed <= 0 || strlen(name) >= MAX_QPATH) return false;

    length = COM_FOpenFile((char *) name, &stream.file);
    if (!stream.file) return false;

    stream.base = ftell(stream.file);
    if (stream.base < 0 || !S_ReadWavCacheInfo(stream.file, stream.base, length, &stream.info)) {
        Con_DPrintf("Sound cache: unsupported or invalid PCM WAV: %s\n", name);
        fclose(stream.file);
        return false;
    }
    stepscale    = (float) stream.info.rate / shm->speed;
    stream.width = loadas8bit.value ? 1 : stream.info.width;
    if ((double) stream.info.samples / (double) stepscale < 1 ||
      (double) stream.info.samples / (double) stepscale > (16u * 1024u * 1024u) / stream.width)
    {
        fclose(stream.file);
        return false;
    }
    samples = stream.info.samples / stepscale;
    loop    = stream.info.loopstart < 0 ? -1 : stream.info.loopstart / stepscale;
    if (loop >= samples || fseek(stream.file, stream.base, SEEK_SET) ||
      !AssetCache_FileCRC(stream.file, length, &crc)) { fclose(stream.file); return false; }
    settings[0]       = 0x534e4401u;
    settings[1]       = shm->speed;
    settings[2]       = loadas8bit.value != 0;
    settings[3]       = LittleShort(1) == 1;
    variant           = AssetCache_HashWords(settings, 4);
    entry.size        = (size_t) samples * stream.width;
    entry.metadata[0] = samples;
    entry.metadata[1] = loop + 1;
    entry.metadata[2] = shm->speed;
    entry.metadata[3] = stream.width;
    if (AssetCache_Validate(ASSET_CACHE_SOUND, name, crc, variant, &cached) &&
      cached.size == entry.size && !memcmp(cached.metadata, entry.metadata, sizeof(entry.metadata)))
    {
        fclose(stream.file);
        return true;
    }
    stream.step = stepscale * 256;
    saved       = AssetCache_StoreStream(ASSET_CACHE_SOUND, name, crc, variant, &entry, S_ReadPreparedSound, &stream);
    fclose(stream.file);
    return saved;
} /* S_BuildSoundCache */



/*
===============================================================================

WAV loading

===============================================================================
*/


byte	*data_p;
byte 	*iff_end;
byte 	*last_chunk;
byte 	*iff_data;
int 	iff_chunk_len;


short GetLittleShort(void)
{
	short val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	data_p += 2;
	return val;
}

int GetLittleLong(void)
{
	uint32_t val = (uint32_t)data_p[0] | (uint32_t)data_p[1] << 8 |
		(uint32_t)data_p[2] << 16 | (uint32_t)data_p[3] << 24;
	data_p += 4;
	return val;
}

void FindNextChunk(char *name)
{
	while (1)
	{
		data_p=last_chunk;

		if (data_p > iff_end || iff_end - data_p < 8)
		{	// didn't find the chunk
			data_p = NULL;
			return;
		}

		data_p += 4;
		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0 || iff_chunk_len > iff_end - data_p)
		{
			data_p = NULL;
			return;
		}
//		if (iff_chunk_len > 1024*1024)
//			Sys_Error ("FindNextChunk: %i length is past the 1 meg sanity limit", iff_chunk_len);
		data_p -= 8;
		last_chunk = data_p + 8 + iff_chunk_len;
		if ((iff_chunk_len & 1) && last_chunk < iff_end)
			last_chunk++;
		if (!strncmp((char*) data_p, name, 4))
			return;
	}
}

void FindChunk(char *name)
{
	last_chunk = iff_data;
	FindNextChunk (name);
}


void DumpChunks(void)
{
	char	str[5];

	str[4] = 0;
	data_p=iff_data;
	do
	{
#ifdef PSP_VFPU
		memcpy_vfpu(str, data_p, 4);
#else
		memcpy(str, data_p, 4);
#endif // PSP_VFPU
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		Con_Printf ("0x%x : %s (%d)\n", (intptr_t)(data_p - 4), str, iff_chunk_len);
		data_p += (iff_chunk_len + 1) & ~1;
	} while (data_p < iff_end);
}

/*
============
GetWavinfo
============
*/
wavinfo_t GetWavinfo (char *name, byte *wav, int wavlength)
{
	wavinfo_t	info;
	int     i;
	int     format;
	int		samples;

	memset (&info, 0, sizeof(info));

	if (!wav || wavlength < 12)
		return info;

	iff_data = wav;
	iff_end = wav + wavlength;

// find "RIFF" chunk
	FindChunk("RIFF");
	if (!(data_p && iff_chunk_len >= 4 && !strncmp((char*) data_p+8, "WAVE", 4)))
	{
		Con_Printf("Missing RIFF/WAVE chunks\n");
		return info;
	}

// get "fmt " chunk
	iff_data = data_p + 12;
// DumpChunks ();

	FindChunk("fmt ");
	if (!data_p || iff_chunk_len < 16)
	{
		Con_Printf("Missing fmt chunk\n");
		return info;
	}
	data_p += 8;
	format = GetLittleShort();
	if (format != 1)
	{
		Con_Printf("Microsoft PCM format only\n");
		return info;
	}

	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 4+2;
	info.width = GetLittleShort() / 8;
	if ((info.width != 1 && info.width != 2) || info.rate <= 0)
		return info;

// get cue chunk
	FindChunk("cue ");
	if (data_p && iff_chunk_len >= 28)
	{
		data_p += 32;
		info.loopstart = GetLittleLong();
//		Con_Printf("loopstart=%d\n", sfx->loopstart);

	// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk ("LIST");
		if (data_p && iff_chunk_len >= 24)
		{
			if (!strncmp ((char*) data_p + 28, "mark", 4))
			{	// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				i = GetLittleLong ();	// samples in loop
				if (i < 0 || info.loopstart < 0 || i > INT_MAX - info.loopstart) {
					info.samples = 0;
					return info;
				}
				info.samples = info.loopstart + i;
//				Con_Printf("looped length: %i\n", i);
			}
		}
	}
	else
		info.loopstart = -1;

// find data chunk
	FindChunk("data");
	if (!data_p)
	{
		Con_Printf("Missing data chunk\n");
		return info;
	}

	data_p += 4;
	samples = GetLittleLong () / info.width;

	if (info.samples)
	{
		if (samples < info.samples)
		{
			Con_Printf("Sound %s has a bad loop length\n", name);
			info.samples = 0;
			return info;
		}
	}
	else
		info.samples = samples;

	info.dataofs = data_p - wav;
	if (info.loopstart < -1 || info.loopstart >= info.samples)
		info.samples = 0;

	return info;
}
