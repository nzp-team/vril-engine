/*
Copyright (C) 1996-1997 Id Software, Inc.

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

// screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "../../../nzportable_def.h"

/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Con_Printf ();

net 
turn off messages option

the refresh is allways rendered, unless the console is full screen


console is:
	notify lines
	half
	full
	

*/


int			glx, gly, glwidth, glheight;

// only the refresh window will be updated unless these variables are flagged 
int			scr_copytop;
int			scr_copyeverything;

float		scr_con_current;
float		scr_conlines;		// lines of console to display

float		oldscreensize, oldfov, oldsbaralpha;

cvar_t viewsize = {"viewsize", "100", true};
cvar_t scr_fov = {"fov", "90", true};

cvar_t scr_conspeed = {"scr_conspeed", "300", true};
cvar_t scr_centertime = {"scr_centertime", "2", true};
cvar_t scr_sbaralpha = {"scr_sbaralpha", "0.50", true};
cvar_t scr_conscale = {"scr_conscale", "1", true};
cvar_t scr_conwidth = {"scr_conwidth", "0", true};
cvar_t scr_conalpha = {"scr_conalpha", "0.75", true};
cvar_t scr_menuscale = {"scr_menuscale", "2", true};
cvar_t scr_sbarscale = {"scr_sbarscale", "1", true};
cvar_t scr_crosshairscale = {"scr_crosshairscale", "1", true};

cvar_t showram = {"showram", "1", true};
cvar_t showturtle = {"showturtle", "0", true};
cvar_t showpause = {"showpause", "1", true};
cvar_t scr_printspeed = {"scr_printspeed", "8", true};
cvar_t 	scr_showfps = {"scr_showfps", "1"};
cvar_t	scr_loadscreen = {"scr_loadscreen","1"};
cvar_t gl_triplebuffer = {"gl_triplebuffer", "0", true};
cvar_t cl_crosshair_debug = {"cl_crosshair_debug", "0", true};

extern	cvar_t	crosshair;

bool	scr_initialized;		// ready to draw

int      	hitmark;

int			scr_fullupdate;

int			clearconsole;
int			clearnotify;

int			sb_lines;

viddef_t	vid;				// global video state

vrect_t		scr_vrect;

qboolean	scr_disabled_for_loading;
qboolean	scr_drawloading;
float		scr_disabled_time;

qboolean	block_drawing;

void SCR_ScreenShot_f (void);

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[1024];
float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
	strncpy (scr_centerstring, str, sizeof(scr_centerstring)-1);
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

// count the number of lines for centering
	scr_center_lines = 1;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}


void SCR_DrawCenterString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;
	

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = 200*0.35;	//johnfitz -- 320x200 coordinate system
	else
		y = 48;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (320 - l*8)/2;	//johnfitz -- 320x200 coordinate system
		for (j=0 ; j<l ; j++, x+=8)
		{
			Draw_Character (x, y, start[j]);	
			if (!remaining--)
				return;
		}
			
		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	scr_copytop = 1;
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;
	
	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString ();
}

//=============================================================================

