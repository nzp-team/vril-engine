/*
Copyright (C) 2008 Eluan Miranda

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
#include <ogc/cache.h>
#include <ogc/gx.h>
#include <ogc/gx_struct.h>
#include <ogc/system.h>
#include <ogc/video.h>
#include <ogc/video_types.h>
#include <ogc/conf.h>
#include <malloc.h>
#include <gccore.h>

// ELUTODO: blank all the framebuffers to prevent artifacts before rendering takes place. Happens between the frontend ending and the quake console showing up

#include "../../nzportable_def.h"

extern void				*framebuffer[2];
extern u32				fb;
extern GXRModeObj		*rmode;

static void	*gp_fifo;
static const size_t	fifo_size = 1024 * 256;

#define WARP_WIDTH              640
#define WARP_HEIGHT             480

static int scr_width, scr_height;

static bool vidmode_active = false;

int texture_extension_number = 1;

/*-----------------------------------------------------------------------*/

unsigned		d_8to24table[256];

float		gldepthmin, gldepthmax;

static float vid_gamma = 1.0;

/*-----------------------------------------------------------------------*/

Mtx44 perspective;
Mtx view, model, modelview;

cvar_t vid_tvborder = {"vid_tvborder", "0", (qboolean)true};
cvar_t vid_conmode = {"vid_conmode", "0", (qboolean)true};

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}

void VID_Shutdown(void)
{
	if (vidmode_active)
	{
		// Free the FIFO.
		free(MEM_K1_TO_K0(gp_fifo));
		gp_fifo = 0;

		vidmode_active = false;
	}
}

void VID_ShiftPalette(unsigned char *p)
{
	VID_SetPalette(p);
}

void	VID_SetPalette (unsigned char *palette)
{
	byte	*pal;
	unsigned r,g,b;
	unsigned v;
	unsigned short i;
	unsigned	*table;

//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to24table;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;
		
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		*table++ = v;
	}
	//d_8to24table[255] &= 0xffffff;	// 255 is transparent
	d_8to24table[255] = 0;				// ELUTODO: will look prettier until we solve the filtering issue
}

void VID_ConModeUpdate(void)
{
	// update console resolution
	/*
	switch((int)vid_conmode.value)
	{
		default:
		case 0:
			vid.conwidth = 320;
			vid.conheight = 240;
			break;
		case 1:
			vid.conwidth = 400;
			vid.conheight = 300;
			break;
		case 2:
			vid.conwidth = 480;
			vid.conheight = 360;
			break;
		case 3:
			vid.conwidth = 560;
			vid.conheight = 420;
			break;
		case 4:
			vid.conwidth = 640;
			vid.conheight = 480;
			break;
	}
	*/
	
	vid.conwidth = vid.width;
	vid.conheight = vid.height;
	
	if (vid.conheight > scr_height)
		vid.conheight = scr_height;
	if (vid.conwidth > scr_width)
		vid.conwidth = scr_width;

	//conback->width = vid.conwidth;
	//conback->height = vid.conheight;
}

