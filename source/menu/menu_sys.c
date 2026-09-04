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

int				MENU_KEY_CONFIRM = -1;
int				MENU_KEY_BACK = -1;
int				MENU_KEY_DELETE = -1;
int				MENU_KEY_SAVE_INPUT = -1;

qboolean    	menu_sound_playing;
static int      menu_drag_slider = -1;

extern image_t 	b_topface;
extern image_t 	b_bottomface;
extern image_t 	b_leftface;
extern image_t 	b_rightface;

image_t Menu_GetConfirmIcon(void)
{
#ifdef PLATFORM_USES_GENERIC_GLYPHS
	return HUD_KeyHasIcon(MENU_KEY_CONFIRM) ? 1 : -1;
#else
	switch (MENU_KEY_CONFIRM)
	{
	case K_BOTTOMFACE:
		return b_bottomface;
	case K_RIGHTFACE:
		return b_rightface;
	case K_TOPFACE:
		return b_topface;
	case K_LEFTFACE:
		return b_leftface;
	default:
		return -1;
	}
#endif
}

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
            *x = (vid.width * 0.5) + (*x*vid.scale);
            break;
        case UI_ANCHOR_LEFT:
			// no op
            break;
        case UI_ANCHOR_RIGHT:
            *x = vid.width - (*x*vid.scale);
            break;
    }

	switch (current_frame.point_y) {
        case UI_ANCHOR_CENTER:
            *y = (vid.height * 0.5) + (*y*vid.scale);
            break;
        case UI_ANCHOR_TOP:
			// no op
            break;
        case UI_ANCHOR_BOTTOM:
            *y = vid.height - (*y*vid.scale);
            break;
    }
}

// UI scaling helper functions
// int casting is for NSPIRE platform
// which uses unsigned ints for 
// vid.width/vid.height
int UI_X(int x)
{
    if (x == (int)vid.width) return x;

    //printf("x %i\n", x);
    int ret = (x * vid.scale);
    //printf("ret %i\n", ret);
    return (ret);
}

int UI_Y(int y)
{
    if (y == (int)vid.height) return y;

    //printf("y %i\n", y);
    int ret = (y * vid.scale);
    //printf("ret %i\n", ret);
    return (ret);
}

int UI_W(int w)
{
    if (w == (int)vid.width) return w;
    //printf("w %i\n", w);
    int ret = (w * vid.scale);
    //printf("ret %i\n", ret);
    return (ret);
}

int UI_H(int h)
{
    if (h == (int)vid.height) return h;
    //printf("h %i\n", h);
    int ret = (h * vid.scale);
    //printf("ret %i\n", ret);
    return (ret);
}

void Menu_SetInputDevice (in_device_t device)
{
	if (device == IN_DEVICE_KEYBOARD_MOUSE) {
	MENU_KEY_CONFIRM = K_ENTER;
	MENU_KEY_BACK = K_ESCAPE;
	MENU_KEY_DELETE = K_DELETE;
	MENU_KEY_SAVE_INPUT = K_TAB;
	} else {
#ifdef PLATFORM_CONFIRM_FLIPPED
	MENU_KEY_CONFIRM = K_RIGHTFACE;
	MENU_KEY_BACK = K_BOTTOMFACE;
#else
	MENU_KEY_CONFIRM = K_BOTTOMFACE;
	MENU_KEY_BACK = K_RIGHTFACE;
#endif
	MENU_KEY_DELETE = K_TOPFACE;
	MENU_KEY_SAVE_INPUT = K_LEFTFACE;
	}
}

