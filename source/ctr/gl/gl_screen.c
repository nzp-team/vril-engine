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

#include "../../quakedef.h"

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

float		oldscreensize, oldfov;

cvar_t		scr_viewsize = {"viewsize","100", true};
cvar_t		scr_fov = {"fov","90"};	// 10 - 170
cvar_t 		scr_fov_viewmodel = {"r_viewmodel_fov","70"};
cvar_t		scr_conspeed = {"scr_conspeed","300"};
cvar_t		scr_centertime = {"scr_centertime","2"};
cvar_t		scr_showram = {"showram","1"};
cvar_t		scr_showturtle = {"showturtle","0"};
cvar_t		scr_showpause = {"showpause","1"};
cvar_t		scr_printspeed = {"scr_printspeed","8"};
cvar_t 		scr_showfps = {"scr_showfps", "0"};
cvar_t		scr_loadscreen = {"scr_loadscreen","1"};
cvar_t		gl_triplebuffer = {"gl_triplebuffer", "1", true };
cvar_t 		cl_crosshair_debug = {"cl_crosshair_debug", "0", true};

extern	cvar_t	crosshair;

qboolean	scr_initialized;		// ready to draw

qpic_t      *hitmark;
qpic_t 		*lscreen;

int			scr_fullupdate;

int			loadingScreen;
int			ShowBlslogo;

qboolean 	loadscreeninit;

char* 		loadname2;
char* 		loadnamespec;

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
extern qpic_t 		*b_abutton;
extern qpic_t 		*b_bbutton;
extern qpic_t 		*b_ybutton;
extern qpic_t 		*b_xbutton;
extern qpic_t 		*b_left;
extern qpic_t 		*b_right;
extern qpic_t 		*b_up;
extern qpic_t 		*b_down;
extern qpic_t 		*b_lt;
extern qpic_t 		*b_rt;
extern qpic_t 		*b_start;
extern qpic_t 		*b_select;
extern qpic_t		*b_zlt;
extern qpic_t 		*b_zrt;

/*
==============
SCR_UsePrint

Similiar to above, but will also print the current button for the action.
==============
*/

qpic_t *GetButtonIcon (char *buttonname)
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
			// naievil -- need to fix these
			if (!strcmp(Key_KeynumToString(j), "PADUP"))
				return b_up;
			else if (!strcmp(Key_KeynumToString(j), "PADDOWN"))
				return b_down;
			else if (!strcmp(Key_KeynumToString(j), "PADLEFT"))
				return b_left;
			else if (!strcmp(Key_KeynumToString(j), "PADRIGHT"))
				return b_right;
			else if (!strcmp(Key_KeynumToString(j), "SELECT"))
				return b_select;
			else if (!strcmp(Key_KeynumToString(j), "ABUTTON"))
				return b_abutton;
			else if (!strcmp(Key_KeynumToString(j), "BBUTTON"))
				return b_bbutton;
			else if (!strcmp(Key_KeynumToString(j), "XBUTTON"))
				return b_xbutton;
			else if (!strcmp(Key_KeynumToString(j), "YBUTTON"))
				return b_ybutton;
			else if (!strcmp(Key_KeynumToString(j), "LTRIGGER"))
				return b_lt;
			else if (!strcmp(Key_KeynumToString(j), "RTRIGGER"))
				return b_rt;
			else if (!strcmp(Key_KeynumToString(j), "ZLTRIGGER"))
				return b_zlt;
			else if (!strcmp(Key_KeynumToString(j), "ZRTRIGGER"))
				return b_zrt;
		}
	}
	return b_abutton;
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
		Draw_Pic (x + button_pic_x, y - 4, GetButtonIcon("+use"));
}

