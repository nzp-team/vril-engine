/*
Copyright (C) 2025-2026 NZ:P Team

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

float   		menu_text_scale_factor;

qboolean		menu_is_solo;

float			CHAR_WIDTH;
float			CHAR_HEIGHT;

int 			big_bar_height;
int 			small_bar_height;

int 			num_stock_maps;
char*			current_selected_bsp;

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
===============
Menu_DictateScaleFactor
===============
*/
void Menu_DictateScaleFactor(void)
{
	// Platform-dictated text scale.
#ifdef __NSPIRE__
	menu_text_scale_factor = 1.0f;
#elif __PSP__
	menu_text_scale_factor = 1.0f;
#elif __vita__
	menu_text_scale_factor = 2.0f;
#elif __3DS__
	menu_text_scale_factor = 1.0f;
#else
	menu_text_scale_factor = 1.0f;
#endif // __NSPIRE__, __PSP__, __vita__, __3DS__

	CHAR_WIDTH = 8*menu_text_scale_factor;
	CHAR_HEIGHT = 8*menu_text_scale_factor;

	big_bar_height = 25;
	small_bar_height = 2;
}

image_t Menu_PickBackground (void)
{
	// No custom images
	if (num_custom_images == 0) return menu_bk;

	int i = (rand() % num_custom_images);

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
	char map_command[64];

	for (i = 0; i < num_user_maps; i++) {
		if (!strcmp(selected_map, custom_maps[i].map_name)) 
			break;
	}

	map_loadname = current_selected_bsp;
    map_loadname_pretty = custom_maps[i].map_name_pretty;

	key_dest = key_game;
	if (sv.active) {
		Cbuf_AddText ("disconnect\n");
	}
	snprintf (map_command, sizeof(map_command), "map %s\n", map_loadname);
	Cbuf_AddText (map_command);
	loadingScreen = 1;
}

void Menu_SetOptionCvar (cvar_t option, char *option_string, int min_option_value, int max_option_value, float increment_amount)
{
	if (current_menu.slider_pressed != 0) {
		// set option value to be modified
		float current_option = option.value;
		// slider_pressed can be a value of
		// 1 or -1, determined by which
		// arrow key was pressed
		current_option = current_option + (current_menu.slider_pressed * increment_amount);

		if (current_option > max_option_value) {
			current_option = max_option_value;
		} else if (current_option < min_option_value) {
			current_option = min_option_value;
		}
		
		Cvar_SetValue (option_string, current_option);
		current_menu.slider_pressed = 0;
	}
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

void Menu_DrawFill (int x, int y, int width, int height, int r, int g, int b, int a)
{
	UI_Align (&x, &y);

	int x_value = UI_X(x);
	int y_value = UI_Y(y);
	int width_value = UI_W(width);
	int height_value = UI_H(height);

	Draw_FillByColor (x_value, y_value, width_value, height_value, r, g, b, a);
}

void Menu_DrawString (int x, int y, char* string, int r, int g, int b, int a, float scale, int FLIP_TEXT_POS_START)
{
	UI_Align (&x, &y);

	int x_value = UI_X((float)x);
	int y_value = UI_Y((float)y);

	if (FLIP_TEXT_POS_START) {
		x_value = x_value - getTextWidth(string, scale);
	}

	Draw_ColoredString (x_value, y_value, string, r, g, b, a, scale);
}

void Menu_DrawStringCentered (int x, int y, char* text, int r, int g, int b, int a)
{
	UI_Align (&x, &y);

	int x_value = UI_X((float)x);
	int y_value = UI_Y((float)y);

	x_value = x_value - (getTextWidth(text, menu_text_scale_factor)/2);

	Draw_ColoredString (x_value, y_value, text, r, g, b, a, menu_text_scale_factor);
}

void Menu_DrawPic (int x, int y, int index, int width, int height)
{
	UI_Align (&x, &y);

	int x_value = UI_X((float)x);
	int y_value = UI_Y((float)y);
	int width_value = UI_W((float)width);
	int height_value = UI_H((float)height);

	Draw_StretchPic(x_value, y_value, index, width_value, height_value);
}

void Menu_DrawSubPic (int x, int y, int pic, float s, float t, float s_coord_size, float t_coord_size, float scale, float r, float g , float b, float a)
{
	UI_Align (&x, &y);

	if (pic > 0) {
		Draw_SubPic(x, y, pic, s, t, s_coord_size, t_coord_size, scale, r, g, b, a);
	}
}

/*
======================
Menu_DrawDivider
======================
*/
void Menu_DrawDivider (int order)
{
	int y_factor = 15;
	int y_pos = 30 + (order*y_factor) - (CHAR_HEIGHT/2);
	int x_length = 140;
	int y_length = 1;

	UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_TOP);

	Menu_DrawFill(0, y_pos, x_length, y_length, 130, 130, 130, 255);
}

