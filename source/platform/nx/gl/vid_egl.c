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

#include <EGL/egl.h>
#include <SDL2/sdl.h>

#include "../../../nzportable_def.h"
#include <switch.h>

#define WARP_WIDTH 320
#define WARP_HEIGHT 200

int vid_modenum = VID_MODE_NONE;

static cvar_t vid_mode = {"vid_mode", "0"};

unsigned short d_8to16table[256];
unsigned d_8to24table[256];
unsigned char d_15to8table[65536];

viddef_t vid;

qboolean VID_IsFullScreen(void) { return vid_modenum != 0; }

qboolean VID_CheckAdequateMem(int width, int height) { return true; }

void (*VID_SetGammaRamp)(unsigned short ramp[3][256]) = NULL;

float gldepthmin, gldepthmax;
qboolean gl_mtexable;
cvar_t gl_ztrick = {"gl_ztrick", "0"};

void VID_Update(vrect_t *rects) {}
void D_BeginDirectRect(int x, int y, const byte *pbitmap, int width, int height) {}
void D_EndDirectRect(int x, int y, int width, int height) {}

/*
 * FIXME!!
 *
 * Move stuff around or create abstractions so these hacks aren't needed
 */
void IN_ProcessEvents(void);
void Sys_SendKeyEvents(void) { IN_ProcessEvents(); }

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

static void setMesaConfig() {
    // Uncomment below to disable error checking and save CPU time (useful for
    // production): setenv("MESA_NO_ERROR", "1", 1);

    setenv("MESA_GL_VERSION_OVERRIDE", "3.2COMPAT", 1);

//#ifdef GPUDEBUG
    setenv("EGL_LOG_LEVEL", "debug", 1);
    setenv("MESA_VERBOSE", "all", 1);
    setenv("MESA_DEBUG", "1", 1);
    setenv("NOUVEAU_MESA_DEBUG", "1", 1);
    setenv("NV50_PROG_OPTIMIZE", "0", 1);
    setenv("NV50_PROG_DEBUG", "1", 1);
    setenv("NV50_PROG_CHIPSET", "0x120", 1);
//#endif
}

void Sys_InitSDL (void)
{
	SDL_version v;
	SDL_version *sdl_version = &v;
	SDL_GetVersion(&v);

	Sys_Printf("Found SDL version %i.%i.%i\n",sdl_version->major,sdl_version->minor,sdl_version->patch);
}

EGLDisplay s_display;
EGLSurface s_surface;
EGLContext s_context;

static qboolean InitEGL (NWindow *input_win)
{
	// Connect to the EGL default display
	s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (!s_display)
	{
		Sys_Error("Could not connect to display! error: %d", eglGetError());
		goto _fail0;
	}

	// Initialize the EGL display connection
	eglInitialize(s_display, NULL, NULL);

	if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE)
	{
		Sys_Error("Could not set API! error: %d", eglGetError());
		goto _fail1;
	}

	// Get an appropriate EGL framebuffer configuration
	EGLConfig config;
	EGLint numConfigs;
	static const EGLint attributeList[] =
	{
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_NONE
	};
	eglChooseConfig(s_display, attributeList, &config, 1, &numConfigs);
	if (numConfigs == 0)
	{
		Sys_Error("No config found! error: %d", eglGetError());
		goto _fail1;
	}

	// Create an EGL window surface
	s_surface = eglCreateWindowSurface(s_display, config, input_win, NULL);
	
	if (!s_surface)
	{
		Sys_Error("Surface creation failed! error: %d", eglGetError());
		goto _fail1;
	}

	static const EGLint ctxAttributeList[] = 
	{
		EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
		EGL_NONE
	};

	// Create an EGL rendering context
	s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, ctxAttributeList);
	if (!s_context)
	{
		Sys_Error("Context creation failed! error: %d", eglGetError());
		goto _fail2;
	}

	// Connect the context to the surface
	eglMakeCurrent(s_display, s_surface, s_surface, s_context);

	return true;

_fail2:
	eglDestroySurface(s_display, s_surface);
	s_surface = NULL;
_fail1:
	eglTerminate(s_display);
	s_display = NULL;
_fail0:
	return false;
}

static void DeinitEGL(void) {
    if (s_display) {
        eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (s_context) {
            eglDestroyContext(s_display, s_context);
            s_context = NULL;
        }
        if (s_surface) {
            eglDestroySurface(s_display, s_surface);
            s_surface = NULL;
        }
        eglTerminate(s_display);
        s_display = NULL;
    }
}

