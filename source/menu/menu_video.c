/*
Copyright (C) 2025 NZ:P Team

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

//=============================================================================
/* VIDEO MENU */

char* fps_string;
char* particles_string;
char* retro_string;
char* dithering_string;

qboolean platform_supports_dithering;

extern cvar_t show_fps;
extern cvar_t cl_maxfps;
extern cvar_t scr_fov;
extern cvar_t v_gamma;
extern cvar_t r_runqmbparticles;
extern cvar_t r_retro;
extern cvar_t r_dithering;

void Menu_Video_AllocStrings (void)
{
    fps_string = malloc(16*sizeof(char));
    particles_string = malloc(16*sizeof(char));
    retro_string = malloc(16*sizeof(char));
    dithering_string = malloc(16*sizeof(char));
}

void Menu_Video_FreeStrings (void)
{
    free(fps_string);
    free(particles_string);
    free(retro_string);
    free(dithering_string);
}

void Menu_Video_ApplyShowFPS (void)
{
    float current_showfps = show_fps.value;

    current_showfps += 1;
    if (current_showfps > 1) {
        current_showfps = 0;
    }

    Cvar_SetValue ("show_fps", current_showfps);
}

void Menu_Video_ApplyParticles (void)
{
    float current_particles = r_runqmbparticles.value;

    current_particles += 1;
    if (current_particles > 1) {
        current_particles = 0;
    }

    Cvar_SetValue ("r_runqmbparticles", current_particles);
}

void Menu_Video_ApplyTextureFiltering (void)
{
    float current_filter = r_retro.value;

    current_filter += 1;
    if (current_filter > 1) {
        current_filter = 0;
    }

    Cvar_SetValue ("r_retro", current_filter);
}

void Menu_Video_ApplyDithering (void)
{
    float current_dithering = r_dithering.value;

    current_dithering += 1;
    if (current_dithering > 1) {
        current_dithering = 0;
    }

    Cvar_SetValue ("r_dithering", current_dithering);
}

/*
===============
Menu_Video_Set
===============
*/
void Menu_Video_Set (void)
{
#ifdef RENDERER_SUPPORTS_DITHERING
    platform_supports_dithering = true;
#else
    platform_supports_dithering = false;
#endif

    Menu_ResetMenuButtons();
    Menu_Video_AllocStrings();
	m_state = m_video;
}

void Menu_Video_SetStrings (void)
{
    if ((int)show_fps.value == 1) {
        fps_string = "ENABLED";
    } else {
        fps_string = "DISABLED";
    }

    if ((int)r_runqmbparticles.value == 1) {
        particles_string = "ENABLED";
    } else {
        particles_string = "DISABLED";
    }

    if ((int)r_retro.value == 1) {
        retro_string = "LINEAR";
    } else {
        retro_string = "NEAREST";
    }

    if ((int)r_dithering.value == 1) {
        dithering_string = "ENABLED";
    } else {
        dithering_string = "DISABLED";
    }
}

/*
===============
Menu_Video_Draw
===============
*/
void Menu_Video_Draw (void)
{
    int video_items = 0;

	// Background
	Menu_DrawCustomBackground (true);

	// Header
	Menu_DrawTitle ("VIDEO OPTIONS", MENU_COLOR_WHITE);
    // Map panel makes the background darker
    Menu_DrawMapPanel();
    // Set value strings
    Menu_Video_SetStrings();

    // Show FPS
    Menu_DrawButton (1, video_items++, "SHOW FPS", "Toggles display of FPS Values.", Menu_Video_ApplyShowFPS); ;
    Menu_DrawOptionButton (1, fps_string);

    // Max FPS
    Menu_DrawButton (2, video_items++, "MAX FPS", "Sets the max FPS Value.", NULL);
    Menu_DrawOptionSlider (2, video_items-1, 5, 60, cl_maxfps, "cl_maxfps", true, true, 5.0f);

    // Field of View
    Menu_DrawButton (3, video_items++, "FIELD OF VIEW", "Change Camera Field of View", NULL);
    Menu_DrawOptionSlider (3, video_items-1, 60, 160, scr_fov, "fov", false, true, 10.0f);

    // Gamma
    Menu_DrawButton (4, video_items++, "GAMMA", "Adjust game Black Level.", NULL);
    Menu_DrawOptionSlider (4, video_items-1, 0, 1, v_gamma, "gamma", false, false, 0.1f);

    // Particles 
    Menu_DrawButton (5, video_items++, "PARTICLES", "Trade Particle Effects for Performance.", Menu_Video_ApplyParticles);
    Menu_DrawOptionButton (5, particles_string);

    // Retro (texture filtering)
    Menu_DrawButton (6, video_items++, "TEXTURE FILTERING", "Choose 3D Environment Filtering Mode.", Menu_Video_ApplyTextureFiltering);
    Menu_DrawOptionButton (6, retro_string);

    // Dithering 
    if (platform_supports_dithering) {
        Menu_DrawButton (7, video_items++, "DITHERING", "Toggle Decrease in Color Banding", Menu_Video_ApplyDithering);
        Menu_DrawOptionButton (7, dithering_string);
    }

	Menu_DrawButton(11.5, video_items, "BACK", "Return to Main Menu.", Menu_Configuration_Set);
}