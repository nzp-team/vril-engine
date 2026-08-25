/*
Copyright (C) 2025 NZ:P Team.

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

#include "../nzportable_def.h"

cvar_t sys_testmode = {"sys_testmode", "0"};

static const test_suite_t test_suites[] =
{
	{1, "map-boot", true, Test_MapBoot_Start, Test_MapBoot_Frame},
	{2, "restart-stress", true, Test_Restart_Start, Test_Restart_Frame},
	{3, "qcvm-opcodes", false, Test_QCVM_Start, NULL}
};

static const test_suite_t *active_suite;
static qboolean suite_started;

static const test_suite_t *TestHandler_FindSuite(int mode)
{
	int i;

	for (i = 0; i < (int)(sizeof(test_suites) / sizeof(test_suites[0])); i++)
		if (test_suites[i].mode == mode)
			return &test_suites[i];
	return NULL;
}

qboolean TestHandler_ArgumentsAllowHeadless(int argc, char **argv)
{
	const test_suite_t *suite;
	int i;

	for (i = 1; i + 1 < argc; i++)
	{
		if (strcmp(argv[i], "+sys_testmode"))
			continue;
		suite = TestHandler_FindSuite(Q_atoi(argv[i + 1]));
		return suite && !suite->requires_server;
	}
	return false;
}

static void TestHandler_Finish(test_status_t status)
{
	if (status == TEST_STATUS_RUNNING)
		return;
	if (status == TEST_STATUS_PASSED)
	{
		Con_Printf("Test suite passed: %s\n", active_suite->name);
		Sys_Quit();
		return;
	}
	Sys_Error("Test suite failed: %s", active_suite->name);
}

void TestHandler_Frame(void)
{
	int requested_mode;
	test_status_t status;

	if (!active_suite)
	{
		requested_mode = (int)sys_testmode.value;
		if (!requested_mode)
			return;
		active_suite = TestHandler_FindSuite(requested_mode);
		if (!active_suite)
			Sys_Error("Unknown test mode: %i", requested_mode);
	}

	if (!suite_started)
	{
		suite_started = true;
		Con_Printf("Starting test suite: %s\n", active_suite->name);
		status = active_suite->start ? active_suite->start() : TEST_STATUS_RUNNING;
		TestHandler_Finish(status);
	}

	if (active_suite->frame)
		TestHandler_Finish(active_suite->frame());
}

qboolean TestHandler_Init(void)
{
	int parm;
	int requested_mode;

	Cvar_RegisterVariable(&sys_testmode);

	// Early startup hack for some asset-less testing
	parm = COM_CheckParm("+sys_testmode");
	if (!parm || parm + 1 >= com_argc)
		return false;

	requested_mode = Q_atoi(com_argv[parm + 1]);
	active_suite = TestHandler_FindSuite(requested_mode);
	if (!active_suite)
		Sys_Error("Unknown test mode: %i", requested_mode);
	if (!active_suite->requires_server)
	{
		TestHandler_Frame();
		return true;
	}
	return false;
}
