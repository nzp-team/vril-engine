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

#include "../../nzportable_def.h"
#include "r_local.h"

// only the refresh window will be updated unless these variables are flagged 
int			scr_copytop;
int			scr_copyeverything;

float		scr_con_current;
float		scr_conlines;		// lines of console to display

float		oldscreensize, oldfov;
cvar_t		scr_viewsize = {"viewsize","100", true};
cvar_t		scr_fov = {"fov","68"};	// 10 - 170
cvar_t		scr_conspeed = {"scr_conspeed","300"};
cvar_t		scr_centertime = {"scr_centertime","2"};
cvar_t		scr_showpause = {"showpause","1"};
cvar_t		scr_printspeed = {"scr_printspeed","8"};
cvar_t 		scr_showfps = {"scr_showfps", "1"};
cvar_t		scr_loadscreen = {"scr_loadscreen","1"};
cvar_t 		cl_crosshair_debug = {"cl_crosshair_debug", "0", true};

int      	hitmark;
int 		lscreen;

int			scr_fullupdate;

int			ShowBlslogo;

qboolean	scr_initialized;		// ready to draw

int			clearconsole;
int			clearnotify;

extern viddef_t	vid;				// global video state

vrect_t		*pconupdate;
vrect_t		scr_vrect;

qboolean	scr_disabled_for_loading;
qboolean	scr_drawloading;
float		scr_disabled_time;
qboolean	scr_skipupdate;

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

void SCR_EraseCenterString (void)
{
	int		y;

	if (scr_erase_center++ > vid.numpages)
	{
		scr_erase_lines = 0;
		return;
	}

	if (scr_center_lines <= 4)
		y = vid.height*0.35;
	else
		y = 48;

	scr_copytop = 1;
	Draw_TileClear (0, y,vid.width, 8*scr_erase_lines);
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
		y = vid.height*0.35;
	else
		y = 48;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - getTextWidth(start, 1))/2;
		for (j=0 ; j<l ; j++)
		{
			Draw_Character (x, y, start[j]);	

			// Hooray for variable-spacing!
			if (start[j] == ' ')
				x += 4;
			else if ((int)start[j] < 33 || (int)start[j] > 126)
				x += 8;
			else
				x += (font_kerningamount[(int)(start[j] - 33)] + 1);

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

/*
===============================================================================

Press somthing printing

===============================================================================
*/

char		scr_usestring[64];
char 		scr_usestring2[64];
float		scr_usetime_off = 0.0f;
int			button_pic_x;

/*
==============
SCR_UsePrint

Similiar to above, but will also print the current button for the action.
==============
*/

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
			strcpy(s, va("Hold     to open Door\n"));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 2://debris
			strcpy(s, va("Hold     to remove Debris\n"));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 3://ammo
			strcpy(s, va("Hold     to buy Ammo for %s\n", pr_strings+sv_player->v.Weapon_Name_Touch));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 4://weapon
			strcpy(s, va("Hold     to buy %s\n", pr_strings+sv_player->v.Weapon_Name_Touch));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 5://window
			strcpy(s, va("Hold     to Rebuild Barrier\n"));
			strcpy(c, "");
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 6://box
			strcpy(s, va("Hold     to for Mystery Box\n"));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 7://box take
			strcpy(s, va("Hold     for %s\n", pr_strings+sv_player->v.Weapon_Name_Touch));
			strcpy(c, "");
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 8://power
			strcpy(s, "The Power must be Activated first\n");
			strcpy(c, "");
			button_pic_x = 100;
			break;
		case 9://perk
			strcpy(s, va("Hold     to buy %s\n", GetPerkName(weapon)));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 10://turn on power
			strcpy(s, va("Hold     to Turn On the Power\n"));
			strcpy(c, "");
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 11://turn on trap
			strcpy(s, va("Hold     to Activate the Trap\n"));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 12://PAP
			strcpy(s, va("Hold     to Pack-a-Punch\n"));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 13://revive
			strcpy(s, va("Hold     to Fix your Code.. :)\n"));
			strcpy(c, "");
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 14://use teleporter (free)
			strcpy(s, va("Hold     to use Teleporter\n"));
			strcpy(c, "");
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 15://use teleporter (cost)
			strcpy(s, va("Hold     to use Teleporter\n"));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 16://tp cooldown
			strcpy(s, "Teleporter is cooling down\n");
			strcpy(c, "");
			button_pic_x = 100;
			break;
		case 17://link
			strcpy(s, va("Hold     to initiate link to pad\n"));
			strcpy(c, "");
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 18://no link
			strcpy(s, "Link not active\n");
			strcpy(c, "");
			button_pic_x = 100;
			break;
		case 19://finish link
			strcpy(s, va("Hold     to link pad with core\n"));
			strcpy(c, "");
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 20://buyable ending
			strcpy(s, va("Hold     to End the Game\n"));
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
	int	y;
	int x;

	if (cl.stats[STAT_HEALTH] < 0)
		return;
// the finale prints the characters one at a time

	y = 160;
    x = (vid.width - getTextWidth(scr_usestring, 1))/2;

	Draw_ColoredStringCentered(y, scr_usestring, 255, 255, 255, 255, 1);
	Draw_ColoredStringCentered(y + 10, scr_usestring2, 255, 255, 255, 255, 1);

	if (button_pic_x != 100)
	{
		int keys[2];
		// TODO MENU
		//M_FindKeysForCommand ("+use", keys);
		if (keys[0] != -1)
		{
			Draw_CharacterRGBA (x + button_pic_x + 4, y, *Key_KeynumToString(keys[0]), 255, 255, 0, 255, 1);
		}	
	}		
}