/*
===============
GL_Init
===============
*/
void GL_Init (void)
{
	f32 yscale;
	u32 xfbHeight;

	GXColor background = {0, 0, 0, 0xff};

	// Initialise GX.
	gp_fifo = memalign(32, fifo_size);
	if (!gp_fifo)
		Sys_Error("VID_Init: !gp_fifo\n");

	gp_fifo = MEM_K0_TO_K1(gp_fifo);
	memset(gp_fifo, 0, fifo_size);
	GX_Init(gp_fifo, fifo_size);

	// clears the bg to color and clears the z buffer
	GX_SetCopyClear(background, GX_MAX_Z24);

	// other gx setup
	GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
	yscale = GX_GetYScaleFactor(rmode->efbHeight,rmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopySrc(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth,xfbHeight);
	GX_SetCopyFilter(rmode->aa,rmode->sample_pattern,GX_TRUE,rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering,((rmode->viHeight==2*rmode->xfbHeight)?GX_ENABLE:GX_DISABLE));
	
	if (rmode->aa)
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	else
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	
	GX_CopyDisp(framebuffer[fb & 1],GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	GX_SetZCompLoc(false); // ELUTODO
	
	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
	
	// setup the vertex attribute table
	// describes the data
	// args: vat location 0-7, type of data, data format, size, scale
	// so for ex. in the first call we are sending position data with
	// 3 values X,Y,Z of size F32. scale sets the number of fractional
	// bits for non float data.
 
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
 
	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);

	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_InvVtxCache();
	GX_InvalidateTexAll();
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	// ELUTODO: lol at the * 2 on height
	*x = 0;
	*y = vid_tvborder.value * 200;
	*width = scr_width;
	*height = scr_height - (vid_tvborder.value * 400);

	GX_SetScissor(*x,*y,*width > 640 ? 640 : *width,*height);
	
	// ELUTODO: really necessary?
	//GX_InvVtxCache();
	GX_InvalidateTexAll();
}

void GL_EndRendering (void)
{
	GX_SetColorUpdate(GX_TRUE);
	GX_SetAlphaUpdate(GX_TRUE);
	
	// Start copying the frame buffer every vsync.
	GX_CopyDisp(framebuffer[fb], GX_TRUE);

	// Finish up any graphics operations.
	GX_DrawDone();
	GX_Flush();
	
	VIDEO_SetNextFramebuffer(framebuffer[fb]);
	VIDEO_Flush();
	// Keep framerate		
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) {
		VIDEO_WaitVSync();
	}
	
	fb ^= 1;
}	

// This is not the "v_gamma/gamma" cvar
static void Check_Gamma (unsigned char *pal)
{
	float	f, inf;
	unsigned char	palette[768];
	int		i;

	if ((i = COM_CheckParm("-gamma")) == 0) {
		vid_gamma = 0.7; // default to 0.7 on non-3dfx hardware
	} else
		vid_gamma = atof(com_argv[i+1]);

	for (i=0 ; i<768 ; i++)
	{
		f = pow ( (pal[i]+1)/256.0 , vid_gamma );
		inf = f*255 + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		palette[i] = inf;
	}

	memcpy (pal, palette, sizeof(palette));
	
	Build_Gamma_Table ();
}

// ELUTODO: proper widescreen support
// ELUTODO: one more mess to remove: glwidth is the VIEWAREA and scr_width is the WHOLE SCREEN (will have to clean this up if I want to implemented split-screen multiplayer
// ELUTODO: crosshair, osk are NOT right on higher 2d resolutions and split-screen setups (is the use of scr_vrect for cl_crossx/y right?)
// scr_width/height = 3D res, vid.conwidth/conheight = 2D res, vid.width/height = "natural" resolution (320x{200,240})
// many things rely on a minimum resolution of 320x{200,240}
void VID_Init(unsigned char *palette)
{
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

// interpret command-line params

// only multiples of eight, please
// set vid parameters
	scr_width = CONF_GetAspectRatio() == CONF_ASPECT_16_9 ? 854 : rmode->fbWidth;
	scr_height = rmode->efbHeight;

	vid.width = CONF_GetAspectRatio() == CONF_ASPECT_16_9 ? 854 : 640; //640
	vid.height = 480; //480

	if (vid.height > scr_height)
		vid.height = scr_height;
	if (vid.width > scr_width)
		vid.width = scr_width;

	vid.aspect = ((float)vid.height / (float)vid.width) * (640.0 / 480.0); //640.0/480.0
	vid.numpages = 2;

	//GL_Init();

	Check_Gamma(palette);
	VID_SetPalette(palette);

	Con_Printf ("Video mode %dx%d initialized.\n", scr_width, scr_height);

	vid.recalc_refdef = 1;				// force a surface cache flush

	Cvar_RegisterVariable(&vid_tvborder);
	Cvar_RegisterVariable(&vid_retromode);
	Cvar_RegisterVariable(&vid_conmode);

	vidmode_active = true;
}
