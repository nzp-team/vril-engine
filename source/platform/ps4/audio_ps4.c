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
#include <orbis/AudioOut.h>
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>
#include <mpg123.h>
#include <pthread.h>
#include "ps4_local.h"

#define BLOCK_FRAMES 256
#define RING_SAMPLES 32768
static pthread_t thread;
static pthread_mutex_t music_lock;
static int port = -1;
static int audio_running;
static unsigned int audio_position;
static mpg123_handle * decoder;
static qboolean music_ready;
volatile int music_job_started;
int music_volume;
qboolean music_paused;
extern int music_loop;

static void *
AudioThread(void * unused)
{
    int16_t output[BLOCK_FRAMES * 2], music[BLOCK_FRAMES * 2];

    (void) unused;

    while (__atomic_load_n(&audio_running, __ATOMIC_ACQUIRE)) {
        unsigned int cursor = __atomic_load_n(&audio_position, __ATOMIC_RELAXED);
        size_t decoded = 0, i;

        memcpy(output, shm->buffer + cursor * sizeof(int16_t), sizeof(output));
        memset(music, 0, sizeof(music));

        pthread_mutex_lock(&music_lock);

        if (decoder && music_job_started && !music_paused) {
            int result = mpg123_read(decoder, music, sizeof(music), &decoded);

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

        pthread_mutex_unlock(&music_lock);

        if (sceAudioOutOutput(port, output) < 0)
            break;  // Blocking output paces the thread.

        __atomic_store_n(&audio_position, (cursor + BLOCK_FRAMES * 2) % RING_SAMPLES, __ATOMIC_RELEASE);
    }

    return NULL;
}

qboolean
SNDDMA_Init(void)
{
    if (PS4_LoadModule(ORBIS_SYSMODULE_INTERNAL_AUDIOOUT) < 0)
        Sys_Error("PS4: audio module could not be loaded");

    int result = sceAudioOutInit();

    if (result < 0)
        Sys_Error("PS4: audio initialization failed (%08x)", result);

    port = sceAudioOutOpen(0xff, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
        BLOCK_FRAMES, 48000, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);

    if (port < 0)
        Sys_Error("PS4: audio output could not open (%08x)", port);

    shm = &sn;
    shm->splitbuffer      = 0;
    shm->samplebits       = 16;
    shm->speed            = 48000;
    shm->channels         = 2;
    shm->samples          = RING_SAMPLES;
    shm->samplepos        = 0;
    shm->submission_chunk = BLOCK_FRAMES * 2;
    shm->buffer           = calloc(RING_SAMPLES, sizeof(int16_t));

    if (!shm->buffer)
        goto fail;

    if (pthread_mutex_init(&music_lock, NULL) != 0)
        goto fail;

    __atomic_store_n(&audio_position, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&audio_running, 1, __ATOMIC_RELEASE);

    if (pthread_create(&thread, NULL, AudioThread, NULL) != 0) {
        __atomic_store_n(&audio_running, 0, __ATOMIC_RELEASE);
        pthread_mutex_destroy(&music_lock);
        goto fail;
    }

    return true;

fail:
    free(shm->buffer);
    shm->buffer = NULL;
    sceAudioOutClose(port);
    port = -1;
    Sys_Error("PS4: audio buffer or thread initialization failed");

    return false;
} /* SNDDMA_Init */

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
    pthread_join(thread, NULL);
    music_stop();
    pthread_mutex_destroy(&music_lock);
    sceAudioOutClose(port);
    port = -1;

    free(shm->buffer);
    shm->buffer = NULL;
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

    pthread_mutex_lock(&music_lock);

    if (decoder) {
        mpg123_close(decoder);
        mpg123_delete(decoder);
        decoder = NULL;
    }

    music_job_started = 0;
    music_paused      = false;
    pthread_mutex_unlock(&music_lock);
}

int
music_start_play(char * filename, int position)
{
    int result = 2;

    music_stop();

    if (!music_ready)
        return 2;

    pthread_mutex_lock(&music_lock);
    decoder = mpg123_new(NULL, NULL);

    if (decoder) {
        // Force the mixer's format; mpg123 resamples source tracks to 48 kHz.
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

    pthread_mutex_unlock(&music_lock);

    return result;
}

void
music_pause(void)
{
    if (port >= 0) {
        pthread_mutex_lock(&music_lock);
        music_paused = true;
        pthread_mutex_unlock(&music_lock);
    }
}

void
music_resume(void)
{
    if (port >= 0) {
        pthread_mutex_lock(&music_lock);
        music_paused = false;
        pthread_mutex_unlock(&music_lock);
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
