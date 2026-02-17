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
/* CONTROLS MENU */

char* aimassist_string;
char* invert_string;
char *anub_string;

cvar_t			global_sensitivity;

extern cvar_t 	in_aimassist;
#ifdef __PSP__
extern cvar_t	in_sensitivity;
extern cvar_t	in_tolerance;
#else
extern cvar_t	sensitivity;
#endif // __PSP__
extern cvar_t	in_acceleration;
extern cvar_t	in_anub_mode;
extern cvar_t	m_pitch;

void Menu_Controls_AllocStrings (void)
{
	aimassist_string = malloc(16*sizeof(char));
	invert_string = malloc(16*sizeof(char));
#ifdef __PSP__
	anub_string = malloc(32*sizeof(char));
#endif
}

void Menu_Controls_FreeStrings (void)
{
	free(aimassist_string);
	free(invert_string);
#ifdef __PSP__
	free(anub_string);
#endif
}

/*
===============
Menu_Controls_Set
===============
*/
void Menu_Controls_Set (void)
{
	Menu_ResetMenuButtons();
	Menu_Controls_AllocStrings();
	m_state = m_controls;
}

void Menu_Controls_SetStrings (void)
{
#ifdef __PSP__
	global_sensitivity = in_sensitivity;

	if((int)in_anub_mode.value == 1) {
		anub_string = "MOVE";
	} else {
		anub_string = "LOOK";
	}
#else
	global_sensitivity = sensitivity;
#endif

	if((int)in_aimassist.value == 1) {
		aimassist_string = "ENABLED";
	} else {
		aimassist_string = "DISABLED";
	}

	if((int)m_pitch.value > 0) {
		invert_string = "ENABLED";
	} else {
		invert_string = "DISABLED";
	}
}

void Menu_Controls_ApplyAimAssist (void)
{
    float current_aimassist = in_aimassist.value;

    current_aimassist += 1;
    if (current_aimassist > 1) {
        current_aimassist = 0;
    }

    Cvar_SetValue ("in_aimassist", current_aimassist);
}

void Menu_Controls_ApplyLookInversion (void)
{
    float current_lookinversion = m_pitch.value;

    current_lookinversion += 1;
    if (current_lookinversion > 1) {
        current_lookinversion = 0;
    }

    Cvar_SetValue ("m_pitch", current_lookinversion);
}

void Menu_Controls_ApplyAnubMode (void)
{
	float current_anubmode = in_anub_mode.value;

    current_anubmode += 1;
    if (current_anubmode > 1) {
        current_anubmode = 0;
    }

    Cvar_SetValue ("in_anub_mode", current_anubmode);
}

/*
===============
Menu_Controls_Draw
===============
*/
void Menu_Controls_Draw (void)
{
	int controls_index = 0;
	int controls_buttons = 1;

	// Background
	Menu_DrawCustomBackground (true);

	// Header
	Menu_DrawTitle ("CONTROL OPTIONS", MENU_COLOR_WHITE);

	Menu_Controls_SetStrings();

	// Map panel makes the background darker
    Menu_DrawMapPanel();

	// Aim Assist
	Menu_DrawButton (controls_buttons++, controls_index++, "AIM ASSIST", "Toggle Assisted-Aim to Improve Targetting.", Menu_Controls_ApplyAimAssist);
	Menu_DrawOptionButton (controls_buttons-1, aimassist_string);

	// Look Sensitivity
	Menu_DrawButton (controls_buttons++, controls_index++, "LOOK SENSITIVITY", "Alter look Sensitivity.", NULL);
	Menu_DrawOptionSlider (controls_buttons-1, controls_index-1, 0, 10, global_sensitivity, "sensitivity", false, true, 1);

	// Look Acceleration
	Menu_DrawButton (controls_buttons++, controls_index++, "LOOK ACCELERATION", "Alter look Acceleration.", NULL);
	Menu_DrawOptionSlider (controls_buttons-1, controls_index-1, 0, 1, in_acceleration, "acceleration", false, true, 0.1f);

	// Look Inversion
	Menu_DrawButton (controls_buttons++, controls_index++, "INVERT LOOK", "Invert Y-Axis Camera Input.", Menu_Controls_ApplyLookInversion);
	Menu_DrawOptionButton (controls_buttons-1, invert_string);

#ifdef __PSP__
	// Anub tolerance
	Menu_DrawButton (controls_buttons++, controls_index++, "A-NUB TOLERANCE", "Change A-Nub Tolerance.", NULL);
	Menu_DrawOptionSlider (controls_buttons-1, controls_index-1, 0, 1, in_tolerance, "tolerance", false, true, 0.25f);

	// Anub-mode (look/move)
	Menu_DrawButton (controls_buttons++, controls_index++, "A-NUB MODE", "Toggle between Look and Move A-Nub Options.", Menu_Controls_ApplyAnubMode);
	Menu_DrawOptionButton (controls_buttons-1, anub_string);
#endif

	// Bindings
	Menu_DrawButton (controls_buttons++, controls_index++, "BINDINGS", "Change Input Bindings.", Menu_Bindings_Set);

	Menu_DrawButton (-1, controls_index++, "BACK", "Return to Main Menu.", Menu_Configuration_Set);
}