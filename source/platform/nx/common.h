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
// common.h -- general definitions

#ifndef COMMON_H
#define COMMON_H

#include <stdarg.h>
#include <stdio.h>

//#include "shell.h"

#ifndef MAX_OSPATH
#define MAX_OSPATH 1024
#endif

//#if !defined(GAMENAME) && defined(__SWITCH__)
//#define GAMENAME "/switch/nzportable"
//#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 1024
#endif

#define MAX_NUM_ARGVS 50

#define stringify__(x) #x
#define stringify(x) stringify__(x)

#define bound(a, b, c) ((a) >= (c) ? (a) : (b) < (a) ? (a) : (b) > (c) ? (c) : (b))

typedef unsigned char byte;

typedef uint32_t qboolean;

#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:        the pointer to the member.
 * @type:       the type of the container struct this is embedded in.
 * @member:     the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member)                    \
    ({                                                     \
        typeof(((type *)0)->member) *__mptr = (ptr);       \
        (type *)((char *)__mptr - offsetof(type, member)); \
    })

#define const_container_of(ptr, type, member)                          \
    ({                                                                 \
        const typeof(((type *)0)->member) *__mptr = (ptr);             \
        (const type *)((const char *)__mptr - offsetof(type, member)); \
    })

//============================================================================

typedef struct sizebuf_s {
    qboolean allowoverflow;  // if false, do a Sys_Error
    qboolean overflowed;     // set to true if the buffer size failed
    byte *data;
    int maxsize;
    int cursize;
} sizebuf_t;

#ifdef NQ_HACK
void SZ_Alloc(sizebuf_t *buf, int startsize);
void SZ_Free(sizebuf_t *buf);
#endif
void SZ_Clear(sizebuf_t *buf);
void SZ_Write(sizebuf_t *buf, const void *data, int length);
void SZ_Print(sizebuf_t *buf, const char *data);  // strcats onto the sizebuf

//============================================================================

typedef struct link_s {
    struct link_s *prev, *next;
} link_t;

void ClearLink(link_t *l);
void RemoveLink(link_t *l);
void InsertLinkBefore(link_t *l, link_t *before);
void InsertLinkAfter(link_t *l, link_t *after);

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
#define	STRUCT_FROM_LINK(l,t,m) ((t *)((byte *)l - (intptr_t)&(((t *)0)->m)))

//============================================================================

#ifndef NULL
#define NULL ((void *)0)
#endif

#define Q_MAXCHAR ((char)0x7f)
#define Q_MAXSHORT ((short)0x7fff)
#define Q_MAXINT ((int)0x7fffffff)
#define Q_MAXLONG ((int)0x7fffffff)
#define Q_MAXFLOAT ((int)0x7fffffff)

#define Q_MINCHAR ((char)0x80)
#define Q_MINSHORT ((short)0x8000)
#define Q_MININT ((int)0x80000000)
#define Q_MINLONG ((int)0x80000000)
#define Q_MINFLOAT ((int)0x7fffffff)

//============================================================================

extern qboolean bigendien;

extern short (*BigShort)(short l);
extern short (*LittleShort)(short l);
extern int (*BigLong)(int l);
extern int (*LittleLong)(int l);
extern float (*BigFloat)(float l);
extern float (*LittleFloat)(float l);

//============================================================================

#ifdef QW_HACK
extern struct usercmd_s nullcmd;
#endif

void MSG_WriteChar(sizebuf_t *sb, int c);
void MSG_WriteByte(sizebuf_t *sb, int c);
void MSG_WriteShort(sizebuf_t *sb, int c);
void MSG_WriteLong(sizebuf_t *sb, int c);
void MSG_WriteFloat(sizebuf_t *sb, float f);
void MSG_WriteString(sizebuf_t *sb, const char *s);
void MSG_WriteStringf(sizebuf_t *sb, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void MSG_WriteStringvf(sizebuf_t *sb, const char *fmt, va_list ap)
    __attribute__((format(printf, 2, 0)));
void MSG_WriteCoord(sizebuf_t *sb, float f);
void MSG_WriteAngle(sizebuf_t *sb, float f);
void MSG_WriteAngle16(sizebuf_t *sb, float f);
#ifdef QW_HACK
void MSG_WriteDeltaUsercmd(sizebuf_t *sb, const struct usercmd_s *from,
                           const struct usercmd_s *cmd);
#endif
#ifdef NQ_HACK
void MSG_WriteControlHeader(sizebuf_t *sb);
#endif

extern int msg_readcount;
extern qboolean msg_badread;  // set if a read goes beyond end of message

void MSG_BeginReading(void);
#ifdef QW_HACK
int MSG_GetReadCount(void);
#endif
int MSG_ReadChar(void);
int MSG_ReadByte(void);
int MSG_ReadShort(void);
int MSG_ReadLong(void);
float MSG_ReadFloat(void);
char *MSG_ReadString(void);
#ifdef QW_HACK
char *MSG_ReadStringLine(void);
#endif

float MSG_ReadCoord(void);
float MSG_ReadAngle(void);
float MSG_ReadAngle16(void);
#ifdef QW_HACK
void MSG_ReadDeltaUsercmd(const struct usercmd_s *from, struct usercmd_s *cmd);
#endif
#ifdef NQ_HACK
int MSG_ReadControlHeader(void);
#endif

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

extern char *com_token;
extern qboolean com_eof;

char *COM_Parse(char *data);

extern unsigned com_argc;
extern char **com_argv;

unsigned COM_CheckParm(char *parm);

void COM_Init(char *path);
void COM_InitArgv(int argc, char **argv);

char *COM_SkipPath(char *pathname);
void COM_StripExtension(char *filename, char *out, size_t buflen);
void COM_FileBase(char *in, char *out, size_t buflen);
void COM_DefaultExtension (char *path, char *extension);
char *COM_FileExtension(char *in);

char *CopyString (char *in);
char *va(const char *format, ...) __attribute__((format(printf, 1, 2)));

// does a varargs printf into a temp buffer

//============================================================================

extern int com_filesize;
struct cache_user_s;

extern char com_basedir[MAX_OSPATH];
extern char com_gamedir[MAX_OSPATH];

void COM_WriteFile(char *filename, const void *data, int len);
int COM_FOpenFile(char *filename, FILE **file);
void *COM_LoadStackFile (char *path, void *buffer, int bufsize, size_t *size);
void *COM_LoadTempFile(char *path);
void *COM_LoadHunkFile(char *path);
void COM_LoadCacheFile(char *path, struct cache_user_s *cu);

extern struct cvar_s registered;
extern qboolean standard_quake, rogue, hipnotic;

void Q_strncpyz (char *dest, char *src, size_t size);        //Diabolickal HLBSP

#endif /* COMMON_H */
