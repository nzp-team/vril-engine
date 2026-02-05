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

#define	CONFIGURATION_ITEMS	6

menu_t 			configuration_menu;
menu_button_t 	configuration_buttons[CONFIGURATION_ITEMS];

void Menu_Configuration_Back (void) 
{
	if (key_dest == key_menu_pause) {
		Menu_Pause_Set();
	} else {
		Menu_Main_Set(false);
	}
}

void Menu_Configuration_BuildMenuItems (void)
{
    configuration_buttons[0] = (menu_button_t){ true, 0, Menu_Video_Set };
    configuration_buttons[1] = (menu_button_t){ true, 1, Menu_Audio_Set };
    configuration_buttons[2] = (menu_button_t){ true, 2, Menu_Controls_Set };
    configuration_buttons[3] = (menu_button_t){ true, 3, Menu_Accessibility_Set };
    configuration_buttons[4] = (menu_button_t){ true, 4, Con_ToggleConsole_f };
    configuration_buttons[5] = (menu_button_t){ true, 5, Menu_Configuration_Back };

	configuration_menu = (menu_t) {
		configuration_buttons,
		CONFIGURATION_ITEMS,
		0
	};
}

/*
===============
Menu_Configuration_Set
===============
*/
void Menu_Configuration_Set (void)
{
	Menu_Configuration_BuildMenuItems();
	m_state = m_configuration;
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

	Menu_DrawButton(1, &configuration_menu, &configuration_buttons[0], "VIDEO", "Visual Fidelity options.");
	Menu_DrawButton(2, &configuration_menu, &configuration_buttons[1], "AUDIO", "Volume sliders.");
	Menu_DrawButton(3, &configuration_menu, &configuration_buttons[2], "CONTROLS", "Control Options and Bindings.");
    Menu_DrawButton(4, &configuration_menu, &configuration_buttons[3], "ACCESSIBILITY", "Content, Interface, and Readability options.");

	Menu_DrawDivider(5);

    Menu_DrawButton(5.25, &configuration_menu, &configuration_buttons[4], "OPEN CONSOLE", "Access the Developer Console.");

	Menu_DrawButton(11.5, &configuration_menu, &configuration_buttons[5], "BACK", "Return to Main Menu.");
}

/*
===============
Menu_Configuration_Key
===============
*/
void Menu_Configuration_Key (int key)
{
	Menu_KeyInput(key, &configuration_menu);
}