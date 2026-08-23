/*
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
// tests/test_handler.h

extern cvar_t sys_testmode;

typedef enum
{
	TEST_STATUS_RUNNING,
	TEST_STATUS_PASSED,
	TEST_STATUS_FAILED
} test_status_t;

typedef struct
{
	int mode;
	const char *name;
	qboolean requires_server;
	test_status_t (*start)(void);
	test_status_t (*frame)(void);
} test_suite_t;

test_status_t Test_QCVM_Start(void);
test_status_t Test_MapBoot_Start(void);
test_status_t Test_MapBoot_Frame(void);
test_status_t Test_Restart_Start(void);
test_status_t Test_Restart_Frame(void);

void TestHandler_Frame(void);
qboolean TestHandler_ArgumentsAllowHeadless(int argc, char **argv);
qboolean TestHandler_Init(void);
