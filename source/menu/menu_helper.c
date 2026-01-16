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

float   		menu_scale_factor;
float			CHAR_WIDTH;
float			CHAR_HEIGHT;

StockMaps		stock_maps[4];

StockMaps stock_maps[4] = {
	[0] = { .bsp_name = "ndu", .array_index = 0 },
	[1] = { .bsp_name = "nzp_warehouse", .array_index = 0 },
	[2] = { .bsp_name = "nzp_warehouse2", .array_index = 0 },
	[3] = { .bsp_name = "christmas_special", .array_index = 0 }
};

/*
===============
Menu_DictateScaleFactor
===============
*/
void Menu_DictateScaleFactor(void)
{
	// Platform-dictated text scale.
#ifdef __NSPIRE__
	menu_scale_factor = 1.0f;
#elif __PSP__
	menu_scale_factor = 1.0f;
#elif __vita__
	menu_scale_factor = 2.0f;
#elif __3DS__
	menu_scale_factor = 1.0f;
#else
	menu_scale_factor = 1.0f;
#endif // __NSPIRE__, __PSP__, __vita__, __3DS__

	CHAR_WIDTH = 8*menu_scale_factor;
	CHAR_HEIGHT = 8*menu_scale_factor;

	big_bar_height = vid.height/9;
	small_bar_height = vid.height/64;
}

/*
======================
Menu_LoadPics
======================
*/
void Menu_LoadPics ()
{
	menu_bk 	= Image_LoadImage("gfx/menu/menu_background", IMAGE_TGA, 0, false, false);
	menu_social = Image_LoadImage("gfx/menu/social", IMAGE_TGA, 0, false, false);

	Menu_Preload_Custom_Images ();
}

/*
======================
Menu_StartSound
======================
*/
void Menu_StartSound (int type)
{
    switch (type) {
        case MENU_SND_NAVIGATE:
            S_LocalSound ("sounds/menu/navigate.wav");
            break;
        case MENU_SND_ENTER:
            S_LocalSound ("sounds/menu/enter.wav");
            break;
        case MENU_SND_BEEP:
            S_LocalSound ("sounds/misc/talk2.wav");
            break;
        default:
            Con_Printf("Unsupported menu sound\n");
            break;
    }
}

image_t Menu_PickBackground ()
{
	// No custom images
	if (num_custom_images == 0) return menu_bk;

	int i = (rand() % num_custom_images);

	// Sometimes custom map images are invalid
	if (menu_usermap_image[i] <= 0)	return menu_bk;

    return (image_t)menu_usermap_image[i];
}

void Menu_InitStockMaps (void)
{
	num_stock_maps = sizeof(stock_maps)/sizeof(stock_maps[0]);

	for (int i = 0; i < num_stock_maps; i++) {
		for (int j = 0; j < num_user_maps; j++) {
			if(!strcmp(stock_maps[i].bsp_name, custom_maps[j].map_name)) {
				stock_maps[i].array_index = j;
			}
		}
	}
}

int UserMapSupportsCustomGameLookup (char *bsp_name)
{
	for (int i = 0; i < num_user_maps; i++) {
		if (!strcmp(bsp_name, custom_maps[i].map_name)) {
			return custom_maps[i].map_allow_game_settings;
		}
	}

	return 0;
}

int Menu_GetMapImage (char *bsp_name)
{
	for (int i = 0; i < num_user_maps; i++) {
		if (custom_maps[i].map_use_thumbnail) {
			if (!strcmp(bsp_name, custom_maps[i].map_name)) {
				return menu_usermap_image[i];
			}
		}
	}

	return 0;
}

void Map_SetDefaultValues (void)
{
	Cvar_SetValue("sv_gamemode", 0);
	Cvar_SetValue("sv_difficulty", 0);
	Cvar_SetValue("sv_startround", 0);
	Cvar_SetValue("sv_magic", 1);
	Cvar_SetValue("sv_headshotonly", 0);
	Cvar_SetValue("sv_maxai", 24);
	Cvar_SetValue("sv_fastrounds", 0);
}

void Menu_LoadMap (char *selected_map)
{
	int i;

	for (i = 0; i < num_user_maps; i++) {
		if (!strcmp(selected_map, custom_maps[i].map_name)) 
			break;
	}

	map_loadname = current_selected_bsp;
    map_loadname_pretty = custom_maps[i].map_name_pretty;

	key_dest = key_game;
	if (sv.active)
		Cbuf_AddText ("disconnect\n");
	Cbuf_AddText ("maxplayers 1\n");
	Cbuf_AddText (va("map %s\n", map_loadname));
	loadingScreen = 1;
}

