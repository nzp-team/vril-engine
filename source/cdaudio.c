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
#include <stdio.h>

#ifdef __PSP__
#include <pspaudiolib.h>
#include "platform/psp/mp3.h"
int sceKernelDelayThread(int delay);
#elif __3DS__
#include <3ds.h>
#include "platform/ctr/mp3.h"
#elif __PSP2__
#include "platform/psp2/mp3.h"
#endif

int				MAX_VOLUME;

extern cvar_t 	bgmtype;
extern cvar_t 	bgmvolume;

static bool 	playing  = false;
static bool 	paused   = false;
static bool 	enabled  = false;

static char		*last_track_string = "";

int 			cd_loop = 0;

static void CD_f (void);

void CDAudio_VolumeChange(float bgmvolume)
{
	int volume = (int)(bgmvolume*(float)MAX_VOLUME);
	mp3_volume = volume;
}

void CDAudio_PlayFromString(char* track_name, qboolean looping)
{
	CDAudio_Stop();

	char path[512];
	snprintf(path, 512, "%s/tracks/%s.mp3", com_gamedir, track_name);

	int ret = mp3_start_play(path, looping);
	cd_loop = looping;
	last_track_string = track_name;

	if (ret != 2) playing = true;
	else {
		Con_Printf("Couldn't find %s\n", path);
		playing = false;
		Cvar_Set("bgmtype","none");
		CDAudio_VolumeChange(0);
	}

	//CDAudio_VolumeChange(0.75);
}

void CDAudio_Stop(void)
{
    mp3_stop();
    mp3_job_started = 0;
    playing = false;
    mp3_volume = 0;
}

void CDAudio_Pause(void)
{
	mp3_volume = 0;
	paused = true;
}

void CDAudio_Resume(void)
{
	CDAudio_VolumeChange(bgmvolume.value);
	paused = false;
}

void CDAudio_Update(void)
{
    if (!enabled || paused || !playing) {
		return;
	}

    if (!mp3_job_started && cd_loop) {
        CDAudio_PlayFromString(last_track_string, cd_loop);
    } else if (!mp3_job_started) {
        playing = false;
    }
}

void CDAudio_DelayThread(int delay)
{
#ifdef __PSP__
	sceKernelDelayThread(delay);
#elif __3DS__
	svcSleepThread(delay*2);
#endif
}

void CDAudio_SetMaxVolume(void)
{
#ifdef __PSP__
	MAX_VOLUME = PSP_VOLUME_MAX;
#elif __3DS__
	MAX_VOLUME = 60;
#else
	MAX_VOLUME = 60;
#endif
}

void CDAudio_Init(void)
{
	if (cls.state == ca_dedicated) return;
	if (COM_CheckParm("-nocdaudio")) return;

	if (mp3_init() == 0) {
		Sys_Error("Could not Initialize CDAudio.");
	}

	CDAudio_DelayThread(5*10000);
	CDAudio_SetMaxVolume();

	enabled = true;
	Cmd_AddCommand ("cd", CD_f);

	Con_Printf("cdaudio init fine\n");
}

void CDAudio_Shutdown(void)
{
	CDAudio_Stop();
	CDAudio_DelayThread(5*10000);
	mp3_deinit();
}

static void CD_f (void)
{
	char	*command;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("commands:");
		Con_Printf("on, off, reset, \n");
		Con_Printf("playstring, stop, pause, resume\n");
		Con_Printf("eject, close, info\n");
		return;
	}

	command = Cmd_Argv (1);

	if (Q_strcasecmp(command, "on") == 0)
	{
		enabled = true;
		return;
	}

	if (Q_strcasecmp(command, "off") == 0)
	{
		if (playing)
			CDAudio_Stop();
		enabled = false;
		return;
	}

	if (Q_strcasecmp(command, "reset") == 0)
	{
		enabled = true;
		if (playing)
			CDAudio_Stop();
		return;
	}

	if (Q_strcasecmp(command, "playstring") == 0)
	{
		char* track_name = Cmd_Argv(2);
		qboolean loop = (qboolean)atoi(Cmd_Argv(3));
		CDAudio_PlayFromString(track_name, loop);
	}

	if (Q_strcasecmp(command, "stop") == 0)
	{
			CDAudio_Stop();
		return;
	}

	if (Q_strcasecmp(command, "pause") == 0)
	{
		CDAudio_Pause();
		return;
	}

	if (Q_strcasecmp(command, "resume") == 0)
	{
		CDAudio_Resume();
		return;
	}

	if (Q_strcasecmp(command, "eject") == 0)
	{
		if (playing)
			CDAudio_Stop();
		return;
	}
}