void Menu_InitUI (void)
{
	current_frame.point_x = 0;
	current_frame.point_y = 0;
	Menu_SetInputDevice(IN_GetActiveDevice());

	// Set OSK button images
	// these are currently the only
	// two platforms which use OSK
	// will be expanded to NSpire once
	// button images are added.
#if defined(__PSP__) || defined (__PSP2__)
	osk_button[0] = b_leftface;
	osk_button[1] = b_rightface;
	osk_button[2] = b_topface;
	osk_button[3] = b_bottomface;
#endif
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

void Menu_MouseMove (int x, int y)
{
	int i;
	if (key_dest != key_menu && key_dest != key_menu_pause) return;
	if (menu_drag_slider >= 0) {
		menu_button_t *slider = &current_menu.button[menu_drag_slider];
		float position = (float)(x - slider->slider_x) / slider->slider_width;
		float value;
		if (position < 0.0f) position = 0.0f;
		if (position > 1.0f) position = 1.0f;
		value = slider->slider_min + position * (slider->slider_max - slider->slider_min);
		if (slider->slider_step > 0.0f)
			value = slider->slider_min + floorf((value - slider->slider_min) / slider->slider_step + 0.5f) * slider->slider_step;
		Cvar_SetValue(slider->slider_cvar, value);
		return;
	}
	for (i = 0; i < MAX_MENU_BUTTONS; ++i) {
		menu_button_t *button = &current_menu.button[i];
		if (!button->enabled) break;
		if ((x >= button->x && x < button->x + button->width &&
			y >= button->y && y < button->y + button->height) ||
			(button->is_slider && x >= button->slider_x && x <= button->slider_x + button->slider_width &&
			y >= button->slider_y && y < button->slider_y + button->slider_height)) {
			if (current_menu.cursor != i) {
				current_menu.cursor = i;
				Menu_SetSound(MENU_SND_NAVIGATE);
			}
			return;
		}
	}
}

qboolean Menu_MouseButton (int x, int y, qboolean down)
{
	menu_button_t *button;
	if (!down) {
		qboolean was_dragging = menu_drag_slider >= 0;
		menu_drag_slider = -1;
		return was_dragging;
	}
	if (current_menu.cursor < 0 || current_menu.cursor >= MAX_MENU_BUTTONS) return false;
	button = &current_menu.button[current_menu.cursor];
	if (!button->enabled || !button->is_slider || x < button->slider_x ||
		x > button->slider_x + button->slider_width || y < button->slider_y ||
		y >= button->slider_y + button->slider_height) return false;
	menu_drag_slider = current_menu.cursor;
	Menu_MouseMove(x, y);
	return true;
}

void Menu_SetPreviousMenu (void)
{
	switch (m_previous_state) {
		case m_main:
			Menu_Main_Set();
			return;
		case m_stockmaps:
			Menu_StockMaps_Set();
			break;
		case m_lobby:
			Menu_Lobby_Set();
			break;
		case m_custommaps:
			Menu_CustomMaps_Set();
			break;
		case m_pause:
			Menu_Pause_Set();
			break;
		case m_configuration:
			Menu_Configuration_Set();
			break;
		case m_controls:
			Menu_Controls_Set();
			break;
		case m_accessibility:
			Menu_Accessibility_Set();
			break;
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
			if (dir == K_LEFTARROW || dir == K_DPAD_LEFT) {
				current_menu.slider_pressed = -1;
			} else if (dir == K_RIGHTARROW || dir == K_DPAD_RIGHT) {
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
	if (in_bind) {
		Menu_WaitForKeybind (key);
		return;
	}

	switch (key)
	{
	case K_DOWNARROW:
	case K_DPAD_DOWN:
		Menu_DecreaseCursor();
		break;

	case K_UPARROW:
	case K_DPAD_UP:
		Menu_IncreaseCursor();
		break;

	case K_LEFTARROW:
	case K_DPAD_LEFT:
		Menu_IncrementSlider(key);
		break;

	case K_RIGHTARROW:
	case K_DPAD_RIGHT:
		Menu_IncrementSlider(key);
		break;
	}

	if(key == MENU_KEY_CONFIRM || key == K_ENTER || key == K_BOTTOMFACE) {
		Menu_ButtonPress();
	}

	if(key == MENU_KEY_BACK || key == K_ESCAPE || key == K_RIGHTFACE ||
		(key == K_START && key_dest == key_menu_pause)) {
		if (m_state == m_pause && menu_paus_submenu == 0)
			Menu_Resume();
		else
			Menu_SetPreviousMenu();
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

	for (j=0 ; j<MAX_KEYS ; j++)
	{
		if (!IN_KeyMatchesActiveDevice(j))
			continue;
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

	for (j=0 ; j<MAX_KEYS ; j++)
	{
		if (!IN_KeyMatchesActiveDevice(j))
			continue;
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_SetBinding (j, "");
	}
}
