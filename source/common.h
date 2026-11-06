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
// comndef.h  -- general definitions

#pragma once

#include <stddef.h>
#include <stdint.h>
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#define bound(a, b, c) ((a) >= (c) ? (a) : (b) < (a) ? (a) : (b) > (c) ? (c) : (b))

#if !defined BYTE_DEFINED
typedef unsigned char 		byte;
#define BYTE_DEFINED 1
#endif

typedef unsigned int qboolean;

typedef struct
{
	char    name[MAX_QPATH];
	int             filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char    filename[MAX_OSPATH];
	int             handle;
	int             numfiles;
	packfile_t      *files;
} pack_t;

typedef struct searchpath_s
{
	unsigned int path_id;
	char    filename[MAX_OSPATH];
	pack_t  *pack;          // only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;



//============================================================================

typedef struct sizebuf_s
{
	qboolean	allowoverflow;	// if false, do a Sys_Error
	qboolean	overflowed;		// set to true if the buffer size failed
	byte	*data;
	int		maxsize;
	int		cursize;
} sizebuf_t;

void SZ_Alloc (sizebuf_t *buf, int startsize);
void SZ_Free (sizebuf_t *buf);
void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, int length);
void SZ_Write (sizebuf_t *buf, void *data, int length);
void SZ_Print (sizebuf_t *buf, char *data);	// strcats onto the sizebuf

//============================================================================

typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;


void ClearLink (link_t *l);
void RemoveLink (link_t *l);
void InsertLinkBefore (link_t *l, link_t *before);
void InsertLinkAfter (link_t *l, link_t *after);

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
#define STRUCT_FROM_LINK(link, type, member) ((type *)((byte *)(link) - offsetof(type, member)))

//============================================================================

#ifndef NULL
#define NULL ((void *)0)
#endif

#define Q_MAXCHAR ((char)0x7f)
#define Q_MAXSHORT ((short)0x7fff)
#define Q_MAXINT	((int)0x7fffffff)
#define Q_MAXLONG ((int)0x7fffffff)
#define Q_MAXFLOAT ((int)0x7fffffff)

#define Q_MINCHAR ((char)0x80)
#define Q_MINSHORT ((short)0x8000)
#define Q_MININT 	((int)0x80000000)
#define Q_MINLONG ((int)0x80000000)
#define Q_MINFLOAT ((int)0x7fffffff)

//============================================================================

extern	qboolean		bigendien;

extern	short	(*BigShort) (short l);

extern	int	(*BigLong) (int l);

extern float (*BigFloat) (float l);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define LittleLong(l) ((int)__builtin_bswap32((uint32_t)(l)))
#define LittleShort(l) ((short)__builtin_bswap16((uint16_t)(l)))
float FloatSwap(float value);
#define LittleFloat(l) FloatSwap(l)
#else
#define LittleLong(l) (l)
#define LittleShort(l) (l)
#define LittleFloat(l) (l)
#endif

//============================================================================

void MSG_WriteChar (sizebuf_t *sb, int c);
void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, char *s);
void MSG_WriteCoord (sizebuf_t *sb, float f);
void MSG_WriteAngle (sizebuf_t *sb, float f);

extern	int			msg_readcount;
extern	qboolean	msg_badread;		// set if a read goes beyond end of message

void MSG_BeginReading (void);
int MSG_ReadChar (void);
int MSG_ReadByte (void);
int MSG_ReadShort (void);
int MSG_ReadLong (void);
float MSG_ReadFloat (void);
char *MSG_ReadString (void);

float MSG_ReadCoord (void);
float MSG_ReadAngle (void);

//============================================================================

void Q_memset (void *dest, int fill, int count);
void Q_memcpy (void *dest, void *src, int count);
int Q_memcmp (void *m1, void *m2, int count);
void Q_strcpy (char *dest, char *src);
void Q_strncpy (char *dest, char *src, int count);
int Q_strlen (char *str);
char *Q_strrchr (char *s, char c);
void Q_strcat (char *dest, char *src);
int Q_strcasecmp (char *s1, char *s2);
int Q_strncasecmp (char *s1, char *s2, int n);
int	Q_atoi (char *str);
float Q_atof (char *str);
int q_snprintf (char *str, size_t size, const char *format, ...);
int q_vsnprintf(char *str, size_t size, const char *format, va_list args);

//============================================================================

extern	char		com_token[1024];
extern	qboolean	com_eof;

char *COM_Parse (char *data);


extern	int		com_argc;
extern	char	**com_argv;

int COM_CheckParm (char *parm);
void COM_Init (char *path);
void COM_InitArgv (int argc, char **argv);

char *COM_SkipPath (char *pathname);
void COM_StripExtension (char *in, char *out);
void COM_FileBase (char *in, char *out);
void COM_DefaultExtension (char *path, char *extension);
char *COM_FileExtension (char *in);

char	*va(char *format, ...);
char *CopyString (char *in);
// does a varargs printf into a temp buffer


//============================================================================

extern int com_filesize;
struct cache_user_s;

extern	char	com_gamedir[MAX_OSPATH];

void COM_WriteFile (char *filename, void *data, int len);
int COM_OpenFile (char *filename, int *hndl, unsigned int *path_id);
int COM_FOpenFile (char *filename, FILE **file);
void COM_CloseFile (int h);

byte *COM_LoadStackFile (char *path, void *buffer, int bufsize);
byte *COM_LoadTempFile (char *path);
byte *COM_LoadHunkFile (char *path);
void COM_LoadCacheFile (char *path, struct cache_user_s *cu);


extern	struct cvar_s	registered;

extern qboolean		standard_quake, rogue, hipnotic;
void Q_strncpyz (char *dest, char *src, size_t size);        //Diabolickal HLBSP

#define stringify__(x) #x
#define stringify(x) stringify__(x)
#define Q_CLAMP(a, b, c) bound(a, b, c)

void MSG_WritePreciseAngle(sizebuf_t *sb, float value);
float MSG_ReadPreciseAngle(void);
int Q_strcmp(char *a, char *b);
int Q_strncmp(char *a, char *b, int count);
void Q_snprintfz(char *dest, size_t size, char *format, ...);
size_t Q_strlcpy(char *dst, const char *src, size_t size);
size_t Q_strlcat(char *dst, const char *src, size_t size);
char *strltrim(char *s);
void COM_ForceExtension(char *path, char *extension);
void COM_GetFolder(char *in, char *out);
float COM_Clamp(float value, float minimum, float maximum);
qboolean COM_BuildFilePath(char *path, size_t size, const char *directory, const char *filename);
void COM_CreatePath(char *path);
byte *COM_LoadFile(char *path, int usehunk, unsigned int *path_id);
extern searchpath_t *com_searchpaths, *com_verifypaths;

/* min and max macros with type checking */
#define qmax(a, b)           \
    ({                       \
        typeof(a) a_ = (a);  \
        typeof(b) b_ = (b);  \
        (void)(&a_ == &b_);  \
        (a_ > b_) ? a_ : b_; \
    })
#define qmin(a, b)           \
    ({                       \
        typeof(a) a_ = (a);  \
        typeof(b) b_ = (b);  \
        (void)(&a_ == &b_);  \
        (a_ < b_) ? a_ : b_; \
    })


char *strtolower(char *s);

void COM_CopyFile(char *netpath, char *cachepath);
void COM_SetFileSystemCaseSensitive(qboolean case_sensitive);
void COM_AddGameDirectory(char *directory);

void COM_InitFilesystem(void);
