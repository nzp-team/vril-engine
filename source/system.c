/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2023 NZ:P Team

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
// system.c -- Common system functions

#include <stdarg.h>

#include "nzportable_def.h"
#include "_build_info.h"

void Sys_SystemError(char *error);

void Sys_PrintError(const char *function_name, const char *source_file, int line_number, char *message, ...)
{
    // Clear the sound buffer.
	S_ClearBuffer();

	// Put the error message in a buffer.
	va_list args;
	va_start(args, message);
	char buffer[1024];
	memset(buffer, 0, sizeof(buffer));
	vsnprintf(buffer, sizeof(buffer) - 1, message, args);
	va_end(args);

    char *error_message = va(
    "%s\nFile a ticket: https://github.com/nzp-team/nzportable/issues\n\n--- file info ---\nFunction: [%s]\nFile: [%s:%d]\n--- git info ---\nCommit Hash: [%s]\nBranch: [%s]\nBuild Date: [%s]",
    message,
    function_name,
    source_file,
    line_number,
    GIT_HASH,
    GIT_BRANCH,
    BUILD_DATE);

    Con_Printf(error_message);
    Sys_SystemError(error_message); 
}