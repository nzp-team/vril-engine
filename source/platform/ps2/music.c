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
#include <kernel.h>
#include <mpg123.h>
#include <unistd.h>

#define MUSIC_SAMPLES 8192
#define MUSIC_CHUNK 512
#define MUSIC_PRIORITY 0x40
#define MUSIC_PRIORITY_URGENT 0x30
#define MUSIC_MIN_BUFFER (MUSIC_CHUNK * 4)

static mpg123_handle *handle;
static FILE *music_file;
static char music_file_buffer[32768];
static short music_buffer[MUSIC_SAMPLES];
static unsigned char music_stack[32768] __attribute__((aligned(16)));
static volatile unsigned int music_readpos, music_writepos;
static volatile qboolean music_eof, music_quit;
static int music_lock = -1;
static int music_ready = -1;
static int music_done = -1;
static int music_thread_id = -1;
static volatile int music_priority = MUSIC_PRIORITY;
static qboolean music_initialized;
qboolean music_paused;
volatile int music_job_started;
int music_volume;

extern void *_gp;

static void music_wake(void)
{
	if (!music_quit && music_job_started && !music_paused && !music_eof &&
		music_writepos - music_readpos < MUSIC_MIN_BUFFER && music_priority != MUSIC_PRIORITY_URGENT) {
		music_priority = MUSIC_PRIORITY_URGENT;
		ChangeThreadPriority(music_thread_id, MUSIC_PRIORITY_URGENT);
	}
	SignalSema(music_ready);
}

static ssize_t music_read(void *file, void *buffer, size_t count)
{
	size_t bytes = fread(buffer, 1, count, file);

	return bytes == 0 && ferror((FILE *)file) ? -1 : (ssize_t)bytes;
}

static off_t music_seek(void *file, off_t offset, int whence)
{
	if (fseek(file, offset, whence) != 0)
		return -1;
	return ftell(file);
}

static void music_thread(void *arg)
{
	short decoded[MUSIC_CHUNK];
	size_t bytes;
	unsigned int i, writepos;
	int result;

	(void)arg;
	while (!music_quit) {
		WaitSema(music_ready);
		WaitSema(music_lock);
		while (!music_quit && music_job_started && !music_paused && !music_eof) {
			writepos = music_writepos;
			if (MUSIC_SAMPLES - (writepos - music_readpos) < MUSIC_CHUNK)
				break;
			result = mpg123_read(handle, (unsigned char *)decoded, sizeof(decoded), &bytes);
			for (i = 0; i < bytes / sizeof(short); i++)
				music_buffer[(writepos + i) & (MUSIC_SAMPLES - 1)] = decoded[i];
			music_writepos = writepos + bytes / sizeof(short);
			if (result == MPG123_DONE || (result != MPG123_OK && result != MPG123_NEW_FORMAT))
				music_eof = true;
		}
		SignalSema(music_lock);
		if (music_priority != MUSIC_PRIORITY && (music_quit || music_paused || music_eof ||
			music_writepos - music_readpos >= MUSIC_MIN_BUFFER)) {
			music_priority = MUSIC_PRIORITY;
			ChangeThreadPriority(GetThreadId(), MUSIC_PRIORITY);
		}
	}
	SignalSema(music_done);
	ExitThread();
}

int music_init(void)
{
	ee_sema_t sema;
	ee_thread_t thread;

	if (music_initialized)
		return 1;
	if (mpg123_init() != MPG123_OK)
		return 0;
	memset(&sema, 0, sizeof(sema));
	sema.init_count = sema.max_count = 1;
	music_lock = CreateSema(&sema);
	sema.init_count = 0;
	music_ready = CreateSema(&sema);
	music_done = CreateSema(&sema);
	memset(&thread, 0, sizeof(thread));
	thread.func = music_thread;
	thread.stack = music_stack;
	thread.stack_size = sizeof(music_stack);
	thread.gp_reg = &_gp;
	thread.initial_priority = MUSIC_PRIORITY;
	music_priority = MUSIC_PRIORITY;
	music_quit = false;
	music_thread_id = CreateThread(&thread);
	if (music_lock < 0 || music_ready < 0 || music_done < 0 || music_thread_id < 0 || StartThread(music_thread_id, NULL) < 0) {
		if (music_thread_id >= 0)
			DeleteThread(music_thread_id);
		if (music_lock >= 0)
			DeleteSema(music_lock);
		if (music_ready >= 0)
			DeleteSema(music_ready);
		if (music_done >= 0)
			DeleteSema(music_done);
		music_lock = music_ready = music_done = music_thread_id = -1;
		mpg123_exit();
		return 0;
	}
	music_initialized = true;
	return 1;
}

