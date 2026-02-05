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

menu_t          stockmaps_menu;
menu_button_t   stockmaps_menu_buttons[STOCKMAPS_ITEMS];

void Menu_StockMaps_SetMap (void)
{
    Map_SetDefaultValues();
    current_selected_bsp = stock_maps[stockmaps_menu.cursor].bsp_name;
    Menu_Lobby_Set();
}

void Menu_StockMaps_SetRandomMap (void)
{
    int i = (rand() % num_user_maps);

    Map_SetDefaultValues();
    current_selected_bsp = custom_maps[i].map_name;
    Menu_Lobby_Set();
}

void Menu_StockMaps_SetMainMenu (void) { if (menu_is_solo) {Menu_Main_Set(false);} };

void Menu_StockMaps_BuildMenuItems (void)
{
    int i;
    for (i = 0; i < num_stock_maps; i++) {
        stockmaps_menu_buttons[i] = (menu_button_t){ true, i, Menu_StockMaps_SetMap };
    }
    
    stockmaps_menu_buttons[i] = (menu_button_t){ true, i, Menu_CustomMaps_Set };
    stockmaps_menu_buttons[i+1] = (menu_button_t){ true, i+1, Menu_StockMaps_SetRandomMap };
    stockmaps_menu_buttons[i+2] = (menu_button_t){ true, i+2, Menu_StockMaps_SetMainMenu };

	stockmaps_menu = (menu_t) {
		stockmaps_menu_buttons,
        STOCKMAPS_ITEMS,
		0
	};
}

/*
=======================
Menu_SinglePlayer_Set
=======================
*/
void Menu_StockMaps_Set (void)
{
    Menu_StockMaps_BuildMenuItems();
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
        Menu_DrawMapButton(i+1, &stockmaps_menu, &stockmaps_menu_buttons[i], -1, stock_maps[i].bsp_name);
    }

    Menu_DrawDivider(i+1.25);

    Menu_DrawButton(i+1.5, &stockmaps_menu, &stockmaps_menu_buttons[i], "USER MAPS", "View User-Created Maps.");
    Menu_DrawButton(i+2.5, &stockmaps_menu, &stockmaps_menu_buttons[i+1], "RANDOM", "Feeling indecisive? Try rolling the dice.");

    Menu_DrawButton(i+3.5, &stockmaps_menu, &stockmaps_menu_buttons[i+2], "BACK", back_text);
}

/*
===============
Menu_Main_Key
===============
*/
void Menu_StockMaps_Key (int key)
{
	Menu_KeyInput(key, &stockmaps_menu);
}