void Menu_DrawSocialBadge (int order, int which)
{
	UI_SetAlignment (UI_ANCHOR_RIGHT, UI_ANCHOR_BOTTOM);

	int y_factor = 15;
	int y_pos = ((big_bar_height + small_bar_height) + 5) + (order*y_factor);
	int x_pos = 20;
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
	int y_pos = 5;

	switch (color) {
		case MENU_COLOR_WHITE:
			Draw_ColoredString (x_pos, y_pos, title_name, 255, 255, 255, 255, menu_text_scale_factor*2);
			break;
		case MENU_COLOR_YELLOW:
			Draw_ColoredString (x_pos, y_pos, title_name, 255, 255, 0, 255, menu_text_scale_factor*2);
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
	int horizontal_border_height = 1;
	int vertical_border_width = 1;
	int border_height = (horizontal_border_height*2) + (CHAR_HEIGHT);
	int border_width = 140 + vertical_border_width;

	// Draw selection box
	// Done in 4 parts
	if (current_frame.point_y == UI_ANCHOR_TOP) {
		// Back(ground)
		Menu_DrawFill (0, y_pos - horizontal_border_height*4, border_width, border_height + (horizontal_border_height*4), 0, 0, 0, 120);
		// Top
		Menu_DrawFill (0, y_pos - (horizontal_border_height*4), border_width, horizontal_border_height, 255, 0, 0, 255);
		// Bottom
		Menu_DrawFill (0, y_pos + border_height, border_width, horizontal_border_height, 255, 0, 0, 255);
		// Side
		Menu_DrawFill (border_width, y_pos - (horizontal_border_height*4), vertical_border_width, border_height + (horizontal_border_height*4), 255, 0, 0, 255);
	} else if (current_frame.point_y == UI_ANCHOR_BOTTOM) {
		// Back(ground)
		Menu_DrawFill (0, y_pos + horizontal_border_height*4, border_width, border_height + (horizontal_border_height*4), 0, 0, 0, 120);
		// Top
		Menu_DrawFill (0, y_pos + (horizontal_border_height*4), border_width, horizontal_border_height, 255, 0, 0, 255);
		// Bottom
		Menu_DrawFill (0, y_pos - border_height, border_width, horizontal_border_height, 255, 0, 0, 255);
		// Side
		Menu_DrawFill (border_width, y_pos + (horizontal_border_height*4), vertical_border_width, border_height + (horizontal_border_height*4), 255, 0, 0, 255);
	}
	
}

/*
======================
Menu_DrawMapBorder
======================
*/
void Menu_DrawMapBorder (int x_pos, int y_pos, int image_width, int image_height)
{
	int border_side_width = 2;

	// Top
	Menu_DrawFill (x_pos, y_pos, image_width+border_side_width, small_bar_height, 130, 130, 130, 255);
	// Left Side
	Menu_DrawFill (x_pos, y_pos, border_side_width, image_height, 130, 130, 130, 255);
	// Right Side
	Menu_DrawFill (x_pos+image_width, y_pos, border_side_width, image_height, 130, 130, 130, 255);
	// Bottom
	if (current_frame.point_y == UI_ANCHOR_TOP) {
		Menu_DrawFill (x_pos, y_pos+image_height, image_width+border_side_width, small_bar_height, 130, 130, 130, 255);
	} else if (current_frame.point_y == UI_ANCHOR_BOTTOM) {
		Menu_DrawFill (x_pos, y_pos-image_height, image_width+border_side_width, small_bar_height, 130, 130, 130, 255);
	}
	
}


/*
======================
Menu_DrawMapPanel
======================
*/
void Menu_DrawMapPanel (void)
{
	int x_pos = 150;
	int y_pos = big_bar_height + small_bar_height;

	UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_TOP);
	// Big box
	Menu_DrawFill(x_pos, y_pos, (vid.width - x_pos), vid.height - (y_pos*2), 0, 0, 0, 120);
	// Yellow bar
	Menu_DrawFill(x_pos, y_pos, 2, vid.height - (y_pos*2), 255, 255, 0, 200);
}

