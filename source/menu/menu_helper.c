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

	big_bar_height = STD_UI_HEIGHT/9;
	small_bar_height = STD_UI_HEIGHT/64;
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
	char map_command[32];

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

void Menu_DrawFill (int x, int y, int width, int height, int r, int g, int b, int a, int UI_ANCHOR)
{
	UI_Align (&x, &y, width, height, UI_ANCHOR);

	float x_value = UI_X((float)x);
	float y_value = UI_Y((float)y);
	float width_value = UI_W((float)width);
	float height_value = UI_H((float)height);

	Draw_FillByColor ((int)x_value, (int)y_value, (int)width_value, (int)height_value, r, g, b, a);
}

void Menu_DrawString (int x, int y, char* string, int r, int g, int b, int a, float scale, int UI_ANCHOR, int FLIP_TEXT_POS)
{
	UI_Align (&x, &y, 0, 0, UI_ANCHOR);

	float x_value = UI_X((float)x);
	float y_value = UI_Y((float)y);

	if (FLIP_TEXT_POS) {
		x_value = x_value - getTextWidth(string, menu_text_scale_factor);
	}

	Draw_ColoredString ((int)x_value, (int)y_value, string, r, g, b, a, scale);
}

void Menu_DrawStringCentered (int x, int y, char* text, int r, int g, int b, int a)
{
	UI_Align (&x, &y, 0, 0, UI_ANCHOR_TOPLEFT);

	float x_value = UI_X((float)x);
	float y_value = UI_Y((float)y);

	x_value = x_value - (getTextWidth(text, menu_text_scale_factor)/2);

	Draw_ColoredString (x, y, text, r, g, b, a, menu_text_scale_factor);
}

void Menu_DrawPic (int x, int y, int index, int width, int height, int UI_ANCHOR)
{
	float x_value = UI_X((float)x);
	float y_value = UI_Y((float)y);
	float width_value = UI_W((float)width);
	float height_value = UI_H((float)height);

	Draw_StretchPic((int)x_value, (int)y_value, index, (int)width_value, (int)height_value);
}

