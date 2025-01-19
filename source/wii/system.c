/*
Quake GameCube port.
Copyright (C) 2007 Peter Mackay
Copyright (C) 2008 Eluan Miranda

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

#include <sys/unistd.h>

#include <ogc/gx_struct.h>
#include <ogc/pad.h>
#include <ogc/system.h>
#include <ogc/video.h>
#include <ogc/lwp_watchdog.h>
#include <sys/stat.h>
#include <wiiuse/wpad.h>
#include <errno.h>
#include <fat.h>

#include "../quakedef.h"

FILE *logfile;

void Sys_Error (char *error, ...)
{
	// Clear the sound buffer.
	S_ClearBuffer();

	// Put the error message in a buffer.
	va_list args;
	va_start(args, error);
	char buffer[1024];
	memset(buffer, 0, sizeof(buffer));
	vsnprintf(buffer, sizeof(buffer) - 1, error, args);
	va_end(args);

	// Print the error message to the debug screen.
	printf("\n\n\nThe following error occurred:\n");
	printf("%s\n\n", buffer);

	int i = 0;

	while(i < 60)
	{
		VIDEO_WaitVSync();
		i++;
	};
	printf("Press A to quit.\n");

	// Wait for the user to release the button.
	do
	{
		VIDEO_WaitVSync();
		PAD_ScanPads();
		WPAD_ScanPads();
	}
	while (PAD_ButtonsHeld(0) & PAD_BUTTON_A || WPAD_ButtonsHeld(WPAD_CHAN_0) & WPAD_BUTTON_A);

	// Wait for the user to press the button.
	do
	{
		VIDEO_WaitVSync();
		PAD_ScanPads();
		WPAD_ScanPads();
	}
	while (((PAD_ButtonsHeld(0) & PAD_BUTTON_A) == 0) && ((WPAD_ButtonsHeld(WPAD_CHAN_0) & WPAD_BUTTON_A) == 0));

	printf("Sys_Quit();\n");

	// Quit.
	Sys_Quit();
}

void Sys_Init_Logfile(void)
{

#ifdef LOGFILE	
 logfile= fopen("/apps/nzportable/logfile.txt", "w");
#endif
}

void Sys_Finish_Logfile(void)
{
#ifdef LOGFILE	
 if (logfile) fclose(logfile);
#endif
}

void Sys_Printf (const char *fmt, ...)
{
#ifdef LOGFILE	
	va_list args;
	va_start(args, fmt);
	if (logfile) vfprintf(logfile, fmt, args);
	va_end(args);
#endif
}

void Sys_Quit (void)
{
	Sys_Printf("%s", "Returning to loader...\n");

	// Shut down the host system.
	if (host_initialized > 0)
	{
		Host_Shutdown();
	}

	VIDEO_SetBlack(true);
	
	Sys_Finish_Logfile();

	// Exit.
	exit(0);
}

void Sys_Reset (void)
{
	Sys_Printf("%s", "Resetting...\n");

	// Shut down the host system.
	if (host_initialized > 0)
	{
		Host_Shutdown();
	}

	VIDEO_SetBlack(true);
	
	Sys_Finish_Logfile();

	// Exit.
	//SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
	exit(0);
}

void Sys_Shutdown (void)
{
	Sys_Printf("%s", "Shutting down...\n");

	// Shut down the host system.
	if (host_initialized > 0)
	{
		Host_Shutdown();
	}

	VIDEO_SetBlack(true);
	
	Sys_Finish_Logfile();

	// Exit.
	SYS_ResetSystem(SYS_POWEROFF, 0, 0);
}
//Current time in seconds
//Current time in seconds
double Sys_FloatTime (void)
{
	static u64	base;
	static qboolean	initialized = FALSE;
	u64 ms;

	ms = ticks_to_millisecs(gettime());
	if (!initialized)
	{
		base = ms;
		initialized = TRUE;
	}
	return ((double)(ms - base)) / 1000.0;
}

char *Sys_ConsoleInput (void)
{
	return 0;
}

void Sys_SendKeyEvents (void)
{
}

void Sys_LowFPPrecision (void)
{
}

void Sys_HighFPPrecision (void)
{
}

/*
===============================================================================

FILE IO

===============================================================================
*/

#define MAX_HANDLES             16
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

int Sys_FileOpenRead (char *path, int *hndl)
{
	FILE    *f;
	int             i;
	
	struct stat file_info;

	int res = stat(path, &file_info);
	if (res != 0)
	{
		return -1;
	}

	i = findhandle ();

	f = fopen(path, "rb");
	if (f <= 0)
	{
		*hndl = -1;
		return -1;
	}
	sys_handles[i] = f;
	*hndl = i;

	return file_info.st_size;
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

void Sys_FileSeekCur (int handle, int offset)
{
	fseek (sys_handles[handle], offset, SEEK_CUR);
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
}
