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
#include "ps3_local.h"
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <sys/systime.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>
SYS_PROCESS_PARAM(1001, 0x100000);

#define MAX_HANDLES 32
static FILE * sys_handles[MAX_HANDLES];
qboolean isDedicated;
static qboolean running = true;

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

static int
Sys_FileLength(FILE * file)
{
    long position = ftell(file);
    long length;

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, position, SEEK_SET);

    return (int) length;
}

int
Sys_FileOpenRead(char * path, int * handle)
{
    FILE * file = fopen(path, "rb");
    int index;

    if (!file) {
        *handle = -1;

        return -1;
    }

    index = Sys_FindHandle();
    sys_handles[index] = file;
    *handle = index;

    return Sys_FileLength(file);
}

int
Sys_FileOpenWrite(char * path)
{
    FILE * file = fopen(path, "wb");
    int index;

    if (!file)
        Sys_Error("Error opening %s: %s", path, strerror(errno));

    index = Sys_FindHandle();
    sys_handles[index] = file;

    return index;
}

void
Sys_FileClose(int handle)
{
    fclose(sys_handles[handle]);
    sys_handles[handle] = NULL;
}

void
Sys_FileSeek(int handle, int position)
{
    fseek(sys_handles[handle], position, SEEK_SET);
}

int
Sys_FileRead(int handle, void * dest, int count)
{
    return (int) fread(dest, 1, count, sys_handles[handle]);
}

int
Sys_FileWrite(int handle, void * data, int count)
{
    return (int) fwrite(data, 1, count, sys_handles[handle]);
}

int
Sys_FileTime(char * path)
{
    #if defined(_WIN32)
    struct _stat st;

    return _stat(path, &st) == 0 ? (int) st.st_mtime : -1;

    #else
    struct stat st;

    return stat(path, &st) == 0 ? (int) st.st_mtime : -1;

    #endif
}

struct sys_directory_s {
    s32 handle;
};

sys_directory_t *
Sys_OpenDirectory(const char * path)
{
    s32 handle;

    if (sysLv2FsOpenDir(path, &handle) != 0)
        return NULL;

    sys_directory_t * directory = malloc(sizeof(*directory));

    if (!directory) {
        sysLv2FsCloseDir(handle);
        return NULL;
    }

    directory->handle = handle;
    return directory;
}

int
Sys_ReadDirectory(sys_directory_t * directory, char * name, size_t size)
{
    sysFSDirent entry;
    u64 count;

    while (sysLv2FsReadDir(directory->handle, &entry, &count) == 0 && count) {
        if (entry.d_namlen >= size)
            continue;

        memcpy(name, entry.d_name, entry.d_namlen);
        name[entry.d_namlen] = 0;
        return true;
    }

    return false;
}

void
Sys_CloseDirectory(sys_directory_t * directory)
{
    sysLv2FsCloseDir(directory->handle);
    free(directory);
}

void
Sys_mkdir(char * path)
{
    #if defined(_WIN32)
    _mkdir(path);
    #else
    mkdir(path, 0777);
    #endif
}

void
Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length)
{
    (void) startaddr;
    (void) length;
}

void
Sys_PrintSystemInfo(void)
{
    Con_Printf("Vril Engine PS3 / PSL1GHT\n");
}

void
Sys_Printf(char * fmt, ...)
{
    va_list arguments;

    va_start(arguments, fmt);
    vprintf(fmt, arguments);
    va_end(arguments);
}

void
Sys_SystemError(char * error)
{
    fputs(error, stderr);
    FILE * log = fopen(PS3_BASEDIR "/error.log", "a");

    if (log) {
        fputs(error, log);
        fclose(log);
    }

    PS3_GraphicsBeginExit();
    IN_PlatformShutdown();
    exit(1);
}

void
Sys_Quit(void)
{
    running = false;
    PS3_GraphicsBeginExit();
}

double
Sys_FloatTime(void)
{
    static uint64_t start;
    static double tick_seconds;

    if (!tick_seconds)
        tick_seconds = 1.0 / (double) sysGetTimebaseFrequency();

    uint64_t now;
    __asm__ volatile ("mftb %0" : "=r" (now));

    if (!start)
        start = now;

    return (double) (now - start) * tick_seconds;
}

char *
Sys_ConsoleInput(void)
{
    return NULL;
}

void
Sys_Sleep(void)
{
    sysUsleep(1000);
}

void
Sys_HighFPPrecision(void)
{
}

void
Sys_LowFPPrecision(void)
{
}

void
Sys_SetFPCW(void)
{
}

void
Sys_DebugLog(char * file, char * fmt, ...)
{
    (void) file;
    (void) fmt;
}

void
Sys_DefaultConfig(void)
{
}

