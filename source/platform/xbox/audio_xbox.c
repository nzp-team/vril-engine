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
#include <windows.h>
#include <hal/audio.h>
#include <limits.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#define MINIMP3_NO_STDIO

#include <minimp3_ex.h>

#define BLOCK_FRAMES 1024
#define BUFFER_COUNT 3
#define RING_SAMPLES 32768

static HANDLE audio_thread, audio_semaphore;
static CRITICAL_SECTION music_lock;
static short * audio_buffers[BUFFER_COUNT];
static unsigned int audio_position;
static int audio_running;
static qboolean audio_initialized;
static mp3dec_ex_t decoder;
static FILE * music_file;
static mp3dec_io_t music_io;
static short decoded[MINIMP3_MAX_SAMPLES_PER_FRAME], music_sample[2];
static size_t decoded_count, decoded_position;
static unsigned int music_phase;
volatile int music_job_started;
int music_volume;
qboolean music_paused;
extern int music_loop;

static size_t
Music_Read(void * buffer, size_t size, void * file)
{
    return fread(buffer, 1, size, file);
}

static int
Music_Seek(uint64_t position, void * file)
{
    return position > LONG_MAX ? -1 : fseek(file, (long) position, SEEK_SET);
}

static qboolean
Music_NextSample(void)
{
    int channel;

    if (decoded_position >= decoded_count) {
        decoded_count    = mp3dec_ex_read(&decoder, decoded, MINIMP3_MAX_SAMPLES_PER_FRAME);
        decoded_position = 0;
        if (!decoded_count && music_loop && !decoder.last_error && !mp3dec_ex_seek(&decoder, 0))
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

    if (!music_file || !music_job_started || music_paused)
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
AudioCallback(void * device, void * data)
{
    (void) device;
    (void) data;
    // The AC97 callback runs at DPC level. Decoding and mixing run on the worker.
    ReleaseSemaphore(audio_semaphore, 1, NULL);
}

static DWORD WINAPI
AudioThread(void * unused)
{
    int next_buffer = 0;
    short mixed[BLOCK_FRAMES * 2];

    (void) unused;

    while (__atomic_load_n(&audio_running, __ATOMIC_ACQUIRE)) {
        unsigned int position;
        short * output;

        WaitForSingleObject(audio_semaphore, INFINITE);
        if (!__atomic_load_n(&audio_running, __ATOMIC_ACQUIRE))
            break;
        position = __atomic_load_n(&audio_position, __ATOMIC_RELAXED);
        output   = audio_buffers[next_buffer];
        memcpy(mixed, sn.buffer + position * sizeof(short), sizeof(mixed));
        EnterCriticalSection(&music_lock);
        Music_Mix(mixed);
        LeaveCriticalSection(&music_lock);
        // DMA buffers are write-combined: never read them back while mixing.
        memcpy(output, mixed, sizeof(mixed));
        __asm__ __volatile__ ("sfence" ::: "memory");
        XAudioProvideSamples((unsigned char *) output, BLOCK_FRAMES * 2 * sizeof(short), false);
        next_buffer = (next_buffer + 1) % BUFFER_COUNT;
        __atomic_store_n(&audio_position, (position + BLOCK_FRAMES * 2) % RING_SAMPLES, __ATOMIC_RELEASE);
    }
    return 0;
}

qboolean
SNDDMA_Init(void)
{
    int i;

    shm = &sn;
    shm->splitbuffer      = 0;
    shm->samplebits       = 16;
    shm->speed            = 48000;
    shm->channels         = 2;
    shm->samples          = RING_SAMPLES;
    shm->samplepos        = 0;
    shm->submission_chunk = 1;
    shm->buffer           = calloc(RING_SAMPLES, sizeof(short));
    audio_semaphore       = CreateSemaphore(NULL, 0, BUFFER_COUNT, NULL);
    for (i = 0; i < BUFFER_COUNT; ++i)
        audio_buffers[i] = MmAllocateContiguousMemoryEx(BLOCK_FRAMES * 2 * sizeof(short),
            0, 0x03ffffff, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
    if (!shm->buffer || !audio_semaphore || !audio_buffers[0] || !audio_buffers[1] || !audio_buffers[2])
        goto failed;
    InitializeCriticalSection(&music_lock);
    __atomic_store_n(&audio_running, 1, __ATOMIC_RELEASE);
    audio_thread = CreateThread(NULL, 65536, AudioThread, NULL, 0, NULL);
    if (!audio_thread) {
        __atomic_store_n(&audio_running, 0, __ATOMIC_RELEASE);
        DeleteCriticalSection(&music_lock);
        goto failed;
    }
    XAudioInit(16, 2, AudioCallback, NULL);
    for (i = 1; i < BUFFER_COUNT; ++i) {
        memset(audio_buffers[i], 0, BLOCK_FRAMES * 2 * sizeof(short));
        XAudioProvideSamples((unsigned char *) audio_buffers[i], BLOCK_FRAMES * 2 * sizeof(short), false);
    }
    audio_initialized = true;
    XAudioPlay();
    return true;

failed:
    for (i = 0; i < BUFFER_COUNT; ++i) {
        if (audio_buffers[i])
            MmFreeContiguousMemory(audio_buffers[i]);
        audio_buffers[i] = NULL;
    }
    if (audio_semaphore)
        CloseHandle(audio_semaphore);
    audio_semaphore = NULL;
    free(shm->buffer);
    shm->buffer = NULL;
    return false;
} /* SNDDMA_Init */

int SNDDMA_GetDMAPos(void){ return (int) __atomic_load_n(&audio_position, __ATOMIC_ACQUIRE); }

void SNDDMA_Submit(void){ }

void
SNDDMA_Shutdown(void)
{
    int i;

    if (!audio_initialized)
        return;

    __atomic_store_n(&audio_running, 0, __ATOMIC_RELEASE);
    ReleaseSemaphore(audio_semaphore, 1, NULL);
    WaitForSingleObject(audio_thread, INFINITE);
    // Disable callbacks before freeing the semaphore and DMA buffers.
    XAudioPause();
    XAudioInit(16, 2, NULL, NULL);
    CloseHandle(audio_thread);
    CloseHandle(audio_semaphore);
    for (i = 0; i < BUFFER_COUNT; ++i) {
        MmFreeContiguousMemory(audio_buffers[i]);
        audio_buffers[i] = NULL;
    }
    DeleteCriticalSection(&music_lock);
    // S_Shutdown clears the shared shm pointer before calling the platform hook.
    free(sn.buffer);
    sn.buffer         = NULL;
    audio_initialized = false;
}

int music_init(void){ return audio_initialized; }

void music_update(void){ }

static void
Music_Close(void)
{
    if (music_file) {
        mp3dec_ex_close(&decoder);
        fclose(music_file);
        music_file = NULL;
    }
    music_job_started = 0;
    music_paused      = false;
    decoded_count     = decoded_position = 0;
    music_phase       = 48000;
}

int
music_start_play(char * fname, int pos)
{
    int result = 2;

    (void) pos;

    if (!audio_initialized)
        return 0;

    EnterCriticalSection(&music_lock);
    Music_Close();
    music_file = fopen(fname, "rb");
    if (music_file) {
        music_io = (mp3dec_io_t){ Music_Read, music_file, Music_Seek, music_file };
        if (mp3dec_ex_open_cb(&decoder, &music_io, MP3D_DO_NOT_SCAN) ||
          decoder.info.channels < 1 || decoder.info.channels > 2 ||
          decoder.info.hz <= 0 || decoder.info.hz > 48000)
        {
            Music_Close();
            result = 0;
        } else {
            music_job_started = 1;
            result = 1;
        }
    }
    LeaveCriticalSection(&music_lock);
    return result;
}

void
music_stop(void)
{
    if (!audio_initialized)
        return;

    EnterCriticalSection(&music_lock);
    Music_Close();
    LeaveCriticalSection(&music_lock);
}

void music_deinit(void){ music_stop(); }

void
music_pause(void)
{
    if (!audio_initialized)
        return;

    EnterCriticalSection(&music_lock);
    music_paused = true;
    LeaveCriticalSection(&music_lock);
}

void
music_resume(void)
{
    if (!audio_initialized)
        return;

    EnterCriticalSection(&music_lock);
    music_paused = false;
    LeaveCriticalSection(&music_lock);
}
