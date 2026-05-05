/*
Copyright (C) 2007 Peter Mackay and Chris Swindle.
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
#include "nzportable_def.h"

extern cvar_t 	bgmvolume;
static float	prev_bgmvolume;
float 			cur_bgmvolume;

static bool 	playing  = false;
static bool 	enabled  = false;

static char 	last_track_string[MAX_QPATH] = "";
int 			music_loop = 0;

static void Music_f (void);

void Music_VolumeChange(float bgmvolume)
{
	int volume = (int)(bgmvolume*(float)PLATFORM_VOLUME_MAX);
	music_volume = volume;
}

void Music_PlayFromString(char* track_name, qboolean looping)
{
	Music_Stop();

	char path[512];
	snprintf(path, 512, "%s/tracks/%s.mp3", com_gamedir, track_name);

	music_loop = looping;
	int ret = music_start_play(path, 0);
	
	snprintf(last_track_string, MAX_QPATH, "%s", track_name);

	if (ret != 2) {
		playing = true;
		Music_VolumeChange(bgmvolume.value);
	} else {
		Con_Printf("Couldn't find %s\n", path);
		playing = false;
		Music_VolumeChange(0);
	}
}

void Music_Stop(void)
{
	// if paused, we need to unpause to unlock
	// the thread before shutting it down
	music_paused = false;

    music_stop();

    music_job_started = 0;
    music_volume = 0;
	playing = false;
}

void Music_Pause(void)
{
	if (!playing || music_paused) return;

	music_pause();
	music_volume = 0;
}

void Music_Resume(void)
{
	if (!music_paused) return;

	Music_VolumeChange(bgmvolume.value);
	music_resume();
}

void Music_Update(void)
{
    if (!enabled || !playing) {
		return;
	}

	if (!music_paused) {
		cur_bgmvolume = bgmvolume.value;
		if (cur_bgmvolume != prev_bgmvolume) {
			Music_VolumeChange(bgmvolume.value);
			prev_bgmvolume = bgmvolume.value;
		}
	}

    if (!music_job_started) {
		if(music_loop && !music_paused) {
			Music_PlayFromString(last_track_string, music_loop);
		} else if (!music_loop) {
			playing = false;
			music_paused = false;
		}
    }
}

void Music_Init(void)
{
	if (cls.state == ca_dedicated) return;
	if (COM_CheckParm("-nocdaudio")) return;

	if (music_init() == 0) {
		Sys_Error("Could not Initialize Music Subsystem.");
	}

	enabled = true;
	Cmd_AddCommand ("cd", Music_f);

	prev_bgmvolume = bgmvolume.value;
}

void Music_Shutdown(void)
{
	Music_Stop();
	music_deinit();
}

static void Music_f (void)
{
	char	*command;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("commands:");
		Con_Printf("on, off, reset, \n");
		Con_Printf("playstring, stop, pause,\n");
		Con_Printf("resume, eject\n");
		return;
	}

	command = Cmd_Argv (1);

	if (Q_strcasecmp(command, "on") == 0) {
		enabled = true;
		return;
	}

	if (Q_strcasecmp(command, "off") == 0) {
		if (playing) {
			Music_Stop();
		}
		enabled = false;
		return;
	}

	if (Q_strcasecmp(command, "reset") == 0)
	{
		enabled = true;
		if (playing) {
			Music_Stop();
		}
		return;
	}

	if (Q_strcasecmp(command, "playstring") == 0)
	{
		char* track_name = Cmd_Argv(2);
		qboolean loop = (qboolean)atoi(Cmd_Argv(3));
		Music_PlayFromString(track_name, loop);
	}

	if (Q_strcasecmp(command, "stop") == 0)
	{
		Music_Stop();
		return;
	}

	if (Q_strcasecmp(command, "pause") == 0)
	{
		Music_Pause();
		return;
	}

	if (Q_strcasecmp(command, "resume") == 0)
	{
		Music_Resume();
		return;
	}

	if (Q_strcasecmp(command, "eject") == 0)
	{
		if (playing) {
			Music_Stop();
		}
		return;
	}
}