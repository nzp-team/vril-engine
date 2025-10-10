/*
NZ:P Universal Image loading
Copyright (C) 2025

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

#include "nzportable_def.h"

#ifdef __PSP__
#include <pspgu.h>
#endif // __PSP__

int		    image_width;
int		    image_height;

/*
============
tex_filebase
============
*/
// create a unique identifier for each externally loaded texture based on filename
void tex_filebase (char *in, char *out)
{
	char *s, *s2;
	
	s = in + strlen(in) - 1;
	
	for (s2 = s ; s2 != in && *s2 && *s2 != '/' ; s2--) {
		;
	}
	
	if (s-s2 < 2) {
		Sys_Error("tex_filebase: bad input\n");
	} else {
		strncpy (out,s2+1, (s+1)-(s2));
		out[s-s2] = 0;
	}
}

/*
=================================================================
  PCX Loading
  mem fix by Crow_bar.
=================================================================
*/
#pragma pack(1)
typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned char	data;			// unbounded
} pcx_t;

byte* LoadPCX(FILE* f, int matchwidth, int matchheight) 
{
    pcx_t pcxbuf;
    fread(&pcxbuf, 1, sizeof(pcxbuf), f);

    pcx_t* pcx = &pcxbuf;

#ifdef __WII__
	pcx->xmin = LittleShort(pcx->xmin);
    pcx->ymin = LittleShort(pcx->ymin);
    pcx->xmax = LittleShort(pcx->xmax);
    pcx->ymax = LittleShort(pcx->ymax);
    pcx->hres = LittleShort(pcx->hres);
    pcx->vres = LittleShort(pcx->vres);
    pcx->bytes_per_line = LittleShort(pcx->bytes_per_line);
    pcx->palette_type = LittleShort(pcx->palette_type);
#endif // __WII__

    if (pcx->manufacturer != 0x0a || pcx->version != 5 || pcx->encoding != 1 ||
        pcx->bits_per_pixel != 8 || pcx->xmax >= 640 || pcx->ymax >= 480) {
		fclose(f);
        Sys_Error("Bad pcx file\n");
        return NULL;
    }

    if (matchwidth && (pcx->xmax + 1) != matchwidth) {
		Sys_Error("Matchwidth!\n");
        fclose(f);
        return NULL;
    }

    if (matchheight && (pcx->ymax + 1) != matchheight) {
		Sys_Error("Matchheight!\n");
        fclose(f);
        return NULL;
    }

    fseek(f, -768, SEEK_END);
    unsigned char palette[768];
    fread(palette, 1, 768, f);

    fseek(f, 128, SEEK_SET);

    int count = (pcx->xmax + 1) * (pcx->ymax + 1);
    byte* image_rgba = (unsigned char*)(Q_malloc(4 * count));
	byte* pix = image_rgba;

    for (int y = 0; y <= pcx->ymax; y++) {
        for (int x = 0; x <= pcx->xmax;) {
            int dataByte = fgetc(f);

            int runLength = 1;
            if ((dataByte & 0xC0) == 0xC0) {
                runLength = dataByte & 0x3F;
                dataByte = fgetc(f);
            }

            while (runLength-- > 0) {
                byte* color = palette + dataByte * 3;
                pix[0] = color[0];
                pix[1] = color[1];
                pix[2] = color[2];
                pix[3] = 0xFF;  // set alpha to 255
                pix += 4;
                x++;
            }
        }
    }

    image_width = pcx->xmax + 1;
    image_height = pcx->ymax + 1;

    fclose(f);
    return image_rgba;
}

/*small function to read files with stb_image - single-file image loader library.
** downloaded from: https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
** */
#define STBI_ONLY_TGA
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"
byte* LoadSTBI_Image(FILE *f)
{
	int bpp;
	int inwidth, inheight;
	byte* image = stbi_load_from_file(f, &inwidth, &inheight, &bpp, 4);

	if(image == NULL) {
		Sys_Error(" stbi failure: %s\n", stbi_failure_reason());
		return NULL;
	}

	image_width = inwidth;
	image_height = inheight;
	fclose(f);
	return image;
}

