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
#include "../quakedef.h"
#include <mp3player.h>

// sB MP3 Playback 
// will expand to add OGG support in future?

byte* mp3_data;
qboolean stopmp3 = false;
qboolean isplaying = false;
qboolean enabled = false;

void CDAudio_Play(byte track, qboolean looping)
{
	return;
}

void CDAudio_PlayFromString(char* track_name, qboolean looping)
{
	stopmp3 = false;
	byte *mp3buffer;
	
	char path[256];
	sprintf(path, "tracks/%s.mp3", track_name);
	mp3buffer = COM_LoadFile (path, 5);
	
	if (mp3buffer == NULL) {
		Con_Printf("NULL MP3: %s\n", path);
		return;
	}
	
	//Con_Printf("PLAYING MP3: %s\n", path);
	
	mp3_data = mp3buffer;
	
	MP3Player_Volume(255);	
	MP3Player_PlayBuffer (mp3buffer, com_filesize, NULL);
	
	isplaying = true;
	
	return;
}

void CDAudio_Stop(void)
{
	stopmp3 = true;
}


void CDAudio_Pause(void)
{
}


void CDAudio_Resume(void)
{
	stopmp3 = false;
}


void CDAudio_Update(void)
{
	isplaying = MP3Player_IsPlaying ();
	
	if (isplaying == false) {
		free (mp3_data);
		stopmp3 = true;
	}
	
	if (stopmp3 == true) {
		if (mp3_data) {
			free (mp3_data);
		}
	}
	
}

int cd_loop = 0;

static void CD_f (void)
{
	char	*command;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("commands:");
		Con_Printf("on, off, reset, remap, \n");
		Con_Printf("play, stop, loop, pause, resume\n");
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
		if (isplaying)
			CDAudio_Stop();
		enabled = false;
		return;
	}

	if (Q_strcasecmp(command, "reset") == 0)
	{
		enabled = true;
		if (isplaying)
			CDAudio_Stop();
		return;
	}

	if (Q_strcasecmp(command, "play") == 0)
	{
		CDAudio_Play((byte)atoi(Cmd_Argv (2)), (qboolean) false);
		return;
	}

	if (Q_strcasecmp(command, "playstring") == 0)
	{
		char* track_name = Cmd_Argv(2);
		qboolean loop = (qboolean)atoi(Cmd_Argv(3));
		CDAudio_PlayFromString(track_name, loop);
	}

	if (Q_strcasecmp(command, "loop") == 0)
	{
		CDAudio_Play((byte)atoi(Cmd_Argv (2)), (qboolean) true);
		return;
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

	if (Q_strcasecmp(command, "info") == 0)
	{
		Con_Printf("MP3 Player By Crow_bar\n");
		Con_Printf("Based On sceMp3 Lib\n");
		Con_Printf("Additional fixed by\n");
		Con_Printf("dr_mabuse1981 and Baker.\n");
		Con_Printf("string support: cypress.\n");
		Con_Printf("\n");
		return;
	}
}


int CDAudio_Init(void)
{
	
	MP3Player_Init();
	
	enabled = true;
	
	Cmd_AddCommand ("cd", CD_f);
	
	Con_Printf("MP3 library Initialized\n");
	
	return 0;
}


void CDAudio_Shutdown(void)
{
	stopmp3 = true;
}
