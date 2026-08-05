/*
Copyright (C) 2017 Felipe Izzo
Copyright (C) 1996-1997 Id Software, Inc.

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

#include <stdio.h>
#include <switch.h>
#include <string.h>
#include <malloc.h>
#include "../../nzportable_def.h"

#define SAMPLE_RATE   	22050
#define NUM_SAMPLES 	2048
#define SAMPLE_SIZE		4
#define DATA_SIZE 		NUM_SAMPLES*SAMPLE_SIZE
#define BUFFER_SIZE 	(DATA_SIZE + 0xfff) & ~0xfff

static int sound_initialized = 0;
static byte *audio_buffer;
static AudioOutBuffer *released_buffer = NULL;
static AudioOutBuffer wave_buf;
static float tick_rate = 0;
static u64 initial_tick = 0;
static Thread audio_thread;

void audio_thread_func(void* unused) {
	while(sound_initialized) {
		audoutPlayBuffer(&wave_buf, &released_buffer);
	}
}

qboolean SNDDMA_Init(void)
{
	sound_initialized = 0;

  	audoutInitialize(); 
	audoutStartAudioOut();

    audio_buffer = memalign(0x1000, BUFFER_SIZE);
	if(!audio_buffer) return false;
	memset(audio_buffer, 0, BUFFER_SIZE);

	memset(&wave_buf, 0, sizeof(wave_buf));
	wave_buf.next = NULL;
	wave_buf.buffer = audio_buffer;
	wave_buf.buffer_size = BUFFER_SIZE;
	wave_buf.data_size = DATA_SIZE;

	shm = &sn;
	shm->splitbuffer = 0;
	shm->samplebits = 16;
	shm->speed = audoutGetSampleRate();
	shm->channels = 2;
	shm->samples = DATA_SIZE / 2;
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = audio_buffer;
	
	initial_tick = armGetSystemTick();
	sound_initialized = 1;
	threadCreate(&audio_thread, audio_thread_func, NULL, NULL, 128*1024, 0x2C, -2);
	threadStart(&audio_thread);
	tick_rate = 1.0f / armGetSystemTickFreq();

	return true;
}

int SNDDMA_GetDMAPos(void)
{
	if (!snd_initialized)
		return 0;

	u64 tick = armGetSystemTick();
	const unsigned int delta_tick  = tick - initial_tick;
	const double deltaSecond = (double)delta_tick * (double)tick_rate;
	u64 samplepos = deltaSecond * audoutGetSampleRate();
	shm->samplepos = samplepos;
	return samplepos;
}

void SNDDMA_Shutdown(void)
{
	if(!sound_initialized)
		return;

	audoutStopAudioOut();
	audoutExit();
	free(audio_buffer);

	sound_initialized = 0;
}

void SNDDMA_Submit(void)
{
}