/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
        float   a;
        float   x;

        if (fov_x < 1 || fov_x > 179)
                Sys_Error ("Bad fov: %f", fov_x);

        x = width/tan(fov_x/360*M_PI);

        a = atan (height/x);

        a = a*360/M_PI;

        return a;
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
static void SCR_CalcRefdef (void)
{
	vrect_t		vrect;
	float		size, scale;
	int		h;
	bool		full = false;


	scr_fullupdate = 0;		// force a background redraw
	vid.recalc_refdef = 0;

//========================================
	
	// bound viewsize
	if (viewsize.value < 30)
		Cvar_Set ("viewsize","30");
	if (viewsize.value > 120)
		Cvar_Set ("viewsize","120");

	// bound field of view
	if (scr_fov.value < 10)
		Cvar_Set ("fov","10");
	if (scr_fov.value > 170)
		Cvar_Set ("fov","170");
	
	vid.recalc_refdef = 0;

	//johnfitz -- rewrote this section
	size = viewsize.value;
	scale = Q_CLAMP (1.0, scr_sbarscale.value, (float)glwidth / 320.0);

	if (size >= 120 || cl.intermission || scr_sbaralpha.value < 1) //johnfitz -- scr_sbaralpha.value
		sb_lines = 0;
	else if (size >= 110)
		sb_lines = 24 * scale;
	else
		sb_lines = 48 * scale;

	size = fmin(viewsize.value, 100) / 100;
	//johnfitz

	//johnfitz -- rewrote this section
	r_refdef.vrect.width = fmax(glwidth * size, 96); //no smaller than 96, for icons
	r_refdef.vrect.height = fmin(glheight * size, glheight - sb_lines); //make room for sbar
	r_refdef.vrect.x = (glwidth - r_refdef.vrect.width)/2;
	r_refdef.vrect.y = (glheight - sb_lines - r_refdef.vrect.height)/2;
	//johnfitz

	r_refdef.fov_x = scr_fov.value;
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

	scr_vrect = r_refdef.vrect;
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	Cvar_SetValue ("viewsize", viewsize.value+10);
	vid.recalc_refdef = 1;
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	Cvar_SetValue ("viewsize", viewsize.value-10);
	vid.recalc_refdef = 1;
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	Cvar_RegisterVariable (&viewsize);
	Cvar_RegisterVariable (&scr_fov);
	Cvar_RegisterVariable (&scr_sbaralpha);
	Cvar_RegisterVariable (&scr_conalpha);
	Cvar_RegisterVariable (&scr_conwidth);
	Cvar_RegisterVariable (&scr_conscale);
	Cvar_RegisterVariable (&scr_menuscale);
	Cvar_RegisterVariable (&scr_sbarscale);
	Cvar_RegisterVariable (&scr_conspeed);
	Cvar_RegisterVariable (&showram);
	Cvar_RegisterVariable (&showturtle);
	Cvar_RegisterVariable (&showpause);
	Cvar_RegisterVariable (&scr_centertime);
	Cvar_RegisterVariable (&scr_printspeed);
	Cvar_RegisterVariable (&cl_crosshair_debug);
	Cvar_RegisterVariable (&scr_showfps);
	Cvar_RegisterVariable (&scr_loadscreen);
	Cvar_RegisterVariable (&gl_triplebuffer);

//
// register our commands
//
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);

	hitmark = Image_LoadImage("gfx/hud/hit_marker", IMAGE_TGA, 0, true, false);

	scr_initialized = true;
}

//=============================================================================

//============================================================================

/*
==============
SCR_DrawFPS -- johnfitz
==============
*/
void SCR_DrawFPS (void)
{
	static double	oldtime = 0;
	static double	lastfps = 0;
	static int	oldframecount = 0;
	double	elapsed_time;
	int	frames;

	elapsed_time = realtime - oldtime;
	frames = r_framecount - oldframecount;

	if (elapsed_time < 0 || frames < 0)
	{
		oldtime = realtime;
		oldframecount = r_framecount;
		return;
	}
	// update value every 3/4 second
	if (elapsed_time > 0.75)
	{
		lastfps = frames / elapsed_time;
		oldtime = realtime;
		oldframecount = r_framecount;
	}

	if (scr_showfps.value)
	{
		char	st[16];
		sprintf (st, "%4.0f fps", lastfps);
		Draw_String (vid.width - 40, 30, st);
	}
}

/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
	//johnfitz -- let's hack away the problem of slow console when host_timescale is <0
	extern cvar_t host_timescale;
	float timescale;
	//johnfitz
	
	Con_CheckResize ();
	
	if (scr_drawloading)
		return;		// never a console with loading plaque
		
