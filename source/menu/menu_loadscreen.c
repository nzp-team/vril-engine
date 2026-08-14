/*
Copyright (C) 2025-2026 NZ:P Team

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
#include "../nzportable_def.h"
#include "menu_defs.h"
#include "menu_loadscreen_tips.h"

//=============================================================================
/* LOADING SCREEN */

int				loadingScreen;
double      	loadingtimechange;
int         	loadingdot;
char        	*loadinglinetext;

qboolean 		loadscreeninit;

char* 			map_loadname;
char* 			map_loadname_pretty;

float loading_cur_step;
float loading_num_step;
int loading_step;

static image_t 	lscreen_image;
static char lscreen_path[MAX_OSPATH];
static char lscreen_identifier[64];
static int lscreen_format;
static qboolean loading_waiting_for_input;
static qboolean loading_spawn_released;
static int loading_skip_key = -1;
static double loadscreen_start_time;
static float loading_progress_shown;
static qboolean loading_progress_phase_active;
static float loading_progress_phase_start;
static float loading_progress_phase_end;
static int loading_progress_phase_current;
static int loading_progress_phase_total;
char *LoadingScreen_ReturnTip(void)
{
	return (char *)loading_tips[rand() % (sizeof(loading_tips) / sizeof(loading_tips[0]))];
}

void LoadingScreen_DrawProgressBar(void)
{
	float progress;
	int width;
	int height;
	int x;
	int y;

	if (loading_num_step <= 0)
		return;

	if (loading_progress_phase_active)
	{
		progress = loading_progress_phase_start;
		if (loading_progress_phase_total > 0)
			progress += (loading_progress_phase_end - loading_progress_phase_start) * loading_progress_phase_current / loading_progress_phase_total;
	}
	else
	{
		// Client populated additional work, so we need to reflect it in the loading bar.
		progress = 0.45f * loading_cur_step / loading_num_step;
	}
	if (progress < loading_progress_shown)
		progress = loading_progress_shown;
	else
		loading_progress_shown = progress;

	if (progress > 1)
		progress = 1;

	width = vid.width * 3 / 4;
	height = max(2, 2 * vid.scale);
	x = (vid.width - width) / 2;
	y = vid.height - 17 * vid.scale;

	Draw_FillByColor(x - vid.scale, y - vid.scale, width + 2 * vid.scale, height + 2 * vid.scale, 0, 0, 0, 220);
	Draw_FillByColor(x, y, (int)(width * progress), height, 255, 204, 0, 255);
}

void LoadingScreen_ClearProgress(void)
{
	if (loadingScreen)
		return;

	loading_cur_step = 0;
	loading_num_step = 0;
	loading_step = -1;
	loading_progress_shown = 0;
	loading_progress_phase_active = false;
}

void LoadingScreen_BeginProgressPhase(float start, float end, int total)
{
	loading_progress_phase_active = true;
	loading_progress_phase_start = start;
	loading_progress_phase_end = end;
	loading_progress_phase_current = 0;
	loading_progress_phase_total = total;
	if (loading_progress_shown < start)
		loading_progress_shown = start;
}

void LoadingScreen_AdvanceProgress(void)
{
	if (loading_progress_phase_active &&
		loading_progress_phase_current < loading_progress_phase_total)
		loading_progress_phase_current++;
}

void LoadingScreen_CompleteProgress(void)
{
	if (loading_num_step > 0)
		loading_cur_step = loading_num_step;
	loading_progress_shown = 1;
	loading_progress_phase_active = false;
}

static float LoadingScreen_Fade(double elapsed, double start, double duration)
{
	float fade = (float)((elapsed - start) / duration);
	if (fade < 0)
		return 0;
	if (fade > 1)
		return 1;
	return fade;
}

static char *LoadingScreen_GameModeName(void)
{
	switch ((int)sv_gamemode.value)
	{
		case 0: return "CLASSIC";
		case 1: return "GRIEF";
		case 2: return "GUN GAME";
		case 3: return "HARDCORE";
		case 4: return "WILD WEST";
		case 5: return "STICKS & STONES";
		case 6: return "FEVER";
		default: return "???";
	}
}

static void LoadingScreen_LoadImage(void)
{
	if (!loadscreeninit)
	{
		snprintf(lscreen_path, sizeof(lscreen_path), "gfx/lscreen/%s", map_loadname);
		lscreen_format = IMAGE_TGA | IMAGE_PNG | IMAGE_JPG;
		lscreen_image = Image_LoadImage(lscreen_path, lscreen_format, 0, false, false);

		if (lscreen_image < 0)
		{
			snprintf(lscreen_path, sizeof(lscreen_path), "gfx/lscreen/lscreen");
			lscreen_format = IMAGE_PNG;
			lscreen_image = Image_LoadImage(lscreen_path, lscreen_format, 0, false, false);
		}

		tex_filebase(lscreen_path, lscreen_identifier);
		loadscreeninit = true;
		return;
	}

	lscreen_image = Image_FindImage(lscreen_identifier);
	if (lscreen_image < 0)
		lscreen_image = Image_LoadImage(lscreen_path, lscreen_format, 0, false, false);
}