void music_mix(short *buffer, int count)
{
	unsigned int i, readpos, available;
	int value, volume = music_volume;

	if (!music_initialized || !music_job_started || music_paused)
		return;
	readpos = music_readpos;
	available = music_writepos - readpos;
	if (available > count)
		available = count;
	for (i = 0; i < available; i++) {
		value = buffer[i] + music_buffer[(readpos + i) & (MUSIC_SAMPLES - 1)] * volume / PLATFORM_VOLUME_MAX;
		buffer[i] = value > 32767 ? 32767 : value < -32768 ? -32768 : value;
	}
	music_readpos = readpos + available;
	if (music_eof && music_readpos == music_writepos)
		music_job_started = 0;
	music_wake();
}

void music_stop(void)
{
	if (!music_initialized)
		return;
	WaitSema(music_lock);
	music_job_started = 0;
	music_paused = false;
	music_readpos = music_writepos = 0;
	if (handle) {
		mpg123_close(handle);
		mpg123_delete(handle);
		handle = NULL;
	}
	if (music_file) {
		fclose(music_file);
		music_file = NULL;
	}
	SignalSema(music_lock);
}

int music_start_play(char *filename, int startpos)
{
	int result;

	music_stop();
	if (!music_initialized)
		return 2;
	WaitSema(music_lock);
	handle = mpg123_new(NULL, NULL);
	music_file = fopen(filename, "rb");
	if (!handle || !music_file) {
		if (handle)
			mpg123_delete(handle);
		if (music_file)
			fclose(music_file);
		handle = NULL;
		music_file = NULL;
		SignalSema(music_lock);
		return 2;
	}
	setvbuf(music_file, music_file_buffer, _IOFBF, sizeof(music_file_buffer));
	mpg123_replace_reader_handle(handle, music_read, music_seek, NULL);
	mpg123_param(handle, MPG123_ADD_FLAGS, MPG123_FORCE_MONO, 0);
	mpg123_param(handle, MPG123_FORCE_RATE, 22050, 0);
	mpg123_format_none(handle);
	mpg123_format(handle, 22050, MPG123_MONO, MPG123_ENC_SIGNED_16);
	result = mpg123_open_handle(handle, music_file);
	if (result == MPG123_OK) {
		if (startpos)
			mpg123_seek_frame(handle, startpos, SEEK_SET);
		music_eof = false;
		music_job_started = 1;
	} else {
		mpg123_delete(handle);
		fclose(music_file);
		handle = NULL;
		music_file = NULL;
	}
	SignalSema(music_lock);
	music_wake();
	return result == MPG123_OK ? 1 : 2;
}

void music_pause(void)
{
	music_paused = true;
}

void music_resume(void)
{
	music_paused = false;
	if (music_initialized)
		music_wake();
}

void music_deinit(void)
{
	ee_thread_status_t status;

	if (!music_initialized)
		return;
	music_stop();
	music_quit = true;
	music_wake();
	WaitSema(music_done);
	while (ReferThreadStatus(music_thread_id, &status) >= 0 && status.status != THS_DORMANT)
		usleep(1000);
	music_initialized = false;
	DeleteThread(music_thread_id);
	DeleteSema(music_lock);
	DeleteSema(music_ready);
	DeleteSema(music_done);
	music_lock = music_ready = music_done = music_thread_id = -1;
	mpg123_exit();
}
