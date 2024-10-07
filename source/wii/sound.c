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

#include <ogc/audio.h>
#include <ogc/cache.h>

#include "../quakedef.h"

// Represents a single sound sample.
typedef u32 sample;

// We copy Quake's audio into double buffered DMA buffers while it is
// being transferred to the GameCube's audio system.
static const size_t		samples_per_dma_buffer	= 2048;
static sample			dma_buffers[2][2048] __attribute__((aligned(32)));
static size_t			current_dma_buffer		= 0;

// Quake writes its audio into this mix buffer.
static const size_t		samples_per_mix_buffer	= 32768; //65536
static sample			mix_buffer[32768];
static volatile size_t	mix_buffer_pointer		= 0;

// Called whenever more audio data is required.
static void play_more_audio()
{
	// Copy from mix buffer to DMA buffer.
	sample* const		src_begin	= &mix_buffer[mix_buffer_pointer];
	sample* const		dst_begin	= &dma_buffers[current_dma_buffer][0];
	const sample* const	dst_end		= dst_begin + samples_per_dma_buffer;
	sample*				src			= src_begin;
	sample*				dst			= dst_begin;
	while (dst != dst_end)
	{
		// We have to swap the channels, because Quake stores the left
		// channel first, whereas the GameCube expects right first.
		const u32 mix_sample = *src;
		*src++ = 0;
		*dst++ = (mix_sample >> 16) | ((mix_sample & 0x0000ffff) << 16);
	}

	// Set up the DMA.
	const u32		dma_src_address	= (u32)(dst_begin);
	const size_t	bytes			= samples_per_dma_buffer * sizeof(sample);
	AUDIO_InitDMA(dma_src_address, bytes);

	// Flush the data cache.
	DCFlushRange(dst_begin, bytes);

	// Start the DMA.
	AUDIO_StartDMA();

	// Move the mix buffer pointer.
	mix_buffer_pointer = (mix_buffer_pointer + samples_per_dma_buffer) % samples_per_mix_buffer;

	// Use the other DMA buffer next time.
	current_dma_buffer = 1 - current_dma_buffer;
}

qboolean SNDDMA_Init(void)
{
	// Set up Quake's audio.
	shm = &sn;
	shm->channels			= 2;
	shm->samplebits			= 16;
	shm->speed				= 32000;
	shm->soundalive			= true;
	shm->splitbuffer		= false;
	shm->samples			= samples_per_mix_buffer * shm->channels;
	shm->samplepos			= 0;
	shm->submission_chunk	= 1;
	shm->buffer				= (unsigned char*)(&mix_buffer[0]);

	// Initialise the audio system.
	AUDIO_Init(0);
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_32KHZ);
	AUDIO_RegisterDMACallback(play_more_audio);

	// Start the first chunk of audio playing.
	play_more_audio();

	return true;
}

void SNDDMA_Shutdown(void)
{
	// Stop streaming.
	AUDIO_RegisterDMACallback(0);
	AUDIO_StopDMA();
}

int SNDDMA_GetDMAPos(void)
{
	return mix_buffer_pointer * shm->channels;
}

void SNDDMA_Submit(void)
{
}