void SCR_CheckDrawUseString (void)
{
	scr_copytop = 1;

	scr_usetime_off -= host_frametime;

	if (scr_usetime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;
    if (cl.stats[STAT_HEALTH] <= 0)
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

        x = width/tanf(fov_x/360*M_PI);

        a = atanf (height/x);

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
	int		h;
	qboolean		full = false;


	scr_fullupdate = 0;		// force a background redraw
	vid.recalc_refdef = 0;

// force the status bar to redraw
	Sbar_Changed ();

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

// intermission is always full screen	
	if (cl.intermission)
		size = 120;
	else
		size = scr_viewsize.value;

	if (size >= 120)
		sb_lines = 0;		// no status bar at all
	else if (size >= 110)
		sb_lines = 24;		// no inventory
	else
		sb_lines = 24+16+8;

	if (scr_viewsize.value >= 100.0) {
		full = true;
		size = 100.0;
	} else
		size = scr_viewsize.value;
	if (cl.intermission)
	{
		full = true;
		size = 100;
		sb_lines = 0;
	}
	size /= 100.0;

	h = vid.height - sb_lines;

	r_refdef.vrect.width = vid.width * size;
	if (r_refdef.vrect.width < 96)
	{
		size = 96.0 / r_refdef.vrect.width;
		r_refdef.vrect.width = 96;	// min for icons
	}

	r_refdef.vrect.height = vid.height * size;
	if (r_refdef.vrect.height > vid.height - sb_lines)
		r_refdef.vrect.height = vid.height - sb_lines;
	if (r_refdef.vrect.height > vid.height)
			r_refdef.vrect.height = vid.height;
	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width)/2;
	if (full)
		r_refdef.vrect.y = 0;
	else 
		r_refdef.vrect.y = (h - r_refdef.vrect.height)/2;

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
	Cvar_RegisterVariable (&scr_fov_viewmodel);
	Cvar_RegisterVariable (&scr_viewsize);
	Cvar_RegisterVariable (&scr_conspeed);
	Cvar_RegisterVariable (&scr_showram);
	Cvar_RegisterVariable (&scr_showturtle);
	Cvar_RegisterVariable (&scr_showpause);
	Cvar_RegisterVariable (&scr_centertime);
	Cvar_RegisterVariable (&scr_printspeed);
	Cvar_RegisterVariable (&scr_showfps);
	Cvar_RegisterVariable (&scr_loadscreen);
	Cvar_RegisterVariable (&cl_crosshair_debug);

	Cvar_RegisterVariable (&gl_triplebuffer);

//
// register our commands
//
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);

	hitmark = Draw_CachePic("gfx/hud/hit_marker");


	scr_initialized = true;
}

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
		Draw_String (300, 0, st);
	}
}


//=============================================================================

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	qpic_t	*pic;

	if (!scr_drawloading)
		return;

	pic = Draw_CachePic ("gfx/loading.lmp");
	Draw_Pic ( (vid.width - pic->width)/2,
		(vid.height - 48 - pic->height)/2, pic);
}

int Random_Int (int max_int)
{
	float	f;
	f = (rand ()&0x7fff) / ((float)0x7fff) * max_int;
	if (f > 0)
		return (int)(f + 0.5) + 1;
	else
		return (int)(f - 0.5) + 1;
}
/*
==============
SCR_DrawLoadScreen
==============
*/

/*
	Creds to the following people from the 2020
	Loading Screen Hint Submission/Contest:

	* BCDeshiG
	* Derped_Crusader
	* Aidan
	* yasen
	* greg
	* Asher
	* Bernerd
	* Omar Alejandro
	* TheSmashers
*/

// 47 character limit

