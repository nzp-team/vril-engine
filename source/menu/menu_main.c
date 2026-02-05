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
/* MAIN MENU */

#define			MAIN_ITEMS	6

menu_t 			main_menu;
menu_button_t 	main_menu_buttons[MAIN_ITEMS];

void 			Menu_Solo(void)			{ menu_is_solo = true; Menu_StockMaps_Set(); }
void 			Menu_Quit(void)         { Menu_Quit_Set(false); }

void Menu_Main_BuildMenuItems (void)
{
	main_menu_buttons[0] = (menu_button_t){ true, 0, Menu_Solo };
	main_menu_buttons[1] = (menu_button_t){ false, -1, NULL };
	main_menu_buttons[2] = (menu_button_t){ true, 1, Menu_Configuration_Set };
	main_menu_buttons[3] = (menu_button_t){ false, -1, NULL };
	main_menu_buttons[4] = (menu_button_t){ true, 2, Menu_Credits_Set };
	main_menu_buttons[5] = (menu_button_t){ true, 3, Menu_Quit };

	main_menu = (menu_t) {
		main_menu_buttons,
		MAIN_ITEMS,
		0
	};
}

/*
===============
Menu_Main_Set
===============
*/
void Menu_Main_Set (qboolean init)
{
	Menu_LoadPics();
	Menu_Main_BuildMenuItems();

	if (init) {
		Menu_DictateScaleFactor();
		Menu_InitStockMaps();
	}

	key_dest = key_menu;
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

	Menu_DrawButton(1, &main_menu, &main_menu_buttons[0], "SOLO", "Play Solo.");
	Menu_DrawButton(2, &main_menu, &main_menu_buttons[1], "COOPERATIVE", "");

	Menu_DrawDivider(3);

	Menu_DrawButton(3.25, &main_menu, &main_menu_buttons[2], "CONFIGURATION", "Tweak Game Related Options");
	Menu_DrawButton(4.25, &main_menu, &main_menu_buttons[3], "ACHIEVEMENTS", "");

	Menu_DrawDivider(5.25);

	Menu_DrawButton(5.50, &main_menu, &main_menu_buttons[4], "CREDITS", "NZ:P Team + Special Thanks");

	Menu_DrawDivider(6.50);

	Menu_DrawButton(6.75, &main_menu, &main_menu_buttons[5], "QUIT GAME", "Return to Home Screen");

	Menu_DrawSocialBadge (1, MENU_SOC_YOUTUBE);
	Menu_DrawSocialBadge (2, MENU_SOC_BLUESKY);
	Menu_DrawSocialBadge (3, MENU_SOC_PATREON);
	Menu_DrawSocialBadge (4, MENU_SOC_DOCS);
}

/*
===============
Menu_Main_Key
===============
*/
void Menu_Main_Key (int key)
{
	Menu_KeyInput(key, &main_menu);
}