/*
 * Copyright (C) 1996-1997 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#pragma once
#ifndef PS3_INSTALLDIR
#define PS3_INSTALLDIR "/dev_hdd0/game/NZPT00001/USRDIR"
#endif
#ifndef PS3_BASEDIR
#define PS3_BASEDIR PS3_INSTALLDIR "/nzportable"
#endif
int
PS3_GraphicsInit(void);
int
PS3_GraphicsSwap(int vsync);
void
PS3_GraphicsShutdown(void);
void
PS3_GraphicsBeginExit(void);
const char *
PS3_GraphicsError(void);

int
PS3_OSKActive(void);
void
PS3_OSKShutdown(void);
void
IN_OpenOSKeyboard(void);
