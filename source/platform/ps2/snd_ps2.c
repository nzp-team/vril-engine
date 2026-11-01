/*
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
#include "../../nzportable_def.h"
#include <kernel.h>
#include <audsrv.h>
#include <unistd.h>

#define SAMPLE_RATE 22050
#define AUDIO_SAMPLES 8192
#define AUDIO_CHUNK 512

static short audiobuffer[AUDIO_SAMPLES] __attribute__((aligned(64)));
static unsigned char audio_stack[32768] __attribute__((aligned(16)));
static volatile int audio_position;
static volatile qboolean stop_audio;
static int audio_thread_id = -1;
static int audio_done = -1;

extern void music_mix(short *buffer, int count);
extern void *_gp;

static void audio_thread(void *arg)
{
	short output[AUDIO_CHUNK] __attribute__((aligned(64)));
	int i, sent, offset;

	(void)arg;
	while (!stop_audio) {
		for (i = 0; i < AUDIO_CHUNK; i++) {
			output[i] = audiobuffer[audio_position + i];
			audiobuffer[audio_position + i] = 0;
		}
		audio_position = (audio_position + AUDIO_CHUNK) & (AUDIO_SAMPLES - 1);
		music_mix(output, AUDIO_CHUNK);
		offset = 0;
		while (offset < sizeof(output) && !stop_audio) {
			if (audsrv_wait_audio(sizeof(output) - offset) != 0)
				break;
			sent = audsrv_play_audio((char *)output + offset, sizeof(output) - offset);
			if (sent <= 0)
				break;
			offset += sent;
		}
	}
	SignalSema(audio_done);
	ExitThread();
}

qboolean SNDDMA_Init(void)
{
	struct audsrv_fmt_t format;
	ee_thread_t thread;
	ee_sema_t sema;

	snd_initialized = 0;
	if (audsrv_init() != 0)
		return false;
	format.bits = 16;
	format.freq = SAMPLE_RATE;
	format.channels = 1;
	if (audsrv_set_format(&format) != 0) {
		audsrv_quit();
		return false;
	}
	audsrv_set_volume(MAX_VOLUME);
	memset(audiobuffer, 0, sizeof(audiobuffer));
	audio_position = 0;
	stop_audio = false;
	shm = &sn;
	shm->splitbuffer = 0;
	shm->samplebits = 16;
	shm->speed = SAMPLE_RATE;
	shm->channels = 1;
	shm->samples = AUDIO_SAMPLES;
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *)audiobuffer;

	memset(&sema, 0, sizeof(sema));
	sema.max_count = 1;
	audio_done = CreateSema(&sema);
	memset(&thread, 0, sizeof(thread));
	thread.func = audio_thread;
	thread.stack = audio_stack;
	thread.stack_size = sizeof(audio_stack);
	thread.gp_reg = &_gp;
	thread.initial_priority = 0x20;
	audio_thread_id = CreateThread(&thread);
	if (audio_done < 0 || audio_thread_id < 0 || StartThread(audio_thread_id, NULL) < 0) {
		if (audio_thread_id >= 0)
			DeleteThread(audio_thread_id);
		if (audio_done >= 0)
			DeleteSema(audio_done);
		audio_thread_id = audio_done = -1;
		audsrv_quit();
		return false;
	}
	snd_initialized = 1;
	return true;
}

int SNDDMA_GetDMAPos(void)
{
	shm->samplepos = audio_position;
	return shm->samplepos;
}

void SNDDMA_Shutdown(void)
{
	ee_thread_status_t status;

	if (!snd_initialized)
		return;
	stop_audio = true;
	WaitSema(audio_done);
	while (ReferThreadStatus(audio_thread_id, &status) >= 0 && status.status != THS_DORMANT)
		usleep(1000);
	DeleteThread(audio_thread_id);
	DeleteSema(audio_done);
	audio_thread_id = audio_done = -1;
	audsrv_stop_audio();
	audsrv_quit();
	snd_initialized = 0;
}

void SNDDMA_Submit(void)
{
}