/*
======================
Menu_DrawCustomBackground
======================
*/
void Menu_DrawCustomBackground ()
{
    float elapsed_background_time = menu_time - menu_starttime;

	// TODO

	// 
	// Slight background pan
	//

	// TODO

	//float x_pos = 0 + (((vid.width * 1.05) - vid.width) * (elapsed_background_time/7));

	Draw_StretchPic(0, 0, menu_background, vid.width, vid.height);
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 84);

	//
	// Fade new images in/out
	//
	float alphaf = 0;

	if (elapsed_background_time < 1.0f) {
        alphaf = 1.0f - (float)sin((elapsed_background_time / 1.0f) * ((float)M_PI / 2));  // Opacity from 1 -> 0
    } else if (elapsed_background_time < 6.0f) {
        alphaf = 0.0f;
    } else if (elapsed_background_time < 7.0f) {
        alphaf = (float)sin(((elapsed_background_time - 6.0f) / 1.0f) * ((float)M_PI / 2));  // Opacity from 0 -> 1
    }

	if (alphaf < 0.0f) alphaf = 0.0f;
	if (alphaf > 1.0f) alphaf = 1.0f;

	float alpha = (alphaf * 255);

	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, alpha);

	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 84);

	// Top Bars
	Draw_FillByColor(0, 0, vid.width, big_bar_height, 0, 0, 0, 228);
	Draw_FillByColor(0, big_bar_height, vid.width, small_bar_height, 130, 130, 130, 255);

	// Bottom Bars
	Draw_FillByColor(0, (vid.height - big_bar_height), vid.width, big_bar_height, 0, 0, 0, 228);
	Draw_FillByColor(0, (vid.height - big_bar_height) - small_bar_height, vid.width, small_bar_height, 130, 130, 130, 255);
}

void Menu_DrawSocialBadge (int order, int which)
{
	int y_factor = (vid.height/16);
	int y_pos = (vid.height/2) + (vid.width/36) + (order*y_factor);
	int x_pos = vid.width - (vid.width/20);
	float s = 0;
	float t = 0;

	switch (which) {
		case MENU_SOC_DOCS:
			s = 0;
			t = 0;
			break;

		case MENU_SOC_YOUTUBE:
			s = 0.5;
			t = 0;
			break;

		case MENU_SOC_BLUESKY:
			s = 0;
			t = 0.5;
			break;

		case MENU_SOC_PATREON:
			s = 0.5;
			t = 0.5;
			break;
	}

	Draw_SubPic (x_pos, y_pos, menu_social, s, t, 0.5, 0.25, 255, 255, 255, 255);
}

/*
======================
Menu_DrawTitle
======================
*/
void Menu_DrawTitle (char *title_name, int color)
{
	int x_pos = vid.width/64;
	int y_pos = vid.height/24;

	switch (color) {
		case MENU_COLOR_WHITE:
			Draw_ColoredString (x_pos, y_pos, title_name, 255, 255, 255, 255, menu_scale_factor*2);
			break;
		case MENU_COLOR_YELLOW:
			Draw_ColoredString (x_pos, y_pos, title_name, 255, 255, 0, 255, menu_scale_factor*2);
			break;
	}
}

qboolean Menu_IsHovered (int button_number)
{
	switch (m_state) {
		case m_main:
			if (button_number == (menu_main_cursor+1)) {
				return true;
			}
			break;
		case m_stockmaps:
			if (button_number == (menu_stockmaps_cursor+1)) {
				return true;
			}
			break;
		case m_lobby:
			if (button_number == (menu_lobby_cursor+1)) {
				return true;
			}
			break;
		case m_gamesettings:
			if (button_number == (menu_gamesettings_cursor+1)) {
				return true;
			}
	}

	return false;
}

/*
======================
Menu_DrawSelectionBox
======================
*/
void Menu_DrawSelectionBox (int x_pos, int y_pos)
{
	int border_width = (vid.height/144);
	int x_length = (vid.width/3) + border_width;
	int border_height_offset = (vid.height/96);

	// Draw selection box
	// Done in 3 parts
	// Top
	Draw_FillByColor (0, y_pos - border_height_offset, x_length, border_width, 255, 0, 0, 255);
	// Bottom
	Draw_FillByColor (0, (y_pos + (border_height_offset*2)) + (CHAR_HEIGHT/2), x_length, border_width, 255, 0, 0, 255);
	// Side
	Draw_FillByColor (x_length, (y_pos - (CHAR_HEIGHT/2)) + (border_height_offset), border_width, (border_height_offset) + CHAR_HEIGHT + border_width, 255, 0, 0, 255);
}

