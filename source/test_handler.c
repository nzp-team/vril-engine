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
TestHandler_Init
====================
*/
void TestHandler_Init(void)
{
    Cvar_RegisterVariable(&sys_testmode);
}