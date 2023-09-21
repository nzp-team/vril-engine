/*
Copyright (C) 2009 Crow_bar.

Used code from "Fuhquake" modify by Crow_bar

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

extern "C"
{
#include "../quakedef.h"
}
#include <pspgu.h>
#include <malloc.h>
#include <list>

std::list<FILE*> UnloadFileList;

#define TEXWAD_MAXIMAGES 16384
typedef struct
{
	char name[16];
	FILE *file;
	int position;
	int size;
} texwadlump_t;

texwadlump_t	texwadlump[TEXWAD_MAXIMAGES];
int numwadtextures;

//By Crow_bar
void UnloadWads (void)
{
	FILE *files;

	while (UnloadFileList.size() > 0)
	{
		files = UnloadFileList.front();
		UnloadFileList.pop_front();
		fclose(files);
	}
	numwadtextures = 0;
}

void WAD3_LoadTextureWadFile (char *filename)
{
	lumpinfo_t *lumps, *lump_p;
	wadinfo_t header;
	int i, j, infotableofs, numlumps, lowmark;
	FILE *file;

	if (FS_FOpenFile (va("textures/wad3/%s", filename), &file) != -1)
		goto loaded;
	if (FS_FOpenFile (va("textures/halflife/%s", filename), &file) != -1)
		goto loaded;
	if (FS_FOpenFile (va("textures/%s", filename), &file) != -1)
		goto loaded;
	if (FS_FOpenFile (filename, &file) != -1)
		goto loaded;


 	Host_Error ("Couldn't load halflife wad \"%s\"\n", filename);

loaded:
	if (fread(&header, 1, sizeof(wadinfo_t), file) != sizeof(wadinfo_t))
	{
		Con_Printf ("WAD3_LoadTextureWadFile: unable to read wad header");
        fclose(file);
		return;
	}

	if (memcmp(header.identification, "WAD3", 4))
	{
		Con_Printf ("WAD3_LoadTextureWadFile: Wad file %s doesn't have WAD3 id\n",filename);
        fclose(file);
		return;
	}

	numlumps = LittleLong(header.numlumps);

	if (numlumps < 1 || numlumps > TEXWAD_MAXIMAGES)
	{
		Con_Printf ("WAD3_LoadTextureWadFile: invalid number of lumps (%i)\n", numlumps);
        fclose(file);
		return;
	}

	infotableofs = LittleLong(header.infotableofs);

	if (fseek(file, infotableofs, SEEK_SET))
	{
		Con_Printf ("WAD3_LoadTextureWadFile: unable to seek to lump table");
        fclose(file);
		return;
	}

	lowmark = Hunk_LowMark();

	if (!(lumps = static_cast<lumpinfo_t*>(Hunk_Alloc(sizeof(lumpinfo_t) * numlumps))))
	{
		Con_Printf ("WAD3_LoadTextureWadFile: unable to allocate temporary memory for lump table");
        fclose(file);
		return;
	}

	if (fread(lumps, 1, sizeof(lumpinfo_t) * numlumps, file) != sizeof(lumpinfo_t) * numlumps)
	{
		Con_Printf ("WAD3_LoadTextureWadFile: unable to read lump table");
        fclose(file);
		Hunk_FreeToLowMark(lowmark);
		return;
	}

	UnloadFileList.push_back(file); //Crow_bar. UnloadWads code

	for (i = 0, lump_p = lumps; i < numlumps; i++,lump_p++)
	{
        W_CleanupName (lump_p->name, lump_p->name);
		for (j = 0;j < numwadtextures;j++)
		{
			if (!strcmp(lump_p->name, texwadlump[j].name)) // name match, replace old one
				break;
		}
		if (j >= TEXWAD_MAXIMAGES)
			break; // abort loading
		if (j == numwadtextures)
		{
			W_CleanupName (lump_p->name, texwadlump[j].name);
			texwadlump[j].file = file;
			texwadlump[j].position = LittleLong(lump_p->filepos);
			texwadlump[j].size = LittleLong(lump_p->disksize);
			numwadtextures++;
		}
	}

	Hunk_FreeToLowMark(lowmark);
	//leaves the file open
}

//converts paletted to rgba
int ConvertWad3ToRGBA(miptex_t *tex)
{
	// Check that texture has data
    if (!tex->offsets[0]) {
        Sys_Error("ConvertWad3ToRGBA: tex->offsets[0] == 0");
    }

    // Get pointers to WAD3 data and palette
    byte* wadData = ((byte*)tex) + tex->offsets[0];
    byte* palette = ((byte*)tex) + tex->offsets[3] + (tex->width>>3)*(tex->height>>3) + 2;
	//byte* palette = wadData + tex->offsets[MIPLEVELS]; // Palette starts 2 bytes after the last mipmap

    // Allocate buffer for RGBA data
    int imageSize = tex->width * tex->height;
    byte* rgbaData = (byte*)Q_malloc(imageSize * 4);

    // Convert WAD3 data to RGBA format
    for (int i = 0; i < imageSize; i++) {
        byte colorIndex = wadData[i];
        rgbaData[i * 4]     = palette[colorIndex * 3 + 0];
        rgbaData[i * 4 + 1] = palette[colorIndex * 3 + 1];
        rgbaData[i * 4 + 2] = palette[colorIndex * 3 + 2];
        rgbaData[i * 4 + 3] = 255; // Set alpha to opaque

		if (rgbaData[i * 4] == 0 && rgbaData[i * 4 + 1] == 0 && rgbaData[i * 4 + 2] == 255) {
			rgbaData[i * 4] = rgbaData[i * 4 + 1] = rgbaData[i * 4 + 2] = 128;
			rgbaData[i * 4 + 3] = 0;
		}
    }

	int index = GL_LoadImages(tex->name, tex->width, tex->height, rgbaData, qtrue, GU_LINEAR, 0, 4);

	free(rgbaData);

	return index;
}

struct colentry_t {
	byte r;
	byte g;
	byte b;
	byte a;
	unsigned short pixcount;
};
colentry_t colentries[256];
typedef byte bucket_t[256];

bucket_t buckets[16];
int bucket_counts[16];
int num_buckets = 0;

byte byte_to_halfbyte_map[256];

void add_to_bucket(int bucket, int color) {
	buckets[bucket][bucket_counts[bucket]] = color;
	bucket_counts[bucket]++;
}

void remove_last_from_bucket(int bucket) {
	bucket_counts[bucket]--;
	buckets[bucket][bucket_counts[bucket]] = 0;
}


void init_buckets() {
	memset(buckets, 0, sizeof(buckets));
	memset(bucket_counts, 0, sizeof(bucket_counts));
	memset(colentries, 0, sizeof(colentries));
	memset(byte_to_halfbyte_map, 0, sizeof(byte_to_halfbyte_map));
	num_buckets = 0;
}

int get_new_bucket() {
	// Con_Printf("Create new bucket %d\n", num_buckets);
	num_buckets += 1;
	return num_buckets - 1;
}

colentry_t * color_of_bucket(int bucket, int index) {
	return &(colentries[buckets[bucket][index]]);
}

int get_color_index(int bucket, int index) {
	return buckets[bucket][index];
}

void sort_and_cut_bucket(int current_bucket, int depth, int target) {
	if (num_buckets >= target) {
		// Con_Printf("quit due to too many buckets %d / %d\n", num_buckets, target);
		return;
	}

	int count = bucket_counts[current_bucket];
	// find the highest range and count pixels in bucket
	byte minR = 255;
	byte maxR = 0;
	byte minG = 255;
	byte maxG = 0;
	byte minB = 255;
	byte maxB = 0;
	int pixelsInBucket = 0;
	for (int i = 0; i < count; i++) {
		colentry_t * col = color_of_bucket(current_bucket, i);
		pixelsInBucket += col->pixcount;
		if (col->r < minR) minR = col->r;
		if (col->r > maxR) maxR = col->r;
		if (col->g < minG) minG = col->g;
		if (col->g > maxG) maxG = col->g;
		if (col->b < minB) minB = col->b;
		if (col->b > maxB) maxB = col->b;
	}
	int rangeR = maxR - minR;
	int rangeG = maxG - minG;
	int rangeB = maxB - minB;

	// 0 = r, 1 = g, 2 = b
	int sortBy = 0;
	if (rangeG > rangeR && rangeG > rangeB) sortBy = 1;
	if (rangeB > rangeR && rangeB > rangeG) sortBy = 2;

	// Con_Printf("%d: depth: %d, count: %d, pixels: %d, sortBy: %d\n", current_bucket, depth, count, pixelsInBucket, sortBy);
	// bubble sort, replace with comb sort or heap sort
	bool sorted = false;
	while (!sorted) {
		sorted = true; // this will change if a swap is done
		for (int i = 0; i < (count - 1); i++) {
			colentry_t * col1 = color_of_bucket(current_bucket, i);
			colentry_t * col2 = color_of_bucket(current_bucket, i + 1);
			bool swap = false;
			switch (sortBy) {
				case 0: swap = col1->r > col2->r; break;
				case 1: swap = col1->g > col2->g; break;
				case 2: swap = col1->b > col2->b; break;
			}
			if (swap) {
				byte prevval = buckets[current_bucket][i];
				byte nextval = buckets[current_bucket][i + 1];
				buckets[current_bucket][i] = nextval;
				buckets[current_bucket][i + 1] = prevval;
				sorted = false;
			}
		}
	}

	// median cut
	int pixelsCounted = 0;
	int split = 0;
	for (int i = 0; i < count; i++) {
		if (pixelsCounted > pixelsInBucket * 0.5) {
			split = i;
			break;
		}
		byte_to_halfbyte_map[get_color_index(current_bucket, i)] = current_bucket;
		colentry_t * col = color_of_bucket(current_bucket, i);
		pixelsCounted += col->pixcount;
	}

	int next_bucket = get_new_bucket();
	for (int i = count-1; i >= split; i--) {
		byte colorindex = get_color_index(current_bucket, i);
		byte_to_halfbyte_map[colorindex] = next_bucket;
		add_to_bucket(next_bucket, colorindex);
		remove_last_from_bucket(current_bucket);
	}

	if (depth >= 3) return;

	sort_and_cut_bucket(current_bucket, depth + 1, target);
	sort_and_cut_bucket(next_bucket, depth + 1, target);
} 

int ConvertWad3ToClut4(miptex_t *tex)
{
	// Check that texture has data
    if (!tex->offsets[0]) {
        Sys_Error("ConvertWad3ToRGBA: tex->offsets[0] == 0");
    }

    // Get pointers to WAD3 data and palette
    byte* wadData = ((byte*)tex) + tex->offsets[0];
    byte* palette = ((byte*)tex) + tex->offsets[3] + (tex->width>>3)*(tex->height>>3) + 2;
	//byte* palette = wadData + tex->offsets[MIPLEVELS]; // Palette starts 2 bytes after the last mipmap

	init_buckets();
	int current_bucket = get_new_bucket();

	// Set bucket pixelcounts
	for (int pixi = 0; pixi < tex->width * tex->height; pixi++) {
		byte color = wadData[pixi];
		colentries[color].pixcount += 1;
	}

	int MAX_COLORS = 256;
	bool has_transparency = false;
	byte transparent_index = 0;
	int colors_used = 0;
	// Set colors and fill first bucket
	for (int i = 0; i < MAX_COLORS; i++) {
		colentry_t * ce = &(colentries[i]);
		if (ce->pixcount == 0) {
			continue; // speed up by not processing unused colors
		}
		colors_used++;

		ce->r = palette[i * 3 + 0];
		ce->g = palette[i * 3 + 1];
		ce->b = palette[i * 3 + 2];
		ce->a = 255;
		if (ce->r == 0 && ce->g == 0 && ce->b == 255) {
			ce->r = ce->g = ce->b = 128;
			ce->a = 255; 
			has_transparency = true;
			transparent_index = i;
			continue; // don't add transparencies to buckets
		}
		add_to_bucket(current_bucket, i);
	}
	// Con_Printf("Initial bucket %d: %d colors\n", current_bucket, bucket_counts[current_bucket]);

	// could check if bucket has 16 colors or less and early out

	sort_and_cut_bucket(current_bucket, 0, has_transparency ? 15 : 16);

	if (has_transparency) {
		int transparent_bucket = get_new_bucket();
		byte_to_halfbyte_map[transparent_index] = transparent_bucket; // last index
		add_to_bucket(transparent_bucket, transparent_index);
	}
	// Con_Printf("num buckets: %d, has transparency: %d, colors actually used: %d\n", num_buckets, has_transparency, colors_used);

	/* hack to resample width/height if it's less than 32 */
	bool stretch_sideways = false;
	if (tex->width <= 16) {
		tex->width *= 2;
		stretch_sideways = true;
	}
	int imageSize = tex->width * tex->height * 0.5;
	byte* texData = (byte*)memalign(16, 16 * 4 + imageSize);
	byte* unswizzled = (byte*)memalign(16, imageSize);
	unsigned int * newPal = (unsigned int*)&(texData[imageSize]);

	/*
	for (int y = 0; y < tex->height; y++) {
		for (int x = 0; x < tex->width; x++) {
			byte colorIndex = wadData[y * (tex->width / width_mult) / height_mult + x / width_mult * 2];
			unswizzled[y * tex->width + x] = byte_to_halfbyte_map[colorIndex];
			colorIndex = wadData[y * (tex->width / width_mult) / height_mult + x / width_mult * 2 + 1];
			unswizzled[y * tex->width + x] = byte_to_halfbyte_map[colorIndex] << 4;
		}
	}
	*/
	
	if (stretch_sideways) {
		for (int i = 0; i < imageSize; i++) {
			byte colorIndex = wadData[i];
			unswizzled[i] = byte_to_halfbyte_map[colorIndex] + (byte_to_halfbyte_map[colorIndex] << 4);
		}
	} else {
		for (int i = 0; i < imageSize; i++) {
			byte colorIndex = wadData[i*2];
			unswizzled[i] = byte_to_halfbyte_map[colorIndex];
			colorIndex = wadData[i*2 + 1];
			unswizzled[i] += byte_to_halfbyte_map[colorIndex] << 4;
		}
	}

	for (int i = 0; i < num_buckets; i++) {
		int r = 0;
		int g = 0;
		int b = 0;
		int alpha = 255;
		colentry_t * col;
		for (int j = 0; j < bucket_counts[i]; j++) {
			byte colindex = get_color_index(i, j);
			if (has_transparency && transparent_index == colindex) {
				r = 128;
				g = 128;
				b = 128;
				alpha = 0;
				break;
			}
			col = color_of_bucket(i, j);
			r += col->r;
			g += col->g;
			b += col->b;
		}
		r /= bucket_counts[i];
		g /= bucket_counts[i];
		b /= bucket_counts[i];

		newPal[i] = (alpha << 24) + (b << 16) + (g << 8) + r;
		//Con_Printf("Bucket %d count: %d\n", i, bucket_counts[i]);
		//Con_Printf("Color %d: %d %d %d %x\n", i, r, g, b, newPal[i]);
	}
	//Con_Printf("_ _ _ \n");

	swizzle_fast(texData, unswizzled, tex->width * 0.5, tex->height);
	// memcpy(texData, unswizzled, imageSize);
	int index = GL_LoadTexture4(tex->name, tex->width, tex->height, texData, GU_LINEAR, qtrue);

	free(texData);
	free(unswizzled);

	return index;
}