byte* Image_LoadPixels(char* filename, int image_format)
{
	FILE	*f;
	char name[256];

	if (image_format & IMAGE_PCX) {
		snprintf (name, sizeof(name), "%s.pcx", filename);
		if (COM_FOpenFile(name, &f) != -1)
			return LoadPCX (f, 0, 0);
	}

	if (image_format & IMAGE_TGA) {
		snprintf (name, sizeof(name), "%s.tga", filename);
		if (COM_FOpenFile(name, &f) != -1)
			return LoadSTBI_Image (f);
	}

	if (image_format & IMAGE_PNG) {
		snprintf (name, sizeof(name), "%s.png", filename);
		if (COM_FOpenFile(name, &f) != -1)
			return LoadSTBI_Image (f);
	}

	if (image_format & IMAGE_JPG) {
		snprintf (name, sizeof(name), "%s.jpg", filename);
		if (COM_FOpenFile(name, &f) != -1)
			return LoadSTBI_Image (f);

		snprintf (name, sizeof(name), "%s.jpeg", filename);
		if (COM_FOpenFile(name, &f) != -1)
			return LoadSTBI_Image (f);
	}

	return NULL;					
}

int Image_LoadImage(char* filename, int image_format, int filter, bool keep, bool mipmap)
{
	int texture_index;
	byte *data;
	char texname[32];

	// create a unique identifier
	tex_filebase (filename, texname);

	// does the texture already exist?
	texture_index = Image_FindImage(texname);
	if (texture_index >= 0) {
		return texture_index;
	}

	data = Image_LoadPixels (filename, image_format);

	if(data == NULL) {
		return 0;
	}
	
#ifdef __PSP__
	texture_index = GL_LoadImages (texname, image_width, image_height, data, true, filter, 0, 4, keep);
#elif __vita__
	texture_index = GL_LoadTexture (texname, image_width, image_height, data, mipmap, true, 4, keep);
#elif __3DS__
	texture_index = GL_LoadTexture (texname, image_width, image_height, data, mipmap, true, 4, keep);
#elif __WII__
	texture_index = GL_LoadTexture (texname, image_width, image_height, data, false, true, keep, 4);
#elif __NSPIRE__
	qboolean transparenttoblack = (qboolean)filter;
	texture_index = Soft_LoadTexture (texname, image_width, image_height, data, transparenttoblack, keep);
#endif

	if(texture_index < 0) {
		Sys_Error("Image_LoadImage: failed to upload texture %s\n", texname);
	}

	free(data);

	return texture_index;
}

// used on PSP
#ifdef __PSP__
/*
=============
loadrgbafrompal
=============
*/
int loadrgbafrompal (char* name, int width, int height, byte* data)
{
	int pixels = width * height;
	byte* rgbadata = (byte*)Q_malloc(pixels * 4);
	char texname[32];

	tex_filebase (name, texname);

	for(int i = 0; i < pixels; i++) {
		byte palette_index = data[i];
		rgbadata[i * 4]     = host_basepal[palette_index * 3 + 0];
        rgbadata[i * 4 + 1] = host_basepal[palette_index * 3 + 1];
        rgbadata[i * 4 + 2] = host_basepal[palette_index * 3 + 2];
        rgbadata[i * 4 + 3] = 255; // Set alpha to opaque
	}

	int ret = GL_LoadImages(texname, width, height, rgbadata, true, GU_LINEAR, 0, 4, true);

	free(rgbadata);
	return ret;
}

// Hacky thing to only load a top half of an image for skybox sides, hard to imagine other use for this
int loadskyboxsideimage (char* filename, int image_format, bool keep, int filter)
{
	int texture_index;
	byte *data;
	char texname[32];

	// create a unique identifier
	tex_filebase (filename, texname);

	// does the texture already exist?
	texture_index = Image_FindImage(texname);
	if (texture_index >= 0) {
		return texture_index;
	}

	data = Image_LoadPixels (filename, image_format);

	if(data == NULL) {
		return 0;
	}

	int newheight = image_height * 0.5;
	
	texture_index = GL_LoadImages (texname, image_width, newheight, data, true, filter, 0, 4, keep);

	free(data);

	return texture_index;
}

