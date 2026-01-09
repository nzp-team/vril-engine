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

int	m_main_cursor;
#define	MAIN_ITEMS	4

/*
===============
Menu_Main_Set
===============
*/
void Menu_Main_Set (void)
{
	key_dest = key_menu;
	m_state = m_main;
}

/*
===============
Menu_Main_Draw
===============
*/
void Menu_Main_Draw (void)
{
	// Background
	Menu_DrawCustomBackground ();

	// Header
	Menu_DrawTitle ("MAIN MENU");

	// Version String
	Menu_DrawBuildDate();

	Menu_DrawButton(1, 1, "SOLO", MENU_BUTTON_ACTIVE, "Play Solo.");
	Menu_DrawButton(2, 0, "COOPERATIVE", MENU_BUTTON_INACTIVE, "");

	Menu_DrawDivider(3);

	Menu_DrawButton(3.25, 2, "CONFIGURATION", MENU_BUTTON_ACTIVE, "Tweak Game Related Options");
	Menu_DrawButton(4.25, 0, "ACHIEVEMENTS", MENU_BUTTON_INACTIVE, "");

	Menu_DrawDivider(5.25);

	Menu_DrawButton(5.50, 3, "CREDITS", MENU_BUTTON_ACTIVE, "NZ:P Team + Special Thanks");

	Menu_DrawDivider(6.50);

	Menu_DrawButton(6.75, 4, "QUIT GAME", MENU_BUTTON_ACTIVE, "Return to Home Screen");

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
	switch (key)
	{

	case K_DOWNARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_UPARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_ENTER:
	case K_AUX1:
		Menu_StartSound(MENU_SND_ENTER);
		switch (m_main_cursor)
		{
			case 0:
				menu_is_solo = true;
				Menu_StockMaps_Set ();
				break;

			case 1:
				Menu_Options_Set ();
				break;

			case 2:
				Menu_Credits_Set ();
				break;

			case 3:
				Menu_Quit_Set ();
				break;
		}
	}
}