/*
Quake GameCube port.
Copyright (C) 2007 Peter Mackay
Copyright (C) 2008 Eluan Miranda
Copyright (C) 2015 Fabio Olimpieri

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

// ELUTODO: I use libfat here, move everything to system_libfat.h and use generic calls here

// Handy switches.
#define CONSOLE_DEBUG		0
#define TIME_DEMO			0
#define USE_THREAD			1
#define TEST_CONNECTION		0
#define USBGECKO_DEBUG		0
#define WIFI_DEBUG			0

// OGC includes.
#include <ogc/lwp.h>
#include <ogc/machine/processor.h>
#include <ogc/lwp_watchdog.h>
#include <ogcsys.h>
#include <wiiuse/wpad.h>
#include <wiikeyboard/keyboard.h>
#include <fat.h>
#include "input_wiimote.h"

#if USBGECKO_DEBUG || WIFI_DEBUG
#include <debug.h>
#endif

#include <sys/stat.h>
#include <sys/dir.h>
#include <unistd.h>

#include "../quakedef.h"

extern void Sys_Reset(void);
extern void Sys_Shutdown(void);

// Video globals.
void		*framebuffer[2]		= {NULL, NULL};
u32		fb			= 0;
GXRModeObj	*rmode			= 0;

int want_to_reset = 0;
int want_to_shutdown = 0;
double time_wpad_off = 0;
double current_time = 0;
int rumble_on = 0;

// Set up the heap.
static size_t	heap_size	= 37 * 1024 * 1024;
static char		*heap;
static u32		real_heap_size;
float sys_frame_length;

// ELUTODO: ugly and beyond quake's limits, I think
int parms_number = 0;
char parms[1024];
char *parms_ptr = parms;
char *parms_array[64];

void reset_system(void)
{
	want_to_reset = 1;
}

void shutdown_system(void)
{
	want_to_shutdown = 1;
}

inline void *align32 (void *p)
{
	return (void*)((((int)p + 31)) & 0xffffffe0);
}
void GL_Init (void);
static void init()
{
	// Initialise the video system.
	VIDEO_Init();
	VIDEO_SetBlack(true);
	rmode = VIDEO_GetPreferredMode(NULL);

	// Allocate the frame buffer.
	framebuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	framebuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	
	fb = 0;
	
	// ELUTODO: Add option to adjust the width like the overscan height, can go from 720 to 640
	// Once that is done set viWidth to 704 by default

	// This is safe to always set to 672, the Homebrew Channel does it for example
    rmode->viWidth = 672;
	
	if (rmode == &TVPal576IntDfScale || rmode == &TVPal576ProgScale) 
	{
		rmode->viXOrigin = (VI_MAX_WIDTH_PAL - rmode->viWidth) / 2;
		rmode->viYOrigin = (VI_MAX_HEIGHT_PAL - rmode->viHeight) / 2;
	} 
	else 
	{
		rmode->viXOrigin = (VI_MAX_WIDTH_NTSC - rmode->viWidth) / 2;
		rmode->viYOrigin = (VI_MAX_HEIGHT_NTSC - rmode->viHeight) / 2;
	}
	// Set up the video system with the chosen mode.
	VIDEO_Configure(rmode);

	// Set the frame buffer.
	VIDEO_SetNextFramebuffer(framebuffer[fb]);

	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE)
	{
		VIDEO_WaitVSync();
	}
	
	fb++;

	// Initialise the debug console.
	// ELUTODO: only one framebuffer with it?
	console_init(framebuffer[fb & 1], 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * 2);
	
	GL_Init();

	// Initialise the controller library.
	PAD_Init();

	if(!fatInitDefault())
		printf("Error initializing filesystem\n");
			
	Sys_Init_Logfile();

	if (WPAD_Init() != WPAD_ERR_NONE)
		Sys_Error("WPAD_Init() failed.\n");

	wiimote_ir_res_x = rmode->fbWidth;
	wiimote_ir_res_y = rmode->xfbHeight;
	
	// hope the parms are all set by now
	if (parms_number > 0 ) {
		COM_InitArgv(parms_number, parms_array);
	}

	// Initialise the Host module.
	quakeparms_t parms;
	memset(&parms, 0, sizeof(parms));
	parms.argc		= com_argc;
	parms.argv		= com_argv;
	parms.basedir	= QUAKE_WII_BASEDIR;
	parms.memsize	= real_heap_size;
	parms.membase	= heap;
	if (parms.membase == 0)
	{
		Sys_Error("Heap allocation failed");
	}
	memset(parms.membase, 0, parms.memsize);
	Host_Init(&parms);

#if TEST_CONNECTION
	Cbuf_AddText("connect 192.168.0.2");
#endif

	SYS_SetResetCallback(reset_system);
	SYS_SetPowerCallback(shutdown_system);
}

int cstring_cmp(const void *p1, const void *p2)
{
	return strcmp(*(char * const *)p1, *(char * const *)p2);
}

qboolean isDedicated = false;

void alloc_main_heap (void) {
	u32 level;
	
	_CPU_ISR_Disable(level);
	heap = (char *)align32(SYS_GetArena2Lo());
	real_heap_size = heap_size - ((u32)heap - (u32)SYS_GetArena2Lo());
	if ((u32)heap + real_heap_size > (u32)SYS_GetArena2Hi())
	{
		_CPU_ISR_Restore(level);
		Sys_Error("heap + real_heap_size > (u32)SYS_GetArena2Hi()");
	}	
	else
	{
		SYS_SetArena2Lo(heap + real_heap_size);
		_CPU_ISR_Restore(level);
	}
}

int main(int argc, char* argv[])
{
	// is this safe to do? I'm not sure yet> 
	// could lock out iOS functionality temporarily?
	L2Enhance(); // sB activate 64-byte fetches for the L2 cache

#if USBGECKO_DEBUG
	DEBUG_Init(GDBSTUB_DEVICE_USB, 1); // Slot B
	_break();
#endif

#if WIFI_DEBUG
	printf("Now waiting for WI-FI debugger\n");
	DEBUG_Init(GDBSTUB_DEVICE_WIFI, 8000); // Port 8000 (use whatever you want)
	_break();
#endif

	// Initialize.
	alloc_main_heap ();
	init();
	
	// Run the main loop.
	double current_time, last_time;
	
	last_time = Sys_FloatTime ();
	
	for (;;)
	{
		if (want_to_reset)
			Sys_Reset();
		if (want_to_shutdown)
			Sys_Shutdown();

		// Get the frame time in ticks.
		current_time = Sys_FloatTime ();
		// Run the frame.
		Host_Frame(current_time - last_time);
		last_time = current_time;
		
		if (rumble_on&&(current_time > time_wpad_off)) 
		{
			WPAD_Rumble(0, false);
			rumble_on = 0;
		}
		
		CDAudio_Update();
	};

	exit(0);
	return 0;
}
