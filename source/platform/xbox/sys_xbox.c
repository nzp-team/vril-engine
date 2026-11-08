/*
 * Copyright (C) 2026 NZ:P Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "../../nzportable_def.h"
#include "xbox_local.h"
#include <windows.h>
#include <hal/debug.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <pbkit/pbkit.h>

#define MAX_HANDLES       32
#define DEFAULT_MEMORY_MB 20

static FILE * sys_handles[MAX_HANDLES];
qboolean isDedicated;

struct sys_directory_s {
    HANDLE          handle;
    WIN32_FIND_DATA entry;
    qboolean        first;
};

static int
Sys_FindHandle(void)
{
    int i;

    for (i = 1; i < MAX_HANDLES; ++i)
        if (!sys_handles[i])
            return i;

    Sys_Error("out of handles");
    return -1;
}

int
Sys_FileOpenRead(char * path, int * handle)
{
    FILE * file = fopen(path, "rb");
    long length;

    *handle = -1;
    if (!file)
        return -1;

    if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET)) {
        fclose(file);
        return -1;
    }
    *handle = Sys_FindHandle();
    sys_handles[*handle] = file;
    return (int) length;
}

int
Sys_FileOpenWrite(char * path)
{
    int handle = Sys_FindHandle();

    sys_handles[handle] = fopen(path, "wb");
    if (!sys_handles[handle])
        Sys_Error("Error opening %s", path);
    return handle;
}

void Sys_FileClose(int handle){ fclose(sys_handles[handle]); sys_handles[handle] = NULL; }

void Sys_FileSeek(int handle, int position){ fseek(sys_handles[handle], position, SEEK_SET); }

int Sys_FileRead(int handle, void * dest, int count){ return (int) fread(dest, 1, count, sys_handles[handle]); }

int Sys_FileWrite(int handle, void * data, int count){ return (int) fwrite(data, 1, count, sys_handles[handle]); }

int
Sys_FileTime(char * path)
{
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    ULARGE_INTEGER time;
    char native[MAX_PATH];

    if (!Sys_XboxNativePath(path, native, sizeof(native)) ||
      !GetFileAttributesEx(native, GetFileExInfoStandard, &attributes))
        return -1;

    time.LowPart  = attributes.ftLastWriteTime.dwLowDateTime;
    time.HighPart = attributes.ftLastWriteTime.dwHighDateTime;
    return (int) (time.QuadPart / 10000000ULL - 11644473600ULL);
}

sys_directory_t *
Sys_OpenDirectory(const char * path)
{
    char pattern[MAX_PATH], native[MAX_PATH];
    sys_directory_t * directory = calloc(1, sizeof(*directory));

    if (!directory)
        return NULL;

    if (!Sys_XboxNativePath(path, native, sizeof(native)) ||
      snprintf(pattern, sizeof(pattern), "%s\\*", native) >= sizeof(pattern))
    {
        free(directory);
        return NULL;
    }
    directory->handle = FindFirstFile(pattern, &directory->entry);
    if (directory->handle == INVALID_HANDLE_VALUE) {
        free(directory);
        return NULL;
    }
    directory->first = true;
    return directory;
}

int
Sys_ReadDirectory(sys_directory_t * directory, char * name, size_t size)
{
    do {
        if (!directory->first && !FindNextFile(directory->handle, &directory->entry))
            return false;

        directory->first = false;
        if (strlen(directory->entry.cFileName) < size) {
            Q_strlcpy(name, directory->entry.cFileName, size);
            return true;
        }
    } while (true);
}

void
Sys_CloseDirectory(sys_directory_t * directory)
{
    FindClose(directory->handle);
    free(directory);
}

void
Sys_mkdir(char * path)
{
    char native[MAX_PATH];

    if (Sys_XboxNativePath(path, native, sizeof(native)))
        CreateDirectory(native, NULL);
}

void Sys_MakeCodeWriteable(unsigned long address, unsigned long length){ (void) address; (void) length; }

void Sys_HighFPPrecision(void){ }

void Sys_LowFPPrecision(void){ }

void Sys_SetFPCW(void){ }

char * Sys_ConsoleInput(void){ return NULL; }

void Sys_Sleep(void){ Sleep(1); }

void Sys_SendKeyEvents(void){ IN_XboxPoll(); }

void
Sys_Quit(void)
{
    // Console quit must not return to a frame that can wait for another flip.
    if (host_initialized)
        Host_Shutdown();
    XLaunchXBE(NULL);
    Sys_SystemError("Could not return to the Xbox dashboard");
}

double
Sys_FloatTime(void)
{
    static ULONGLONG start;
    ULONGLONG now = KeQueryPerformanceCounter();

    if (!start)
        start = now;
    return (double) (now - start) / KeQueryPerformanceFrequency();
}

void Sys_PrintSystemInfo(void){ }

void
Sys_Printf(char * fmt, ...)
{
    char text[2048];
    va_list args;

    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    DbgPrint("%s", text);
}

void
Sys_DebugLog(char * file, char * fmt, ...)
{
    FILE * log = fopen(file, "ab");
    va_list args;

    if (!log)
        return;

    va_start(args, fmt);
    vfprintf(log, fmt, args);
    va_end(args);
    fclose(log);
}

void
Sys_SystemError(char * error)
{
    Sys_Printf("%s\n", error);
    IN_PlatformShutdown();
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    debugPrint("Vril Engine\n\n%s\n", error);
    for (;;)
        Sleep(50);
}

void
Sys_CaptureScreenshot(void)
{
    byte header[54] = { 'B', 'M' };
    uint32_t file_size, offset = 54, dib_size = 40, pixel_size;
    uint16_t planes = 1, bits = 32;
    int width = glwidth, height = glheight, i;
    byte * pixels;
    FILE * file;
    qboolean captured;

    if (vid_headless || width <= 0 || height <= 0)
        return;

    pixel_size = width * height * 4;
    pixels     = malloc(pixel_size);
    if (!pixels)
        return;

    Hyena_SelectReadBuffer(true);
    captured = Hyena_ReadPixels(glx, gly, width, height, pixels);
    Hyena_SelectReadBuffer(false);
    if (!captured) {
        free(pixels);
        return;
    }
    file_size = sizeof(header) + pixel_size;
    memcpy(header + 2, &file_size, 4);
    memcpy(header + 10, &offset, 4);
    memcpy(header + 14, &dib_size, 4);
    memcpy(header + 18, &width, 4);
    memcpy(header + 22, &height, 4);
    memcpy(header + 26, &planes, 2);
    memcpy(header + 28, &bits, 2);
    memcpy(header + 34, &pixel_size, 4);
    for (i = 0; i < width * height; ++i) {
        byte red = pixels[i * 4];
        pixels[i * 4]     = pixels[i * 4 + 2];
        pixels[i * 4 + 2] = red;
    }
    file = fopen("D:/capture.bmp", "wb");
    if (file) {
        fwrite(header, 1, sizeof(header), file);
        fwrite(pixels, 1, pixel_size, file);
        fclose(file);
    }
    free(pixels);
} /* Sys_CaptureScreenshot */

