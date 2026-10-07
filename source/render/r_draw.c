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

// Shared batched 2D drawing.
// Hyena owns texture storage and ordered quad submission.

#include "../nzportable_def.h"

#define DRAW2D_MAX_QUADS 2048

int char_texture;
int font_kerningamount[96];

static hyena_2d_quad_t draw2d_quads[DRAW2D_MAX_QUADS];
static int draw2d_count;
static int draw2d_texture = -1;
static unsigned int draw2d_flags;

static void
Draw_InitKerningMap(void)
{
    byte * kerning_map;
    char buffer[1024];
    int length;
    int i;

    for (i = 0; i < 96; ++i)
        font_kerningamount[i] = 8;

    kerning_map = COM_LoadTempFile("gfx/kerning_map.txt");
    if (!kerning_map)
        return;

    length = com_filesize < (int) sizeof(buffer) - 1 ? com_filesize : (int) sizeof(buffer) - 1;
    memcpy(buffer, kerning_map, length);
    buffer[length] = '\0';
    {
        char * token = strtok(buffer, ",");
        for (i = 0; token && i < 96; ++i) {
            font_kerningamount[i] = atoi(token);
            token = strtok(NULL, ",");
        }
    }
}

int ref_texture;
int nonetexture;

static byte nontexdt[8][8] = {
    { 0x43, 0x43, 0x43, 0x43, 0x53, 0x53, 0x53, 0x53 },
    { 0x43, 0x43, 0x43, 0x43, 0x53, 0x53, 0x53, 0x53 },
    { 0x43, 0x43, 0x43, 0x43, 0x53, 0x53, 0x53, 0x53 },
    { 0x43, 0x43, 0x43, 0x43, 0x53, 0x53, 0x53, 0x53 },

    { 0x53, 0x53, 0x53, 0x53, 0x43, 0x43, 0x43, 0x43 },
    { 0x53, 0x53, 0x53, 0x53, 0x43, 0x43, 0x43, 0x43 },
    { 0x53, 0x53, 0x53, 0x53, 0x43, 0x43, 0x43, 0x43 },
    { 0x53, 0x53, 0x53, 0x53, 0x43, 0x43, 0x43, 0x43 },
};

// from Quake3

#define DLIGHT_SIZE 16
static void
R_CreateDlightImage(void)
{
    int x, y;
    byte data[DLIGHT_SIZE][DLIGHT_SIZE];
    int b;

    for (x = 0; x < DLIGHT_SIZE; x++) {
        for (y = 0; y < DLIGHT_SIZE; y++) {
            float d;

            d = (DLIGHT_SIZE / 2 - 0.5f - x) * (DLIGHT_SIZE / 2 - 0.5f - x)
              + (DLIGHT_SIZE / 2 - 0.5f - y) * (DLIGHT_SIZE / 2 - 0.5f - y);
            b = 4000 / d;

            if (b > 255) {
                b = 255;
            } else if (b < 75) {
                b = 0;
            }

            data[y][x] = b;
        }
    }
    ref_texture = Image_LoadTexture("reftexture", DLIGHT_SIZE, DLIGHT_SIZE, (byte *) data, HYE_TEXTURE_INDEX8,
        HYE_FILTER_LINEAR, 0, true, true, false);
}

void
R_InitFallbackTextures(void)
{
    nonetexture = Image_LoadTexture(
        "nonetexture", 8, 8, (byte *) nontexdt, HYE_TEXTURE_INDEX8, HYE_FILTER_NEAREST, 0, true, true, false);
    Hyena_KeepTexture(nonetexture);
    R_CreateDlightImage();
}

void
Draw_Init(void)
{
    Hyena_InitTextures();
    char_texture = Image_LoadImage("gfx/charset", IMAGE_TGA, HYE_FILTER_NEAREST, true, false);
    if (char_texture < 0)
        Sys_Error("Could not load gfx/charset; verify the game data is installed correctly\n");
    Clear_LoadingFill();
    Draw_InitKerningMap();
}

static byte
Draw_ClampColor(float value)
{
    if (value <= 0.0f)
        return 0;

    if (value >= 255.0f)
        return 255;

    return (byte) value;
}

void
Draw_Flush(void)
{
    if (!draw2d_count)
        return;

    Hyena_Draw2DQuads(draw2d_quads, draw2d_count, draw2d_texture, draw2d_flags);
    draw2d_count   = 0;
    draw2d_texture = -1;
    draw2d_flags   = 0;
}

