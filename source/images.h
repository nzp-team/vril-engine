byte* loadimagepixels (char* filename, qboolean complain, int matchwidth, int matchheight);
int loadtextureimage (char* filename, int matchwidth, int matchheight, qboolean complain, int filter, qboolean keep, qboolean mipmap);
int loadskyboxsideimage (char* filename, int matchwidth, int matchheight, qboolean complain, int filter);
int loadrgbafrompal (char* name, int width, int height, byte* data);