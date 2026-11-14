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

#include "../../nzportable_def.h"
#include "xbox360_local.h"
#include <xenon_sound/sound.h>
#include <time/time.h>
#include <xenon_soc/xenon_power.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#define MINIMP3_NO_STDIO

#include <minimp3_ex.h>

#define BLOCK_FRAMES 512
#define QUEUE_FRAMES 2048
#define RING_SAMPLES 32768
#define AUDIO_THREAD 2

static _Atomic unsigned int audio_position;
static short effects_buffer[RING_SAMPLES];
static int effects_paintedtime;
static _Atomic unsigned effects_peak;
static _Atomic int audio_lock;
static _Atomic qboolean audio_running;
static qboolean audio_threaded;
static byte audio_stack[64 * 1024] __attribute__((aligned(128)));
extern volatile int secondary_alive;
static uint64_t audio_consumed;
static int last_paintedtime;
static unsigned audio_reports;
static sfx_t * reported_sounds[128];
static unsigned reported_sound_count;
static int audio_diagnostics;
static double next_audio_report;
static qboolean audio_initialized;
static mp3dec_ex_t decoder;
static byte * music_data;
static short decoded[MINIMP3_MAX_SAMPLES_PER_FRAME], music_sample[2];
static size_t decoded_count, decoded_position;
static unsigned int music_phase;
static int track_loop;
_Atomic int music_job_started;
_Atomic int music_volume;
_Atomic qboolean music_paused;
extern int music_loop;

static void
Audio_Lock(void)
{
    while (__atomic_exchange_n(&audio_lock, 1, __ATOMIC_ACQUIRE))
        udelay(10);
}

static void
Audio_Unlock(void)
{
    __atomic_store_n(&audio_lock, 0, __ATOMIC_RELEASE);
}

static qboolean
Music_NextSample(void)
{
    int channel;

    if (decoded_position >= decoded_count) {
        decoded_count    = mp3dec_ex_read(&decoder, decoded, MINIMP3_MAX_SAMPLES_PER_FRAME);
        decoded_position = 0;
        if (!decoded_count && track_loop && !decoder.last_error && !mp3dec_ex_seek(&decoder, 0))
            decoded_count = mp3dec_ex_read(&decoder, decoded, MINIMP3_MAX_SAMPLES_PER_FRAME);
        if (!decoded_count) {
            music_job_started = 0;
            return false;
        }
    }
    for (channel = 0; channel < 2; ++channel)
        music_sample[channel] = decoded[decoded_position + (decoder.info.channels == 1 ? 0 : channel)];
    decoded_position += decoder.info.channels;
    return true;
}

static void
Music_Mix(short * samples)
{
    int i, channel;
    int volume = bound(0, music_volume, PLATFORM_VOLUME_MAX);

    if (!music_data || !music_job_started || music_paused)
        return;

    for (i = 0; i < BLOCK_FRAMES; ++i) {
        if (music_phase >= 48000) {
            music_phase -= 48000;
            if (!Music_NextSample())
                return;
        }
        for (channel = 0; channel < 2; ++channel) {
            int index = i * 2 + channel;
            int mixed = samples[index] + music_sample[channel] * volume / PLATFORM_VOLUME_MAX;
            samples[index] = bound(-32768, mixed, 32767);
        }
        music_phase += decoder.info.hz;
    }
}

static void
Audio_Fill(void)
{
    short mixed[BLOCK_FRAMES * 2];
    byte output[sizeof(mixed)];
    unsigned i;

    while (xenon_sound_get_unplayed() < QUEUE_FRAMES * 4 && xenon_sound_get_free() >= (int) sizeof(output)) {
        uint64_t prepared;
        unsigned samples, first;

        Audio_Lock();
        if (effects_paintedtime < last_paintedtime)
            audio_consumed = audio_position;
        last_paintedtime = effects_paintedtime;
        prepared = (uint64_t) max(0, effects_paintedtime) * 2;
        samples = prepared > audio_consumed ? min(prepared - audio_consumed, BLOCK_FRAMES * 2) : 0;
        first = min(samples, RING_SAMPLES - audio_position);
        memset(mixed, 0, sizeof(mixed));
        memcpy(mixed, effects_buffer + audio_position, first * sizeof(short));
        if (samples > first)
            memcpy(mixed + first, effects_buffer, (samples - first) * sizeof(short));
        audio_consumed += samples;
        audio_position = (audio_position + samples) % RING_SAMPLES;
        for (i = 0; i < samples; ++i)
            effects_peak = max(effects_peak, (unsigned) abs((int) mixed[i]));
        Music_Mix(mixed);
        Audio_Unlock();
        for (i = 0; i < BLOCK_FRAMES * 2; ++i) {
            output[i * 2] = (unsigned short) mixed[i] & 255;
            output[i * 2 + 1] = (unsigned short) mixed[i] >> 8;
        }
        xenon_sound_submit(output, sizeof(output));
    }
}

static void
Audio_Thread(void)
{
    while (audio_running) {
        Audio_Fill();
        udelay(1000);
    }
}

