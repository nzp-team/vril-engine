#pragma once

#include <SDL.h>

extern SDL_Window *sdl_window;
extern SDL_GLContext sdl_gl_context;
extern int sdl_window_width;
extern int sdl_window_height;
extern qboolean sdl_running;
void IN_SDLControllerAdded(int device_index);
void IN_SDLControllerRemoved(SDL_JoystickID instance_id);
void IN_SDLControllerActivated(SDL_JoystickID instance_id);
void VID_SDLResize(void);
void VID_SetFullscreen(qboolean fullscreen);
void VID_SetVSync(qboolean vsync);