double loadingtimechange;
int loadingdot;
char *lodinglinetext;
qpic_t *awoo;
char *ReturnLoadingtex (void)
{
    int StringNum = Random_Int(80);
    switch(StringNum)
    {
        case 1:
			return  "Released in 1996, Quake is over 25 years old!";
            break;
        case 2:
            return  "Use the Kar98k to be the hero we need!";
            break;
        case 3:
            return  "Lots of modern engines are based on Quake!";
            break;
        case 4:
            return  "NZ:P began development on September 27 2009!";
            break;
        case 5:
            return  "NZ:P was first released on December 25, 2010!";
            break;
        case 6:
            return  "NZ:P Beta 1.1 has over 300,000 downloads!";
            break;
        case 7:
            return  "NZ:P has been downloaded over 500,000 times!";
            break;
        case 8:
            return  "A lot of people have worked on NZ:P!";
            break;
        case 9:
            return  "Blubswillrule, or \"blubs\", is from the US.";
            break;
        case 10:
            return  "Jukki is from Finland.";
            break;
        case 11:
            return  "Ju[s]tice, or \"tom\" is from Lithuania.";
            break;
        case 12:
            return  "This game has given us bad sleeping habits!";
            break;
        case 13:
            return  "We had a lot of fun making this game!";
            break;
        case 14:
            return  "Pro Tip: you can make your own custom map!";
            break;
        case 15:
            return  "Try Retro Mode, it's in the Graphics Settings!";
            break;
        case 16:
			return  "Tired of our maps? Go make your own!";
            break;
        case 17:
            return  "Slay zombies & be grateful.";
            break;
        case 18:
            return  "Custom maps, CUSTOM MAPS!";
            break;
        case 19:
            return  "Go outside & build a snowman!";
            break;
        case 20:
            return  "Please surround yourself with zombies!";
            break;
        case 21:
            return  "Don't play for too long.. zombies may eat you.";
            break;
        case 22:
            return  "That was epic... EPIC FOR THE WIIIN!"; //why
            break;
        case 23:
            return  "Mikeage and Citra are awesome 3DS emulators!";
            break;
        case 24:
            return  "You dead yet?";
            break;
        case 25:
            return  "Now 21% cooler!";
            break;
        case 26:
            return  "your lg is nothink on the lan!"; //what
            break;
        case 27:
            return  "I'm not your chaotic on dm6!"; 
            break;
        case 28:
            return  "Shoot or knife zombies to kill them, up to you!";
            break;
        case 29:
            return 	"How many people forgot to Compile today?";
            break;
        case 30:
            return  "ggnore";
            break;
        case 31:
			return  "NZ:P is also on PC, Switch, Vita, and PSP!";
            break;
        case 32:
            return  "Submerge your device in water for godmode!";
            break;
        case 33:
            return  "10/10/10 was a good day.";
            break;
        case 34:
            return  "Also check out \"Halo Revamped\" for 3DS!";
            break;
        case 35:
            return 	"CypressImplex, or \"Ivy\", is from the USA.";
            break;
        case 36:
            return  "Zombies don't like bullets.";
            break;
        case 37:
            return  "Thanks for being an awesome fan!";
            break;
		case 38:
			return 	"Removed Herobrine";
			break;
		case 39:
			return 	"Pack-a-Punch the Kar98k to get to round 100000.";
			break;
		case 40:
			return 	"I feel like I'm being gaslit.";
			break;
		case 41:
			return 	"Heads up! You will die if you are killed!";
			break;
		case 42:
			return 	"Zombies legally can't kill you if you say no!";
			break;
		case 43:
			return 	"Please help me find the meaning of   . Thanks.";
			break;
		case 44:
			return  "Discord is ONLY for Thomas the Tank Engine RP!";
			break;
		case 45:
			return 	"\"Get rid of the 21% tip, it's an MLP reference.\"";
			break;
		case 46:
			return 	"You're playing on a 3DS!";
			break;
		case 47:
			return 	"Don't leak the beta!";
			break;
		case 48:
			return  "Jugger-Nog increases your health!";
			break;
		case 49:
			return  "greg was here";
			break;
		case 50:
			return  "Where the hell is the Mystery Box?!";
			break;
		case 51:
			return  "Zombies like getting shot.. I think.";
			break;
		case 52:
			return  "pro tip: aiming helps";
			break;
		case 53:
			return  "\"my mom gave me plunger money\"";
			break;
		case 54:
			return "dolphin dive on top of your friend for god mode";
			break;
		case 55:
			return "no free rides. ass, grass, or cash!";
			break;
		case 56:
			return "nzp-team.github.io/latest/game.html";
			break;
		case 57:
			return "im an mlg gamer girl so its pretty guaranteed";
			break;
		case 58:
			return "this is a w because you cant have enough fnaf";
			break;
		case 59:
			return "i hope santa drops bombs on the uk";
			break;
		case 60:
			return "Hoyl shit, bro! You fucking ported fortnite!";
			break;
		case 61:
			return "icarly feet futtishist.";
			break;
		case 62:
			return "Well, it's impossible to play, I'm disgusted.";
			break;
		case 63:
			return "I like my women to not be cartoons";
			break;
		case 64:
			return "Plot twist: NZP was always broken";
			break;
		case 65:
			return "testing some think.";
			break;
		case 66:
			return "fnaf is older than gay marriage in the us";
			break;
		case 67:
			return "i want that twink Obliterated";
			break;
		case 68:
			return "i think he started the femboy transition process";
			break;
		case 69:
			return "nice";
			break;
		case 70:
			return "He's FUCKING annoying";
			break;
		case 71:
			return "yeah pog female bikers";
			break;
		case 72:
			return "Its either a stroke of genius or just a stroke";
			break;
		case 73:
			return  "Play some Custom Maps!";
			break;
		case 74:
			return  "Real OGs play on \"Old\" 3DS models!";
			break;
		case 75:
			return  "Adding this tip improved framerate by 39%!";
			break;
		case 76:
			return  "The NZ in NZP stands for New Zealand!";
			break;
		case 77:
			return  "The P in NZP stands for Professional!";
			break;
		case 78:
			return  "Remember to stay hydrated!";
			break;
		case 79:
			return  "cofe";
			break;
    }
    return "wut wut";
}
qboolean load_screen_exists;
void SCR_DrawLoadScreen (void)
{

	if (developer.value) {
		return;
	}
	if (!con_forcedup) {
	    return;
	}

	if (loadingScreen) {
		Draw_FillByColor(0, 0, 400, 240, 0, 0, 0, 255);
		if (!loadscreeninit) {
			load_screen_exists = qfalse;

			char* lpath;
			lpath = (char*)Z_Malloc(sizeof(char)*32);
			strcpy(lpath, "gfx/lscreen/");
			strcat(lpath, loadname2);

			lscreen = Draw_CachePic(lpath);
			awoo = Draw_CachePic("gfx/menu/awoo");

			if (lscreen != NULL)
				load_screen_exists = qtrue;

			loadscreeninit = qtrue;
		}

		if (load_screen_exists == qtrue)
			Draw_StretchPic(scr_vrect.x, scr_vrect.y, lscreen, 400, 240);

		Draw_FillByColor(0, 0, 480, 24, 0, 0, 0, 175);
		Draw_FillByColor(0, 216, 480, 24, 0, 0, 0, 175);

		Draw_ColoredString(2, 4, loadnamespec, 255, 255, 0, 255, 2);
	}

	if (loadingtimechange < Sys_FloatTime ())
	{
        lodinglinetext = ReturnLoadingtex();
        loadingtimechange = Sys_FloatTime () + 5;
	}

	if (key_dest == key_game) {
		Draw_ColoredStringCentered(225, lodinglinetext, 255, 255, 255, 255, 1);

		if (strcmp(lodinglinetext, "Please help me find the meaning of   . Thanks.") == 0) {
			Draw_Pic(280, 225, awoo);
		}
	}
}


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
	if (!cl.worldmodel || cls.signon != SIGNONS)//blubs here, undid it actually
	{
		con_forcedup = qtrue;
	}
	else
	{
		con_forcedup = qfalse;
	}

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

	if (clearnotify++ < vid.numpages)
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
		Con_DrawConsole (scr_con_current, true);
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
	Sbar_Changed ();
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

