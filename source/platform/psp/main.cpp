/*
Copyright (C) 2007 Peter Mackay and Chris Swindle.

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

#define CONSOLE_DEBUG 0

#include <stdexcept>
#include <sys/unistd.h>

#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspkernel.h>
#include <pspmoduleinfo.h>
#include <psppower.h>
#include <psprtc.h>
#include <pspctrl.h>
#include <psphprm.h>
#include <pspsdk.h>
#include <pspge.h>
#include <pspsysevent.h>

extern "C"
{
#include "../../nzportable_def.h"
#include "thread.h"
#include "m33libs/kubridge.h"
void VramSetSize(int kb);
int psp_system_model;
bool system_has_right_stick;
}

#include "battery.hpp"
#include "system.hpp"
#include "module.h"

qboolean depthfl = false;

qboolean isDedicated = false;

extern	int  com_argc;
extern	char **com_argv;

#define printf	pspDebugScreenPrintf

// Clock speeds.
int cpuClockSpeed;
int ramClockSpeed;
int busClockSpeed;

#define HEAP_SIZE_SLIM		(30 * 1024 * 1024)
#define HEAP_SIZE_PHAT 		(13 * 1024 * 1024)

namespace quake
{
	namespace main
	{
		// Should the main loop stop running?
		static volatile bool	quit			= false;

		// Is the PSP in suspend mode?
		static volatile bool	suspended		= false;

		static int exitCallback(int arg1, int arg2, void* common)
		{
			// Signal the main thread to stop.
			quit = true;
			return 0;
		}

		static int powerCallback(int unknown, int powerInfo, void* common)
		{
			if (powerInfo & PSP_POWER_CB_POWER_SWITCH || powerInfo & PSP_POWER_CB_SUSPENDING)
			{
				suspended = true;
			}
			else if (powerInfo & PSP_POWER_CB_RESUMING)
			{
			}
			else if (powerInfo & PSP_POWER_CB_RESUME_COMPLETE)
			{
				suspended = false;
			}

			return 0;
		}

		static int callback_thread(SceSize args, void *argp)
		{
			// Register the exit callback.
			const SceUID exitCallbackID = sceKernelCreateCallback("exitCallback", exitCallback, NULL);
			sceKernelRegisterExitCallback(exitCallbackID);

			// Register the power callback.
			const SceUID powerCallbackID = sceKernelCreateCallback("powerCallback", powerCallback, NULL);
			scePowerRegisterCallback(0, powerCallbackID);

			// Sleep and handle callbacks.
			sceKernelSleepThreadCB();
			return 0;
		}

		static int setUpCallbackThread(void)
		{
			const int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, PSP_THREAD_ATTR_USER, 0);
			if (thid >= 0)
				sceKernelStartThread(thid, 0, 0);
			return thid;
		}
//#endif

		static void disableFloatingPointExceptions()
		{
#ifndef _WIN32
			asm(
				".set push\n"
				".set noreorder\n"
				"cfc1    $2, $31\n"		// Get the FP Status Register.
				"lui     $8, 0x80\n"	// (?)
				"and     $8, $2, $8\n"	// Mask off all bits except for 23 of FCR. (? - where does the 0x80 come in?)
				"ctc1    $8, $31\n"		// Set the FP Status Register.
				".set pop\n"
				:						// No inputs.
				:						// No outputs.
				: "$2", "$8"			// Clobbered registers.
				);
#endif
		}
	}
}

using namespace quake;
using namespace quake::main;

int 	user_main(SceSize argc, void* argp);

int ctrl_kernel = 0;
SceUID mod[2];
char mod_names[2][64];

void InitExtModules (void)
{
	char 	currentDirectory[1024];
	char 	gameDirectory[1024];
	char   	path_f[256];

	memset(gameDirectory, 0, sizeof(gameDirectory));
	memset(currentDirectory, 0, sizeof(currentDirectory));
	getcwd(currentDirectory, sizeof(currentDirectory) - 1);

	strcpy(path_f,currentDirectory);
	strcat(path_f,"/hooks/vramext.prx");
	sprintf(mod_names[1], path_f);

	mod[1] = pspSdkLoadStartModule(mod_names[1], PSP_MEMORY_PARTITION_KERNEL);
	if (mod[1] >= 0)
	{
		if (kuKernelGetModel() != 0)
			VramSetSize(4096);
		else
			VramSetSize(2048);
	}
}

void ShutdownExtModules (void)
{
  	if(mod[0] < 0)
	   return;

	sceKernelStopModule(mod[0], 0, 0, 0, 0);
	sceKernelUnloadModule(mod[0]);
}

int main(int argc, char *argv[])
{
#ifdef KERNEL_MODE
	// Load the network modules

    	// create user thread, tweek stack size here if necessary
    	SceUID thid = sceKernelCreateThread("User Mode Thread", user_main,
            0x20, // default priority
            512 * 1024, // stack size (256KB is regular default)
            PSP_THREAD_ATTR_USBWLAN | PSP_THREAD_ATTR_USER  | PSP_THREAD_ATTR_VFPU , NULL);

	// start user thread, then wait for it to do everything else
	sceKernelStartThread(thid, 0, NULL);
	sceKernelWaitThreadEnd(thid, NULL);

	sceKernelExitGame();
}

int user_main(SceSize argc, void* argp)
	{
#endif
	// Set up the callback thread, this is not appropriate for use with
	// the loader, so don't bother calling it as it apparently seems to
	// cause problems with firmware 2.0+
	setUpCallbackThread();

	psp_system_model = Sys_GetPSPModel();
	system_has_right_stick = Sys_HasRightStick();


	// Disable floating point exceptions.
	// If this isn't done, Quake crashes from (presumably) divide by zero
	// operations.
	disableFloatingPointExceptions();

	// Initialize the Common module.
    InitExtModules ();

	ramClockSpeed = cpuClockSpeed = scePowerGetCpuClockFrequencyInt();
	busClockSpeed = scePowerGetBusClockFrequencyInt();

	// Get the current working dir.
	char currentDirectory[1024];
	char gameDirectory[1024];
	const char *baseDirectory;
	startup_arguments_t startup;
	char startupError[256];
	size_t heapSize;
	void *heap;

	memset(gameDirectory, 0, sizeof(gameDirectory));
	memset(currentDirectory, 0, sizeof(currentDirectory));
	getcwd(currentDirectory, sizeof(currentDirectory) - 1);

	char path_f[1024];
	snprintf(path_f, sizeof(path_f), "%s/setup.ini", currentDirectory);
#ifdef KERNEL_MODE
	if (!Startup_LoadArguments(&startup, 0, NULL, path_f,
		startupError, sizeof(startupError)))
#else
	if (!Startup_LoadArguments(&startup, argc, argv, path_f,
		startupError, sizeof(startupError)))
#endif
		Sys_Error("Startup: %s", startupError);

	if (Startup_FindArgument(&startup, "-32depth"))
		depthfl = true;

	if (!Startup_GetBaseDirectory(&startup, currentDirectory, &baseDirectory,
		startupError, sizeof(startupError)))
		Sys_Error("Startup: %s", startupError);
	strncpy(gameDirectory, baseDirectory, sizeof(gameDirectory) - 1);
	gameDirectory[sizeof(gameDirectory) - 1] = '\0';

	heap = Startup_AllocateHeap(&startup,
		psp_system_model == PSP_MODEL_PHAT ? HEAP_SIZE_PHAT : HEAP_SIZE_SLIM,
		&heapSize, startupError, sizeof(startupError));
	if (!heap)
		Sys_Error("Startup: %s", startupError);

#if CONSOLE_DEBUG
	if (startup.argc > 1) {
		startup.argv[startup.argc++] = "-condebug";
		COM_InitArgv(startup.argc, startup.argv);
	}
	else {
		startup.argv[0] = "";
		startup.argv[1] = "-condebug";
		COM_InitArgv(2, startup.argv);
	}
#else
	COM_InitArgv(startup.argc, startup.argv);
#endif

#ifdef PSP_SOFTWARE_VIDEO
	// Bump up the clock frequency.
	if (!COM_CheckParm("-cpu222")) {
		scePowerSetClockFrequency(333, 333, 166);
	}
#else
	if (COM_CheckParm("-cpu333")) {
		scePowerSetClockFrequency(333, 333, 166);
	}
#endif

	// Catch exceptions from here.
	try
	{
		// Initialise the Host module.
		quakeparms_t parms;
		memset(&parms, 0, sizeof(parms));
		parms.argc		= com_argc;
		parms.argv		= com_argv;
		parms.basedir	= gameDirectory;
		parms.memsize	= (int)heapSize;
		parms.membase	= heap;
		Host_Init(&parms);

		// Precalculate the tick rate.
		const float oneOverTickRate = 1.0f / sceRtcGetTickResolution();

		// Record the time that the main loop started.
		u64 lastTicks;
		sceRtcGetCurrentTick(&lastTicks);

		// Enter the main loop.
		while (!quit)
		{

			// Handle suspend & resume.
			if (suspended)
			{
				// Suspend.
				S_ClearBuffer();
				quake::system::suspend();

				// Wait for resume.
				while (suspended && !quit)
				{
					sceDisplayWaitVblankStart();
				}

				// Resume.
				quake::system::resume();

				// Reset the clock.
				sceRtcGetCurrentTick(&lastTicks);
			}

			// What is the time now?
			u64 ticks;
			sceRtcGetCurrentTick(&ticks);

			// How much time has elapsed?
			const unsigned int	deltaTicks		= ticks - lastTicks;
			const float			deltaSeconds	= deltaTicks * oneOverTickRate;

			// Check the battery status.
			battery::check();

			// Run the frame.
			Host_Frame(deltaSeconds);
			// Remember the time for next frame.
			lastTicks = ticks;
		}
	}
	catch (const std::exception& e)
	{
		// Report the error and quit.
		free(heap);
		Startup_FreeArguments(&startup);
		Sys_Error("C++ Exception: %s", e.what());
		return 0;
	}

	// Quit.
	free(heap);
	Startup_FreeArguments(&startup);
	Sys_Quit();
	return 0;
}

int Sys_GetPSPModel(void) 
{
	// PS VITA's pspemu exclussively has this module on its flash0 partition,
	// Credit to DaedalusX64 for this concept.
	int vitaprx = sceIoOpen("flash0:/kd/registry.prx", PSP_O_RDONLY | PSP_O_WRONLY, 0777);
	if (vitaprx >= 0) {
		sceIoClose(vitaprx);
		return PSP_MODEL_PSVITA;
	}

#if 0
	// PPSSPP will return zero (success) for trying to read the emulator: device,
	// Credit to Linblow for this concept.
	int device_result = 0;
	int device_ret = sceIoDevctl("emulator:", 3, &device_result, 0, NULL, 0);
	if (device_result == 0) return PSP_MODEL_EMULATED;
#endif

	// Use kuBridge and trust the Kernel to report the system model.
	int model = kuKernelGetModel();
	switch (model) {
		case 0: return PSP_MODEL_PHAT;
		case 1: return PSP_MODEL_SLIM;
		case 2: return PSP_MODEL_BRITE;
		case 4: return PSP_MODEL_GO;
		case 10: return PSP_MODEL_STREET;
		default: break;
	}

	// Fallback.
	return PSP_MODEL_UNKNOWN;
}

bool Sys_HasRightStick(void)
{
	int psp_system_model = Sys_GetPSPModel();
	if (psp_system_model == PSP_MODEL_PSVITA /*|| psp_system_model == PSP_MODEL_EMULATED*/) {
		return true;
	} else /*if (psp_system_model == PSP_MODEL_GO)*/ {
		SceCtrlData pad;
		sceCtrlPeekBufferPositive(&pad, 1);
		if (pad.Rsrv[0] != 0 && pad.Rsrv[1] != 0) {
			return true;
		}
	}
	return false;
}
