/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2026 NZ:P Team

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
// input.c  -- Global input handler

#include "nzportable_def.h"
/*
void IN_Init (void)
{
#ifdef PLATFORM_INPUT_KBM
    IN_SetMouseToRelative(true);
#else

#endif
}

void IN_Shutdown (void)
{
#ifdef PLATFORM_INPUT_KBM
    IN_SetMouseToRelative(false);
#else

#endif
}

void IN_Commands (void)
{

}
*/
float IN_CalcInput(int axis, float speed, float tolerance, float acceleration)
{
    float value = ((float) axis / 154.0f);

	if (value == 0.0f) {
		return 0.0f;
	}

	float abs_value = fabsf(value);

	if (abs_value < tolerance) {
		return 0.0f;
	}

	abs_value -= tolerance;
	abs_value /= (1.0f - tolerance);
	abs_value = powf(abs_value, acceleration);
	abs_value *= speed;

	if (value < 0.0f) {
		value = -abs_value;
	} else {
		value = abs_value;
	}
	return value;
}
/*
void IN_Move (usercmd_t *cmd)
{
    

    // update crosshair position
	if (move_x < 50 && move_x > -50 && move_y < 50 && move_y > -50) {
		croshhairmoving = false;

		crosshair_opacity += 22;

		if (crosshair_opacity >= 255)
			crosshair_opacity = 255;
	} else {
		croshhairmoving = true;
		crosshair_opacity -= 8;
		if (crosshair_opacity <= 128)
			crosshair_opacity = 128;
	}
}
*/