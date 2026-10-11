// Copyright (C) 1996-1997 Id Software, Inc.
// Copyright (C) 2026 NZ:P Team
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

// r_sky.c -- Shared depth-masked skybox rendering

#include "../nzportable_def.h"

static qboolean skybox_enabled = true;
static qboolean sky_preserve_alpha_test;
static qboolean sky_reset_blend = true;

void
Sky_Configure(qboolean enabled, qboolean preserve_alpha_test, qboolean reset_blend)
{
    skybox_enabled = enabled;
    sky_preserve_alpha_test = preserve_alpha_test;
    sky_reset_blend         = reset_blend;
}

static void
R_BeginSky(void)
{
    Hyena_DisableCapability(HYE_BLEND);
    if (!sky_preserve_alpha_test)
        Hyena_DisableCapability(HYE_ALPHA_TEST);
    Hyena_SetTextureMode(HYE_REPLACE);
    // Draw the cube once behind the world, without leaving depth behind.
    Hyena_DepthMask(false);
    Hyena_DisableCapability(HYE_DEPTH_TEST);
}

static void
R_DrawSkyFace(int texture, const vertex_t * vertices)
{
    hyena_arrays_t arrays = { 0 };

    if (texture >= 0) {
        Hyena_EnableCapability(HYE_TEXTURE_2D);
        Hyena_BindTexture(texture);
    } else {
        Hyena_DisableCapability(HYE_TEXTURE_2D);
    }
    arrays.position.data   = &vertices[0].xyz;
    arrays.position.stride = sizeof(*vertices);
    arrays.count = 4;
    // Deferred backends copy stack vertices and perform any required polygon clipping.
    arrays.flags = HYE_CLIP_POLYGON | HYE_COPY_VERTICES;
    if (texture >= 0 || !Hyena_SeparateArrays())
        arrays.uv.data = &vertices[0].uv;
    arrays.uv.stride = sizeof(*vertices);
    Hyena_DrawArrays(HYE_QUADS, &arrays, 4);
}

static void
R_DrawSkyFog(const sky_fog_vertex_t * vertices, int quad_count)
{
    hyena_arrays_t arrays = { 0 };

    Hyena_DisableCapability(HYE_TEXTURE_2D);
    if (sky_reset_blend)
        Hyena_SetBlendFunction(HYE_SRC_ALPHA, HYE_ONE_MINUS_SRC_ALPHA);
    else
        Hyena_SetTextureMode(HYE_REPLACE);
    Hyena_SetShadeMode(HYE_SMOOTH);
    Hyena_EnableCapability(HYE_BLEND);
    arrays.position.data   = vertices[0].xyz;
    arrays.color.data      = vertices[0].color;
    arrays.position.stride = arrays.color.stride = sizeof(*vertices);
    arrays.color.type      = HYE_UBYTE;
    arrays.count = quad_count * 4;
    arrays.flags = HYE_COPY_VERTICES;
    Hyena_DrawArrays(HYE_QUADS, &arrays, quad_count * 4);
}

static void
R_EndSky(void)
{
    // Color arrays changed fixed-function color, not the default color for later arrays.
    Hyena_SetCurrentColor(1, 1, 1, 1);
    Hyena_EnableCapability(HYE_TEXTURE_2D);
    Hyena_DisableCapability(HYE_BLEND);
    Hyena_SetTextureMode(HYE_MODULATE);
    Hyena_DepthMask(true);
    Hyena_EnableCapability(HYE_DEPTH_TEST);
}

#define SKY_DEPTH     256.0f
#define SKY_FACE_SIZE 256.0f

static int skyimage[5];
char skybox_name[32] = "";
int skytexturenum    = -1;
cvar_t r_skyfogblend = { "r_skyfogblend", "0.6", true };