int WAD3_LoadTexture(miptex_t *mt)
{
	char texname[MAX_QPATH];
	int i, j, lowmark = 0;
	FILE *file;
	miptex_t *tex;
	int index;

	if (mt->offsets[0])
 	    return ConvertWad3ToClut4(mt); // ConvertWad3ToRGBA(mt);

	texname[sizeof(texname) - 1] = 0;
	W_CleanupName (mt->name, texname);

	for (i = 0;i < numwadtextures;i++)
	{
		if (!texwadlump[i].name[0])
			break;

	    if (strcmp(texname, texwadlump[i].name))
			continue;

		file = texwadlump[i].file;

		if (fseek(file, texwadlump[i].position, SEEK_SET))
		{
			fclose(file);
			Con_Printf("WAD3_LoadTexture: corrupt WAD3 file");
            return 0;
		}

		lowmark = Hunk_LowMark();
		tex = static_cast<miptex_t*>(Hunk_Alloc(texwadlump[i].size));

		if (fread(tex, 1, texwadlump[i].size, file) < texwadlump[i].size)
		{
            Con_Printf("WAD3_LoadTexture: corrupt WAD3 file");
			Hunk_FreeToLowMark(lowmark);
            return 0;
		}

		tex->width = LittleLong(tex->width);
		tex->height = LittleLong(tex->height);

		if (tex->width != mt->width || tex->height != mt->height)
		{
			Hunk_FreeToLowMark(lowmark);
			return 0;
		}

		for (j = 0;j < MIPLEVELS;j++)
			tex->offsets[j] = LittleLong(tex->offsets[j]);

	    index = ConvertWad3ToClut4(mt); // ConvertWad3ToRGBA(tex);

		Hunk_FreeToLowMark(lowmark);

	    return index;
	}
 	return  0;
}