void Menu_DrawSubPic (int x, int y, int pic, float s, float t, float s_coord_size, float t_coord_size, float scale, float r, float g , float b, float a)
{
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
	int y_factor = (STD_UI_HEIGHT/16);
	int y_pos = ((STD_UI_HEIGHT/8) + (order*y_factor)) - (CHAR_HEIGHT/2);
	int x_length = STD_UI_WIDTH/3;
	int y_length = STD_UI_HEIGHT/144;

	Menu_DrawFill(0, y_pos, x_length, y_length, 130, 130, 130, 255, UI_ANCHOR_TOPLEFT);
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
void Menu_DrawSelectionBox (int x_pos, int y_pos, int UI_ANCHOR)
{
	int horizontal_border_height = (STD_UI_HEIGHT/144);
	int vertical_border_width = (STD_UI_WIDTH/256);
	int border_height = (horizontal_border_height*2) + (CHAR_HEIGHT);
	int border_width = (STD_UI_WIDTH/3) + vertical_border_width;

	// Draw selection box
	// Done in 4 parts
	// Back(ground)
	Menu_DrawFill (0, y_pos - horizontal_border_height, border_width, border_height + (horizontal_border_height*2), 0, 0, 0, 86, UI_ANCHOR);
	// Top
	Menu_DrawFill (0, y_pos - (horizontal_border_height*2), border_width, horizontal_border_height, 255, 0, 0, 255, UI_ANCHOR);
	// Bottom
	Menu_DrawFill (0, y_pos + border_height, border_width, horizontal_border_height, 255, 0, 0, 255, UI_ANCHOR);
	// Side
	Menu_DrawFill (border_width, y_pos - (horizontal_border_height*2), vertical_border_width, border_height + (horizontal_border_height*3), 255, 0, 0, 255, UI_ANCHOR);
}

/*
======================
Menu_DrawMapBorder
======================
*/
void Menu_DrawMapBorder (int x_pos, int y_pos, int image_width, int image_height, int UI_ANCHOR)
{
	int border_side_width = (STD_UI_WIDTH/144)/2;

	// Top
	Menu_DrawFill (x_pos, y_pos, image_width+border_side_width, small_bar_height/2, 130, 130, 130, 255, UI_ANCHOR);
	// Left Side
	Menu_DrawFill (x_pos, y_pos, border_side_width, image_height, 130, 130, 130, 255, UI_ANCHOR);
	// Right Side
	Menu_DrawFill (x_pos+image_width, y_pos, border_side_width, image_height, 130, 130, 130, 255, UI_ANCHOR);
	// Bottom
	Menu_DrawFill (x_pos, y_pos+image_height, image_width+border_side_width, small_bar_height/2, 130, 130, 130, 255, UI_ANCHOR);
}


/*
======================
Menu_DrawMapPanel
======================
*/
void Menu_DrawMapPanel (void)
{
	int x_pos = (STD_UI_WIDTH/3);
	int y_pos = big_bar_height + small_bar_height;

	// Big box
	Menu_DrawFill(x_pos, y_pos, STD_UI_WIDTH - x_pos, STD_UI_HEIGHT - (y_pos*2), 0, 0, 0, 120, UI_ANCHOR_TOPRIGHT);
	// Yellow bar
	Menu_DrawFill(x_pos, y_pos, (STD_UI_WIDTH/136), STD_UI_HEIGHT - (y_pos*2), 255, 255, 0, 200, UI_ANCHOR_TOPRIGHT);
}

void Menu_DrawSubMenu (char *line_one, char *line_two)
{
	int y_pos = (vid.height/3);

	// Dark red background
    Menu_DrawFill (0, 0, vid.width, vid.height, 255, 0, 0, 20, UI_ANCHOR_TOPLEFT);
    // Black background box
    Menu_DrawFill (0, vid.height/3, vid.width, vid.height/3, 0, 0, 0, 255, UI_ANCHOR_TOPLEFT);
    // Top yellow line
    Menu_DrawFill (0, (vid.height/3)-small_bar_height, vid.width, small_bar_height, 255, 255, 0, 255, UI_ANCHOR_TOPLEFT);
    // Bottom yellow line
    Menu_DrawFill (0, vid.height-(vid.height/3), vid.width, small_bar_height, 255, 255, 0, 255, UI_ANCHOR_TOPLEFT);

	// Quit/Restart Text
    Menu_DrawStringCentered (vid.width/2, y_pos+CHAR_HEIGHT, line_one, 255, 255, 255, 255);
    // Lose progress text
    Menu_DrawStringCentered (vid.width/2, y_pos+(CHAR_HEIGHT*2), line_two, 255, 255, 255, 255);
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
	Menu_DrawFill(0, 0, STD_UI_WIDTH, big_bar_height, 0, 0, 0, 228, UI_ANCHOR_TOPLEFT);
	Menu_DrawFill(0, big_bar_height, STD_UI_WIDTH, small_bar_height, 130, 130, 130, 255, UI_ANCHOR_TOPLEFT);

	// Bottom Bars
	Menu_DrawFill(0, (STD_UI_HEIGHT - big_bar_height), STD_UI_WIDTH, big_bar_height, 0, 0, 0, 228, UI_ANCHOR_TOPLEFT);
	Menu_DrawFill(0, (STD_UI_HEIGHT - big_bar_height) - small_bar_height, STD_UI_WIDTH, small_bar_height, 130, 130, 130, 255, UI_ANCHOR_TOPLEFT);
}

/*
======================
Menu_DrawButton
======================
*/
void Menu_DrawButton (int order, int button_index, char* button_name, char* button_summary, void *on_activate)
{
	int y_factor = (STD_UI_HEIGHT/16);
	int x_pos = (STD_UI_WIDTH/3);
	int y_pos = (STD_UI_HEIGHT/8) + (order*y_factor);

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
		Menu_DrawSelectionBox (x_pos, y_pos, UI_ANCHOR_TOPLEFT);

		// Draw button string
		Menu_DrawString (x_pos, y_pos, button_name, 255, 0, 0, 255, menu_text_scale_factor, UI_ANCHOR_TOPLEFT, UI_FLIPTEXTPOS);

		// Draw the bottom screen text for selected button 
		Draw_ColoredStringCentered (vid.height - (vid.height/12), button_summary, 255, 255, 255, 255, menu_text_scale_factor);
	} else {
		// Not hovering over button
		Menu_DrawString (x_pos, y_pos, button_name, 255, 255, 255, 255, menu_text_scale_factor, UI_ANCHOR_TOPLEFT, UI_FLIPTEXTPOS);
	}
}

