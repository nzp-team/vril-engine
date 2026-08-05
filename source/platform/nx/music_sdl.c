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

#include <switch.h>
#include <mpg123.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define                 CHANNEL	        1
#define                 MP3_BUFFERS     4

static volatile bool    music_running = false;
qboolean                music_paused = false;

static Thread           music_thread;

uint32_t                rate;
uint8_t			        music_channels;
size_t                  buffersize;
static int16_t          *buffers[4];

static AudioOutBuffer      *waveBuf;
static AudioOutBuffer      *waveBufReleased = NULL;
static mpg123_handle    *handle = NULL;

volatile int            music_job_started;
int                     music_volume;

#include <malloc.h>
#define ALIGN(x) (((x) + 0xfff) & ~0xfff)
#define linearAlloc(x) memalign(0x1000, ALIGN(x))
#define linearFree free

int music_init (void)
{
    Con_Printf("MP3: music_init!\n");
    int err = mpg123_init();
    if (err != MPG123_OK) {
        Sys_Error("mpg123_init error: %s\n", mpg123_plain_strerror(err));
        return 0;
    }

    return 1;
}

uint64_t mp3_decode(void *buffer)
{
    Con_Printf("MP3: mp3_decode!\n");
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

	return ret;
}

static void music_thread_func(void *arg)
{
    Con_Printf("MP3: music_thread_func!\n");
    while(music_running) {
        Con_Printf("MP3: music is running!\n");
        if (music_paused) {
            svcSleepThread(500000);
            continue;
        }

        for(int i = 0; i < MP3_BUFFERS; i++) {
            int16_t *buf = buffers[i];

            size_t read = mp3_decode(buf);

            if(read == 0) {
                music_job_started = 0;
                music_running = false;
                Con_Printf("MP3: No Audio To Read!\n");
                return;
            } else {
                Con_Printf("MP3: Yes Audio To Read!\n");
            }

            waveBuf[i].buffer = buf;
            waveBuf[i].data_size = read;
            waveBuf[i].buffer_size = ALIGN(read);

            audoutPlayBuffer(&waveBuf[i], &waveBufReleased);
        }
    }
    threadExit();
}

void music_pause(void)
{
    Con_Printf("MP3: music_pause!\n");
    if (!music_running) return;
    music_paused = true;
}

void music_resume(void)
{
    Con_Printf("MP3: music_resume!\n");
    if (!music_running) return;
    music_paused = false;
}

void music_stop(void)
{
    Con_Printf("MP3: music_stop!\n");
    music_running = false;
    music_job_started = 0;


    for(int i = 0; i < 4; i++) {
        if(buffers[i]) {
            linearFree(buffers[i]);
            buffers[i] = NULL;
        }
    }

    if(waveBuf) {
        linearFree(waveBuf);
        waveBuf = NULL;
    }
}

int music_start_play(char *filename, int startpos)
{
    Con_Printf("MP3: music_start_play!\n");
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
        Sys_Error("Could not set handle for mpg123\n");
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

    // allocate and clear buffers
    // slightly larger buffersize to allow
    // for less cpu load (bigger blocks)
    buffersize = mpg123_outblock(handle)*24;
    for(int i = 0; i < 4; i++) 
    {
        buffers[i] = linearAlloc(buffersize);
        if(!buffers[i]) {
            Sys_Error("Failed to allocate MP3 buffers\n");
            return 0;
        }
        memset(buffers[i], 0, buffersize);
    }

    waveBuf = (AudioOutBuffer*)malloc(sizeof(AudioOutBuffer)*MP3_BUFFERS);
    if(!waveBuf) {
        Sys_Error("Failed to allocate MP3 waveBufs\n");
        return 0;
    }
    memset(waveBuf, 0, sizeof(AudioOutBuffer)*MP3_BUFFERS);

    for(int i = 0; i < MP3_BUFFERS; i++) {
        int16_t *buf = buffers[i];

        size_t read = mp3_decode(buf);
        if(read == 0) {
            Con_Printf("MP3: No Audio To Read!\n");
            break;
        } else {
            Con_Printf("MP3: Yes Audio To Read!\n");
        }

        waveBuf[i].next = NULL;
        waveBuf[i].buffer = buf;
        waveBuf[i].buffer_size = ALIGN(read);
        waveBuf[i].data_size = read;
    }

    music_job_started = 1;
    music_running = true;

    Con_Printf("MP3: starting music_thread!\n");
    threadCreate(&music_thread, music_thread_func, NULL, NULL, 128*1024, 0x2C, -2);
    threadStart(&music_thread);

    return 1;
}

void music_deinit (void)
{
    Con_Printf("MP3: music_deinit!\n");
    music_stop();
    mpg123_close(handle);
	mpg123_delete(handle);
    handle = NULL;
	mpg123_exit();
}