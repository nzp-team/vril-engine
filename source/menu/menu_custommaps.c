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
#ifdef __NSPIRE__
#include <libndls.h>
#include <dirent.h>
#include <sys/types.h>
#include <os.h>
#elif __PSP__
#include <pspkernel.h>
#include <psputility.h>
#include <pspiofilemgr.h>
#elif __PSP2__
#include <dirent.h>
#else
#include <sys/dirent.h>
#endif

image_t             	menu_usermap_image[MAX_CUSTOMMAPS];
achievement_list_t  	achievement_list[MAX_ACHIEVEMENTS];
usermap_t           	custom_maps[MAX_CUSTOMMAPS];

int		user_maps_page;
int     num_user_maps = 0;
int     num_custom_images = 0;
int     custom_map_pages = 0;

// needs to be global so 
// we can grab the index
int maps_start_position = 0;

#define MAX_SETTINGS_LINES 12
#define MAX_MAP_PER_PAGE 8
#define MAP_DESC_LINES 8

#ifdef PLATFORM_GAMEFILES_IN_PAK
typedef struct {
    char id[4];          // "PACK"
    int  dirofs;         // offset to directory
    int  dirlen;         // length of directory
} pakheader_t;

typedef struct {
    char name[56];       // file path (e.g. "maps/ndu.bsp")
    int  filepos;        // offset to file data
    int  filelen;        // length of file data
} pakentry_t;

typedef struct {
    FILE *fp;
    pakentry_t *entries;
    int num_entries;
    int index;
} PakDir;

PakDir *Pak_Open(const char *pakpath)
{
    pakheader_t hdr;
    PakDir *p = malloc(sizeof(PakDir));
    if (!p) return NULL;

    memset(p, 0, sizeof(*p));

    p->fp = fopen(pakpath, "rb");
    if (!p->fp) {
        free(p);
        return NULL;
    }

    fread(&hdr, sizeof(hdr), 1, p->fp);
    if (memcmp(hdr.id, "PACK", 4) != 0) {
        fclose(p->fp);
        free(p);
        return NULL;
    }

    p->num_entries = hdr.dirlen / sizeof(pakentry_t);
    p->entries = malloc(hdr.dirlen);

    fseek(p->fp, hdr.dirofs, SEEK_SET);
    fread(p->entries, sizeof(pakentry_t), p->num_entries, p->fp);

    p->index = 0;
    return p;
}

void Pak_Close(PakDir *p)
{
    if (!p) return;
    fclose(p->fp);
    free(p->entries);
    free(p);
}

int Pak_ReadMap(PakDir *p, char *out_name, size_t out_size)
{
    while (p->index < p->num_entries) {
        pakentry_t *e = &p->entries[p->index++];
        
        if (strncmp(e->name, "maps/", 5) != 0) {
			continue;
		}

        if ((!strcmp(COM_FileExtension(e->name),"bsp")) || 
			(!strcmp(COM_FileExtension(e->name),"BSP"))) {

			// strip "maps/"
			const char *filename = e->name + 5;
			strlcpy(out_name, filename, out_size);
			return 1;
		}
    }
    return 0;
}

#endif

typedef struct GlobalDir {
#ifdef __PSP__
    SceUID 		dir;
    SceIoDirent dirent;
#else
    DIR 		*dir;
	struct 		dirent *dp;
#endif
} GlobalDir;

GlobalDir *Dir_Open(char *path)
{
    GlobalDir *d = malloc(sizeof(GlobalDir));
    if (!d) {
		return NULL;
	}
        
#ifdef __PSP__
    d->dir = sceIoDopen(path);
    if (d->dir < 0) {
        free(d);
        return NULL;
    }

    memset(&d->dirent, 0, sizeof(SceIoDirent));
#else
    d->dir = opendir(path);
    if (!d->dir) {
        free(d);
        return NULL;
    }
#endif

    return d;
}

int Dir_Read(GlobalDir *d, char *out_name, size_t out_size)
{
#ifdef __PSP__
    if (sceIoDread(d->dir, &d->dirent) <= 0) {
		return 0;
	}

    strlcpy(out_name, d->dirent.d_name, out_size);
    return 1;
#else
    struct dirent *dp = readdir(d->dir);
    if (!dp) {
		return 0;
	}

	strlcpy(out_name, dp->d_name, out_size);
    return 1;
#endif
}

