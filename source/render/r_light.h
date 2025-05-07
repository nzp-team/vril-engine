/*
 * Copyright (C) 1996-1997 Id Software, Inc.
 * Copyright (C) 2007 Peter Mackay and Chris Swindle.
 * Copyright (C) 2010-2014 QuakeSpasm developers
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
// r_light.h -- Light updating and sampling header

#ifndef _RENDER_LIGHT_H_
#define _RENDER_LIGHT_H_

void
R_AnimateLight(void);
void
R_MarkLights(dlight_t * light, int bit, mnode_t * node);
void
R_PushDlights(void);
int
R_LightPoint(vec3_t p);

#endif // _RENDER_LIGHT_H_
