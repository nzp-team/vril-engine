/*
 * Copyright (C) 2026 NZP Team.
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
 */
// r_alias_lerp.h -- Alias model interpolation
#pragma once

#include <math.h>

static inline float R_AliasLerpDistanceSquared(float height, float base_fov, float render_fov)
{
    const float radians = 3.14159265358979323846f / 360.0f;
    float distance = 400.0f * height / 240.0f;
    if (render_fov < base_fov)
        distance *= tanf(base_fov * radians) / tanf(render_fov * radians);
    return distance * distance;
}
