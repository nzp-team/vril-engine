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
// nzportable_def.h -- primary header for client

#pragma once

#ifndef PLATFORM_DIRECTORY
#error "Unknown platform! (Please build with -DPLATFORM_DIRECTORY='your_platform')"
#endif // PLATFORM_DIRECTORY

#ifndef PLATFORM_RENDERER
#error "Unknown renderer! (Please build with -DPLATFORM_RENDERER='your_renderer')"
#endif // PLATFORM_RENDERER

#define STRINGIFY_MACRO(x) STR(x)
#define STR(x) #x
#define EXPAND(x) x
#define CONCAT(n1, n2, n3, n4) STRINGIFY_MACRO(EXPAND(n1)EXPAND(n2)EXPAND(n3)EXPAND(n4))
#define CONCAT8(n1, n2, n3, n4, n5, n6, n7, n8) STRINGIFY_MACRO(EXPAND(n1)EXPAND(n2)EXPAND(n3)EXPAND(n4)EXPAND(n5)EXPAND(n6)EXPAND(n7)EXPAND(n8))
#define PLATFORM_FILE(file) CONCAT(platform/,PLATFORM_DIRECTORY,/,file)
#define RENDERER_FILE(file) CONCAT8(platform/,PLATFORM_DIRECTORY,/,PLATFORM_RENDERER,/,PLATFORM_RENDERER,_,file)

#define	QUAKE_GAME			// as opposed to utilities

#define	VERSION				2.0
#define	GLQUAKE_VERSION		1.00
#define	D3DQUAKE_VERSION	0.01
#define	WINQUAKE_VERSION	0.996
#define	LINUX_VERSION		1.30
#define	X11_VERSION			1.10

#define	GAMENAME	"nzp"

// Platforms such as Nspire have filesystem restrictions where
// files on-flash must meet certain requirements. They can set
// these in their Makefiles to respect them for use in read/write
// of specific files. Defaults (blanks) are defined here if unset
// for visibility.
#ifndef FILE_SPECIAL_PREFIX
#define FILE_SPECIAL_PREFIX 	""
#endif // FILE_SPECIAL_PREFIX
#ifndef FILE_SPECIAL_SUFFIX
#define FILE_SPECIAL_SUFFIX		""
#endif // FILE_SPECIAL_SUFFIX

// This is the size of the AI pathfinding array and the upper bounds
// for maximum AI the server is allowed to let exist at once
#ifndef MAX_AI_COUNT
#pragma message "Platform does not set MAX_AI_COUNT, defaulting to 24.."
#define MAX_AI_COUNT 24
#endif // MAX_AI_COUNT

#include PLATFORM_FILE(platform_def.h)

#include <math.h>
#include <stdarg.h>
#include <setjmp.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define	VID_LockBuffer()
#define	VID_UnlockBuffer()

#if defined __i386__
#define id386	1
#else
#define id386	0
#endif

#if id386
#define UNALIGNED_OK	1	// set to 0 if unaligned accesses are not supported
#else
#define UNALIGNED_OK	0
#endif

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
#define CACHE_SIZE	32		// used to align key data structures

#define UNUSED(x)	(x = x)	// for pesky compiler / lint warnings

#define	MINIMUM_MEMORY			0x550000
#define	MINIMUM_MEMORY_LEVELPAK	(MINIMUM_MEMORY + 0x100000)

#define MAX_NUM_ARGVS	50

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2


#define	MAX_QPATH		64			// max length of a quake game pathname
#define	MAX_OSPATH		128			// max length of a filesystem pathname

#define	ON_EPSILON		0.1			// point on plane side epsilon

#define	MAX_MSGLEN		64000		// max length of a reliable message Crow_Bar. UP for PSP
#define	MAX_DATAGRAM	8000		// max length of unreliable message Crow_Bar. UP for PSP

//
// per-level limits
//
#define	MAX_EDICTS		1024			// note to myself: don't increase this further, 1024 is more than enough. :P
#define	MAX_LIGHTSTYLES	64
#define	MAX_MODELS		300			// motolegacy -- nzp protocol(115), uses memory inefficient shorts for model indexes, yay!
#define	MAX_SOUNDS		256			// so they cannot be blindly increased

#define	SAVEGAME_COMMENT_LENGTH	39

#define	MAX_STYLESTRING	64

