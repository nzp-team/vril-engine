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
	if (in == NULL || !*in) return;

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

static byte *Image_LoadFailure(FILE *file, const char *message, qboolean quiet)
{
    fclose(file);
    if (quiet)
        Con_DPrintf("Image cache: %s\n", message);
    else
        Sys_Error("%s\n", message);
    return NULL;
}

static byte *LoadPCX(FILE *file, int length, qboolean quiet)
{
    pcx_t header;
    byte palette[768], *pixels;
    long start = ftell(file), palette_offset;
    int width, height, stride, y;

    if (start < 0 || length < 128 + 768 || fread(&header, 1, 128, file) != 128)
        return Image_LoadFailure(file, "Truncated pcx header", quiet);
    width = (unsigned short)LittleShort(header.xmax) + 1;
    height = (unsigned short)LittleShort(header.ymax) + 1;
    stride = (unsigned short)LittleShort(header.bytes_per_line);
    if (header.manufacturer != 0x0a || header.version != 5 || header.encoding != 1 ||
        header.bits_per_pixel != 8 || header.color_planes != 1 || width > 640 || height > 480 ||
        header.xmin || header.ymin || stride < width || stride > 4096)
        return Image_LoadFailure(file, "Bad pcx file", quiet);
    palette_offset = start + length - 768;
    if (fseek(file, palette_offset, SEEK_SET) || fread(palette, 1, sizeof(palette), file) != sizeof(palette) ||
        fseek(file, start + 128, SEEK_SET))
        return Image_LoadFailure(file, "Truncated pcx palette", quiet);
    pixels = malloc((size_t)width * height * 4);
    if (!pixels) {
        fclose(file);
        return NULL;
    }
    for (y = 0; y < height; ++y) {
        int x = 0;
        while (x < stride) {
            int value = ftell(file) < palette_offset ? fgetc(file) : EOF;
            int run = 1;
            if (value != EOF && (value & 0xc0) == 0xc0) {
                run = value & 0x3f;
                value = ftell(file) < palette_offset ? fgetc(file) : EOF;
            }
            if (value == EOF || !run || run > stride - x) {
                free(pixels);
                return Image_LoadFailure(file, "Truncated or invalid pcx run", quiet);
            }
            while (run--) {
                if (x < width) {
                    byte *out = pixels + ((size_t)y * width + x) * 4;
                    memcpy(out, palette + value * 3, 3);
                    out[3] = 255;
                }
                ++x;
            }
        }
    }
    image_width = width;
    image_height = height;
    fclose(file);
    return pixels;
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
static byte *LoadSTBI_Image(FILE *f, qboolean quiet)
{
	int bpp;
	int inwidth, inheight;
	if (quiet) {
		long start = ftell(f);
		if (start < 0 || !stbi_info_from_file(f, &inwidth, &inheight, &bpp) ||
			inwidth < 1 || inheight < 1 || inwidth > 4096 || inheight > 4096 ||
			(size_t)inwidth * inheight > (16u * 1024u * 1024u) / 4 ||
			fseek(f, start, SEEK_SET))
			return Image_LoadFailure(f, "Image is invalid or too large to prepare", true);
	}
	byte* image = stbi_load_from_file(f, &inwidth, &inheight, &bpp, 4);

	if(image == NULL) {
        return Image_LoadFailure(f, stbi_failure_reason(), quiet);
	}

	image_width = inwidth;
	image_height = inheight;
	fclose(f);
	return image;
}

#define IMAGE_CACHE_VERSION 1

static byte *Image_LoadSource(FILE *file, const char *name, int format, int length, qboolean quiet)
{
    asset_cache_data_t cached;
    byte *pixels;
    uint32_t source_crc = 0;
    qboolean use_cache = AssetCache_Enabled() && length >= 0 && AssetCache_FileCRC(file, length, &source_crc);

    if (use_cache && AssetCache_Load(ASSET_CACHE_IMAGE, name, source_crc, IMAGE_CACHE_VERSION, &cached)) {
        size_t width = cached.metadata[0], height = cached.metadata[1];
        if (width && height && width <= 4096 && height <= 4096 && cached.size == width * height * 4) {
            image_width = width;
            image_height = height;
            fclose(file);
            return cached.data;
        }
        free(cached.data);
    }
    pixels = format == IMAGE_PCX ? LoadPCX(file, length, quiet) : LoadSTBI_Image(file, quiet);
    if (use_cache && pixels && image_width > 0 && image_height > 0 && image_width <= 4096 && image_height <= 4096) {
        memset(&cached, 0, sizeof(cached));
        cached.data = pixels;
        cached.size = (size_t)image_width * image_height * 4;
        cached.metadata[0] = image_width;
        cached.metadata[1] = image_height;
        AssetCache_Store(ASSET_CACHE_IMAGE, name, source_crc, IMAGE_CACHE_VERSION, &cached);
    }
    return pixels;
}

byte* Image_LoadPixels(char* filename, int image_format)
{
    static const struct { int format; const char *extension; } formats[] = {
        {IMAGE_PCX, "pcx"}, {IMAGE_TGA, "tga"}, {IMAGE_PNG, "png"},
        {IMAGE_JPG, "jpg"}, {IMAGE_JPG, "jpeg"}
    };
    FILE *file;
    char name[MAX_QPATH];
    size_t i;

    for (i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
        if (!(image_format & formats[i].format))
            continue;
        snprintf(name, sizeof(name), "%s.%s", filename, formats[i].extension);
        int length = COM_FOpenFile(name, &file);
        if (length >= 0)
            return Image_LoadSource(file, name, formats[i].format, length, false);
    }
    return NULL;
}

qboolean Image_BuildCache(const char *name)
{
    char extension[8];
    const char *dot = strrchr(name, '.');
    FILE *file;
    byte *pixels;
    int format, length;

    if (!AssetCache_Enabled() || !dot || strlen(dot + 1) >= sizeof(extension))
        return false;
    Q_strlcpy(extension, dot + 1, sizeof(extension));
    format = !Q_strcasecmp(extension, "pcx") ? IMAGE_PCX :
        !Q_strcasecmp(extension, "tga") ? IMAGE_TGA :
        !Q_strcasecmp(extension, "png") ? IMAGE_PNG :
        (!Q_strcasecmp(extension, "jpg") || !Q_strcasecmp(extension, "jpeg")) ? IMAGE_JPG : 0;
    if (!format)
        return false;
    length = COM_FOpenFile((char *)name, &file);
    if (!file)
        return false;
    if (length <= 0 || length > 16 * 1024 * 1024) {
        fclose(file);
        return false;
    }
    pixels = Image_LoadSource(file, name, format, length, true);
    if (!pixels)
        return false;
    free(pixels);
    return true;
}

image_t Image_LoadImageWithIdentifier(char *filename, char *identifier, int image_format,
  int filter, bool keep, bool mipmap)
{
	int texture_index;
	byte *data;

	if (filename == NULL || identifier == NULL || identifier[0] == '\0'
	  || strlen(identifier) >= 64) {
		Con_DPrintf("bad texture %s\n", filename);
		return -1;
	}

	// does the texture already exist?
	texture_index = Hyena_FindTexture(identifier);
	if (texture_index >= 0) {
		return texture_index;
	}

	data = Image_LoadPixels (filename, image_format);

	if(data == NULL) {
		return -1;
	}

	texture_index = Image_LoadTexture(identifier, image_width, image_height, data, HYE_TEXTURE_RGBA8, filter, mipmap ? 1 : 0, true, keep, true);

	if(texture_index < 0) {
		Sys_Error("Image_LoadImage: failed to upload texture %s\n", identifier);
	}

	free(data);

	return texture_index;
}

image_t Image_LoadImage(char* filename, int image_format, int filter, bool keep, bool mipmap)
{
	char texname[64] = {0};

	if (filename == NULL) return -1;
	tex_filebase (filename, texname);
	return Image_LoadImageWithIdentifier(filename, texname, image_format, filter, keep, mipmap);
}

// used on PSP
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

	int ret = Image_LoadTexture(texname, width, height, rgbadata, HYE_TEXTURE_RGBA8, HYE_FILTER_LINEAR, 0, true, true, true);

	free(rgbadata);
	return ret;
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
	int cached = Hyena_FindTexture(texname);
	if (cached >= 0) {
		Hyena_KeepTexture(cached);
		return cached;
	}
	COM_FOpenFile(name, &f);
	if (!f) {
		Con_DPrintf("Could not load PCX file %s\n", name);
		return -1;
	}

	pcx_t pcxbuf;
	if (fread(&pcxbuf, 1, sizeof(pcxbuf), f) != sizeof(pcxbuf)) {
		Con_DPrintf("Bad pcx file %s\n", name);
		fclose(f);
		return -1;
	}

    pcx_t* pcx = &pcxbuf;

    if (pcx->manufacturer != 0x0a || pcx->version != 5 || pcx->encoding != 1 ||
        pcx->bits_per_pixel != 8 || pcx->xmax >= 320 || pcx->ymax >= 256) {
        Con_DPrintf("Bad pcx file %s\n", name);
        fclose(f);
        return -1;
    }

	unsigned int width = pcx->xmax + 1;
	unsigned int height = pcx->ymax + 1;

	byte palette[768];
	if (fseek(f, -768, SEEK_END) != 0 || fread(palette, 1, sizeof(palette), f) != sizeof(palette) ||
		fseek(f, sizeof(pcxbuf) - 4, SEEK_SET) != 0) {
		Con_DPrintf("Bad pcx file %s\n", name);
		fclose(f);
		return -1;
	}
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

	int texture = -1;
	sprintf(name, "%s.palhint", filename);
	FILE* f2;
	COM_FOpenFile(name, &f2);

	if (!f2) {
		texture = Hyena_LoadPalettedTexture(texname, width, height, data, palette, 3, NULL, filter, 0, true, true);
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
		texture = Hyena_LoadPalettedTexture(texname, width, height, data, palette, 3, (byte *)palhint, filter, 0, true, true);
	}

	free(data);

	return texture;
}

