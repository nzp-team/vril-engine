/*
 * Copyright (C) 2023 NZ:P Team
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
// gu_fog.c -- PSP sceGu Fog Rendering

#include "../../../nzportable_def.h"
#include <pspgu.h>

/**
 * @brief Sets fogging mode. (Platform-specific, see `Fog_SetupFrame()`)
 */
void
Hyena_Fog_Set(bool is_world_geometry, float start, float end, float red, float green, float blue, float alpha)
{
    // If we have normal color fog for world geom + lightmap, then the resulting color when fully fogged is wrong.
    // If we have exactly 0.5 0.5 0.5 gray for world game and normal fog for lightmap, the end result of fully fogged
    // stuff is the actual fog color.
    unsigned int color = is_world_geometry ? GU_COLOR(0.5f, 0.5f, 0.5f, alpha) : GU_COLOR(red * 0.01f, green * 0.01f,
        blue * 0.01f, alpha);

    sceGuFog(start, end, color);
}

/**
 * @brief Enables fogging mode. (Platform-specific, see `Fog_EnableGFog()`)
 */
void
Hyena_Fog_Enable(void)
{
    sceGuEnable(GU_FOG);
}

/**
 * @brief Disables fogging mode. (Platform-specific, see `Fog_DisableGFog()`)
 */
void
Hyena_Fog_Disable(void)
{
    sceGuDisable(GU_FOG);
}

/**
 * @brief Called at startup, inits graphics functions. (Platform-specific, see `Fog_Init()`)
 */
void
Hyena_Fog_Init(void)
{
}