static void
PS3_BitmapField(byte * header, unsigned offset, unsigned value)
{
    for (unsigned i = 0; i < 4; ++i)
        header[offset + i] = (byte) (value >> (i * 8));
}

void
Sys_CaptureScreenshot(void)
{
    byte header[54] = { 'B', 'M' };
    unsigned width = vid.width, height = vid.height;
    size_t bytes = (size_t) width * height * 4;
    char path[MAX_OSPATH];
    byte * pixels;
    FILE * file;

    if (vid_headless || !width || !height)
        return;

    pixels = malloc(bytes);

    if (!pixels)
        Sys_Error("PS3: screenshot allocation failed");

    Hyena_SelectReadBuffer(true);
    qboolean captured = Hyena_ReadPixels(0, 0, width, height, pixels);
    Hyena_SelectReadBuffer(false);

    if (!captured) {
        free(pixels);
        Sys_Error("PS3: screenshot readback failed");
    }

    PS3_BitmapField(header, 2, sizeof(header) + bytes);
    PS3_BitmapField(header, 10, sizeof(header));
    PS3_BitmapField(header, 14, 40);
    PS3_BitmapField(header, 18, width);
    PS3_BitmapField(header, 22, height);
    header[26] = 1;
    header[28] = 32;
    PS3_BitmapField(header, 34, bytes);

    for (size_t i = 0; i < bytes; i += 4) {
        byte red = pixels[i];
        pixels[i]     = pixels[i + 2];
        pixels[i + 2] = red;
    }

    snprintf(path, sizeof(path), "%s/capture.bmp", host_parms.basedir);
    file = fopen(path, "wb");

    if (!file) {
        free(pixels);
        Sys_Error("PS3: could not open screenshot");
    }

    int success = fwrite(header, 1, sizeof(header), file) == sizeof(header);
    success = (fwrite(pixels, 1, bytes, file) == bytes) && success;
    success = (fclose(file) == 0) && success;
    free(pixels);

    if (!success) {
        remove(path);
        Sys_Error("PS3: screenshot write failed");
    }
}

static void
ExitCallback(u64 status, u64 parameter, void * data)
{
    (void) parameter;
    (void) data;

    if (status == SYSUTIL_EXIT_GAME) {
        Con_SafePrintf("PS3: XMB exit requested\n");
        Sys_Quit();
    }
}

int
main(int argc, char ** argv)
{
    quakeparms_t parms = { 0 };
    startup_arguments_t startup;
    const char * basedir;
    const char * setup_path;
    size_t heap_size;
    char error[256];
    double previous;

    if (mkdir(PS3_BASEDIR, 0777) < 0 && errno != EEXIST)
        Sys_Error("PS3: cannot create %s: %s", PS3_BASEDIR, strerror(errno));

    struct stat base_stat;

    if (stat(PS3_BASEDIR, &base_stat) < 0)
        Sys_Error("PS3: cannot stat %s: %s", PS3_BASEDIR, strerror(errno));

    if (!S_ISDIR(base_stat.st_mode))
        Sys_Error("PS3: data path is not a directory: %s", PS3_BASEDIR);

    setup_path = access(PS3_BASEDIR "/setup.ini", R_OK) == 0 ? PS3_BASEDIR "/setup.ini" : NULL;
    sysUtilRegisterCallback(0, ExitCallback, NULL);

    if (!Startup_LoadArguments(&startup, argc, argv, setup_path, error, sizeof(error)))
        Sys_Error("Startup: %s", error);

    if (!Startup_GetBaseDirectory(&startup, PS3_BASEDIR, &basedir, error, sizeof(error)))
        Sys_Error("Startup: %s", error);

    parms.membase =
      Startup_AllocateHeap(&startup, 128 * 1024 * 1024, &heap_size, error, sizeof(error));

    if (!parms.membase)
        Sys_Error("Startup: %s", error);

    parms.memsize = (int) heap_size;
    parms.basedir = (char *) basedir;
    COM_InitArgv(startup.argc, startup.argv);
    parms.argc = com_argc;
    parms.argv = com_argv;

    Host_Init(&parms);
    Cvar_SetValue("cl_maxfps", 60);
    Cbuf_AddText("cl_maxfps 60\n");

    previous = Sys_FloatTime();

    while (running) {
        sysUtilCheckCallback();

        if (!running)
            break;

        double now = Sys_FloatTime();

        Host_Frame(now - previous);
        previous = now;
    }

    Con_SafePrintf("PS3: shutdown begin\n");
    Host_Shutdown();
    sysUtilUnregisterCallback(0);
    Con_SafePrintf("PS3: shutdown complete; returning to system\n");
    fflush(NULL);
    free(parms.membase);
    Startup_FreeArguments(&startup);

    return 0;
}
