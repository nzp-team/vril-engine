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
#include <asndlib.h>
#include <mp3player.h>

// sB MP3 Playback 
// will expand to add OGG support in future?

byte* mp3_data;
qboolean stopmp3 = false;

void CDAudio_Play(byte track, qboolean looping)
{
	return;
}

void CDAudio_PlayFromString(char* track_name, qboolean looping)
{
	stopmp3 = false;
	byte *mp3buffer;
	
	mp3buffer = COM_LoadFile (track_name, 5);
	
	if (mp3buffer == NULL) {
		Con_Printf("NULL MP3: %s\n", track_name);
		return;
	}
	
	mp3_data = mp3buffer;
	
	MP3Player_Volume(255);
	
	MP3Player_PlayBuffer (mp3buffer, com_filesize, NULL);
	
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
	qboolean isplaying;
	
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


int CDAudio_Init(void)
{
	/*
	ASND_Init();
	MP3Player_Init();
	*/
	return 0;
}


void CDAudio_Shutdown(void)
{
	stopmp3 = true;
}
