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

float   menu_scale_factor;
float	CHAR_WIDTH;
float	CHAR_HEIGHT;

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
}

/*
======================
Menu_LoadPics
======================
*/
void Menu_LoadPics ()
{
	menu_bk 	= Image_LoadImage("gfx/menu/menu_background", IMAGE_TGA, 0, true, false);
	menu_ndu 	= Image_LoadImage("gfx/menu/nacht_der_untoten", IMAGE_PNG, 0, false, false);
	menu_wh 	= Image_LoadImage("gfx/menu/nzp_warehouse", IMAGE_PNG, 0, false, false);
	menu_wh2 	= Image_LoadImage("gfx/menu/nzp_warehouse2", IMAGE_PNG, 0, false, false);
	menu_ch 	= Image_LoadImage("gfx/menu/christmas_special", IMAGE_PNG, 0, false, false);
	menu_custom = Image_LoadImage("gfx/menu/custom", IMAGE_PNG, 0, false, false);

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
	int i = (rand() % num_custom_images);

	if (menu_usermap_image[i] <= 0)	return menu_bk;

    return (image_t)menu_usermap_image[i];
}


/*
======================
Menu_DrawCustomBackground
======================
*/
void Menu_DrawCustomBackground ()
{
    float elapsed_background_time = menu_time - menu_starttime;

	int big_bar_height = vid.height/10;
	int small_bar_height = vid.height/64;

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
	float alpha = 0;

	if (elapsed_background_time < 1.0) {
        alpha = 1.0f - sin((elapsed_background_time / 1.0f) * (M_PI / 2));  // Opacity from 1 -> 0
    } else if (elapsed_background_time < 6.0f) {
        alpha = 0.0f;
    } else if (elapsed_background_time < 7.0f) {
        alpha = sin(((elapsed_background_time - 6.0f) / 1.0f) * (M_PI / 2));  // Opacity from 0 -> 1
    }

	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, alpha);

	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 84);

	// Top Bars
	Draw_FillByColor(0, 0, vid.width, big_bar_height, 0, 0, 0, 228);
	Draw_FillByColor(0, big_bar_height + small_bar_height, vid.width, small_bar_height, 130, 130, 130, 255);

	// Bottom Bars
	Draw_FillByColor(0, (vid.height - big_bar_height), vid.width, big_bar_height, 0, 0, 0, 228);
	Draw_FillByColor(0, (vid.height - big_bar_height) - small_bar_height, vid.width, small_bar_height, 130, 130, 130, 255);
}

/*
======================
Menu_DrawTitle
======================
*/
void Menu_DrawTitle (char *title_name)
{
	int x_pos = vid.width/64;
	int y_pos = vid.height/24;

	Draw_ColoredString (x_pos, y_pos, title_name, 255, 255, 255, 255, menu_scale_factor*2);
}

/*
======================
Menu_DrawButton
======================
*/
void Menu_DrawButton (int order, char* button_name, int button_active, int button_selected, char* button_summary)
{
	// y factor for vertical menu ordering
	int y_factor = (vid.height/16);

	int x_pos = (vid.width/3) - getTextWidth(button_name, menu_scale_factor); 
	int y_pos = (vid.height/8) + (order*y_factor);

	int border_width = (vid.height/144);
	int x_length = (vid.width/3) + border_width;
	int border_height_offset = (vid.height/96);

	// Inactive (grey) menu button
	// Non-selectable
	if (button_active == MENU_BUTTON_INACTIVE) {
		Draw_ColoredString (x_pos, y_pos, button_name, 128, 128, 128, 255, menu_scale_factor);
	} else if (button_active == MENU_BUTTON_ACTIVE) {
		// Active menu buttons
		// Selected
		if (button_selected == BUTTON_SELECTED) {
			// Make button red and add a box around it

			// Draw selection box
			// Done in 3 parts
			// Top
			Draw_FillByColor (0, y_pos - border_height_offset, x_length, border_width, 255, 0, 0, 255);
			// Bottom
			Draw_FillByColor (0, (y_pos + (border_height_offset*2)) + (CHAR_HEIGHT/2), x_length, border_width, 255, 0, 0, 255);
			// Side
			Draw_FillByColor (x_length, (y_pos - (CHAR_HEIGHT/2)) + (border_height_offset), border_width, (border_height_offset) + CHAR_HEIGHT + border_width, 255, 0, 0, 255);

			// Draw button string
			Draw_ColoredString (x_pos, y_pos, button_name, 255, 0, 0, 255, menu_scale_factor);

			// Draw the bottom screen text for selected button 
			Draw_ColoredStringCentered (vid.height - (vid.height/14), button_summary, 255, 255, 255, 255, menu_scale_factor);
		} else if (button_selected == BUTTON_DESELECTED) {
			// Unselected button
			Draw_ColoredString (x_pos, y_pos, button_name, 255, 255, 255, 255, menu_scale_factor);
		}
	}
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