// decide on the height of the console
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

	if (con_forcedup)
	{
		scr_conlines = vid.height;		// full screen
		scr_con_current = scr_conlines;
	}
	else if (key_dest == key_console)
		scr_conlines = vid.height/2;	// half screen
	else
		scr_conlines = 0;				// none visible
	
	timescale = (host_timescale.value > 0) ? host_timescale.value : 1; //johnfitz -- timescale
	
	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed.value*host_frametime/timescale; //johnfitz -- timescale
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed.value*host_frametime/timescale; //johnfitz -- timescale
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (clearconsole++ < vid.numpages)
	{
	}
	else if (clearnotify++ < vid.numpages)
	{
	}
	else
		con_notifylines = 0;
}
	
/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	if (scr_con_current)
	{
		scr_copyeverything = 1;
		Con_DrawConsole (scr_con_current, true, 2);
		clearconsole = 0;
	}
	else
	{
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();	// only draw notify in game
	}
}


/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/* 
================== 
SCR_ScreenShot_f
================== 
*/  
void SCR_ScreenShot_f (void) 
{
	byte		*buffer;
	char		pcxname[80]; 
	char		checkname[MAX_OSPATH];
	int			i, c, temp;
// 
// find a file name to save it to 
// 
	strcpy(pcxname,"quake00.tga");
		
	for (i=0 ; i<=99 ; i++) 
	{ 
		pcxname[5] = i/10 + '0'; 
		pcxname[6] = i%10 + '0'; 
		sprintf (checkname, "%s/%s", com_gamedir, pcxname);
		if (Sys_FileTime(checkname) == -1)
			break;	// file doesn't exist
	} 
	if (i==100) 
	{
		Con_Printf ("SCR_ScreenShot_f: Couldn't create a PCX file\n"); 
		return;
 	}


	buffer = malloc(glwidth*glheight*3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = glwidth&255;
	buffer[13] = glwidth>>8;
	buffer[14] = glheight&255;
	buffer[15] = glheight>>8;
	buffer[16] = 24;	// pixel size

	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer+18 ); 

	// swap rgb to bgr
	c = 18+glwidth*glheight*3;
	for (i=18 ; i<c ; i+=3)
	{
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}
	COM_WriteFile (pcxname, buffer, glwidth*glheight*3 + 18 );

	free (buffer);
	Con_Printf ("Wrote %s\n", pcxname);
} 


//=============================================================================


/*
===============
SCR_BeginLoadingPlaque

================
*/
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds (true);

	if (cls.state != ca_connected)
		return;
	if (cls.signon != SIGNONS)
		return;
	
// redraw with no console and the loading plaque
	Con_ClearNotify ();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	scr_fullupdate = 0;
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;
	scr_fullupdate = 0;
}

/*
===============
SCR_EndLoadingPlaque

================
*/
void SCR_EndLoadingPlaque (void)
{
	scr_disabled_for_loading = false;
	scr_fullupdate = 0;
	Con_ClearNotify ();
}

//=============================================================================

char	*scr_notifystring;
bool	scr_drawdialog;

