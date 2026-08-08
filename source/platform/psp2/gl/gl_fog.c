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
// gl_fog.c -- CTR picaGL Fog Rendering

#include "../../../nzportable_def.h"

/**
 * @brief Sets fogging mode. (Platform-specific, see `Fog_SetupFrame()`)
 */
void
Platform_Fog_Set(bool is_world_geometry, float start, float end, float red, float green, float blue, float alpha)
{
	/*
	 * The world is rendered in base-texture and multiplicative-lightmap passes.
	 * Neutral gray on the first pass lets the second pass produce the requested
	 * fog color. Game fog colors use a 0..100 scale; GL expects 0..1.
	 */
	float color[4] = {
		is_world_geometry ? 0.5f : red * 0.01f,
		is_world_geometry ? 0.5f : green * 0.01f,
		is_world_geometry ? 0.5f : blue * 0.01f,
		alpha
	};

    glFogfv(GL_FOG_COLOR, color);
	glFogf(GL_FOG_START, start);
	glFogf(GL_FOG_END, end);
}

/**
 * @brief Enables fogging mode. (Platform-specific, see `Fog_EnableGFog()`)
 */
void
Platform_Fog_Enable(void)
{
    glEnable(GL_FOG);
}

/**
 * @brief Disables fogging mode. (Platform-specific, see `Fog_DisableGFog()`)
 */
void
Platform_Fog_Disable(void)
{
    glDisable(GL_FOG);
}

/**
 * @brief Called at startup, inits graphics functions. (Platform-specific, see `Fog_Init()`)
 */
void
Platform_Fog_Init(void)
{
	glFogi(GL_FOG_MODE, GL_LINEAR);
}
