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
#include <audio/audio.h>
#include <sysmodule/sysmodule.h>
#include <sys/thread.h>
#include <sys/mutex.h>
#include <sys/systime.h>
#include <mpg123.h>

#define BLOCK_FRAMES 256
#define RING_SAMPLES 32768

static sys_ppu_thread_t thread;
static sys_mutex_t music_lock;
static int port = -1;
static audioPortConfig config;
static sys_event_queue_t queue;
static sys_ipc_key_t queue_key;
static int audio_running;
static unsigned int audio_position;

static int16_t * audio_ring;

static mpg123_handle * decoder;
static qboolean music_ready;
volatile int music_job_started;
int music_volume;
qboolean music_paused;
extern int music_loop;

static void
AudioThread(void * unused)
{
    int16_t output[BLOCK_FRAMES * 2], music[BLOCK_FRAMES * 2];

    (void) unused;

    while (__atomic_load_n(&audio_running, __ATOMIC_ACQUIRE)) {
        sys_event_t event;

        if (sysEventQueueReceive(queue, &event, 100000) != 0)
            continue;

        unsigned int cursor = __atomic_load_n(&audio_position, __ATOMIC_RELAXED);
        size_t decoded = 0, i;

        memcpy(output, audio_ring + cursor, sizeof(output));
        memset(music, 0, sizeof(music));

        sysMutexLock(music_lock, 0);

        if (decoder && music_job_started && !music_paused) {
            int result = mpg123_read(decoder, (unsigned char *) music, sizeof(music), &decoded);

            if (result == MPG123_DONE) {
                if (music_loop)
                    mpg123_seek(decoder, 0, SEEK_SET);
                else
                    music_job_started = 0;
            } else if (result != MPG123_OK && result != MPG123_NEW_FORMAT) {
                music_job_started = 0;
            }
        }

        for (i = 0; i < decoded / sizeof(int16_t); ++i) {
            int sample = output[i] + (int) music[i] * music_volume / PLATFORM_VOLUME_MAX;

            output[i] = sample < -32768 ? -32768 : sample > 32767 ? 32767 : sample;
        }

        sysMutexUnlock(music_lock);

        u64 read_index = *(volatile u64 *) (uintptr_t) config.readIndex;
        unsigned block = (read_index + 1) % config.numBlocks;
        float * dest   = (float *) (uintptr_t) config.audioDataStart + block * BLOCK_FRAMES * 2;

        for (i = 0; i < BLOCK_FRAMES * 2; ++i)
            dest[i] = output[i] / 32768.0f;

        __atomic_thread_fence(__ATOMIC_SEQ_CST);

        __atomic_store_n(&audio_position, (cursor + BLOCK_FRAMES * 2) % RING_SAMPLES,
          __ATOMIC_RELEASE);
    }

    sysThreadExit(0);
}

