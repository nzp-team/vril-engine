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

qboolean		has_anub = false;

extern cvar_t 	in_aimassist;
#ifdef __PSP__
extern cvar_t	in_sensitivity;
#else
extern cvar_t	sensitivity;
#endif // __PSP__
extern cvar_t	in_acceleration;
extern cvar_t	in_tolerance;
extern cvar_t	in_anub_mode;
extern cvar_t	m_pitch;

#ifdef __3DS__
// Gyroscope aiming feature added by Yassine Milal.
extern cvar_t	gyro_enable;
extern cvar_t	gyro_sensitivity;
extern cvar_t	gyro_pitch_sensitivity;
extern cvar_t	gyro_yaw_sensitivity;
extern cvar_t	gyro_invert_pitch;
extern cvar_t	gyro_invert_yaw;
char			*gyro_string;
char			*gyro_invert_pitch_string;
char			*gyro_invert_yaw_string;
qboolean		controls_gyro_submenu = false;
#endif

/*
===============
Menu_Controls_Set
===============
*/
void Menu_Controls_Set (void)
{
	Menu_ResetMenuButtons();
	has_anub = false;

#ifdef __PSP__
	has_anub = true;
#elif __3DS__
	controls_gyro_submenu = false;
	if (circlepadpro_flag || new3ds_flag) {
		has_anub = true;
	}
#endif

    m_previous_state = m_configuration;
	m_state = m_controls;
}

