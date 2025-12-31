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
#include "menu_dummy.h"

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
	m_entersound = true;
}

/*
===============
Menu_Main_Draw
===============
*/
void Menu_Main_Draw (void)
{
	// Background
	Menu_DrawCustomBackground();

	// Header
	Menu_DrawTitle ("MAIN MENU");

	// Version String
	Menu_DrawBuildDate();

	// Solo
	if (m_main_cursor == 0)
		Menu_DrawButton(1, "SOLO", MENU_BUTTON_ACTIVE, BUTTON_SELECTED, "Play Solo.");
	else
		Menu_DrawButton(1, "SOLO", MENU_BUTTON_ACTIVE, BUTTON_DESELECTED, "");

	Menu_DrawButton(2, "COOPERATIVE (Coming Soon!)", MENU_BUTTON_INACTIVE, BUTTON_DESELECTED, "");

	Menu_DrawDivider(3);

	if (m_main_cursor == 1)
		Menu_DrawButton(4, "CONFIGURATION", MENU_BUTTON_ACTIVE, BUTTON_SELECTED, "Tweak Game Related Options");
	else
		Menu_DrawButton(4, "CONFIGURATION", MENU_BUTTON_ACTIVE, BUTTON_DESELECTED, "");;

	Menu_DrawButton(5, "ACHIEVEMENTS", MENU_BUTTON_INACTIVE, BUTTON_DESELECTED, "");

	Menu_DrawDivider(6);

	if (m_main_cursor == 2)
		Menu_DrawButton(7, "CREDITS", MENU_BUTTON_ACTIVE, BUTTON_SELECTED, "NZ:P Team + Special Thanks");
	else
		Menu_DrawButton(7, "CREDITS", MENU_BUTTON_ACTIVE, BUTTON_DESELECTED, "");

	Menu_DrawDivider(8);

	if (m_main_cursor == 3)
		Menu_DrawButton(9, "QUIT GAME", MENU_BUTTON_ACTIVE, BUTTON_SELECTED, "Return to Home Screen");
	else
		Menu_DrawButton(9, "QUIT GAME", MENU_BUTTON_ACTIVE, BUTTON_DESELECTED, "");

	// Descriptions
	switch(m_main_cursor) {
		case 0: // Solo
			Draw_ColoredString(5, 220, "Take on the Hordes by yourself.", 255, 255, 255, 255, 1);
			break;
		case 1: // Settings
			Draw_ColoredString(5, 220, "Adjust your Settings to Optimize your Experience.", 255, 255, 255, 255, 1);
			break;
		case 2: // Credits
			Draw_ColoredString(5, 220, "See who made NZ:P possible.", 255, 255, 255, 255, 1);
			break;
		case 3: // Exit
			Draw_ColoredString(5, 220, "Return to Home Menu.", 255, 255, 255, 255, 1);
			break;
	}
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
		m_entersound = true;
		Menu_StartSound(MENU_SND_ENTER);
		switch (m_main_cursor)
		{
		case 0:
			Menu_SinglePlayer_Set ();
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