static void VID_InitGL(void) {
    Cvar_RegisterVariable(&vid_mode);
    Cvar_RegisterVariable(&gl_ztrick);

    // Must be first
    appletInitialize();
    appletLockExit(); // prevent HOME exit race

    NWindow *win = nwindowGetDefault();

    setMesaConfig ();

    // Now you can safely create EGL surface/context
    if (!InitEGL(win))
        Sys_Error("Failed to initialize EGL!\n");

    eglSwapInterval(s_display, 1);

    int version = gladLoadGL(eglGetProcAddress);
    if (version == 0) {
        Sys_Error("Failed to initialize OpenGL context\n");
        return;
    }

    // Successfully loaded OpenGL
    printf("Loaded OpenGL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
/*
    gl_vendor = (const char *)glGetString(GL_VENDOR);
    gl_renderer = (const char *)glGetString(GL_RENDERER);
    gl_version = (const char *)glGetString(GL_VERSION);
    gl_extensions = (const char *)glGetString(GL_EXTENSIONS);

    Con_Printf("GL_VENDOR: %s\n", gl_vendor);
    Con_Printf("GL_RENDERER: %s\n", gl_renderer);
    Con_Printf("GL_VERSION: %s\n", gl_version);
    Con_Printf("GL_EXTENSIONS: %s\n", gl_extensions);
*/
    gl_mtexable = false;

    glCullFace(GL_FRONT);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

}

qboolean VID_SetMode(const qvidmode_t *mode, const byte *palette) {
    vid.numpages = 1;
    vid.width = vid.conwidth = mode->width;
    vid.height = vid.conheight = mode->height;
    vid.maxwarpwidth = WARP_WIDTH;
    vid.maxwarpheight = WARP_HEIGHT;
    vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);

    vid.colormap = host_colormap;
    vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));

    vid_modenum = mode - modelist;
    Cvar_SetValue("vid_mode", vid_modenum);
    Cvar_SetValue("vid_fullscreen", 1);
    Cvar_SetValue("vid_width", mode->width);
    Cvar_SetValue("vid_height", mode->height);
    Cvar_SetValue("vid_bpp", mode->bpp);
    Cvar_SetValue("vid_refreshrate", mode->refresh);

    vid.recalc_refdef = 1;

    return true;
}

int nummodes;
void VID_Init(unsigned char *palette) {
    static qboolean did_init = false;
    qvidmode_t *mode;
    const qvidmode_t *setmode;

    if (did_init) return;

    VID_InitModeCvars();

    Cmd_AddCommand("vid_describemodes", VID_DescribeModes_f);

    /* Init the default windowed mode */
    mode = modelist;
    mode->modenum = 0;
    mode->bpp = 32;
    mode->refresh = 0;
    mode->width = 1280;
    mode->height = 720;
    nummodes = 1;

    /* TODO: read config files first to avoid multiple mode sets */
    setmode = &modelist[0];

    VID_InitGL();


    VID_SetMode(setmode, palette);
    VID_SetPalette(palette);

    //vid_menudrawfn = VID_MenuDraw;
    //vid_menukeyfn = VID_MenuKey;
    did_init = true;
}

void VID_Shutdown(void) {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
	DeinitEGL();
}

void GL_BeginRendering(int *x, int *y, int *width, int *height) {
    *x = *y = 0;
    *width = vid.width;
    *height = vid.height;
}

void GL_EndRendering(void) {
    glFlush();
    eglSwapBuffers(s_display, s_surface);
}
/*
void VID_SetPalette(unsigned char *palette) {
    unsigned i, r, g, b, pixel;

    switch (gl_solid_format) {
    case GL_RGB:
    case GL_RGBA:
        for (i = 0; i < 256; i++) {
            r = palette[0];
            g = palette[1];
            b = palette[2];
            palette += 3;
            pixel = (0xff << 24) | (r << 0) | (g << 8) | (b << 16);
            d_8to24table[i] = LittleLong(pixel);
        }
        break;
    default:
        Sys_Error("%s: unsupported texture format (%d)", __func__, gl_solid_format);
    }
}
*/
void	VID_SetPalette (unsigned char *palette)
{
	byte	*pal;
	unsigned r,g,b;
	unsigned v;
	int     r1,g1,b1;
	int		k;
	unsigned short i;
	unsigned	*table;
	int dist, bestdist;

//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to24table;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;
		
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		*table++ = v;
	}
	d_8to24table[255] &= 0xffffff;	// 255 is transparent

	// JACK: 3D distance calcs - k is last closest, l is the distance.
	for (i=0; i < (1<<15); i++) {
		/* Maps
		000000000000000
		000000000011111 = Red  = 0x1F
		000001111100000 = Blue = 0x03E0
		111110000000000 = Grn  = 0x7C00
		*/
		r = ((i & 0x1F) << 3)+4;
		g = ((i & 0x03E0) >> 2)+4;
		b = ((i & 0x7C00) >> 7)+4;
		pal = (unsigned char *)d_8to24table;
		for (v=0,k=0,bestdist=10000*10000; v<256; v++,pal+=4) {
			r1 = (int)r - (int)pal[0];
			g1 = (int)g - (int)pal[1];
			b1 = (int)b - (int)pal[2];
			dist = (r1*r1)+(g1*g1)+(b1*b1);
			if (dist < bestdist) {
				k=v;
				bestdist = dist;
			}
		}
		d_15to8table[i]=k;
	}
}

void VID_ShiftPalette(unsigned char *palette) {
    /* Done via gl_polyblend instead */
    // VID_SetPalette(palette);
}