//
// stats are integers communicated to the client by the server
//
#define	MAX_CL_STATS		32
#define	STAT_HEALTH			0
#define	STAT_points			1
#define	STAT_WEAPON			2
#define	STAT_AMMO			3
#define	STAT_SECGRENADES	4
#define	STAT_WEAPONFRAME	5
#define	STAT_CURRENTMAG		6
#define	STAT_ZOOM			7
#define	STAT_WEAPONSKIN		8
#define	STAT_GRENADES		9
#define	STAT_ACTIVEWEAPON	10
#define	STAT_ROUNDS			11
#define	STAT_ROUNDCHANGE	12
#define	STAT_X2				13
#define	STAT_INSTA			14
#define	STAT_PRIGRENADES	15
#define	STAT_WEAPON2		17
#define	STAT_WEAPON2SKIN	18
#define	STAT_WEAPON2FRAME	19
#define STAT_CURRENTMAG2 	20

// stock defines

#define W_COLT 		1
#define W_KAR 		2
#define W_THOMPSON 	3
#define W_357		4
#define W_BAR		5
#define W_BK		6
#define W_BROWNING	7
#define W_DB		8
#define W_FG		9
#define W_GEWEHR	10
#define W_KAR_SCOPE	11
#define W_M1		12
#define W_M1A1		13
#define W_M2		14
#define W_MP40		15
#define W_MG		16
#define W_PANZER	17
#define W_PPSH		18
#define W_PTRS		19
#define W_RAY		20
#define W_SAWNOFF	21
#define W_STG		22
#define W_TRENCH	23
#define W_TYPE		24

#define W_BIATCH  28
#define W_KILLU   29 //357
#define W_COMPRESSOR 30 // Gewehr
#define W_M1000  31 //garand
#define W_KOLLIDER  32 // mp5
#define W_PORTER  33 // Ray
#define W_WIDDER  34 // M1A1
#define W_FIW  35 //upgraded flamethrower
#define W_ARMAGEDDON  36 //Kar
//#define W_WUNDER  37
#define W_GIBS  38 // thompson
#define W_SAMURAI  39 //Type
#define W_AFTERBURNER  40 //mp40
#define W_SPATZ  41 // stg
#define W_SNUFF  42 // sawn off
#define W_BORE  43 // double barrel
#define W_IMPELLER  44 //fg
#define W_BARRACUDA  45 //mg42
#define W_ACCELERATOR  46 //M1919 browning
#define W_GUT  47 //trench
#define W_REAPER  48 //ppsh
#define W_HEADCRACKER  49 //scoped kar
#define W_LONGINUS  50 //panzer
#define W_PENETRATOR  51 //ptrs
#define W_WIDOW  52 //bar
//#define W_KRAUS  53 //ballistic
#define W_MP5   54
#define W_M14   55

#define W_TESLA  56
#define W_DG3 	 57 //tesla

#define W_SPRING 58
#define W_PULVERIZER 59

#define W_NOWEP   420

//===========================================

#define	MAX_SCOREBOARD		16
#define	MAX_SCOREBOARDNAME	32

#define	SOUND_CHANNELS		8

#include PLATFORM_FILE(common.h)
#include PLATFORM_FILE(vid.h)
#include PLATFORM_FILE(sys.h)

#include "system.h"
#include "zone.h"
#include "mathlib.h"
#include "bspfile.h"

typedef struct
{
	vec3_t	origin;
	vec3_t	angles;
	int		modelindex;
	int		frame;
	int		colormap;
	int		skin;
	int		effects;
	// dr_mabuse1981: HalfLife rendermodes fixed START
	unsigned short renderamt;
	unsigned short rendermode;
	unsigned short rendercolor;
	// dr_mabuse1981: HalfLife rendermodes fixed END
} entity_state_t;

#include "wad.h"
#include "draw.h"
#include "cvar.h"

#include PLATFORM_FILE(screen.h)
#include PLATFORM_FILE(net.h)

#include "protocol.h"
#include "cmd.h"
#include "test_handler.h"

#include PLATFORM_FILE(render.h)
#include PLATFORM_FILE(client.h)

#include "cl_hud.h"
#include "sound.h"

#include "progs.h"

#include PLATFORM_FILE(server.h)
#include RENDERER_FILE(model.h)

#ifdef SOFTWARE_RENDERER
#include PLATFORM_FILE(d_iface.h)
#endif // SOFTWARE_RENDERER

#include "input.h"
#include "world.h"

#include PLATFORM_FILE(keys.h)

#include "console.h"
#include "view.h"