void SCR_DrawNotifyString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	
	start = scr_notifystring;

	y = (int)(200*0.35);

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (320 - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
			Draw_Character (x, y, start[j]);	
			
		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

/*
==================
SCR_ModalMessage

Displays a text string in the center of the screen and waits for a Y or N
keypress.  
==================
*/
int SCR_ModalMessage (char *text)
{
	if (cls.state == ca_dedicated)
		return true;

	scr_notifystring = text;
 
// draw a fresh screen
	scr_fullupdate = 0;
	scr_drawdialog = true;
	SCR_UpdateScreen ();
	scr_drawdialog = false;
	
	S_ClearBuffer ();		// so dma doesn't loop current sound

	do
	{
		key_count = -1;		// wait for a key down and up
		Sys_SendKeyEvents ();
	} while (key_lastpress != 'y' && key_lastpress != 'n' && key_lastpress != K_ESCAPE);

	scr_fullupdate = 0;
	SCR_UpdateScreen ();

	return key_lastpress == 'y';
}


//=============================================================================

/*
===============
SCR_BringDownConsole

Brings the console down and fades the palettes back to normal
================
*/
void SCR_BringDownConsole (void)
{
	int		i;
	
	scr_centertime_off = 0;
	
	for (i=0 ; i<20 && scr_conlines != scr_con_current ; i++)
		SCR_UpdateScreen ();

	cl.cshifts[0].percent = 0;		// no area contents palette on next frame
	VID_SetPalette (host_basepal);
}

extern cvar_t crosshair;
extern qboolean croshhairmoving;
//extern cvar_t cl_zoom;

int CrossHairWeapon (void)
{
    int i;
	switch(cl.stats[STAT_ACTIVEWEAPON])
	{
		case W_COLT:
		case W_BIATCH:
		case W_357:
		case W_KILLU:
			i = 22;
			break;
		case W_PTRS:
		case W_PENETRATOR:
		case W_KAR_SCOPE:
		case W_HEADCRACKER:
		case W_KAR:
		case W_ARMAGEDDON:
		case W_SPRING:
		case W_PULVERIZER:
			i = 65;
			break;
		case W_MP40:
		case W_AFTERBURNER:
		case W_STG:
		case W_SPATZ:
		case W_THOMPSON:
		case W_GIBS:
		case W_BAR:
		case W_WIDOW:
		case W_PPSH:
		case W_REAPER:
		case W_RAY:
		case W_PORTER:
		case W_TYPE:
		case W_SAMURAI:
		case W_FG:
		case W_IMPELLER:
		case W_MP5:
		case W_KOLLIDER:
			i = 10;
			break;
		case W_BROWNING:
		case W_ACCELERATOR:
		case W_MG:
		case W_BARRACUDA:
			i = 30;
			break;
		case W_SAWNOFF:
		case W_SNUFF:
			i = 50;
			break;
		case W_TRENCH:
		case W_GUT:
		case W_DB:
		case W_BORE:
			i = 35;
			break;
		case W_GEWEHR:
		case W_COMPRESSOR:
		case W_M1:
		case W_M1000:
		case W_M1A1:
		case W_WIDDER:
			i = 5;
			break;
		case W_PANZER:
		case W_LONGINUS:
		case W_TESLA:
			i = 0;
			break;
		default:
			i = 0;
			break;
	}

	i *= 0.68;
	i += 6;

    if (cl.perks & 64)
        i *= 0.75;

    return i;
}
int CrossHairMaxSpread (void)
{
	int i;
	switch(cl.stats[STAT_ACTIVEWEAPON])
	{
		case W_COLT:
		case W_BIATCH:
		case W_STG:
		case W_SPATZ:
		case W_MP40:
		case W_AFTERBURNER:
		case W_THOMPSON:
		case W_GIBS:
		case W_BAR:
		case W_WIDOW:
		case W_357:
		case W_KILLU:
		case W_BROWNING:
		case W_ACCELERATOR:
		case W_FG:
		case W_IMPELLER:
		case W_MP5:
		case W_KOLLIDER:
		case W_MG:
		case W_BARRACUDA:
		case W_PPSH:
		case W_REAPER:
		case W_RAY:
		case W_PORTER:
		case W_TYPE:
		case W_SAMURAI:
			i = 48;
			break;
		case W_PTRS:
		case W_PENETRATOR:
		case W_KAR_SCOPE:
		case W_HEADCRACKER:
		case W_KAR:
		case W_ARMAGEDDON:
		case W_SPRING:
		case W_PULVERIZER:
			i = 75;
			break;
		case W_SAWNOFF:
		case W_SNUFF:
			i = 50;
			break;
		case W_DB:
		case W_BORE:
		case W_TRENCH:
		case W_GUT:
		case W_GEWEHR:
		case W_COMPRESSOR:
		case W_M1:
		case W_M1000:
		case W_M1A1:
		case W_WIDDER:
			i = 35;
			break;
		case W_PANZER:
		case W_LONGINUS:
		case W_TESLA:
			i = 0;
			break;
		default:
			i = 0;
			break;
	}

	i *= 0.68;
	i += 6;

    if (cl.perks & 64)
        i *= 0.75;

    return i;
}

extern int	sniper_scope;
extern float crosshair_opacity;
extern cvar_t cl_crosshair_debug;
extern qboolean crosshair_pulse_grenade;
void Draw_Crosshair (void)
{	
	if (cl_crosshair_debug.value) {
		Draw_FillByColor(vid.width/2, 0, 1, 240, 255, 0, 0, 255);
		Draw_FillByColor(0, vid.height/2, 400, 1, 0, 255, 0, 255);
	}

	if (cl.stats[STAT_HEALTH] <= 20)
		return;

	if (cl.stats[STAT_ZOOM] == 2)
		Draw_Pic (-39, -15, sniper_scope);
   	if (Hitmark_Time > sv.time)
	   Draw_ColoredStretchPic ((vid.width - 16)/2, (vid.height - 16)/2, hitmark, 16, 16, 255, 255, 255, 225);

	// Make sure to do this after hitmark drawing.
	if (cl.stats[STAT_ZOOM] == 2 || cl.stats[STAT_ZOOM] == 1)
		return;

	if (!crosshair_opacity)
		crosshair_opacity = 255;

	float col;

	if (sv_player->v.facingenemy == 1) {
		col = 0;
	} else {
		col = 255;
	}

	// crosshair moving
	if (crosshair_spread_time > sv.time && crosshair_spread_time)
    {
        cur_spread = cur_spread + 10;
		crosshair_opacity = 128;

		if (cur_spread >= CrossHairMaxSpread())
			cur_spread = CrossHairMaxSpread();
    }
	// crosshair not moving
	else if (crosshair_spread_time < sv.time && crosshair_spread_time)
    {
        cur_spread = cur_spread - 4;
		crosshair_opacity = 255;

		if (cur_spread <= 0) {
			cur_spread = 0;
			crosshair_spread_time = 0;
		}
    }

	int x_value, y_value;
    int crosshair_offset;

	// Standard crosshair (+)
	if (crosshair.value == 1) {
		crosshair_offset = CrossHairWeapon() + cur_spread;
		if (CrossHairMaxSpread() < crosshair_offset || croshhairmoving)
			crosshair_offset = CrossHairMaxSpread();

		if (sv_player->v.view_ofs[2] == 8) {
			crosshair_offset *= 0.80f;
		} else if (sv_player->v.view_ofs[2] == -10) {
			crosshair_offset *= 0.65f;
		}

		crosshair_offset_step += (crosshair_offset - crosshair_offset_step) * 0.5f;

		x_value = (vid.width - 3)/2.0f - crosshair_offset_step;
		y_value = (vid.height - 1)/2.0f;
		Draw_FillByColor(x_value, y_value, 3, 1, 255, (int)col, (int)col, (int)crosshair_opacity);

		x_value = (vid.width - 3)/2.0f + crosshair_offset_step;
		y_value = (vid.height - 1)/2.0f;
		Draw_FillByColor(x_value, y_value, 3, 1, 255, (int)col, (int)col, (int)crosshair_opacity);

		x_value = (vid.width - 1)/2.0f;
		y_value = (vid.height - 3)/2.0f - crosshair_offset_step;
		Draw_FillByColor(x_value, y_value, 1, 3, 255, (int)col, (int)col, (int)crosshair_opacity);

		x_value = (vid.width - 1)/2.0f;
		y_value = (vid.height - 3)/2.0f + crosshair_offset_step;
		Draw_FillByColor(x_value, y_value, 1, 3, 255, (int)col, (int)col, (int)crosshair_opacity);
	}
	// Area of Effect (o)
	else if (crosshair.value == 2) {
		Draw_CharacterRGBA((vid.width)/2-4, (vid.height)/2, 'O', 255, (int)col, (int)col, (int)crosshair_opacity, 2);
	}
	// Dot crosshair (.)
	else if (crosshair.value == 3) {
		Draw_CharacterRGBA((vid.width - 8)/2, (vid.height - 8)/2, '.', 255, (int)col, (int)col, (int)crosshair_opacity, 2);
	}
	// Grenade crosshair
	else if (crosshair.value == 4) {
		if (crosshair_pulse_grenade) {
			crosshair_offset_step = 0;
			cur_spread = 2;
		}

		crosshair_pulse_grenade = false;

		crosshair_offset = 12 + cur_spread;
		crosshair_offset_step += (crosshair_offset - crosshair_offset_step) * 0.5f;

		x_value = (vid.width - 3)/2.0f - crosshair_offset_step;
		y_value = (vid.height - 1)/2.0f;
		Draw_FillByColor(x_value, y_value, 3, 1, 255, 255, 255, 255);

		x_value = (vid.width - 3)/2.0f + crosshair_offset_step;
		y_value = (vid.height - 1)/2.0f;
		Draw_FillByColor(x_value, y_value, 3, 1, 255, 255, 255, 255);

		x_value = (vid.width - 1)/2.0f;
		y_value = (vid.height - 3)/2.0f - crosshair_offset_step;
		Draw_FillByColor(x_value, y_value, 1, 3, 255, 255, 255, 255);

		x_value = (vid.width - 1)/2.0f;
		y_value = (vid.height - 3)/2.0f + crosshair_offset_step;
		Draw_FillByColor(x_value, y_value, 1, 3, 255, 255, 255, 255);
	}
}

char	scr_usestring[64];
char 	scr_usestring2[64];
float	scr_usetime_off = 0.0f;
int	button_pic_x;
extern int 	b_circle;
extern int 	b_square;
extern int 	b_cross;
extern int 	b_triangle;
extern int 	b_left;
extern int 	b_right;
extern int 	b_up;
extern int 	b_down;
extern int 	b_lt;
extern int 	b_rt;
extern int 	b_start;
extern int 	b_select;

/*
==============
SCR_UsePrint

Similiar to above, but will also print the current button for the action.
==============
*/

int GetButtonIcon (char *buttonname)
{
	int		j;
	int		l;
	char	*b;
	l = strlen(buttonname);

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, buttonname, l) )
		{
			if (!strcmp(Key_KeynumToString(j), "UPARROW"))
				return b_up = Image_LoadImage ("gfx/butticons/dpadup", IMAGE_TGA, 0, true, false);
			else if (!strcmp(Key_KeynumToString(j), "DOWNARROW"))
				return b_down = Image_LoadImage ("gfx/butticons/dpaddown", IMAGE_TGA, 0, true, false);
			else if (!strcmp(Key_KeynumToString(j), "LEFTARROW"))
				return b_left = Image_LoadImage ("gfx/butticons/dpadleft", IMAGE_TGA, 0, true, false);
			else if (!strcmp(Key_KeynumToString(j), "RIGHTARROW"))
				return b_right = Image_LoadImage ("gfx/butticons/dpadright", IMAGE_TGA, 0, true, false);
			else if (!strcmp(Key_KeynumToString(j), "SELECT"))
				return b_select = Image_LoadImage ("gfx/butticons/funcselect", IMAGE_TGA, 0, true, false);
			else if (!strcmp(Key_KeynumToString(j), "TRIANGLE"))
				return b_triangle = Image_LoadImage ("gfx/butticons/fbtntriangle", IMAGE_TGA, 0, true, false);
			else if (!strcmp(Key_KeynumToString(j), "CIRCLE"))
				return b_circle = Image_LoadImage ("gfx/butticons/fbtncircle", IMAGE_TGA, 0, true, false);
			else if (!strcmp(Key_KeynumToString(j), "CROSS"))
				return b_cross = Image_LoadImage ("gfx/butticons/fbtncross", IMAGE_TGA, 0, true, false);
			else if (!strcmp(Key_KeynumToString(j), "SQUARE"))
				return b_square = Image_LoadImage ("gfx/butticons/fbtnsquare", IMAGE_TGA, 0, true, false);
			else if (!strcmp(Key_KeynumToString(j), "LTRIGGER"))
				return b_lt = Image_LoadImage ("gfx/butticons/backl1", IMAGE_TGA, 0, true, false);
			else if (!strcmp(Key_KeynumToString(j), "RTRIGGER"))
				return b_rt = Image_LoadImage ("gfx/butticons/backr1", IMAGE_TGA, 0, true, false);
		}
	}
	return b_cross = Image_LoadImage ("gfx/butticons/fbtncross", IMAGE_TGA, 0, true, false);
}

