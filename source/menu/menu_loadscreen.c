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

//=============================================================================
/* LOADING SCREEN */

int			loadingScreen;
image_t     lscreen_image;
double      loadingtimechange;
int         loadingdot;
char        *loadinglinetext;

qboolean 	loadscreeninit;

char* 		map_loadname;
char* 		map_loadname_pretty;

//=============================================================================

/*
==============
Menu_DrawLoadScreen
==============
*/
void Menu_DrawLoadScreen (void)
{
	if (developer.value) {
		return;
	}
	if (!con_forcedup) {
	    return;
	}

	if (loadingScreen) {
		Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 255);
		if (!loadscreeninit) {
			loadinglinetext = malloc(256*sizeof(char));

			loadscreeninit = true;
		}

		lscreen_image = Image_LoadImage(va("gfx/lscreen/%s", map_loadname), IMAGE_TGA | IMAGE_PNG | IMAGE_JPG, 0, false, false);

		if (lscreen_image < 0 || lscreen_image < 0) {
			lscreen_image = Image_LoadImage("gfx/lscreen/lscreen", IMAGE_PNG, 0, true, false);
		}

		Draw_StretchPic(0, 0, lscreen_image, vid.width, vid.height);

		Draw_FillByColor(0, 0, vid.width, big_bar_height, 0, 0, 0, 175);
		Draw_FillByColor(0, vid.height - big_bar_height/2, vid.width, big_bar_height/2, 0, 0, 0, 175);

		if (map_loadname_pretty != NULL) {
			Menu_DrawTitle(map_loadname_pretty, MENU_COLOR_YELLOW);
		}

		if (loadingtimechange < Sys_FloatTime ())
		{
			loadinglinetext = Platform_ReturnLoadingText();
			loadingtimechange = Sys_FloatTime () + 5;
		}

		if (key_dest == key_game) {
			Draw_ColoredStringCentered((vid.height - (vid.height/24)), loadinglinetext, 255, 255, 255, 255, menu_scale_factor);
		}
	}
}