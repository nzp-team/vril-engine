/*
 * Copyright (C) 2023 Shpuld and NZP Team.
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
 */
// r_color_quantization.c -- 8bpp to 4bpp quantizer for textures

#include "../nzportable_def.h"
#include <malloc.h>

typedef struct {
    byte           r;
    byte           g;
    byte           b;
    byte           a;
    unsigned short pixcount;
} colentry_t;
colentry_t colentries[256];
typedef byte bucket_t[256];

bucket_t buckets[16];
int bucket_counts[16];
int num_buckets = 0;

int bucket_queue_start = 0;
int bucket_queue_end = 0;
byte bucket_queue[64];

byte byte_to_halfbyte_map[256];

void
push_bucket_to_queue(int bucket)
{
    // Realisticilly this should not go past num_buckets * 2 - 1
    if (bucket_queue_end >= 64) {
        return;
    }
    bucket_queue[bucket_queue_end] = bucket;
    bucket_queue_end += 1;
}

void
bucket_queue_advance()
{
    bucket_queue_start += 1;
}

int
bucket_queue_length()
{
    return bucket_queue_end - bucket_queue_start;
}

void
add_to_bucket(int bucket, int color)
{
    buckets[bucket][bucket_counts[bucket]] = color;
    bucket_counts[bucket]++;
}

void
remove_last_from_bucket(int bucket)
{
    bucket_counts[bucket]--;
    buckets[bucket][bucket_counts[bucket]] = 0;
}

void
init_buckets()
{
    memset(buckets, 0, sizeof(buckets));
    memset(bucket_counts, 0, sizeof(bucket_counts));
    memset(colentries, 0, sizeof(colentries));
    memset(byte_to_halfbyte_map, 0, sizeof(byte_to_halfbyte_map));
    num_buckets = 0;
    bucket_queue_start = 0;
    bucket_queue_end = 0;
}

int
get_new_bucket()
{
    num_buckets += 1;
    return num_buckets - 1;
}

colentry_t *
color_of_bucket(int bucket, int index)
{
    return &(colentries[buckets[bucket][index]]);
}

int
get_color_index(int bucket, int index)
{
    return buckets[bucket][index];
}

void
sort_and_cut_bucket(int current_bucket, int depth, int target)
{
    // We already have max colors
    if (num_buckets >= target) {
        return;
    }

    int count = bucket_counts[current_bucket];
    // This bucket only has one color, can't get better than that
    if (count <= 1) {
        return;
    }

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

    // bubble sort, replace with comb sort or heap sort
    bool sorted = false;
    while (!sorted) {
        sorted = true; // this will change if a swap is done
        for (int i = 0; i < (count - 1); i++) {
            colentry_t * col1 = color_of_bucket(current_bucket, i);
            colentry_t * col2 = color_of_bucket(current_bucket, i + 1);
            bool swap         = false;
            switch (sortBy) {
                case 0: swap = col1->r > col2->r;
                    break;
                case 1: swap = col1->g > col2->g;
                    break;
                case 2: swap = col1->b > col2->b;
                    break;
            }
            if (swap) {
                byte prevval = buckets[current_bucket][i];
                byte nextval = buckets[current_bucket][i + 1];
                buckets[current_bucket][i]     = nextval;
                buckets[current_bucket][i + 1] = prevval;
                sorted = false;
            }
        }
    }

    // median cut, careful to not do split at 0 or length-1
    int pixelsCounted = 0;
    int split         = 1;
    for (int i = 0; i < count; i++) {
        if ((i > 0) && (pixelsCounted > pixelsInBucket * 0.5 || (i >= count - 2))) {
            split = i;
            break;
        }
        byte_to_halfbyte_map[get_color_index(current_bucket, i)] = current_bucket;
        colentry_t * col = color_of_bucket(current_bucket, i);
        pixelsCounted += col->pixcount;
    }

    int next_bucket = get_new_bucket();
    for (int i = count - 1; i >= split; i--) {
        byte colorindex = get_color_index(current_bucket, i);
        byte_to_halfbyte_map[colorindex] = next_bucket;
        add_to_bucket(next_bucket, colorindex);
        remove_last_from_bucket(current_bucket);
    }

    // queue up next buckets, favor one with more colors when choosing order
    if (split > (count / 2)) {
        push_bucket_to_queue(current_bucket);
        push_bucket_to_queue(next_bucket);
    } else {
        push_bucket_to_queue(next_bucket);
        push_bucket_to_queue(current_bucket);
    }
} /* sort_and_cut_bucket */

void
convert_8bpp_to_4bpp(const byte * indata, const byte * inpal, int width, int height, byte * outdata, byte * outpal)
{
    init_buckets();
    int current_bucket = get_new_bucket();

    // Set bucket pixelcounts
    for (int pixi = 0; pixi < width * height; pixi++) {
        byte color = indata[pixi];
        colentries[color].pixcount += 1;
    }

    int MAX_COLORS         = 256;
    bool has_transparency  = false;
    byte transparent_index = 0;
    int colors_used        = 0;
    // Set colors and fill first bucket
    for (int i = 0; i < MAX_COLORS; i++) {
        colentry_t * ce = &(colentries[i]);
        if (ce->pixcount == 0) {
            continue; // speed up by not processing unused colors
        }
        colors_used++;

        ce->r = inpal[i * 3 + 0];
        ce->g = inpal[i * 3 + 1];
        ce->b = inpal[i * 3 + 2];
        ce->a = 255;
        if (ce->r == 0 && ce->g == 0 && ce->b == 255) {
            ce->r = ce->g = ce->b = 128;
            ce->a = 255;
            has_transparency  = true;
            transparent_index = i;
            continue; // don't add transparencies to buckets
        }
        add_to_bucket(current_bucket, i);
    }

    // could check if bucket has 16 colors or less and early out
  
    int target_colors_count = MIN(16, colors_used);
    target_colors_count = has_transparency ? (target_colors_count - 1) : target_colors_count;
    push_bucket_to_queue(current_bucket);
    while (bucket_queue_length() > 0) {
        sort_and_cut_bucket(bucket_queue[bucket_queue_start], 0, target_colors_count);
        bucket_queue_advance();
    }

    if (has_transparency) {
        int transparent_bucket = get_new_bucket();
        byte_to_halfbyte_map[transparent_index] = transparent_bucket; // last index
        add_to_bucket(transparent_bucket, transparent_index);
    }

    int image_size = width * height * 0.5;

    for (int i = 0; i < image_size; i++) {
        byte color_index = indata[i * 2];
        outdata[i]  = byte_to_halfbyte_map[color_index];
        color_index = indata[i * 2 + 1];
        outdata[i] += byte_to_halfbyte_map[color_index] << 4;
    }

    for (int i = 0; i < num_buckets; i++) {
        int r     = 0;
        int g     = 0;
        int b     = 0;
        int alpha = 255;
        colentry_t * col;
        for (int j = 0; j < bucket_counts[i]; j++) {
            byte colindex = get_color_index(i, j);
            if (has_transparency && transparent_index == colindex) {
                r     = 128;
                g     = 128;
                b     = 128;
                alpha = 0;
                break;
            }
            col = color_of_bucket(i, j);
            r  += col->r;
            g  += col->g;
            b  += col->b;
        }
        r /= bucket_counts[i];
        g /= bucket_counts[i];
        b /= bucket_counts[i];

        ((unsigned int *) outpal)[i] = (alpha << 24) + (b << 16) + (g << 8) + r;
    }
} /* convert_8bpp_to_4bpp */