/*
======================
Menu_DrawGreyButton
======================
*/
void Menu_DrawGreyButton (int order, char* button_name)
{
	int y_factor = (STD_UI_HEIGHT/16);
	int x_pos = (STD_UI_WIDTH/3); 
	int y_pos = (STD_UI_HEIGHT/8) + (order*y_factor);

	Menu_DrawString (x_pos, y_pos, button_name, 128, 128, 128, 255, menu_text_scale_factor, UI_ANCHOR_TOPLEFT, UI_FLIPTEXTPOS);
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
void Menu_DrawMapButton (int order, int button_index, int usermap_index, int map_category, char* bsp_name, void *on_activate)
{
	int i;
	int index = 0;
	char *button_name;
	char *final_name;
	int desc_y_factor = (STD_UI_HEIGHT/24);

	int image_width = (STD_UI_WIDTH/3);
	int image_height = (STD_UI_HEIGHT/3);

	int x_pos = ((STD_UI_WIDTH/3)+(STD_UI_WIDTH/36)) + (image_width/2);
	int y_pos = (STD_UI_HEIGHT/5)-(STD_UI_HEIGHT/24);

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
		Menu_DrawPic (x_pos, y_pos, menu_usermap_image[index], image_width, image_height, UI_ANCHOR_TOPRIGHT);

		// Draw border around map image
		Menu_DrawMapBorder (x_pos, y_pos, image_width, image_height, UI_ANCHOR_TOPRIGHT);

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

		Menu_DrawString(x_pos + ((STD_UI_WIDTH/144)*1.5) + menu_badges_width, y_pos + image_height - (menu_badges_height/2) - (CHAR_HEIGHT/2), badge_name, 255, 255, 0, 255, menu_text_scale_factor, UI_ANCHOR_TOPRIGHT, 0);
		Menu_DrawSubPic(x_pos + (STD_UI_WIDTH/144), y_pos + image_height - menu_badges_height, menu_badges, s, t, s_coord_size, t_coord_size, scale, 255, 255, 255, 255);

		// Draw map description
		for (int j = 0; j < 8; j++) {
			Menu_DrawStringCentered (x_pos + (STD_UI_WIDTH/6), (y_pos + image_height + (small_bar_height*2)) + j*desc_y_factor, custom_maps[index].map_desc[j], 255, 255, 255, 255);
		}

		// Draw map author
		if (custom_maps[index].map_author != NULL) {
			Draw_ColoredStringCentered (STD_UI_HEIGHT - (STD_UI_HEIGHT/24), custom_maps[index].map_author, 255, 255, 0, 255, menu_text_scale_factor);
		}
	}
}

void Menu_DrawOptionButton(int order, char* selection_name)
{
	int y_factor = (STD_UI_WIDTH/16);
	int x_pos = (STD_UI_WIDTH/3) + (STD_UI_WIDTH/16); 
	int y_pos = (STD_UI_HEIGHT/8) + (order*y_factor);

	Draw_ColoredString(x_pos, y_pos, selection_name, 255, 255, 255, 255, menu_text_scale_factor);
}

