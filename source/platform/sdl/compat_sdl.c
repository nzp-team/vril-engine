#include "../../nzportable_def.h"

size_t SDL_strlcpy(char *dst, const char *src, size_t size)
{
	size_t length = strlen(src);
	if (size) {
		size_t copy = length >= size ? size - 1 : length;
		memcpy(dst, src, copy);
		dst[copy] = '\0';
	}
	return length;
}

size_t SDL_strlcat(char *dst, const char *src, size_t size)
{
	size_t used = strnlen(dst, size);
	if (used == size) return size + strlen(src);
	return used + SDL_strlcpy(dst + used, src, size - used);
}