char *GetUseButtonL ()
{
	int		j;
	int		l;
	char	*b;
	l = strlen("+use");

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, "+use", l) )
		{
			if (!strcmp(Key_KeynumToString(j), "SELECT") ||
				!strcmp(Key_KeynumToString(j), "LTRIGGER") ||
				!strcmp(Key_KeynumToString(j), "RTRIGGER") ||
				!strcmp(Key_KeynumToString(j), "HOME"))
				return "  ";
			else
				return " ";
		}
	}
	return " ";
}

char *GetGrenadeButtonL ()
{
	int		j;
	int		l;
	char	*b;
	l = strlen("+grenade");

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, "+grenade", l) )
		{
			if (!strcmp(Key_KeynumToString(j), "SELECT") ||
				!strcmp(Key_KeynumToString(j), "LTRIGGER") ||
				!strcmp(Key_KeynumToString(j), "RTRIGGER") ||
				!strcmp(Key_KeynumToString(j), "HOME"))
				return "  ";
			else
				return " ";
		}
	}
	return " ";
}

char *GetPerkName (int perk)
{
	switch (perk)
	{
		case 1:
			return "Quick Revive";
		case 2:
			return "Juggernog";
		case 3:
			return "Speed Cola";
		case 4:
			return "Double Tap";
		case 5:
			return "Stamin-Up";
		case 6:
			return "PhD Flopper";
		case 7:
			return "Deadshot Daiquiri";
		case 8:
			return "Mule Kick";
		default:
			return "NULL";
	}
}

