#include "../../nzportable_def.h"
#include "../../menu/menu_defs.h"
#include "sdl_local.h"

#include <errno.h>
#if defined(_WIN32)
#include <direct.h>
#endif
#include <sys/stat.h>
#include <unistd.h>

#define MAX_HANDLES 32
#define DEFAULT_MEMORY_MB 128

static FILE *sys_handles[MAX_HANDLES];
qboolean isDedicated;
qboolean sdl_running = true;

static int Sys_FindHandle(void)
{
	int i;
	for (i = 1; i < MAX_HANDLES; ++i)
		if (!sys_handles[i]) return i;
	Sys_Error("out of handles");
	return -1;
}

static int Sys_FileLength(FILE *file)
{
	long position = ftell(file);
	long length;
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, position, SEEK_SET);
	return (int)length;
}

int Sys_FileOpenRead(char *path, int *handle)
{
	FILE *file = fopen(path, "rb");
	int index;
	if (!file) { *handle = -1; return -1; }
	index = Sys_FindHandle();
	sys_handles[index] = file;
	*handle = index;
	return Sys_FileLength(file);
}

int Sys_FileOpenWrite(char *path)
{
	FILE *file = fopen(path, "wb");
	int index;
	if (!file) Sys_Error("Error opening %s: %s", path, strerror(errno));
	index = Sys_FindHandle();
	sys_handles[index] = file;
	return index;
}

void Sys_FileClose(int handle) { fclose(sys_handles[handle]); sys_handles[handle] = NULL; }
void Sys_FileSeek(int handle, int position) { fseek(sys_handles[handle], position, SEEK_SET); }
int Sys_FileRead(int handle, void *dest, int count) { return (int)fread(dest, 1, count, sys_handles[handle]); }
int Sys_FileWrite(int handle, void *data, int count) { return (int)fwrite(data, 1, count, sys_handles[handle]); }
int Sys_FileTime(char *path) {

	#if defined(_WIN32)
	struct _stat st;
	return _stat(path, &st) == 0 ? (int)st.st_mtime : -1;
	#else
	struct stat st;
	return stat(path, &st) == 0 ? (int)st.st_mtime : -1;
	#endif
}
void Sys_mkdir(char *path)
{ 
	#if defined(_WIN32)
	_mkdir(path);
	#else
	mkdir(path, 0777);
	#endif
}
void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length) { (void)startaddr; (void)length; }