void Menu_DrawSubMenu (char *line_one, char *line_two)
{
	int y_pos = 70;

	UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_TOP);
#ifndef MENU_DONT_DRAW_BACKGROUND_IMAGES
	// Dark red background
    Menu_DrawFill (0, 0, vid.width, vid.height, 255, 0, 0, 20);
#endif
    // Black background box
    Menu_DrawFill (0, y_pos, vid.width, 100, 0, 0, 0, 255);
    // Top yellow line
    Menu_DrawFill (0, y_pos-small_bar_height, vid.width, small_bar_height, 255, 255, 0, 255);
    // Bottom yellow line
    Menu_DrawFill (0, y_pos+100, vid.width, small_bar_height, 255, 255, 0, 255);

	UI_SetAlignment (UI_ANCHOR_CENTER, UI_ANCHOR_TOP);

	// Quit/Restart Text
    Menu_DrawStringCentered (0, y_pos + CHAR_HEIGHT, line_one, 255, 255, 255, 255);
    // Lose progress text
    Menu_DrawStringCentered (0, y_pos+(CHAR_HEIGHT*2), line_two, 255, 255, 255, 255);
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

	UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_TOP);

#ifdef MENU_DONT_DRAW_BACKGROUND_IMAGES
	draw_images = false;
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 255, 255);
#endif

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
	Menu_DrawFill(0, 0, vid.width, big_bar_height, 0, 0, 0, 228);
	Menu_DrawFill(0, big_bar_height, vid.width, small_bar_height, 130, 130, 130, 255);

	// Bottom Bars
	Menu_DrawFill(0, (vid.height - big_bar_height), vid.width, big_bar_height, 0, 0, 0, 228);
	Menu_DrawFill(0, (vid.height - big_bar_height) - small_bar_height, vid.width, small_bar_height, 130, 130, 130, 255);
}

/*
======================
Menu_DrawButton
======================
*/
void Menu_DrawButton (int order, int button_index, char* button_name, char* button_summary, void *on_activate)
{
	int y_factor = 15;
	int x_pos = 140;
	int y_pos = 0;

	if (order < 0) {
		y_pos = big_bar_height + ((order*-1)*y_factor);
		UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_BOTTOM);
	} else {
		y_pos = 30 + (order*y_factor);
		UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_TOP);
	}

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
		Menu_DrawString (x_pos, y_pos, button_name, 255, 0, 0, 255, menu_text_scale_factor, UI_FLIPTEXTPOS);

		// Draw the bottom screen text for selected button 
		UI_SetAlignment (UI_ANCHOR_CENTER, UI_ANCHOR_BOTTOM);
		Menu_DrawStringCentered (0, big_bar_height - CHAR_HEIGHT, button_summary, 255, 255, 255, 255);
	} else {
		// Not hovering over button
		if (order < 0) {
			UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_BOTTOM);
		} else {
			UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_TOP);
		}
		Menu_DrawString (x_pos, y_pos, button_name, 255, 255, 255, 255, menu_text_scale_factor, UI_FLIPTEXTPOS);
	}
}

/*
======================
Menu_DrawGreyButton
======================
*/
void Menu_DrawGreyButton (int order, char* button_name)
{
	int y_factor = 15;
	int x_pos = 140;
	int y_pos = 0;

	if (order < 0) {
		y_pos = big_bar_height + ((order*-1)*y_factor);
		UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_BOTTOM);
	} else {
		y_pos = 30 + (order*y_factor);
		UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_TOP);
	}

	Menu_DrawString (x_pos, y_pos, button_name, 128, 128, 128, 255, menu_text_scale_factor, UI_FLIPTEXTPOS);
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
	int desc_y_factor = 10;

	int image_width = vid.width/3;
	int image_height = vid.height/3;

	int map_region_center = 150 + (((vid.width-150)/2)+2);

	int x_pos = map_region_center - (image_width/2);
	int y_pos = 40;

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

		UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_TOP);
		// Draw map thumbnail picture
		Menu_DrawPic (x_pos, y_pos, menu_usermap_image[index], image_width, image_height);

		// Draw border around map image
		Menu_DrawMapBorder (x_pos, y_pos, image_width, image_height);

		// Draw Map Badge
		char *badge_name = NULL;
		float s_coord_size = 0.25f;
		float t_coord_size = 0.5f;
		float s = 0;
		float t = 0;
		float scale = (0.175f*menu_text_scale_factor);
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

		Menu_DrawString(x_pos + 4 + menu_badges_width, y_pos + image_height - (menu_badges_height/2) - (CHAR_HEIGHT/2), badge_name, 255, 255, 0, 255, menu_text_scale_factor, 0);
		Menu_DrawSubPic(x_pos + 4, y_pos + image_height - menu_badges_height, menu_badges, s, t, s_coord_size, t_coord_size, scale, 255, 255, 255, 255);

