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
/* BINDINGS MENU */

int         current_bindings_page = 0;
int         bindings_currentwatch = -1;

qboolean    in_bind = false;
char        *current_command;

/*
===============
Menu_Bindings_Set
===============
*/
void Menu_Bindings_Set (void)
{
	Menu_ResetMenuButtons();

    if (!current_command) {
        current_command = malloc(32*sizeof(char));
    }

	m_state = m_bindings;
}

void Menu_WaitForKeybind (int key)
{
    Menu_UnbindCommand(current_command);
    Key_SetBinding(key, current_command);
    current_command = "";
    in_bind = false;
    bindings_currentwatch = -1;
}

void Menu_Bindings_PrintBindForCommand (int order, char *command)
{
    char    *bind;
    int     keys[2];

    if (bindings_currentwatch == order) {
        Menu_DrawOptionKey(order, "< >");
        current_command = command;
        return;
    }

    Menu_FindKeysForCommand(command, keys);

    if (keys[0] != -1) {
        bind = Key_KeynumToString(keys[0]);
    } else {
        bind = "UNBOUND";
    }

    Menu_DrawOptionKey(order, bind);
}

void Menu_Bindings_SetBindListen (void)
{
    Menu_SetSound(MENU_SND_ENTER);
    in_bind = true;
    bindings_currentwatch = current_menu.cursor+1;
}

void Menu_Bindings_NextPage (void)
{
    current_bindings_page++;
    if (current_bindings_page > 1) current_bindings_page = 1;
    Menu_ResetMenuButtons();
}

void Menu_Bindings_PrevPage (void)
{
    current_bindings_page--;
    if (current_bindings_page < 0) current_bindings_page = 0;
    Menu_ResetMenuButtons();
}

void Menu_Bindings_Apply (void)
{
    // TODO
    // Neccessary??
}

void Menu_Bindings_PrevMenu (void)
{
    current_bindings_page = 0;
    bindings_currentwatch = -1;
    Menu_Controls_Set();
}

/*
===============
Menu_Bindings_Draw
===============
*/
void Menu_Bindings_Draw (void)
{
    int bindings_index = 0;

	// Background
	Menu_DrawCustomBackground (true);

	// Header
	Menu_DrawTitle ("CONTROL BINDINGS", MENU_COLOR_WHITE);

    // Map panel makes the background darker
    Menu_DrawMapPanel();

    // Page 1
    if (current_bindings_page == 0) {
        bindings_index = 0;

        Menu_DrawButton(1, bindings_index++, "JUMP", "", Menu_Bindings_SetBindListen);
        Menu_Bindings_PrintBindForCommand(1, "+jump");

        Menu_DrawButton(2, bindings_index++, "SPRINT", "", Menu_Bindings_SetBindListen);
        Menu_Bindings_PrintBindForCommand(2, "impulse 23");

        Menu_DrawButton(3, bindings_index++, "CHANGE STANCE", "", Menu_Bindings_SetBindListen);
        Menu_Bindings_PrintBindForCommand(3, "impulse 30");
        
        Menu_DrawButton(4, bindings_index++, "SWAP WEAPON", "", Menu_Bindings_SetBindListen);
        Menu_Bindings_PrintBindForCommand(4, "+switch");
        
        Menu_DrawButton(5, bindings_index++, "INTERACT", "", Menu_Bindings_SetBindListen);
        Menu_Bindings_PrintBindForCommand(5, "+use");
        
        Menu_DrawButton(6, bindings_index++, "RELOAD", "", Menu_Bindings_SetBindListen);
        Menu_Bindings_PrintBindForCommand(6, "+reload");
        
        Menu_DrawButton(7, bindings_index++, "MELEE", "", Menu_Bindings_SetBindListen);
        Menu_Bindings_PrintBindForCommand(7, "+knife");

        Menu_DrawDivider(8.25);

        Menu_DrawButton(8.5, bindings_index++, "NEXT PAGE", "Advance to the next Keybind page.", Menu_Bindings_NextPage);
        Menu_DrawGreyButton(9.5, "PREVIOUS PAGE");
    } else {
        // Page 2
        bindings_index = 0;

        Menu_DrawButton(1, bindings_index++, "PRIMARY GRENADE", "", Menu_Bindings_SetBindListen);
        Menu_Bindings_PrintBindForCommand(1, "+grenade");
        
        Menu_DrawButton(2, bindings_index++, "SECONDARY GRENADE", "", Menu_Bindings_SetBindListen);
        Menu_Bindings_PrintBindForCommand(2, "impulse 33");

        Menu_DrawButton(3, bindings_index++, "AIM DOWN SIGHTS", "", Menu_Bindings_SetBindListen);
        Menu_Bindings_PrintBindForCommand(3, "+aim");

        Menu_DrawButton(4, bindings_index++, "SHOOT WEAPON", "", Menu_Bindings_SetBindListen);
        Menu_Bindings_PrintBindForCommand(4, "+attack");

        Menu_DrawDivider(5.25);

        Menu_DrawGreyButton(5.5, "NEXT PAGE");
        Menu_DrawButton(6.5, bindings_index++, "PREVIOUS PAGE", "Return to last Keybind page.", Menu_Bindings_PrevPage);
    }

    Menu_DrawButton(-2, bindings_index++, "APPLY", "Save & Apply Settings.", Menu_Bindings_Apply);
	Menu_DrawButton (-1, bindings_index++, "BACK", "Return to Controls Options.", Menu_Bindings_PrevMenu);
}