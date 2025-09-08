/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyrgith (C) 2025 NZ:P Team

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
// platform_def.h -- CTR-specific header to compliment primary definitions (../nzportable_def.h)
//                   as a general rule, please try to keep this file as small as possible, and
//                   provide context as to why additions are necessary.


// Some platforms have not fully migrated to bool from qboolean, CTR included. 
#include "gl/glad/gl.h"