/*
======================
Menu_DrawMapBorder
======================
*/
void Menu_DrawMapBorder (int x_pos, int y_pos, int image_width, int image_height)
{
	int border_side_width = (vid.width/144)/2;

	// Top
	Draw_FillByColor (x_pos, y_pos, image_width+border_side_width, small_bar_height/2, 130, 130, 130, 255);
	// Left Side
	Draw_FillByColor (x_pos, y_pos, border_side_width, image_height, 130, 130, 130, 255);
	// Right Side
	Draw_FillByColor (x_pos+image_width, y_pos, border_side_width, image_height, 130, 130, 130, 255);
	// Bottom
	Draw_FillByColor (x_pos, y_pos+image_height, image_width+border_side_width, small_bar_height/2, 130, 130, 130, 255);
}

/*
======================
Menu_DrawButton
======================
*/
void Menu_DrawButton (int order, int button_number, char* button_name, int button_active, char* button_summary)
{
	// y factor for vertical menu ordering
	int y_factor = (vid.height/16);

	int x_pos = (vid.width/3) - getTextWidth(button_name, menu_scale_factor); 
	int y_pos = (vid.height/8) + (order*y_factor);

	// Inactive (grey) menu button
	// Non-selectable
	if (button_active == MENU_BUTTON_INACTIVE) {
		Draw_ColoredString (x_pos, y_pos, button_name, 128, 128, 128, 255, menu_scale_factor);
	} else if (button_active == MENU_BUTTON_ACTIVE) {
		// Active menu buttons
		// Hovering over button
		if (Menu_IsHovered(button_number)) {
			// Make button red and add a box around it
			// Draw button string
			Draw_ColoredString (x_pos, y_pos, button_name, 255, 0, 0, 255, menu_scale_factor);

			// Draw selection box
			Menu_DrawSelectionBox (x_pos, y_pos);

			// Draw the bottom screen text for selected button 
			Draw_ColoredStringCentered (vid.height - (vid.height/12), button_summary, 255, 255, 255, 255, menu_scale_factor);
		} else {
			// Not hovering over button
			Draw_ColoredString (x_pos, y_pos, button_name, 255, 255, 255, 255, menu_scale_factor);
		}
	}
}

void Menu_DrawOptionButton(int order, char* selection_name)
{
	// y factor for vertical menu ordering
	int y_factor = (vid.height/16);

	int x_pos = (vid.width/3) + getTextWidth(selection_name, menu_scale_factor) - (vid.width/24); 
	int y_pos = (vid.height/8) + (order*y_factor);

	Draw_ColoredString(x_pos, y_pos, selection_name, 255, 255, 255, 255, menu_scale_factor);
}

/*
======================
Menu_DrawMapButton
======================
*/
void Menu_DrawMapButton (int order, int button_number, int usermap_index, char* bsp_name)
{
	int i;
	int index = 0;
	char *button_name;
	int y_factor = (vid.height/24);

	int image_width = (vid.width/3);
	int image_height = (vid.height/3);

	int x_pos = ((vid.width/3)+(vid.width/36)) + (image_width/2);
	int y_pos = (vid.height/5)-(vid.height/24);

	// if this is a stock map
	if (usermap_index < 0) {
		for (i = 0; i < num_stock_maps; i++) {
			if (stock_maps[i].bsp_name == bsp_name) {
				index = stock_maps[i].array_index;
			}
		}
	} else {
		index = usermap_index;
	}

	button_name = custom_maps[index].map_name_pretty;
	if (!button_name) {
		button_name = custom_maps[index].map_name;
	}

	// Draw the selectable button
	Menu_DrawButton (order, button_number, button_name, MENU_BUTTON_ACTIVE, button_name);

	if (Menu_IsHovered(button_number)) {
		// Draw map thumbnail picture
		Draw_StretchPic (x_pos, y_pos, menu_usermap_image[index], image_width, image_height);

		// Draw border around map image
		Menu_DrawMapBorder (x_pos, y_pos, image_width, image_height);

		// Draw map description
		for (int j = 0; j < 8; j++) {
			Draw_ColoredString (x_pos - ((vid.width/42)*2), (y_pos + image_height + (small_bar_height*2)) + j*y_factor, custom_maps[index].map_desc[j], 255, 255, 255, 255, menu_scale_factor);
		}

		// Draw map author
		if (custom_maps[index].map_author != NULL) {
			Draw_ColoredStringCentered (vid.height - (vid.height/24), custom_maps[index].map_author, 255, 255, 0, 255, menu_scale_factor);
		}
	}
}