static void LoadingScreen_DrawSkipPrompt(double elapsed)
{
	const int padding = 4 * vid.scale;
	const int gap = 3 * vid.scale;
	const int prompt_y = vid.height - (5 * _CHAR_HEIGHT * vid.scale);
	const int press_width = getTextWidth("Press ", vid.scale);
	const int skip_width = getTextWidth("to skip", vid.scale);
	const image_t confirm_icon = Menu_GetConfirmIcon();
	const char *confirm_name = Key_KeynumToString(MENU_KEY_CONFIRM);
	const int middle_width = confirm_icon > 0 ? 12 * vid.scale : getTextWidth((char *)confirm_name, vid.scale);
	const int prompt_width = press_width + middle_width + gap + skip_width;
	const int prompt_x = vid.width - prompt_width - (8 * vid.scale);
	const int alpha = 205 + (int)(50 * sin(elapsed * 2.0));

	Draw_FillByColor(prompt_x - padding, prompt_y - padding, prompt_width + (padding * 2), (_CHAR_HEIGHT * vid.scale) + (padding * 2), 0, 0, 0, 100);
	Draw_ColoredString(prompt_x, prompt_y, "Press ", 255, 255, 255, alpha, vid.scale);

	if (confirm_icon > 0)
		Draw_ColoredStretchPic(prompt_x + press_width, prompt_y - (2 * vid.scale), confirm_icon, middle_width, middle_width, 255, 255, 255, alpha);
	else
		Draw_ColoredString(prompt_x + press_width, prompt_y, (char *)confirm_name, 255, 255, 0, alpha, vid.scale);

	Draw_ColoredString(prompt_x + press_width + middle_width + gap, prompt_y, " to skip", 255, 255, 255, alpha, vid.scale);
}

void LoadingScreen_Begin(const char *map_name)
{
	LoadingScreen_ClearProgress();
	loadingScreen = 1;
	loadscreeninit = false;
	lscreen_image = -1;
	lscreen_identifier[0] = '\0';
	loading_waiting_for_input = menu_is_solo;
	loading_spawn_released = false;
	loading_skip_key = -1;
	loadscreen_start_time = Sys_FloatTime();
	Music_PlayLoadingTrack(map_name);
}

static void LoadingScreen_ReleaseSpawn(void)
{
	if (!loading_waiting_for_input)
		return;

	IN_ClearPendingInput();
	loading_waiting_for_input = false;
	loading_spawn_released = true;
	if (cls.signon == 2)
		CL_SignonReply();
}

qboolean LoadingScreen_IsActive(void)
{
	return loadingScreen;
}

qboolean LoadingScreen_IsWaiting(void)
{
	return loading_waiting_for_input;
}

qboolean LoadingScreen_ShouldWaitForSpawn(void)
{
	if (!loadingScreen || !menu_is_solo || loading_spawn_released)
		return false;

	loading_waiting_for_input = true;
	SCR_EndLoadingPlaque();
	return true;
}

qboolean LoadingScreen_Key(int key, qboolean down)
{
	if (key == loading_skip_key)
	{
		if (!down)
			loading_skip_key = -1;
		return true;
	}

	if (!loading_waiting_for_input || key != MENU_KEY_CONFIRM)
		return false;

	if (down)
	{
		loading_skip_key = key;
		LoadingScreen_ReleaseSpawn();
	}
	return true;
}

void LoadingScreen_Update(void)
{
	if (loading_waiting_for_input && !Music_IsPlaying())
		LoadingScreen_ReleaseSpawn();
}

void LoadingScreen_Finish(void)
{
	loadingScreen = 0;
	loading_waiting_for_input = false;
	loading_spawn_released = false;
	loading_skip_key = -1;
	LoadingScreen_ClearProgress();
	Music_Stop();
}

//=============================================================================

/*
==============
Menu_DrawLoadScreen
==============
*/
void Menu_DrawLoadScreen (void)
{
	double elapsed;
	float image_fade;
	float map_fade;
	float mode_fade;

	if (developer.value) {
		return;
	}
	
	if (!con_forcedup && !loading_waiting_for_input) {
	    return;
	}

	if (loadingScreen) {
		elapsed = Sys_FloatTime() - loadscreen_start_time;
		map_fade = LoadingScreen_Fade(elapsed, 0, 1.0);
		mode_fade = LoadingScreen_Fade(elapsed, 1.25, 1.0);
		image_fade = LoadingScreen_Fade(elapsed, 2.5, 1.0);

		Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 255);
		LoadingScreen_LoadImage();

		Draw_StretchPic(0, 0, lscreen_image, vid.width, vid.height);
		Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, (int)(255 * (1.0f - image_fade)));

		Draw_FillByColor(0, 0, vid.width, ((18 * vid.scale) * 2), 0, 0, 0, 175);
		Draw_FillByColor(0, vid.height - ((8 * vid.scale) * 2), vid.width, ((8 * vid.scale) * 2), 0, 0, 0, 175);

		if (map_loadname_pretty != NULL) {
			Draw_ColoredString(vid.width/64, 5 * vid.scale, map_loadname_pretty, 255, 255, 0, 255 * map_fade, vid.scale * 2);
		} else {
			Draw_ColoredString(vid.width/64, 5 * vid.scale, map_loadname, 255, 255, 0, 255 * map_fade, vid.scale * 2);
		}

		Draw_ColoredString(vid.width/64, 22 * vid.scale, LoadingScreen_GameModeName(), 255, 255, 255, 255 * mode_fade, vid.scale);

		if (loadingtimechange < Sys_FloatTime ())
		{
			loadinglinetext = LoadingScreen_ReturnTip();
			loadingtimechange = Sys_FloatTime () + 15;
		}

		if (key_dest == key_game) {
			Draw_ColoredString(vid.width/2 - (getTextWidth(loadinglinetext, vid.scale)/2), vid.height - (4 * vid.scale) - (_CHAR_HEIGHT * vid.scale), loadinglinetext, 255, 255, 255, 255, vid.scale);
		}

		if (loading_waiting_for_input)
			LoadingScreen_DrawSkipPrompt(elapsed);
	}
}
