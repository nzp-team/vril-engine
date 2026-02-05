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
/* CONTROLS MENU */

int	menu_controls_cursor;
#define	MAX_CONTROLS_ITEMS 11

menu_t 			controls_menu;
menu_button_t 	controls_menu_buttons[MAX_CONTROLS_ITEMS];

char* ads_string;
char* invert_string;

void Menu_Controls_AllocStrings (void)
{
    ads_string = malloc(16*sizeof(char));
	invert_string = malloc(16*sizeof(char));
}

void Menu_Controls_FreeStrings (void)
{
    free(ads_string);
	free(invert_string);
}

void Menu_Controls_BuildMenuItems (void)
{
    controls_menu_buttons[0] = (menu_button_t){ true, 0, NULL };
    controls_menu_buttons[1] = (menu_button_t){ true, 1, NULL };
    controls_menu_buttons[2] = (menu_button_t){ true, 2, NULL };
    controls_menu_buttons[3] = (menu_button_t){ true, 3, NULL };
    controls_menu_buttons[4] = (menu_button_t){ true, 4, NULL };
    controls_menu_buttons[5] = (menu_button_t){ true, 5, NULL };
	controls_menu_buttons[6] = (menu_button_t){ true, 6, NULL };
	controls_menu_buttons[7] = (menu_button_t){ false, 6, NULL };
	controls_menu_buttons[8] = (menu_button_t){ false, 6, NULL };
	controls_menu_buttons[9] = (menu_button_t){ false, 6, NULL };
	controls_menu_buttons[10] = (menu_button_t){ false, 6, NULL };

	controls_menu = (menu_t) {
		controls_menu_buttons,
		MAX_CONTROLS_ITEMS,
		0
	};
}

/*
===============
Menu_Controls_Set
===============
*/
void Menu_Controls_Set (void)
{
	Menu_Controls_AllocStrings();
	m_state = m_controls;
}

void Menu_Controls_SetStrings (void)
{

}

/*
===============
Menu_Controls_Draw
===============
*/
void Menu_Controls_Draw (void)
{
	// Background
	Menu_DrawCustomBackground (true);

	// Header
	Menu_DrawTitle ("CONTROL OPTIONS", MENU_COLOR_WHITE);

	// Aim Down Sight 
	Menu_DrawButton (1, &controls_menu, &controls_menu_buttons[0], "AIM DOWN SIGHT", "Switch between Hold and Toggle ADS Modes.");

	// Aim Assist
	Menu_DrawButton (2, &controls_menu, &controls_menu_buttons[1], "AIM ASSIST", "Toggle Assisted-Aim to Improve Targetting.");

	// Look Sensitivity
	Menu_DrawButton (3, &controls_menu, &controls_menu_buttons[2], "LOOK SENSITIVITY", "Alter look Sensitivity.");

	// Look Acceleration
	Menu_DrawButton (4, &controls_menu, &controls_menu_buttons[3], "LOOK ACCELERATION", "Alter look Acceleration.");

	// Look Inversion
	Menu_DrawButton (5, &controls_menu, &controls_menu_buttons[4], "INVERT LOOK", "Invert Y-Axis Camera Input.");

	/*
	PSP SPECIFIC SETTINGS: 

	// A-Nub Tolerance 

	// Left A-Nub Mode

	*/

	// Bindings
	Menu_DrawButton (6, &controls_menu, &controls_menu_buttons[5], "BINDINGS", "Change Input Bindings.");

	Menu_DrawButton (11.5, &controls_menu, &controls_menu_buttons[6], "BACK", "Return to Main Menu.");
}

/*
===============
Menu_Controls_Key
===============
*/
void Menu_Controls_Key (int key)
{
	switch (key)
	{

	case K_DOWNARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (++menu_controls_cursor >= MAX_CONTROLS_ITEMS)
			menu_controls_cursor = 0;
		break;

	case K_UPARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (--menu_controls_cursor < 0)
			menu_controls_cursor = MAX_CONTROLS_ITEMS - 1;
		break;

	case K_ENTER:
	case K_AUX1:
		Menu_StartSound(MENU_SND_ENTER);
		switch (menu_controls_cursor)
		{
			case 0:
				break;
			case 1:
				break;
			case 2:
				break;
			case 3:
				break;
            case 4:
                break;
            case 7:
                Menu_Configuration_Set();
                break;
		}
	}
}