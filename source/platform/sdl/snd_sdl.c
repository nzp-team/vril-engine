#include "../../nzportable_def.h"
#include "sdl_local.h"

#define SDL_DMA_SAMPLES 32768
static SDL_AudioDeviceID audio_device;
static unsigned int audio_position;

static void SDLCALL AudioCallback(void *userdata, Uint8 *stream, int length)
{
	int bytes = shm->samples * (shm->samplebits / 8);
	int offset = (audio_position * (shm->samplebits / 8)) % bytes;
	int first = length < bytes - offset ? length : bytes - offset;
	(void)userdata;
	memcpy(stream, shm->buffer + offset, first);
	if (first < length) memcpy(stream + first, shm->buffer, length - first);
	audio_position = (audio_position + length / (shm->samplebits / 8)) % shm->samples;
}

qboolean SNDDMA_Init(void)
{
	SDL_AudioSpec wanted, obtained;
	memset(&wanted, 0, sizeof(wanted));
	wanted.freq = 44100;
	wanted.format = AUDIO_S16SYS;
	wanted.channels = 2;
	wanted.samples = 1024;
	wanted.callback = AudioCallback;
	audio_device = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
	if (!audio_device) { Con_Printf("SDL audio disabled: %s\n", SDL_GetError()); return false; }
	shm = &sn;
	shm->splitbuffer = 0;
	shm->samplebits = 16;
	shm->speed = obtained.freq;
	shm->channels = obtained.channels;
	shm->samples = SDL_DMA_SAMPLES;
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = calloc(SDL_DMA_SAMPLES, sizeof(short));
	if (!shm->buffer) { SDL_CloseAudioDevice(audio_device); audio_device = 0; return false; }
	SDL_PauseAudioDevice(audio_device, 0);
	return true;
}

int SNDDMA_GetDMAPos(void) { return (int)audio_position; }
void SNDDMA_Submit(void) {}
void SNDDMA_Shutdown(void) { if (audio_device) SDL_CloseAudioDevice(audio_device); free(shm ? shm->buffer : NULL); audio_device = 0; }