void Dir_Close(GlobalDir *d)
{
    if (!d) return;

#ifdef __PSP__
    sceIoDclose(d->dir);
#else
    closedir(d->dir);
#endif

    free(d);
}

void Menu_CustomMaps_AllocStrings (void)
{
	custom_maps[num_user_maps].map_name = malloc(MAX_QPATH*sizeof(char));
	custom_maps[num_user_maps].map_name_pretty = malloc(MAX_QPATH*sizeof(char));
	custom_maps[num_user_maps].map_author = malloc(MAX_QPATH*sizeof(char));
	custom_maps[num_user_maps].map_thumbnail_path = malloc(MAX_OSPATH*sizeof(char));

	for (int i = 0; i < MAP_DESC_LINES; i++) {
		custom_maps[num_user_maps].map_desc[i] = malloc(MAX_QPATH*sizeof(char));
	}
}

void Menu_CustomMaps_AddMap (char *map_name)
{
	// definitions for map setting values
	char settings_line[MAX_QPATH];
	char settings_path[MAX_OSPATH*2];
	int total_settings_lines = 0;

	// We found a map
	// Explicitly clear fields to be safe
	memset(&custom_maps[num_user_maps], 0, sizeof(usermap_t));
	// Allocate space for strings
	Menu_CustomMaps_AllocStrings();
	// Copy over the map name
	snprintf(custom_maps[num_user_maps].map_name, MAX_QPATH, "%s", map_name);

	// Map is occupied
	custom_maps[num_user_maps].occupied = true;

	// Set setting path to read 
	snprintf(settings_path, MAX_OSPATH*2, "maps/%s.txt", map_name);

	// Open map settings file and load into custom_maps
	FILE *settings_file;
	if(COM_FOpenFile(settings_path, &settings_file) != -1) {
		while ((fgets(settings_line, sizeof(settings_line), settings_file) != NULL) && total_settings_lines < MAX_SETTINGS_LINES) {
			strip_newline (settings_line);
			switch (total_settings_lines) {
				// Map Name
				case 0:
					snprintf(custom_maps[num_user_maps].map_name_pretty, MAX_QPATH, "%s", settings_line);
					break;
				// Map desc lines
				case 1 ... 8:
					snprintf(custom_maps[num_user_maps].map_desc[total_settings_lines-1], MAX_QPATH, "%s", settings_line);
					break;
				case 9:
					snprintf(custom_maps[num_user_maps].map_author, MAX_QPATH, "%s", settings_line);
					break;
				// Map uses thumbnail value
				case 10:			
					custom_maps[num_user_maps].map_use_thumbnail = atoi(settings_line);
					break;
				// Game settings allowed value
				case 11:
					custom_maps[num_user_maps].map_allow_game_settings = atoi(settings_line);
					break;
				default:
					Con_DPrintf("Menu_CustomMaps_MapFinder: map %s has incorrect setting file formatting\n", map_name);
					break;
				}
			total_settings_lines++;
		}
		fclose(settings_file);
	} else {
		// No settings file
		// set the pretty map name for menu drawing
		// and zero out the rest of memory
		snprintf(custom_maps[num_user_maps].map_name_pretty, MAX_QPATH, "%s", map_name);
		for (int i = 0; i < MAP_DESC_LINES; i++) {
			custom_maps[num_user_maps].map_desc[i][0] = '\0';
		}
		custom_maps[num_user_maps].map_author[0] = '\0';
		custom_maps[num_user_maps].map_use_thumbnail = 0;
		custom_maps[num_user_maps].map_allow_game_settings = 0;
	}
	// Set the thumbnail path for loadscreens
	if (custom_maps[num_user_maps].map_use_thumbnail) {
		snprintf(custom_maps[num_user_maps].map_thumbnail_path, MAX_OSPATH+MAX_QPATH, "gfx/menu/custom/%s", map_name);
	} else {
		custom_maps[num_user_maps].map_thumbnail_path[0] = '\0';
	}
	// Increment total custom maps amount
	num_user_maps++;
}

