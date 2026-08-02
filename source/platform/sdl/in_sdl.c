#include "../../nzportable_def.h"
#include "sdl_local.h"

static int mouse_dx;
static int mouse_dy;

static int SDL_KeyToQuake(SDL_Keycode key)
{
	switch (key) {
	case SDLK_UP: return K_UPARROW; case SDLK_DOWN: return K_DOWNARROW;
	case SDLK_LEFT: return K_LEFTARROW; case SDLK_RIGHT: return K_RIGHTARROW;
	case SDLK_ESCAPE: return K_ESCAPE; case SDLK_RETURN: return K_ENTER;
	case SDLK_TAB: return K_TAB; case SDLK_BACKSPACE: return '\b';
	case SDLK_DELETE: return K_DELETE;
	case SDLK_F1: return K_AUX1; case SDLK_F2: return K_AUX2; case SDLK_F3: return K_AUX3;
	case SDLK_F4: return K_AUX4; case SDLK_F5: return K_AUX5; case SDLK_F6: return K_AUX6;
	case SDLK_F7: return K_AUX7; case SDLK_F8: return K_AUX8; case SDLK_F9: return K_AUX9;
	case SDLK_F10: return K_AUX10; case SDLK_F11: return K_AUX11; case SDLK_F12: return K_AUX12;
	case SDLK_LSHIFT: case SDLK_RSHIFT: return K_SHIFT;
	case SDLK_LCTRL: case SDLK_RCTRL: return K_CTRL;
	case SDLK_SPACE: return K_SPACE;
	default: return key >= 32 && key < 127 ? (int)key : 0;
	}
}

static int SDL_MouseToQuake(Uint8 button)
{
	switch (button) { case SDL_BUTTON_LEFT: return K_JOY1; case SDL_BUTTON_RIGHT: return K_JOY2; case SDL_BUTTON_MIDDLE: return K_JOY3; default: return 0; }
}

void Sys_SendKeyEvents(void)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		int key;
		switch (event.type) {
		case SDL_QUIT: sdl_running = false; break;
		case SDL_KEYDOWN: case SDL_KEYUP:
			key = SDL_KeyToQuake(event.key.keysym.sym);
			if (key && !event.key.repeat) Key_Event(key, event.type == SDL_KEYDOWN);
			break;
		case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
			key = SDL_MouseToQuake(event.button.button);
			if (key) Key_Event(key, event.type == SDL_MOUSEBUTTONDOWN);
			break;
		case SDL_MOUSEMOTION: mouse_dx += event.motion.xrel; mouse_dy += event.motion.yrel; break;
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) SDL_SetRelativeMouseMode(SDL_TRUE);
			break;
		}
	}
}

void IN_Init(void) { SDL_SetRelativeMouseMode(SDL_TRUE); }
void IN_Shutdown(void) { SDL_SetRelativeMouseMode(SDL_FALSE); }
void IN_Commands(void) {}
void IN_ClearStates(void) { mouse_dx = mouse_dy = 0; }
void IN_Move(usercmd_t *cmd)
{
	(void)cmd;
	if (mouse_dx || mouse_dy) {
		float pitch_direction = m_pitch.value > 0 ? -1.0f : 1.0f;
		V_StopPitchDrift();
		cl.viewangles[YAW] -= mouse_dx * sensitivity.value * 0.022f;
		cl.viewangles[PITCH] += mouse_dy * sensitivity.value * 0.022f * pitch_direction;
		if (cl.viewangles[PITCH] > 80) cl.viewangles[PITCH] = 80;
		if (cl.viewangles[PITCH] < -70) cl.viewangles[PITCH] = -70;
		mouse_dx = mouse_dy = 0;
	}
}
