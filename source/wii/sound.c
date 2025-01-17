/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2007 Peter Mackay

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

// ELUTODO: go to 48khz once we have hi-res sound packs

#include <gccore.h>
#include <asndlib.h>

#include "../quakedef.h"

volatile int snd_current_sample_pos;

void SNDDMA_Loopback(s32 voice)
{
	ASND_AddVoice(voice, (unsigned short*)shm->buffer + snd_current_sample_pos, shm->samples >> 2);
	snd_current_sample_pos += (shm->samples >> 3);
	if(snd_current_sample_pos >= shm->samples)
		snd_current_sample_pos = 0;
}

qboolean SNDDMA_Init(void)
{
	ASND_Init();
	
	// Set up Quake's audio.
	shm = (void *) Hunk_AllocName(sizeof(*shm), "shm");
	shm->channels			= 2;
	shm->samplebits			= 16;
	shm->speed				= 22050;
	shm->soundalive			= true;
	shm->splitbuffer		= false;
	shm->samples			= 32768;
	shm->samplepos			= 0;
	shm->submission_chunk	= 1;
	shm->buffer = Hunk_AllocName(65536, "shmbuf");

	ASND_SetVoice(1, VOICE_STEREO_16BIT, shm->speed, 0, shm->buffer, shm->samples >> 2, 255, 255, SNDDMA_Loopback);
	snd_current_sample_pos = (shm->samples >> 3);

	return true;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples, not stereo)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos(void)
{
	shm->samplepos = snd_current_sample_pos;
	return shm->samplepos;
}

void SNDDMA_Shutdown(void)
{
	ASND_End();
}

void SNDDMA_Submit(void)
{
}
