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

image_t             menu_usermap_image[MAX_CUSTOMMAPS];
achievement_list_t  achievement_list[MAX_ACHIEVEMENTS];

usermap_t           custom_maps[MAX_CUSTOMMAPS];

char    user_levels[256][MAX_QPATH];

int		menu_custommaps_cursor;

int     current_custom_map_page;
int     num_user_maps = 0;
int     num_custom_images = 0;
int     custom_map_pages = 0;

void Menu_CustomMaps_Set ()
{

}

void Menu_CustomMaps_Draw ()
{

}

void Menu_CustomMaps_Key (int key)
{

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