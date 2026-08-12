#include "../../nzportable_def.h"
#include "sdl_local.h"

extern int mouse_dx;
extern int mouse_dy;

void IN_SetMouseToRelative(bool relative)
{
	if (relative)
		SDL_SetRelativeMouseMode(SDL_TRUE);
	else
		SDL_SetRelativeMouseMode(SDL_FALSE);
}

void IN_Move(usercmd_t *cmd)
{
	(void)cmd;

	V_StopPitchDrift();
	if (mouse_dx || mouse_dy) {
		float pitch_direction = m_pitch.value > 0 ? -1.0f : 1.0f;
		cl.viewangles[YAW] -= mouse_dx * sensitivity.value * 0.022f;
		cl.viewangles[PITCH] += mouse_dy * sensitivity.value * 0.022f * pitch_direction;
		if (cl.viewangles[PITCH] > 80) cl.viewangles[PITCH] = 80;
		if (cl.viewangles[PITCH] < -70) cl.viewangles[PITCH] = -70;
		mouse_dx = mouse_dy = 0;
	}
}
