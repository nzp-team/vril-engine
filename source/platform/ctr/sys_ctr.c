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

#include "../../nzportable_def.h"
#include "errno.h"
#include "touch_ctr.h"
#include "circle_pad_pro.h"

#include <3ds.h>
#include <GL/picaGL.h>
#include <sys/stat.h>
#include <unistd.h>

#define TICKS_PER_SEC 268123480.0

// this is more than enough for the hunk
#define QUAKE_HUNK_MB			20	 	// cypress -- usable quake hunk size in mB
#define QUAKE_HUNK_MB_NEW3DS	64		// ^^ ditto, but n3ds

// we don't need a very big stack. this seems to work fine
u32 __stacksize__ = 256*1024;

// linear heap is where PicaGL stores 
// textures, unless able to store in VRAM
u32 __ctru_linear_heap_size = 20 * 1024 * 1024;

bool new3ds_flag;
bool circlepadpro_flag;

extern void Touch_Init();
extern void Touch_Update();

qboolean isDedicated;

/*
===============================================================================

FILE IO

===============================================================================
*/

#define MAX_HANDLES             10
FILE    *sys_handles[MAX_HANDLES];

int             findhandle (void)
{
	int             i;
	
	for (i=1 ; i<MAX_HANDLES ; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error ("out of handles");
	return -1;
}

/*
================
filelength
================
*/
int filelength (FILE *f)
{
	int             pos;
	int             end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenRead (char *path, int *hndl)
{
	FILE    *f;
	int             i;
	
	i = findhandle ();

	f = fopen(path, "rb");
	if (!f)
	{
		*hndl = -1;
		return -1;
	}
	sys_handles[i] = f;
	*hndl = i;
	
	return filelength(f);
}

int Sys_FileOpenWrite (char *path)
{
	FILE    *f;
	int             i;
	
	i = findhandle ();

	f = fopen(path, "wb");
	if (!f)
		Sys_Error ("Error opening %s: %s", path,strerror(errno));
	sys_handles[i] = f;
	
	return i;
}

void Sys_FileClose (int handle)
{
	fclose (sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void Sys_FileSeek (int handle, int position)
{
	fseek (sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
	return fread (dest, 1, count, sys_handles[handle]);
}

int Sys_FileWrite (int handle, void *data, int count)
{
	return fwrite (data, 1, count, sys_handles[handle]);
}

int     Sys_FileTime (char *path)
{
	FILE    *f;
	
	f = fopen(path, "rb");
	if (f)
	{
		fclose(f);
		return 1;
	}
	
	return -1;
}

void Sys_mkdir (char *path)
{
	mkdir(path, 0777);
}

void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
}

void Sys_PrintSystemInfo(void)
{
	Con_Printf ("3DS NZP v%4.1f (3DSX: "__TIME__" "__DATE__")\n", (double)(VERSION));

	if (new3ds_flag)
		Con_Printf ("3DS Model: NEW Nintendo 3DS\n");
	else
		Con_Printf ("3DS Model: Nintendo 3DS\n");
}

void Sys_SystemError(char *error)
{
	consoleInit(GFX_TOP, NULL);

	printf("%s=== Vril Engine Exception ===\n", CONSOLE_RED);
	printf("%s%s\n\n", CONSOLE_WHITE, error);
	
	printf("%sPress START to quit.\n", CONSOLE_CYAN);

	while(!(hidKeysDown() & KEY_START))
		hidScanInput();

	Host_Shutdown();

	gfxExit();
	Sys_Quit();
}

void Sys_Printf (char *fmt, ...)
{
	va_list         argptr;
	
	va_start (argptr,fmt);
	vprintf (fmt,argptr);
	va_end (argptr);
}

void Sys_Quit (void)
{
	Host_Shutdown();

	gfxExit();
	exit(0);
}

double Sys_FloatTime (void)
{
	static u64 initial_tick = 0;

	if(!initial_tick)
		initial_tick = svcGetSystemTick();
	
	u64 current_tick = svcGetSystemTick();

	return (current_tick - initial_tick)/TICKS_PER_SEC;
}

char *Sys_ConsoleInput (void)
{
	return NULL;
}

void Sys_Sleep (void)
{
}

void Sys_DefaultConfig(void)
{
	// naievil -- fixme I didn't do this
	Cbuf_AddText ("bind ABUTTON +moveright\n");
	Cbuf_AddText ("bind BBUTTON +back\n");
	Cbuf_AddText ("bind XBUTTON +forward\n");
	Cbuf_AddText ("bind YBUTTON ++moveleft\n");
	Cbuf_AddText ("bind LTRIGGER +aim\n");
	Cbuf_AddText ("bind RTRIGGER +attack\n");
	Cbuf_AddText ("bind UPARROW \"impulse 10\"\n");
	Cbuf_AddText ("bind DOWNARROW \"impulse 12\"\n");
	//Cbuf_AddText ("lookstrafe \"1.000000\"\n");
	//Cbuf_AddText ("lookspring \"0.000000\"\n");
}

void Sys_SetKeys(u32 keys, u32 state){
	if( keys & KEY_SELECT)
		Key_Event(K_SELECT, state);
	if( keys & KEY_START)
		Key_Event(K_START, state);
	if( keys & KEY_DUP)
		Key_Event(K_UPARROW, state);
	if( keys & KEY_DDOWN)
		Key_Event(K_DOWNARROW, state);
	if( keys & KEY_DLEFT)
		Key_Event(K_LEFTARROW, state);
	if( keys & KEY_DRIGHT)
		Key_Event(K_RIGHTARROW, state);
	if( keys & KEY_Y)
		Key_Event(K_LEFTFACE, state);
	if( keys & KEY_X)
		Key_Event(K_TOPFACE, state);
	if( keys & KEY_B)
		Key_Event(K_BOTTOMFACE, state);
	if( keys & KEY_A)
		Key_Event(K_RIGHTFACE, state);
	if( keys & KEY_L)
		Key_Event(K_LTRIGGER, state);
	if( keys & KEY_R)
		Key_Event(K_RTRIGGER, state);
	if( keys & KEY_ZL)
		Key_Event(K_ZLTRIGGER, state);
	if( keys & KEY_ZR)
		Key_Event(K_ZRTRIGGER, state);
}

void Sys_SendKeyEvents (void)
{
	hidScanInput();
	
	u32 kDown = hidKeysDown();
	u32 kUp = hidKeysUp();
	if(circlepadpro_flag){
		kDown |= cppKeysDown();
		kUp |= cppKeysUp();
	}
	if(kDown)
		Sys_SetKeys(kDown, true);
	if(kUp)
		Sys_SetKeys(kUp, false);
	Touch_Update();
}

void Sys_HighFPPrecision (void)
{
}

void Sys_LowFPPrecision (void)
{
}

void Sys_CaptureScreenshot(void)
{
	FILE *file;
	byte *pixels;
	byte *framebuffer;
	byte header[54] = {0};
	int width = vid.width * (gfxIsWide() ? 2 : 1);
	int height = vid.height;
	int row_size = (width * 3 + 3) & ~3;
	int image_size = row_size * height;
	int x, y;
	const char *filename = new3ds_flag ? "capture-new.bmp" : "capture-old.bmp";

	if (width <= 0 || height <= 0)
		Sys_Error("Could not capture screenshot before video initialization");

	pixels = calloc(1, image_size);
	if (!pixels)
		Sys_Error("Could not allocate screenshot buffer");

	// picaGL does not implement glReadPixels. Transfer the rendered top screen
	// to its BGR framebuffer, then wait before reading the rotated columns.
	glFinish();
	framebuffer = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	pglSwapBuffersEx(1, 0);
	glFinish();
	GSPGPU_InvalidateDataCache(framebuffer, width * height * 3);
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			memcpy(pixels + y * row_size + x * 3,
				framebuffer + (x * height + y) * 3, 3);

	file = fopen(filename, "wb");
	if (!file)
	{
		free(pixels);
		Sys_Error("Could not open %s: %s", filename, strerror(errno));
	}

	header[0] = 'B';
	header[1] = 'M';
	header[2] = (byte)((54 + image_size) & 0xff);
	header[3] = (byte)((54 + image_size) >> 8);
	header[4] = (byte)((54 + image_size) >> 16);
	header[5] = (byte)((54 + image_size) >> 24);
	header[10] = 54;
	header[14] = 40;
	header[18] = (byte)(width & 0xff);
	header[19] = (byte)(width >> 8);
	header[22] = (byte)(height & 0xff);
	header[23] = (byte)(height >> 8);
	header[26] = 1;
	header[28] = 24;
	header[34] = (byte)(image_size & 0xff);
	header[35] = (byte)(image_size >> 8);
	header[36] = (byte)(image_size >> 16);
	header[37] = (byte)(image_size >> 24);
	if (fwrite(header, 1, sizeof(header), file) != sizeof(header) ||
		fwrite(pixels, 1, image_size, file) != (size_t)image_size)
	{
		fclose(file);
		remove(filename);
		free(pixels);
		Sys_Error("Could not write screenshot %s", filename);
	}

	if (fclose(file))
	{
		remove(filename);
		free(pixels);
		Sys_Error("Could not close screenshot %s", filename);
	}
	free(pixels);
	Con_Printf("Wrote %s\n", filename);
}

//=============================================================================

bool game_running;
int main (int argc, char **argv)
{
	static float time, oldtime;
	static quakeparms_t parms;
	startup_arguments_t startup;
	const char *base_directory;
	char startup_error[256];
	int testmode_parm;
	size_t heap_size;
	new3ds_flag = false;

	osSetSpeedupEnable(true);

	APT_CheckNew3DS(&new3ds_flag);

	gfxInit(GSP_BGR8_OES, GSP_RGB565_OES, false); 
	gfxSetDoubleBuffering(GFX_BOTTOM, false);
	gfxSwapBuffersGpu();

	uint8_t model;

	cfguInit();
	CFGU_GetSystemModel(&model);
	cfguExit();
	
	chdir("sdmc:/3ds/nzportable");

	if (!Startup_LoadArguments(&startup, argc, argv, "setup.ini", startup_error, sizeof(startup_error)))
		Sys_Error("Startup: %s", startup_error);

	if (!Startup_GetBaseDirectory(&startup, ".", &base_directory, startup_error, sizeof(startup_error)))
		Sys_Error("Startup: %s", startup_error);

	parms.membase = Startup_AllocateHeap(&startup,
		(new3ds_flag ? QUAKE_HUNK_MB_NEW3DS : QUAKE_HUNK_MB) * 1024 * 1024,
		&heap_size, startup_error, sizeof(startup_error));
	if (!parms.membase)
		Sys_Error("Startup: %s", startup_error);
	parms.memsize = (int)heap_size;
	parms.basedir = (char *)base_directory;

	COM_InitArgv(startup.argc, startup.argv);

	// Select the test resolution before picaGL allocates its render targets.
	testmode_parm = COM_CheckParm("+sys_testmode");
	if (testmode_parm && testmode_parm + 1 < com_argc &&
		Q_atof(com_argv[testmode_parm + 1]) > 0)
		gfxSetWide(false);
	else
		gfxSetWide(model != CFG_MODEL_2DS && new3ds_flag);

	parms.argc = com_argc;
	parms.argv = com_argv;

	if(!new3ds_flag){
		Result res = cppInit();
		if (R_FAILED(res)) {
			cppExit();
		}
	}
	Host_Init (&parms);
	Touch_Init();
	Touch_DrawOverlay();

	oldtime = Sys_FloatTime();

	game_running = true;
	while (aptMainLoop() && game_running)
	{
		time = Sys_FloatTime();
		Host_Frame (time - oldtime);
		oldtime = time;
	}

	if (circlepadpro_flag)
		cppExit();
	if (host_initialized)
		Host_Shutdown();

	free(parms.membase);
	Startup_FreeArguments(&startup);

	return 0;
}
