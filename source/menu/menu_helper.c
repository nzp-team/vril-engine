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
#include <assert.h>

float   		menu_scale_factor;

qboolean		menu_is_solo;

float			CHAR_WIDTH;
float			CHAR_HEIGHT;

int 			big_bar_height;
int 			small_bar_height;

int 			num_stock_maps;
char*			current_selected_bsp;

StockMaps 		stock_maps[8] = {
	[0] = { .bsp_name = "ndu", .category = MAP_CATEGORY_WAW, .array_index = 0 },
	[1] = { .bsp_name = "nzp_warehouse2", .category = MAP_CATEGORY_NZP, .array_index = 0 },
	[2] = { .bsp_name = "nzp_xmas2", .category = MAP_CATEGORY_NZP, .array_index = 0 },
	[3] = { .bsp_name = "nzp_warehouse", .category = MAP_CATEGORY_NZPBETA, .array_index = 0 },
	[4] = { .bsp_name = "christmas_special", .category = MAP_CATEGORY_NZPBETA, .array_index = 0 },
	[5] = { .bsp_name = "lexi_house", .category = MAP_CATEGORY_BLACKOPSDS, .array_index = 0 },
	[6] = { .bsp_name = "lexi_temple", .category = MAP_CATEGORY_BLACKOPSDS, .array_index = 0 },
	[7] = { .bsp_name = "lexi_overlook", .category = MAP_CATEGORY_BLACKOPSDS, .array_index = 0 }
};

void strip_newline(char *s)
{
    s[strcspn(s, "\r\n")] = '\0';
}

// Lower string
char* strtolower(char* s) 
{
	assert(s != NULL);

	char* p = s;
	while (*p != '\0') {
		*p = tolower(*p);
		p++;
	}

	return s;
}

// Upper string
char* strtoupper(char* s) 
{
	assert(s != NULL);

	char* p = s;
	while (*p != '\0') {
		*p = toupper(*p);
		p++;
	}

	return s;
}

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
	menu_badges = Image_LoadImage("gfx/menu/map_badges", IMAGE_TGA, 0, false, false);

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

image_t Menu_PickBackground (void)
{
	// No custom images
	if (num_custom_images == 0) return menu_bk;

	int i = (rand() % num_custom_images);

    return (image_t)menu_usermap_image[i];
}

