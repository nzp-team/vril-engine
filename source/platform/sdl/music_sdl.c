#include "../../nzportable_def.h"
#include <SDL_mixer.h>

volatile int music_job_started;
int music_volume;
qboolean music_paused;
static Mix_Music *current_music;
static qboolean music_initialized;
static int applied_music_volume = -1;
extern int music_loop;

static void SDLCALL Music_Finished(void)
{
	music_job_started = 0;
}

int music_init(void)
{
	if ((Mix_Init(MIX_INIT_MP3) & MIX_INIT_MP3) == 0) {
		Con_Printf("SDL_mixer MP3 support unavailable: %s\n", Mix_GetError());
		return 0;
	}
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) < 0) {
		Con_Printf("SDL_mixer audio initialization failed: %s\n", Mix_GetError());
		Mix_Quit();
		return 0;
	}
	Mix_HookMusicFinished(Music_Finished);
	music_initialized = true;
	return 1;
}

void music_deinit(void)
{
	music_stop();
	Mix_HookMusicFinished(NULL);
	Mix_CloseAudio();
	Mix_Quit();
	music_initialized = false;
	applied_music_volume = -1;
}

int music_start_play(char *fname, int pos)
{
	(void)pos;
	music_stop();
	current_music = Mix_LoadMUS(fname);
	if (!current_music)
		return 2;
	Mix_VolumeMusic(music_volume);
	applied_music_volume = music_volume;
	if (Mix_PlayMusic(current_music, music_loop ? -1 : 0) < 0) {
		Con_Printf("SDL_mixer could not play %s: %s\n", fname, Mix_GetError());
		Mix_FreeMusic(current_music);
		current_music = NULL;
		return 0;
	}
	music_job_started = 1;
	music_paused = false;
	return 1;
}

void music_pause(void)
{
	if (music_job_started) Mix_PauseMusic();
	music_paused = true;
}

void music_stop(void)
{
	Mix_HaltMusic();
	if (current_music) Mix_FreeMusic(current_music);
	current_music = NULL;
	music_job_started = 0;
	music_paused = false;
}

void music_resume(void)
{
	if (music_job_started) Mix_ResumeMusic();
	music_paused = false;
}

void music_update(void)
{
	if (music_initialized && music_volume != applied_music_volume) {
		Mix_VolumeMusic(music_volume);
		applied_music_volume = music_volume;
	}
}
