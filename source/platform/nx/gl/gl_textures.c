/*
Copyright (C) 1996-1997 Id Software, Inc.

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

#include "../../../nzportable_def.h"

// FIXME - should I let this get larger, with view to enhancements?
cvar_t gl_max_size = {"gl_max_size", "1024"};

static cvar_t gl_nobind = {"gl_nobind", "0"};
static cvar_t gl_picmip = {"gl_picmip", "0"};

int	gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int	gl_filter_max = GL_LINEAR;

int gl_lightmap_format = GL_RGBA; // 4
int gl_solid_format = GL_RGB;     // 3
int gl_alpha_format = GL_RGBA;    // 4

gltexture_t gltextures[MAX_GLTEXTURES];
static int numgltextures = 0;
static GLuint current_gl_id = 0;

void GL_Bind (int texnum)
{
	if (texnum < 0) {
        Con_DPrintf("GL_Bind: invalid texnum %d\n", texnum);
        return;
    }
    gltexture_t *glt = &gltextures[texnum];
    if (!glt->used) {
        Con_DPrintf("GL_Bind: unused texnum %d\n", texnum);
        return;
    }

    if (current_gl_id != glt->gl_id) {
        glBindTexture(GL_TEXTURE_2D, glt->gl_id);
        current_gl_id = glt->gl_id;
    }
}

void GL_FreeTextures (int texnum)
{
	if (texnum <= 0) return;

	gltexture_t *glt = &gltextures[texnum];
	if (glt->used == false) return;
	if (glt->keep) return;

	glDeleteTextures(1, &glt->gl_id);
	glt->gl_id = 0;
	glt->texnum = texnum;
	strcpy(glt->identifier, "");
	glt->width = 0;
	glt->height = 0;
	glt->original_width = 0;
	glt->original_height = 0;
	glt->bpp = 0;
	glt->keep = false;
	glt->used = false;

	numgltextures--;
	current_gl_id = 0;
}

void GL_UnloadTextures (void)
{
	for (int i = 0; i < (MAX_GLTEXTURES); i++) {
		GL_FreeTextures(i);
	}
}

typedef struct
{
	char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
===============
Draw_TextureMode_f
===============
*/
void Draw_TextureMode_f (void)
{
	int		i;
	gltexture_t	*glt;

	if (Cmd_Argc() == 1)
	{
		for (i=0 ; i< 6 ; i++)
			if (gl_filter_min == modes[i].minimize)
			{
				Con_Printf ("%s\n", modes[i].name);
				return;
			}
		Con_Printf ("current filter is unknown???\n");
		return;
	}

	for (i=0 ; i< 6 ; i++)
	{
		if (!Q_strcasecmp (modes[i].name, Cmd_Argv(1) ) )
			break;
	}
	if (i == 6)
	{
		Con_Printf ("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (glt->mipmap)
		{
			GL_Bind (glt->texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

/*
================
GL_FindTexture
================
*/
int GL_FindTexture(const char *identifier)
{
    for (int i = 0; i < MAX_GLTEXTURES; i++) {
        if (gltextures[i].used && strcmp(gltextures[i].identifier, identifier) == 0) {
            return i;  // return array index directly
        }
    }
    return -1;
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (uint32_t *in, int inwidth, int inheight, uint32_t *out,  int outwidth, int outheight)
{
	int		i, j;
	uint32_t	*inrow;
	uint32_t	frac, fracstep;

	fracstep = inwidth*0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j+=4)
		{
			out[j] = inrow[frac>>16];
			frac += fracstep;
			out[j+1] = inrow[frac>>16];
			frac += fracstep;
			out[j+2] = inrow[frac>>16];
			frac += fracstep;
			out[j+3] = inrow[frac>>16];
			frac += fracstep;
		}
	}
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap (byte *in, int width, int height)
{
	int		i, j;
	byte	*out;

	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}


/*
===============
GL_Upload32
===============
*/
void GL_Upload32(GLuint gl_id, uint32_t *data, int width, int height, qboolean mipmap, qboolean alpha)
{
    int scaled_width = 1, scaled_height = 1;
    while (scaled_width < width) scaled_width <<= 1;
    while (scaled_height < height) scaled_height <<= 1;

    scaled_width >>= (int)gl_picmip.value;
    scaled_height >>= (int)gl_picmip.value;

    if (scaled_width > gl_max_size.value) scaled_width = gl_max_size.value;
    if (scaled_height > gl_max_size.value) scaled_height = gl_max_size.value;

    if (scaled_width < 1) scaled_width = 1;
    if (scaled_height < 1) scaled_height = 1;

    uint32_t *scaled = malloc(scaled_width * scaled_height * sizeof(uint32_t));
    if (!scaled) Sys_Error("GL_Upload32: out of memory");

    // bind the texture
    glBindTexture(GL_TEXTURE_2D, gl_id);
	current_gl_id = gl_id; //update texture cache

    if (scaled_width != width || scaled_height != height)
        GL_ResampleTexture(data, width, height, scaled, scaled_width, scaled_height);
    else
        memcpy(scaled, data, width * height * 4);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);

    // mipmap generation (if enabled)
	
    if (mipmap) {
        int level = 0;
        int w = scaled_width, h = scaled_height;
        while (w > 1 || h > 1) {
            GL_MipMap((byte*)scaled, w, h);
            w = w > 1 ? w >> 1 : 1;
            h = h > 1 ? h >> 1 : 1;
            level++;
            glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
        }
    }

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    free(scaled);
}

/*
===============
GL_Upload8
===============
*/
void GL_Upload8 (GLuint gl_id, byte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, s;
	qboolean	noalpha;
	int			p;
	uint32_t *trans = malloc(width*height*sizeof(uint32_t));

	s = width*height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha) {
		noalpha = true;
		for (i=0 ; i<s ; i++) {
			p = data[i];
			if (p == 255)
				noalpha = false;
			trans[i] = d_8to24table[p];
		}

		if (alpha && noalpha)
			alpha = false;
	} else {
		if (s&3)
			Sys_Error ("s&3");
		for (i=0 ; i<s ; i+=4) {
			trans[i] = d_8to24table[data[i]];
			trans[i+1] = d_8to24table[data[i+1]];
			trans[i+2] = d_8to24table[data[i+2]];
			trans[i+3] = d_8to24table[data[i+3]];
		}
	}

	GL_Upload32 (gl_id, trans, width, height, mipmap, alpha);

	free(trans);
}

int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, int bytesperpixel, qboolean keep)
{
	int texture_index = GL_FindTexture(identifier);
	if (texture_index >= 0) {
		return texture_index;
	}

	int i;
	gltexture_t	*glt;

	texture_index = -1;
	for (i=0, glt=gltextures; i<MAX_GLTEXTURES; i++, glt++) {
		if (glt->used == false) {
			texture_index = i;
			break;
		}
	}

	if (numgltextures == MAX_GLTEXTURES) {
		Sys_Error("GL_LoadTexture: Out of GL textures\n");
	}

	if (texture_index < 0) {
		Sys_Error("Could not find a free GL texture!\n");
	}

    glt = &gltextures[texture_index];

	GLuint texID;
	glGenTextures(1, &texID);     // Ask GL for a real texture object

	strcpy(glt->identifier, identifier);
	glt->gl_id = texID;
	glt->texnum = texture_index;
	glt->width = width;
	glt->height = height;
	glt->original_width = width;
	glt->original_height = height;
	glt->bpp = bytesperpixel;
	glt->mipmap = mipmap;
	glt->keep = keep;
	glt->used = true;

	//Con_DPrintf("tex num %i tex name %s size %im\n", texture_index, identifier, (width*height*bytesperpixel)/1024);
	if (bytesperpixel == 1) {
		GL_Upload8 (glt->gl_id, data, width, height, mipmap, alpha);
	}
	else if (bytesperpixel == 4) {
		GL_Upload32 (glt->gl_id, (uint32_t*)data, width, height, mipmap, alpha);
	}
	else {
		Sys_Error("GL_LoadTexture: unknown bytesperpixel\n");
	}
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	numgltextures++;

	return texture_index;
}

int GL_LoadLMTexture (char *identifier, int width, int height, byte *data, qboolean update)
{
	int i;
	gltexture_t	*glt;

	int texture_index = -1;

	if (update == false) {

		for (i=0, glt=gltextures; i<MAX_GLTEXTURES; i++, glt++) {
			if (glt->used == false) {
				texture_index = i;
				break;
			}
		}

		if (numgltextures == MAX_GLTEXTURES) {
			Sys_Error("GL_LoadTexture: Out of GL textures\n");
		}

		if (texture_index < 0) {
			Sys_Error("Could not find a free GL texture!\n");
		}

		glt = &gltextures[texture_index];

		GLuint texID;
		glGenTextures(1, &texID);

		//Con_DPrintf("lm num %i lm name %s\n", texture_index, identifier);

		strcpy(glt->identifier, identifier);
		glt->gl_id = texID;
		glt->texnum = texture_index;
		glt->width = width;
		glt->height = height;
		glt->original_width = width;
		glt->original_height = height;
		glt->bpp = 4;
		glt->mipmap = false;
		glt->keep = false;
		glt->used = true;

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GL_Upload32 (glt->gl_id, (uint32_t*)data, width, height, false, false);

		numgltextures++;
	} else if (update == true) {
		texture_index = GL_FindTexture(identifier);

		if (texture_index < 0) Sys_Error("tried to upload inactive lightmap\n");

		glt = &gltextures[texture_index];

		if (width == glt->original_width && height == glt->original_height) {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			GL_Upload32 (glt->gl_id, (uint32_t*)data, width, height, false, false);
		}
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	return texture_index;
}

void GL_InitTextures(void) {
    GLint max_size;

	numgltextures++;

    Cvar_RegisterVariable(&gl_nobind);
    Cvar_RegisterVariable(&gl_max_size);
    Cvar_RegisterVariable(&gl_picmip);

    // FIXME - could do better to check on each texture upload with
    //         GL_PROXY_TEXTURE_2D
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
    if (gl_max_size.value > max_size) {
        Con_DPrintf("Reducing gl_max_size from %d to %d\n", (int)gl_max_size.value, max_size);
        Cvar_Set("gl_max_size", va("%d", max_size));
    }

    Cmd_AddCommand("gl_texturemode", Draw_TextureMode_f);
}
