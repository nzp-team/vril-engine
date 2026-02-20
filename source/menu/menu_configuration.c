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
/* CONFIGURATION MENU */

void Menu_Configuration_Back (void) 
{
	if (key_dest == key_menu_pause) {
		Menu_Pause_Set();
	} else {
		Menu_Main_Set();
	}
}

/*
===============
Menu_Configuration_Set
===============
*/
void Menu_Configuration_Set (void)
{
	Menu_ResetMenuButtons();
	if (key_dest == key_menu_pause) {
		m_previous_state = m_pause;
	} else {
		m_previous_state = m_main;
	}
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

	Menu_DrawButton(1, 0, "VIDEO", "Visual Fidelity options.", Menu_Video_Set);
	Menu_DrawButton(2, 1, "AUDIO", "Volume sliders.", Menu_Audio_Set);
	Menu_DrawButton(3, 2, "CONTROLS", "Control Options and Bindings.", Menu_Controls_Set);
    Menu_DrawButton(4, 3, "ACCESSIBILITY", "Content, Interface, and Readability options.", Menu_Accessibility_Set);

	Menu_DrawDivider(5);

    Menu_DrawButton(5.25, 4, "OPEN CONSOLE", "Access the Developer Console.", Con_ToggleConsole_f);

	Menu_DrawButton(-1, 5, "BACK", "Return to Main Menu.", Menu_Configuration_Back);
}