int GetWeaponZoomAmmount (void)
{
    switch (cl.stats[STAT_ACTIVEWEAPON])
    {
        case W_COLT:
            return 10;
			break;
		case W_SPRING:
		case W_PULVERIZER:
		case W_KAR:
		case W_ARMAGEDDON:
			return 25;
			break;
		case W_KAR_SCOPE:
		case W_HEADCRACKER:
			return 47;
			break;
		case W_THOMPSON:
		case W_GIBS:
			return 10;
			break;
		case W_TRENCH:
		case W_GUT:
			return 10;
			break;
		case W_357:
		case W_KILLU:
			return 5;
			break;
		case W_MG:
		case W_BARRACUDA:
			return 15;
			break;
		case W_DB:
		case W_BORE:
		case W_SAWNOFF:
			return 10;
			break;
		case W_M1A1:
		case W_WIDDER:
			return 20;
			break;
		case W_BAR:
		case W_WIDOW:
			return 30;
			break;
		case W_FG:
		case W_IMPELLER:
			return 30;
			break;
		case W_GEWEHR:
		case W_COMPRESSOR:
			return 25;
			break;
		case W_PPSH:
		case W_REAPER:
			return 10;
			break;
		case W_MP40:
		case W_AFTERBURNER:
			return 10;
			break;
		case W_MP5:
		case W_KOLLIDER:
			return 10;
			break;
		case W_STG:
		case W_SPATZ:
			return 20;
			break;
		case W_M1:
		case W_M1000:
			return 25;
			break;
		case W_BROWNING:
		case W_ACCELERATOR:
			return 15;
			break;
		case W_PTRS:
		case W_PENETRATOR:
			return 50;
			break;
		case W_TYPE:
		case W_SAMURAI:
			return 10;
			break;
		case W_RAY:
		case W_PORTER:
			return 5;
			break;
        default:
            return 5;
            break;
    }
}