/*
======================
Menu_DrawLobbyInfo
======================
*/
void Menu_DrawLobbyInfo (char* bsp_name, char* info_gamemode, char* info_difficulty, char* info_startround, char* info_magic, char* info_headshotonly, char* info_fastrounds, char* info_hordesize)
{
	int texnum;
	int	i;

	int image_width = (vid.width/4);
	int image_height = (vid.height/4);

	int image_x_pos = ((vid.width)-(vid.width/3.5) - image_width/2);
	int image_y_pos = (vid.height) - (big_bar_height + small_bar_height) - (image_height) - (vid.height/24);

	int left_column_x = (vid.width/3) + (vid.width/3);
	int left_column_y = (vid.height/6);

	int y_offset = CHAR_HEIGHT;
	int y_newline = (vid.height/10);
	int x_newline = (vid.width/4);

	texnum = Menu_GetMapImage (bsp_name);

	for (i = 0; i < num_user_maps; i++) {
		if (!strcmp(bsp_name, custom_maps[i].map_name)) 
			break;
	}

	// Draw map thumbnail picture
	Draw_StretchPic (image_x_pos, image_y_pos, texnum, image_width, image_height);

	// Draw border around map image
	Menu_DrawMapBorder (image_x_pos, image_y_pos, image_width, image_height);

	// Game Mode
	Draw_ColoredString (left_column_x - getTextWidth("Game Mode", menu_scale_factor), left_column_y, "Game Mode", 255, 255, 255, 255, menu_scale_factor);
	Draw_ColoredString (left_column_x - getTextWidth(info_gamemode, menu_scale_factor) + (getTextWidth("Game Mode", menu_scale_factor)/96), left_column_y + y_offset, info_gamemode, 255, 255, 0, 255, menu_scale_factor);
	Draw_FillByColor (left_column_x - getTextWidth("Game Mode", menu_scale_factor), left_column_y + (y_offset*2), getTextWidth("Game Mode", menu_scale_factor), (small_bar_height/3), 130, 130, 130, 255);

	// Difficulty
	Draw_ColoredString (left_column_x - getTextWidth("Difficulty", menu_scale_factor) + x_newline, left_column_y, "Difficulty", 255, 255, 255, 255, menu_scale_factor);
	Draw_ColoredString (left_column_x - getTextWidth(info_difficulty, menu_scale_factor) + (getTextWidth("Difficulty", menu_scale_factor)/96) + x_newline, left_column_y + y_offset, info_difficulty, 255, 255, 0, 255, menu_scale_factor);
	Draw_FillByColor (left_column_x - getTextWidth("Difficulty", menu_scale_factor) + x_newline, left_column_y + (y_offset*2), getTextWidth("Difficulty", menu_scale_factor), (small_bar_height/3), 130, 130, 130, 255);

	// Start Round
	Draw_ColoredString (left_column_x - getTextWidth("Start Round", menu_scale_factor), left_column_y + y_newline, "Start Round", 255, 255, 255, 255, menu_scale_factor);
	Draw_ColoredString (left_column_x - getTextWidth(info_startround, menu_scale_factor) + (getTextWidth("Start Round", menu_scale_factor)/96), left_column_y + y_offset + y_newline, info_startround, 255, 255, 0, 255, menu_scale_factor);
	Draw_FillByColor (left_column_x - getTextWidth("Start Round", menu_scale_factor), left_column_y + (y_offset*2) + y_newline, getTextWidth("Start Round", menu_scale_factor), (small_bar_height/3), 130, 130, 130, 255);

	// Magic
	Draw_ColoredString (left_column_x - getTextWidth("Magic", menu_scale_factor) + x_newline, left_column_y + y_newline, "Magic", 255, 255, 255, 255, menu_scale_factor);
	Draw_ColoredString (left_column_x - getTextWidth(info_magic, menu_scale_factor) + (getTextWidth("Magic", menu_scale_factor)/96) + x_newline, left_column_y + y_offset + y_newline, info_magic, 255, 255, 0, 255, menu_scale_factor);
	Draw_FillByColor (left_column_x - getTextWidth("Magic", menu_scale_factor) + x_newline, left_column_y + (y_offset*2) + y_newline, getTextWidth("Magic", menu_scale_factor), (small_bar_height/3), 130, 130, 130, 255);

	// Headshots Only
	Draw_ColoredString (left_column_x - getTextWidth("Headshots Only", menu_scale_factor), left_column_y + (y_newline*2), "Headshots Only", 255, 255, 255, 255, menu_scale_factor);
	Draw_ColoredString (left_column_x - getTextWidth(info_headshotonly, menu_scale_factor) + (getTextWidth("Headshots Only", menu_scale_factor)/96), left_column_y + y_offset + (y_newline*2), info_headshotonly, 255, 255, 0, 255, menu_scale_factor);
	Draw_FillByColor (left_column_x - getTextWidth("Headshots Only", menu_scale_factor), left_column_y + (y_offset*2) + (y_newline*2), getTextWidth("Headshots Only", menu_scale_factor), (small_bar_height/3), 130, 130, 130, 255);

	// Horde Size
	Draw_ColoredString (left_column_x - getTextWidth("Horde Size", menu_scale_factor) + x_newline, left_column_y + (y_newline*2), "Horde Size", 255, 255, 255, 255, menu_scale_factor);
	Draw_ColoredString (left_column_x - getTextWidth(info_hordesize, menu_scale_factor) + (getTextWidth("Horde Size", menu_scale_factor)/96) + x_newline, left_column_y + y_offset + (y_newline*2), info_hordesize, 255, 255, 0, 255, menu_scale_factor);
	Draw_FillByColor (left_column_x - getTextWidth("Horde Size", menu_scale_factor) + x_newline, left_column_y + (y_offset*2) + (y_newline*2), getTextWidth("Horde Size", menu_scale_factor), (small_bar_height/3), 130, 130, 130, 255);

	// Fast Rounds
	Draw_ColoredString (left_column_x - getTextWidth("Fast Rounds", menu_scale_factor), left_column_y + (y_newline*3), "Fast Rounds", 255, 255, 255, 255, menu_scale_factor);
	Draw_ColoredString (left_column_x - getTextWidth(info_fastrounds, menu_scale_factor) + (getTextWidth("Fast Rounds", menu_scale_factor)/96), left_column_y + y_offset + (y_newline*3), info_fastrounds, 255, 255, 0, 255, menu_scale_factor);
	Draw_FillByColor (left_column_x - getTextWidth("Fast Rounds", menu_scale_factor), left_column_y + (y_offset*2) + (y_newline*3), getTextWidth("Fast Rounds", menu_scale_factor), (small_bar_height/3), 130, 130, 130, 255);

	// Map Name (Pretty)
	Draw_ColoredString (image_x_pos, image_y_pos + image_height - (small_bar_height/2) - (vid.height/48), custom_maps[i].map_name_pretty, 255, 255, 0, 255, menu_scale_factor);
}

