/*
Copyright (C) 2026 NZ:P Team

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
// startup.c -- argument specification via setup.ini
#include "nzportable_def.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
Startup_SetError(char * error, size_t error_size, const char * message)
{
    if (!error || error_size == 0)
        return;

    snprintf(error, error_size, "%s", message);
}

static int
Startup_AddArgument(startup_arguments_t * arguments, char * value,
  char * error, size_t error_size)
{
    if (arguments->argc >= STARTUP_MAX_ARGS) {
        Startup_SetError(error, error_size, "too many startup arguments");
        return 0;
    }

    arguments->argv[arguments->argc++] = value;
    return 1;
}

static int
Startup_ReadFile(startup_arguments_t * arguments, const char * path,
  char * error, size_t error_size)
{
    FILE * file;
    long length;
    size_t bytes_read;

    file = fopen(path, "rb");
    if (!file) {
        if (errno == ENOENT)
            return 1;

        snprintf(error, error_size, "could not open %s: %s", path, strerror(errno));
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
      fseek(file, 0, SEEK_SET) != 0)
    {
        snprintf(error, error_size, "could not determine the size of %s", path);
        fclose(file);
        return 0;
    }

    if ((uintmax_t) length > SIZE_MAX - 1) {
        snprintf(error, error_size, "%s is too large", path);
        fclose(file);
        return 0;
    }

    arguments->file_buffer = malloc((size_t) length + 1);
    if (!arguments->file_buffer) {
        snprintf(error, error_size, "could not allocate memory for %s", path);
        fclose(file);
        return 0;
    }

    bytes_read = fread(arguments->file_buffer, 1, (size_t) length, file);
    fclose(file);
    if (bytes_read != (size_t) length) {
        snprintf(error, error_size, "could not read %s", path);
        free(arguments->file_buffer);
        arguments->file_buffer = NULL;
        return 0;
    }
    arguments->file_buffer[bytes_read] = '\0';
    return 1;
}

int
Startup_LoadArguments(startup_arguments_t * arguments, int argc, char ** argv,
  const char * setup_path, char * error, size_t error_size)
{
    char * cursor;
    int i;

    memset(arguments, 0, sizeof(*arguments));
    if (error && error_size)
        error[0] = '\0';

    if (argc > 0 && argv && argv[0]) {
        if (!Startup_AddArgument(arguments, argv[0], error, error_size))
            return 0;
    } else {
        static char empty_argument[] = "";
        if (!Startup_AddArgument(arguments, empty_argument, error, error_size))
            return 0;
    }

    // Prioritize actual CLI arguments if provided
    for (i = 1; i < argc; ++i) {
        if (argv[i] && !Startup_AddArgument(arguments, argv[i], error, error_size))
            return 0;
    }

    if (!setup_path || !Startup_ReadFile(arguments, setup_path, error, error_size))
        return setup_path == NULL;

    cursor = arguments->file_buffer;
    while (cursor && *cursor) {
        while (*cursor && ((unsigned char) *cursor <= 32 || (unsigned char) *cursor > 126))
            ++cursor;
        if (!*cursor)
            break;
        if (!Startup_AddArgument(arguments, cursor, error, error_size))
            return 0;

        while (*cursor && (unsigned char) *cursor > 32 && (unsigned char) *cursor <= 126)
            ++cursor;
        if (*cursor)
            *cursor++ = '\0';
    }

    return 1;
}

void
Startup_FreeArguments(startup_arguments_t * arguments)
{
    free(arguments->file_buffer);
    memset(arguments, 0, sizeof(*arguments));
}

int
Startup_FindArgument(const startup_arguments_t * arguments, const char * name)
{
    int i;

    for (i = 1; i < arguments->argc; ++i)
        if (arguments->argv[i] && strcmp(arguments->argv[i], name) == 0)
            return i;

    return 0;
}

int
Startup_GetBaseDirectory(const startup_arguments_t * arguments,
  const char * default_directory, const char ** directory, char * error, size_t error_size)
{
    int index = Startup_FindArgument(arguments, "-basedir");

    if (!index)
        index = Startup_FindArgument(arguments, "-gamedir");
    if (!index) {
        *directory = default_directory;
        return 1;
    }
    if (index + 1 >= arguments->argc || arguments->argv[index + 1][0] == '-') {
        Startup_SetError(error, error_size, "-basedir requires a directory");
        return 0;
    }

    *directory = arguments->argv[index + 1];
    return 1;
}

void *
Startup_AllocateHeap(const startup_arguments_t * arguments,
  size_t default_size, size_t * heap_size, char * error, size_t error_size)
{
    int index = Startup_FindArgument(arguments, "-heap");
    size_t requested_size = default_size;
    void * heap;

    if (index) {
        char * end;
        unsigned long megabytes;

        if (index + 1 >= arguments->argc) {
            Startup_SetError(error, error_size, "-heap requires a size in megabytes");
            return NULL;
        }
        errno     = 0;
        megabytes = strtoul(arguments->argv[index + 1], &end, 10);
        if (errno || end == arguments->argv[index + 1] || *end || megabytes == 0 ||
          megabytes > SIZE_MAX / (1024 * 1024) ||
          megabytes * (1024 * 1024) > INT_MAX)
        {
            Startup_SetError(error, error_size, "-heap must be a positive size in megabytes");
            return NULL;
        }
        requested_size = (size_t) megabytes * 1024 * 1024;
    }

    heap = calloc(1, requested_size);
    if (!heap) {
        snprintf(error, error_size, "could not allocate %lu megabytes for the engine heap",
          (unsigned long) (requested_size / (1024 * 1024)));
        return NULL;
    }

    *heap_size = requested_size;
    return heap;
}