int WAD3_LoadTextureName(char *name)
{
	char texname[MAX_QPATH];
	int i, j, lowmark = 0;
	FILE *file;
    miptex_t *tex;
	int index;

	texname[sizeof(texname) - 1] = 0;
    W_CleanupName (name, texname);

	for (i = 0;i < numwadtextures;i++)
	{
		if (!texwadlump[i].name[0])
			break;

	    if (strcmp(texname, texwadlump[i].name))
			continue;

		file = texwadlump[i].file;

		if (fseek(file, texwadlump[i].position, SEEK_SET))
		{
			fclose(file);
			Con_Printf("WAD3_LoadTexture: corrupt WAD3 file");
            return 0;
		}

		lowmark = Hunk_LowMark();
		tex = static_cast<miptex_t*>(Hunk_Alloc(texwadlump[i].size));

		if (fread(tex, 1, texwadlump[i].size, file) < texwadlump[i].size)
		{
            Con_Printf("WAD3_LoadTexture: corrupt WAD3 file");
            fclose(file);
			Hunk_FreeToLowMark(lowmark);
            return 0;
		}

		tex->width = LittleLong(tex->width);
		tex->height = LittleLong(tex->height);
#if 0
		if (tex->width != mt->width || tex->height != mt->height)
		{
            fclose(file);
			Hunk_FreeToLowMark(lowmark);
			return 0;
		}
#endif
		for (j = 0;j < MIPLEVELS;j++)
			tex->offsets[j] = LittleLong(tex->offsets[j]);

	    index = ConvertWad3ToRGBA(tex);

		UnloadFileList.push_back(file); //Crow_bar. UnloadWads code
/*
        fclose(file);
*/
		Hunk_FreeToLowMark(lowmark);

	    return index;
	}
 	return  0;
}

