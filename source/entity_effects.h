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

#ifndef ENTITY_EFFECTS_H
#define ENTITY_EFFECTS_H

#define EF_BLUELIGHT            1
#define EF_MUZZLEFLASH          2
#define EF_BRIGHTLIGHT          4
#define EF_REDLIGHT             8
#define EF_ORANGELIGHT          16
#define EF_GREENLIGHT           32
#define EF_PINKLIGHT            64
#define EF_NODRAW               128
#define EF_LIMELIGHT            256
#define EF_FULLBRIGHT           512
#define EF_CYANLIGHT            1024
#define EF_YELLOWLIGHT          2048
#define EF_PURPLELIGHT          4096
#define EF_RAYRED               8196
#define EF_RAYGREEN             16384
#define EF_RAYBEAM              32768
#define EF_RAYBEAMRED           (EF_RAYBEAM | EF_RAYRED)
#define EF_RAYBEAMGREEN         (EF_RAYBEAM | EF_RAYGREEN)

#endif