qboolean
SNDDMA_Init(void)
{
    audioPortParam params = { AUDIO_PORT_2CH, AUDIO_BLOCK_8, AUDIO_PORT_INITLEVEL, 1.0f };
    u32 number;

    if (sysModuleIsLoaded(SYSMODULE_AUDIO) != 0 && sysModuleLoad(SYSMODULE_AUDIO) != 0)
        return false;

    if (audioInit() != 0)
        return false;

    if (audioPortOpen(&params, &number) != 0) {
        audioQuit();
        return false;
    }

    port = number;

    if (audioGetPortConfig(port, &config) != 0 ||
      audioCreateNotifyEventQueue(&queue, &queue_key) != 0)
    {
        audioPortClose(port);
        audioQuit();
        port = -1;
        return false;
    }

    if (audioSetNotifyEventQueue(queue_key) != 0) {
        sysEventQueueDestroy(queue, 0);
        audioPortClose(port);
        audioQuit();
        port = -1;
        return false;
    }

    memset((void *) (uintptr_t) config.audioDataStart, 0, config.portSize);

    shm = &sn;
    shm->splitbuffer      = 0;
    shm->samplebits       = 16;
    shm->speed            = 48000;
    shm->channels         = 2;
    shm->samples          = RING_SAMPLES;
    shm->samplepos        = 0;
    shm->submission_chunk = BLOCK_FRAMES * 2;
    audio_ring            = calloc(RING_SAMPLES, sizeof(int16_t));
    sn.buffer = (unsigned char *) audio_ring;

    if (!audio_ring)
        goto fail;

    sys_mutex_attr_t attr;
    sysMutexAttrInitialize(attr);

    if (sysMutexCreate(&music_lock, &attr) != 0)
        goto fail;

    __atomic_store_n(&audio_position, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&audio_running, 1, __ATOMIC_RELEASE);

    if (sysThreadCreate(&thread, AudioThread, NULL, 900, 128 * 1024, THREAD_JOINABLE,
      "nzp audio") != 0)
    {
        __atomic_store_n(&audio_running, 0, __ATOMIC_RELEASE);
        sysMutexDestroy(music_lock);
        goto fail;
    }

    if (audioPortStart(port) != 0) {
        __atomic_store_n(&audio_running, 0, __ATOMIC_RELEASE);
        u64 status;
        sysThreadJoin(thread, &status);
        sysMutexDestroy(music_lock);
        goto fail;
    }

    return true;

fail:
    free(audio_ring);
    audio_ring = NULL;
    sn.buffer  = NULL;
    audioRemoveNotifyEventQueue(queue_key);
    sysEventQueueDestroy(queue, 0);
    audioPortClose(port);
    audioQuit();
    port = -1;
    Sys_Error("PS3: audio buffer or thread initialization failed");

    return false;
}

int
SNDDMA_GetDMAPos(void)
{
    return __atomic_load_n(&audio_position, __ATOMIC_ACQUIRE);
}

void
SNDDMA_Submit(void)
{
}

void
SNDDMA_Shutdown(void)
{
    if (port < 0)
        return;

    __atomic_store_n(&audio_running, 0, __ATOMIC_RELEASE);
    u64 status;
    sysThreadJoin(thread, &status);
    audioPortStop(port);
    music_stop();
    sysMutexDestroy(music_lock);
    audioRemoveNotifyEventQueue(queue_key);
    sysEventQueueDestroy(queue, 0);
    audioPortClose(port);
    audioQuit();
    port = -1;

    free(audio_ring);
    audio_ring = NULL;
    sn.buffer  = NULL;
    Con_SafePrintf("PS3: audio worker stopped\n");
}

int
music_init(void)
{
    music_ready = port >= 0 && mpg123_init() == MPG123_OK;

    return music_ready;
}

void
music_stop(void)
{
    if (port < 0)
        return;

    sysMutexLock(music_lock, 0);

    if (decoder) {
        mpg123_close(decoder);
        mpg123_delete(decoder);
        decoder = NULL;
    }

    music_job_started = 0;
    music_paused      = false;
    sysMutexUnlock(music_lock);
}

int
music_start_play(char * filename, int position)
{
    int result = 2;

    music_stop();

    if (!music_ready)
        return 2;

    sysMutexLock(music_lock, 0);
    decoder = mpg123_new(NULL, NULL);

    if (decoder) {
        mpg123_param(decoder, MPG123_FORCE_RATE, 48000, 0);
        mpg123_param(decoder, MPG123_ADD_FLAGS, MPG123_FORCE_STEREO, 0);
        mpg123_format_none(decoder);
        mpg123_format(decoder, 48000, MPG123_STEREO, MPG123_ENC_SIGNED_16);

        if (mpg123_open(decoder, filename) == MPG123_OK) {
            if (position > 0)
                mpg123_seek(decoder, position, SEEK_SET);

            music_job_started = 1;
            result = 1;
        } else {
            mpg123_delete(decoder);
            decoder = NULL;
        }
    }

    sysMutexUnlock(music_lock);

    return result;
}

void
music_pause(void)
{
    if (port >= 0) {
        sysMutexLock(music_lock, 0);
        music_paused = true;
        sysMutexUnlock(music_lock);
    }
}

void
music_resume(void)
{
    if (port >= 0) {
        sysMutexLock(music_lock, 0);
        music_paused = false;
        sysMutexUnlock(music_lock);
    }
}

void
music_deinit(void)
{
    music_stop();

    if (music_ready)
        mpg123_exit();

    music_ready = false;
}

void
music_update(void)
{
}
