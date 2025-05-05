/*
 * Copyright (C) 1996-1997 Id Software, Inc.
 * Copyright (C) 2002-2009 John Fitzgibbons and others
 * Copyright (C) 2010-2014 QuakeSpasm developers
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
// r_fog.c -- Fog handling code

#include "../nzportable_def.h"

// MARK: Global Fog Handling

float old_density;
float old_start;
float old_end;
float old_red;
float old_green;
float old_blue;

float fade_time;
float fade_done;

/**
 * @brief Invoked when the client or server sends a request to update fog values.
 * @param start The requested start value.
 * @param end The requested end value.
 * @param red Red color intensity, 0-100 range.
 * @param green Green color intensity, 0-100 range.
 * @param blue Blue color intensity, 0-100 range.
 * @param time Time in seconds to transition from old fog to new request.
 */
void
Fog_Update(float start, float end, float red, float green, float blue, float time)
{
    if (start <= 0.01f || end <= 0.01f) {
        start = 0.0f;
        end   = 0.0f;
    }

    // save previous settings for fade
    if (time > 0) {
        // check for a fade in progress
        if (fade_done > cl.time) {
            float f;

            f         = (fade_done - cl.time) / fade_time;
            old_start = f * old_start + (1.0 - f) * r_refdef.fog_start;
            old_end   = f * old_end + (1.0 - f) * r_refdef.fog_end;
            old_red   = f * old_red + (1.0 - f) * r_refdef.fog_red;
            old_green = f * old_green + (1.0 - f) * r_refdef.fog_green;
            old_blue  = f * old_blue + (1.0 - f) * r_refdef.fog_blue;
        } else {
            old_start = r_refdef.fog_start;
            old_end   = r_refdef.fog_end;
            old_red   = r_refdef.fog_red;
            old_green = r_refdef.fog_green;
            old_blue  = r_refdef.fog_blue;
        }
    }

    r_refdef.fog_start = start;
    r_refdef.fog_end   = end;
    r_refdef.fog_red   = red;
    r_refdef.fog_green = green;
    r_refdef.fog_blue  = blue;
    fade_time = time;
    fade_done = cl.time + time;
} /* Fog_Update */

/**
 * @brief Called when the server sends the client an SVC_FOG request.
 */
void
Fog_ParseServerMessage(void)
{
    float start, end, red, green, blue, time;

    start = MSG_ReadByte() / 255.0;
    end   = MSG_ReadByte() / 255.0;
    red   = MSG_ReadByte() / 255.0;
    green = MSG_ReadByte() / 255.0;
    blue  = MSG_ReadByte() / 255.0;
    time  = MSG_ReadShort() / 100.0;

    if (start < 0.01f || end < 0.01f) {
        start = 0.0f;
        end   = 0.0f;
    }

    Fog_Update(start, end, red, green, blue, time);
}

/**
 * @brief 'fog' console command interpreter.
 */
