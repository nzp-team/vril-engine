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

int	menu_stockmaps_cursor;
#define	STOCKMAPS_ITEMS 11

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
    int     i;
    char*   header_text;
    char*   back_text;

    // Background
	Menu_DrawCustomBackground(true);

    if (menu_is_solo) {
        header_text = "SELECT MAP: SOLO";
        back_text = "Return to Main Menu.";
    } else {
        header_text = "SELECT MAP: COOP";
        back_text = "Return to Create Game Menu.";
    }

    // Header
	Menu_DrawTitle(header_text, MENU_COLOR_WHITE);
    // Map panel makes the background darker
    Menu_DrawMapPanel();

    for (i = 0; i < num_stock_maps; i++) {
        Menu_DrawMapButton(i+1, i+1, -1, stock_maps[i].bsp_name);
    }

    Menu_DrawDivider(i+1.25);

    Menu_DrawButton(i+1.5, i+1, "USER MAPS", MENU_BUTTON_ACTIVE, "View User-Created Maps.");
    Menu_DrawButton(i+2.5, i+2, "RANDOM", MENU_BUTTON_ACTIVE, "Feeling indecisive? Try rolling the dice.");

    Menu_DrawButton(i+3.5, i+3, "BACK", MENU_BUTTON_ACTIVE, back_text);
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
		if (++menu_stockmaps_cursor >= STOCKMAPS_ITEMS)
			menu_stockmaps_cursor = 0;
		break;

	case K_UPARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (--menu_stockmaps_cursor < 0)
			menu_stockmaps_cursor = STOCKMAPS_ITEMS - 1;
		break;

	case K_ENTER:
	case K_AUX1:
		Menu_StartSound(MENU_SND_ENTER);
		switch (menu_stockmaps_cursor)
		{
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                Map_SetDefaultValues();
                current_selected_bsp = stock_maps[menu_stockmaps_cursor].bsp_name;
                Menu_Lobby_Set();
                break;
            case 8:
                // Custom map menu
                Menu_CustomMaps_Set();
				break;
            case 9:
                // Random map
                //Menu_ChooseRandomMap();
                Menu_Lobby_Set();
                break;
            case 10:
                if (menu_is_solo) {
                    Menu_Main_Set(false);
                }
                break;
		}
	}
}