/*
=============
loadpcxas4bpp
=============
converts an indexed pcx to a 16 color one, if there's a palhint file, use that palette
*/
int loadpcxas4bpp (char* filename, int filter) 
{
	FILE* f;
	char name[128];
	char texname[32];

	sprintf(name, "%s.pcx", filename);
	tex_filebase (name, texname);
	COM_FOpenFile(name, &f);
	if (!f) {
		Con_DPrintf("Could not load PCX file %s\n", name);
		return 0;
	}

	pcx_t pcxbuf;
    fread(&pcxbuf, 1, sizeof(pcxbuf), f);

    pcx_t* pcx = &pcxbuf;

    if (pcx->manufacturer != 0x0a || pcx->version != 5 || pcx->encoding != 1 ||
        pcx->bits_per_pixel != 8 || pcx->xmax >= 320 || pcx->ymax >= 256) {
        Con_DPrintf("Bad pcx file %s\n", name);
        return 0;
    }

	unsigned int width = pcx->xmax + 1;
	unsigned int height = pcx->ymax + 1;

	fseek(f, -768, SEEK_END);
    byte palette[768];
    fread(palette, 1, 768, f);
	fseek(f, sizeof(pcxbuf) - 4, SEEK_SET);
	int size = width * height;
	byte* data = (unsigned char*)(Q_malloc(4 * size));
	int data_index = 0;
	for (int y = 0; y <= pcx->ymax; y++) {
        for (int x = 0; x <= pcx->xmax;) {
            int dataByte = fgetc(f);

            int runLength = 1;
            if ((dataByte & 0xC0) == 0xC0) {
                runLength = dataByte & 0x3F;
                dataByte = fgetc(f);
            }

            while (runLength-- > 0) {
                data[data_index] = dataByte;
                data_index += 1;
                x++;
            }
        }
    }

	fclose(f);

	int texture = 0;
	sprintf(name, "%s.palhint", filename);
	FILE* f2;
	COM_FOpenFile(name, &f2);

	if (!f2) {
		texture = GL_LoadTexture8to4(texname, width, height, data, palette, filter, 3, NULL);
	} else {
		// contain padding for extra whitespace etc 
		size_t size = 16 * 4 * 2 * 2;
		char filecontent[size];
		int bytes = fread(&filecontent, 1, size, f2);
		fclose(f2);

		int index = 0;
		unsigned int palhint[16];
		memset(palhint, 0xff, 16 * 4);
		int colors_read = 0;
		char color[7];
		while (index < (bytes - 1)) {
			if (filecontent[index] == '#') {
				index++;
				memcpy(color, &(filecontent[index]), 6);
				color[6] = '\0';
				int parsed = strtol(color, NULL, 16);
				byte r, g, b;
				r = parsed >> 16;
				g = parsed >> 8;
				b = parsed;
				palhint[colors_read] = 0xff000000 + (b << 16) + (g << 8) + r;
				colors_read++;
				index += 6;
			} else {
				index++;
			}
		}
		texture = GL_LoadTexture8to4(texname, width, height, data, palette, filter, 3, (byte*)palhint);
	}

	free(data);

	return texture;
}
#endif

#ifdef __NSPIRE__

cachepic_t		cachepics[MAX_CACHED_PICS];
int				numcachepics = 1;
// naievil -- texture conversion start 
byte converted_pixels[MAX_SINGLE_PLANE_PIXEL_SIZE]; 
byte temp_pixel_storage_pixels[MAX_SINGLE_PLANE_PIXEL_SIZE*4]; // naievil -- rgba storage for max pic size 
// naievil -- texture conversion end

byte* loadimagepixelstoqpal (char* texname, int width, int height, byte *data, qboolean transparenttoblack, qboolean usehunk)
{
	// Set the buffer to empty
	memset(converted_pixels, 0, width * height);

	// Convert the pixels 
	int converted_counter = 0;
	byte result;
	for (int i = 0; i < width * height * 4; i+= 4) {
		result = findclosestpalmatch(data[i], data[i + 1], data[i + 2], data[i + 3]);
		converted_pixels[converted_counter] = transparenttoblack && result == 255 ? 0 : result;
		converted_counter++;
	}

	if (usehunk)
	{
		byte *hunk_ptr = Hunk_AllocName (image_width * image_height, texname);
		Q_memcpy (hunk_ptr, converted_pixels, width * height);

		return hunk_ptr;
	}
	else
	{
		return converted_pixels;
	}
	
}

/*
================
Soft_LoadTexture
================
*/
int Soft_LoadTexture (char *texname, int width, int height, byte *data, qboolean transparenttoblack, qboolean usehunk)
{
	cachepic_t	*pic;
	int			i;
	int			texture_index = -1;
	
	for (pic=cachepics, i=0 ; i<numcachepics ; pic++, i++) {
		if (!pic->used) {
			texture_index = i;
			break;
		}
	}

	if (numcachepics == MAX_CACHED_PICS) {
		Sys_Error ("numcachepics == MAX_CACHED_PICS");
	}

	byte *buf = loadimagepixelstoqpal(texname, width, height, data, transparenttoblack, usehunk);
	if (!buf) {
		Sys_Error ("Soft_LoadTexture: failed to load %s", texname);
	}

	pic->data = Hunk_Alloc(sizeof(byte) + width * height);
	memcpy(pic->data, buf, width * height);
	strcpy (pic->name, texname);
	pic->width = width;
	pic->height = height;
	pic->used = qtrue;
	
	numcachepics++;

	return texture_index;
}
#endif // __NSPIRE__