void Menu_CustomMaps_MapFinder (void)
{
	char 	d_name[MAX_QPATH];
	char	map_name[MAX_QPATH];

#ifndef PLATFORM_GAMEFILES_IN_PAK
	// File structure is loose folders
	GlobalDir *dir = Dir_Open (va("%s/maps", com_gamedir));

	if (!dir) {
		Sys_Error ("MapFinder was unable to open game directory");
	}

	// Open the directory
	while (Dir_Read(dir, d_name, sizeof(d_name))) {
		if ((!strcmp(COM_FileExtension(d_name),"bsp")) || 
			(!strcmp(COM_FileExtension(d_name),"BSP"))) {
			// Max amount of custom maps is reached
			if (num_user_maps >= MAX_CUSTOMMAPS) {
				Sys_Error("Too many custom maps (max %d)\n", MAX_CUSTOMMAPS);
				break;
			}

			// Set map name
			COM_StripExtension(d_name, map_name);
			strtolower(map_name);

			// Add the map to our custom_maps[] struct
			Menu_CustomMaps_AddMap(map_name);
		}
	}
	Dir_Close(dir); // Close the handle (pointer)
#else
	// Parse the pak file and find maps
	char pakfile[MAX_OSPATH];
	snprintf (pakfile, MAX_OSPATH+MAX_QPATH, "%s/%snzp.pak%s", com_gamedir, FILE_SPECIAL_PREFIX, FILE_SPECIAL_SUFFIX);
	//snprintf (pakfile, MAX_OSPATH+MAX_QPATH, "%s/nzp.pak", com_gamedir);

	PakDir *pak = Pak_Open(pakfile);
    if (pak) {
        while (Pak_ReadMap(pak, d_name, sizeof(d_name))) {
			COM_StripExtension(d_name, map_name);
			strtolower(map_name);

            Menu_CustomMaps_AddMap(map_name);
        }
        Pak_Close(pak);
    }
#endif
	custom_map_pages = (int)ceil((double)(num_user_maps)/8);
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
			int menu_image = Image_LoadImage(custom_maps[i].map_thumbnail_path, IMAGE_PNG | IMAGE_TGA | IMAGE_JPG, 1, false, false);
			if (menu_image > 0) {
				menu_usermap_image[i] = menu_image;
				num_custom_images++;
			}
		}
	}
}

void Menu_CustomMaps_SetMap (void)
{
	int map_index = maps_start_position + current_menu.cursor;

	Map_SetDefaultValues();
	current_selected_bsp = custom_maps[map_index].map_name;
	Menu_Lobby_Set();
}

void Menu_CustomMaps_NextPage (void)
{
	Menu_ResetMenuButtons();
    Menu_SetSound(MENU_SND_ENTER);
    user_maps_page++;
}

void Menu_CustomMaps_PrevPage (void)
{
	Menu_ResetMenuButtons();
    Menu_SetSound(MENU_SND_ENTER);
    user_maps_page--;
}

void Menu_CustomMaps_Set (void)
{
	Menu_ResetMenuButtons();
	user_maps_page = 0;
	key_dest = key_menu;
	m_state = m_custommaps;
}

void Menu_CustomMaps_Draw (void)
{
	int     i;
	int 	custommap_items = 0;
    char*   header_text;

    // Background
	Menu_DrawCustomBackground(true);

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
	maps_start_position = user_maps_page * MAX_MAP_PER_PAGE;
	// default to 8, all that will fit on the UI.
	int maps_on_page = MAX_MAP_PER_PAGE;

	// Not all 8 slots can be filled..
	if (MAX_MAP_PER_PAGE > num_user_maps - maps_start_position) {
		// So set how many maps will be on the last page
		maps_on_page = num_user_maps - maps_start_position;
	}

	for (i = maps_start_position; i < maps_start_position + maps_on_page; i++) {
		int menu_position = (i + 1) - (user_maps_page * MAX_MAP_PER_PAGE);
		Menu_DrawMapButton(menu_position, custommap_items, i, MAP_CATEGORY_USER, custom_maps[i].map_name, Menu_CustomMaps_SetMap);
		custommap_items++;
	}

	Menu_DrawDivider(9.25);

	if (maps_on_page + maps_start_position < num_user_maps) {
		Menu_DrawButton(9.5, custommap_items++, "NEXT PAGE", "Advance to next User Map page.", Menu_CustomMaps_NextPage);
	} else {
		Menu_DrawGreyButton(9.5, "NEXT PAGE");
	}

	if (user_maps_page != 0) {
		Menu_DrawButton(10.5, custommap_items++, "PREVIOUS PAGE", "Return to last User Map page.", Menu_CustomMaps_PrevPage);
	} else {
		Menu_DrawGreyButton(10.5, "PREVIOUS PAGE");
	}

    Menu_DrawButton(11.5, custommap_items, "BACK", "Return to Stock Map selection.", Menu_StockMaps_Set);
}