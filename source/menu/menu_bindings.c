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
/* BINDINGS MENU */

/*
===============
Menu_Bindings_Set
===============
*/
void Menu_Bindings_Set (void)
{
	Menu_ResetMenuButtons();
	m_state = m_bindings;
}

/*
===============
Menu_Bindings_Draw
===============
*/
void Menu_Bindings_Draw (void)
{
	// Background
	Menu_DrawCustomBackground (true);

	// Header
	Menu_DrawTitle ("CONTROL BINDINGS", MENU_COLOR_WHITE);

    // Map panel makes the background darker
    Menu_DrawMapPanel();

    Menu_DrawButton(1, 0, "JUMP", "", NULL);
    //Menu_Bindings_PrintBindForCommand(1, "impulse 10");

    Menu_DrawButton(2, 1, "SPRINT", "", NULL);
    //Menu_Bindings_PrintBindForCommand(2, "impulse 23");

    Menu_DrawButton(3, 2, "CHANGE STANCE", "", NULL);
    //Menu_Bindings_PrintBindForCommand(3, "impulse 30");
    
    Menu_DrawButton(4, 3, "SWAP WEAPON", "", NULL);
    //Menu_Bindings_PrintBindForCommand(4, "+switch");
    
    Menu_DrawButton(5, 4, "INTERACT", "", NULL);
    //Menu_Bindings_PrintBindForCommand(5, "+use");
    
    Menu_DrawButton(6, 5, "RELOAD", "", NULL);
    //Menu_Bindings_PrintBindForCommand(6, "+reload");
    
    Menu_DrawButton(7, 6, "MELEE", "", NULL);
    //Menu_Bindings_PrintBindForCommand(7, "+knife");
    
    Menu_DrawButton(8, 7, "PRIMARY GRENADE", "", NULL);
    //Menu_Bindings_PrintBindForCommand(8, "+grenade");
    
    Menu_DrawButton(9, 8, "SECONDARY GRENADE", "", NULL);
    //Menu_Bindings_PrintBindForCommand(9, "impulse 33");

    Menu_DrawButton(10, 9, "WEAPON FIRE", "", NULL);
    //Menu_Bindings_PrintBindForCommand(10, "+attack");

    Menu_DrawButton(11, 10, "AIM DOWN SIGHTS", "", NULL);
    //Menu_Bindings_PrintBindForCommand(11, "+aim");

    Menu_DrawDivider(12.25);
    Menu_DrawButton(-2, 11, "APPLY", "Save & Apply Settings.", NULL);

	Menu_DrawButton (-1, 12, "BACK", "Return to Controls Options.", Menu_Controls_Set);
}