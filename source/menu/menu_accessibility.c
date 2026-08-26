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

//=============================================================================
/* ACCESSIBILITY MENU */

char 			*hitmarkers_string;
char 			*colorblind_string;
char 			*screenflash_string;
char 			*monthspoof_string;
char 			*crosshairdot_string;

extern cvar_t 	cl_hitmarkers;
extern cvar_t 	cl_colorblind;
extern cvar_t 	scr_whiteflash;
extern cvar_t 	sv_spoofmonth;
extern cvar_t 	cl_textopacity;
extern cvar_t 	cl_crosshairdot;

void Menu_Accessibility_ApplyHitmarkers (void)
{
	float current_hitmarkers = cl_hitmarkers.value;

    current_hitmarkers += 1;
    if (current_hitmarkers > 1) {
        current_hitmarkers = 0;
    }

    Cvar_SetValue ("cl_hitmarkers", current_hitmarkers);
}

void Menu_Accessibility_ApplyColorblind (void)
{
	float current_colorblind = cl_colorblind.value;

    current_colorblind += 1;
    if (current_colorblind > 1) {
        current_colorblind = 0;
    }

    Cvar_SetValue ("cl_colorblind", current_colorblind);
}

void Menu_Accessibility_ApplyScreenflash (void)
{
	float current_screenflash = scr_whiteflash.value;

    current_screenflash += 1;
    if (current_screenflash > 1) {
        current_screenflash = 0;
    }

    Cvar_SetValue ("scr_whiteflash", current_screenflash);
}

void Menu_Accessibility_ApplyMonthSpoof (void)
{
	float current_monthspoof = sv_spoofmonth.value;

    current_monthspoof += 1;
    if (current_monthspoof > 12) {
        current_monthspoof = 0;
    }

    Cvar_SetValue ("sv_spoofmonth", current_monthspoof);
}

void Menu_Accessibility_ApplyCrosshairDot (void)
{
	Cvar_SetValue ("cl_crosshairdot", !cl_crosshairdot.value);
}

void Menu_Accessibility_ApplySettings (void)
{
	// no op
	Menu_SetSound(MENU_SND_ENTER);
}

/*
===============
Menu_Accessibility_Set
===============
*/
void Menu_Accessibility_Set (void)
{
	Menu_ResetMenuButtons();

	m_previous_state = m_state;
	m_state = m_accessibility;
}

void Menu_Accessibility_SetStrings (void)
{
	if((int)cl_hitmarkers.value == 1) {
		hitmarkers_string = "ENABLED";
	} else {
		hitmarkers_string = "DISABLED";
	}

	if((int)cl_colorblind.value == 1) {
		colorblind_string = "ENABLED";
	} else {
		colorblind_string = "DISABLED";
	}

	if ((int)scr_whiteflash.value == 1) {
		screenflash_string = "FORBID WHITE";
	} else {
		screenflash_string = "ALLOW WHITE";
	}

	if ((int)cl_crosshairdot.value == 1) {
		crosshairdot_string = "ENABLED";
	} else {
		crosshairdot_string = "DISABLED";
	}

	switch((int)sv_spoofmonth.value) {
		case 0:
			monthspoof_string = "REAL TIME";
			break;
		case 1:
			monthspoof_string = "JANUARY";
			break;
		case 2:
			monthspoof_string = "FEBRUARY";
			break;
		case 3:
			monthspoof_string = "MARCH";
			break;
		case 4:
			monthspoof_string = "APRIL";
			break;
		case 5:
			monthspoof_string = "MAY";
			break;
		case 6:
			monthspoof_string = "JUNE";
			break;
		case 7:
			monthspoof_string = "JULY";
			break;
		case 8:
			monthspoof_string = "AUGUST";
			break;
		case 9:
			monthspoof_string = "SEPTEMBER";
			break;
		case 10:
			monthspoof_string = "OCTOBER";
			break;
		case 11:
			monthspoof_string = "NOVEMBER";
			break;
		case 12:
			monthspoof_string = "DECEMBER";
			break;
	}
}

/*
===============
Menu_Accessibility_Draw
===============
*/
void Menu_Accessibility_Draw (void)
{
	// Background
	Menu_DrawCustomBackground (true);
	// Header
	Menu_DrawTitle ("ACCESSIBILITY OPTIONS", MENU_COLOR_WHITE);
	// Map panel makes the background darker
    Menu_DrawMapPanel();
    // Set value strings
    Menu_Accessibility_SetStrings();

	// Hitmarkers
	Menu_DrawButton(1, 0, "HITMARKERS", "HUD Hitmarkers for visual feedback.", Menu_Accessibility_ApplyHitmarkers);
	Menu_DrawOptionButton(1, hitmarkers_string);

	// Text Backdrop
	Menu_DrawButton(2, 1, "TEXT BACKDROP", "Opacity of backdrop for text elements on HUD.", NULL);
	Menu_DrawOptionSlider(2, 1, 0, 1, cl_textopacity, "cl_textopacity", false, false, 0.05f);

	// Accessible Colors
	Menu_DrawButton(3, 2, "ACCESSIBLE COLORS", "Uses enhanced Player colors to improve visibilty.", Menu_Accessibility_ApplyColorblind);
	Menu_DrawOptionButton(3, colorblind_string);

	// Screen Flashes
	Menu_DrawButton(4, 3, "SCREEN FLASHES", "Choose the color of screen flashes.", Menu_Accessibility_ApplyScreenflash);
	Menu_DrawOptionButton(4, screenflash_string);

	// Month Spoof
	Menu_DrawButton(5, 4, "MONTH SPOOF", "Lie to the game about the current Month, if you are host.", Menu_Accessibility_ApplyMonthSpoof);
	Menu_DrawOptionButton(5, monthspoof_string);

	// Crosshair Dot
	Menu_DrawButton(6, 5, "CROSSHAIR DOT", "Add a dot to the middle of the crosshair.", Menu_Accessibility_ApplyCrosshairDot);
	Menu_DrawOptionButton(6, crosshairdot_string);

	Menu_DrawDivider(-2.5);
	Menu_DrawButton(-2, 6, "APPLY", "Save & Apply Settings.", Menu_Accessibility_ApplySettings);
	Menu_DrawButton(-1, 7, "BACK", "Return to Configuration Menu.", Menu_Configuration_Set);
}
