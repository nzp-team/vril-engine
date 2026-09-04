#include "../../nzportable_def.h"
#include "sdl_local.h"

extern int mouse_dx;
extern int mouse_dy;

qboolean IN_PlatformHasMouse(void) { return true; }
qboolean IN_PlatformHasGamepad(void) { return true; }

void IN_SetMouseToRelative(bool relative)
{
	if (relative)
		SDL_SetRelativeMouseMode(SDL_TRUE);
	else
		SDL_SetRelativeMouseMode(SDL_FALSE);
}

void IN_PlatformClearPendingInput(void)
{
	mouse_dx = 0;
	mouse_dy = 0;
}

void IN_PlatformMouseMove(usercmd_t *cmd)
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

#define MAX_SDL_CONTROLLERS 4
static SDL_GameController *sdl_controllers[MAX_SDL_CONTROLLERS];
static SDL_GameController *sdl_controller;

static void IN_SDLOpenController(int device_index)
{
	int i;
	SDL_GameController *controller;

	if (!SDL_IsGameController(device_index)) return;
	controller = SDL_GameControllerOpen(device_index);
	if (!controller) return;
	for (i = 0; i < MAX_SDL_CONTROLLERS; ++i) {
		if (!sdl_controllers[i]) {
			sdl_controllers[i] = controller;
			if (!sdl_controller) sdl_controller = controller;
			return;
		}
	}
	SDL_GameControllerClose(controller);
}

void IN_PlatformInit(void)
{
	int i;
	Cvar_SetValue("in_anub_mode", 1);
	for (i = 0; i < SDL_NumJoysticks(); ++i)
		IN_SDLOpenController(i);
}

void IN_PlatformShutdown(void)
{
	int i;
	for (i = 0; i < MAX_SDL_CONTROLLERS; ++i) {
		if (sdl_controllers[i]) SDL_GameControllerClose(sdl_controllers[i]);
		sdl_controllers[i] = NULL;
	}
	sdl_controller = NULL;
}

void IN_PlatformCommands(void) {}
void IN_PlatformMove(usercmd_t *cmd) { (void)cmd; }

void IN_GetAnalogStick(in_analog_stick_id_t stick, in_analog_stick_t *value)
{
	SDL_GameControllerAxis xaxis = stick == IN_STICK_LEFT ? SDL_CONTROLLER_AXIS_LEFTX : SDL_CONTROLLER_AXIS_RIGHTX;
	SDL_GameControllerAxis yaxis = stick == IN_STICK_LEFT ? SDL_CONTROLLER_AXIS_LEFTY : SDL_CONTROLLER_AXIS_RIGHTY;
	value->x = value->y = 0.0f;
	if (!sdl_controller) return;
	value->x = SDL_GameControllerGetAxis(sdl_controller, xaxis) / 32767.0f;
	value->y = -SDL_GameControllerGetAxis(sdl_controller, yaxis) / 32767.0f;
}

void IN_SDLControllerAdded(int device_index)
{
	IN_SDLOpenController(device_index);
}

void IN_SDLControllerActivated(SDL_JoystickID instance_id)
{
	int i;
	for (i = 0; i < MAX_SDL_CONTROLLERS; ++i) {
		if (sdl_controllers[i] &&
			SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(sdl_controllers[i])) == instance_id) {
			sdl_controller = sdl_controllers[i];
			return;
		}
	}
}

void IN_SDLControllerRemoved(SDL_JoystickID instance_id)
{
	int i;
	for (i = 0; i < MAX_SDL_CONTROLLERS; ++i) {
		if (sdl_controllers[i] &&
			SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(sdl_controllers[i])) == instance_id) {
			if (sdl_controller == sdl_controllers[i]) sdl_controller = NULL;
			SDL_GameControllerClose(sdl_controllers[i]);
			sdl_controllers[i] = NULL;
			break;
		}
	}
	if (!sdl_controller)
		for (i = 0; i < MAX_SDL_CONTROLLERS; ++i)
			if (sdl_controllers[i]) { sdl_controller = sdl_controllers[i]; break; }
}