//Other wad3 loaders
void W_LoadTextureWadFileHL (char *filename, int complain)
{
	lumpinfo_t		*lumps, *lump_p;
	wadinfo_t		header;
	unsigned		i, j;
	int				infotableofs;
	FILE			*file;
	int				numlumps;

	FS_FOpenFile (filename, &file);
	if (!file)
	{
		if (complain)
			Con_Printf ("W_LoadTextureWadFile: couldn't find %s\n", filename);
		return;
	}

	if (fread(&header, sizeof(wadinfo_t), 1, file) != 1)
	{Con_Printf ("W_LoadTextureWadFile: unable to read wad header");return;}

	if(header.identification[0] != 'W'
	|| header.identification[1] != 'A'
	|| header.identification[2] != 'D'
	|| header.identification[3] != '3')
	{
		fclose(file);
		Con_Printf ("W_LoadTextureWadFile: Wad file %s doesn't have WAD3 id\n",filename);
		return;
	}

	numlumps = LittleLong(header.numlumps);

	if (numlumps < 1 || numlumps > TEXWAD_MAXIMAGES)
	{
    fclose(file);
	Con_Printf ("W_LoadTextureWadFile: invalid number of lumps (%i)\n", numlumps);
	return;
	}
	infotableofs = LittleLong(header.infotableofs);

	if (fseek(file, infotableofs, SEEK_SET))
	{
    fclose(file);
	Con_Printf ("W_LoadTextureWadFile: unable to seek to lump table");
	return;
	}

	if (!(lumps = static_cast<lumpinfo_t*>(Q_malloc(sizeof(lumpinfo_t)*numlumps))))
	{
    fclose(file);
	Con_Printf ("W_LoadTextureWadFile: unable to allocate temporary memory for lump table");
	return;
	}

	if (fread(lumps, sizeof(lumpinfo_t), numlumps, file) != (unsigned)numlumps)
	{
    fclose(file);
	Con_Printf ("W_LoadTextureWadFile: unable to read lump table");
	return;
	}

	for (i=0, lump_p = lumps ; i<(unsigned)numlumps ; i++,lump_p++)
	{
		W_CleanupName (lump_p->name, lump_p->name);
		for (j = 0;j < TEXWAD_MAXIMAGES;j++)
		{
			if (texwadlump[j].name[0]) // occupied slot, check the name
			{
				if (!strcmp(lump_p->name, texwadlump[j].name)) // name match, replace old one
					break;
			}
			else // empty slot
				break;
		}
		if (j >= TEXWAD_MAXIMAGES)
			break; // abort loading

		W_CleanupName (lump_p->name, texwadlump[j].name);
		texwadlump[j].file = file;
		texwadlump[j].position = LittleLong(lump_p->filepos);
		texwadlump[j].size = LittleLong(lump_p->disksize);
	}
	free(lumps);
    //fclose(file);
	// leaves the file open
}