void
X360_AudioUpdate(void)
{
    unsigned i;

    if (!audio_initialized)
        return;
    if (!audio_threaded)
        Audio_Fill();
    if (audio_reports < 60 && Sys_FloatTime() >= next_audio_report) {
        unsigned voices = 0;
        uint64_t consumed;

        Audio_Lock();
        consumed = audio_consumed;
        Audio_Unlock();

        for (i = 0; i < (unsigned) total_channels; ++i)
            voices += channels[i].sfx && (channels[i].leftvol || channels[i].rightvol);
        Sys_Printf("audio: painted=%d consumed=%llu queued=%d effects_peak=%u voices=%u music=%d gain=%d\n",
          paintedtime, (unsigned long long) consumed, xenon_sound_get_unplayed(), __atomic_exchange_n(&effects_peak, 0, __ATOMIC_RELAXED),
          voices, music_job_started, music_volume);
        ++audio_reports;
        next_audio_report = Sys_FloatTime() + 5;
    }
}

qboolean
SNDDMA_Init(void)
{
    shm = &sn;
    shm->splitbuffer = 0;
    shm->samplebits = 16;
    shm->speed = 48000;
    shm->channels = 2;
    shm->samples = RING_SAMPLES;
    shm->samplepos = 0;
    shm->submission_chunk = BLOCK_FRAMES * 2;
    shm->buffer = calloc(RING_SAMPLES, sizeof(short));
    if (!shm->buffer)
        return false;
    audio_position = 0;
    audio_consumed = 0;
    last_paintedtime = effects_paintedtime = paintedtime;
    memset(effects_buffer, 0, sizeof(effects_buffer));
    audio_reports = effects_peak = 0;
    reported_sound_count = 0;
    audio_diagnostics = COM_CheckParm("-diagnostics");
    next_audio_report = 0;
    xenon_sound_init();
    audio_initialized = true;
    audio_running = true;
    audio_threaded = (secondary_alive & (1 << AUDIO_THREAD)) &&
      xenon_run_thread_task(AUDIO_THREAD, audio_stack + sizeof(audio_stack) - 256, Audio_Thread) == 0;
    return true;
}

int
SNDDMA_GetDMAPos(void)
{
    return audio_position;
}

void
SNDDMA_Submit(void)
{
    int i;

    for (i = 0; audio_diagnostics && i < total_channels && reported_sound_count < 128; ++i) {
        channel_t * channel = &channels[i];
        sfxcache_t * cache;
        unsigned j, peak = 0;

        if (!channel->sfx)
            continue;
        for (j = 0; j < reported_sound_count; ++j)
            if (reported_sounds[j] == channel->sfx)
                break;
        if (j < reported_sound_count)
            continue;
        cache = Cache_Check(&channel->sfx->cache);
        if (!cache)
            continue;
        for (j = 0; j < (unsigned) cache->length; ++j) {
            int sample = cache->width == 1 ? ((signed char *) cache->data)[j] * 256 :
              ((short *) cache->data)[j];

            peak = max(peak, (unsigned) abs(sample));
        }
        Sys_Printf("effect %s: length=%d width=%d peak=%u left=%d right=%d pos=%d end=%d painted=%d step=%u\n",
          channel->sfx->name, cache->length, cache->width, peak, channel->leftvol, channel->rightvol,
          channel->pos, channel->end, paintedtime, channel->step);
        reported_sounds[reported_sound_count++] = channel->sfx;
    }
    Audio_Lock();
    memcpy(effects_buffer, sn.buffer, sizeof(effects_buffer));
    effects_paintedtime = paintedtime;
    Audio_Unlock();
    X360_AudioUpdate();
}

void
SNDDMA_Shutdown(void)
{
    audio_running = false;
    if (audio_threaded) {
        while (xenon_is_thread_task_running(AUDIO_THREAD))
            udelay(1000);
        audio_threaded = false;
    }
    music_stop();
    audio_initialized = false;
    free(sn.buffer);
    sn.buffer = NULL;
}

int
music_init(void)
{
    return audio_initialized;
}

void
music_update(void)
{
    X360_AudioUpdate();
}

static void
Music_Close(void)
{
    Audio_Lock();
    if (music_data) {
        mp3dec_ex_close(&decoder);
        free(music_data);
        music_data = NULL;
    }
    music_job_started = 0;
    music_paused      = false;
    decoded_count     = decoded_position = 0;
    music_phase       = 48000;
    Audio_Unlock();
}

int
music_start_play(char * fname, int pos)
{
    FILE * file;
    byte * data = NULL;
    long size;

    (void) pos;

    if (!audio_initialized)
        return 0;

    Music_Close();
    file = fopen(fname, "rb");
    if (!file)
        return 2;
    if (!fseek(file, 0, SEEK_END) && (size = ftell(file)) > 0 && !fseek(file, 0, SEEK_SET)) {
        data = malloc((size_t) size);
        if (data && fread(data, 1, (size_t) size, file) != (size_t) size) {
            free(data);
            data = NULL;
        }
    }
    fclose(file);
    if (!data)
        return 0;

    if (mp3dec_ex_open_buf(&decoder, data, (size_t) size, MP3D_DO_NOT_SCAN) ||
      decoder.info.channels < 1 || decoder.info.channels > 2 ||
      decoder.info.hz <= 0 || decoder.info.hz > 48000) {
        mp3dec_ex_close(&decoder);
        free(data);
        return 0;
    }

    Audio_Lock();
    music_data = data;
    track_loop = music_loop;
    music_job_started = 1;
    Audio_Unlock();
    return 1;
}

void
music_stop(void)
{
    if (!audio_initialized)
        return;

    Music_Close();
}

void music_deinit(void){ music_stop(); }

void
music_pause(void)
{
    if (!audio_initialized)
        return;

    music_paused = true;
}

void
music_resume(void)
{
    if (!audio_initialized)
        return;

    music_paused = false;
}
