#include "../../../nzportable_def.h"
#include "../sdl_local.h"

SDL_Window *sdl_window;
SDL_GLContext sdl_gl_context;
int sdl_window_width = 1280;
int sdl_window_height = 720;
cvar_t vid_width = {"vid_width", "1280", true};
cvar_t vid_height = {"vid_height", "720", true};
cvar_t vid_fullscreen = {"vid_fullscreen", "0", true};
cvar_t r_vsync = {"r_vsync", "1", true};

unsigned d_8to24table[256];
unsigned char d_15to8table[65536];
int texture_mode = GL_LINEAR;
double gldepthmin, gldepthmax;
cvar_t gl_ztrick = {"gl_ztrick", "0"};
qboolean isPermedia = true;
qboolean gl_mtexable = false;
static float vid_gamma = 1.0f;
static int sdl_fullscreen = -1;
static int sdl_vsync = -1;

void GL_Init(void)
{
	glClearDepth(1.0);
	glClearColor(16.0f / 255.0f, 32.0f / 255.0f, 64.0f / 255.0f, 1.0f);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_FLAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glClear(GL_COLOR_BUFFER_BIT);
}

void GL_BeginRendering(int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	SDL_GL_GetDrawableSize(sdl_window, width, height);
	sdl_window_width = *width;
	sdl_window_height = *height;
}

void VID_SDLResize(void)
{
	int width, height;
	if (!sdl_window) return;
	SDL_GL_GetDrawableSize(sdl_window, &width, &height);
	if (width < 1 || height < 1 || (width == (int)vid.width && height == (int)vid.height)) return;
	sdl_window_width = width;
	sdl_window_height = height;
	vid.width = vid.maxwarpwidth = width;
	vid.height = vid.maxwarpheight = height;
	vid.aspect = ((float)height / width) * (320.0f / 240.0f);
	vid.scale = height / STD_UI_HEIGHT;
	if (vid.scale < 1) vid.scale = 1;
	vid.conwidth = width;
	vid.conheight = height;
	vid.recalc_refdef = true;
}

void VID_SetFullscreen(qboolean fullscreen)
{
	if (!sdl_window) return;
	if (SDL_SetWindowFullscreen(sdl_window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) {
		Con_Printf("Could not change fullscreen mode: %s\n", SDL_GetError());
		return;
	}
	Cvar_SetValue("vid_fullscreen", fullscreen ? 1 : 0);
	sdl_fullscreen = fullscreen ? 1 : 0;
	VID_SDLResize();
}

void VID_SetVSync(qboolean vsync)
{
	if (!sdl_gl_context) return;
	if (SDL_GL_SetSwapInterval(vsync ? 1 : 0) != 0) {
		Con_Printf("Could not change VSync: %s\n", SDL_GetError());
		return;
	}
	Cvar_SetValue("r_vsync", vsync ? 1 : 0);
	sdl_vsync = vsync ? 1 : 0;
}

void GL_EndRendering(void)
{
	int fullscreen = vid_fullscreen.value != 0.0f;
	int vsync = r_vsync.value != 0.0f;
	if (fullscreen != sdl_fullscreen)
		VID_SetFullscreen(fullscreen);
	if (vsync != sdl_vsync)
		VID_SetVSync(vsync);
	SDL_GL_SwapWindow(sdl_window);
}

void VID_SetPalette(unsigned char *palette)
{
	unsigned i;
	for (i = 0; i < 256; ++i)
		d_8to24table[i] = (255u << 24) | palette[i * 3] | (palette[i * 3 + 1] << 8) | (palette[i * 3 + 2] << 16);
	d_8to24table[255] &= 0x00ffffff;
}

void VID_ShiftPalette(unsigned char *palette) { (void)palette; }

static void Check_Gamma(unsigned char *pal)
{
	unsigned char adjusted[768];
	int i, parameter = COM_CheckParm("-gamma");
	vid_gamma = parameter && parameter + 1 < com_argc ? Q_atof(com_argv[parameter + 1]) : 0.7f;
	for (i = 0; i < 768; ++i) {
		float value = powf((pal[i] + 1) / 256.0f, vid_gamma) * 255.0f + 0.5f;
		adjusted[i] = (unsigned char)(value < 0 ? 0 : value > 255 ? 255 : value);
	}
	memcpy(pal, adjusted, sizeof(adjusted));
}

void VID_Init(unsigned char *palette)
{
	int parameter;
	Cvar_RegisterVariable(&vid_width);
	Cvar_RegisterVariable(&vid_height);
	Cvar_RegisterVariable(&vid_fullscreen);
	Cvar_RegisterVariable(&r_vsync);
	sdl_window_width = (int)vid_width.value;
	sdl_window_height = (int)vid_height.value;
	parameter = COM_CheckParm("-width");
	if (!parameter) parameter = COM_CheckParm("+vid_width");
	if (parameter && parameter + 1 < com_argc) sdl_window_width = Q_atoi(com_argv[parameter + 1]);
	parameter = COM_CheckParm("-height");
	if (!parameter) parameter = COM_CheckParm("+vid_height");
	if (parameter && parameter + 1 < com_argc) sdl_window_height = Q_atoi(com_argv[parameter + 1]);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	sdl_window = SDL_CreateWindow("Nazi Zombies: Portable", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		sdl_window_width, sdl_window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
		(vid_fullscreen.value ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
	if (!sdl_window) Sys_Error("SDL_CreateWindow: %s", SDL_GetError());
	sdl_gl_context = SDL_GL_CreateContext(sdl_window);
	if (!sdl_gl_context) Sys_Error("SDL_GL_CreateContext: %s", SDL_GetError());
	sdl_fullscreen = !!vid_fullscreen.value;
	if (SDL_GL_SetSwapInterval(r_vsync.value ? 1 : 0) != 0)
		Con_Printf("Could not set VSync: %s\n", SDL_GetError());
	sdl_vsync = !!r_vsync.value;
	SDL_GL_GetDrawableSize(sdl_window, &sdl_window_width, &sdl_window_height);

	Cvar_RegisterVariable(&gl_ztrick);
	vid.maxwarpwidth = vid.width = sdl_window_width;
	vid.maxwarpheight = vid.height = sdl_window_height;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));
	vid.conwidth = 640;
	vid.conheight = 480;
	vid.aspect = ((float)vid.height / vid.width) * (320.0f / 240.0f);
	vid.scale = vid.height / STD_UI_HEIGHT;
	if (vid.scale < 1) vid.scale = 1;
	vid.numpages = 2;
	GL_Init();
	Check_Gamma(palette);
	VID_SetPalette(palette);
	Con_SafePrintf("SDL OpenGL video mode %dx%d initialized.\n", sdl_window_width, sdl_window_height);
}

void VID_Shutdown(void)
{
	if (sdl_gl_context) SDL_GL_DeleteContext(sdl_gl_context);
	if (sdl_window) SDL_DestroyWindow(sdl_window);
	sdl_gl_context = NULL;
	sdl_window = NULL;
}
