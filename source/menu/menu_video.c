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

int	menu_video_cursor;
int	VIDEO_ITEMS	= 7;

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

void Menu_Video_AllocStrings (void);


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

    Menu_Video_AllocStrings();
	m_state = m_video;
}

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
        retro_string = "ENABLED";
    } else {
        retro_string = "DISABLED";
    }

    if ((int)r_dithering.value == 1) {
        dithering_string = "ENABLED";
    } else {
        dithering_string = "DISABLED";
    }
}

void Menu_Video_ApplySliders (int dir)
{
    switch (menu_video_cursor) {
        // Max FPS
        case 1:
            float current_maxfps = cl_maxfps.value;
            if (dir == MENU_SLIDER_LEFT) {
                current_maxfps -= 5;
                if (current_maxfps < 5) current_maxfps = 5;
                Cvar_SetValue ("cl_maxfps", current_maxfps);
            } else if (dir == MENU_SLIDER_RIGHT) {
                current_maxfps += 5;
                if (current_maxfps > 60) current_maxfps = 60;
                Cvar_SetValue ("cl_maxfps", current_maxfps);
            }
            break;
        // Field of View
        case 2:
            float current_fov = scr_fov.value;
            if (dir == MENU_SLIDER_LEFT) {
                current_fov -= 5;
                if (current_fov < 50) current_fov = 50;
                Cvar_SetValue ("fov", current_fov);
            } else if (dir == MENU_SLIDER_RIGHT) {
                current_fov += 5;
                if (current_fov > 160) current_fov = 160;
                Cvar_SetValue ("fov", current_fov);
            }
            break;
        // Gamma
        case 3:
            double current_gamma = (double)v_gamma.value;
            if (dir == MENU_SLIDER_LEFT) {
                current_gamma -= 0.1;
                if (current_gamma < 0) current_gamma = 0;
                Cvar_SetValue ("gamma", current_gamma);
            } else if (dir == MENU_SLIDER_RIGHT) {
                current_gamma += 0.1;
                if (current_gamma > 1) current_gamma = 1;
                Cvar_SetValue ("gamma", (float)current_gamma);
            }
            break;
    }
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

void Menu_Video_ApplyRetro (void)
{
    float current_retro = r_retro.value;

    current_retro += 1;
    if (current_retro > 1) {
        current_retro = 0;
    }

    Cvar_SetValue ("r_retro", current_retro);
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
Menu_Video_Draw
===============
*/
void Menu_Video_Draw (void)
{
	// Background
	Menu_DrawCustomBackground ();

	// Header
	Menu_DrawTitle ("VIDEO OPTIONS", MENU_COLOR_WHITE);
    // Map panel makes the background darker
    Menu_DrawMapPanel();
    // Set value strings
    Menu_Video_SetStrings();

    if (platform_supports_dithering) {
        if (VIDEO_ITEMS < 8) {
            VIDEO_ITEMS++;
        }
    }

    // Show FPS
    Menu_DrawButton (1, 1, "SHOW FPS", MENU_BUTTON_ACTIVE, "Toggles display of FPS Values.");
    Menu_DrawOptionButton (1, fps_string);

    // Max FPS
    Menu_DrawButton (2, 2, "MAX FPS", MENU_BUTTON_ACTIVE, "Sets the max FPS Value.");
    Menu_DrawOptionSlider (2, 60, cl_maxfps, true, true);

    // Field of View
    Menu_DrawButton (3, 3, "FIELD OF VIEW", MENU_BUTTON_ACTIVE, "Change Camera Field of View");
    Menu_DrawOptionSlider (3, 160, scr_fov, false, true);

    // Gamma
    Menu_DrawButton (4, 4, "GAMMA", MENU_BUTTON_ACTIVE, "Adjust game Black Level.");
    Menu_DrawOptionSlider (4, 1, v_gamma, false, false);

    // Particles 
    Menu_DrawButton (5, 5, "PARTICLES", MENU_BUTTON_ACTIVE, "Trade Particle Effects for Performance.");
    Menu_DrawOptionButton (5, particles_string);

    // Retro (texture filtering)
    Menu_DrawButton (6, 6, "RETRO", MENU_BUTTON_ACTIVE, "Toggle Texture filtering.");
    Menu_DrawOptionButton (6, retro_string);

    // Dithering 
    if (platform_supports_dithering) {
        Menu_DrawButton (7, 7, "DITHERING", MENU_BUTTON_ACTIVE, "Toggle Decrease in Color Banding");
        Menu_DrawOptionButton (7, dithering_string);
    }

	Menu_DrawButton(11.5, VIDEO_ITEMS, "BACK", MENU_BUTTON_ACTIVE, "Return to Main Menu.");
}

/*
===============
Menu_Video_Key
===============
*/
void Menu_Video_Key (int key)
{
	switch (key)
	{

	case K_DOWNARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (++menu_video_cursor >= VIDEO_ITEMS)
			menu_video_cursor = 0;
		break;

	case K_UPARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (--menu_video_cursor < 0)
			menu_video_cursor = VIDEO_ITEMS - 1;
		break;

    case K_RIGHTARROW:
        Menu_Video_ApplySliders(MENU_SLIDER_RIGHT);
        break;

    case K_LEFTARROW:
        Menu_Video_ApplySliders(MENU_SLIDER_LEFT);
        break;

	case K_ENTER:
	case K_AUX1:
		Menu_StartSound(MENU_SND_ENTER);
		switch (menu_video_cursor)
		{
			case 0:
                Menu_Video_ApplyShowFPS();
				break;
            case 4:
                Menu_Video_ApplyParticles();
                break;
            case 5:
                Menu_Video_ApplyRetro();
                break;
            case 6:
                if (VIDEO_ITEMS == 8) {
                    Menu_Video_ApplyDithering();
                } else {
                    Menu_Configuration_Set();
                }
                break;
            case 7:
                Menu_Configuration_Set();
                break;
		}
	}
}