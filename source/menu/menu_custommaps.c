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
#include "menu_dummy.h"
// needed for Map_Finder()
#include <sys/dirent.h>

// =============
// Custom maps
// =============
image_t             menu_usermap_image[MAX_CUSTOMMAPS];
achievement_list_t  achievement_list[MAX_ACHIEVEMENTS];

typedef struct
{
	int 		occupied;
	int 	 	map_allow_game_settings;
	int 	 	map_use_thumbnail;
	char* 		map_name;
	char* 		map_name_pretty;
	char* 		map_desc_1;
	char* 		map_desc_2;
	char* 		map_desc_3;
	char* 		map_desc_4;
	char* 		map_desc_5;
	char* 		map_desc_6;
	char* 		map_desc_7;
	char* 		map_desc_8;
	char* 		map_author;
	char* 		map_thumbnail_path;
} usermap_t;

usermap_t           custom_maps[MAX_CUSTOMMAPS];

int	    m_map_cursor;
int	    MAP_ITEMS;
int     user_maps_num = 0;
int     current_custom_map_page;
int     custom_map_pages;
int     multiplier;
int     num_custom_images = 0;
char    user_levels[256][MAX_QPATH];

/*
========================
remove_windows_newlines
========================
*/
char* remove_windows_newlines(const char* line)
{
    const char* p = line;
    size_t len = strlen(line);
    char* result = (char*)malloc(len + 1);
    
    if (result == NULL) {
        return NULL;
    }
    
    char* q = result;
    
    for (size_t i = 0; i < len; i++) {
        if (p[i] == '\r') {
            continue;
        }
        *q++ = p[i];
    }
    
    *q = '\0';
    
    return result;
}

/*
===============
Map_Finder
===============
*/
void Menu_Map_Finder(void)
{
	struct dirent *dp;
    DIR *dir = opendir(va("%s/maps", com_gamedir)); // Open the directory - dir contains a pointer to manage the dir

	if(dir < 0)
	{
		Sys_Error ("Map_Finder");
		return;
	}

	for (int i = 0; i < 50; i++) {
		custom_maps[i].occupied = false;
	}
	
	while((dp = readdir(dir)))
	{
		
		if(dp->d_name[0] == '.')
		{
			continue;
		}

		if(!strcmp(COM_FileExtension(dp->d_name),"bsp")|| !strcmp(COM_FileExtension(dp->d_name),"BSP"))
	    {
			char ntype[32];

			COM_StripExtension(dp->d_name, ntype);
			custom_maps[user_maps_num].occupied = true;
			custom_maps[user_maps_num].map_name = malloc(sizeof(char)*32);
			sprintf(custom_maps[user_maps_num].map_name, "%s", ntype);

			char* 		setting_path;

			setting_path 								  	= malloc(sizeof(char)*64);
			custom_maps[user_maps_num].map_thumbnail_path 	= malloc(sizeof(char)*64);
			strcpy(setting_path, 									va("%s/maps/", com_gamedir));
			strcpy(custom_maps[user_maps_num].map_thumbnail_path, 	"gfx/menu/custom/");
			strcat(setting_path, 									custom_maps[user_maps_num].map_name);
			strcat(custom_maps[user_maps_num].map_thumbnail_path, 	custom_maps[user_maps_num].map_name);
			strcat(setting_path, ".txt");

			FILE    *setting_file;			
			setting_file = fopen(setting_path, "rb");
			if (setting_file != NULL) {

				fseek(setting_file, 0L, SEEK_END);
				size_t sz = ftell(setting_file);
				fseek(setting_file, 0L, SEEK_SET);

				int state = 0;
				int value = 0;

				custom_maps[user_maps_num].map_name_pretty = malloc(sizeof(char)*32);
				custom_maps[user_maps_num].map_desc_1 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_desc_2 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_desc_3 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_desc_4 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_desc_5 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_desc_6 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_desc_7 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_desc_8 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_author = malloc(sizeof(char)*40);

				char* buffer = (char*)calloc(sz+1, sizeof(char));
				fread(buffer, sz, 1, setting_file);

				strtok(buffer, "\n");
				while(buffer != NULL) {
					switch(state) {
						case 0: strcpy(custom_maps[user_maps_num].map_name_pretty, remove_windows_newlines(buffer)); break;
						case 1: strcpy(custom_maps[user_maps_num].map_desc_1, remove_windows_newlines(buffer)); break;
						case 2: strcpy(custom_maps[user_maps_num].map_desc_2, remove_windows_newlines(buffer)); break;
						case 3: strcpy(custom_maps[user_maps_num].map_desc_3, remove_windows_newlines(buffer)); break;
						case 4: strcpy(custom_maps[user_maps_num].map_desc_4, remove_windows_newlines(buffer)); break;
						case 5: strcpy(custom_maps[user_maps_num].map_desc_5, remove_windows_newlines(buffer)); break;
						case 6: strcpy(custom_maps[user_maps_num].map_desc_6, remove_windows_newlines(buffer)); break;
						case 7: strcpy(custom_maps[user_maps_num].map_desc_7, remove_windows_newlines(buffer)); break;
						case 8: strcpy(custom_maps[user_maps_num].map_desc_8, remove_windows_newlines(buffer)); break;
						case 9: strcpy(custom_maps[user_maps_num].map_author, remove_windows_newlines(buffer)); break;
						case 10: sscanf(remove_windows_newlines(buffer), "%d", &value); custom_maps[user_maps_num].map_use_thumbnail = value; break;
						case 11: sscanf(remove_windows_newlines(buffer), "%d", &value); custom_maps[user_maps_num].map_allow_game_settings = value; break;
						default: break;
					}
					state++;
					buffer = strtok(NULL, "\n");
				}
				free(buffer);
				buffer = 0;
				fclose(setting_file);
			}
			user_maps_num++;
		}
	}
    closedir(dir); // close the handle (pointer)
	custom_map_pages = (int)ceil((double)(user_maps_num)/15);
}

/*
=========================
Preload_CustomMap_Images
=========================
*/
void Menu_Preload_Custom_Images(void) 
{
    // Pre-load all custom map menu images to use in the background
	for (int i = 0; i < user_maps_num; i++) {
		if (custom_maps[i].map_use_thumbnail == 1) {
			// custom map is loaded and has a menu image
			menu_usermap_image[i] = Image_LoadImage(custom_maps[i].map_thumbnail_path, IMAGE_PNG | IMAGE_TGA | IMAGE_JPG, 0, false, false);
			if (menu_usermap_image[i] > -1) {
				num_custom_images++;
			}
		}
	}
}