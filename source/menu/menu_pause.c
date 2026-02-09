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

// Waypoint saving in pause screen
extern cvar_t 	waypoint_mode;
int 			menu_paus_submenu;

void Menu_Resume(void) { key_dest = key_game; m_state = m_none; };
void Menu_Configuration(void) { Menu_Configuration_Set(); key_dest = key_menu_pause; };
void Menu_Pause_EnterSubMenu(void)
{
	menu_paus_submenu = current_menu.cursor;
	Menu_ResetMenuButtons();
    Menu_SetSound(MENU_SND_ENTER);
}

void Menu_Pause_Yes(void)
{
    if (menu_paus_submenu == 1) {
		menu_paus_submenu = 0;
		key_dest = key_game;
		m_state = m_none;
		// Perform Soft Restart
		PR_ExecuteProgram (pr_global_struct->Soft_Restart);
	} else if (menu_paus_submenu == 3) {
		menu_paus_submenu = 0;
		Cbuf_AddText("disconnect\n");
		Menu_Main_Set(false);
	}

    Menu_Pause_EnterSubMenu();
}

void Menu_Pause_No (void)
{
    Menu_Pause_Set();
}

/*
===============
Menu_Pause_Set
===============
*/
void Menu_Pause_Set (void)
{
	Menu_ResetMenuButtons();
	Menu_SetSound(MENU_SND_ENTER);

	menu_paus_submenu = 0;
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

	if (menu_paus_submenu == 0) {
		// Resume
		Menu_DrawButton (1, 0, "RESUME CARNAGE", "Return to Game.", Menu_Resume);
		// Restart
		Menu_DrawButton (2, 1, "RESTART LEVEL", "Tough luck? Give things another go.", Menu_Pause_EnterSubMenu);
		// Options
		Menu_DrawButton (3, 2, "OPTIONS", "Tweak Game related Options.", Menu_Configuration);
		// End game
		Menu_DrawButton (4, 3, "END GAME", "Return to Main Menu.", Menu_Pause_EnterSubMenu);
	} else {
		Menu_DrawGreyButton (1,"RESUME CARNAGE");
		Menu_DrawGreyButton (2, "RESTART LEVEL");
		Menu_DrawGreyButton (3, "OPTIONS");
		Menu_DrawGreyButton (4, "END GAME");

		// Draw Sub Menu
		if (menu_paus_submenu == 1) {
			Menu_DrawSubMenu("Are you sure you want to restart?", "You will lose any progress that you have made.");
		} else if (menu_paus_submenu == 3) {
			Menu_DrawSubMenu("Are you sure you want to quit?", "You will lose any progress that you have made.");
		}
    
		Menu_DrawButton (6.5, 0, "GET ME OUTTA HERE!", "", Menu_Pause_Yes);
		Menu_DrawButton (7.5, 1, "I WILL PERSEVERE", "", Menu_Pause_No);
	}
	
}