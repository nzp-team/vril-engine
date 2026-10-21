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
//#include "touch_ctr.h"
//#include "circle_pad_pro.h"

#include <switch.h>
#include <sys/stat.h>
#include <unistd.h>

//#define TICKS_PER_SEC 268123480.0

// this is more than enough for the hunk
#define QUAKE_HUNK_MB	128


qboolean isDedicated;
PadState pad;


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
	Con_Printf ("NX NZP v%4.1f (NRO: "__TIME__" "__DATE__")\n", (double)(VERSION));
}

void Sys_SystemError(char *error)
{
    char text[4096];
    int page = 0;
    qboolean more;
    // EGL owns the native window during gameplay. Release it before the
    // software console creates its framebuffer, and reset the game's crop.
    VID_Shutdown();
    PrintConsole *console = consoleInit(NULL);
    nwindowSetCrop(nwindowGetDefault(), 0, 0,
        console->consoleWidth * 16, console->consoleHeight * 16);
    // Errors can occur before the normal input setup has completed.
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    padUpdate(&pad);

    do
    {
        more = Sys_ErrorPage(error, page, 78, 35, text, sizeof(text));
        consoleClear();
        printf("%s=== Vril Engine Exception (page %d) ===\n", CONSOLE_RED, page + 1);
        printf("%s%s\n", CONSOLE_WHITE, text);
        printf("%sLEFT/RIGHT: pages   PLUS: quit\n", CONSOLE_CYAN);
        consoleUpdate(NULL);
        while (appletMainLoop())
        {
            padUpdate(&pad);
            u64 buttons = padGetButtonsDown(&pad);
            if (buttons & HidNpadButton_Plus)
                goto done;
            if ((buttons & HidNpadButton_Left) && page > 0)
            {
                page--;
                break;
            }
            if ((buttons & HidNpadButton_Right) && more)
            {
                page++;
                break;
            }
            consoleUpdate(NULL);
        }
        if (!appletMainLoop())
            break;
    } while (1);

done:
    consoleExit(NULL);
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

	//gfxExit();
	exit(0);
}

double Sys_FloatTime (void)
{
	static u64 initial_tick = 0;

	if(!initial_tick)
		initial_tick = armGetSystemTick();
	
	u64 current_tick = armGetSystemTick();

	return (double)(current_tick - initial_tick)/armGetSystemTickFreq();
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

void Sys_SetKeys(u64 keys, qboolean state){
	if( keys & HidNpadButton_Minus)
		Key_Event(K_SELECT, state);
	if( keys & HidNpadButton_Plus)
		Key_Event(K_START, state);
	if( keys & HidNpadButton_Up)
		Key_Event(K_UPARROW, state);
	if( keys & HidNpadButton_Down)
		Key_Event(K_DOWNARROW, state);
	if( keys & HidNpadButton_Left)
		Key_Event(K_LEFTARROW, state);
	if( keys & HidNpadButton_Right)
		Key_Event(K_RIGHTARROW, state);
	if( keys & HidNpadButton_Y)
		Key_Event(K_LEFTFACE, state);
	if( keys & HidNpadButton_X)
		Key_Event(K_TOPFACE, state);
	if( keys & HidNpadButton_B)
		Key_Event(K_BOTTOMFACE, state);
	if( keys & HidNpadButton_A)
		Key_Event(K_RIGHTFACE, state);
	if( keys & HidNpadButton_L)
		Key_Event(K_LTRIGGER, state);
	if( keys & HidNpadButton_R)
		Key_Event(K_RTRIGGER, state);
	if( keys & HidNpadButton_ZL)
		Key_Event(K_ZLTRIGGER, state);
	if( keys & HidNpadButton_ZR)
		Key_Event(K_ZRTRIGGER, state);
	if( keys & HidNpadButton_StickL)
		Key_Event(K_LTHUMB, state);
	if( keys & HidNpadButton_StickR)
		Key_Event(K_RTHUMB, state);
}

void Sys_SendKeyEvents (void)
{
	padUpdate(&pad);

	u64 kDown = padGetButtonsDown(&pad);
	u64 kUp = padGetButtonsUp(&pad);

	if(kDown)
		Sys_SetKeys(kDown, true);
	if(kUp)
		Sys_SetKeys(kUp, false);
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

	padConfigureInput(1, HidNpadStyleSet_NpadStandard);
	padInitializeDefault(&pad);
	
	chdir("/switch/nzportable/");

	parms.memsize = QUAKE_HUNK_MB * 1024 * 1024;
	
	parms.membase = malloc(parms.memsize);
	parms.basedir = "/switch/nzportable/";

	COM_InitArgv (argc, argv);

	parms.argc = com_argc;
	parms.argv = com_argv;

	Host_Init (&parms);

	oldtime = Sys_FloatTime();

	game_running = true;
	while (appletMainLoop() && game_running)
	{
		time = Sys_FloatTime();
		Host_Frame (time - oldtime);
		music_update();
		oldtime = time;
	}
	return 0;
}
