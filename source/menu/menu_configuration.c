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
/* CONFIGURATION MENU */

int	menu_configuration_cursor;
#define	CONFIGURATION_ITEMS	6

/*
===============
Menu_Configuration_Set
===============
*/
void Menu_Configuration_Set (void)
{
	m_state = m_configuration;
	menu_configuration_cursor = 0;
}

/*
===============
Menu_Configuration_Draw
===============
*/
void Menu_Configuration_Draw (void)
{
	// Background
	Menu_DrawCustomBackground (true);

	// Header
	Menu_DrawTitle ("CONFIGURATION", MENU_COLOR_WHITE);

	Menu_DrawButton(1, 1, "VIDEO", MENU_BUTTON_ACTIVE, "Visual Fidelity options.");
	Menu_DrawButton(2, 2, "AUDIO", MENU_BUTTON_ACTIVE, "Volume sliders.");
	Menu_DrawButton(3, 3, "CONTROLS", MENU_BUTTON_ACTIVE, "Control Options and Bindings.");
    Menu_DrawButton(4, 4, "ACCESSIBILITY", MENU_BUTTON_ACTIVE, "Content, Interface, and Readability options.");

	Menu_DrawDivider(5);

    Menu_DrawButton(5.25, 5, "OPEN CONSOLE", MENU_BUTTON_ACTIVE, "Access the Developer Console.");

	Menu_DrawButton(11.5, 6, "BACK", MENU_BUTTON_ACTIVE, "Return to Main Menu.");
}

/*
===============
Menu_Configuration_Key
===============
*/
void Menu_Configuration_Key (int key)
{
	switch (key)
	{

	case K_DOWNARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (++menu_configuration_cursor >= CONFIGURATION_ITEMS)
			menu_configuration_cursor = 0;
		break;

	case K_UPARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (--menu_configuration_cursor < 0)
			menu_configuration_cursor = CONFIGURATION_ITEMS - 1;
		break;

	case K_ENTER:
	case K_AUX1:
		Menu_StartSound(MENU_SND_ENTER);
		switch (menu_configuration_cursor)
		{
			case 0:
				Menu_Video_Set ();
				break;
			case 1:
				Menu_Audio_Set ();
				break;
			case 2:
				Menu_Controls_Set ();
				break;
			case 3:
				Menu_Accessibility_Set ();
				break;
            case 4:
                Con_ToggleConsole_f ();
                break;
            case 5:
                if (key_dest == key_menu_pause) {
                    Menu_Paused_Set();
                } else {
                    Menu_Main_Set(false);
                }
                break;
		}
	}
}