#ifndef MENU_HIDE_MAP_DESCRIPTIONS
		// Draw map description
		for (int j = 0; j < 8; j++) {
			Menu_DrawStringCentered (x_pos + (image_width/2), (y_pos + image_height + small_bar_height + (CHAR_HEIGHT/2)) + j*desc_y_factor, custom_maps[index].map_desc[j], 255, 255, 255, 255);
		}
#endif
		// Draw map author
		UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_BOTTOM);
		if (custom_maps[index].map_author != NULL) {
			Menu_DrawStringCentered (vid.width/2, 0 + CHAR_HEIGHT, custom_maps[index].map_author, 255, 255, 0, 255);		
		}
	}
}

void Menu_DrawOptionButton(int order, char* selection_name)
{
	int y_factor = 15;
	int x_pos = 165; 
	int y_pos = 30 + (order*y_factor);

	UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_TOP);
	Menu_DrawString(x_pos, y_pos, selection_name, 255, 255, 255, 255, menu_text_scale_factor, 0);
}

void Menu_DrawOptionSlider(int order, int button_index, int min_option_value, int max_option_value, cvar_t option, char* option_string, qboolean zero_to_one, qboolean draw_option_string, float increment_amount)
{
	int y_factor = 15;
	int x_pos = 165; 
	int y_pos = 30 + (order*y_factor);
	int slider_box_width = 100;
	int slider_box_height = 4;

	int slider_width = 2;

	if (Menu_IsButtonHovered(button_index)) {
		Menu_SetOptionCvar(option, option_string, min_option_value, max_option_value, increment_amount);
	}

	char _option_string[16] = "";
	// if increment amount is a float
	if (increment_amount < 1) {
		sprintf(_option_string, "%.2f", option.value);
	} else {
		sprintf(_option_string, "%i", (int)option.value);
	}

	if (zero_to_one) {
		if (!strcmp(_option_string, "0")) {
			strcpy(_option_string, "1");
		}
	}

	UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_TOP);

	float current_option = option.value;
	float option_pos = (current_option - min_option_value)/(max_option_value - min_option_value);
	// Slider box
	Menu_DrawFill(x_pos, y_pos, slider_box_width, slider_box_height, 255, 160, 160, 255);
	// Selection box
	Menu_DrawFill((x_pos+(option_pos*slider_box_width)) - slider_width, y_pos-2, slider_width, slider_box_height+4, 255, 255, 255, 255);
	// Cvar value string
	if (draw_option_string) {
		Menu_DrawString(x_pos + slider_box_width + CHAR_WIDTH, y_pos - 2, _option_string, 255, 255, 255, 255, menu_text_scale_factor, 0);
	}
}

