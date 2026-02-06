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
	Menu_ResetMenuButtons();
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
	Menu_DrawButton (1, 0, "AIM DOWN SIGHT", "Switch between Hold and Toggle ADS Modes.", NULL);

	// Aim Assist
	Menu_DrawButton (2, 1, "AIM ASSIST", "Toggle Assisted-Aim to Improve Targetting.", NULL);

	// Look Sensitivity
	Menu_DrawButton (3, 2, "LOOK SENSITIVITY", "Alter look Sensitivity.", NULL);

	// Look Acceleration
	Menu_DrawButton (4, 3, "LOOK ACCELERATION", "Alter look Acceleration.", NULL);

	// Look Inversion
	Menu_DrawButton (5, 4, "INVERT LOOK", "Invert Y-Axis Camera Input.", NULL);

	/*
	PSP SPECIFIC SETTINGS: 

	// A-Nub Tolerance 

	// Left A-Nub Mode

	*/

	// Bindings
	Menu_DrawButton (6, 5, "BINDINGS", "Change Input Bindings.", NULL);

	Menu_DrawButton (11.5, 6, "BACK", "Return to Main Menu.", Menu_Configuration_Set);
}