void
Fog_FogCommand_f(void)
{
    switch (Cmd_Argc()) {
        default:
        case 1:
            Con_Printf("usage:\n");
            Con_Printf("   fog <fade>\n");
            Con_Printf("   fog <start> <end>\n");
            Con_Printf("   fog <red> <green> <blue>\n");
            Con_Printf("   fog <fade> <red> <green> <blue>\n");
            Con_Printf("   fog <start> <end> <red> <green> <blue>\n");
            Con_Printf("   fog <start> <end> <red> <green> <blue> <fade>\n");
            Con_Printf("current values:\n");
            Con_Printf("   \"start\" is \"%f\"\n", r_refdef.fog_start);
            Con_Printf("   \"end\" is \"%f\"\n", r_refdef.fog_end);
            Con_Printf("   \"red\" is \"%f\"\n", r_refdef.fog_red);
            Con_Printf("   \"green\" is \"%f\"\n", r_refdef.fog_green);
            Con_Printf("   \"blue\" is \"%f\"\n", r_refdef.fog_blue);
            Con_Printf("   \"fade\" is \"%f\"\n", fade_time);
            break;
        case 2: // TEST
            Fog_Update(r_refdef.fog_start,
              r_refdef.fog_end,
              r_refdef.fog_red,
              r_refdef.fog_green,
              r_refdef.fog_blue,
              atof(Cmd_Argv(1)));
            break;
        case 3:
            Fog_Update(atof(Cmd_Argv(1)),
              atof(Cmd_Argv(2)),
              r_refdef.fog_red,
              r_refdef.fog_green,
              r_refdef.fog_blue,
              0.0);
            break;
        case 4:
            Fog_Update(r_refdef.fog_start,
              r_refdef.fog_end,
              CLAMP(0.0, atof(Cmd_Argv(1)), 100.0),
              CLAMP(0.0, atof(Cmd_Argv(2)), 100.0),
              CLAMP(0.0, atof(Cmd_Argv(3)), 100.0),
              0.0);
            break;
        case 5:
            Fog_Update(r_refdef.fog_start,
              r_refdef.fog_end,
              CLAMP(0.0, atof(Cmd_Argv(1)), 100.0),
              CLAMP(0.0, atof(Cmd_Argv(2)), 100.0),
              CLAMP(0.0, atof(Cmd_Argv(3)), 100.0),
              atof(Cmd_Argv(4)));
            break;
        case 6:
            Fog_Update(atof(Cmd_Argv(1)),
              atof(Cmd_Argv(2)),
              CLAMP(0.0, atof(Cmd_Argv(3)), 100.0),
              CLAMP(0.0, atof(Cmd_Argv(4)), 100.0),
              CLAMP(0.0, atof(Cmd_Argv(5)), 100.0),
              0.0);
            break;
        case 7:
            Fog_Update(atof(Cmd_Argv(1)),
              atof(Cmd_Argv(2)),
              CLAMP(0.0, atof(Cmd_Argv(3)), 100.0),
              CLAMP(0.0, atof(Cmd_Argv(4)), 100.0),
              CLAMP(0.0, atof(Cmd_Argv(5)), 100.0),
              atof(Cmd_Argv(6)));
            break;
    }
} /* Fog_FogCommand_f */

/**
 * @brief Called during map loading process, parses worldspawn fields
 * to get starting fog value.
 */
void
Fog_ParseWorldspawn(void)
{
    char key[128], value[4096];
    char * data;

    // initially no fog
    r_refdef.fog_start = 0;
    old_start = 0;

    r_refdef.fog_end = -1;
    old_end = -1;

    r_refdef.fog_red = 0.0;
    old_red = 0.0;

    r_refdef.fog_green = 0.0;
    old_green = 0.0;

    r_refdef.fog_blue = 0.0;
    old_blue = 0.0;

    fade_time = 0.0;
    fade_done = 0.0;

    data = COM_Parse(cl.worldmodel->entities);
    if (!data)
        return;  // error

    if (com_token[0] != '{')
        return;  // error

    while (1) {
        data = COM_Parse(data);
        if (!data)
            return;  // error

        if (com_token[0] == '}')
            break;  // end of worldspawn
        if (com_token[0] == '_')
            strcpy(key, com_token + 1);
        else
            strcpy(key, com_token);
        while (key[strlen(key) - 1] == ' ') // remove trailing spaces
            key[strlen(key) - 1] = 0;
        data = COM_Parse(data);
        if (!data)
            return;  // error

        strcpy(value, com_token);

        if (!strcmp("fog", key)) {
            sscanf(value, "%f %f %f %f %f", &r_refdef.fog_start, &r_refdef.fog_end, &r_refdef.fog_red,
              &r_refdef.fog_green, &r_refdef.fog_blue);
        }
    }

    if (r_refdef.fog_start <= 0.01 || r_refdef.fog_end <= 0.01) {
        r_refdef.fog_start = 0.0f;
        r_refdef.fog_end   = 0.0f;
    }
} /* Fog_ParseWorldspawn */

/**
 * @brief Called at the start of each frame during render sequence.
 * Handles updating dynamic fog.
 * @param is_world_geometry TRUE if we are calling this during world rendering,
 * for special handling per-platform if necessary.
 */
