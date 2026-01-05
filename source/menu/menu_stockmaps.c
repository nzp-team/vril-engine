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
/* STOCK MAPS MENU */

int	m_stockmaps_cursor;
#define	STOCKMAPS_ITEMS 7

/*
=======================
Menu_SinglePlayer_Set
=======================
*/
void Menu_StockMaps_Set (void)
{
    key_dest = key_menu;
	m_state = m_stockmaps;
}

/*
=======================
Menu_SinglePlayer_Draw
=======================
*/
void Menu_StockMaps_Draw (void)
{
    char* header_text;
    char* back_text;

    // Background
	Menu_DrawCustomBackground ();

    if (menu_is_solo) {
        header_text = "SELECT MAP: SOLO";
        back_text = "Return to Main Menu.";
    } else {
        header_text = "SELECT MAP: COOP";
        back_text = "Return to Create Game Menu.";
    }

    // Header
	Menu_DrawTitle(header_text);

    Menu_DrawMapPanel();

}

/*
===============
Menu_Main_Key
===============
*/
void Menu_StockMaps_Key (int key)
{
	switch (key)
	{

	case K_DOWNARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (++m_stockmaps_cursor >= STOCKMAPS_ITEMS)
			m_stockmaps_cursor = 0;
		break;

	case K_UPARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (--m_stockmaps_cursor < 0)
			m_stockmaps_cursor = STOCKMAPS_ITEMS - 1;
		break;

	case K_ENTER:
	case K_AUX1:
		Menu_StartSound(MENU_SND_ENTER);
		switch (m_stockmaps_cursor)
		{
            case 0:
                map_loadname = "ndu";
                map_loadname_pretty = "Nacht der Untoten";
                Menu_Lobby_Set();
                break;
            case 1:
                map_loadname = "nzp_warehouse";
                map_loadname_pretty = "Warehouse (Classic)";
                Menu_Lobby_Set();
                break;
            case 2:
                map_loadname = "nzp_warehouse2";
                map_loadname_pretty = "Warehouse";
                Menu_Lobby_Set();
                break;
            case 3:
                map_loadname = "christmas_special";
                map_loadname_pretty = "Christmas Special";
                Menu_Lobby_Set();
                break;
            case 4:
                // Custom map menu
                //Menu_CustomMaps_Set();
				break;
            case 5:
                // Random map
                //Menu_ChooseRandomMap();
                //Menu_Lobby_Set();
                break;
            case 6:
                if (menu_is_solo) {
                    Menu_Main_Set();
                }
                break;
		}
	}
}