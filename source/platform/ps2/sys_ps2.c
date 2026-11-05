/*
Copyright (C) 1996-1997 Id Software, Inc.

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

#include "../../nzportable_def.h"
#include <errno.h>
#include <limits.h>
#include <malloc.h>
#include <stdint.h>
#include <sys/stat.h>
#include <dirent.h>
#include <kernel.h>
#include <ee_regs.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <timer.h>
#include <debug.h>
#include <ee_debug.h>
#include <unistd.h>
#include <sbv_patches.h>
#include <smod.h>
#include <iopcontrol.h>
#define NEWLIB_PORT_AWARE
#include <io_common.h>
#include <fileXio_rpc.h>
#include <usbhdfsd-common.h>
#include <hdd-ioctl.h>

static char sys_data_directory[MAX_QPATH] = "mass:/APPS/NZPORTABLE";
static char sys_hdd_partition[APA_IDMAX + 6];
typedef enum {
	PS2_STORAGE_USB,
	PS2_STORAGE_MX4SIO,
	PS2_STORAGE_MMCE,
	PS2_STORAGE_MEMORY_CARD,
	PS2_STORAGE_HDD,
	PS2_STORAGE_ATA
} sys_ps2_storage_t;
static qboolean sys_boot_screen;
static char sys_last_file[MAX_OSPATH];
static unsigned int sys_sound_opens;

qboolean isDedicated = false;
int tex_cache;
unsigned char proto_idx;
cvar_t vid_vsync = {"vid_vsync", "1", true};

extern unsigned char iomanX_irx[];
extern unsigned int size_iomanX_irx;
extern unsigned char fileXio_irx[];
extern unsigned int size_fileXio_irx;
extern unsigned char bdm_irx[];
extern unsigned int size_bdm_irx;
extern unsigned char bdmfs_fatfs_irx[];
extern unsigned int size_bdmfs_fatfs_irx;
extern unsigned char usbd_irx[];
extern unsigned int size_usbd_irx;
extern unsigned char usbmass_bd_irx[];
extern unsigned int size_usbmass_bd_irx;
extern unsigned char mx4sio_bd_irx[];
extern unsigned int size_mx4sio_bd_irx;
extern unsigned char mmceman_irx[];
extern unsigned int size_mmceman_irx;
extern unsigned char ps2dev9_irx[];
extern unsigned int size_ps2dev9_irx;
extern unsigned char ata_bd_irx[];
extern unsigned int size_ata_bd_irx;
extern unsigned char ps2hdd_irx[];
extern unsigned int size_ps2hdd_irx;
extern unsigned char ps2fs_irx[];
extern unsigned int size_ps2fs_irx;
extern unsigned char sio2man_irx[];
extern unsigned int size_sio2man_irx;
extern unsigned char padman_irx[];
extern unsigned int size_padman_irx;
extern unsigned char audsrv_irx[];
extern unsigned int size_audsrv_irx;
extern void IN_PS2Poll(void);

#define MAX_HANDLES             10
FILE    *sys_handles[MAX_HANDLES];

int Sys_FindHandle(void)
{
	int i;

	for (i=1;i<MAX_HANDLES;i++)
		if (!sys_handles[i]) return i;
	Sys_Error("out of handles");
	return -1;
}

int Sys_FileLength(FILE *f)
{
	struct stat info;

	if (fstat(fileno(f), &info) < 0 || info.st_size < 0 || info.st_size > INT_MAX)
		return -1;
	return (int)info.st_size;
}

int Sys_FileOpenRead(char *path, int *hndl)
{
	FILE    *f;
	int             i, length;

	i = Sys_FindHandle();

	f = fopen(path, "rb");
	if (!f)
	{
		*hndl = -1;
		return -1;
	}
	length = Sys_FileLength(f);
	if (length < 0) {
		fclose(f);
		*hndl = -1;
		return -1;
	}
	sys_handles[i] = f;
	Q_strlcpy(sys_last_file, path, sizeof(sys_last_file));
	if (strstr(path, "/sounds/"))
		sys_sound_opens++;
	*hndl = i;

	return length;
}

int Sys_FileOpenWrite(char *path)
{
	FILE    *f;
	int             i;

	i = Sys_FindHandle();

	f = fopen(path, "wb");
	if (!f)
		Sys_Error("Error opening %s: %s", path, strerror(errno));
	sys_handles[i] = f;

	return i;
}

void Sys_FileClose(int handle)
{
	fclose(sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void Sys_FileSeek(int handle, int position)
{
	fseek(sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead(int handle, void *dest, int count)
{
	return fread(dest, 1, count, sys_handles[handle]);
}

int Sys_FileWrite(int handle, void *data, int count)
{
	return fwrite(data, 1, count, sys_handles[handle]);
}

int     Sys_FileTime(char *path)
{
	struct stat info;

	return stat(path, &info) == 0 ? 1 : -1;
}

sys_directory_t *Sys_OpenDirectory(const char *path)
{
	return (sys_directory_t *)opendir(path);
}

int Sys_ReadDirectory(sys_directory_t *directory, char *name, size_t size)
{
	struct dirent *entry;
	while ((entry = readdir((DIR *)directory)) != NULL) {
		if (strlen(entry->d_name) < size) {
			Q_strlcpy(name, entry->d_name, size);
			return true;
		}
	}
	return false;
}

void Sys_CloseDirectory(sys_directory_t *directory)
{
	closedir((DIR *)directory);
}

void Sys_mkdir(char *path)
{
	mkdir(path, 0777);
}








/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length)
{
}

void Sys_Quit(void)
{
	Host_Shutdown();
	exit(0);
}

void Sys_PrintSystemInfo(void)
{
	Con_Printf("PS2 NZP v%4.1f (ELF: "__TIME__" "__DATE__")\n", (float)VERSION);
}

void Sys_SystemError(char *error)
{
	init_scr();
	sys_boot_screen = true;
	scr_printf("%s\n", error);
	scr_printf("Last file: %s\n", sys_last_file);
	for (;;)
		SleepThread();
}

void Sys_Printf(char *fmt, ...)
{
	va_list argptr;

	if (sys_boot_screen) {
		va_start(argptr, fmt);
		scr_vprintf(fmt, argptr);
		va_end(argptr);
	}
}

void Sys_PS2GPUError(const char *stage)
{
	Sys_Error("GPU timeout: %s\nmap %s\n"
		"VIF1 STAT %08x CODE %08x MARK %08x\n"
		"VIF1 ERR %08x CYCLE %08x NUM %08x\n"
		"D1 CHCR %08x TADR %08x MADR %08x QWC %08x\n"
		"D2 CHCR %08x GIF STAT %08x\nGS CSR %08x\n"
		"view %.3f %.3f %.3f angles %.3f %.3f %.3f\nLast file: %s",
		stage, cl.worldmodel ? cl.worldmodel->name : "none",
		*R_EE_VIF1_STAT, *R_EE_VIF1_CODE, *R_EE_VIF1_MARK,
		*R_EE_VIF1_ERR, *R_EE_VIF1_CYCLE, *R_EE_VIF1_NUM,
		*R_EE_D1_CHCR, *R_EE_D1_TADR, *R_EE_D1_MADR, *R_EE_D1_QWC,
		*R_EE_D2_CHCR, *R_EE_GIF_STAT, (unsigned)*R_EE_GS_CSR,
		r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2],
		r_refdef.viewangles[0], r_refdef.viewangles[1], r_refdef.viewangles[2],
		sys_last_file);
}

void Sys_PS2BeginVideo(void)
{
	sys_boot_screen = false;
}

char *Sys_ConsoleInput(void)
{
	return NULL;
}

void Sys_Sleep(void)
{
	usleep(1000);
}

double Sys_FloatTime(void)
{
	static u64 initial_tick;
	u64 tick = GetTimerSystemTime();

	if (!initial_tick)
		initial_tick = tick;
	return (double)(tick - initial_tick) * (1.0 / kBUSCLK);
}

void Sys_HighFPPrecision(void)
{
}

void Sys_LowFPPrecision(void)
{
}

void Sys_SendKeyEvents(void)
{
	IN_PS2Poll();
}

void Sys_CaptureScreenshot(void)
{
    byte header[54] = {'B', 'M'}, *row;
    char path[MAX_OSPATH];
    unsigned int size = 54 + vid.width * vid.height * 4, value;
    FILE *file;
    int x, y;
    row = malloc(vid.width * 4);
    if (!row) return;
    snprintf(path, sizeof(path), "%s/capture.bmp", sys_data_directory);
    file = fopen(path, "wb");
    if (!file) { free(row); return; }
    memcpy(header + 2, &size, 4);
    value = 54; memcpy(header + 10, &value, 4);
    value = 40; memcpy(header + 14, &value, 4);
    value = vid.width; memcpy(header + 18, &value, 4);
    value = vid.height; memcpy(header + 22, &value, 4);
    header[26] = 1;
    header[28] = 32;
    fwrite(header, 1, sizeof(header), file);
    for (y = 0; y < vid.height; y++) {
        if (!Hyena_ReadPixels(0, y, vid.width, 1, row)) break;
        for (x = 0; x < vid.width; x++) {
            byte red = row[x * 4];
            row[x * 4] = row[x * 4 + 2];
            row[x * 4 + 2] = red;
        }
        if (fwrite(row, 4, vid.width, file) != vid.width) break;
    }
    fclose(file);
    free(row);
    if (y != vid.height) {
        remove(path);
        Con_Printf("Could not capture the framebuffer.\n");
    }
}

static void Sys_LoadModule(char *path)
{
	int id;

	id = SifLoadModule(path, 0, NULL);
	if (id < 0)
		Sys_SystemError(va("Could not load %s (%d)", path, id));
}

static void Sys_LoadModuleBuffer(char *name, void *buffer, int size)
{
	int id, result = -1;

	id = SifExecModuleBuffer(buffer, size, 0, NULL, &result);
	if (id < 0 || result < 0 || result == 1)
		Sys_SystemError(va("Could not load %s (id %d, result %d)", name, id, result));
}

static sys_ps2_storage_t Sys_PS2BootStorage(int argc, char **argv, int *unit)
{
	smod_mod_info_t module;
	char driver[16] = {0};
	int fd, length;
	const char *partition;
	sys_ps2_storage_t storage;

	*unit = 0;
	sys_hdd_partition[0] = 0;
	if (argc < 1 || !argv || !argv[0])
		return PS2_STORAGE_USB;
	if (!strncmp(argv[0], "hdd0:", 5) || !strncmp(argv[0], "hdd1:", 5)) {
		*unit = argv[0][3] - '0';
		partition = argv[0] + 5;
		if (*partition == '/')
			partition++;
		length = strcspn(partition, ":/");
		if (length > APA_IDMAX)
			Sys_SystemError("HDD partition name is too long");
		if (length)
			snprintf(sys_hdd_partition, sizeof(sys_hdd_partition),
				"%.5s%.*s", argv[0], length, partition);
		return PS2_STORAGE_HDD;
	}
	if (!strncmp(argv[0], "pfs", 3))
		return PS2_STORAGE_HDD;
	if (!strncmp(argv[0], "ata", 3))
		return PS2_STORAGE_ATA;
	if (!strncmp(argv[0], "mc0:", 4) || !strncmp(argv[0], "mc1:", 4)) {
		*unit = argv[0][2] - '0';
		return PS2_STORAGE_MEMORY_CARD;
	}
	if (!strncmp(argv[0], "mmce0:", 6) || !strncmp(argv[0], "mmce1:", 6) ||
		!strncmp(argv[0], "mmce:", 5)) {
		*unit = argv[0][4] == '1';
		return PS2_STORAGE_MMCE;
	}
	if (!strncmp(argv[0], "mx4sio:", 7))
		return PS2_STORAGE_MX4SIO;
	if (strncmp(argv[0], "mass", 4))
		return PS2_STORAGE_USB;
	if (!smod_get_mod_by_name("IOX/File_Manager_Rpc", &module))
		return smod_get_mod_by_name("mx4sio", &module) &&
			!smod_get_mod_by_name("usbmass_bd", &module) ?
			PS2_STORAGE_MX4SIO : PS2_STORAGE_USB;
	if (fileXioInit() < 0)
		return PS2_STORAGE_USB;
	fd = fileXioOpen(argv[0], FIO_O_RDONLY, 0);
	if (fd >= 0) {
		fileXioIoctl2(fd, USBMASS_IOCTL_GET_DRIVERNAME, NULL, 0,
			driver, sizeof(driver) - 1);
		fileXioClose(fd);
	}
	if (!strcmp(driver, "sdc"))
		storage = PS2_STORAGE_MX4SIO;
	else if (!strcmp(driver, "ata"))
		storage = PS2_STORAGE_ATA;
	else
		storage = PS2_STORAGE_USB;
	fileXioExit();
	return storage;
}

static qboolean Sys_PS2MountHDD(const char *partition)
{
	DIR *dir;

	if (fileXioMount("pfs0:", partition, FIO_MT_RDWR) < 0)
		return false;
	dir = opendir("pfs0:/APPS/NZPORTABLE/nzp");
	if (!dir) {
		fileXioUmount("pfs0:");
		return false;
	}
	closedir(dir);
	Q_strlcpy(sys_data_directory, "pfs0:/APPS/NZPORTABLE", sizeof(sys_data_directory));
	return true;
}

static void Sys_PS2InitHDD(int unit)
{
	iox_dirent_t entry;
	char device[] = "hdd0:", partition[APA_IDMAX + 6];
	int fd;
	qboolean mounted = false;

	Sys_LoadModuleBuffer("ps2hdd.irx", ps2hdd_irx, size_ps2hdd_irx);
	Sys_LoadModuleBuffer("ps2fs.irx", ps2fs_irx, size_ps2fs_irx);
	if (fileXioInit() < 0)
		Sys_SystemError("Could not initialize HDD file access");
	if (sys_hdd_partition[0]) {
		if (!Sys_PS2MountHDD(sys_hdd_partition))
			Sys_SystemError(va("Could not load APPS/NZPORTABLE/nzp from %s", sys_hdd_partition));
		return;
	}
	device[3] += unit;
	fd = fileXioDopen(device);
	if (fd >= 0) {
		while (fileXioDread(fd, &entry) > 0) {
			if (entry.stat.mode != APA_TYPE_PFS || (entry.stat.attr & APA_FLAG_SUB))
				continue;
			snprintf(partition, sizeof(partition), "%s%.*s", device, APA_IDMAX, entry.name);
			if ((mounted = Sys_PS2MountHDD(partition)))
				break;
		}
		fileXioDclose(fd);
	}
	if (!mounted)
		Sys_SystemError("HDD game data not found: APPS/NZPORTABLE/nzp");
}

static void Sys_PS2InitStorage(sys_ps2_storage_t storage, int unit)
{
	DIR *dir;
	char root[16];
	int i, slot;

	Sys_LoadModuleBuffer("iomanX.irx", iomanX_irx, size_iomanX_irx);
	Sys_LoadModuleBuffer("fileXio.irx", fileXio_irx, size_fileXio_irx);
	if (storage == PS2_STORAGE_MMCE || storage == PS2_STORAGE_MEMORY_CARD) {
		Sys_LoadModuleBuffer("mmceman.irx", mmceman_irx, size_mmceman_irx);
		if (fileXioInit() < 0)
			Sys_SystemError("Could not initialize storage file access");
		for (i = 0; i < 100; i++) {
			for (slot = 0; slot < 2; slot++) {
				snprintf(root, sizeof(root), "mmce%d:/", unit ^ slot);
				dir = opendir(root);
				if (dir) {
					closedir(dir);
					snprintf(sys_data_directory, sizeof(sys_data_directory),
						"%sAPPS/NZPORTABLE", root);
					return;
				}
			}
			if (storage == PS2_STORAGE_MEMORY_CARD)
				break;
			usleep(100000);
		}
		if (storage == PS2_STORAGE_MMCE)
			Sys_SystemError("MMCE SD card not accessible (check firmware and card)");
	}
	Sys_LoadModuleBuffer("bdm.irx", bdm_irx, size_bdm_irx);
	Sys_LoadModuleBuffer("bdmfs_fatfs.irx", bdmfs_fatfs_irx, size_bdmfs_fatfs_irx);
	if (storage == PS2_STORAGE_HDD || storage == PS2_STORAGE_ATA) {
		Sys_LoadModuleBuffer("ps2dev9.irx", ps2dev9_irx, size_ps2dev9_irx);
		Sys_LoadModuleBuffer("ata_bd.irx", ata_bd_irx, size_ata_bd_irx);
		if (storage == PS2_STORAGE_HDD) {
			Sys_PS2InitHDD(unit);
			return;
		}
	} else if (storage == PS2_STORAGE_MX4SIO) {
		Sys_LoadModuleBuffer("mx4sio_bd.irx", mx4sio_bd_irx, size_mx4sio_bd_irx);
	} else {
		Sys_LoadModuleBuffer("usbd.irx", usbd_irx, size_usbd_irx);
		Sys_LoadModuleBuffer("usbmass_bd.irx", usbmass_bd_irx, size_usbmass_bd_irx);
	}
	if (fileXioInit() < 0)
		Sys_SystemError("Could not initialize storage file access");
	for (i = 0; i < 100; i++) {
		dir = opendir("mass:/");
		if (dir) {
			closedir(dir);
			break;
		}
		usleep(100000);
	}
	if (i == 100)
		Sys_SystemError(storage == PS2_STORAGE_ATA ? "HDD did not mount" :
			storage == PS2_STORAGE_MX4SIO ? "MX4SIO card did not mount" : "USB drive did not mount");
}

extern int hunk_size, hunk_low_used, hunk_high_used;
extern void Hyena_PS2PrintMemory(void);
extern size_t Hyena_AliasCacheSize(void);

static size_t Sys_PS2HeapAvailable(void)
{
	struct mallinfo info = mallinfo();
	uintptr_t current = (uintptr_t)sbrk(0), limit = (uintptr_t)EndOfHeap();
	return info.fordblks + (limit > current ? limit - current : 0);
}

void Sys_PS2Memory_f(void)
{
	struct mallinfo info = mallinfo();

	Con_Printf("PS2 hunk: %d KB total, %d KB low, %d KB high, %d KB for cache\n",
		hunk_size / 1024, hunk_low_used / 1024, hunk_high_used / 1024,
		(hunk_size - hunk_low_used - hunk_high_used) / 1024);
	Con_Printf("PS2 malloc: %u KB allocated, %u KB free, %u KB available, break %p, limit %p\n",
		(unsigned)info.uordblks / 1024, (unsigned)info.fordblks / 1024,
		(unsigned)Sys_PS2HeapAvailable() / 1024,
		sbrk(0), EndOfHeap());
	Con_Printf("PS2 sound file opens: %u\n", sys_sound_opens);
	Hyena_PS2PrintMemory();
	Con_Printf("PS2 alias cache: %u KB\n", (unsigned)Hyena_AliasCacheSize() / 1024);
}

static EE_RegFrame sys_exception_frame __attribute__((aligned(128)));
static byte sys_exception_stack[16384] __attribute__((aligned(128)));
static volatile int sys_exception_active;

static void Sys_PS2WriteException(void)
{
	EE_RegFrame *frame = &sys_exception_frame;
	char text[2048], path[MAX_OSPATH];
	int fd, length, i, written = 0;

	EIntr();
	length = snprintf(text, sizeof(text),
		"NZ:P CPU exception %u\nEPC %08x CAUSE %08x BADVADDR %08x\n"
		"RA %08x SP %08x STATUS %08x\nLast file: %s\n",
		(frame->cause >> 2) & 31,
		frame->epc, frame->cause, frame->badvaddr,
		frame->ra[0], frame->sp[0], frame->status, sys_last_file);
	for (i = 0; i < 32; i++) {
		const u32 *reg = (const u32 *)frame + i * 4;
		length += snprintf(text + length, sizeof(text) - length,
			"r%02d %08x %08x %08x %08x\n", i, reg[0], reg[1], reg[2], reg[3]);
	}
	snprintf(path, sizeof(path), "%s/crash.log", sys_data_directory);
	fd = fileXioOpen(path, FIO_O_WRONLY | FIO_O_CREAT | FIO_O_TRUNC, 0666);
	if (fd >= 0) {
		while (written < length) {
			int count = fileXioWrite(fd, text + written, length - written);
			if (count <= 0)
				break;
			written += count;
		}
		fileXioClose(fd);
	}
	for (;;)
		SleepThread();
}

static int Sys_PS2Exception(EE_RegFrame *frame)
{
	if (sys_exception_active)
		for (;;) {}
	sys_exception_active = 1;
	memcpy(&sys_exception_frame, frame, sizeof(*frame));
	frame->epc = (u32)Sys_PS2WriteException;
	memset(frame->sp, 0, sizeof(frame->sp));
	frame->sp[0] = (u32)(sys_exception_stack + sizeof(sys_exception_stack) - 16);
	frame->cause &= ~0x80000000u;
	return 0;
}

int main(int argc, char **argv)
{
	static quakeparms_t parms;
	static startup_arguments_t startup;
	const char *base_directory;
	char startup_error[256], game_directory[MAX_OSPATH], setup_file[MAX_OSPATH];
	const char *setup_path = setup_file;
	size_t heap_size, default_heap;
	DIR *dir;
	double oldtime, newtime;
	int i, heaparg, unit;
	sys_ps2_storage_t storage;

	ChangeThreadPriority(GetThreadId(), 0x40);
	init_scr();
	ee_dbg_install(1);
	for (i = 1; i <= 13; i++)
		if (i <= 7 || i >= 10)
			ee_dbg_set_level1_handler(i, Sys_PS2Exception);
	sys_boot_screen = true;
	Sys_Printf("NZ:P PS2\n");
	SifInitRpc(0);
	storage = Sys_PS2BootStorage(argc, argv, &unit);
	while (!SifIopReset(NULL, 0)) {}
	while (!SifIopSync()) {}
	SifInitRpc(0);
	sbv_patch_enable_lmb();
	Sys_LoadModuleBuffer("sio2man.irx", sio2man_irx, size_sio2man_irx);
	Sys_PS2InitStorage(storage, unit);
	COM_SetFileSystemCaseSensitive(storage == PS2_STORAGE_HDD);
	Sys_LoadModuleBuffer("padman.irx", padman_irx, size_padman_irx);
	Sys_LoadModule("rom0:LIBSD");
	Sys_LoadModuleBuffer("audsrv.irx", audsrv_irx, size_audsrv_irx);

	snprintf(setup_file, sizeof(setup_file), "%s/setup.ini", sys_data_directory);
	if (Sys_FileTime((char *)setup_path) < 0)
		setup_path = NULL;
	if (!Startup_LoadArguments(&startup, argc, argv, setup_path,
		startup_error, sizeof(startup_error)))
		Sys_Error("Startup: %s", startup_error);
	if (!Startup_GetBaseDirectory(&startup, sys_data_directory, &base_directory,
		startup_error, sizeof(startup_error)))
		Sys_Error("Startup: %s", startup_error);
	for (i = 1; i < startup.argc; i++)
		if (!strcmp(startup.argv[i], "-mem"))
			startup.argv[i] = "-heap";
	COM_InitArgv(startup.argc, startup.argv);
	if (COM_CheckParm("-ps2-exception-test"))
		__asm__ volatile("teq $zero, $zero" ::: "memory");
	parms.basedir = (char *)base_directory;
	if (snprintf(game_directory, sizeof(game_directory), "%s/nzp", base_directory) >= sizeof(game_directory))
		Sys_Error("Startup: game directory path is too long");
	dir = opendir(game_directory);
	if (!dir)
		Sys_Error("Game data not found: %s", game_directory);
	closedir(dir);

	heaparg = Startup_FindArgument(&startup, "-heap");
	default_heap = 16 * 1024 * 1024;
	if (!heaparg && Sys_PS2HeapAvailable() < default_heap + 6 * 1024 * 1024)
		default_heap = 15 * 1024 * 1024;
	parms.membase = Startup_AllocateHeap(&startup, default_heap, &heap_size,
		startup_error, sizeof(startup_error));
	if (!parms.membase && !heaparg && default_heap > 15 * 1024 * 1024)
		parms.membase = Startup_AllocateHeap(&startup, 15 * 1024 * 1024, &heap_size,
			startup_error, sizeof(startup_error));
	if (!parms.membase)
		Sys_Error("Startup: %s", startup_error);
	if (heap_size < MINIMUM_MEMORY || heap_size > 20 * 1024 * 1024)
		Sys_Error("Invalid heap size (use -heap 6 through 20)");
	parms.memsize = (int)heap_size;
	parms.argc = com_argc;
	parms.argv = com_argv;

	Host_Init(&parms);
	Cbuf_AddText("exec config.cfg\n");
	oldtime = Sys_FloatTime();
	while (1) {
		newtime = Sys_FloatTime();
		Host_Frame(newtime - oldtime);
		RotateThreadReadyQueue(0x40);
		oldtime = newtime;
	}
	return 0;
}
