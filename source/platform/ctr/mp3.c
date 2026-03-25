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

#include <3ds.h>
#include <mpg123.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define                 CHANNEL	        1
#define                 MP3_BUFFERS     4

static Thread           mp3_thread;
static volatile bool    mp3_running = false;

uint32_t                rate;
uint8_t			        mp3_channels;
size_t                  buffersize;
static int16_t          *buffers = NULL;

static ndspWaveBuf      waveBuf[MP3_BUFFERS];
static mpg123_handle    *handle = NULL;

int                     mp3_job_started;
int                     mp3_volume;

int mp3_init (void)
{
    int err = mpg123_init();
    if (err != MPG123_OK) {
        Sys_Error("mpg123_init error: %s\n", mpg123_plain_strerror(err));
        return 0;
    }

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    return 1;
}

uint64_t mp3_decode(void *buffer)
{
    size_t ret = 0;
    int err = mpg123_read(handle, buffer, buffersize, &ret);

    if(err == MPG123_DONE) {
        return 0;
    }
        
    if(err != MPG123_OK && err != MPG123_NEW_FORMAT) {
        return 0;
    }

	return ret/(sizeof(int16_t));
}

static void mp3_thread_func(void *arg)
{
    mp3_running = true;

    while(mp3_running) {
        svcSleepThread(500*1000);

        for(int i = 0; i < MP3_BUFFERS; i++) {
            if(waveBuf[i].status == NDSP_WBUF_DONE) {
                int16_t *buf = buffers + (i * (buffersize / sizeof(int16_t)));

                size_t read = mp3_decode(buf);

                if(read == 0) {
                    mp3_running = false;
                    return;
                }

                waveBuf[i].nsamples = read / mp3_channels;
                waveBuf[i].status = NDSP_WBUF_FREE;

                DSP_FlushDataCache(buf, read*sizeof(int16_t));
                ndspChnWaveBufAdd(CHANNEL, &waveBuf[i]);
            }
        }
    }
}

void mp3_stop(void)
{
    mp3_running = false;

    if(mp3_thread) {
        threadJoin(mp3_thread, U64_MAX);
        threadFree(mp3_thread);
        mp3_thread = NULL;
    }

    ndspChnWaveBufClear(CHANNEL);

    if(buffers) {
        linearFree(buffers);
        buffers = NULL;
    }

    ndspChnReset(CHANNEL);
}

int mp3_start_play(char *filename, int startpos)
{
    if(mp3_running) {
        mp3_stop();
    }

    int             encoding = 0;

    handle = mpg123_new(NULL, NULL);
    if (handle == NULL) {
        Sys_Error("Could not set handle for mpg123:\n");
        return 0;
    }

    int err = mpg123_open(handle, filename); 
    if (err != MPG123_OK) return 0;

    err = mpg123_getformat(handle, (long*)&rate, (int*)&mp3_channels, &encoding);
    if (err != MPG123_OK) return 0;

    mpg123_format_none(handle);
	mpg123_format(handle, rate, mp3_channels, MPG123_ENC_SIGNED_16);

    mpg123_seek_frame(handle, startpos, SEEK_SET);
    mpg123_volume(handle, bgmvolume.value);

    buffersize = mpg123_outblock(handle)*64;

    buffers = linearAlloc(buffersize*MP3_BUFFERS);
    memset(buffers, 0, buffersize*MP3_BUFFERS);

    ndspChnReset(CHANNEL);
    ndspChnWaveBufClear(CHANNEL);
    ndspChnSetInterp(CHANNEL, NDSP_INTERP_LINEAR);
    ndspChnSetRate(CHANNEL, rate);
    ndspChnSetFormat(CHANNEL, NDSP_FORMAT_STEREO_PCM16);

    float mix[12] = {0};
    mix[0] = 1.0f; // left -> left
    mix[3] = 1.0f; // right -> right
    ndspChnSetMix(CHANNEL, mix);
    ndspChnWaveBufClear(CHANNEL);

    memset(waveBuf, 0, sizeof(waveBuf));

    for(int i = 0; i < MP3_BUFFERS; i++) {
        int16_t *buf = buffers + (i * (buffersize / sizeof(int16_t)));

        size_t read = mp3_decode(buf);

        waveBuf[i].data_vaddr = buf;
        waveBuf[i].nsamples = read / mp3_channels;
        waveBuf[i].status = NDSP_WBUF_FREE;

        DSP_FlushDataCache(buf, read*sizeof(int16_t));
        ndspChnWaveBufAdd(CHANNEL, &waveBuf[i]);
    }

    DSP_FlushDataCache(buffers, buffersize*MP3_BUFFERS);

    mp3_job_started = 1;

    mp3_thread = threadCreate(mp3_thread_func, NULL, 64*1024, 0x17, -2, false);

    return 1;
}

void mp3_deinit (void)
{
    mpg123_close(handle);
	mpg123_delete(handle);
	mpg123_exit();
}