void SCR_UsePrint (int type, int cost, int weapon)
{
	//naievil -- fixme
    char s[128];
	char c[128];

    switch (type)
	{
		case 0://clear
			strcpy(s, "");
			strcpy(c, "");
			break;
		case 1://door
			strcpy(s, va("Hold  %s  to open Door\n", GetUseButtonL()));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 2://debris
			strcpy(s, va("Hold  %s  to remove Debris\n", GetUseButtonL()));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 3://ammo
			strcpy(s, va("Hold  %s  to buy Ammo for %s\n", GetUseButtonL(), pr_strings+sv_player->v.Weapon_Name_Touch));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 4://weapon
			strcpy(s, va("Hold  %s  to buy %s\n", GetUseButtonL(), pr_strings+sv_player->v.Weapon_Name_Touch));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 5://window
			strcpy(s, va("Hold  %s  to Rebuild Barrier\n", GetUseButtonL()));
			strcpy(c, "");
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 6://box
			strcpy(s, va("Hold  %s  to for Mystery Box\n", GetUseButtonL()));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 7://box take
			strcpy(s, va("Hold  %s  for %s\n", GetUseButtonL(), pr_strings+sv_player->v.Weapon_Name_Touch));
			strcpy(c, "");
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 8://power
			strcpy(s, "The Power must be Activated first\n");
			strcpy(c, "");
			button_pic_x = 100;
			break;
		case 9://perk
			strcpy(s, va("Hold  %s  to buy %s\n", GetUseButtonL(), GetPerkName(weapon)));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 10://turn on power
			strcpy(s, va("Hold  %s  to Turn On the Power\n", GetUseButtonL()));
			strcpy(c, "");
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 11://turn on trap
			strcpy(s, va("Hold  %s  to Activate the Trap\n", GetUseButtonL()));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 12://PAP
			strcpy(s, va("Hold  %s  to Pack-a-Punch\n", GetUseButtonL()));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 13://revive
			strcpy(s, va("Hold  %s  to Fix your Code.. :)\n", GetUseButtonL()));
			strcpy(c, "");
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 14://use teleporter (free)
			strcpy(s, va("Hold  %s  to use Teleporter\n", GetUseButtonL()));
			strcpy(c, "");
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 15://use teleporter (cost)
			strcpy(s, va("Hold  %s  to use Teleporter\n", GetUseButtonL()));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 16://tp cooldown
			strcpy(s, "Teleporter is cooling down\n");
			strcpy(c, "");
			button_pic_x = 100;
			break;
		case 17://link
			strcpy(s, va("Hold  %s  to initiate link to pad\n", GetUseButtonL()));
			strcpy(c, "");
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 18://no link
			strcpy(s, "Link not active\n");
			strcpy(c, "");
			button_pic_x = 100;
			break;
		case 19://finish link
			strcpy(s, va("Hold  %s  to link pad with core\n", GetUseButtonL()));
			strcpy(c, "");
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 20://buyable ending
			strcpy(s, va("Hold  %s  to End the Game\n", GetUseButtonL()));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		default:
			Con_Printf ("No type defined in engine for useprint\n");
			break;
	}

	strncpy (scr_usestring, va(s), sizeof(scr_usestring)-1);
	strncpy (scr_usestring2, va(c), sizeof(scr_usestring2)-1);
	scr_usetime_off = 0.1;
}


