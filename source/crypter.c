/*
  DATA Decrypt
*/
#include <stdlib.h>
#include <ctype.h>
char rotate(char c, int key)
{
    int l = 'Z' - 'A';

    c += key % l;

    if(c < 'A')
	   c += l;

	if(c > 'Z')
	   c -= l;

	return c;
}

char encrypt(char c, int key)
{
    if(c >= 'a' && c <= 'z')
	   c = c + ('A'-'a'); // toupper

	if(c >= 'A' && c <= 'Z')
	   c = rotate(c, key);

	return c;
}

char decrypt(char c, int key)
{

    if(c < 'A' || c > 'Z')
	   return c;
    else
	   return rotate(c, key);

}

void* Q_malloc(size_t);

char *strencrypt(char *s, int key, int len)
{
	int i;
	char *result = Q_malloc(len);
	for(i = 0; i < len; i++)
		result[i] = encrypt(s[i], key);
	return result;
}

char *strdecrypt(char *s, int key, int len)
{
	int i;
	char *result = Q_malloc(len);
	for(i = 0; i < len; i++)
		result[i] = decrypt(s[i], -key);
	return result;
}

