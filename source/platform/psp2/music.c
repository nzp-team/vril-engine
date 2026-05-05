/*
Copyright (C) NZ:P Team.

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
#include "../../nzportable_def.h"

#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>
#include <mpg123.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define                 CHANNEL	        1

static volatile bool    music_running = false;
qboolean                music_paused = false;

static SceUID           music_thread;

uint32_t                rate;
uint8_t			        music_channels;
size_t                  buffersize;
static void             *decode_buf = NULL;

static mpg123_handle    *handle = NULL;

volatile int            music_job_started;
int                     music_volume;

static int              audio_port = -1;

int music_init (void)
{
    int err = mpg123_init();
    if (err != MPG123_OK) {
        Sys_Error("mpg123_init error: %s\n", mpg123_plain_strerror(err));
        return 0;
    }

    return 1;
}

uint64_t mp3_decode(void *buffer)
{
    size_t ret = 0;
    int err = mpg123_read(handle, buffer, buffersize, &ret);

    if(err == MPG123_DONE) {
        // eof
        return 0;
    }
        
    if(err != MPG123_OK && err != MPG123_NEW_FORMAT) {
        Sys_Error("MP3: decode error %d: %s\n", err, mpg123_plain_strerror(err));
        return 0;
    }

	return ret/(sizeof(int16_t));
}

static int music_thread_func(SceSize args, void *argp)
{
    while(music_running) {
        if (music_paused) {
            memset(decode_buf, 0, buffersize);
            sceAudioOutOutput(audio_port, decode_buf);
            continue;
        }

        size_t read = mp3_decode(decode_buf);

        if(read == 0) {
            music_job_started = 0;
            music_running = false;
            break;
        }

        sceAudioOutSetVolume(audio_port, SCE_AUDIO_VOLUME_FLAG_L_CH, &music_volume);
        sceAudioOutSetVolume(audio_port, SCE_AUDIO_VOLUME_FLAG_R_CH, &music_volume);
        sceAudioOutOutput(audio_port, decode_buf);
    }

    sceKernelExitDeleteThread(0);
    return 0;
}

void music_pause(void)
{
    if (!music_running) return;
    music_paused = true;
    int vol = 0;
    sceAudioOutSetVolume(audio_port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, &vol);
}

void music_resume(void)
{
    if (!music_running) return;
    music_paused = false;
    sceAudioOutSetVolume(audio_port, SCE_AUDIO_VOLUME_FLAG_L_CH, &music_volume);
    sceAudioOutSetVolume(audio_port, SCE_AUDIO_VOLUME_FLAG_R_CH, &music_volume);
}

void music_stop(void)
{
    music_running = false;
    music_job_started = 0;

    if(music_thread) {
        sceKernelWaitThreadEnd(music_thread, NULL, NULL);
        sceKernelDeleteThread(music_thread);
        music_thread = 0;
    }

    sceAudioOutReleasePort(audio_port);
    audio_port = -1;

    if(decode_buf) {
        free(decode_buf);
        decode_buf = NULL;
    }
}

int music_start_play(char *filename, int startpos)
{
    if(music_running) {
        music_stop();
    }

    int encoding = 0;

    if(handle) {
        mpg123_close(handle);
        mpg123_delete(handle);
        handle = NULL;
    }

    handle = mpg123_new(NULL, NULL);
    if (handle == NULL) {
        Sys_Error("Could not set handle for mpg123:\n");
        return 0;
    }

    int err = mpg123_open(handle, filename); 
    if (err != MPG123_OK) return 0;

    err = mpg123_getformat(handle, (long*)&rate, (int*)&music_channels, &encoding);
    if (err != MPG123_OK) return 0;

    mpg123_format_none(handle);
	mpg123_format(handle, rate, music_channels, MPG123_ENC_SIGNED_16);

    mpg123_seek_frame(handle, startpos, SEEK_SET);
    mpg123_volume(handle, bgmvolume.value);

    // temporarily stop all DMA audio
    // to prevent buffer looping
    S_StopAllSounds (true);

    int samples_per_buf = 256;

    audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, samples_per_buf, rate, music_channels == 1 ? SCE_AUDIO_OUT_MODE_MONO : SCE_AUDIO_OUT_MODE_STEREO);
    if(audio_port < 0) {
        Sys_Error("Failed to open audio port: %d\n", audio_port);
        return 0;
    }

    int vol[2] = {SCE_AUDIO_VOLUME_0DB, SCE_AUDIO_VOLUME_0DB};
    sceAudioOutSetVolume(audio_port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vol);

    // single decode buffer is enough since output is blocking
    buffersize = samples_per_buf*music_channels*sizeof(int16_t);
    decode_buf = malloc(buffersize);
    if(!decode_buf) {
        Sys_Error("Failed to allocate MP3 decode buffer\n");
        return 0;
    }

    music_job_started = 1;
    music_running = true;
    music_thread = sceKernelCreateThread("music_thread", music_thread_func, 0x10000100, 0x10000, 0, 0, NULL);
    sceKernelStartThread(music_thread, 0, NULL);

    return 1;
}

void music_deinit (void)
{
    music_stop();
    mpg123_close(handle);
	mpg123_delete(handle);
    handle = NULL;
	mpg123_exit();
}