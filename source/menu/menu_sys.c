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
#include <assert.h>

float       ui_scale_x;
float       ui_scale_y;

qboolean    menu_sound_playing;

// Set stock map attributes
StockMaps   stock_maps[8] = {
	[0] = { .bsp_name = "ndu", .category = MAP_CATEGORY_WAW, .array_index = 0 },
	[1] = { .bsp_name = "nzp_warehouse2", .category = MAP_CATEGORY_NZP, .array_index = 0 },
	[2] = { .bsp_name = "nzp_xmas2", .category = MAP_CATEGORY_NZP, .array_index = 0 },
	[3] = { .bsp_name = "nzp_warehouse", .category = MAP_CATEGORY_NZPBETA, .array_index = 0 },
	[4] = { .bsp_name = "christmas_special", .category = MAP_CATEGORY_NZPBETA, .array_index = 0 },
	[5] = { .bsp_name = "lexi_house", .category = MAP_CATEGORY_BLACKOPSDS, .array_index = 0 },
	[6] = { .bsp_name = "lexi_temple", .category = MAP_CATEGORY_BLACKOPSDS, .array_index = 0 },
	[7] = { .bsp_name = "lexi_overlook", .category = MAP_CATEGORY_BLACKOPSDS, .array_index = 0 }
};

// Strip new line for windows and linux (not really a newline but eh)
void strip_newline(char *s)
{
    s[strcspn(s, "\r\n")] = '\0';
}

// Lower string
char* strtolower(char* s) 
{
	assert(s != NULL);

	char* p = s;
	while (*p != '\0') {
		*p = tolower(*p);
		p++;
	}

	return s;
}

// Upper string
char* strtoupper(char* s) 
{
	assert(s != NULL);

	char* p = s;
	while (*p != '\0') {
		*p = toupper(*p);
		p++;
	}

	return s;
}

void Menu_PlaySound (char* sound_path)
{
    menu_sound_playing = true;
    S_LocalSound (sound_path);
    menu_sound_playing = false;
}

/*
======================
Menu_SetSound
======================
*/
void Menu_SetSound (int type)
{
    if (!menu_sound_playing) {
        switch (type) {
            case MENU_SND_NAVIGATE:
                Menu_PlaySound("sounds/menu/navigate.wav");
                break;
            case MENU_SND_ENTER:
                Menu_PlaySound("sounds/menu/enter.wav");
                break;
            case MENU_SND_BEEP:
                Menu_PlaySound("sounds/misc/talk2.wav");
                break;
            default:
                Con_Printf("Unsupported menu sound\n");
                break;
        }
    }
}

void UI_Align (int *x, int *y, int width, int height, int UI_ANCHOR)
{
    switch (UI_ANCHOR) {
        case UI_ANCHOR_CENTER:
            *x = (vid.width - width) * 0.5 + *x;
            *y = (vid.height - height) * 0.5 + *y;
            break;
        case UI_ANCHOR_TOPLEFT:
            break;
        case UI_ANCHOR_TOPRIGHT:
            *x = vid.width - *x - width;
            break;
        case UI_ANCHOR_BOTTOMLEFT:
            *y = vid.height - *y - height;
            break;
        case UI_ANCHOR_BOTTOMRIGHT:
            *x = vid.width - *x - width;
            *y = vid.height - *y - height;
            break;
    }
}

float UI_X(float x)
{
    if (x == (float)vid.width) return vid.width;

    //printf("x %i\n", x);
    float ret = (x * ui_scale_x);
    //printf("ret %i\n", ret);
    return (ret);
}

float UI_Y(float y)
{
    if (y == (float)vid.height) return vid.height;

    //printf("y %i\n", y);
    float ret = (y * ui_scale_y);
    //printf("ret %i\n", ret);
    return (ret);
}

float UI_W(float w)
{
    if (w == (float)vid.width) return vid.width;
    //printf("w %i\n", w);
    float ret = (w * ui_scale_x);
    //printf("ret %i\n", ret);
    return (ret);
}

float UI_H(float h)
{
    if (h == (float)vid.height) return vid.height;
    //printf("h %i\n", h);
    float ret = (h * ui_scale_y);
    //printf("ret %i\n", ret);
    return (ret);
}

void Menu_InitUIScale (void)
{
    // Set scale

    printf("vid.width %i\n", vid.width);
    printf("vid.height %i\n", vid.height);

	ui_scale_x = (float)vid.width/STD_UI_WIDTH;
	ui_scale_y = (float)vid.height/STD_UI_HEIGHT;

    printf("ui_scale_x %f\n", ui_scale_x);
    printf("ui_scale_y %f\n", ui_scale_y);
}

int Menu_GetActiveMenuButtons (void)
{
	int num_active_buttons = 0;

	for (int i = 0; i < MAX_MENU_BUTTONS; i++) {
		if (current_menu.button[i].enabled) {
			num_active_buttons++;
		}
	}

	return num_active_buttons;
}

void Menu_IncreaseCursor (void)
{
	int current_active_buttons = Menu_GetActiveMenuButtons();

	Menu_SetSound(MENU_SND_NAVIGATE);

	if (--current_menu.cursor < 0) {
		current_menu.cursor = current_active_buttons-1;
	}
}

void Menu_DecreaseCursor (void)
{
	int current_active_buttons = Menu_GetActiveMenuButtons();

	Menu_SetSound(MENU_SND_NAVIGATE);

	if (++current_menu.cursor >= current_active_buttons) {
		current_menu.cursor = 0;
	}
}

void Menu_ButtonPress (void)
{
	for (int i = 0; i < MAX_MENU_BUTTONS; i++) {
		if (!current_menu.button[i].enabled) {
			break;
		}

		if (current_menu.cursor == i) {
			if (current_menu.button[i].on_activate != NULL) {
				Menu_SetSound(MENU_SND_ENTER);
				current_menu.button[i].on_activate();
				break;
			}
		}
	}
}

void Menu_IncrementSlider (int dir)
{
	for (int i = 0; i < MAX_MENU_BUTTONS; i++) {
		if (!current_menu.button[i].enabled) {
			break;
		}

		if (current_menu.cursor == i) {
			Menu_SetSound(MENU_SND_NAVIGATE);
			if (dir == K_LEFTARROW) {
				current_menu.slider_pressed = -1;
			} else if (dir == K_RIGHTARROW) {
				current_menu.slider_pressed = 1;
			}
			break;
		}
	}
}

qboolean Menu_IsButtonHovered (int button_index)
{
	if (button_index == current_menu.cursor) {
		return true;
	}

	return false;
}

void Menu_KeyInput (int key)
{
	switch (key)
	{
	case K_DOWNARROW:
		Menu_DecreaseCursor();
		break;

	case K_UPARROW:
		Menu_IncreaseCursor();
		break;

	case K_LEFTARROW:
		Menu_IncrementSlider(key);
		break;

	case K_RIGHTARROW:
		Menu_IncrementSlider(key);
		break;

	case K_ENTER:
	case K_AUX1:
	case K_CROSS:
		Menu_ButtonPress();
		break;
	}
}