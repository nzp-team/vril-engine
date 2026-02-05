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
/* AUDIO MENU */

int	menu_audio_cursor;
#define	AUDIO_ITEMS	3

extern cvar_t volume;
extern cvar_t bgmvolume;

menu_t 			audio_menu;
menu_button_t 	audio_menu_buttons[AUDIO_ITEMS];

/*
===============
Menu_Audio_Set
===============
*/
void Menu_Audio_Set (void)
{
	m_state = m_audio;
}

void Menu_Audio_ApplySliders (int dir)
{
	switch (menu_audio_cursor) {
		case 0:
			float current_volume = volume.value;
			if (dir == MENU_SLIDER_LEFT) {
				if (current_volume == 0) break;
				current_volume -= 0.1f;
			} else if (dir == MENU_SLIDER_RIGHT) {
				if (current_volume == 1) break;
				current_volume += 0.1f;
			}
			if (current_volume < 0) current_volume = 0;
			if (current_volume > 1) current_volume = 1;

			Cvar_SetValue ("volume", current_volume);
			break;
		case 1:
			float current_bgmvolume = bgmvolume.value;
			if (dir == MENU_SLIDER_LEFT) {
				if (current_bgmvolume == 0) break;
				current_bgmvolume -= 0.1f;
			} else if (dir == MENU_SLIDER_RIGHT) {
				if (current_bgmvolume == 1) break;
				current_bgmvolume += 0.1f;
			}
			if (current_bgmvolume < 0) current_bgmvolume = 0;
			if (current_bgmvolume > 1) current_bgmvolume = 1;

			Cvar_SetValue ("bgmvolume", current_bgmvolume);
			break;
	}
}

/*
===============
Menu_Audio_Draw
===============
*/
void Menu_Audio_Draw (void)
{
	// Background
	Menu_DrawCustomBackground (true);

	// Header
	Menu_DrawTitle ("AUDIO OPTIONS", MENU_COLOR_WHITE);

	// Master Volume
	Menu_DrawButton(1, &audio_menu, &audio_menu_buttons[0], "MASTER VOLUME", "Volume for all Audio.");
	Menu_DrawOptionSlider (1, 1, volume, false, false);
	// Music Volume
	Menu_DrawButton(2, &audio_menu, &audio_menu_buttons[1], "MUSIC VOLUME", "Volume for Background Music.");
	Menu_DrawOptionSlider (2, 1, bgmvolume, false, false);

	Menu_DrawButton(11.5, &audio_menu, &audio_menu_buttons[2], "BACK", "Return to Main Menu.");
}

/*
===============
Menu_Audio_Key
===============
*/
void Menu_Audio_Key (int key)
{
	switch (key)
	{

	case K_DOWNARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (++menu_audio_cursor >= AUDIO_ITEMS)
			menu_audio_cursor = 0;
		break;

	case K_UPARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (--menu_audio_cursor < 0)
			menu_audio_cursor = AUDIO_ITEMS - 1;
		break;

	case K_RIGHTARROW:
        Menu_Audio_ApplySliders(MENU_SLIDER_RIGHT);
        break;

    case K_LEFTARROW:
        Menu_Audio_ApplySliders(MENU_SLIDER_LEFT);
        break;

	case K_ENTER:
	case K_AUX1:
		Menu_StartSound(MENU_SND_ENTER);
		switch (menu_audio_cursor)
		{
			case 2:
				Menu_Configuration_Set();
				break;
		}
	}
}