void SCR_DrawUseString (void)
{
	int	x, y, iconTexNum;

	if (cl.stats[STAT_HEALTH] < 0)
		return;
// the finale prints the characters one at a time

	y = vid.height/2;
    x = (vid.width - getTextWidth(scr_usestring, 1))/2;
	iconTexNum = GetButtonIcon("+use");

	Draw_ColoredStringCentered(y, scr_usestring, 255, 255, 255, 255, 1);
	Draw_ColoredStringCentered(y + 10, scr_usestring2, 255, 255, 255, 255, 1);

	if (button_pic_x != 100)
	Draw_ColorPic (x + button_pic_x, y-4, iconTexNum, 255, 255, 255, 255);
}

void SCR_CheckDrawUseString (void)
{
	scr_copytop = 1;

	scr_usetime_off -= (float)host_frametime;

	if ((scr_usetime_off <= 0 && !cl.intermission) || key_dest != key_game || cl.stats[STAT_HEALTH] <= 0)
		return;

	SCR_DrawUseString ();
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
void SCR_UpdateScreen (void)
{
	vrect_t		vrect;

	if (block_drawing)
		return;

	vid.numpages = (int)(2 + gl_triplebuffer.value);

	scr_copytop = 0;
	scr_copyeverything = 0;

	if (scr_disabled_for_loading)
	{
		if (realtime - scr_disabled_time > 60)
		{
			scr_disabled_for_loading = false;
			Con_Printf ("load failed.\n");
		}
		else
			return;
	}

	if (!scr_initialized || !con_initialized)
		return;				// not initialized yet


	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	
	//
	// determine size of refresh window
	//
	if (oldfov != scr_fov.value)
	{
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}

	if (oldscreensize != viewsize.value)
	{
		oldscreensize = viewsize.value;
		vid.recalc_refdef = true;
	}
	
	if (oldsbaralpha != scr_sbaralpha.value)
	{
		oldsbaralpha = scr_sbaralpha.value;
		vid.recalc_refdef = true;
	}

	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

//
// do 3D refresh drawing, and then update the screen
//
	SCR_SetUpToDrawConsole ();

	V_RenderView ();

	GL_Set2D ();

	Draw_Crosshair ();

	//muff - to show FPS on screen
	SCR_DrawFPS ();
	SCR_CheckDrawCenterString ();
	SCR_CheckDrawUseString ();
	HUD_Draw ();
	SCR_DrawConsole ();
	Menu_Draw ();

	if(scr_loadscreen.value) {
		Menu_DrawLoadScreen();
	}

	Draw_LoadingFill();

	V_UpdatePalette ();

	GL_EndRendering ();
}