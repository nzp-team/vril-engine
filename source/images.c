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
	
	for (s2 = s ; s2 != in && *s2 && *s2 != '/' ; s2--)
	;
	
	if (s-s2 < 2) {
		strcpy (out,"bad input");
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

byte* LoadPCX(FILE* f, int matchwidth, int matchheight) {
    pcx_t pcxbuf;
    fread(&pcxbuf, 1, sizeof(pcxbuf), f);

    pcx_t* pcx = &pcxbuf;

    if (pcx->manufacturer != 0x0a || pcx->version != 5 || pcx->encoding != 1 ||
        pcx->bits_per_pixel != 8 || pcx->xmax >= 320 || pcx->ymax >= 256) {
        Con_Printf("Bad pcx file\n");
        return NULL;
    }

    if (matchwidth && (pcx->xmax + 1) != matchwidth) {
        fclose(f);
        return NULL;
    }

    if (matchheight && (pcx->ymax + 1) != matchheight) {
        fclose(f);
        return NULL;
    }

    fseek(f, -768, SEEK_END);
    byte palette[768];
    fread(palette, 1, 768, f);

    fseek(f, sizeof(pcxbuf) - 4, SEEK_SET);

    int count = (pcx->xmax + 1) * (pcx->ymax + 1);
    byte* image_rgba = (byte*)(Q_malloc(4 * count));

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
#define STBI_ASSERT(x)
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#define STBI_ONLY_PIC
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

/*
=============
loadimagepixels
=============
*/
										// HACK HACK HACK
byte* loadimagepixels (char* filename, qboolean complain, int matchwidth, int matchheight)
{
	FILE	*f;
	char	basename[128], name[133];
	byte	 *c;

	if (complain == false)
		COM_StripExtension(filename, basename); // strip the extension to allow TGA and PCX
	else
		strcpy(basename, filename);

	c = (byte*)(basename);

	while (*c)
	{
		if (*c == '*')
			*c = '+';
		c++;
	}
	
	snprintf (name, 132, "%s.pcx", basename);
	COM_FOpenFile(name, &f);
	if (f)
		return LoadPCX (f, matchwidth, matchheight);
    
    snprintf (name, 132, "%s.tga", basename);
	COM_FOpenFile(name, &f);
	if (f)
		return LoadSTBI_Image (f);
    
    snprintf (name, 132, "%s.png", basename);
	COM_FOpenFile(name, &f);
	if (f)
		return LoadSTBI_Image (f);
	
	snprintf (name, 132, "%s.jpg", basename);
	COM_FOpenFile(name, &f);
	if (f)
	{
		return LoadSTBI_Image (f);
	}
    snprintf (name, 133, "%s.jpeg", basename);
	COM_FOpenFile(name, &f);
	if (f)
	{
		return LoadSTBI_Image (f);
	}
	
	return NULL;
}

/*
=============
loadtextureimage
=============
*/

int loadtextureimage (char* filename, int matchwidth, int matchheight, qboolean complain, int filter, qboolean keep, qboolean mipmap)
{
	int texture_index;
	byte *data;
    char texname[32];

	int hunk_start = Hunk_LowMark();
	data = loadimagepixels (filename, complain, matchwidth, matchheight);
	int hunk_stop = Hunk_LowMark();

	if(!data)
	{
		return 0;
	}

    tex_filebase (filename, texname);
	
#ifdef __PSP__
	texture_index = GL_LoadImages (texname, image_width, image_height, data, true, filter, 0, 4);
#elif __3DS__
    texture_index = GL_LoadTexture (filename, image_width, image_height, data, mipmap, true, 4);
#elif __WII__
    texture_index = GL_LoadTexture (texname, image_width, image_height, data, false, true, keep, 4);
#endif

	// Only free the hunk if it was used.
	if (hunk_start != hunk_stop)
		Hunk_FreeToLowMark(hunk_start);
	else
		free(data);

	return texture_index;
}
// Hacky thing to only load a top half of an image for skybox sides, hard to imagine other use for this
int loadskyboxsideimage (char* filename, int matchwidth, int matchheight, qboolean complain, int filter)
{
	int texture_index;
	byte *data;

	int hunk_start = Hunk_LowMark();
	data = loadimagepixels (filename, complain, matchwidth, matchheight);
	int hunk_stop = Hunk_LowMark();

	if(!data)
	{
		return 0;
	}
    
#ifdef __PSP__
	int newheight = image_height * 0.5;
	texture_index = GL_LoadImages (filename, image_width, newheight, data, true, filter, 0, 4);
#endif

	// Only free the hunk if it was used.
	if (hunk_start != hunk_stop)
		Hunk_FreeToLowMark(hunk_start);
	else
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

	int ret = GL_LoadImages(name, width, height, rgbadata, true, GU_LINEAR, 0, 4);

	free(rgbadata);
	return ret;
}
#endif