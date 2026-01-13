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
/* GAME SETTINGS MENU */

int	menu_gamesettings_cursor;
#define GAMESETTINGS_ITEMS 8

void Menu_GameSettings_Set (void)
{
    key_dest = key_menu;
	m_state = m_gamesettings;
}

void Menu_GameSettings_Draw (void)
{
    // Background
	Menu_DrawCustomBackground ();
    // Title
    Menu_DrawTitle("GAME SETTINGS", MENU_COLOR_WHITE);
    // Map panel makes the background darker
    Menu_DrawMapPanel();


}

void Menu_GameSettings_Key (int key)
{
    switch (key)
	{

	case K_DOWNARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (++menu_gamesettings_cursor >= GAMESETTINGS_ITEMS)
			menu_gamesettings_cursor = 0;
		break;

	case K_UPARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (--menu_gamesettings_cursor < 0)
			menu_gamesettings_cursor = GAMESETTINGS_ITEMS - 1;
		break;

    case K_ENTER:
	case K_AUX1:
		Menu_StartSound(MENU_SND_ENTER);
        switch (menu_gamesettings_cursor)
        {
            case 0:
                break;
            case 1:
                break;
            case 8:
                Menu_Lobby_Set();
                break;
        }
    }
}