static void
Draw_QueueQuad(int texture, unsigned int flags, float x0, float y0, float x1, float y1, float u0, float v0, float u1,
  float v1, float r, float g, float b, float a)
{
    hyena_2d_quad_t * quad;

    if (draw2d_count && (draw2d_texture != texture || draw2d_flags != flags))
        Draw_Flush();
    if (draw2d_count == DRAW2D_MAX_QUADS)
        Draw_Flush();

    if (!draw2d_count) {
        draw2d_texture = texture;
        draw2d_flags   = flags;
    }

    quad     = &draw2d_quads[draw2d_count++];
    quad->x0 = x0;
    quad->y0 = y0;
    quad->x1 = x1;
    quad->y1 = y1;
    quad->u0 = u0;
    quad->v0 = v0;
    quad->u1 = u1;
    quad->v1 = v1;
    quad->r  = Draw_ClampColor(r);
    quad->g  = Draw_ClampColor(g);
    quad->b  = Draw_ClampColor(b);
    quad->a  = Draw_ClampColor(a);
}

static void
Draw_GetTextureSize(int texture, int * width, int * height, int * original_width, int * original_height)
{
    Hyena_GetTextureSize(texture, width, height, original_width, original_height);
    if (*original_width <= 0)
        *original_width = *width;
    if (*original_height <= 0)
        *original_height = *height;
}

void
Draw_Character(int x, int y, int num)
{
    int row, col;

    if (num == 32 || y <= -8)
        return;

    num &= 255;
    row  = num >> 4;
    col  = num & 15;
    Draw_QueueQuad(char_texture, HYE_2D_TEXTURED | HYE_2D_BLEND, x, y, x + 8, y + 8, col * (1.0f / 16.0f),
      row * (1.0f / 16.0f), (col + 1) * (1.0f / 16.0f), (row + 1) * (1.0f / 16.0f), 255, 255, 255, 255);
}

void
Draw_CharacterRGBA(int x, int y, int num, float r, float g, float b, float a, float scale)
{
    int row, col;

    if (num == 32 || y <= -8)
        return;

    num &= 255;
    row  = num >> 4;
    col  = num & 15;
    Draw_QueueQuad(char_texture, HYE_2D_TEXTURED | HYE_2D_BLEND, x, y, x + 8.0f * scale, y + 8.0f * scale,
      col * (1.0f / 16.0f), row * (1.0f / 16.0f), (col + 1) * (1.0f / 16.0f), (row + 1) * (1.0f / 16.0f), r, g, b, a);
}

void
Draw_String(int x, int y, char * str)
{
    Draw_ColoredString(x, y, str, 255, 255, 255, 255, 1);
}

void
Draw_ColoredString(int x, int y, char * str, float r, float g, float b, float a, float scale)
{
    int scale_int = (int) rintf(scale);

    if (!str || y <= -8)
        return;

    if (scale_int < 1)
        scale_int = 1;

    while (*str) {
        Draw_CharacterRGBA(x, y, (byte) * str, r, g, b, a, scale_int);
        if (*str == ' ')
            x += 4 * scale_int;
        else if ((byte) * str < 33 || (byte) * str > 126)
            x += 8 * scale_int;
        else
            x += (font_kerningamount[(byte) * str - 33] + 1) * scale_int;
        ++str;
    }
}

int
getTextWidth(char * str, float scale)
{
    int width     = 0;
    int scale_int = (int) scale;

    if (!str)
        return 0;

    if (scale_int < 1)
        scale_int = 1;
    while (*str) {
        if (*str == ' ')
            width += 4 * scale_int;
        else if ((byte) * str < 33 || (byte) * str > 126)
            width += 8 * scale_int;
        else
            width += (font_kerningamount[(byte) * str - 33] + 1) * scale_int;
        ++str;
    }
    return width;
}

void
Draw_ColoredStringCentered(int y, char * str, float r, float g, float b, float a, float scale)
{
    Draw_ColoredString((vid.width - getTextWidth(str, scale)) / 2, y, str, r, g, b, a, scale);
}

void
Draw_DebugChar(char num)
{
    (void) num;
}

void
Draw_ColorPic(int x, int y, int pic, float r, float g, float b, float a)
{
    int width, height, original_width, original_height;

    if (pic < 0)
        return;

    Draw_GetTextureSize(pic, &width, &height, &original_width, &original_height);
    Draw_QueueQuad(
        pic, HYE_2D_TEXTURED | HYE_2D_BLEND, x, y, x + original_width, y + original_height, 0, 0, 1, 1, r, g, b, a);
}

