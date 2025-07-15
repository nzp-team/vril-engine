//gl_fullbright.c
#include "../../../nzportable_def.h"

extern void DrawGLPoly (glpoly_t *p);

int FindFullbrightTexture (byte *pixels, int num_pix)
{
	int i;
	for (i = 0; i < num_pix; i++)
	if (pixels[i] > 223)
		return 1;
	return 0;
}

void ConvertPixels (byte *pixels, int num_pixels)
{
	int i;
	for (i = 0; i < num_pixels; i++)
		if (pixels[i] < 224)
			pixels[i] = 255;
}

void DrawFullBrightTextures (msurface_t *first_surf, int num_surfs)
{
	int i;
	msurface_t *fa;
	texture_t *t;
	if (r_fullbright.value)
		return;
	
	glEnable (GL_BLEND);
	
	for (fa = first_surf, i = 0; i < num_surfs; fa++, i++)
	{
		// find the correct texture
		t = R_TextureAnimation (fa->texinfo->texture);
		if (t->fullbright != -1 && fa->draw_this_frame)
		{
			if (t->luma) glBlendFunc (GL_ONE, GL_ONE);
			else glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			GL_Bind (t->fullbright);
			DrawGLPoly (fa->polys);
		}
		fa->draw_this_frame = 0;
	}
	
	glDisable (GL_BLEND);
	glBlendFunc (GL_ZERO, GL_SRC_COLOR);
}