void Sys_PrintSystemInfo(void) { Con_Printf("Vril Engine SDL (%s)\n", SDL_GetPlatform()); }
void Sys_Printf(char *fmt, ...) { va_list args; va_start(args, fmt); vfprintf(stdout, fmt, args); va_end(args); }
void Sys_SystemError(char *error) { fprintf(stderr, "Vril Engine: %s\n", error); SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vril Engine", error, sdl_window); SDL_Quit(); exit(1); }
void Sys_Quit(void) { sdl_running = false; }
double Sys_FloatTime(void) { static Uint64 start; Uint64 now = SDL_GetPerformanceCounter(); if (!start) start = now; return (double)(now - start) / SDL_GetPerformanceFrequency(); }
char *Sys_ConsoleInput(void) { return NULL; }
void Sys_Sleep(void) { SDL_Delay(1); }
void Sys_HighFPPrecision(void) {}
void Sys_LowFPPrecision(void) {}
void Sys_SetFPCW(void) {}
void Sys_DebugLog(char *file, char *fmt, ...) { (void)file; (void)fmt; }
void Sys_CaptureScreenshot(void)
{
	SDL_Surface *screenshot;
	unsigned char *pixels;
	int width = vid.width;
	int height = vid.height;
	int row_size = width * 3;
	int y;

	pixels = malloc((size_t)row_size * height);
	if (!pixels) {
		Con_Printf("Could not allocate screenshot buffer.\n");
		return;
	}

	glReadBuffer(GL_BACK);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	screenshot = SDL_CreateRGBSurface(0, width, height, 24,
		0xff0000, 0x00ff00, 0x0000ff, 0);
#else
	screenshot = SDL_CreateRGBSurface(0, width, height, 24,
		0x0000ff, 0x00ff00, 0xff0000, 0);
#endif
	if (!screenshot) {
		Con_Printf("Could not create screenshot surface: %s\n", SDL_GetError());
		free(pixels);
		return;
	}

	for (y = 0; y < height; ++y)
		memcpy((byte *)screenshot->pixels + y * screenshot->pitch,
			pixels + (height - y - 1) * row_size, row_size);

	if (SDL_SaveBMP(screenshot, "capture.bmp") != 0)
		Con_Printf("Could not save capture.bmp: %s\n", SDL_GetError());

	SDL_FreeSurface(screenshot);
	free(pixels);
}

void Sys_DefaultConfig(void)
{
	Cbuf_AddText("bind MOUSE1 +attack\n");
	Cbuf_AddText("bind MOUSE2 +aim\n");
	Cbuf_AddText("bind w +forward\n");
	Cbuf_AddText("bind s +back\n");
	Cbuf_AddText("bind a +moveleft\n");
	Cbuf_AddText("bind d +moveright\n");
	Cbuf_AddText("bind SPACE +jump\n");
	Cbuf_AddText("bind BOTTOMFACE +jump\n");
	Cbuf_AddText("bind RIGHTFACE +aim\n");
	Cbuf_AddText("bind ZRTRIGGER +attack\n");
}


int mouse_dx;
int mouse_dy;

static int SDL_KeyToQuake(SDL_Keycode key)
{
	switch (key) {
	case SDLK_UP: return K_UPARROW; case SDLK_DOWN: return K_DOWNARROW;
	case SDLK_LEFT: return K_LEFTARROW; case SDLK_RIGHT: return K_RIGHTARROW;
	case SDLK_ESCAPE: return K_ESCAPE; case SDLK_RETURN: case SDLK_KP_ENTER: return K_ENTER;
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
	switch (button) { case SDL_BUTTON_LEFT: return K_MOUSE1; case SDL_BUTTON_RIGHT: return K_MOUSE2; case SDL_BUTTON_MIDDLE: return K_MOUSE3; default: return 0; }
}

static int SDL_ControllerToQuake(SDL_GameControllerButton button)
{
	switch (button) {
	case SDL_CONTROLLER_BUTTON_A: return K_BOTTOMFACE;
	case SDL_CONTROLLER_BUTTON_B: return K_RIGHTFACE;
	case SDL_CONTROLLER_BUTTON_X: return K_LEFTFACE;
	case SDL_CONTROLLER_BUTTON_Y: return K_TOPFACE;
	case SDL_CONTROLLER_BUTTON_BACK: return K_SELECT;
	case SDL_CONTROLLER_BUTTON_START: return K_START;
	case SDL_CONTROLLER_BUTTON_LEFTSTICK: return K_LTHUMB;
	case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return K_RTHUMB;
	case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return K_LTRIGGER;
	case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return K_RTRIGGER;
	case SDL_CONTROLLER_BUTTON_DPAD_UP: return K_DPAD_UP;
	case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return K_DPAD_DOWN;
	case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return K_DPAD_LEFT;
	case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return K_DPAD_RIGHT;
	default: return 0;
	}
}

static void SDL_MenuCoordinates(int window_x, int window_y, int *drawable_x, int *drawable_y)
{
	int window_width, window_height;
	SDL_GetWindowSize(sdl_window, &window_width, &window_height);
	*drawable_x = window_width > 0 ? window_x * (int)vid.width / window_width : window_x;
	*drawable_y = window_height > 0 ? window_y * (int)vid.height / window_height : window_y;
}

void Sys_SendKeyEvents(void)
{
	SDL_Event event;
	static qboolean trigger_down[2];
	while (SDL_PollEvent(&event)) {
		int key;
		switch (event.type) {
		case SDL_QUIT: sdl_running = false; break;
		case SDL_KEYDOWN: case SDL_KEYUP:
			if (event.type == SDL_KEYDOWN) { IN_SetActiveDevice(IN_DEVICE_KEYBOARD_MOUSE); Menu_SetInputDevice(IN_DEVICE_KEYBOARD_MOUSE); }
			key = SDL_KeyToQuake(event.key.keysym.sym);
			if (key && !event.key.repeat) Key_Event(key, event.type == SDL_KEYDOWN);
			break;
		case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
			if (event.type == SDL_MOUSEBUTTONDOWN) { IN_SetActiveDevice(IN_DEVICE_KEYBOARD_MOUSE); Menu_SetInputDevice(IN_DEVICE_KEYBOARD_MOUSE); }
			key = SDL_MouseToQuake(event.button.button);
			if (key && (key_dest == key_menu || key_dest == key_menu_pause)) {
				int x, y;
				qboolean slider_handled = false;
				SDL_MenuCoordinates(event.button.x, event.button.y, &x, &y);
				Menu_MouseMove(x, y);
				if (key == K_MOUSE1)
					slider_handled = Menu_MouseButton(x, y, event.type == SDL_MOUSEBUTTONDOWN);
				if (event.type == SDL_MOUSEBUTTONDOWN && key == K_MOUSE1 && !slider_handled) Menu_ButtonPress();
			} else if (key) Key_Event(key, event.type == SDL_MOUSEBUTTONDOWN);
			break;
		case SDL_MOUSEMOTION:
			if (event.motion.xrel || event.motion.yrel) { IN_SetActiveDevice(IN_DEVICE_KEYBOARD_MOUSE); Menu_SetInputDevice(IN_DEVICE_KEYBOARD_MOUSE); }
			if (key_dest == key_menu || key_dest == key_menu_pause) {
				int x, y;
				SDL_MenuCoordinates(event.motion.x, event.motion.y, &x, &y);
				Menu_MouseMove(x, y);
			}
			else { mouse_dx += event.motion.xrel; mouse_dy += event.motion.yrel; }
			break;
		case SDL_MOUSEWHEEL:
			if (key_dest == key_menu || key_dest == key_menu_pause) {
				if (event.wheel.y > 0) Menu_IncreaseCursor();
				if (event.wheel.y < 0) Menu_DecreaseCursor();
			}
			break;
		case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP:
			if (event.type == SDL_CONTROLLERBUTTONDOWN) { IN_SetActiveDevice(IN_DEVICE_GAMEPAD); Menu_SetInputDevice(IN_DEVICE_GAMEPAD); }
			key = SDL_ControllerToQuake(event.cbutton.button);
			if (key) Key_Event(key, event.type == SDL_CONTROLLERBUTTONDOWN);
			break;
		case SDL_CONTROLLERAXISMOTION:
			if (event.caxis.value > 8192 || event.caxis.value < -8192) { IN_SetActiveDevice(IN_DEVICE_GAMEPAD); Menu_SetInputDevice(IN_DEVICE_GAMEPAD); }
			if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ||
				event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
				int trigger = event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? 0 : 1;
				qboolean down = event.caxis.value > 16384;
				if (down != trigger_down[trigger]) {
					trigger_down[trigger] = down;
					Key_Event(trigger ? K_ZRTRIGGER : K_ZLTRIGGER, down);
				}
			}
			break;
		case SDL_CONTROLLERDEVICEADDED: IN_SDLControllerAdded(event.cdevice.which); break;
		case SDL_CONTROLLERDEVICEREMOVED: IN_SDLControllerRemoved(event.cdevice.which); break;
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event == SDL_WINDOWEVENT_RESIZED)
				VID_SDLResize();
			break;
		}
	}
	SDL_SetRelativeMouseMode((key_dest == key_game && SDL_GetKeyboardFocus() == sdl_window) ? SDL_TRUE : SDL_FALSE);
}

