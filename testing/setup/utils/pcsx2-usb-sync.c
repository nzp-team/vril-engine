#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

static FILE *open_image(const char *path, const char *mode, const char *symbol)
{
	FILE *(*open_file)(const char *, const char *) = dlsym(RTLD_NEXT, symbol);
	FILE *file = open_file(path, mode);
	const char *name = strrchr(path, '/');
	if (file && !strcmp(name ? name + 1 : path, "usb.img"))
		setvbuf(file, NULL, _IONBF, 0);
	return file;
}

FILE *fopen(const char *path, const char *mode)
{
	return open_image(path, mode, "fopen");
}

FILE *fopen64(const char *path, const char *mode)
{
	return open_image(path, mode, "fopen64");
}