void Menu_DrawOptionKey (int order, char *current_bind)
{
	int 	y_factor = 15;
	int 	x_pos = 165; 
	int 	y_pos = 28 + (order*y_factor);
	char 	*b;

	UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_TOP);

	int keynum = Key_StringToKeynum (current_bind);
	if (keynum > 0) {
		b = keybindings[keynum];

		if (!b) {
			return;
		}

		Menu_DrawPic (x_pos, y_pos, GetButtonIcon(b), 12, 12);
	} else {
		Menu_DrawString(x_pos, y_pos, current_bind, 255, 255, 0, 255, menu_text_scale_factor, 0);
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

	int image_width = vid.width/3;
	int image_height = vid.height/3;

	int map_region_center = 150 + (((vid.width-150)/2)+2);
	int image_x_pos = map_region_center - (image_width/2);
	int image_y_pos = ((big_bar_height+small_bar_height)+(CHAR_HEIGHT)/2) + image_height;

	int left_column_x = map_region_center - (image_width/2) + 20;
	int left_column_y = 30;

	int y_offset = 10;
	int y_newline = 25;
	int x_newline = image_width/1.33;

	int line_x = map_region_center - (image_width/2) - 8;

	texnum = Menu_GetMapImage (bsp_name);

	for (i = 0; i < num_user_maps; i++) {
		if (!strcmp(bsp_name, custom_maps[i].map_name)) 
			break;
	}

	// Set anchor for game settings display
	UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_TOP);
	// Game Mode
	Menu_DrawStringCentered (left_column_x, left_column_y, "Game Mode", 255, 255, 255, 255);
	Menu_DrawStringCentered (left_column_x, left_column_y + y_offset, info_gamemode, 255, 255, 0, 255);
	Menu_DrawFill (line_x, left_column_y + (y_offset*2), 55, (small_bar_height/2), 130, 130, 130, 255);

	// Difficulty
	Menu_DrawStringCentered (left_column_x + x_newline, left_column_y, "Difficulty", 255, 255, 255, 255);
	Menu_DrawStringCentered (left_column_x + x_newline, left_column_y + y_offset, info_difficulty, 255, 255, 0, 255);
	Menu_DrawFill (line_x + x_newline, left_column_y + (y_offset*2), 55, (small_bar_height/2), 130, 130, 130, 255);

	// Start Round
	Menu_DrawStringCentered (left_column_x, left_column_y + y_newline, "Start Round", 255, 255, 255, 255);
	Menu_DrawStringCentered (left_column_x, left_column_y + y_offset + y_newline, info_startround, 255, 255, 0, 255);
	Menu_DrawFill (line_x , left_column_y + (y_offset*2) + y_newline, 55, (small_bar_height/2), 130, 130, 130, 255);

	// Magic
	Menu_DrawStringCentered (left_column_x + x_newline, left_column_y + y_newline, "Magic", 255, 255, 255, 255);
	Menu_DrawStringCentered (left_column_x + x_newline, left_column_y + y_offset + y_newline, info_magic, 255, 255, 0, 255);
	Menu_DrawFill (line_x + x_newline, left_column_y + (y_offset*2) + y_newline, 55, (small_bar_height/2), 130, 130, 130, 255);

	// Headshots Only
	Menu_DrawStringCentered (left_column_x, left_column_y + (y_newline*2), "Headshots Only", 255, 255, 255, 255);
	Menu_DrawStringCentered (left_column_x, left_column_y + y_offset + (y_newline*2), info_headshotonly, 255, 255, 0, 255);
	Menu_DrawFill (line_x, left_column_y + (y_offset*2) + (y_newline*2), 55, (small_bar_height/2), 130, 130, 130, 255);

	// Horde Size
	Menu_DrawStringCentered (left_column_x + x_newline, left_column_y + (y_newline*2), "Horde Size", 255, 255, 255, 255);
	Menu_DrawStringCentered (left_column_x + x_newline, left_column_y + y_offset + (y_newline*2), info_hordesize, 255, 255, 0, 255);
	Menu_DrawFill (line_x + x_newline, left_column_y + (y_offset*2) + (y_newline*2), 55, (small_bar_height/2), 130, 130, 130, 255);

	// Fast Rounds
	Menu_DrawStringCentered (left_column_x, left_column_y + (y_newline*3), "Fast Rounds", 255, 255, 255, 255);
	Menu_DrawStringCentered (left_column_x, left_column_y + y_offset + (y_newline*3), info_fastrounds, 255, 255, 0, 255);
	Menu_DrawFill (line_x, left_column_y + (y_offset*2) + (y_newline*3), 55, (small_bar_height/2), 130, 130, 130, 255);

	// Draw Map picture and name
	// Anchored to bottom of the screen
	UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_BOTTOM);
	// Draw map thumbnail picture
	Menu_DrawPic (image_x_pos, image_y_pos, texnum, image_width, image_height);
	// Draw border around map image
	Menu_DrawMapBorder (image_x_pos, image_y_pos, image_width, image_height);
	// Map Name (Pretty)
	Menu_DrawStringCentered (image_x_pos + (image_width/2), image_y_pos - image_height + CHAR_HEIGHT, custom_maps[i].map_name_pretty, 255, 255, 0, 255);
}

