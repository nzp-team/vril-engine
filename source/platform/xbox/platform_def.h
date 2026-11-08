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

// platform_def.h -- Xbox-specific definitions
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// nxdk exposes the MSVCRT spelling of case-insensitive string comparisons.
#define strcasecmp  _stricmp
#define strncasecmp _strnicmp
char *
strtok_r(char * text, const char * delimiters, char ** next);
int
fopen_s(FILE ** file, const char * path, const char * mode);

// stupid windows backslash stuff
FILE *
Sys_XboxFOpen(const char * path, const char * mode);
int
Sys_XboxRemove(const char * path);
int
Sys_XboxRename(const char *old_path, const char *new_path);
#define fopen                 Sys_XboxFOpen
#define remove                Sys_XboxRemove
#define rename                Sys_XboxRename

#define RENDERER_MODEL_HEADER "render/r_model.h"
