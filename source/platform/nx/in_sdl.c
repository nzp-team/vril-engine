/*
Copyright (C) 1996-1997 Id Software, Inc.

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

#include <SDL2/SDL.h>

#include "../../nzportable_def.h"


#define K_UNKNOWN 0

cvar_t in_snd_block = {"in_snd_block", "0"};

static qboolean mouse_available;
static int mouse_x, mouse_y;
static int joy_axis[4];
static SDL_Joystick *joystick[2];

static void JoyToKey(int button, int pressed) {
    static const int keymap[] = {
        /* KEY_A      */ K_ENTER,
        /* KEY_B      */ K_BACKSPACE,
        /* KEY_X      */ K_ALT,
        /* KEY_Y      */ K_ALT,
        /* KEY_LSTICK */ K_UNKNOWN,
        /* KEY_RSTICK */ K_UNKNOWN,
        /* KEY_L      */ K_SHIFT,
        /* KEY_R      */ K_SHIFT,
        /* KEY_ZL     */ K_SPACE,
        /* KEY_ZR     */ K_CTRL,
        /* KEY_PLUS   */ K_ESCAPE,
        /* KEY_MINUS  */ K_TAB,
        /* KEY_DLEFT  */ K_LEFTARROW,
        /* KEY_DUP    */ K_UPARROW,
        /* KEY_DRIGHT */ K_RIGHTARROW,
        /* KEY_DDOWN  */ K_DOWNARROW,
    };

    if (button < 0 || button > 15) return;

    Key_Event(keymap[button], pressed);
}

void IN_ProcessEvents(void) {
    SDL_Event event;
    SDL_Keycode keycode;
    int keystate, button, keynum;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                keycode = event.key.keysym.sym;
                keystate = event.key.state;
                switch (keycode) {
                    case SDLK_UNKNOWN:
                        keynum = K_UNKNOWN;
                        break;
                    case SDLK_BACKSPACE:
                        keynum = K_BACKSPACE;
                        break;
                    case SDLK_TAB:
                        keynum = K_TAB;
                        break;
                    case SDLK_RETURN:
                        keynum = K_ENTER;
                        break;
                    case SDLK_PAUSE:
                        keynum = K_PAUSE;
                        break;
                    case SDLK_ESCAPE:
                        keynum = K_ESCAPE;
                        break;
                    case SDLK_SPACE:
                        keynum = K_SPACE;
                        break;
                    case SDLK_RALT:
                        keynum = K_ALT;
                        break;
                    case SDLK_LALT:
                        keynum = K_ALT;
                        break;
                    default:
                        keynum = K_UNKNOWN;
                        break;
                }
                Key_Event(keynum, keystate);

#ifdef DEBUG
                Sys_Printf(
                    "%s: SDL keycode = %s (%d), SDL scancode = %s (%d), "
                    "Quake key = %s (%d)\n",
                    __func__, SDL_GetKeyName(keycode), (int)keycode,
                    SDL_GetScancodeName(event.key.keysym.scancode), (int)event.key.keysym.scancode,
                    Key_KeynumToString(keynum), keynum);
#endif
                break;

            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
                JoyToKey(event.jbutton.button, event.type == SDL_JOYBUTTONDOWN);
                break;

            case SDL_JOYAXISMOTION:
                if (event.jaxis.axis >= 0 && event.jaxis.axis < 4)
                    joy_axis[event.jaxis.axis] = event.jaxis.value / 0x100;
                break;

            case SDL_QUIT:
                Sys_Quit();
                break;

            default:
                break;
        }
    }
}
/*
static void IN_GrabMouse(int grab) {
    SDL_SetRelativeMouseMode(grab);
}

static void windowed_mouse_f(struct cvar_s *var) {}

static cvar_t m_filter = {"m_filter", "0"};
cvar_t _windowed_mouse = {"_windowed_mouse", "0", true, false, 0, windowed_mouse_f};

// FIXME - is this target independent?
static void IN_MouseMove(usercmd_t *cmd) {
    static float old_mouse_x, old_mouse_y;

    if (!mouse_available) return;

    if (m_filter.value) {
        mouse_x = (mouse_x + old_mouse_x) * 0.5;
        mouse_y = (mouse_y + old_mouse_y) * 0.5;
    }

    old_mouse_x = mouse_x;
    old_mouse_y = mouse_y;

    mouse_x *= sensitivity.value;
    mouse_y *= sensitivity.value;

    if ((in_strafe.state & 1) ||
        (lookstrafe.value && ((in_mlook.state & 1) ^ (int)m_freelook.value)))
        cmd->sidemove += m_side.value * mouse_x;
    else
        cl.viewangles[YAW] -= m_yaw.value * mouse_x;
    if ((in_mlook.state & 1) ^ (int)m_freelook.value)
        if (mouse_x || mouse_y) V_StopPitchDrift();

    if (((in_mlook.state & 1) ^ (int)m_freelook.value) && !(in_strafe.state & 1)) {
        cl.viewangles[PITCH] += m_pitch.value * mouse_y;
        if (cl.viewangles[PITCH] > 80) cl.viewangles[PITCH] = 80;
        if (cl.viewangles[PITCH] < -70) cl.viewangles[PITCH] = -70;
    } else {
        if ((in_strafe.state & 1) && noclip_anglehack)
            cmd->upmove -= m_forward.value * mouse_y;
        else
            cmd->forwardmove -= m_forward.value * mouse_y;
    }
    mouse_x = mouse_y = 0.0;
}
    */