void Menu_DrawTextCentered (int x, int y, char* text, int r, int g, int b, int a)
{
	int x_pos = x - (getTextWidth(text, menu_scale_factor)/2);

	Draw_ColoredString (x_pos, y, text, r, g, b, a, menu_scale_factor);
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

qboolean Menu_IsStockMap (char *bsp_name)
{
	for (int i = 0; i < num_stock_maps; i++) {
		if (!strcmp(stock_maps[i].bsp_name, bsp_name)) {
			return true;
		}
	}

	return false;
}

int Menu_UserMapSupportsGameSettings (char *bsp_name)
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

int Menu_GetActiveMenuButtons (void)
{
	int num_active_buttons = 0;

	for (int i = 0; i < MAX_MENU_BUTTONS; i++) {
		if (current_menu.button[i].enabled) {
			num_active_buttons++;
		}
	}

	return num_active_buttons;
}

void Menu_IncreaseCursor (void)
{
	int current_active_buttons = Menu_GetActiveMenuButtons();

	Menu_StartSound(MENU_SND_NAVIGATE);

	if (--current_menu.cursor < 0) {
		current_menu.cursor = current_active_buttons-1;
	}
}

void Menu_DecreaseCursor (void)
{
	int current_active_buttons = Menu_GetActiveMenuButtons();

	Menu_StartSound(MENU_SND_NAVIGATE);

	if (++current_menu.cursor >= current_active_buttons) {
		current_menu.cursor = 0;
	}
}

void Menu_ButtonPress (void)
{
	for (int i = 0; i < MAX_MENU_BUTTONS; i++) {
		if (!current_menu.button[i].enabled) {
			break;
		}

		if (current_menu.cursor == i) {
			if (current_menu.button[i].on_activate != NULL) {
				Menu_StartSound(MENU_SND_ENTER);
				current_menu.button[i].on_activate();
				break;
			}
		}
	}
}

void Menu_IncrementSlider (int dir)
{
	for (int i = 0; i < MAX_MENU_BUTTONS; i++) {
		if (!current_menu.button[i].enabled) {
			break;
		}

		if (current_menu.cursor == i) {
			Menu_StartSound(MENU_SND_NAVIGATE);
			if (dir == K_LEFTARROW) {
				current_menu.slider_pressed = -1;
			} else if (dir == K_RIGHTARROW) {
				current_menu.slider_pressed = 1;
			}
			break;
		}
	}
}

void Menu_KeyInput (int key)
{
	switch (key)
	{
	case K_DOWNARROW:
		Menu_DecreaseCursor();
		break;

	case K_UPARROW:
		Menu_IncreaseCursor();
		break;

	case K_LEFTARROW:
		Menu_IncrementSlider(key);
		break;

	case K_RIGHTARROW:
		Menu_IncrementSlider(key);
		break;

	case K_ENTER:
	case K_AUX1:
		Menu_ButtonPress();
		break;
	}
}

void Menu_SetOptionCvar (cvar_t option, char *option_string, int min_option_value, int max_option_value, int increment_amount)
{
	if (current_menu.slider_pressed != 0) {

		float current_option = option.value;
		current_option += (current_menu.slider_pressed*increment_amount);

		if (current_option > max_option_value) {
			current_option = max_option_value;
		} else if (current_option < min_option_value) {
			current_option = min_option_value;
		}
		
		Cvar_SetValue (option_string, current_option);
	}
	current_menu.slider_pressed = 0;
}

qboolean Menu_IsButtonHovered (int button_index)
{
	if (button_index == current_menu.cursor) {
		return true;
	}

	return false;
}

void Menu_BuildMenuButtons (int button_index, char *button_name, void *on_activate)
{
	// Button is currently inactive,
	// so set its' attributes
	if (!current_menu.button[button_index].enabled) {
		current_menu.button[button_index].enabled = true;
		current_menu.button[button_index].index = button_index;
		current_menu.button[button_index].name = button_name;
		current_menu.button[button_index].on_activate = on_activate;
	} else {
		// This button is now disabled
		// right now this should never happen
		// unless menu state is undergoing a change
		return;
	}
}

/*
======================
Menu_DrawCustomBackground
======================
*/
void Menu_DrawCustomBackground (qboolean draw_images)
{
    float elapsed_background_time = menu_time - menu_starttime;

	// TODO

	// 
	// Slight background pan
	//

	// TODO

	// Only draw background images if in active
	// menu and not in a game
	if (draw_images && key_dest != key_menu_pause) {
		float time = (float)((vid.width * 1.05) - vid.width) * (elapsed_background_time/7);

		Draw_MenuPanningPic(0, 0, menu_background, vid.width, vid.height, time);
		Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 50);

		//
		// Fade new image in/out
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
		Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 50);
	}

	// Top Bars
	Draw_FillByColor(0, 0, vid.width, big_bar_height, 0, 0, 0, 228);
	Draw_FillByColor(0, big_bar_height, vid.width, small_bar_height, 130, 130, 130, 255);

	// Bottom Bars
	Draw_FillByColor(0, (vid.height - big_bar_height), vid.width, big_bar_height, 0, 0, 0, 228);
	Draw_FillByColor(0, (vid.height - big_bar_height) - small_bar_height, vid.width, small_bar_height, 130, 130, 130, 255);
}

void Menu_DrawSubPic (int x, int y, int pic, float s, float t, float s_coord_size, float t_coord_size, float scale, float r, float g , float b, float a)
{
	if (pic > 0) {
		Draw_SubPic(x, y, pic, s, t, s_coord_size, t_coord_size, scale, r, g, b, a);
	}
}

