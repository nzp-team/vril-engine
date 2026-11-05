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
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <orbis/libkernel.h>
#include <orbis/UserService.h>
#include <orbis/Sysmodule.h>
#include <orbis/_types/errors.h>
#include "ps4_local.h"

int ps4_user = -1;

int
PS4_LoadModule(uint32_t module)
{
    static uint32_t loaded[16];
    static unsigned count;
    unsigned i;
    int result;

    for (i = 0; i < count; ++i)
        if (loaded[i] == module)
            return 0;



    result = (int) sceSysmoduleLoadModuleInternal((enum OrbisSysModuleInternal) module);

    if (result >= 0 && count < sizeof(loaded) / sizeof(loaded[0]))
        loaded[count++] = module;

    return result;
}

int
PS4_GetSetupPath(const char ** path)
{
    int file;

    *path = PS4_SETUP_PATH;

    file = sceKernelOpen(PS4_SETUP_PATH, 0, 0);

    if (file >= 0) {
        sceKernelClose(file);

        return 0;
    }

    if ((uint32_t) file == ORBIS_KERNEL_ERROR_ENOENT || file == -ENOENT) {
        *path = NULL;

        return 0;
    }

    return file;
}

int
PS4_InitUserService(void)
{
    OrbisUserServiceInitializeParams params = { ORBIS_KERNEL_PRIO_FIFO_LOWEST };
    int result;

    result = (int) sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE);

    if (result < 0)
        return result;

    result = sceUserServiceInitialize(&params);

    if (result < 0 && (uint32_t) result != ORBIS_USER_SERVICE_ERROR_ALREADY_INITIALIZED)
        return result;

    result = sceUserServiceGetInitialUser(&ps4_user);

    return result;
}

void
PS4_WriteLog(const char * path, const char * text, size_t length)
{
    int file = sceKernelOpen(path, 1 | 0x200 | 8, 0666);

    if (file < 0)
        return;

    while (length) {
        size_t written = sceKernelWrite(file, text, length);

        if (!written || written > length)
            break;

        text   += written;
        length -= written;
    }

    sceKernelFsync(file);
    sceKernelClose(file);
}

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
    int  handle;
    int  position, length;
    char entries[4096];
};

sys_directory_t *
Sys_OpenDirectory(const char * path)
{
    int handle = sceKernelOpen(path, O_RDONLY | O_DIRECTORY, 0);

    if (handle < 0)
        return NULL;

    sys_directory_t * directory = calloc(1, sizeof(*directory));

    if (!directory) {
        sceKernelClose(handle);

        return NULL;
    }

    directory->handle = handle;

    return directory;
}

int
Sys_ReadDirectory(sys_directory_t * directory, char * name, size_t size)
{
    for (;;) {
        if (directory->position == directory->length) {
            directory->length = sceKernelGetdents(directory->handle, directory->entries,
                sizeof(directory->entries));
            directory->position = 0;

            if (directory->length <= 0)
                return false;
        }

        size_t remaining = directory->length - directory->position;

        if (remaining < offsetof(struct dirent, d_name))
            return false;

        const struct dirent * entry = (const struct dirent *) (directory->entries + directory->position);

        if (entry->d_reclen < offsetof(struct dirent, d_name) + entry->d_namlen + 1 ||
          entry->d_reclen > remaining)
            return false;

        directory->position += entry->d_reclen;

        if (entry->d_namlen && entry->d_namlen < size) {
            memcpy(name, entry->d_name, entry->d_namlen);
            name[entry->d_namlen] = 0;

            return true;
        }
    }
}

void
Sys_CloseDirectory(sys_directory_t * directory)
{
    sceKernelClose(directory->handle);
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
    Con_Printf("Vril Engine PS4 / OpenOrbis\n");
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
    size_t length = strlen(error);

    PS4_WriteLog("/data/nzportable/error.log", error, length);
    fputs(error, stderr);

    IN_PlatformShutdown();
    exit(1);
}

void
Sys_Quit(void)
{
    running = false;
}

double
Sys_FloatTime(void)
{
    static uint64_t start;
    uint64_t now = sceKernelGetProcessTime();

    if (!start)
        start = now;

    return (now - start) / 1000000.0;
}

char *
Sys_ConsoleInput(void)
{
    return NULL;
}

void
Sys_Sleep(void)
{
    sceKernelUsleep(1000);
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

void
Sys_CaptureScreenshot(void)
{
    FILE * file;
    unsigned attempt;

    if (!sys_testmode.value)
        return;

    file = fopen("/data/nzportable/capture.enable", "r");

    if (!file)
        return;

    fclose(file);
    remove("/data/nzportable/capture.done");
    file = fopen("/data/nzportable/capture.request", "w");

    if (!file)
        Sys_Error("Sys_CaptureScreenshot: could not request test screenshot");

    fclose(file);

    for (attempt = 0; attempt < 600; ++attempt) {
        file = fopen("/data/nzportable/capture.done", "r");

        if (file) {
            fclose(file);
            remove("/data/nzportable/capture.request");
            return;
        }

        sceKernelUsleep(100000);
    }

    Sys_Error("Sys_CaptureScreenshot: test screenshot timed out");
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

    mkdir("/data/nzportable", 0777);

    if (chdir("/data/nzportable") < 0)
        Sys_Error("PS4: cannot access /data/nzportable: %s", strerror(errno));

    int user_result = PS4_InitUserService();

    if (user_result < 0)
        Sys_Error("PS4: user-service startup failed (0x%08x)", (unsigned) user_result);

    int setup_result = PS4_GetSetupPath(&setup_path);

    if (setup_result < 0)
        Sys_Error("PS4: cannot inspect %s (0x%08x)", PS4_SETUP_PATH, (unsigned) setup_result);

    if (!Startup_LoadArguments(&startup, argc, argv, setup_path, error, sizeof(error)))
        Sys_Error("Startup: %s", error);

    if (!Startup_GetBaseDirectory(&startup, "/data/nzportable", &basedir, error, sizeof(error)))
        Sys_Error("Startup: %s", error);

    if (!PS4_GraphicsInit())
        Sys_Error("PS4: %s", PS4_GraphicsError());

    parms.membase =
      Startup_AllocateHeap(&startup, 256 * 1024 * 1024, &heap_size, error, sizeof(error));

    if (!parms.membase)
        Sys_Error("Startup: %s", error);

    parms.memsize = (int) heap_size;
    parms.basedir = (char *) basedir;
    COM_InitArgv(startup.argc, startup.argv);
    parms.argc = com_argc;
    parms.argv = com_argv;

    Host_Init(&parms);

    previous = Sys_FloatTime();

    while (running) {
        double now = Sys_FloatTime();

        Host_Frame(now - previous);
        previous = now;
    }

    Host_Shutdown();
    free(parms.membase);
    Startup_FreeArguments(&startup);

    return 0;
} /* main */