static void IN_JoyMove(usercmd_t *cmd) {
    int lx = joy_axis[0];
    int ly = joy_axis[1];
    int rx = joy_axis[2];
    int ry = joy_axis[3];

    // stolen from Rinnegatamante's vitaQuake

    int x_mov = abs(lx) < 30 ? 0 : (lx * cl_sidespeed) * 0.01;
    int y_mov = abs(ly) < 30 ? 0 : (ly * (ly > 0 ? cl_backspeed : cl_forwardspeed)) * 0.01;
    cmd->forwardmove -= y_mov;
    cmd->sidemove += x_mov;

    int x_cam = abs(rx) < 50 ? 0 : rx * sensitivity.value * 0.008;
    int y_cam = abs(ry) < 50 ? 0 : ry * sensitivity.value * 0.008;
    cl.viewangles[YAW] -= x_cam;

    V_StopPitchDrift();
    int invert = m_pitch.value < 0 ? -1 : 1;
    cl.viewangles[PITCH] += y_cam * invert;
}

void IN_Init(void) {
    int err;

    err = SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    if (err < 0) Sys_Error("IN: Couldn't load SDL: %s", SDL_GetError());

#if 0
    SDL_EnableUNICODE(1); // Enable UNICODE translation for keyboard input
#endif

    joystick[0] = SDL_JoystickOpen(0);
    joystick[1] = SDL_JoystickOpen(1);

    mouse_x = mouse_y = 0.0;
    mouse_available = !COM_CheckParm("-nomouse");

    //IN_GrabMouse(mouse_available);

    Cvar_RegisterVariable(&in_snd_block);
    //Cvar_RegisterVariable(&m_filter);
    //Cvar_RegisterVariable(&_windowed_mouse);
}

void IN_Shutdown(void) {
    if (joystick[0])
      SDL_JoystickClose(joystick[0]);
    if (joystick[1])
      SDL_JoystickClose(joystick[1]);

    mouse_available = 0;
}

/* Possibly don't need these? */
void IN_Accumulate(void) {}
void IN_UpdateClipCursor(void) {}
void IN_Move(usercmd_t *cmd) {
    //IN_MouseMove(cmd);
    IN_JoyMove(cmd);
}

void IN_Commands(void) {
    IN_ProcessEvents();
}

//
// ctr software keyboard courtesy of libctru samples
//
void IN_SwitchKeyboard(void)
{
    Con_Printf("IN_SwitchKeyboard not implemented\n");
}