/*
======================
Menu_DrawBuildDate
======================
*/
void Menu_DrawBuildDate (void)
{
	char* welcome_text = "Welcome, player  ";

	int y_pos = 4;
	int y_offset = 10;

	UI_SetAlignment (UI_ANCHOR_RIGHT, UI_ANCHOR_TOP);

	// Build date/information
	Menu_DrawString(0, y_pos, game_build_date, 255, 255, 255, 255, menu_text_scale_factor, UI_FLIPTEXTPOS);
	// Welcome text
	Menu_DrawString(0, y_pos + y_offset, welcome_text, 255, 255, 0, 255, menu_text_scale_factor, UI_FLIPTEXTPOS);
}

void Menu_DrawCreditHeader (int order, char *header)
{
	int y_factor = 30;
	int x_pos = 140; 
	int y_pos = 30 + (order*y_factor);

	UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_TOP);

	Menu_DrawString(x_pos, y_pos, header, 255, 255, 0, 255, menu_text_scale_factor, UI_FLIPTEXTPOS);
}

void Menu_DrawCreditContributor (int order, int sub_order, char *header)
{
	int y_factor = 30;
	int x_pos = 160; 
	int y_pos = 30 + (order*y_factor);

	y_pos += sub_order * CHAR_HEIGHT;

	UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_TOP);
	Menu_DrawString(x_pos, y_pos, header, 255, 255, 255, 255, menu_text_scale_factor, 0);
}

#ifdef PLATFORM_USES_OSK
//=============================================================================
/* OSK IMPLEMENTATION */
#define MAX_Y 8
#define MAX_X 12

#define MAX_CHAR_LINE 36
#define MAX_CHAR      72

int  osk_pos_x = 0;
int  osk_pos_y = 0;
int  max_len   = 0;

char* osk_out_buff = NULL;
char  osk_buffer[128];

image_t osk_button[4];

char *osk_text [] =
{
	" 1 2 3 4 5 6 7 8 9 0 - = ` ",
	" q w e r t y u i o p [ ]   ",
	"   a s d f g h j k l ; ' \\ ",
	"     z x c v b n m   , . / ",
	"                           ",
	" ! @ # $ % ^ & * ( ) _ + ~ ",
	" Q W E R T Y U I O P { }   ",
	"   A S D F G H J K L : \" | ",
	"     Z X C V B N M   < > ? "
};

void Con_OSK_f (char *input, char *output, int outlen)
{
	max_len = outlen;
	strncpy(osk_buffer,input,max_len);
	osk_buffer[outlen] = '\0';
	osk_out_buff = output;
}

void Con_SetOSKActive(qboolean active);
void Menu_OSK_Key (int key)
{
    switch (key)
	{
	case K_RIGHTARROW:
		osk_pos_x++;
		if (osk_pos_x > MAX_X)
			osk_pos_x = 0;//MAX_X
		break;
	case K_LEFTARROW:
		osk_pos_x--;
		if (osk_pos_x < 0)
			osk_pos_x = MAX_X;//0
		break;
	case K_DOWNARROW:
		osk_pos_y++;
		if (osk_pos_y > MAX_Y)
			osk_pos_y = 0;//MAX_Y
		break;
	case K_UPARROW:
		osk_pos_y--;
		if (osk_pos_y < 0)
			osk_pos_y = MAX_Y;//0
		break;
	default:
		break;
	}

	if (key == MENU_KEY_CONFIRM) {
		if (max_len > strlen(osk_buffer)) {
			char *selected_line = osk_text[osk_pos_y];
			char selected_char[2];

			selected_char[0] = selected_line[1+(2*osk_pos_x)];

			if (selected_char[0] == '\t')
				selected_char[0] = ' ';

			selected_char[1] = '\0';
			strcat(osk_buffer,selected_char);
		}
	} else if (key == MENU_KEY_SAVE_INPUT) {
		strncpy(osk_out_buff,osk_buffer,max_len);
		Con_SetOSKActive(false);
	} else if (key == MENU_KEY_DELETE) {
		if (strlen(osk_buffer) > 0) {
			osk_buffer[strlen(osk_buffer)-1] = '\0';
		}
	} else if (key == MENU_KEY_BACK) {
		Con_SetOSKActive(false);
	}
}

