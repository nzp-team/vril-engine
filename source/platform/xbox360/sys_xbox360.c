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
#include "xbox360_local.h"
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ppc/timebase.h>
#include <time/time.h>
#include <usb/usbmain.h>
#include <xenon_soc/xenon_power.h>
#include <xenon_smc/xenon_smc.h>
#include <libfat/fat.h>
#include <diskio/ata.h>
#include <xenon_soc/xenon_io.h>
extern DISC_INTERFACE xenon_ata_ops;

#define MAX_HANDLES 32
static FILE * sys_handles[MAX_HANDLES];
static FILE * sys_log;
extern int16_t snd_scaletable[32][256];
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
    int total = 0;

    while (total < count) {
        int bytes = min(count - total, 64 * 1024);
        int result = (int) fread((byte *) dest + total, 1, bytes, sys_handles[handle]);

        total += result;
        X360_AudioUpdate();
        if (result != bytes)
            break;
    }
    return total;
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
    DIR * handle;
};

sys_directory_t *
Sys_OpenDirectory(const char * path)
{
    DIR * handle = opendir(path);
    sys_directory_t * directory;

    if (!handle)
        return NULL;
    directory = malloc(sizeof(*directory));
    if (!directory) {
        closedir(handle);
        return NULL;
    }
    directory->handle = handle;
    return directory;
}

int
Sys_ReadDirectory(sys_directory_t * directory, char * name, size_t size)
{
    struct dirent * entry;

    while ((entry = readdir(directory->handle))) {
        if (strlen(entry->d_name) < size) {
            Q_strlcpy(name, entry->d_name, size);
            return true;
        }
    }
    return false;
}

void
Sys_CloseDirectory(sys_directory_t * directory)
{
    closedir(directory->handle);
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
    Con_Printf("Vril Engine Xbox 360 / libxenon\n");
}

void
Sys_Printf(char * fmt, ...)
{
    va_list arguments, copy;

    va_start(arguments, fmt);
    if (sys_log) {
        va_copy(copy, arguments);
        vfprintf(sys_log, fmt, copy);
        va_end(copy);
        if (fflush(sys_log) != 0 || fsync(fileno(sys_log)) != 0) {
            int error = errno;

            fclose(sys_log);
            sys_log = NULL;
            Sys_Error("Xbox 360: cannot save xbox360.log: %s", strerror(error));
        }
    }
    vprintf(fmt, arguments);
    va_end(arguments);
}

void
Sys_SystemError(char * error)
{
    FILE * file = fopen("error.log", "wb");

    if (file) {
        fputs(error, file);
        fclose(file);
    }
    X360_GraphicsShowError(error);
    IN_PlatformShutdown();
    for (;;) {
        usb_do_poll();
        udelay(1000);
    }
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
    uint32_t high = 0, low = 0, check = 0;
    uint64_t now;

    do {
        __asm__ volatile ("mftbu %0" : "=r" (high));
        __asm__ volatile ("mftb %0" : "=r" (low));
        __asm__ volatile ("mftbu %0" : "=r" (check));
    } while (high != check);
    now = ((uint64_t) high << 32) | low;

    if (!start)
        start = now;
    return (double) (now - start) / PPC_TIMEBASE_FREQ;
}

char *
Sys_ConsoleInput(void)
{
    return NULL;
}

void
Sys_Sleep(void)
{
    usb_do_poll();
    X360_NetworkPoll();
    udelay(1000);
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
}

static qboolean
Sys_TimebaseRunning(void)
{
    uint64_t start = mftb();

    for (unsigned attempt = 0; attempt < 1024; ++attempt)
        if (mftb() != start)
            return true;
    return false;
}

static qboolean
Sys_MountStorage(qboolean initialized)
{
    unsigned attempt;

    if (!initialized) {
        std((void *) 0x200611A0, 0x100);
        *(volatile uint32_t *) 0xD0110010 = __builtin_bswap32(0xEA001300);
        *(volatile uint32_t *) 0xD0110014 = __builtin_bswap32(0xEA001320);
        xenon_ata_init();
        xenon_ata_ops.features &= ~FEATURE_MEDIUM_CANWRITE;
        return fatMountSimple("uda", &xenon_ata_ops);
    }

    xenon_make_it_faster(XENON_SPEED_FULL);
    usb_init();
    for (attempt = 0; attempt < 100; ++attempt) {
        usb_do_poll();
        if (fatInitDefault())
            return true;
        mdelay(100);
    }
    return false;
}

int
main(int argc, char ** argv)
{
    quakeparms_t parms = { 0 };
    startup_arguments_t startup;
    const char * basedir;
    const char * setup_path = NULL;
    FILE * setup;
    size_t heap_size;
    char error[256];
    double previous;

    qboolean boot_initialized = Sys_TimebaseRunning();

    if (!Sys_MountStorage(boot_initialized))
        Sys_Error("Xbox 360: cannot mount FAT storage");
    if (chdir("uda:/nzportable") < 0)
        Sys_Error("Xbox 360: cannot access uda:/nzportable");
    if (boot_initialized) {
        sys_log = fopen("xbox360.log", "w");
        if (!sys_log)
            Sys_Error("Xbox 360: cannot open xbox360.log: %s", strerror(errno));
    }
    Sys_Printf("Xbox 360 diagnostic log, built %s %s\n", __DATE__, __TIME__);
    setup = fopen("setup.ini", "rb");
    if (setup) {
        fclose(setup);
        setup_path = "setup.ini";
    }
    if (!Startup_LoadArguments(&startup, argc, argv, setup_path, error, sizeof(error)))
        Sys_Error("Startup: %s", error);
    if (!Startup_GetBaseDirectory(&startup, "uda:/nzportable", &basedir, error, sizeof(error)))
        Sys_Error("Startup: %s", error);
    COM_InitArgv(startup.argc, startup.argv);
    if (!COM_CheckParm("-dedicated") && !TestHandler_ArgumentsAllowHeadless(startup.argc, startup.argv) && !X360_GraphicsInit())
        Sys_Error("Xbox 360: %s", X360_GraphicsError());
    parms.membase = Startup_AllocateHeap(&startup, 192 * 1024 * 1024, &heap_size, error, sizeof(error));
    if (!parms.membase)
        Sys_Error("Startup: %s", error);
    parms.memsize = (int) heap_size;
    parms.basedir = (char *) basedir;
    parms.argc = com_argc;
    parms.argv = com_argv;
    Host_Init(&parms);
    if (shm) {
        for (int volume = 0; volume < 32; ++volume)
            for (int sample = 0; sample < 256; ++sample)
                if (snd_scaletable[volume][sample] != (sample < 128 ? sample : sample - 256) * volume * 8)
                    Sys_Error("Xbox 360: sound mixer table failed at %d,%d", volume, sample);
        Sys_Printf("Xbox 360: all 8192 sound mixer table entries verified\n");
    }
    Cvar_SetValue("cl_maxfps", 60);
    Cbuf_AddText("cl_maxfps 60\n");
    previous = Sys_FloatTime();
    while (running) {
        double now = Sys_FloatTime();

        X360_NetworkPoll();
        X360_AudioUpdate();
        Host_Frame(now - previous);
        previous = now;
    }
    Host_Shutdown();
    free(parms.membase);
    Startup_FreeArguments(&startup);
    if (sys_log)
        fclose(sys_log);
    if (boot_initialized) {
        usb_shutdown();
        xenon_smc_power_reboot();
    }
    return 0;
}