#include PLATFORM_FILE(menu.h)

#include "crc.h"
#include "cdaudio.h"

#include RENDERER_FILE(main.h)

#include "render/r_main.h"

//=============================================================================

// the host system specifies the base of the directory tree, the
// command line parms passed to the program, and the amount of memory
// available for the program to use

typedef struct
{
	char	*basedir;
	char	*cachedir;		// for development over ISDN lines
	int		argc;
	char	**argv;
	void	*membase;
	int		memsize;
} quakeparms_t;


//=============================================================================



extern qboolean noclip_anglehack;


//
// host
//
extern	quakeparms_t host_parms;

extern	cvar_t		sys_ticrate;
extern	cvar_t		sys_nostdout;
extern	cvar_t		developer;

extern	qboolean	host_initialized;		// true if into command execution
extern	double		host_frametime;
extern	byte		*host_basepal;
extern	byte		*host_colormap;

extern	int			host_framecount;	// incremented every frame, never reset
extern	double		realtime;			// not bounded in any way, changed at
										// start of every frame, never reset

void Host_ClearMemory (void);
void Host_ServerFrame (void);
void Host_InitCommands (void);
void Host_Init (quakeparms_t *parms);
void Host_Shutdown(void);
void Host_Error (char *error, ...);
void Host_EndGame (char *message, ...);
void Host_Frame (float time);
void Host_Quit_f (void);
void Host_ClientCommands (char *fmt, ...);
void Host_ShutdownServer (qboolean crash);

extern qboolean		msg_suppress_1;		// suppresses resolution and cache size console output
										//  an fullscreen DIB focus gain/loss
extern int			current_skill;		// skill level for currently loaded level (in case
										//  the user changes the cvar while the level is
										//  running, this reflects the level actually in use)

extern qboolean		isDedicated;

extern int			minimum_memory;

extern	vec3_t	NULLVEC;

#define ISUNDERWATER(x) ((x) == CONTENTS_WATER || (x) == CONTENTS_SLIME || (x) == CONTENTS_LAVA)

int SV_HullPointContents (hull_t *hull, int num, vec3_t p);
#define TruePointContents(p) SV_HullPointContents(&cl.worldmodel->hulls[0], 0, p)

//
// chase
//
extern	cvar_t	chase_active;

void Chase_Init (void);
void Chase_Reset (void);
void Chase_Update (void);

//ZOMBIE AI STUFF
#define MAX_WAYPOINTS 256 //max waypoints
typedef struct
{
	int pathlist [MAX_WAYPOINTS];
	int pathlist_length;
	int zombienum;
} zombie_ai;

typedef struct
{
	vec3_t origin;
	float g_score, f_score;
	int open; // Determine if the waypoint is "open" a.k.a active
	char special[64]; //special tag is required for the closed waypoints
	int target [8]; //Each waypoint can have up to 8 targets
	float dist [8]; // Distance to the next waypoints
	int came_from; // Used for pathfinding store where we got here to this
	qboolean used; // Set to `qtrue` if this waypoint contains valid data (not an empty slot in a list)
} waypoint_ai;

extern waypoint_ai waypoints[MAX_WAYPOINTS];
extern int n_waypoints;
extern short closest_waypoints[MAX_EDICTS];

// thread structs
typedef struct
{
	vec3_t origin;
	vec3_t forward;
	vec3_t right;
	vec3_t up;
	qboolean ready;
} soundstruct_t;

// ----------------------------------------------------------------------------
// Utils for using cstdlib qsort (Quick sort)
//
// Usage:
//		argsort_entry_t sort_values[10];
//	
//		for(int i = 0; i < 10; i++) {
//			sort_values[i].index = i;
//			sort_values[i].value = something;
//		}
//	
//		qsort(sort_values, 10, sizeof(argsort_entry_t), argsort_comparator);
// 
// ----------------------------------------------------------------------------
// Struct used for sorting a list of indices by some value
typedef struct argsort_entry_s {
	int index;
	float value;
} argsort_entry_t;
extern int argsort_comparator(const void *lhs, const void *rhs);
// ----------------------------------------------------------------------------

extern func_t	EndFrame;


#ifdef __3DS__
#define VERTEXARRAYSIZE 18360
extern float gVertexBuffer[VERTEXARRAYSIZE];
extern float gColorBuffer[VERTEXARRAYSIZE];
extern float gTexCoordBuffer[VERTEXARRAYSIZE];
#endif // __3DS__