void Menu_DrawOptionSlider(int order, int button_index, int min_option_value, int max_option_value, cvar_t option, char* _option_string, qboolean zero_to_one, qboolean draw_option_string, float increment_amount)
{
	int y_factor = (STD_UI_HEIGHT/16);
	int x_pos = (STD_UI_WIDTH/3) + (STD_UI_WIDTH/16); 
	int y_pos = (STD_UI_HEIGHT/8) + (order*y_factor);
	int slider_box_width = (STD_UI_WIDTH/4);
	int slider_box_height = (STD_UI_HEIGHT/48);

	int slider_width = (STD_UI_WIDTH/128);

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
	Menu_DrawFill(x_pos, y_pos, slider_box_width, slider_box_height, 255, 160, 160, 255, UI_ANCHOR_TOPLEFT);
	// Selection box
	Menu_DrawFill((x_pos+(option_pos*slider_box_width)) - slider_width, y_pos-(STD_UI_HEIGHT/64), slider_width, slider_box_height+((STD_UI_HEIGHT/64)*2), 255, 255, 255, 255, UI_ANCHOR_TOPLEFT);
	// Cvar value string
	if (draw_option_string) {
		Draw_ColoredString(x_pos + slider_box_width + (STD_UI_WIDTH/96), y_pos, option_string, 255, 255, 255, 255, menu_text_scale_factor);
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

	int image_width = (STD_UI_WIDTH/3.5);
	int image_height = (STD_UI_HEIGHT/3.5);

	int image_x_pos = (STD_UI_WIDTH/2) + (STD_UI_WIDTH/28);
	int image_y_pos = (STD_UI_HEIGHT) - (big_bar_height + small_bar_height) - (image_height) - (STD_UI_HEIGHT/96);

	int left_column_x = (STD_UI_WIDTH/3) + (STD_UI_WIDTH/5);
	int left_column_y = (STD_UI_HEIGHT/6);

	int y_offset = CHAR_HEIGHT;
	int y_newline = (STD_UI_HEIGHT/10);
	int x_newline = (STD_UI_WIDTH/4);

	int line_x = (STD_UI_WIDTH/3) + (STD_UI_WIDTH/8.5);

	texnum = Menu_GetMapImage (bsp_name);

	for (i = 0; i < num_user_maps; i++) {
		if (!strcmp(bsp_name, custom_maps[i].map_name)) 
			break;
	}

	// Draw map thumbnail picture
	Draw_StretchPic (image_x_pos, image_y_pos, texnum, image_width, image_height);

	// Draw border around map image
	Menu_DrawMapBorder (image_x_pos, image_y_pos, image_width, image_height, UI_ANCHOR_TOPRIGHT);

	// Game Mode
	Menu_DrawStringCentered (left_column_x, left_column_y, "Game Mode", 255, 255, 255, 255);
	Menu_DrawStringCentered (left_column_x, left_column_y + y_offset, info_gamemode, 255, 255, 0, 255);
	Menu_DrawFill (line_x, left_column_y + (y_offset*2), (STD_UI_WIDTH/6), (small_bar_height/3), 130, 130, 130, 255, UI_ANCHOR_TOPLEFT);

	// Difficulty
	Menu_DrawStringCentered (left_column_x + x_newline, left_column_y, "Difficulty", 255, 255, 255, 255);
	Menu_DrawStringCentered (left_column_x + x_newline, left_column_y + y_offset, info_difficulty, 255, 255, 0, 255);
	Menu_DrawFill (line_x + x_newline, left_column_y + (y_offset*2), (STD_UI_WIDTH/6), (small_bar_height/3), 130, 130, 130, 255, UI_ANCHOR_TOPLEFT);

	// Start Round
	Menu_DrawStringCentered (left_column_x, left_column_y + y_newline, "Start Round", 255, 255, 255, 255);
	Menu_DrawStringCentered (left_column_x, left_column_y + y_offset + y_newline, info_startround, 255, 255, 0, 255);
	Menu_DrawFill (line_x , left_column_y + (y_offset*2) + y_newline, (STD_UI_WIDTH/6), (small_bar_height/3), 130, 130, 130, 255, UI_ANCHOR_TOPLEFT);

	// Magic
	Menu_DrawStringCentered (left_column_x + x_newline, left_column_y + y_newline, "Magic", 255, 255, 255, 255);
	Menu_DrawStringCentered (left_column_x + x_newline, left_column_y + y_offset + y_newline, info_magic, 255, 255, 0, 255);
	Menu_DrawFill (line_x + x_newline, left_column_y + (y_offset*2) + y_newline, (STD_UI_WIDTH/6), (small_bar_height/3), 130, 130, 130, 255, UI_ANCHOR_TOPLEFT);

	// Headshots Only
	Menu_DrawStringCentered (left_column_x, left_column_y + (y_newline*2), "Headshots Only", 255, 255, 255, 255);
	Menu_DrawStringCentered (left_column_x, left_column_y + y_offset + (y_newline*2), info_headshotonly, 255, 255, 0, 255);
	Menu_DrawFill (line_x, left_column_y + (y_offset*2) + (y_newline*2), (STD_UI_WIDTH/6), (small_bar_height/3), 130, 130, 130, 255, UI_ANCHOR_TOPLEFT);

	// Horde Size
	Menu_DrawStringCentered (left_column_x + x_newline, left_column_y + (y_newline*2), "Horde Size", 255, 255, 255, 255);
	Menu_DrawStringCentered (left_column_x + x_newline, left_column_y + y_offset + (y_newline*2), info_hordesize, 255, 255, 0, 255);
	Menu_DrawFill (line_x + x_newline, left_column_y + (y_offset*2) + (y_newline*2), (STD_UI_WIDTH/6), (small_bar_height/3), 130, 130, 130, 255, UI_ANCHOR_TOPLEFT);

	// Fast Rounds
	Menu_DrawStringCentered (left_column_x, left_column_y + (y_newline*3), "Fast Rounds", 255, 255, 255, 255);
	Menu_DrawStringCentered (left_column_x, left_column_y + y_offset + (y_newline*3), info_fastrounds, 255, 255, 0, 255);
	Menu_DrawFill (line_x, left_column_y + (y_offset*2) + (y_newline*3), (STD_UI_WIDTH/6), (small_bar_height/3), 130, 130, 130, 255, UI_ANCHOR_TOPLEFT);

	// Map Name (Pretty)
	Menu_DrawStringCentered (image_x_pos + (image_width/2), image_y_pos + image_height - (small_bar_height) - (STD_UI_HEIGHT/32), custom_maps[i].map_name_pretty, 255, 255, 0, 255);
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
	Draw_ColoredString (image_x_pos, image_y_pos + image_height + small_bar_height, custom_maps[i].map_name_pretty, 255, 255, 0, 255, menu_scale_factor);
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
	Menu_DrawString(STD_UI_WIDTH, y_pos, game_build_date, 255, 255, 255, 255, menu_text_scale_factor, UI_ANCHOR_TOPLEFT, UI_FLIPTEXTPOS);
	// Welcome text
	Menu_DrawString(STD_UI_WIDTH, y_pos + y_offset, welcome_text, 255, 255, 0, 255, menu_text_scale_factor, UI_ANCHOR_TOPLEFT, UI_FLIPTEXTPOS);
}