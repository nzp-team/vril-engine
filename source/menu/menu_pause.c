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

#define PAUSE_ITEMS 4

menu_t 			pause_menu;
menu_button_t 	pause_menu_buttons[PAUSE_ITEMS];

// Waypoint saving in pause screen
extern cvar_t waypoint_mode;

void Menu_Pause_Resume(void) { key_dest = key_game; m_state = m_none; };
void Menu_Pause_Configuration(void) { Menu_Configuration_Set(); key_dest = key_menu_pause; };
void Menu_Pause_DecideQuit(void) 
{ 
	if (pause_menu.cursor == 3) {
		Menu_Quit_Set(false);
	} else {
		Menu_Quit_Set(true);
	}
}

void Menu_Pause_BuildMenuItems (void)
{
	pause_menu_buttons[0] = (menu_button_t){ true, 0, Menu_Pause_Resume };
	pause_menu_buttons[1] = (menu_button_t){ true, 1, Menu_Pause_DecideQuit };
	pause_menu_buttons[2] = (menu_button_t){ true, 2, Menu_Pause_Configuration };
	pause_menu_buttons[3] = (menu_button_t){ true, 3, Menu_Pause_DecideQuit };

	pause_menu = (menu_t) {
		pause_menu_buttons,
		PAUSE_ITEMS,
		0
	};
}

/*
===============
Menu_Pause_Set
===============
*/
void Menu_Pause_Set (void)
{
	Menu_Pause_BuildMenuItems();

	loadingScreen = 0;
	loadscreeninit = false;
	key_dest = key_menu_pause;
	m_state = m_pause;
}

/*
===============
Menu_Pause_Draw
===============
*/
void Menu_Pause_Draw (void)
{
    // Background
	Menu_DrawCustomBackground (true);

	// Header
	Menu_DrawTitle ("PAUSED", MENU_COLOR_WHITE);

	// Resume
    Menu_DrawButton (1, &pause_menu, &pause_menu_buttons[0], "RESUME CARNAGE", "Return to Game.");
	// Restart
    Menu_DrawButton (2, &pause_menu, &pause_menu_buttons[1], "RESTART LEVEL", "Tough luck? Give things another go.");
	// Options
    Menu_DrawButton (3, &pause_menu, &pause_menu_buttons[2], "OPTIONS", "Tweak Game related Options.");
	// End game
    Menu_DrawButton (4, &pause_menu, &pause_menu_buttons[3], "END GAME", "Return to Main Menu.");
}

/*
===============
Menu_Pause_Key
===============
*/
void Menu_Pause_Key (int key)
{
	Menu_KeyInput(key, &pause_menu);
}