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

menuframe_t 	current_frame;
int       		ui_scale;

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

void UI_SetAlignment (int alignment_x, int alignment_y)
{
	current_frame.point_x = alignment_x;
	current_frame.point_y = alignment_y;
}

void UI_Align (int *x, int *y)
{
    switch (current_frame.point_x) {
        case UI_ANCHOR_CENTER:
            *x = (vid.width * 0.5) + *x;
            break;
        case UI_ANCHOR_LEFT:
			// no op
            break;
        case UI_ANCHOR_RIGHT:
            *x = vid.width - *x;
            break;
    }

	switch (current_frame.point_y) {
        case UI_ANCHOR_CENTER:
            *y = (vid.height * 0.5) + *y;
            break;
        case UI_ANCHOR_TOP:
			// no op
            break;
        case UI_ANCHOR_BOTTOM:
            *y = vid.height - *y;
            break;
    }
}

// UI scaling helper functions
// int casting is for NSPIRE platform
// which uses unsigned ints for 
// vid.width/vid.height
int UI_X(int x)
{
    if (x == (int)vid.width) return (int)vid.width;

    //printf("x %i\n", x);
    int ret = (x * ui_scale);
    //printf("ret %i\n", ret);
    return (ret);
}

int UI_Y(int y)
{
    if (y == (int)vid.height) return (int)vid.height;

    //printf("y %i\n", y);
    int ret = (y * ui_scale);
    //printf("ret %i\n", ret);
    return (ret);
}

int UI_W(int w)
{
    if (w == (int)vid.width) return (int)vid.width;
    //printf("w %i\n", w);
    int ret = (w * ui_scale);
    //printf("ret %i\n", ret);
    return (ret);
}

int UI_H(int h)
{
    if (h == (int)vid.height) return (int)vid.height;
    //printf("h %i\n", h);
    int ret = (h * ui_scale);
    //printf("ret %i\n", ret);
    return (ret);
}

void Menu_InitUIScale (void)
{
	current_frame.point_x = 0;
	current_frame.point_y = 0;

	// Set scale
	ui_scale = vid.height/STD_UI_HEIGHT;
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

// Add platform-specific menu enter keys
#ifdef ENTER_IS_CONFIRM
	case K_ENTER:
	Menu_ButtonPress();
		break;
#elif FACE_CONFIRM_IS_FLIPPED
	case K_RIGHTFACE:
		Menu_ButtonPress();
		break;
#else
	case K_BOTTOMFACE:
		Menu_ButtonPress();
		break;
#endif
	}
}

void Menu_FindKeysForCommand (char *command, int *twokeys)
{
	int		count;
	int		j;
	int		l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

void Menu_UnbindCommand (char *command)
{
	int		j;
	int		l;
	char	*b;

	l = strlen(command);

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_SetBinding (j, "");
	}
}