byte *W_ConvertWAD3TextureHL(miptex_t *tex)
{
	byte	*in, *data, *out, *pal;
	int		d, p, image_size;

	in		= (byte *)((int) tex + tex->offsets[0]);
	data	= out = static_cast<byte*>(Q_malloc(tex->width * tex->height * 4));

	if (!data)
		return NULL;

	image_size = tex->width * tex->height;

	pal				= in + (((image_size) * 85) >> 6);
	pal				+= 2;

	for (d = 0;d < image_size;d++)
	{
		p = *in++;
		if (tex->name[0] == '{' && p == 255)
			out[0] = out[1] = out[2] = out[3] = 0;
		else
		{
			p *= 3;
			out[0] = pal[p];
			out[1] = pal[p+1];
			out[2] = pal[p+2];
			out[3] = 255;
		}
		out += 4;
	}
	return data;
}

byte *W_GetTextureHL(char *name)
{
	char		texname[17];
	int			i, j;
	FILE		*file;
	miptex_t	*tex;
	byte		*data;

	texname[16] = 0;

	W_CleanupName (name, texname);

	for (i = 0;i < TEXWAD_MAXIMAGES;i++)
	{
		if (texwadlump[i].name[0])
		{
			if (!strcmp(texname, texwadlump[i].name)) // found it
			{
				file = texwadlump[i].file;
				if (fseek(file, texwadlump[i].position, SEEK_SET))
				{
					Con_Printf("W_GetTexture: corrupt WAD3 file");
					return NULL;
				}

				tex = static_cast<miptex_t*>(Q_malloc(texwadlump[i].size));

				if (!tex)
					return NULL;

				if (fread(tex, 1, texwadlump[i].size, file) < (unsigned)texwadlump[i].size)
				{
					Con_Printf("W_GetTexture: corrupt WAD3 file");
					return NULL;
				}

				tex->width = LittleLong(tex->width);
				tex->height = LittleLong(tex->height);
				for (j = 0;j < MIPLEVELS;j++)
					tex->offsets[j] = LittleLong(tex->offsets[j]);

				data = W_ConvertWAD3TextureHL(tex);

				free(tex);
                fclose(file);
				return data;
			}
		}
		else
			break;
	}
	tex->width = tex->width = 0;
	return NULL;
}
