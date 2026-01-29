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
			double current_volume = (double)volume.value;
			if (dir == MENU_SLIDER_LEFT) {
				if (current_volume == 0) return;
				current_volume -= 0.1;
				if (current_volume < 0) current_volume = 0;
				Cvar_SetValue ("volume", (float)current_volume);
			} else if (dir == MENU_SLIDER_RIGHT) {
				current_volume += 0.1;
				if (current_volume > 1) current_volume = 1;
				Cvar_SetValue ("volume", (float)current_volume);
			}
			break;
		case 1:
			double current_bgmvolume = (double)bgmvolume.value;
			if (dir == MENU_SLIDER_LEFT) {
				if (current_bgmvolume == 0) return;
				current_bgmvolume -= 0.1;
				if (current_bgmvolume < 0) current_bgmvolume = 0;
				Cvar_SetValue ("bgmvolume", (float)current_bgmvolume);
			} else if (dir == MENU_SLIDER_RIGHT) {
				current_bgmvolume += 0.1;
				if (current_bgmvolume > 1) current_bgmvolume = 1;
				Cvar_SetValue ("bgmvolume", (float)current_bgmvolume);
			}
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
	Menu_DrawCustomBackground ();

	// Header
	Menu_DrawTitle ("AUDIO OPTIONS", MENU_COLOR_WHITE);

	// Master Volume
	Menu_DrawButton(1, 1, "MASTER VOLUME", MENU_BUTTON_ACTIVE, "Volume for all Audio.");
	Menu_DrawOptionSlider (1, 1, volume, false, false);
	// Music Volume
	Menu_DrawButton(2, 2, "MUSIC VOLUME", MENU_BUTTON_ACTIVE, "Volume for Background Music.");
	Menu_DrawOptionSlider (2, 1, bgmvolume, false, false);

	Menu_DrawButton(11.5, 3, "BACK", MENU_BUTTON_ACTIVE, "Return to Main Menu.");
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