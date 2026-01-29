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

#define MAX_CUSTOMMAP_ITEMS		11

image_t             	menu_usermap_image[MAX_CUSTOMMAPS];
achievement_list_t  	achievement_list[MAX_ACHIEVEMENTS];
usermap_t           	custom_maps[MAX_CUSTOMMAPS];

char    user_levels[256][MAX_QPATH];

int		menu_custommaps_cursor;
int		CUSTOMMAPS_ITEMS;

int		user_maps_page;
int     num_user_maps = 0;
int     num_custom_images = 0;
int     custom_map_pages = 0;

void Menu_CustomMaps_Set ()
{
	key_dest = key_menu;
	m_state = m_custommaps;

	user_maps_page = 0;
}

void Menu_CustomMaps_NextPage (void)
{
    Menu_StartSound(MENU_SND_ENTER);
	menu_custommaps_cursor = 0;
    user_maps_page++;
}

void Menu_CustomMaps_PrevPage (void)
{
    Menu_StartSound(MENU_SND_ENTER);
	menu_custommaps_cursor = 0;
    user_maps_page--;
}

void Menu_CustomMaps_IncreaseItems (void)
{
	if (CUSTOMMAPS_ITEMS < MAX_CUSTOMMAP_ITEMS) {
		CUSTOMMAPS_ITEMS++;
	}
}

void Menu_CustomMaps_DecreaseItems (void)
{
	if (CUSTOMMAPS_ITEMS > (MAX_CUSTOMMAP_ITEMS-1)) {
		CUSTOMMAPS_ITEMS--;
	}
}

void Menu_CustomMaps_Draw (void)
{
	int     i;
    char*   header_text;

    // Background
	Menu_DrawCustomBackground();

    if (menu_is_solo) {
        header_text = "SELECT MAP: SOLO";
    } else {
        header_text = "SELECT MAP: COOP";
    }

    // Header
	Menu_DrawTitle(header_text, MENU_COLOR_WHITE);
    // Map panel makes the background darker
    Menu_DrawMapPanel();

	// calculate the amount of usermaps we can display on this page.
	int maps_on_page = 8; // default to 8, all that will fit on the UI.
	int maps_start_position = (user_maps_page * maps_on_page);

	// Not all 8 slots can be filled..
	if (maps_on_page > num_user_maps - maps_start_position)
		maps_on_page = num_user_maps - maps_start_position;

	for (i = maps_start_position; i < maps_on_page + maps_start_position; i++) {
		int menu_position = (i + 1) - (user_maps_page * maps_on_page);
		char* bsp_name = custom_maps[i].map_name;

		Menu_DrawMapButton(menu_position, menu_position, i, bsp_name);
	}

	int current_registry_position = ((i + 1) - (user_maps_page * maps_on_page)) - 1;

	CUSTOMMAPS_ITEMS = current_registry_position+2;

	Menu_DrawDivider(9.25);

	if (maps_on_page + maps_start_position < num_user_maps) {
		Menu_CustomMaps_IncreaseItems();
		Menu_DrawButton(9.5, CUSTOMMAPS_ITEMS-2, "NEXT PAGE", MENU_BUTTON_ACTIVE, "Advance to next User Map page.");
	} else {
		Menu_CustomMaps_DecreaseItems();
		Menu_DrawButton(9.5, -1, "NEXT PAGE", MENU_BUTTON_INACTIVE, "");
	}

	if (user_maps_page != 0) {
		Menu_CustomMaps_IncreaseItems();
		Menu_DrawButton(10.5, CUSTOMMAPS_ITEMS-1, "PREVIOUS PAGE", MENU_BUTTON_ACTIVE, "Return to last User Map page.");
	} else {
		Menu_CustomMaps_DecreaseItems();
		Menu_DrawButton(10.5, -1, "PREVIOUS PAGE", MENU_BUTTON_INACTIVE, "");
	}

    Menu_DrawButton(11.5, CUSTOMMAPS_ITEMS, "BACK", MENU_BUTTON_ACTIVE, "Return to Stock Map selection.");
}

void Menu_CustomMaps_Key (int key)
{
	switch (key)
	{

	case K_DOWNARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (++menu_custommaps_cursor >= CUSTOMMAPS_ITEMS)
			menu_custommaps_cursor = 0;
		break;

	case K_UPARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (--menu_custommaps_cursor < 0)
			menu_custommaps_cursor = CUSTOMMAPS_ITEMS - 1;
		break;

	case K_ENTER:
	case K_AUX1:
		Menu_StartSound(MENU_SND_ENTER);

		if (menu_custommaps_cursor <= (CUSTOMMAPS_ITEMS-4)) {
			Map_SetDefaultValues();
			current_selected_bsp = custom_maps[menu_custommaps_cursor].map_name;
            Menu_Lobby_Set();
		}
		if (menu_custommaps_cursor == (CUSTOMMAPS_ITEMS-3)) {
			Menu_CustomMaps_NextPage();
		} else if (menu_custommaps_cursor == (CUSTOMMAPS_ITEMS-2)) {
			Menu_CustomMaps_PrevPage();
		}

		if (menu_custommaps_cursor == (CUSTOMMAPS_ITEMS-1)) {
			Menu_StockMaps_Set();
		}
	}
}

/*
=========================
Preload_CustomMap_Images
=========================
*/
void Menu_Preload_Custom_Images(void) 
{
    // Pre-load all custom map menu images to use in the background
	for (int i = 0; i < num_user_maps; i++) {
		if (custom_maps[i].map_use_thumbnail == 1) {
			// custom map is loaded and has a menu image
			// load with linear filtering on psp
			menu_usermap_image[i] = Image_LoadImage(custom_maps[i].map_thumbnail_path, IMAGE_PNG | IMAGE_TGA | IMAGE_JPG, 1, false, false);
			if (menu_usermap_image[i] > -1) {
				num_custom_images++;
			}
		}
	}
}