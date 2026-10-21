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

static qboolean sys_capturing_error;
static char sys_error_details[8192];
static size_t sys_error_details_length;
static qboolean sys_error_details_truncated;

void Sys_BeginErrorReport(void)
{
    sys_capturing_error = true;
    sys_error_details[0] = 0;
    sys_error_details_length = 0;
    sys_error_details_truncated = false;
}

qboolean Sys_CaptureErrorOutput(const char *text)
{
    size_t length, available;
    if (!sys_capturing_error)
        return false;

    length = strlen(text);
    available = sizeof(sys_error_details) - 1 - sys_error_details_length;
    if (length > available)
    {
        length = available;
        sys_error_details_truncated = true;
    }
    memcpy(sys_error_details + sys_error_details_length, text, length);
    sys_error_details_length += length;
    sys_error_details[sys_error_details_length] = 0;
    return true;
}

// Wrap at a fixed width without allocating or relying on the game renderer.
// Callers reserve room for their own title and navigation controls.
qboolean Sys_ErrorPage(const char *error, int page, int columns, int rows,
    char *text, size_t size)
{
    int current_page = 0, row = 0;
    size_t used = 0;
    if (!size)
        return false;
    text[0] = 0;
    if (page < 0 || columns < 1 || rows < 1)
        return false;

    while (*error)
    {
        int column = 0;
        while (*error && *error != '\n' && column < columns)
        {
            unsigned char c = *error++;
            if (c == '\r')
                continue;
            if (current_page == page && used + 1 < size)
                text[used++] = c >= 32 && c < 127 ? c : ' ';
            column++;
        }
        if (*error == '\n')
            error++;
        if (current_page == page && used + 1 < size)
            text[used++] = '\n';
        if (++row == rows)
        {
            if (current_page == page)
                break;
            current_page++;
            row = 0;
        }
    }
    text[used] = 0;
    return *error != 0;
}

void Sys_PrintError(const char *function_name, const char *source_file, int line_number, char *message, ...)
{
    scr_disabled_for_loading = true;

    // Clear the sound buffer.
	S_ClearBuffer();

	// Put the error message in a buffer.
	va_list args;
	va_start(args, message);
	char buffer[1024];
	memset(buffer, 0, sizeof(buffer));
	vsnprintf(buffer, sizeof(buffer) - 1, message, args);
	va_end(args);

    // Keep the complete report out of va() and the 1 KB Host_Error buffer.
    static char error_message[12288];
    sys_capturing_error = false;
    snprintf(error_message, sizeof(error_message),
        "%s\n%s%s%s\nFile a ticket: https://github.com/nzp-team/nzportable/issues\n"
        "\n--- file info ---\nFunction: [%s]\nFile: [%s:%d]\n"
        "--- git info ---\nCommit Hash: [%s]\nBranch: [%s]\nBuild Date: [%s]",
        buffer,
        sys_error_details_length ? "\n--- QCVM diagnostics ---\n" : "",
        sys_error_details,
        sys_error_details_truncated ? "\n[diagnostics truncated]\n" : "",
        function_name, source_file, line_number, GIT_HASH, GIT_BRANCH, BUILD_DATE);

    // Con_Printf has a smaller formatting buffer; emit the report in chunks.
    for (size_t offset = 0; offset < strlen(error_message); offset += 512)
        Con_Printf("%.*s", 512, error_message + offset);
    Sys_SystemError(error_message);
    abort();
}