void
Draw_AlphaPic(int x, int y, int pic, float alpha)
{
    Draw_ColorPic(x, y, pic, 255, 255, 255, alpha);
}

void
Draw_Pic(int x, int y, int pic)
{
    Draw_ColorPic(x, y, pic, 255, 255, 255, 255);
}

void
Draw_ColoredStretchPic(int x, int y, int pic, int width, int height, int r, int g, int b, int a)
{
    if (pic < 0)
        return;

    Draw_QueueQuad(pic, HYE_2D_TEXTURED | HYE_2D_BLEND, x, y, x + width, y + height, 0, 0, 1, 1, r, g, b, a);
}

void
Draw_StretchPic(int x, int y, int pic, int width, int height)
{
    Draw_ColoredStretchPic(x, y, pic, width, height, 255, 255, 255, 255);
}

void
Draw_MenuPanningPic(int x, int y, int pic, int width, int height, float time)
{
    const float zoom    = 0.05f;
    const float visible = 1.0f - zoom * 2.0f;
    float progress;

    if (pic < 0)
        return;

    progress = time / 7.0f;
    if (progress > 1.0f)
        progress = 1.0f;
    if (progress < 0.0f)
        progress = 0.0f;
    Draw_QueueQuad(pic, HYE_2D_TEXTURED | HYE_2D_BLEND | HYE_2D_LINEAR, x, y, x + width, y + height,
      progress * zoom * 2.0f, 0, progress * zoom * 2.0f + visible, 1, 255, 255, 255, 255);
}

void
Draw_SubPic(int x, int y, int pic, float s, float t, float s_coord_size, float t_coord_size, float scale, float r,
  float g, float b, float a)
{
    int width, height, original_width, original_height;
    float width_scale;

    if (pic < 0 || t_coord_size == 0.0f)
        return;

    Draw_GetTextureSize(pic, &width, &height, &original_width, &original_height);
    width_scale = scale * (s_coord_size / t_coord_size);
    Draw_QueueQuad(pic, HYE_2D_TEXTURED | HYE_2D_BLEND | HYE_2D_LINEAR, x, y, x + original_width * width_scale,
      y + original_height * scale, s, t, s + s_coord_size, t + t_coord_size, r, g, b, a);
}

void
Draw_TransPic(int x, int y, int pic)
{
    int width, height, original_width, original_height;

    if (pic < 0)
        return;

    Draw_GetTextureSize(pic, &width, &height, &original_width, &original_height);
    if (x < 0 || (unsigned) (x + original_width) > vid.width || y < 0 || (unsigned) (y + original_height) > vid.height)
        Sys_Error("bad coordinates");
    Draw_Pic(x, y, pic);
}

void
Draw_FillByColor(int x, int y, int width, int height, int r, int g, int b, int a)
{
    Draw_QueueQuad(-1, HYE_2D_BLEND, x, y, x + width, y + height, 0, 0, 0, 0, r, g, b, a);
}

void
Draw_Fill(int x, int y, int width, int height, int color)
{
    Draw_FillByColor(
        x, y, width, height, host_basepal[color * 3], host_basepal[color * 3 + 1], host_basepal[color * 3 + 2], 255);
}

void
Draw_BlackBackground(void)
{
    Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 255);
    Draw_Flush();
}

void
Draw_TileClear(int x, int y, int width, int height)
{
    (void) x;
    (void) y;
    (void) width;
    (void) height;
}

void
Draw_ConsoleBackground(int lines)
{
    Draw_FillByColor(0, 0, vid.width, lines, 0, 0, 0, 255);
}

void
Draw_LoadingFill(void)
{
    LoadingScreen_DrawProgressBar();
}

void
Clear_LoadingFill(void)
{
    LoadingScreen_ClearProgress();
}

void
Draw_FadeScreen(void)
{
    Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 128);
}

void
Draw_Set2D(void)
{
    Draw_Flush();
    Hyena_Set2D();
}

byte *
StringToRGB(char * string)
{
    static byte rgb[4];
    byte * color;

    Cmd_TokenizeString(string);
    if (Cmd_Argc() == 3) {
        rgb[0] = Q_atoi(Cmd_Argv(0));
        rgb[1] = Q_atoi(Cmd_Argv(1));
        rgb[2] = Q_atoi(Cmd_Argv(2));
    } else {
        color  = (byte *) &d_8to24table[(byte) Q_atoi(string)];
        rgb[0] = color[0];
        rgb[1] = color[1];
        rgb[2] = color[2];
    }
    rgb[3] = 255;
    return rgb;
}
