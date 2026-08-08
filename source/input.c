/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2026 NZ:P Team

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
// input.c  -- Global input handler

#include "nzportable_def.h"

void IN_Init (void)
{
#ifdef PLATFORM_INPUT_KBM
    IN_SetMouseToRelative(true);
#else

#endif
}

void IN_Shutdown (void)
{
#ifdef PLATFORM_INPUT_KBM
    IN_SetMouseToRelative(false);
#else

#endif
}

void IN_Commands (void)
{

}

float IN_CalcInput(int axis, float speed, float tolerance, float acceleration)
{

}

void IN_Move (usercmd_t *cmd)
{

}