void SCR_CheckDrawUseString (void)
{
	scr_copytop = 1;

	scr_usetime_off -= host_frametime;

	if ((scr_usetime_off <= 0 && !cl.intermission) || key_dest != key_game || cl.stats[STAT_HEALTH] <= 0)
		return;

	SCR_DrawUseString ();
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
	float		size;

	scr_fullupdate = 0;		// force a background redraw
	vid.recalc_refdef = 0;

//========================================
	
// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_Set ("viewsize","30");
	if (scr_viewsize.value > 120)
		Cvar_Set ("viewsize","120");

// bound field of view
	if (scr_fov.value < 10)
		Cvar_Set ("fov","10");
	if (scr_fov.value > 170)
		Cvar_Set ("fov","170");

	r_refdef.fov_x = scr_fov.value;
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

// intermission is always full screen	
	if (cl.intermission)
		size = 120;
	else
		size = scr_viewsize.value;

// these calculations mirror those in R_Init() for r_refdef, but take no
// account of water warping
	vrect.x = 0;
	vrect.y = 0;
	vrect.width = vid.width;
	vrect.height = vid.height;

	R_SetVrect (&vrect, &scr_vrect, 0);

// guard against going from one mode to another that's less than half the
// vertical resolution
	if (scr_con_current > vid.height)
		scr_con_current = vid.height;

// notify the refresh of the change
	R_ViewChanged (&vrect, 0, vid.aspect);
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	Cvar_SetValue ("viewsize",scr_viewsize.value+10);
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
	Cvar_SetValue ("viewsize",scr_viewsize.value-10);
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
	Cvar_RegisterVariable (&scr_fov);
	Cvar_RegisterVariable (&scr_viewsize);
	Cvar_RegisterVariable (&scr_conspeed);
	Cvar_RegisterVariable (&scr_showpause);
	Cvar_RegisterVariable (&scr_centertime);
	Cvar_RegisterVariable (&scr_printspeed);
	Cvar_RegisterVariable (&scr_showfps);
	Cvar_RegisterVariable (&scr_loadscreen);
	Cvar_RegisterVariable (&cl_crosshair_debug);

//
// register our commands
//
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);

	hitmark = Image_LoadImage("gfx/hud/hit_marker", IMAGE_TGA, 0, qfalse, qfalse);

	scr_initialized = true;
}

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
		int	x, y;
		sprintf (st, "%4.0f fps", lastfps);
		x = 320 - (strlen(st)<<3);
		y = 0;

		Draw_String (x, y, st);
	}
}

//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
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
	
	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed.value*host_frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed.value*host_frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (clearconsole++ < vid.numpages)
	{
		scr_copytop = 1;
		Draw_TileClear (0,(int)scr_con_current,vid.width, vid.height - (int)scr_con_current);
	}
	else if (clearnotify++ < vid.numpages)
	{
		scr_copytop = 1;
		Draw_TileClear (0,0,vid.width, con_notifylines);
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
		Con_DrawConsole (scr_con_current, true, 1.0);
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
 

typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned char	data;			// unbounded
} pcx_t;

/* 
============== 
WritePCXfile 
============== 
*/ 
void WritePCXfile (char *filename, byte *data, int width, int height,
	int rowbytes, byte *palette) 
{
	int		i, j, length;
	pcx_t	*pcx;
	byte		*pack;
	  
	pcx = Hunk_TempAlloc (width*height*2+1000);
	if (pcx == NULL)
	{
		Con_Printf("SCR_ScreenShot_f: not enough memory\n");
		return;
	} 
 
	pcx->manufacturer = 0x0a;	// PCX id
	pcx->version = 5;			// 256 color
 	pcx->encoding = 1;		// uncompressed
	pcx->bits_per_pixel = 8;		// 256 color
	pcx->xmin = 0;
	pcx->ymin = 0;
	pcx->xmax = LittleShort((short)(width-1));
	pcx->ymax = LittleShort((short)(height-1));
	pcx->hres = LittleShort((short)width);
	pcx->vres = LittleShort((short)height);
	Q_memset (pcx->palette,0,sizeof(pcx->palette));
	pcx->color_planes = 1;		// chunky image
	pcx->bytes_per_line = LittleShort((short)width);
	pcx->palette_type = LittleShort(2);		// not a grey scale
	Q_memset (pcx->filler,0,sizeof(pcx->filler));

// pack the image
	pack = &pcx->data;
	
	for (i=0 ; i<height ; i++)
	{
		for (j=0 ; j<width ; j++)
		{
			if ( (*data & 0xc0) != 0xc0)
				*pack++ = *data++;
			else
			{
				*pack++ = 0xc1;
				*pack++ = *data++;
			}
		}

		data += rowbytes - width;
	}
			
// write the palette
	*pack++ = 0x0c;	// palette ID byte
	for (i=0 ; i<768 ; i++)
		*pack++ = *palette++;
		
// write output file 
	length = pack - (byte *)pcx;
	COM_WriteFile (filename, pcx, length);
} 
 


