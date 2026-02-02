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
/* PAUSE MENU */

int menu_paused_cursor;
#define PAUSED_ITEMS 4

// Waypoint saving in pause screen
extern cvar_t waypoint_mode;

/*
===============
Menu_Paused_Set
===============
*/
void Menu_Paused_Set ()
{
	key_dest = key_menu_pause;
	m_state = m_paused;
	loadingScreen = 0;
	loadscreeninit = false;
	menu_paused_cursor = 0;
}

/*
===============
Menu_Paused_Draw
===============
*/
void Menu_Paused_Draw ()
{
    // Background
	Menu_DrawCustomBackground (true);

	// Header
	Menu_DrawTitle ("PAUSED", MENU_COLOR_WHITE);

	// Resume
    Menu_DrawButton (1, 1, "RESUME CARNAGE", MENU_BUTTON_ACTIVE, "Return to Game.");
	// Restart
    Menu_DrawButton (2, 2, "RESTART LEVEL", MENU_BUTTON_ACTIVE, "Tough luck? Give things another go.");
	// Options
    Menu_DrawButton (3, 3, "OPTIONS", MENU_BUTTON_ACTIVE, "Tweak Game related Options.");
	// End game
    Menu_DrawButton (4, 4, "END GAME", MENU_BUTTON_ACTIVE, "Return to Main Menu.");
}

/*
===============
Menu_Paused_Key
===============
*/
void Menu_Paused_Key (int key)
{
	switch (key)
	{
		case K_ESCAPE:
		case K_AUX2:
			Menu_StartSound(MENU_SND_ENTER);
			Cbuf_AddText("togglemenu\n");
			break;

		case K_DOWNARROW:
			Menu_StartSound(MENU_SND_NAVIGATE);
			if (++menu_paused_cursor >= PAUSED_ITEMS)
				menu_paused_cursor = 0;
			break;

		case K_UPARROW:
			Menu_StartSound(MENU_SND_NAVIGATE);
			if (--menu_paused_cursor < 0)
				menu_paused_cursor = PAUSED_ITEMS - 1;
			break;

		case K_ENTER:
		case K_AUX1:
			switch (menu_paused_cursor)
			{
			case 0:
				key_dest = key_game;
				m_state = m_none;
				break;
			case 1:
				Menu_Quit_Set(true);
				break;
			case 2:
				Menu_Configuration_Set();
				key_dest = key_menu_pause;
				break;
			case 3:
				Menu_Quit_Set(false);
				break;
			}
		}
}