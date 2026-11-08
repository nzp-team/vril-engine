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
#include <ctype.h>
#include <time.h>
#include <windows.h>
#include <limits.h>
#include "xbox_local.h"

#include <stb_sprintf.h>

#undef fopen
#undef remove
#undef rename

int
Sys_XboxVSNPrintf(char * buffer, size_t size, const char * format, va_list args)
{
    return stbsp_vsnprintf(buffer, size > INT_MAX ? INT_MAX : (int) size, format, args);
}

int
Sys_XboxSNPrintf(char * buffer, size_t size, const char * format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = Sys_XboxVSNPrintf(buffer, size, format, args);
    va_end(args);
    return result;
}

int
Sys_XboxVSPrintf(char * buffer, const char * format, va_list args)
{
    return stbsp_vsprintf(buffer, format, args);
}

int
Sys_XboxSPrintf(char * buffer, const char * format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = Sys_XboxVSPrintf(buffer, format, args);
    va_end(args);
    return result;
}

static char *
Sys_XboxPrintChunk(const char * buffer, void * context, int length)
{
    return fwrite(buffer, 1, length, context) == (size_t) length ? (char *) buffer : NULL;
}

int
Sys_XboxVFPrintf(FILE * file, const char * format, va_list args)
{
    char buffer[STB_SPRINTF_MIN];
    int result = stbsp_vsprintfcb(Sys_XboxPrintChunk, file, buffer, format, args);

    return ferror(file) ? -1 : result;
}

int
Sys_XboxFPrintf(FILE * file, const char * format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = Sys_XboxVFPrintf(file, format, args);
    va_end(args);
    return result;
}

qboolean
Sys_XboxNativePath(const char * path, char * native, size_t size)
{
    size_t i;

    if (strlen(path) >= size) {
        errno = ENAMETOOLONG;
        return false;
    }
    for (i = 0; path[i]; ++i)
        native[i] = path[i] == '/' ? '\\' : path[i];
    native[i] = '\0';
    return true;
}

FILE *
Sys_XboxFOpen(const char * path, const char * mode)
{
    char native[260];

    return Sys_XboxNativePath(path, native, sizeof(native)) ? fopen(native, mode) : NULL;
}

int
Sys_XboxRemove(const char * path)
{
    char native[260];

    return Sys_XboxNativePath(path, native, sizeof(native)) ? remove(native) : -1;
}

int
Sys_XboxRename(const char *old_path, const char *new_path)
{
    char old_native[260], new_native[260];

    return Sys_XboxNativePath(old_path, old_native, sizeof(old_native)) &&
      Sys_XboxNativePath(new_path, new_native, sizeof(new_native)) ? rename(old_native, new_native) : -1;
}

double
Sys_XboxStrtod(const char * text, char ** end)
{
    const char * start = text;
    const char * digits;
    double value = 0, fraction = 0.1;
    int sign = 1, exponent = 0, exponent_sign = 1;

    while (isspace((unsigned char) *text))
        ++text;
    if (*text == '-' || *text == '+')
        sign = *text++ == '-' ? -1 : 1;
    digits = text;
    while (*text >= '0' && *text <= '9')
        value = value * 10 + *text++ - '0';
    if (*text == '.') {
        ++text;
        while (*text >= '0' && *text <= '9') {
            value    += (*text++ - '0') * fraction;
            fraction *= 0.1;
        }
    }
    if (text == digits || (text == digits + 1 && *digits == '.')) {
        if (end)
            *end = (char *) start;
        return 0;
    }
    if (*text == 'e' || *text == 'E') {
        const char * marker = text++;
        if (*text == '-' || *text == '+')
            exponent_sign = *text++ == '-' ? -1 : 1;
        digits = text;
        while (*text >= '0' && *text <= '9') {
            if (exponent < 10000)
                exponent = exponent * 10 + *text - '0';
            ++text;
        }
        if (text == digits)
            text = marker;
        else if (value != 0)
            value *= pow(10.0, exponent_sign * exponent);
    }
    if (end)
        *end = (char *) text;
    return sign * value;
} /* Sys_XboxStrtod */

double
atof(const char * text)
{
    return strtod(text, NULL);
}

struct tm *
localtime(const time_t * timer)
{
    static struct tm result;
    static const int month_days[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    ULARGE_INTEGER ticks;
    FILETIME utc, local;
    SYSTEMTIME fields;
    int leap;

    if (!timer)
        return NULL;

    ticks.QuadPart     = ((LONGLONG) *timer + 11644473600LL) * 10000000LL;
    utc.dwLowDateTime  = ticks.LowPart;
    utc.dwHighDateTime = ticks.HighPart;
    if (!FileTimeToLocalFileTime(&utc, &local) || !FileTimeToSystemTime(&local, &fields))
        return NULL;

    memset(&result, 0, sizeof(result));
    result.tm_sec  = fields.wSecond;
    result.tm_min  = fields.wMinute;
    result.tm_hour = fields.wHour;
    result.tm_mday = fields.wDay;
    result.tm_mon  = fields.wMonth - 1;
    result.tm_year = fields.wYear - 1900;
    result.tm_wday = fields.wDayOfWeek;
    leap = fields.wYear % 4 == 0 && (fields.wYear % 100 != 0 || fields.wYear % 400 == 0);
    result.tm_yday  = month_days[result.tm_mon] + result.tm_mday - 1 + (leap && result.tm_mon > 1);
    result.tm_isdst = -1;
    return &result;
}

char *
strtok_r(char * text, const char * delimiters, char ** next)
{
    char * start;

    if (!text)
        text = *next;
    if (!text)
        return NULL;

    start = text + strspn(text, delimiters);
    if (!*start) {
        *next = start;
        return NULL;
    }
    text = start + strcspn(start, delimiters);
    if (*text)
        *text++ = '\0';
    *next = text;
    return start;
}

int
fopen_s(FILE ** file, const char * path, const char * mode)
{
    if (!file || !path || !mode)
        return EINVAL;

    *file = Sys_XboxFOpen(path, mode);
    return *file ? 0 : errno;
}
