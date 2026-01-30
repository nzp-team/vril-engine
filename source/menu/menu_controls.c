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
int	CONTROLS_ITEMS;

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
	Menu_DrawCustomBackground ();

	CONTROLS_ITEMS = 6;

	// Header
	Menu_DrawTitle ("CONTROL OPTIONS", MENU_COLOR_WHITE);

	// Aim Down Sight 
	Menu_DrawButton (1, 1, "AIM DOWN SIGHT", MENU_BUTTON_ACTIVE, "Switch between Hold and Toggle ADS Modes.");

	// Aim Assist
	Menu_DrawButton (2, 2, "AIM ASSIST", MENU_BUTTON_ACTIVE, "Toggle Assisted-Aim to Improve Targetting.");

	// Look Sensitivity
	Menu_DrawButton (3, 3, "LOOK SENSITIVITY", MENU_BUTTON_ACTIVE, "Alter look Sensitivity.");

	// Look Acceleration
	Menu_DrawButton (4, 4, "LOOK ACCELERATION", MENU_BUTTON_ACTIVE, "Alter look Acceleration.");

	// Look Inversion
	Menu_DrawButton (5, 5, "INVERT LOOK", MENU_BUTTON_ACTIVE, "Invert Y-Axis Camera Input.");

	/*
	PSP SPECIFIC SETTINGS: 

	// A-Nub Tolerance 

	// Left A-Nub Mode

	*/

	// Bindings
	Menu_DrawButton (6, 6, "BINDINGS", MENU_BUTTON_ACTIVE, "Change Input Bindings.");

	Menu_DrawButton(11.5, CONTROLS_ITEMS, "BACK", MENU_BUTTON_ACTIVE, "Return to Main Menu.");
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
		if (++menu_controls_cursor >= CONTROLS_ITEMS)
			menu_controls_cursor = 0;
		break;

	case K_UPARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (--menu_controls_cursor < 0)
			menu_controls_cursor = CONTROLS_ITEMS - 1;
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