void Menu_Controls_SetStrings (void)
{
#ifdef __PSP__
	global_sensitivity = in_sensitivity;
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

	if (has_anub) {
		if((int)in_anub_mode.value == 1) {
			anub_string = "MOVE";
		} else {
			anub_string = "LOOK";
		}
	}

#ifdef __3DS__
	if ((int)gyro_enable.value == 1) {
		gyro_string = "ENABLED";
	} else {
		gyro_string = "DISABLED";
	}

	if ((int)gyro_invert_pitch.value == 1) {
		gyro_invert_pitch_string = "ENABLED";
	} else {
		gyro_invert_pitch_string = "DISABLED";
	}

	if ((int)gyro_invert_yaw.value == 1) {
		gyro_invert_yaw_string = "ENABLED";
	} else {
		gyro_invert_yaw_string = "DISABLED";
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
	float current_anubmode = in_anub_mode.value;

    current_anubmode += 1;
    if (current_anubmode > 1) {
        current_anubmode = 0;
    }

    Cvar_SetValue ("in_anub_mode", current_anubmode);
}

#ifdef __3DS__
void Menu_Controls_ApplyGyro (void)
{
	float current_gyro = gyro_enable.value;
	current_gyro += 1;
	if (current_gyro > 1)
		current_gyro = 0;
	Cvar_SetValue("gyro_enable", current_gyro);
}

void Menu_Controls_ApplyGyroInvertPitch (void)
{
	float val = gyro_invert_pitch.value;
	val += 1;
	if (val > 1)
		val = 0;
	Cvar_SetValue("gyro_invert_pitch", val);
}

void Menu_Controls_ApplyGyroInvertYaw (void)
{
	float val = gyro_invert_yaw.value;
	val += 1;
	if (val > 1)
		val = 0;
	Cvar_SetValue("gyro_invert_yaw", val);
}

void Menu_Controls_OpenGyroSubmenu (void)
{
	controls_gyro_submenu = true;
	Menu_ResetMenuButtons();
}

void Menu_Controls_CloseGyroSubmenu (void)
{
	controls_gyro_submenu = false;
	Menu_ResetMenuButtons();
}
#endif

void Menu_Controls_ApplySettings (void)
{
    // no op
    Menu_SetSound(MENU_SND_ENTER);
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
	#ifdef __3DS__
	Menu_DrawTitle (controls_gyro_submenu ? "GYRO OPTIONS" : "CONTROL OPTIONS", MENU_COLOR_WHITE);
	#else
	Menu_DrawTitle ("CONTROL OPTIONS", MENU_COLOR_WHITE);
	#endif

	Menu_Controls_SetStrings();

	// Map panel makes the background darker
    Menu_DrawMapPanel();

#ifdef __3DS__
	if (!controls_gyro_submenu) {
#endif

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

	if (has_anub) {
		// Anub tolerance
		Menu_DrawButton (controls_buttons++, controls_index++, "A-NUB TOLERANCE", "Change A-Nub Tolerance.", NULL);
		Menu_DrawOptionSlider (controls_buttons-1, controls_index-1, 0, 1, in_tolerance, "tolerance", false, true, 0.25f);

		// Anub-mode (look/move)
		Menu_DrawButton (controls_buttons++, controls_index++, "A-NUB MODE", "Toggle between Look and Move A-Nub Options.", Menu_Controls_ApplyAnubMode);
		Menu_DrawOptionButton (controls_buttons-1, anub_string);
	}

#ifdef __3DS__
	// Open dedicated gyro submenu to avoid controls list overlap.
	Menu_DrawButton (controls_buttons++, controls_index++, "GYRO SETTINGS",
		"Open Gyroscope Settings.", Menu_Controls_OpenGyroSubmenu);
	Menu_DrawOptionButton (controls_buttons-1, gyro_string);
#endif

	// Bindings
	Menu_DrawButton (controls_buttons++, controls_index++, "BINDINGS", "Change Input Bindings.", Menu_Bindings_Set);

#ifdef __3DS__
	} else {
		Menu_DrawButton (controls_buttons++, controls_index++, "BACK TO CONTROLS",
			"Return to Control Options.", Menu_Controls_CloseGyroSubmenu);

		// ---- Gyroscope Settings ----

		// Gyro Enable
		Menu_DrawButton (controls_buttons++, controls_index++, "GYRO AIMING",
			"Enable Gyroscope Aiming.", Menu_Controls_ApplyGyro);
		Menu_DrawOptionButton (controls_buttons-1, gyro_string);

		// Gyro Sensitivity
		Menu_DrawButton (controls_buttons++, controls_index++, "GYRO SENSITIVITY",
			"Overall Gyroscope Sensitivity.", NULL);
		Menu_DrawOptionSlider (controls_buttons-1, controls_index-1,
			0.1f, 5.0f, gyro_sensitivity, "gyro_sensitivity", false, true, 0.1f);

		// Gyro Yaw Sensitivity
		Menu_DrawButton (controls_buttons++, controls_index++, "GYRO YAW SENS",
			"Gyroscope Horizontal Sensitivity.", NULL);
		Menu_DrawOptionSlider (controls_buttons-1, controls_index-1,
			0.1f, 3.0f, gyro_yaw_sensitivity, "gyro_yaw_sensitivity", false, true, 0.1f);

		// Gyro Pitch Sensitivity
		Menu_DrawButton (controls_buttons++, controls_index++, "GYRO PITCH SENS",
			"Gyroscope Vertical Sensitivity.", NULL);
		Menu_DrawOptionSlider (controls_buttons-1, controls_index-1,
			0.1f, 3.0f, gyro_pitch_sensitivity, "gyro_pitch_sensitivity", false, true, 0.1f);

		// Gyro Invert Pitch
		Menu_DrawButton (controls_buttons++, controls_index++, "GYRO INVERT PITCH",
			"Invert Gyroscope Vertical Axis.", Menu_Controls_ApplyGyroInvertPitch);
		Menu_DrawOptionButton (controls_buttons-1, gyro_invert_pitch_string);

		// Gyro Invert Yaw
		Menu_DrawButton (controls_buttons++, controls_index++, "GYRO INVERT YAW",
			"Invert Gyroscope Horizontal Axis.", Menu_Controls_ApplyGyroInvertYaw);
		Menu_DrawOptionButton (controls_buttons-1, gyro_invert_yaw_string);
	}
#endif

	Menu_DrawDivider(-2.5);
	Menu_DrawButton(-2, controls_index++, "APPLY", "Save & Apply Settings.", Menu_Controls_ApplySettings);
	Menu_DrawButton (-1, controls_index, "BACK", "Return to Main Menu.", Menu_Configuration_Set);
}