void Menu_OSK_Draw (void)
{
	int i;

	char *selected_line = osk_text[osk_pos_y];
	char selected_char[2];

	selected_char[0] = selected_line[1+(2*osk_pos_x)];
	selected_char[1] = '\0';
	if (selected_char[0] == ' ' || selected_char[0] == '\t')
		selected_char[0] = 'X';

	int x_value = (vid.width/3)+80;
	int y_value = (vid.height/3)+20;

	int x_pos = vid.width/4;
	int y_pos = vid.height/2 - y_value;

	UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_TOP);

	// Draw the background fill
	Menu_DrawFill(x_pos-5, y_pos-10, x_value, y_value, 0, 0, 0, 255);
	// Map border is fine for this occassion 
	Menu_DrawMapBorder (x_pos-5, y_pos-10, x_value, y_value);

	for(i=0;i<=MAX_Y;i++) {
		Menu_DrawString (x_pos, y_pos+(CHAR_WIDTH*i), osk_text[i], 255, 255, 255, 255, menu_text_scale_factor, 0);
		
		if (i == 0) {
			Menu_DrawString (x_pos+(x_value)-88, y_pos+(CHAR_HEIGHT*i), "CONFIRM", 255, 255, 0, 255, menu_text_scale_factor, 0);
			Menu_DrawPic (x_pos+(x_value)-20, y_pos+(CHAR_HEIGHT*i), osk_button[0], 8, 8);
		} else if (i == 2) {
			Menu_DrawString (x_pos+(x_value)-88, y_pos+(CHAR_HEIGHT*i), "CANCEL", 255, 255, 0, 255, menu_text_scale_factor, 0);
			Menu_DrawPic (x_pos+(x_value)-20, y_pos+(CHAR_HEIGHT*i), osk_button[1], 8, 8);
		} else if (i == 4) {
			Menu_DrawString (x_pos+(x_value)-88, y_pos+(CHAR_HEIGHT*i), "DELETE", 255, 255, 0, 255, menu_text_scale_factor, 0);
			Menu_DrawPic (x_pos+(x_value)-20, y_pos+(CHAR_HEIGHT*i), osk_button[2], 8, 8);
		} else if (i == 6) {
			Menu_DrawString (x_pos+(x_value)-88, y_pos+(CHAR_HEIGHT*i), "ADD CHAR", 255, 255, 0, 255, menu_text_scale_factor, 0);
			Menu_DrawPic (x_pos+(x_value)-20, y_pos+(CHAR_HEIGHT*i), osk_button[3], 8, 8);
		}
	}
	// Side bar
	Menu_DrawFill(x_pos+(x_value)-95, y_pos-10, 2, y_value-26, 130, 130, 130, 255);
	// Bottom bar
	Menu_DrawFill(x_pos-3, y_pos+y_value-36, x_value-2, 2, 130, 130, 130, 255);
	
	int text_len = strlen(osk_buffer);
	if (text_len > MAX_CHAR_LINE) {
		char oneline[MAX_CHAR_LINE+1];
		strncpy(oneline,osk_buffer,MAX_CHAR_LINE);
		oneline[MAX_CHAR_LINE] = '\0';

		Menu_DrawString (x_pos, y_pos+4+(CHAR_HEIGHT*(MAX_Y+2)), oneline, 255, 0, 0, 255, menu_text_scale_factor, 0);

		strncpy(oneline,osk_buffer+MAX_CHAR_LINE, text_len - MAX_CHAR_LINE);
		oneline[text_len - MAX_CHAR_LINE] = '\0';

		// Current input char
		Menu_DrawString (x_pos+(CHAR_WIDTH/2), y_pos+4+(CHAR_HEIGHT*(MAX_Y+3)), oneline, 255, 255, 255, 255, menu_text_scale_factor, 0);
		// Current cursor
		Menu_DrawFill (x_pos+((CHAR_WIDTH)*(text_len)), y_pos+4+(CHAR_HEIGHT*(MAX_Y+2)), 1, CHAR_HEIGHT, 255, 0, 0, 255);
	} else {
		// Current input char
		Menu_DrawString (x_pos+(CHAR_WIDTH/2), y_pos+4+(CHAR_HEIGHT*(MAX_Y+2)), osk_buffer, 255, 255, 255, 255, menu_text_scale_factor, 0);
		// Current cursor
		Menu_DrawFill (x_pos+((CHAR_WIDTH)*(text_len)), y_pos+4+(CHAR_HEIGHT*(MAX_Y+2)), 1, CHAR_HEIGHT, 255, 0, 0, 255);
	}
	// Current hovered char
	Menu_DrawString (x_pos+(CHAR_WIDTH/2)+(((osk_pos_x*1.25))*(CHAR_WIDTH)), y_pos+(osk_pos_y*CHAR_HEIGHT), selected_char, 255, 0, 0, 255, menu_text_scale_factor, 0);
}
#endif