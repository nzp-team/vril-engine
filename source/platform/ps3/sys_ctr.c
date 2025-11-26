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

// PS3 Headers
#include <sys/systime.h>
#include <io/pad.h>
// TODO: gem stuff for PSMove eventually

#include <sys/stat.h>
#include <unistd.h>

#define TICKS_PER_SEC sysGetTimebaseFrequency()

#define QUAKE_HUNK_MB			24 		// cypress -- usable quake hunk size in mB
#define QUAKE_HUNK_MB_NEW3DS	72		// ^^ ditto, but n3ds
// TODO: Decide how big the PS3 hunk should be

#define LINEAR_HEAP_SIZE_MB		16		// cypress -- we lower this as much as possible while still remaining
										// bootable so we can up the quake hunk and actually viable memory.

// Stack size from Vita, no idea if it's right
SYS_PROCESS_PARAM(1001, 0x800000);

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
	Con_Printf ("PS3 NZP v%4.1f (PKG: "__TIME__" "__DATE__")\n", (double)(VERSION));
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

	// TODO: PS3GL has no deinit
	//gfxExit();
	exit(0);
}

double Sys_FloatTime (void)
{
	static u64 initial_tick = 0;

	if(!initial_tick)
		initial_tick = __builtin_ppc_mftb();
	
	u64 current_tick = __builtin_ppc_mftb();

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
	// fancyTODO: Bind PS3
	Cbuf_AddText ("bind ABUTTON +right\n");
	Cbuf_AddText ("bind BBUTTON +lookdown\n");
	Cbuf_AddText ("bind XBUTTON +lookup\n");
	Cbuf_AddText ("bind YBUTTON +left\n");
	Cbuf_AddText ("bind LTRIGGER +jump\n");
	Cbuf_AddText ("bind RTRIGGER +attack\n");
	Cbuf_AddText ("bind PADUP \"impulse 10\"\n");
	Cbuf_AddText ("bind PADDOWN \"impulse 12\"\n");
	//Cbuf_AddText ("lookstrafe \"1.000000\"\n");
	//Cbuf_AddText ("lookspring \"0.000000\"\n");
}

#if 0
void Sys_SetKeys(u32 keys, u32 state){
	if( keys & KEY_SELECT)
		Key_Event(K_SELECT, state);
	if( keys & KEY_START)
		Key_Event(K_ESCAPE, state);
	if( keys & KEY_DUP)
		Key_Event(K_UPARROW, state);
	if( keys & KEY_DDOWN)
		Key_Event(K_DOWNARROW, state);
	if( keys & KEY_DLEFT)
		Key_Event(K_LEFTARROW, state);
	if( keys & KEY_DRIGHT)
		Key_Event(K_RIGHTARROW, state);
	if( keys & KEY_Y)
		Key_Event(K_AUX4, state);
	if( keys & KEY_X)
		Key_Event(K_AUX3, state);
	if( keys & KEY_B)
		Key_Event(K_AUX2, state);
	if( keys & KEY_A)
		Key_Event(K_AUX1, state);
	if( keys & KEY_L)
		Key_Event(K_AUX5, state);
	if( keys & KEY_R)
		Key_Event(K_AUX7, state);
	if( keys & KEY_ZL)
		Key_Event(K_AUX6, state);
	if( keys & KEY_ZR)
		Key_Event(K_AUX8, state);
}
#endif
void Sys_SendKeyEvents (void)
{
#if 0
	padInfo padinfo;
  	padData paddata;
	ioPadGetInfo(&pad_info);
	for (int i = 0; i < MAX_PORT_NUM; i++)
	{
		if (!pad_info.status[i]) continue;
		ioPadGetData(i, &pad_data[i]);


	u32 kDown = hidKeysDown();
	u32 kUp = hidKeysUp();

	if(kDown)
		Sys_SetKeys(kDown, true);
	if(kUp)
		Sys_SetKeys(kUp, false);
#endif
}

void Sys_HighFPPrecision (void)
{
}

void Sys_LowFPPrecision (void)
{
}

void Sys_CaptureScreenshot(void)
{
	Sys_Error("Not implemented!");
}

//=============================================================================

bool game_running;
int main (int argc, char **argv)
{
	static float time, oldtime;
	static quakeparms_t parms;
	//new3ds_flag = false;

	//osSetSpeedupEnable(true);

	//APT_CheckNew3DS(&new3ds_flag);

	//gfxInit(GSP_BGR8_OES, GSP_RGB565_OES, false); 
	//gfxSetDoubleBuffering(GFX_BOTTOM, false);
	//gfxSwapBuffersGpu();

	uint8_t model;

	cfguInit();
	CFGU_GetSystemModel(&model);
	cfguExit();
	
	ioPadInit(1);
	//if(model != CFG_MODEL_2DS && new3ds_flag == true)
	//	gfxSetWide(true);
	
	//chdir("sdmc:/3ds/nzportable");

	//if (new3ds_flag == true)
		parms.memsize = QUAKE_HUNK_MB_NEW3DS * 1024 * 1024;
	//else
	//	parms.memsize = QUAKE_HUNK_MB * 1024 * 1024;
	
	parms.membase = malloc(parms.memsize);
	parms.basedir = ".";

	COM_InitArgv (argc, argv);

	parms.argc = com_argc;
	parms.argv = com_argv;

	Host_Init (&parms);

	oldtime = Sys_FloatTime();

	game_running = true;
	while (/*aptMainLoop() && */game_running)
	{
		time = Sys_FloatTime();
		Host_Frame (time - oldtime);
		oldtime = time;
	}

	return 0;
}
