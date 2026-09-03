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
/* CONTROLS MENU */

char			*aimassist_string;
char			*invert_string;
char 			*anub_string;

cvar_t			global_sensitivity;

extern cvar_t 	in_aimassist;
extern cvar_t	sensitivity;
extern cvar_t	in_acceleration;
extern cvar_t	in_tolerance;
extern cvar_t	in_anub_mode;
extern cvar_t	m_pitch;
#ifdef PLATFORM_USES_GENERIC_GLYPHS
extern cvar_t    cl_controllerglyphs;
static char     *controller_glyphs_string;
#endif

/*
===============
Menu_Controls_Set
===============
*/
void Menu_Controls_Set (void)
{
	Menu_ResetMenuButtons();

    m_previous_state = m_configuration;
	m_state = m_controls;
}

void Menu_Controls_SetStrings (void)
{
	global_sensitivity = sensitivity;

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

#ifdef PLATFORM_HAS_ONE_ANALOG_STICK
	if((int)in_anub_mode.value == 1) {
		anub_string = "MOVE";
	} else {
		anub_string = "LOOK";
	}
#endif
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
#ifdef PLATFORM_HAS_ONE_ANALOG_STICK
	float current_anubmode = in_anub_mode.value;

    current_anubmode += 1;
    if (current_anubmode > 1) {
        current_anubmode = 0;
    }

    Cvar_SetValue ("in_anub_mode", current_anubmode);
#endif
}

void Menu_Controls_ApplySettings (void)
{
    // no op
    Menu_SetSound(MENU_SND_ENTER);
}

#ifdef PLATFORM_USES_GENERIC_GLYPHS
static void Menu_Controls_ApplyControllerGlyphs(void)
{
	const char *next = "xbox";
	if (!strcmp(cl_controllerglyphs.string, "xbox")) next = "sony";
	else if (!strcmp(cl_controllerglyphs.string, "sony")) next = "nintendo";
	else if (!strcmp(cl_controllerglyphs.string, "nintendo")) next = "generic";
	Cvar_Set("cl_controllerglyphs", (char *)next);
}

static void Menu_Controls_SetControllerGlyphString(void)
{
	if (!strcmp(cl_controllerglyphs.string, "xbox")) controller_glyphs_string = "MICROSOFT";
	else if (!strcmp(cl_controllerglyphs.string, "sony")) controller_glyphs_string = "SONY";
	else if (!strcmp(cl_controllerglyphs.string, "nintendo")) controller_glyphs_string = "NINTENDO";
	else if (!strcmp(cl_controllerglyphs.string, "generic")) controller_glyphs_string = "GENERIC";
	else controller_glyphs_string = cl_controllerglyphs.string;
}
#endif

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

#ifdef PLATFORM_USES_GENERIC_GLYPHS
	Menu_Controls_SetControllerGlyphString();
	Menu_DrawButton(controls_buttons++, controls_index++, "CONTROLLER GLYPHS", "Change the controller glyph tilemap.", Menu_Controls_ApplyControllerGlyphs);
	Menu_DrawOptionButton(controls_buttons-1, controller_glyphs_string);
	Menu_DrawControllerGlyphPreview(controls_buttons-1);
#endif

#ifdef PLATFORM_HAS_ONE_ANALOG_STICK
	// Anub tolerance
	Menu_DrawButton (controls_buttons++, controls_index++, "A-NUB TOLERANCE", "Change A-Nub Tolerance.", NULL);
	Menu_DrawOptionSlider (controls_buttons-1, controls_index-1, 0, 1, in_tolerance, "tolerance", false, true, 0.25f);

	// Anub-mode (look/move)
	Menu_DrawButton (controls_buttons++, controls_index++, "A-NUB MODE", "Toggle between Look and Move A-Nub Options.", Menu_Controls_ApplyAnubMode);
	Menu_DrawOptionButton (controls_buttons-1, anub_string);
#endif

	// Bindings
	Menu_DrawButton (controls_buttons++, controls_index++, "BINDINGS", "Change Input Bindings.", Menu_Bindings_Set);

	Menu_DrawDivider(-2.5);
	Menu_DrawButton(-2, controls_index++, "APPLY", "Save & Apply Settings.", Menu_Controls_ApplySettings);
	Menu_DrawButton (-1, controls_index, "BACK", "Return to Main Menu.", Menu_Configuration_Set);
}