float zoomin_time;
int original_fov;
int original_view_fov;
void SCR_UpdateScreen (void)
{
	static float	oldscr_viewsize;
	vrect_t		vrect;

	if (block_drawing)
		return;

	vid.numpages = 2 + gl_triplebuffer.value;

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
	if (cl.stats[STAT_ZOOM] == 1)
	{
		if(!original_fov) {
			original_fov = scr_fov.value;
			original_view_fov = scr_fov_viewmodel.value;
		}
			
		if(scr_fov.value > (GetWeaponZoomAmmount() + 1))//+1 for accounting for floating point inaccurraces
		{
			scr_fov.value += ((original_fov - GetWeaponZoomAmmount()) - scr_fov.value) * 0.25;
			scr_fov_viewmodel.value += ((original_view_fov - GetWeaponZoomAmmount()) - scr_fov_viewmodel.value) * 0.25;
			Cvar_SetValue("fov",scr_fov.value);
			Cvar_SetValue("r_viewmodel_fov", scr_fov_viewmodel.value);
		}
	}
	else if (cl.stats[STAT_ZOOM] == 2)
	{
		Cvar_SetValue ("fov", 30);
		Cvar_SetValue ("r_viewmodel_fov", 30);
		zoomin_time = 0;
	}
	else if (cl.stats[STAT_ZOOM] == 0 && original_fov != 0)
	{
		if(scr_fov.value < (original_fov + 1))//+1 for accounting for floating point inaccuracies
		{
			scr_fov.value += (original_fov - scr_fov.value) * 0.25;
			scr_fov_viewmodel.value += (original_view_fov - scr_fov_viewmodel.value) * 0.25;
			Cvar_SetValue("fov",scr_fov.value);
			Cvar_SetValue("r_viewmodel_fov", scr_fov_viewmodel.value);
		}
		else
		{
			original_fov = 0;
			original_view_fov = 0;
		}
	}

	if (oldfov != scr_fov.value)
	{
		oldfov = scr_fov.value;
		vid.recalc_refdef = qtrue;
	}

	if (oldscreensize != scr_viewsize.value)
	{
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = qtrue;
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
	M_Draw ();

	if(scr_loadscreen.value) {
		SCR_DrawLoadScreen();
	}

	Draw_LoadingFill();

	V_UpdatePalette ();

	GL_EndRendering ();
}