void Menu_DrawSocialBadge (int order, int which)
{
	int y_factor = (vid.height/16);
	int y_pos = (vid.height/2) + (vid.width/36) + (order*y_factor);
	int x_pos = vid.width - (vid.width/20);
	float coord_size = 0.5f;
	float scale = 0.25f;
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

	Menu_DrawSubPic (x_pos, y_pos, menu_social, s, t, coord_size, coord_size, scale, 255, 255, 255, 255);
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
	// Done in 4 parts
	// Back(ground)
	Draw_FillByColor (0, y_pos - border_height_offset, x_length, border_width, 255, 0, 0, 255);
	// Top
	Draw_FillByColor (0, y_pos - border_height_offset, x_length, (border_height_offset*2) + CHAR_HEIGHT + border_width, 0, 0, 0, 86);
	// Bottom
	Draw_FillByColor (0, (y_pos + (border_height_offset*2)) + (CHAR_HEIGHT/2), x_length, border_width, 255, 0, 0, 255);
	// Side
	Draw_FillByColor (x_length, (y_pos - (CHAR_HEIGHT/2)) + (border_height_offset), border_width, border_height_offset + CHAR_HEIGHT + border_width, 255, 0, 0, 255);
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
void Menu_DrawButton (int order, int button_index, char* button_name, char* button_summary, void *on_activate)
{
	int y_factor = (vid.height/16);
	int x_pos = (vid.width/3) - getTextWidth(button_name, menu_scale_factor); 
	int y_pos = (vid.height/8) + (order*y_factor);

	// Button is currently inactive,
	// so set its' attributes
	if (!current_menu.button[button_index].enabled) {
		Menu_BuildMenuButtons (button_index, button_name, on_activate);
	}

	// Active menu buttons
	// Hovering over button
	if (Menu_IsButtonHovered(button_index)) {
		// Make button red and add a box around it

		// Draw selection box
		Menu_DrawSelectionBox (x_pos, y_pos);

		// Draw button string
		Draw_ColoredString (x_pos, y_pos, button_name, 255, 0, 0, 255, menu_scale_factor);

		// Draw the bottom screen text for selected button 
		Draw_ColoredStringCentered (vid.height - (vid.height/12), button_summary, 255, 255, 255, 255, menu_scale_factor);
	} else {
		// Not hovering over button
		Draw_ColoredString (x_pos, y_pos, button_name, 255, 255, 255, 255, menu_scale_factor);
	}
}

/*
======================
Menu_DrawGreyButton
======================
*/
void Menu_DrawGreyButton (int order, char* button_name)
{
	int y_factor = (vid.height/16);
	int x_pos = (vid.width/3) - getTextWidth(button_name, menu_scale_factor); 
	int y_pos = (vid.height/8) + (order*y_factor);

	Draw_ColoredString (x_pos, y_pos, button_name, 128, 128, 128, 255, menu_scale_factor);
}

/*
======================
Menu_DrawMapButton
======================
*/
void Menu_DrawMapButton (int order, int button_index, int usermap_index, int map_category, char* bsp_name, void *on_activate)
{
	int i;
	int index = 0;
	char *button_name;
	char *final_name;
	int desc_y_factor = (vid.height/24);

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
	if (button_name == NULL) {
		button_name = custom_maps[index].map_name;
	}

	// Don't modify map_name_pretty memory location
	// create a copy for uppercase drawing
	final_name = malloc(MAX_QPATH);
	strcpy(final_name, button_name);
	strtoupper(final_name);

	// Draw the selectable button
	Menu_DrawButton (order, button_index, final_name, button_name, on_activate);
	if (final_name != NULL) {
		free (final_name);
	}

	if (Menu_IsButtonHovered(button_index)) {
		// Draw map thumbnail picture
		Draw_StretchPic (x_pos, y_pos, menu_usermap_image[index], image_width, image_height);

		// Draw border around map image
		Menu_DrawMapBorder (x_pos, y_pos, image_width, image_height);

		// Draw Map Badge
		char *badge_name = NULL;
		float s_coord_size = 0.25f;
		float t_coord_size = 0.5f;
		float s = 0;
		float t = 0;
		float scale = (0.175f*menu_scale_factor);
		// Get width/height of the menu badges image
		// hard-coded until there's a global way to 
		// get image dimensions
		int	menu_badges_height = (72*scale);
		int menu_badges_width = (72*scale);

		switch(map_category) {
			case MAP_CATEGORY_NZPBETA: s = 0; t = 0; badge_name = "NZ:P BETA (2011)"; break;
			case MAP_CATEGORY_WAW: s = 0.25; t = 0; badge_name = "WORLD AT WAR"; break;
			case MAP_CATEGORY_NZP: s = 0; t = 0.5; badge_name = "NZ:P ORIGINAL"; break;
			case MAP_CATEGORY_BLACKOPSDS: s = 0.25; t = 0.5; badge_name = "BLACK OPS (DS)"; break;
			case MAP_CATEGORY_USER: s = 0.5; t = 0; badge_name = "USERMAP"; break;
		}

		Draw_ColoredString(x_pos + ((vid.width/144)*1.5) + menu_badges_width, y_pos + image_height - (menu_badges_height/2) - (CHAR_HEIGHT/2), badge_name, 255, 255, 0, 255, menu_scale_factor);
		Menu_DrawSubPic(x_pos + (vid.width/144), y_pos + image_height - menu_badges_height, menu_badges, s, t, s_coord_size, t_coord_size, scale, 255, 255, 255, 255);

		// Draw map description
		for (int j = 0; j < 8; j++) {
			Menu_DrawTextCentered (x_pos + (vid.width/6), (y_pos + image_height + (small_bar_height*2)) + j*desc_y_factor, custom_maps[index].map_desc[j], 255, 255, 255, 255);
		}

		// Draw map author
		if (custom_maps[index].map_author != NULL) {
			Draw_ColoredStringCentered (vid.height - (vid.height/24), custom_maps[index].map_author, 255, 255, 0, 255, menu_scale_factor);
		}
	}
}

void Menu_DrawOptionButton(int order, char* selection_name)
{
	int y_factor = (vid.height/16);
	int x_pos = (vid.width/3) + (vid.width/16); 
	int y_pos = (vid.height/8) + (order*y_factor);

	Draw_ColoredString(x_pos, y_pos, selection_name, 255, 255, 255, 255, menu_scale_factor);
}

void Menu_DrawOptionSlider(int order, int button_index, int min_option_value, int max_option_value, cvar_t option, char* _option_string, qboolean zero_to_one, qboolean draw_option_string, float increment_amount)
{
	int y_factor = (vid.height/16);
	int x_pos = (vid.width/3) + (vid.width/16); 
	int y_pos = (vid.height/8) + (order*y_factor);
	int slider_box_width = (vid.width/4);
	int slider_box_height = (vid.height/48);

	int slider_width = (vid.width/128);

	if (Menu_IsButtonHovered(button_index)) {
		Menu_SetOptionCvar(option, _option_string, min_option_value, max_option_value, increment_amount);
	}

	char option_string[16] = "";
	sprintf(option_string, "%d", (int)option.value);

	if (zero_to_one) {
		if (!strcmp(option_string, "0")) {
			strcpy(option_string, "1");
		}
	}

	float current_option = option.value;
	float option_pos = (current_option - min_option_value)/(max_option_value - min_option_value);
	// Slider box
	Draw_FillByColor(x_pos, y_pos, slider_box_width, slider_box_height, 255, 160, 160, 255);
	// Selection box
	Draw_FillByColor((x_pos+(option_pos*slider_box_width)) - slider_width, y_pos-(vid.height/64), slider_width, slider_box_height+((vid.height/64)*2), 255, 255, 255, 255);
	// Cvar value string
	if (draw_option_string) {
		Draw_ColoredString(x_pos + slider_box_width + (vid.width/96), y_pos, option_string, 255, 255, 255, 255, menu_scale_factor);
	}
}

void Menu_DrawSubMenu (char *line_one, char *line_two)
{
	int y_pos = (vid.height/3);

	// Dark red background
    Draw_FillByColor (0, 0, vid.width, vid.height, 255, 0, 0, 20);
    // Black background box
    Draw_FillByColor (0, vid.height/3, vid.width, vid.height/3, 0, 0, 0, 255);
    // Top yellow line
    Draw_FillByColor (0, (vid.height/3)-small_bar_height, vid.width, small_bar_height, 255, 255, 0, 255);
    // Bottom yellow line
    Draw_FillByColor (0, vid.height-(vid.height/3), vid.width, small_bar_height, 255, 255, 0, 255);

	// Quit/Restart Text
    Menu_DrawTextCentered (vid.width/2, y_pos+CHAR_HEIGHT, line_one, 255, 255, 255, 255);
    // Lose progress text
    Menu_DrawTextCentered (vid.width/2, y_pos+(CHAR_HEIGHT*2), line_two, 255, 255, 255, 255);
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

	int image_width = (vid.width/3.5);
	int image_height = (vid.height/3.5);

	int image_x_pos = (vid.width/2) + (vid.width/28);
	int image_y_pos = (vid.height) - (big_bar_height + small_bar_height) - (image_height) - (vid.height/96);

	int left_column_x = (vid.width/3) + (vid.width/5);
	int left_column_y = (vid.height/6);

	int y_offset = CHAR_HEIGHT;
	int y_newline = (vid.height/10);
	int x_newline = (vid.width/4);

	int line_x = (vid.width/3) + (vid.width/8.5);

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
	Menu_DrawTextCentered (left_column_x, left_column_y, "Game Mode", 255, 255, 255, 255);
	Menu_DrawTextCentered (left_column_x, left_column_y + y_offset, info_gamemode, 255, 255, 0, 255);
	Draw_FillByColor (line_x, left_column_y + (y_offset*2), (vid.width/6), (small_bar_height/3), 130, 130, 130, 255);

	// Difficulty
	Menu_DrawTextCentered (left_column_x + x_newline, left_column_y, "Difficulty", 255, 255, 255, 255);
	Menu_DrawTextCentered (left_column_x + x_newline, left_column_y + y_offset, info_difficulty, 255, 255, 0, 255);
	Draw_FillByColor (line_x + x_newline, left_column_y + (y_offset*2), (vid.width/6), (small_bar_height/3), 130, 130, 130, 255);

	// Start Round
	Menu_DrawTextCentered (left_column_x, left_column_y + y_newline, "Start Round", 255, 255, 255, 255);
	Menu_DrawTextCentered (left_column_x, left_column_y + y_offset + y_newline, info_startround, 255, 255, 0, 255);
	Draw_FillByColor (line_x , left_column_y + (y_offset*2) + y_newline, (vid.width/6), (small_bar_height/3), 130, 130, 130, 255);

	// Magic
	Menu_DrawTextCentered (left_column_x + x_newline, left_column_y + y_newline, "Magic", 255, 255, 255, 255);
	Menu_DrawTextCentered (left_column_x + x_newline, left_column_y + y_offset + y_newline, info_magic, 255, 255, 0, 255);
	Draw_FillByColor (line_x + x_newline, left_column_y + (y_offset*2) + y_newline, (vid.width/6), (small_bar_height/3), 130, 130, 130, 255);

	// Headshots Only
	Menu_DrawTextCentered (left_column_x, left_column_y + (y_newline*2), "Headshots Only", 255, 255, 255, 255);
	Menu_DrawTextCentered (left_column_x, left_column_y + y_offset + (y_newline*2), info_headshotonly, 255, 255, 0, 255);
	Draw_FillByColor (line_x, left_column_y + (y_offset*2) + (y_newline*2), (vid.width/6), (small_bar_height/3), 130, 130, 130, 255);

	// Horde Size
	Menu_DrawTextCentered (left_column_x + x_newline, left_column_y + (y_newline*2), "Horde Size", 255, 255, 255, 255);
	Menu_DrawTextCentered (left_column_x + x_newline, left_column_y + y_offset + (y_newline*2), info_hordesize, 255, 255, 0, 255);
	Draw_FillByColor (line_x + x_newline, left_column_y + (y_offset*2) + (y_newline*2), (vid.width/6), (small_bar_height/3), 130, 130, 130, 255);

	// Fast Rounds
	Menu_DrawTextCentered (left_column_x, left_column_y + (y_newline*3), "Fast Rounds", 255, 255, 255, 255);
	Menu_DrawTextCentered (left_column_x, left_column_y + y_offset + (y_newline*3), info_fastrounds, 255, 255, 0, 255);
	Draw_FillByColor (line_x, left_column_y + (y_offset*2) + (y_newline*3), (vid.width/6), (small_bar_height/3), 130, 130, 130, 255);

	// Map Name (Pretty)
	Menu_DrawTextCentered (image_x_pos + (image_width/2), image_y_pos + image_height - (small_bar_height) - (vid.height/32), custom_maps[i].map_name_pretty, 255, 255, 0, 255);
}

/*
======================
Menu_DrawBuildDate
======================
*/
void Menu_DrawBuildDate (void)
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
void Menu_DrawMapPanel (void)
{
	int x_pos = (vid.width/3) + (vid.width/36);
	int y_pos = big_bar_height + small_bar_height;

	// Big box
	Draw_FillByColor(x_pos, y_pos, vid.width - x_pos, vid.height - (y_pos*2), 0, 0, 0, 120);
	// Yellow bar
	Draw_FillByColor(x_pos, y_pos, (vid.width/136), vid.height - (y_pos*2), 255, 255, 0, 200);
}