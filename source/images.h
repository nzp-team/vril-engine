void tex_filebase (char *in, char *out);
byte* Image_LoadPixels (char* filename, int image_format);
int Image_LoadImage(char* filename, int image_format, int filter, qboolean keep, qboolean mipmap);
int loadrgbafrompal (char* name, int width, int height, byte* data);
int loadpcxas4bpp (char* filename, int filter);

#define IMAGE_PCX   1
#define IMAGE_TGA   2
#define IMAGE_PNG   4
#define IMAGE_JPG   8

