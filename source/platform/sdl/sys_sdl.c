#include "../../nzportable_def.h"
#include "sdl_local.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_HANDLES 32
#define DEFAULT_MEMORY_MB 128

static FILE *sys_handles[MAX_HANDLES];
qboolean isDedicated;
qboolean sdl_running = true;

static int Sys_FindHandle(void)
{
	int i;
	for (i = 1; i < MAX_HANDLES; ++i)
		if (!sys_handles[i]) return i;
	Sys_Error("out of handles");
	return -1;
}

static int Sys_FileLength(FILE *file)
{
	long position = ftell(file);
	long length;
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, position, SEEK_SET);
	return (int)length;
}

int Sys_FileOpenRead(char *path, int *handle)
{
	FILE *file = fopen(path, "rb");
	int index;
	if (!file) { *handle = -1; return -1; }
	index = Sys_FindHandle();
	sys_handles[index] = file;
	*handle = index;
	return Sys_FileLength(file);
}

int Sys_FileOpenWrite(char *path)
{
	FILE *file = fopen(path, "wb");
	int index;
	if (!file) Sys_Error("Error opening %s: %s", path, strerror(errno));
	index = Sys_FindHandle();
	sys_handles[index] = file;
	return index;
}

void Sys_FileClose(int handle) { fclose(sys_handles[handle]); sys_handles[handle] = NULL; }
void Sys_FileSeek(int handle, int position) { fseek(sys_handles[handle], position, SEEK_SET); }
int Sys_FileRead(int handle, void *dest, int count) { return (int)fread(dest, 1, count, sys_handles[handle]); }
int Sys_FileWrite(int handle, void *data, int count) { return (int)fwrite(data, 1, count, sys_handles[handle]); }
int Sys_FileTime(char *path) { struct stat st; return stat(path, &st) == 0 ? (int)st.st_mtime : -1; }
void Sys_mkdir(char *path) { mkdir(path, 0777); }
void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length) { (void)startaddr; (void)length; }

void Sys_PrintSystemInfo(void) { Con_Printf("Vril Engine SDL (%s)\n", SDL_GetPlatform()); }
void Sys_Printf(char *fmt, ...) { va_list args; va_start(args, fmt); vfprintf(stdout, fmt, args); va_end(args); }
void Sys_SystemError(char *error) { fprintf(stderr, "Vril Engine: %s\n", error); SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vril Engine", error, sdl_window); SDL_Quit(); exit(1); }
void Sys_Quit(void) { sdl_running = false; }
double Sys_FloatTime(void) { static Uint64 start; Uint64 now = SDL_GetPerformanceCounter(); if (!start) start = now; return (double)(now - start) / SDL_GetPerformanceFrequency(); }
char *Sys_ConsoleInput(void) { return NULL; }
void Sys_Sleep(void) { SDL_Delay(1); }
void Sys_HighFPPrecision(void) {}
void Sys_LowFPPrecision(void) {}
void Sys_SetFPCW(void) {}
void Sys_DebugLog(char *file, char *fmt, ...) { (void)file; (void)fmt; }
void Sys_CaptureScreenshot(void) { Con_Printf("Screenshots are not implemented by the SDL backend yet.\n"); }

void Sys_DefaultConfig(void)
{
	Cbuf_AddText("bind JOY1 +attack\n");
	Cbuf_AddText("bind JOY2 +aim\n");
	Cbuf_AddText("bind w +forward\n");
	Cbuf_AddText("bind s +back\n");
	Cbuf_AddText("bind a +moveleft\n");
	Cbuf_AddText("bind d +moveright\n");
	Cbuf_AddText("bind SPACE +jump\n");
}

int main(int argc, char **argv)
{
	quakeparms_t parms;
	double oldtime;
	memset(&parms, 0, sizeof(parms));
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}
	parms.memsize = DEFAULT_MEMORY_MB * 1024 * 1024;
	parms.membase = malloc(parms.memsize);
	parms.basedir = ".";
	if (!parms.membase) { fprintf(stderr, "Unable to allocate engine memory\n"); return 1; }
	COM_InitArgv(argc, argv);
	parms.argc = com_argc;
	parms.argv = com_argv;
	Host_Init(&parms);
	oldtime = Sys_FloatTime();
	while (sdl_running) {
		double now = Sys_FloatTime();
		Host_Frame(now - oldtime);
		music_update();
		oldtime = now;
	}
	Host_Shutdown();
	free(parms.membase);
	SDL_Quit();
	return 0;
}
