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
/* ACCESSIBILITY MENU */

/*
===============
Menu_Accessibility_Set
===============
*/
void Menu_Accessibility_Set (void)
{
	Menu_ResetMenuButtons();
	m_state = m_accessibility;
}

/*
===============
Menu_Accessibility_Draw
===============
*/
void Menu_Accessibility_Draw (void)
{
	// Background
	Menu_DrawCustomBackground (true);
	
	// Header
	Menu_DrawTitle ("ACCESSIBILITY OPTIONS", MENU_COLOR_WHITE);

	Menu_DrawButton(11.5, 0, "BACK", "Return to Main Menu.", Menu_Configuration_Set);
}