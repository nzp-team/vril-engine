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

static volatile bool    mp3_running = false;

static Thread           mp3_thread;

uint32_t                rate;
uint8_t			        mp3_channels;
size_t                  buffersize;
static int16_t          *buffers = NULL;

static ndspWaveBuf      *waveBuf = NULL;
static mpg123_handle    *handle = NULL;

volatile int            mp3_job_started;
int                     mp3_volume;

int mp3_init (void)
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

static void mp3_thread_func(void *arg)
{
    while(mp3_running) {
        bool queued = false;

        DSP_InvalidateDataCache(waveBuf, sizeof(ndspWaveBuf)*MP3_BUFFERS);

        for(int i = 0; i < MP3_BUFFERS; i++) {
            if(waveBuf[i].status == NDSP_WBUF_DONE) {
                int16_t *buf = buffers + (i*(buffersize/sizeof(int16_t)));

                size_t read = mp3_decode(buf);

                if(read == 0) {
                    mp3_job_started = 0;
                    mp3_running = false;
                    return;
                }

                waveBuf[i].nsamples = read/mp3_channels;
                //waveBuf[i].status = NDSP_WBUF_FREE;

                DSP_FlushDataCache(buf, read*sizeof(int16_t));
                ndspChnWaveBufAdd(CHANNEL, &waveBuf[i]);
                queued = true;
            }
        }

        if(queued) {
            svcSleepThread(1000*1000);
        } else {
            svcSleepThread(8*1000*1000);
        }
    }
}

void mp3_stop(void)
{
    mp3_running = false;
    mp3_job_started = 0;

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

    if(waveBuf) {
        linearFree(waveBuf);
        waveBuf = NULL;
    }

    ndspChnReset(CHANNEL);
}

int mp3_start_play(char *filename, int startpos)
{
    if(mp3_running) {
        mp3_stop();
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

    err = mpg123_getformat(handle, (long*)&rate, (int*)&mp3_channels, &encoding);
    if (err != MPG123_OK) return 0;

    mpg123_format_none(handle);
	mpg123_format(handle, rate, mp3_channels, MPG123_ENC_SIGNED_16);

    mpg123_seek_frame(handle, startpos, SEEK_SET);
    mpg123_volume(handle, bgmvolume.value);

    // temporarily stop all DMA audio
    // to prevent buffer looping
    S_StopAllSounds (true);

    // allocate and clear buffers
    // slightly larger buffersize to allow
    // for less cpu load (bigger blocks)
    buffersize = mpg123_outblock(handle)*24;
    buffers = linearAlloc(buffersize*MP3_BUFFERS);
    if(!buffers) {
        Sys_Error("Failed to allocate MP3 buffers\n");
        return 0;
    }
    memset(buffers, 0, buffersize*MP3_BUFFERS);

    ndspChnReset(CHANNEL);
    ndspChnWaveBufClear(CHANNEL);
    ndspChnSetInterp(CHANNEL, NDSP_INTERP_LINEAR);
    ndspChnSetRate(CHANNEL, rate);
    // decide mono or stereo based on input file
    ndspChnSetFormat(CHANNEL, mp3_channels == 1 ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16);

    float mix[12] = {0};
    // set l/r mix
    for (int i = 0; i < 4; i++) {
        mix[i] = 1.0f;
    }
    ndspChnSetMix(CHANNEL, mix);
    ndspChnWaveBufClear(CHANNEL);

    waveBuf = (ndspWaveBuf*)linearAlloc(sizeof(ndspWaveBuf)*MP3_BUFFERS);
    if(!waveBuf) {
        Sys_Error("Failed to allocate MP3 waveBufs\n");
        return 0;
    }
    memset(waveBuf, 0, sizeof(ndspWaveBuf)*MP3_BUFFERS);

    for(int i = 0; i < MP3_BUFFERS; i++) {
        int16_t *buf = buffers + (i * (buffersize / sizeof(int16_t)));

        size_t read = mp3_decode(buf);
        if(read == 0) break;

        waveBuf[i].data_vaddr = buf;
        waveBuf[i].nsamples = read / mp3_channels;
        DSP_FlushDataCache(buf, read * sizeof(int16_t));
        ndspChnWaveBufAdd(CHANNEL, &waveBuf[i]);
    }

    mp3_job_started = 1;
    mp3_running = true;
    mp3_thread = threadCreate(mp3_thread_func, NULL, 128*1024, 0x2C, -2, false);

    return 1;
}

void mp3_deinit (void)
{
    mp3_stop();
    mpg123_close(handle);
	mpg123_delete(handle);
    handle = NULL;
	mpg123_exit();
}