static const int skytexorder[5]   = { 0, 2, 1, 3, 4 };
static const char * skysuffix[5]  = { "rt", "bk", "lf", "ft", "up" };
static const vec3_t skynormals[5] = {
    { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }
};
static const vec3_t skyright[5] = {
    { 0, -1, 0 }, { 0, 1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, -1, 0 }
};
static const vec3_t skyup[5] = {
    { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { -1, 0, 0 }
};

static void
Sky_Unload(void)
{
    int i;

    for (i = 0; i < 5; ++i) {
        if (skyimage[i] >= 0)
            Hyena_DestroyTexture(skyimage[i]);
        skyimage[i] = -1;
    }
}

void
Sky_ClearTextures(void)
{
    int i;

    for (i = 0; i < 5; ++i)
        skyimage[i] = -1;
    skybox_name[0] = 0;
}

static int
Sky_LoadFace(const char * name, int face)
{
    int image;

    image = Image_LoadImage(va("gfx/env/%s%s", name, skysuffix[face]),
        IMAGE_TGA | IMAGE_PNG | IMAGE_JPG, 0, false, false);
    if (image < 0) {
        image = Image_LoadImage(va("gfx/env/%s_%s", name, skysuffix[face]),
            IMAGE_TGA | IMAGE_PNG | IMAGE_JPG, 0, false, false);
    }
    if (image >= 0)
        return image;

    Con_Printf("Sky: %s[%s] not found, used std\n", name, skysuffix[face]);
    image = Image_LoadImage(va("gfx/env/skybox%s", skysuffix[face]),
        IMAGE_TGA | IMAGE_PNG | IMAGE_JPG, 0, false, false);
    if (image < 0)
        Sys_Error("STD SKY NOT FOUND!");
    return image;
}

void
Sky_LoadSkyBox(char * name)
{
    int i, mark;

    if (!strcmp(skybox_name, name))
        return;

    Sky_Unload();
    if (!name[0] || name[0] == '0') {
        skybox_name[0] = 0;
        return;
    }
    if (!skybox_enabled) {
        skybox_name[0] = 0;
        return;
    }
    for (i = 0; i < 5; ++i) {
        mark        = Hunk_LowMark();
        skyimage[i] = Sky_LoadFace(name, i);
        Hunk_FreeToLowMark(mark);
    }
    snprintf(skybox_name, sizeof(skybox_name), "%s", name);
}

void
Sky_NewMap(void)
{
    char key[128], value[4096];
    char * data;
    int i;

    Sky_LoadSkyBox("");
    skytexturenum = -1;
    for (i = 0; i < cl.worldmodel->numtextures; ++i) {
        if (cl.worldmodel->textures[i] && !strncmp(cl.worldmodel->textures[i]->name, "sky", 3))
            skytexturenum = i;
    }
    data = cl.worldmodel->entities;
    if (!data || !(data = COM_Parse(data)) || com_token[0] != '{')
        return;

    while ((data = COM_Parse(data)) != NULL) {
        if (com_token[0] == '}')
            break;
        snprintf(key, sizeof(key), "%.*s", (int) sizeof(key) - 1,
          com_token[0] == '_' ? com_token + 1 : com_token);
        while (key[0] == ' ')
            memmove(key, key + 1, strlen(key));
        while (key[0] && key[strlen(key) - 1] == ' ')
            key[strlen(key) - 1] = 0;
        if (!(data = COM_Parse(data)))
            return;

        snprintf(value, sizeof(value), "%s", com_token);
        if (!strcmp(key, "sky") || !strcmp(key, "skyname") || !strcmp(key, "qlsky"))
            Sky_LoadSkyBox(value);
        else if (!strcmp(key, "r_skycolor"))
            Cvar_Set("r_skycolor", value);
    }
} /* Sky_NewMap */

static void
Sky_Command(void)
{
    if (Cmd_Argc() == 1)
        Con_Printf("\"sky\" is \"%s\"\n", skybox_name);
    else if (Cmd_Argc() == 2)
        Sky_LoadSkyBox(Cmd_Argv(1));
    else
        Con_Printf("usage: sky <skyname>\n");
}

void
Sky_Init(void)
{
    Cmd_AddCommand("sky", Sky_Command);
    Cvar_RegisterVariable(&r_skyfogblend);
    Sky_ClearTextures();
}

static void
Sky_SetVertex(vertex_t * vertex, int face, float right, float up, float u, float v)
{
    vertex->uv.u  = u;
    vertex->uv.v  = v;
    vertex->xyz.x = r_origin[0]
      + (0.99f * skynormals[face][0] + right * skyright[face][0] + up * skyup[face][0]) * SKY_DEPTH;
    vertex->xyz.y = r_origin[1]
      + (0.99f * skynormals[face][1] + right * skyright[face][1] + up * skyup[face][1]) * SKY_DEPTH;
    vertex->xyz.z = r_origin[2]
      + (0.99f * skynormals[face][2] + right * skyright[face][2] + up * skyup[face][2]) * SKY_DEPTH;
}

static void
Sky_DrawFaces(void)
{
    const float low  = 0.5f / SKY_FACE_SIZE;
    const float high = (SKY_FACE_SIZE - 0.5f) / SKY_FACE_SIZE;
    vertex_t vertices[4];
    byte * color;
    int face, texture;

    color = StringToRGB(r_skycolor.string);
    Hyena_SetColor(color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f, 1);
    for (face = 0; face < 5; ++face) {
        if (DotProduct(skynormals[face], vpn) < -0.25f)
            continue;
        Sky_SetVertex(&vertices[0], face, -1, -1, low, high);
        Sky_SetVertex(&vertices[1], face, -1, 1, low, low);
        Sky_SetVertex(&vertices[2], face, 1, 1, high, low);
        Sky_SetVertex(&vertices[3], face, 1, -1, high, high);
        texture = skybox_name[0] ? skyimage[skytexorder[face]] : -1;
        R_DrawSkyFace(texture, vertices);
    }
}

static byte
Sky_FogComponent(float value)
{
    return (byte) (bound(0, value * 2.55f, 255));
}

static void
Sky_DrawFog(void)
{
    sky_fog_vertex_t vertices[32];
    vec3_t angles = { 0, r_refdef.viewangles[YAW], 0 };
    vec3_t forward, right;
    float endheight, startheight, forwardamount, forwardamount2;
    byte red, green, blue;
    int i, j, vertex = 0;

    if (r_skyfogblend.value <= 0 || (r_refdef.fog_start <= 0 && r_refdef.fog_end <= 0))
        return;

    endheight   = SKY_DEPTH * r_skyfogblend.value;
    startheight = MIN(SKY_DEPTH * 0.075f, endheight * 0.3f);
    red         = Sky_FogComponent(r_refdef.fog_red);
    green       = Sky_FogComponent(r_refdef.fog_green);
    blue        = Sky_FogComponent(r_refdef.fog_blue);
    AngleVectors(angles, forward, right, NULLVEC);
    for (i = -2; i < 2; ++i) {
        forwardamount  = SKY_DEPTH * (0.7f - i * i * 0.15f);
        forwardamount2 = SKY_DEPTH * (0.7f - (i + 1) * (i + 1) * 0.15f);
        for (j = 0; j < 2; ++j) {
            float bottom = j ? startheight : -SKY_DEPTH;
            float top    = j ? endheight : startheight;
            int k;

            for (k = 0; k < 4; ++k) {
                vertices[vertex + k].color[0] = red;
                vertices[vertex + k].color[1] = green;
                vertices[vertex + k].color[2] = blue;
                vertices[vertex + k].color[3] = !j || k == 0 || k == 3 ? 255 : 0;
            }
            VectorMA(r_origin, forwardamount, forward, vertices[vertex].xyz);
            VectorMA(vertices[vertex].xyz, i * SKY_DEPTH, right, vertices[vertex].xyz);
            vertices[vertex].xyz[2] += bottom;
            VectorCopy(vertices[vertex].xyz, vertices[vertex + 1].xyz);
            vertices[vertex + 1].xyz[2] += top - bottom;
            VectorMA(r_origin, forwardamount2, forward, vertices[vertex + 2].xyz);
            VectorMA(vertices[vertex + 2].xyz, (i + 1) * SKY_DEPTH, right, vertices[vertex + 2].xyz);
            vertices[vertex + 2].xyz[2] += top;
            VectorCopy(vertices[vertex + 2].xyz, vertices[vertex + 3].xyz);
            vertices[vertex + 3].xyz[2] += bottom - top;
            vertex += 4;
        }
    }
    R_DrawSkyFog(vertices, 8);
} /* Sky_DrawFog */

void
R_DrawSkyBox(void)
{
    if (skytexturenum < 0)
        return;

    Fog_DisableGFog();
    Fog_SetColorForSkyS();
    R_BeginSky();
    Sky_DrawFaces();
    Sky_DrawFog();
    R_EndSky();
    Fog_SetColorForSkyE();
    Fog_EnableGFog();
}
