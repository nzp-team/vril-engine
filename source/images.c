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
	char name[133];

	if (image_format & IMAGE_PCX) {
		snprintf (name, 132, "%s.pcx", filename);
		if (COM_FOpenFile(name, &f) != -1)
			return LoadPCX (f, 0, 0);
	}

	if (image_format & IMAGE_TGA) {
		snprintf (name, 132, "%s.tga", filename);
		if (COM_FOpenFile(name, &f) != -1)
			return LoadSTBI_Image (f);
	}

	if (image_format & IMAGE_PNG) {
		snprintf (name, 132, "%s.png", filename);
		if (COM_FOpenFile(name, &f) != -1)
			return LoadSTBI_Image (f);
	}

	if (image_format & IMAGE_JPG) {
		snprintf (name, 132, "%s.jpg", filename);
		if (COM_FOpenFile(name, &f) != -1)
			return LoadSTBI_Image (f);

		snprintf (name, 133, "%s.jpeg", filename);
		if (COM_FOpenFile(name, &f) != -1)
			return LoadSTBI_Image (f);
	}

	return NULL;						
}

int Image_LoadImage(char* filename, int image_format, int filter, qboolean keep, qboolean mipmap)
{
	int texture_index;
	byte *data;
    char texname[32];

	tex_filebase (filename, texname);

	//Con_DPrintf("filename %s\n", filename);
	//Con_DPrintf("tex name %s\n", texname);
	// already loaded?
	texture_index = GL_FindTexture(texname);
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
	texture_index = GL_LoadTexture32 (texname, image_width, image_height, data, false, true, false);
#elif __3DS__
    texture_index = GL_LoadTexture (texname, image_width, image_height, data, mipmap, true, 4, keep);
#elif __WII__
    texture_index = GL_LoadTexture (texname, image_width, image_height, data, false, true, keep, 4);
#endif

	if(texture_index < 0)
		Sys_Error("Image_LoadImage: failed to upload texture %s\n", texname);

	free(data);

	return texture_index;
}

/*
=============
loadrgbafrompal
=============
*/
// used on PSP
#ifdef __PSP__
int loadrgbafrompal (char* name, int width, int height, byte* data)
{
	int pixels = width * height;
	byte* rgbadata = (byte*)Q_malloc(pixels * 4);

	for(int i = 0; i < pixels; i++) {
		byte palette_index = data[i];
		rgbadata[i * 4]     = host_basepal[palette_index * 3 + 0];
        rgbadata[i * 4 + 1] = host_basepal[palette_index * 3 + 1];
        rgbadata[i * 4 + 2] = host_basepal[palette_index * 3 + 2];
        rgbadata[i * 4 + 3] = 255; // Set alpha to opaque
	}

	int ret = GL_LoadImages(name, width, height, rgbadata, true, GU_LINEAR, 0, 4, true);

	free(rgbadata);
	return ret;
}
#endif