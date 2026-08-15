/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2025 NZ:P Team

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
// test_handler.c -- Handler for various engine tests kicked off during Pull Requests, used by headless emulators.

#include <unistd.h>
#include "nzportable_def.h"

#define NZP_TESTMODE_MAPBOOT    1   // Captures a screenshot after the client spawns, then exits the application.
#define NZP_TESTMODE_RESTART    2   // Restarts the current server 100 times, five seconds apart.
#define NZP_RESTART_TEST_COUNT  100
#define NZP_RESTART_TEST_DELAY  5.0

// cypress: sys_testmode will enable several validation features used for PR testing.
cvar_t 	sys_testmode = {"sys_testmode", "0"};

/*
====================
TestHandler_MapBoot
Invoked shortly into server time, takes a screenshot as soon
as the server spawns and immediately quits the application after.
====================
*/
void TestHandler_MapBoot(void)
{
    if (sys_testmode.value != NZP_TESTMODE_MAPBOOT)
        return;

    // Capture a screenshot
    Sys_CaptureScreenshot();
    Sys_Quit();
}

/*
====================
TestHandler_RestartStress

Exercises the same-map restart path repeatedly at five-second intervals.
====================
*/
void TestHandler_RestartStress(void)
{
	static int restart_count;
	static double next_restart_time;

	if (sys_testmode.value != NZP_TESTMODE_RESTART)
	{
		restart_count = 0;
		next_restart_time = 0;
		return;
	}
	if (!sv.active)
		return;
	if (!next_restart_time)
	{
		next_restart_time = sv.time + NZP_RESTART_TEST_DELAY;
		return;
	}
	if (sv.time < next_restart_time)
		return;

	restart_count++;
	Con_Printf("Restart stress test: %i/%i\n", restart_count, NZP_RESTART_TEST_COUNT);
	SV_RestartServer();
	next_restart_time = sv.time + NZP_RESTART_TEST_DELAY;

	if (restart_count == NZP_RESTART_TEST_COUNT)
	{
		Con_Printf("Restart stress test passed after %i restarts.\n", restart_count);
		Sys_Quit();
	}
}

/*
====================
TestHandler_Init
====================
*/
void TestHandler_Init(void)
{
    Cvar_RegisterVariable(&sys_testmode);
}
