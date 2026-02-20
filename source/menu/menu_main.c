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

//=============================================================================
/* MAIN MENU */
qboolean in_submenu;

void Menu_Solo(void) { menu_is_solo = true; Menu_StockMaps_Set(); }
void Menu_Quit(void) { key_dest = key_console; Host_Quit_f(); }
void Menu_EnterSubMenu(void) { Menu_ResetMenuButtons(); in_submenu = true; }
void Menu_ExitSubMenu(void) { in_submenu = false; Menu_ResetMenuButtons(); }

/*
===============
Menu_Main_Set
===============
*/
void Menu_Main_Set (void)
{
	loadingScreen = 0;
	loadscreeninit = false;

	Menu_LoadPics();
	Menu_ResetMenuButtons();
	Menu_SetSound(MENU_SND_ENTER);

	key_dest = key_menu;
	m_previous_state = 0;
	m_state = m_main;
	loadingScreen = 0;
}

/*
===============
Menu_Main_Draw
===============
*/
void Menu_Main_Draw (void)
{
	// Background
	Menu_DrawCustomBackground (true);

	// Header
	Menu_DrawTitle ("MAIN MENU", MENU_COLOR_WHITE);

	// Version String
	Menu_DrawBuildDate();

	if (!in_submenu) {
		Menu_DrawButton(1, 0, "SOLO", "Play Solo.", Menu_Solo);
		Menu_DrawGreyButton(2, "COOPERATIVE");

		Menu_DrawDivider(3);

		Menu_DrawButton(3.25, 1, "CONFIGURATION", "Tweak Game Related Options", Menu_Configuration_Set);
		Menu_DrawGreyButton(4.25, "ACHIEVEMENTS");

		Menu_DrawDivider(5.25);

		Menu_DrawButton(5.50, 2, "CREDITS", "NZ:P Team + Special Thanks", Menu_Credits_Set);

		Menu_DrawDivider(6.50);

		Menu_DrawButton(6.75, 3, "QUIT GAME", "Return to Home Screen", Menu_EnterSubMenu);
	} else {
		Menu_DrawGreyButton(1, "SOLO");
		Menu_DrawGreyButton(2, "COOPERATIVE");
		Menu_DrawDivider(3);
		Menu_DrawGreyButton(3.25, "CONFIGURATION");
		Menu_DrawGreyButton(4.25, "ACHIEVEMENTS");
		Menu_DrawDivider(5.25);
		Menu_DrawGreyButton(5.50, "CREDITS");
		Menu_DrawDivider(6.50);
		Menu_DrawGreyButton(6.75, "QUIT GAME");

		// Draw Sub Menu
    	Menu_DrawSubMenu("Are you sure you want to quit?", "You will lose any progress that you have made.");

		Menu_DrawButton (7.5, 0, "GET ME OUTTA HERE!", "", Menu_Quit);
		Menu_DrawButton (8.5, 1, "I WILL PERSEVERE", "", Menu_ExitSubMenu);
	}

	Menu_DrawSocialBadge (1, MENU_SOC_YOUTUBE);
	Menu_DrawSocialBadge (2, MENU_SOC_BLUESKY);
	Menu_DrawSocialBadge (3, MENU_SOC_PATREON);
	Menu_DrawSocialBadge (4, MENU_SOC_DOCS);
}