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

#define	MAX_VIDEO_ITEMS	8

menu_t          video_menu;
menu_button_t   video_menu_buttons[MAX_VIDEO_ITEMS];

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

void Menu_Video_ApplySliders (int dir)
{
    switch (video_menu.cursor) {
        // Max FPS
        case 1:
            float current_maxfps = cl_maxfps.value;
            if (dir == MENU_SLIDER_LEFT) {
                if (current_maxfps == 5) break;
                current_maxfps -= 5;   
            } else if (dir == MENU_SLIDER_RIGHT) {
                if (current_maxfps == 60) break;
                current_maxfps += 5;
            }
            if (current_maxfps < 5) current_maxfps = 5;
            if (current_maxfps > 60) current_maxfps = 60;

            Cvar_SetValue ("cl_maxfps", current_maxfps);
            break;
        // Field of View
        case 2:
            float current_fov = scr_fov.value;
            if (dir == MENU_SLIDER_LEFT) {
                if (current_fov == 50) break;
                current_fov -= 5;        
            } else if (dir == MENU_SLIDER_RIGHT) {
                if (current_fov == 160) break;
                current_fov += 5;
            }
            if (current_fov < 50) current_fov = 50;
            if (current_fov > 160) current_fov = 160;

            Cvar_SetValue ("fov", current_fov);
            break;
        // Gamma
        case 3:
            float current_gamma = v_gamma.value;
            if (dir == MENU_SLIDER_LEFT) {
                if (current_gamma == 0.0f) break;
                current_gamma -= 0.1f;
            }
            else if (dir == MENU_SLIDER_RIGHT) {
                if (current_gamma == 1.0f) break;
                current_gamma += 0.1f;
            }
            
            if (current_gamma < 0.0f) current_gamma = 0.0f;
            if (current_gamma > 1.0f) current_gamma = 1.0f;
            
            Cvar_SetValue("gamma", current_gamma);
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

void Menu_Video_BuildMenuItems (void)
{
    video_menu_buttons[0] = (menu_button_t){ true, 0, Menu_Video_ApplyShowFPS };
    video_menu_buttons[1] = (menu_button_t){ true, 1, NULL };
    video_menu_buttons[2] = (menu_button_t){ true, 2, NULL };
    video_menu_buttons[3] = (menu_button_t){ true, 3, NULL };
    video_menu_buttons[4] = (menu_button_t){ true, 4, Menu_Video_ApplyParticles };
    video_menu_buttons[5] = (menu_button_t){ true, 5, Menu_Video_ApplyTextureFiltering };
    if (platform_supports_dithering) {
        video_menu_buttons[6] = (menu_button_t){ true, 6, Menu_Video_ApplyDithering };
    } else {
        video_menu_buttons[6] = (menu_button_t){ false, -1, NULL };
    }
    video_menu_buttons[7] = (menu_button_t){ true, 7, Menu_Configuration_Set };

	video_menu = (menu_t) {
		video_menu_buttons,
        MAX_VIDEO_ITEMS,
		0
	};
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

    Menu_Video_BuildMenuItems();
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
	// Background
	Menu_DrawCustomBackground (true);

	// Header
	Menu_DrawTitle ("VIDEO OPTIONS", MENU_COLOR_WHITE);
    // Map panel makes the background darker
    Menu_DrawMapPanel();
    // Set value strings
    Menu_Video_SetStrings();

    // Show FPS
    Menu_DrawButton (1, &video_menu, &video_menu_buttons[0], "SHOW FPS", "Toggles display of FPS Values."); ;
    Menu_DrawOptionButton (1, fps_string);

    // Max FPS
    Menu_DrawButton (2, &video_menu, &video_menu_buttons[1], "MAX FPS", "Sets the max FPS Value.");
    Menu_DrawOptionSlider (2, 60, cl_maxfps, true, true);

    // Field of View
    Menu_DrawButton (3, &video_menu, &video_menu_buttons[2], "FIELD OF VIEW", "Change Camera Field of View");
    Menu_DrawOptionSlider (3, 160, scr_fov, false, true);

    // Gamma
    Menu_DrawButton (4, &video_menu, &video_menu_buttons[3], "GAMMA", "Adjust game Black Level.");
    Menu_DrawOptionSlider (4, 1, v_gamma, false, false);

    // Particles 
    Menu_DrawButton (5, &video_menu, &video_menu_buttons[4], "PARTICLES", "Trade Particle Effects for Performance.");
    Menu_DrawOptionButton (5, particles_string);

    // Retro (texture filtering)
    Menu_DrawButton (6, &video_menu, &video_menu_buttons[5], "TEXTURE FILTERING", "Choose 3D Environment Filtering Mode.");
    Menu_DrawOptionButton (6, retro_string);

    // Dithering 
    if (platform_supports_dithering) {

        Menu_DrawButton (7, &video_menu, &video_menu_buttons[6], "DITHERING", "Toggle Decrease in Color Banding");
        Menu_DrawOptionButton (7, dithering_string);
    }

	Menu_DrawButton(11.5, &video_menu, &video_menu_buttons[7], "BACK", "Return to Main Menu.");
}

/*
===============
Menu_Video_Key
===============
*/
void Menu_Video_Key (int key)
{
    switch (key) {
        case K_LEFTARROW:
            Menu_Video_ApplySliders(MENU_SLIDER_LEFT);
            break;
        case K_RIGHTARROW:
            Menu_Video_ApplySliders(MENU_SLIDER_RIGHT);
            break;
    }

	Menu_KeyInput(key, &video_menu);
}