byte findclosestpalmatch(byte r, byte g, byte b, byte a)
{
	int best_distance = 0x7fffffff;
	int best_index = 0;
	int i;

	if (a < 128)
		return 255;
	for (i = 0; i < 256; ++i) {
		int red = r - host_basepal[i * 3];
		int green = g - host_basepal[i * 3 + 1];
		int blue = b - host_basepal[i * 3 + 2];
		int distance = red * red + green * green + blue * blue;
		if (distance < best_distance) {
			best_distance = distance;
			best_index = i;
		}
	}
	return (byte)best_index;
}

int
Image_LoadTexture(const char *identifier, int width, int height, const void *pixels,
    hyena_texture_format_t format, hyena_texture_filter_t filter, int mip_levels,
    qboolean alpha, qboolean keep, qboolean stretch_to_power_of_two)
{
    hyena_texture_desc_t desc = { 0 };
    size_t length = strlen(identifier);

    desc.identifier = identifier;
    desc.pixels = pixels;
    desc.width = width;
    desc.height = height;
    desc.format = format;
    desc.filter = filter;
    desc.mip_levels = mip_levels;
    desc.alpha = alpha;
    desc.keep = keep;
    desc.stretch_to_power_of_two = stretch_to_power_of_two;
    // Effect images and sprite frames need interpolated alpha even with DXT1 selected.
    desc.smooth_alpha = (length >= 7 && identifier[length - 7] == '$') || (length >= 5 && !strncmp(identifier + length - 5, "spr", 3));
    return Hyena_CreateTexture(&desc);
}