/* 
================== 
SCR_ScreenShot_f
================== 
*/  
void SCR_ScreenShot_f (void) 
{ 
	int     i; 
	char		pcxname[80]; 
	char		checkname[MAX_OSPATH];

// 
// find a file name to save it to 
// 
	strcpy(pcxname,"quake00.pcx");
		
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

// 
// save the pcx file 
// 
	D_EnableBackBufferAccess ();	// enable direct drawing of console to back
									//  buffer

	WritePCXfile (pcxname, vid.buffer, vid.width, vid.height, vid.rowbytes,
				  host_basepal);

	D_DisableBackBufferAccess ();	// for adapters that can't stay mapped in
									//  for linear writes all the time

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
qboolean	scr_drawdialog;

void SCR_DrawNotifyString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;

	start = scr_notifystring;

	y = vid.height*0.35;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
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


/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
void Draw_Crosshair (void);
void SCR_UpdateScreen (void)
{
	static float	oldscr_viewsize;
	static float	oldlcd_x;
	vrect_t		vrect;
	
	if (scr_skipupdate || block_drawing)
		return;

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

	if (cls.state == ca_dedicated)
		return;				// stdout only

	if (!scr_initialized || !con_initialized)
		return;				// not initialized yet

	Draw_BlackBackground();

	if (scr_viewsize.value != oldscr_viewsize)
	{
		oldscr_viewsize = scr_viewsize.value;
		vid.recalc_refdef = 1;
	}
	
//
// check for vid changes
//
	if (oldfov != scr_fov.value)
	{
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}
	
	if (oldlcd_x != lcd_x.value)
	{
		oldlcd_x = lcd_x.value;
		vid.recalc_refdef = true;
	}
	
	if (oldscreensize != scr_viewsize.value)
	{
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = true;
	}
	
	if (vid.recalc_refdef)
	{
	// something changed, so reorder the screen
		SCR_CalcRefdef ();
	}

//
// do 3D refresh drawing, and then update the screen
//
	/*printf("in SCR_UpdateScreen %s:%d\n", __FILE__, __LINE__ );*/
	D_EnableBackBufferAccess ();	// of all overlay stuff if drawing directly

	if (scr_fullupdate++ < vid.numpages)
	{	// clear the entire screen
		scr_copyeverything = 1;
		Draw_TileClear (0,0,vid.width,vid.height);
	}

	pconupdate = NULL;

	/*printf("in SCR_UpdateScreen %s:%d\n", __FILE__, __LINE__ );*/

	SCR_SetUpToDrawConsole ();
	SCR_EraseCenterString ();

	D_DisableBackBufferAccess ();	// for adapters that can't stay mapped in
									//  for linear writes all the time

	/*printf("in SCR_UpdateScreen %s:%d\n", __FILE__, __LINE__ );*/

	VID_LockBuffer ();

	/*printf("in SCR_UpdateScreen %s:%d\n", __FILE__, __LINE__ );*/
	V_RenderView ();
	/*printf("in SCR_UpdateScreen %s:%d\n", __FILE__, __LINE__ );*/

	VID_UnlockBuffer ();

	D_EnableBackBufferAccess ();	// of all overlay stuff if drawing directly

	Draw_Crosshair ();

	//muff - to show FPS on screen
	SCR_DrawFPS ();
	SCR_CheckDrawCenterString ();
	SCR_CheckDrawUseString ();
	HUD_Draw ();
	SCR_DrawConsole ();
	Menu_Draw ();

	if(scr_loadscreen.value) {
		SCR_DrawLoadScreen();
	}

	//Draw_LoadingFill();
	D_DisableBackBufferAccess ();	// for adapters that can't stay mapped in
									//  for linear writes all the time
	if (pconupdate)
	{
		D_UpdateRects (pconupdate);
	}

	/*printf("in SCR_UpdateScreen %s:%d\n", __FILE__, __LINE__ );*/

	V_UpdatePalette ();

//
// update one of three areas
//

	if (scr_copyeverything)
	{
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height;
		vrect.pnext = 0;
	
		VID_Update (&vrect);
	}
	else if (scr_copytop)
	{
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height;
		vrect.pnext = 0;
	
		VID_Update (&vrect);
	}	
	else
	{
		vrect.x = scr_vrect.x;
		vrect.y = scr_vrect.y;
		vrect.width = scr_vrect.width;
		vrect.height = scr_vrect.height;
		vrect.pnext = 0;
	
		VID_Update (&vrect);
	}
}


/*
==================
SCR_UpdateWholeScreen
==================
*/
void SCR_UpdateWholeScreen (void)
{
	scr_fullupdate = 0;
	SCR_UpdateScreen ();
}