/*
======================
Menu_DrawBuildDate
======================
*/
void Menu_DrawBuildDate ()
{
	char* welcome_text = "Welcome, player  ";

	int y_pos = vid.height/64;
	int y_offset = vid.height/24;

	// Build date/information
	Draw_ColoredString((vid.width - getTextWidth(game_build_date, menu_scale_factor)), y_pos, game_build_date, 255, 255, 255, 255, menu_scale_factor);
	// Welcome text
	Draw_ColoredString((vid.width - getTextWidth(welcome_text, menu_scale_factor)), y_pos + y_offset, welcome_text, 255, 255, 0, 255, menu_scale_factor);
}

/*
======================
Menu_DrawDivider
======================
*/
void Menu_DrawDivider (int order)
{
	// y factor for vertical menu ordering
	int y_factor = (vid.height/16);
	
	int y_pos = ((vid.height/8) + (order*y_factor)) - (CHAR_HEIGHT/2);
	int x_length = (vid.width/3);
	int y_length = (vid.height/144);

	Draw_FillByColor(0, y_pos, x_length, y_length, 130, 130, 130, 255);
}

/*
======================
Menu_DrawMapPanel
======================
*/
void Menu_DrawMapPanel ()
{
	int x_pos = (vid.width/3) + (vid.width/36);
	int y_pos = big_bar_height + small_bar_height;

	// Big box
	Draw_FillByColor(x_pos, y_pos, vid.width - x_pos, vid.height - (y_pos*2), 0, 0, 0, 120);
	// Yellow bar
	Draw_FillByColor(x_pos, y_pos, (vid.width/136), vid.height - (y_pos*2), 255, 255, 0, 200);
}