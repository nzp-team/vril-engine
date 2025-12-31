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
#include "menu_defs.h"

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
        case MENU_SND_ENTER:
            S_LocalSound ("sounds/menu/enter.wav");
        case MENU_SND_BEEP:
            S_LocalSound ("sounds/misc/talk2.wav");
        case default:
            Con_Printf("Unsupported menu sound\n");
    }
}

/*
======================
Menu_DrawCustomBackground
======================
*/
void Menu_DrawCustomBackground ()
{
    float elapsed_background_time = time - menu_starttime;

	int big_bar_height = vid.height/(vid.height>>4);
	int small_bar_height = vid.height/(vid.height>>8);

	// 
	// Slight background pan
	//
	float x_pos = 0 + (((vid.width * 1.05) - vid.width) * (elapsed_background_time/7));

	Draw_StretchPic(x_pos, 0, menu_background, vid.width, vid.height);
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 178);

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

	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 178);

	// Top Bars
	Draw_FillByColor(0, 0, vid.width, big_bar_height, 0, 0, 0, 228);
	Draw_FillByColor(0, (vid.height - big_bar_height) - small_bar_height, vid.width, small_bar_height, 52, 52, 52, 255);

	// Bottom Bars
	Draw_FillByColor(0, vid.height-(big_bar_height), vid.width, big_bar_height, 0, 0, 0, 228);
	Draw_FillByColor(0, (vid.height - big_bar_height) + small_bar_height, vid.width, small_bar_height, 52, 52, 52, 255);
}

/*
======================
Menu_DrawTitle
======================
*/
void Menu_DrawTitle (char *title_name)
{
	int x_pos = (vid.width/(vid.width/vid.width>>6))/menu_scale_factor;
	int y_pos = (vid.height/(vid.height>>6))/menu_scale_factor;

	Draw_ColoredString (x_pos, y_pos, 255, 255, 255, menu_scale_factor*2);
}

/*
======================
Menu_DrawButton
======================
*/
void Menu_DrawButton (int order, char* button_name, int button_active, int button_selected, char* button_summary)
{
	
}

/*
======================
Menu_DrawBuildDate
======================
*/
void Menu_DrawBuildDate ()
{
	int y_pos = (vid.height/(vid.height>>8))/menu_scale_factor;

	Draw_ColoredString((vid.width - getTextWidth(game_build_date, menu_scale_factor)), y_pos, game_build_date, 255, 255, 255, 255, 1);
}

/*
======================
Menu_DrawDivider
======================
*/
void Menu_DrawDivider (int order)
{
	int y_pos;

	Draw_FillByColor(0, 113, 160, 2, 130, 130, 130, 255);
}