void
Sys_DefaultConfig(void)
{
}

int
main(int argc, char ** argv)
{
    quakeparms_t parms = { 0 };
    startup_arguments_t startup;
    const char * base_directory;
    char error[256];
    size_t heap_size;
    double oldtime;

    // nxdk mounts the directory containing default.xbe as D:, on any launch drive.
    if (!Startup_LoadArguments(&startup, argc, argv, "D:/setup.ini", error, sizeof(error)))
        Sys_SystemError(error);
    if (!Startup_GetBaseDirectory(&startup, "D:/", &base_directory, error, sizeof(error)))
        Sys_SystemError(error);
    parms.membase = Startup_AllocateHeap(&startup, DEFAULT_MEMORY_MB * 1024 * 1024,
        &heap_size, error, sizeof(error));
    if (!parms.membase)
        Sys_SystemError(error);
    parms.memsize = (int) heap_size;
    parms.basedir = (char *) base_directory;
    COM_InitArgv(startup.argc, startup.argv);
    parms.argc = com_argc;
    parms.argv = com_argv;
    Host_Init(&parms);
    oldtime = Sys_FloatTime();
    while (true) {
        double now = Sys_FloatTime();
        Host_Frame(now - oldtime);
        music_update();
        oldtime = now;
    }
} /* main */