int main(int argc, char **argv)
{
	quakeparms_t parms;
	startup_arguments_t startup;
	const char *base_directory;
	char startup_error[256];
	size_t heap_size;
	double oldtime;
	qboolean headless_test;
	memset(&parms, 0, sizeof(parms));
	if (!Startup_LoadArguments(&startup, argc, argv, "setup.ini",
		startup_error, sizeof(startup_error))) {
		fprintf(stderr, "Startup: %s\n", startup_error);
		return 1;
	}
	if (!Startup_GetBaseDirectory(&startup, ".", &base_directory,
		startup_error, sizeof(startup_error))) {
		fprintf(stderr, "Startup: %s\n", startup_error);
		Startup_FreeArguments(&startup);
		return 1;
	}
	headless_test = TestHandler_ArgumentsAllowHeadless(startup.argc, startup.argv);
	if (SDL_Init(headless_test ? SDL_INIT_TIMER :
		(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER)) != 0) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		Startup_FreeArguments(&startup);
		return 1;
	}
	parms.membase = Startup_AllocateHeap(&startup, DEFAULT_MEMORY_MB * 1024 * 1024,
		&heap_size, startup_error, sizeof(startup_error));
	if (!parms.membase) {
		fprintf(stderr, "Startup: %s\n", startup_error);
		Startup_FreeArguments(&startup);
		SDL_Quit();
		return 1;
	}
	parms.memsize = (int)heap_size;
	parms.basedir = (char *)base_directory;
	COM_InitArgv(startup.argc, startup.argv);
	parms.argc = com_argc;
	parms.argv = com_argv;
	Host_Init(&parms);
	oldtime = Sys_FloatTime();
	while (sdl_running) {
		double now = Sys_FloatTime();
		Host_Frame(now - oldtime);
		music_update();
		oldtime = now;
	}
	if (host_initialized)
		Host_Shutdown();
	free(parms.membase);
	Startup_FreeArguments(&startup);
	SDL_Quit();
	return 0;
}
