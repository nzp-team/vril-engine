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
// vgl_fog.c -- PSP2 vitaGL Fog Rendering

#include "../../../nzportable_def.h"
#include <vitasdk.h>

/**
 * @brief Sets fogging mode. (Platform-specific, see `Fog_SetupFrame()`)
 */
extern GLint fogcoloruniformlocs[5];
extern GLint fogfaruniformlocs[5];
extern GLint fograngeuniformlocs[5];
void
Platform_Fog_Set(bool is_world_geometry, float start, float end, float red, float green, float blue, float alpha)
{
    float color[4] = {red / 64.0f, green / 64.0f, blue / 64.0f, alpha};
    for(int i = 0; i < 5; i++)
    {
        glUniform4fv(fogcoloruniformlocs[i], 1, color);
        glUniform1f(fogfaruniformlocs[i], end);
        glUniform1f(fograngeuniformlocs[i], end - start);
    }
}

/**
 * @brief Enables fogging mode. (Platform-specific, see `Fog_EnableGFog()`)
 */
void
Platform_Fog_Enable(void) // Stubbed on vitaGL backend due to the lack of a good way to enable fog when using shaders
{
    return;
}

/**
 * @brief Disables fogging mode. (Platform-specific, see `Fog_DisableGFog()`)
 */
void
Platform_Fog_Disable(void) // Stubbed on vitaGL backend due to the lack of a good way to disable fog when using shaders
{
    return;
}

/**
 * @brief Called at startup, inits graphics functions. (Platform-specific, see `Fog_Init()`)
 */
void
Platform_Fog_Init(void) // Uneeded on Vita
{
    return;
}