void
Fog_SetupFrame(bool is_world_geometry)
{
    float c[4];
    float f, s, e;

    if (fade_done > cl.time) {
        f    = (fade_done - cl.time) / fade_time;
        s    = f * old_start + (1.0 - f) * r_refdef.fog_start;
        e    = f * old_end + (1.0 - f) * r_refdef.fog_end;
        c[0] = f * old_red + (1.0 - f) * r_refdef.fog_red;
        c[1] = f * old_green + (1.0 - f) * r_refdef.fog_green;
        c[2] = f * old_blue + (1.0 - f) * r_refdef.fog_blue;
        c[3] = r_skyfog.value;
    } else {
        s    = r_refdef.fog_start;
        e    = r_refdef.fog_end;
        c[0] = r_refdef.fog_red;
        c[1] = r_refdef.fog_green;
        c[2] = r_refdef.fog_blue;
        c[3] = 1.0;
        c[3] = r_skyfog.value;
    }

    if (e == 0)
        e = -1;

    Platform_Fog_Set(is_world_geometry, s, e, c[0], c[1], c[2], c[3]);

    if (s == 0 || e < 0) {
        Platform_Fog_Disable();
    }
}

/**
 * @brief Used for flat (texture-less) skies and for `r_skyfog`.
 * Sets appropriate texture mode and sets renderer color to
 * an approximation from fog value.
 */
void
Fog_SetColorForSkyS(void)
{
    if (r_refdef.fog_end > 0.0f && r_skyfog.value) {
        float a = r_refdef.fog_end * 0.00025f;
        float r = r_refdef.fog_red * 0.01f + (a * 0.25f);
        float g = r_refdef.fog_green * 0.01f + (a * 0.25f);
        float b = r_refdef.fog_blue * 0.01f + (a * 0.25f);

        if (a > 1.0f)
            a = 1.0f;
        if (r > 1.0f)
            r = 1.0f;
        if (g > 1.0f)
            g = 1.0f;
        if (b > 1.0f)
            b = 1.0f;

        Platform_Graphics_SetTextureMode(GFX_REPLACE);
        Platform_Graphics_Color(r, g, b, a);
    }
}

/**
 * @brief Used for flat (texture-less) skies and for `r_skyfog`.
 * Sets appropriate texture mode and sets renderer color back to
 * default.
 */
void
Fog_SetColorForSkyE(void)
{
    if (r_refdef.fog_end > 0.0f && r_skyfog.value) {
        Platform_Graphics_SetTextureMode(GFX_REPLACE);
        Platform_Graphics_Color(1, 1, 1, 1);
    }
}

/**
 * @brief Gets starting value of fog, taking into account
 * transition.
 * @returns Current fog start float.
 */
float
Fog_GetStart(void)
{
    float f;

    if (fade_done > cl.time) {
        f = (fade_done - cl.time) / fade_time;
        return f * old_start + (1.0 - f) * r_refdef.fog_start;
    } else
        return r_refdef.fog_start;
}

/**
 * @brief Gets ending value of fog, taking into account
 * transition.
 * @returns Current fog end float.
 */
float
Fog_GetEnd(void)
{
    float f;

    if (fade_done > cl.time) {
        f = (fade_done - cl.time) / fade_time;
        return f * old_start + (1.0 - f) * r_refdef.fog_end;
    } else
        return r_refdef.fog_end;
}

/**
 * @brief Enables fogging mode if necessary.
 */
void
Fog_EnableGFog(void)
{
    if (!(Fog_GetStart() == 0) || !(Fog_GetEnd() <= 0)) {
        Platform_Fog_Enable();
    }
}

/**
 * @brief Disables fogging mode if necessary.
 */
void
Fog_DisableGFog(void)
{
    if (!(Fog_GetStart() == 0) || !(Fog_GetEnd() <= 0)) {
        Platform_Fog_Disable();
    }
}

// MARK: Fog Initialization

/**
 * @brief Called at startup, registers fog command and
 * inits graphics functions.
 */
void
Fog_Init(void)
{
    Cmd_AddCommand("fog", Fog_FogCommand_f);

    // set up global fog
    r_refdef.fog_start = 0;
    r_refdef.fog_end   = -1;
    r_refdef.fog_red   = 0.5;
    r_refdef.fog_green = 0.5;
    r_refdef.fog_blue  = 0.5;
    fade_time = 1;

    Platform_Fog_Init();
}
