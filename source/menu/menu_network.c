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
/* NETWORK MENU */

void Menu_Network_Set (void)
{
    Menu_ResetMenuButtons();

    key_dest = key_menu;
    m_previous_state = m_main;
	m_state = m_network;
}

void Menu_Network_JoinGame (void)
{
    Cbuf_AddText("connect 10.0.0.136\n");
}

void Menu_Network_CreateGame (void)
{
    char mapname[32];

    snprintf(mapname, sizeof(mapname), "%s\n", "ndu");

    Menu_LoadMap(mapname, false);
    Menu_ResetMenuButtons();
}

void Menu_Network_Draw (void)
{
	Menu_DrawCustomBackground(true);
    Menu_DrawTitle("NETWORKING DEBUG", MENU_COLOR_WHITE);
    Menu_DrawMapPanel();


    Menu_DrawDivider(3.5);

    Menu_DrawButton(4, 0, "JOIN GAME", "", Menu_Network_JoinGame);
    Menu_DrawButton(5, 1, "CREATE GAME", "", Menu_Network_CreateGame);

    Menu_DrawButton(-1, 2, "BACK", "Return to Main Menu.", Menu_Main_Set);
}