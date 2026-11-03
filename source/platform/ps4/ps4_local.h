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

#pragma once
#include <stddef.h>
#include <stdint.h>
#define PS4_SETUP_PATH "/data/nzportable/setup.ini"
extern int ps4_user;

int
PS4_GraphicsInit(void);

int
PS4_GraphicsSwap(int vsync);

void
PS4_GraphicsShutdown(void);

int
PS4_InitUserService(void);

int
PS4_GetSetupPath(const char ** path);

void
PS4_WriteLog(const char * path, const char * text, size_t length);

int
PS4_LoadModule(uint32_t module);

const char *
PS4_GraphicsError(void);

int
PS4_OSKActive(void);

void
PS4_OSKPoll(void);

void
PS4_OSKShutdown(void);
