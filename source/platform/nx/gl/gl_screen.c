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

#include <string.h>

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

CenterPrint();
SlowPrint();
Screen_Update();
Con_Printf();

net
turn off messages option

the refresh is always rendered, unless the console is full screen

console is:
        notify lines
        half
        full
*/

// only the refresh window will be updated unless these variables are flagged
int scr_copytop;
int scr_copyeverything;

float scr_con_current;
static float scr_conlines; /* lines of console to display */

int scr_fullupdate;
static int clearconsole;
int clearnotify;

vrect_t scr_vrect;

qboolean scr_disabled_for_loading;
qboolean scr_block_drawing;
qboolean scr_skipupdate;

static cvar_t scr_centertime = {"scr_centertime", "2"};
static cvar_t scr_printspeed = {"scr_printspeed", "8"};

float oldscreensize, oldfov;

cvar_t scr_viewsize = {"viewsize", "100", true};
cvar_t	scr_fov = {"fov","90"};	// 10 - 170
cvar_t 	scr_fov_viewmodel = {"r_viewmodel_fov","70"};
cvar_t	scr_loadscreen = {"scr_loadscreen","1"};
static cvar_t scr_conspeed = {"scr_conspeed", "300"};
#ifdef GLQUAKE
static cvar_t gl_triplebuffer = {"gl_triplebuffer", "0", true};
#else
static vrect_t *pconupdate;
#endif

cvar_t 	cl_crosshair_debug = {"cl_crosshair_debug", "0", true};

extern cvar_t crosshair;
extern cvar_t show_fps;

qboolean	scr_initialized;		// ready to draw

int      	hitmark;
int 		lscreen;
int			loadingScreen;
qboolean 	loadscreeninit;
char* 		loadname2;
char* 		loadnamespec;

static char scr_centerstring[1024];
static float scr_centertime_start;  // for slow victory printing
float scr_centertime_off;
static int scr_center_lines;
static int scr_erase_lines;
static int scr_erase_center;

#ifdef NQ_HACK
static qboolean scr_drawloading;
static float scr_disabled_time;
#endif
#ifdef QW_HACK
static float oldsbar;
static cvar_t scr_allowsnap = {"scr_allowsnap", "1"};
#endif

int			sb_lines;

//=============================================================================



static void SCR_DrawFPS(void) {
    static double lastframetime;
    static int lastfps;
    double t;
    int x, y;
    char st[80];

    if (!show_fps.value) return;

    t = Sys_FloatTime();
    if ((t - lastframetime) >= 1.0) {
        lastfps = fps_count;
        fps_count = 0;
        lastframetime = t;
    }

    sprintf(st, "%3d FPS", lastfps);
    x = vid.width - strlen(st) * 8 - 8;
    y = vid.height - sb_lines - 8;
    Draw_String(x, y, st);
}



//=============================================================================

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
		remaining = scr_printspeed.value * ((float)cl.time - scr_centertime_start);
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

	scr_centertime_off -= (float)host_frametime;
	
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
extern int 		b_abutton;
extern int 		b_bbutton;
extern int 		b_ybutton;
extern int 		b_xbutton;
extern int 		b_left;
extern int 		b_right;
extern int 		b_up;
extern int 		b_down;
extern int 		b_lt;
extern int 		b_rt;
extern int 		b_start;
extern int 		b_select;
extern int		b_zlt;
extern int 		b_zrt;

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
			strcpy(s, va("Hold  %s  to buy Ammo for %s\n", GetUseButtonL(), PR_GetString(sv_player->v.Weapon_Name_Touch)));
			strcpy(c, va("[Cost: %i]\n", cost));
			button_pic_x = getTextWidth("Hold ", 1);
			break;
		case 4://weapon
			strcpy(s, va("Hold  %s  to buy %s\n", GetUseButtonL(), PR_GetString(sv_player->v.Weapon_Name_Touch)));
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
			strcpy(s, va("Hold  %s  for %s\n", GetUseButtonL(), PR_GetString(sv_player->v.Weapon_Name_Touch)));
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

	scr_usetime_off -= (float)host_frametime;

	if ((scr_usetime_off <= 0 && !cl.intermission) || key_dest != key_game || cl.stats[STAT_HEALTH] <= 0)
		return;

	SCR_DrawUseString ();
}

//=============================================================================

//=============================================================================

static const char *scr_notifystring;
static qboolean scr_drawdialog;

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

/*
==================
SCR_ModalMessage

Displays a text string in the center of the screen and waits for a Y or N
keypress.
==================
*/
int SCR_ModalMessage(const char *text) {
#ifdef NQ_HACK
    if (cls.state == ca_dedicated) return true;
#endif

    scr_notifystring = text;

    // draw a fresh screen
    scr_fullupdate = 0;
    scr_drawdialog = true;
    SCR_UpdateScreen();
    scr_drawdialog = false;

    S_ClearBuffer();  // so dma doesn't loop current sound

    do {
        key_count = -1;  // wait for a key down and up
        Sys_SendKeyEvents();
        Sys_Sleep();
    } while (key_lastpress != 'y' && key_lastpress != 'n' && key_lastpress != K_ESCAPE);

    scr_fullupdate = 0;
    SCR_UpdateScreen();

    return key_lastpress == 'y';
}

//============================================================================

/*
====================
CalcFov
====================
*/
static float CalcFov(float fov_x, float width, float height) {
    float a;
    float x;

    if (fov_x < 1 || fov_x > 179) Sys_Error("Bad fov: %f", fov_x);

    x = width / tan(fov_x / 360 * M_PI);
    a = atan(height / x);
    a = a * 360 / M_PI;

    return a;
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
static void SCR_CalcRefdef(void) {
    vrect_t vrect;
    float size;

    scr_fullupdate = 0;  // force a background redraw
    vid.recalc_refdef = 0;

    // force the status bar to redraw

    //========================================

    // bound viewsize
    if (scr_viewsize.value < 30) Cvar_Set("viewsize", "30");
    if (scr_viewsize.value > 120) Cvar_Set("viewsize", "120");

    // bound field of view
    if (scr_fov.value < 10) Cvar_Set("fov", "10");
    if (scr_fov.value > 170) Cvar_Set("fov", "170");

    // intermission is always full screen
    if (cl.intermission)
        size = 120;
    else
        size = scr_viewsize.value;

    if (size >= 120)
        sb_lines = 0;  // no status bar at all
    else if (size >= 110)
        sb_lines = 24;  // no inventory
    else
        sb_lines = 24 + 16 + 8;

    // these calculations mirror those in R_Init() for r_refdef, but take no
    // account of water warping
    vrect.x = 0;
    vrect.y = 0;
    vrect.width = vid.width;
    vrect.height = vid.height;

#ifdef GLQUAKE
    R_SetVrect(&vrect, &r_refdef.vrect, sb_lines);
#else
    R_SetVrect(&vrect, &scr_vrect, sb_lines);
#endif

    r_refdef.fov_x = scr_fov.value;
    r_refdef.fov_y = CalcFov(r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

#ifdef GLQUAKE
    scr_vrect = r_refdef.vrect;
#else
    // guard against going from one mode to another that's less than half the
    // vertical resolution
    if (scr_con_current > vid.height) scr_con_current = vid.height;

    // notify the refresh of the change
    R_ViewChanged(&vrect, sb_lines, vid.aspect);
#endif
}

/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
static void SCR_SizeUp_f(void) {
    Cvar_SetValue("viewsize", scr_viewsize.value + 10);
    vid.recalc_refdef = 1;
}

/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
static void SCR_SizeDown_f(void) {
    Cvar_SetValue("viewsize", scr_viewsize.value - 10);
    vid.recalc_refdef = 1;
}

/*
==============================================================================

                                SCREEN SHOTS

==============================================================================
*/

#if !defined(NQ_HACK) || !defined(GLQUAKE)
/*
==============
WritePCXfile
==============
*/
static void WritePCXfile(const char *filename, const byte *data, int width, int height,
                         int rowbytes, const byte *palette, qboolean upload) {
    int i, j, length;
    pcx_t *pcx;
    byte *pack;

    pcx = Hunk_TempAlloc(width * height * 2 + 1000);
    if (pcx == NULL) {
        Con_Printf("SCR_ScreenShot_f: not enough memory\n");
        return;
    }

    pcx->manufacturer = 0x0a;  // PCX id
    pcx->version = 5;          // 256 color
    pcx->encoding = 1;         // uncompressed
    pcx->bits_per_pixel = 8;   // 256 color
    pcx->xmin = 0;
    pcx->ymin = 0;
    pcx->xmax = LittleShort((short)(width - 1));
    pcx->ymax = LittleShort((short)(height - 1));
    pcx->hres = LittleShort((short)width);
    pcx->vres = LittleShort((short)height);
    memset(pcx->palette, 0, sizeof(pcx->palette));
    pcx->color_planes = 1;  // chunky image
    pcx->bytes_per_line = LittleShort((short)width);
    pcx->palette_type = LittleShort(1);  // not a grey scale
    memset(pcx->filler, 0, sizeof(pcx->filler));

    // pack the image
    pack = &pcx->data;

#ifdef GLQUAKE
    // The GL buffer addressing is bottom to top?
    data += rowbytes * (height - 1);
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            if ((*data & 0xc0) != 0xc0) {
                *pack++ = *data++;
            } else {
                *pack++ = 0xc1;
                *pack++ = *data++;
            }
        }
        data += rowbytes - width;
        data -= rowbytes * 2;
    }
#else
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            if ((*data & 0xc0) != 0xc0) {
                *pack++ = *data++;
            } else {
                *pack++ = 0xc1;
                *pack++ = *data++;
            }
        }
        data += rowbytes - width;
    }
#endif

    // write the palette
    *pack++ = 0x0c;  // palette ID byte
    for (i = 0; i < 768; i++) *pack++ = *palette++;

    // write output file
    length = pack - (byte *)pcx;

#ifdef QW_HACK
    if (upload) {
        CL_StartUpload((byte *)pcx, length);
        return;
    }
#endif

    COM_WriteFile(filename, pcx, length);
}
#endif /* !defined(NQ_HACK) && !defined(GLQUAKE) */

#ifdef QW_HACK
/*
Find closest color in the palette for named color
*/
static int MipColor(int r, int g, int b) {
    int i;
    float dist;
    int best;
    float bestdist;
    int r1, g1, b1;
    static int lr = -1, lg = -1, lb = -1;
    static int lastbest;

    if (r == lr && g == lg && b == lb) return lastbest;

    bestdist = 256 * 256 * 3;

    best = 0;  // FIXME - Uninitialised? Zero ok?
    for (i = 0; i < 256; i++) {
        r1 = host_basepal[i * 3] - r;
        g1 = host_basepal[i * 3 + 1] - g;
        b1 = host_basepal[i * 3 + 2] - b;
        dist = r1 * r1 + g1 * g1 + b1 * b1;
        if (dist < bestdist) {
            bestdist = dist;
            best = i;
        }
    }
    lr = r;
    lg = g;
    lb = b;
    lastbest = best;
    return best;
}

static void SCR_DrawCharToSnap(int num, byte *dest, int width) {
    int row, col;
    const byte *source;
    int drawline;
    int x, stride;

    row = num >> 4;
    col = num & 15;
    source = draw_chars + (row << 10) + (col << 3);

#ifdef GLQUAKE
    stride = -128;
#else
    stride = 128;
#endif

    if (stride < 0) source -= 7 * stride;

    drawline = 8;
    while (drawline--) {
        for (x = 0; x < 8; x++)
            if (source[x])
                dest[x] = source[x];
            else
                dest[x] = 98;
        source += stride;
        dest += width;
    }
}

static void SCR_DrawStringToSnap(const char *s, byte *buf, int x, int y, int width, int height) {
    byte *dest;
    const unsigned char *p;

#ifdef GLQUAKE
    dest = buf + (height - y - 8) * width + x;
#else
    dest = buf + y * width + x;
#endif

    p = (const unsigned char *)s;
    while (*p) {
        SCR_DrawCharToSnap(*p++, dest, width);
        dest += 8;
    }
}

/*
==================
SCR_RSShot_f
==================
*/
static void SCR_RSShot_f(void) {
    int i;
    int x, y;
    unsigned char *src, *dest;
    char pcxname[80];
    unsigned char *newbuf;
    int w, h;
    int dx, dy, dex, dey, nx;
    int r, b, g;
    int count;
    float fracw, frach;
    char st[80];
    time_t now;

    if (CL_IsUploading()) return;  // already one pending

    if (cls.state < ca_onserver) return;  // gotta be connected

#ifndef GLQUAKE /* <- probably a bug, should check always? */
    if (!scr_allowsnap.value) {
        MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
        MSG_WriteString(&cls.netchan.message, "snap\n");
        Con_Printf("Refusing remote screen shot request.\n");
        return;
    }
#endif

    Con_Printf("Remote screen shot requested.\n");

    //
    // find a file name to save it to
    //
    strcpy(pcxname, "mquake00.pcx");

    for (i = 0; i <= 99; i++) {
        pcxname[6] = i / 10 + '0';
        pcxname[7] = i % 10 + '0';
        if (Sys_FileTime(va("%s/%s", com_gamedir, pcxname)) == -1) break;  // file doesn't exist
    }
    if (i == 100) {
        Con_Printf("SCR_ScreenShot_f: Couldn't create a PCX\n");
        return;
    }

//
// save the pcx file
//
#ifdef GLQUAKE /* FIXME - consolidate common bits */
    newbuf = malloc(glheight * glwidth * 4);

    glReadPixels(glx, gly, glwidth, glheight, GL_RGBA, GL_UNSIGNED_BYTE, newbuf);

    w = (vid.width < RSSHOT_WIDTH) ? glwidth : RSSHOT_WIDTH;
    h = (vid.height < RSSHOT_HEIGHT) ? glheight : RSSHOT_HEIGHT;

    fracw = (float)glwidth / (float)w;
    frach = (float)glheight / (float)h;

    for (y = 0; y < h; y++) {
        dest = newbuf + (w * 4 * y);

        for (x = 0; x < w; x++) {
            r = g = b = 0;

            dx = x * fracw;
            dex = (x + 1) * fracw;
            if (dex == dx) dex++;  // at least one
            dy = y * frach;
            dey = (y + 1) * frach;
            if (dey == dy) dey++;  // at least one

            count = 0;
            for (/* */; dy < dey; dy++) {
                src = newbuf + (glwidth * 4 * dy) + dx * 4;
                for (nx = dx; nx < dex; nx++) {
                    r += *src++;
                    g += *src++;
                    b += *src++;
                    src++;
                    count++;
                }
            }
            r /= count;
            g /= count;
            b /= count;
            *dest++ = r;
            *dest++ = g;
            *dest++ = b;
            dest++;
        }
    }

    // convert to eight bit
    for (y = 0; y < h; y++) {
        src = newbuf + (w * 4 * y);
        dest = newbuf + (w * y);

        for (x = 0; x < w; x++) {
            *dest++ = MipColor(src[0], src[1], src[2]);
            src += 4;
        }
    }
#else
    D_EnableBackBufferAccess();  // enable direct drawing of console to back
    //  buffer

    w = (vid.width < RSSHOT_WIDTH) ? vid.width : RSSHOT_WIDTH;
    h = (vid.height < RSSHOT_HEIGHT) ? vid.height : RSSHOT_HEIGHT;

    fracw = (float)vid.width / (float)w;
    frach = (float)vid.height / (float)h;

    newbuf = malloc(w * h);

    for (y = 0; y < h; y++) {
        dest = newbuf + (w * y);

        for (x = 0; x < w; x++) {
            r = g = b = 0;

            dx = x * fracw;
            dex = (x + 1) * fracw;
            if (dex == dx) dex++;  // at least one
            dy = y * frach;
            dey = (y + 1) * frach;
            if (dey == dy) dey++;  // at least one

            count = 0;
            for (/* */; dy < dey; dy++) {
                src = vid.buffer + (vid.rowbytes * dy) + dx;
                for (nx = dx; nx < dex; nx++) {
                    r += host_basepal[*src * 3];
                    g += host_basepal[*src * 3 + 1];
                    b += host_basepal[*src * 3 + 2];
                    src++;
                    count++;
                }
            }
            r /= count;
            g /= count;
            b /= count;
            *dest++ = MipColor(r, g, b);
        }
    }
#endif

    time(&now);
    strcpy(st, ctime(&now));
    st[strlen(st) - 1] = 0;
    SCR_DrawStringToSnap(st, newbuf, w - strlen(st) * 8, 0, w, h);

    strncpy(st, cls.servername, sizeof(st));
    st[sizeof(st) - 1] = 0;
    SCR_DrawStringToSnap(st, newbuf, w - strlen(st) * 8, 10, w, h);

    strncpy(st, name.string, sizeof(st));
    st[sizeof(st) - 1] = 0;
    SCR_DrawStringToSnap(st, newbuf, w - strlen(st) * 8, 20, w, h);

    WritePCXfile(pcxname, newbuf, w, h, w, host_basepal, true);

    free(newbuf);

#ifndef GLQUAKE
    /* for adapters that can't stay mapped in for linear writes all the time */
    D_DisableBackBufferAccess();
#endif

    Con_Printf("Wrote %s\n", pcxname);
    Con_Printf("Sending shot to server...\n");
}

#endif /* QW_HACK */

#ifdef GLQUAKE
typedef struct _TargaHeader {
    unsigned char id_length, colormap_type, image_type;
    unsigned short colormap_index, colormap_length;
    unsigned char colormap_size;
    unsigned short x_origin, y_origin, width, height;
    unsigned char pixel_size, attributes;
} TargaHeader;

/* FIXME - poorly chosen globals? need to be global? */
int glx, gly, glwidth, glheight;
#endif

/*
==================
SCR_ScreenShot_f
==================
*/
static void SCR_ScreenShot_f(void) {
#ifdef GLQUAKE
    byte *buffer;
    char tganame[80];
    char checkname[MAX_OSPATH*2];
    int i, c, temp;

    //
    // find a file name to save it to
    //
    strcpy(tganame, "quake00.tga");

    for (i = 0; i <= 99; i++) {
        tganame[5] = i / 10 + '0';
        tganame[6] = i % 10 + '0';
        sprintf(checkname, "%s/%s", com_gamedir, tganame);
        if (Sys_FileTime(checkname) == -1) break;  // file doesn't exist
    }
    if (i == 100) {
        Con_Printf("%s: Couldn't create a TGA file\n", __func__);
        return;
    }

    /* Construct the TGA header */
    buffer = malloc(glwidth * glheight * 3 + 18);
    memset(buffer, 0, 18);
    buffer[2] = 2;  // uncompressed type
    buffer[12] = glwidth & 255;
    buffer[13] = glwidth >> 8;
    buffer[14] = glheight & 255;
    buffer[15] = glheight >> 8;
    buffer[16] = 24;  // pixel size

    glReadPixels(glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer + 18);

    // swap rgb to bgr
    c = 18 + glwidth * glheight * 3;
    for (i = 18; i < c; i += 3) {
        temp = buffer[i];
        buffer[i] = buffer[i + 2];
        buffer[i + 2] = temp;
    }
    COM_WriteFile(tganame, buffer, glwidth * glheight * 3 + 18);

    free(buffer);
    Con_Printf("Wrote %s\n", tganame);
#else
    int i;
    char pcxname[80];
    char checkname[MAX_OSPATH];

    //
    // find a file name to save it to
    //
    strcpy(pcxname, "quake00.pcx");

    for (i = 0; i <= 99; i++) {
        pcxname[5] = i / 10 + '0';
        pcxname[6] = i % 10 + '0';
        sprintf(checkname, "%s/%s", com_gamedir, pcxname);
        if (Sys_FileTime(checkname) == -1) break;  // file doesn't exist
    }
    if (i == 100) {
        Con_Printf("%s: Couldn't create a PCX file\n", __func__);
        return;
    }
    //
    // save the pcx file
    //
    D_EnableBackBufferAccess();  // enable direct drawing of console to back
    //  buffer

    WritePCXfile(pcxname, vid.buffer, vid.width, vid.height, vid.rowbytes, host_basepal, false);

    D_DisableBackBufferAccess();  // for adapters that can't stay mapped in
    //  for linear writes all the time

    Con_Printf("Wrote %s\n", pcxname);
#endif
}

//=============================================================================

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

void SCR_DrawLoadScreen (void)
{

	if (developer.value) {
		return;
	}
	if (!con_forcedup) {
	    return;
	}

	if (loadingScreen) {
		Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 255);
		if (!loadscreeninit) {
			lscreen = Image_LoadImage(va("gfx/lscreen/%s", loadname2), IMAGE_TGA | IMAGE_PNG | IMAGE_JPG, 0, false, false);

			if (lscreen == 0 || lscreen == 0) {
				lscreen = Image_LoadImage("gfx/lscreen/lscreen", IMAGE_PNG, 0, false, false);
			}

			loadscreeninit = true;
		}

		Draw_StretchPic(scr_vrect.x, scr_vrect.y, lscreen, vid.width, vid.height);

		Draw_FillByColor(0, 0, vid.width, 48, 0, 0, 0, 175);
		Draw_FillByColor(0, vid.height - 48, vid.width, 48, 0, 0, 0, 175);

		Draw_ColoredString(2, 4, loadnamespec, 255, 255, 0, 255, 2);
	}

	if (loadingtimechange < Sys_FloatTime ())
	{
        lodinglinetext = ReturnLoadingtex();
        loadingtimechange = Sys_FloatTime () + 5;
	}

	if (key_dest == key_game) {
		Draw_ColoredStringCentered(vid.height - 20, lodinglinetext, 255, 255, 255, 255, 1.5f);
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
	if (!cl.worldmodel || cls.signon != SIGNONS)//blubs here, undid it actually
	{
		con_forcedup = true;
	}
	else
	{
		con_forcedup = false;
	}

	if (con_forcedup)
	{
		scr_conlines = vid.height;		// full screen
		scr_con_current = scr_conlines;
	}
	else if (key_dest == key_console)
		scr_conlines = vid.height/2.0f;	// half screen
	else
		scr_conlines = 0;				// none visible

	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed.value*(float)host_frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed.value*(float)host_frametime;
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
		Con_DrawConsole (scr_con_current, true, 1);
		clearconsole = 0;
	}
	else
	{
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();	// only draw notify in game
	}
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
int GetWeaponZoomAmount (void)
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
void Draw_Crosshair (void);
void SCR_UpdateScreen(void) {
#ifndef GLQUAKE
    vrect_t vrect;

    if (scr_skipupdate) return;
#endif
    if (scr_block_drawing) return;

#ifdef GLQUAKE
    vid.numpages = 2 + gl_triplebuffer.value;
#endif

#ifdef NQ_HACK
    if (scr_disabled_for_loading) {
        /*
         * FIXME - this really needs to be fixed properly.
         * Simply starting a new game and typing "changelevel foo" will hang
         * the engine for 5s (was 60s!) if foo.bsp does not exist.
         */
        if (realtime - scr_disabled_time > 5) {
            scr_disabled_for_loading = false;
            Con_Printf("load failed.\n");
        } else
            return;
    }
#endif
#ifdef QW_HACK
    if (scr_disabled_for_loading) return;
#endif

#if defined(_WIN32) && !defined(GLQUAKE)
    /* Don't suck up CPU if minimized */
    if (!window_visible()) return;
#endif

#ifdef NQ_HACK
    if (cls.state == ca_dedicated) return;  // stdout only
#endif

    if (!scr_initialized || !con_initialized) return;  // not initialized yet

    scr_copytop = 0;
    scr_copyeverything = 0;

#ifdef GLQUAKE
    GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
#endif

    if (cl.stats[STAT_ZOOM] == 1)
    {
        if(!original_fov) {
            original_fov = scr_fov.value;
            original_view_fov = scr_fov_viewmodel.value;
        }
            
        if(scr_fov.value > (GetWeaponZoomAmount() + 1))//+1 for accounting for floating point inaccurraces
        {
            scr_fov.value += ((original_fov - GetWeaponZoomAmount()) - scr_fov.value) * 0.25f;
            scr_fov_viewmodel.value += ((original_view_fov - GetWeaponZoomAmount()) - scr_fov_viewmodel.value) * 0.25f;
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
            scr_fov.value += (original_fov - scr_fov.value) * 0.25f;
            scr_fov_viewmodel.value += (original_view_fov - scr_fov_viewmodel.value) * 0.25f;
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
        vid.recalc_refdef = true;
    }

    if (oldscreensize != scr_viewsize.value)
    {
        oldscreensize = scr_viewsize.value;
        vid.recalc_refdef = true;
    }

    if (vid.recalc_refdef)
        SCR_CalcRefdef ();

        /*
         * do 3D refresh drawing, and then update the screen
         */
    SCR_SetUpToDrawConsole();

    V_RenderView();

    GL_Set2D();

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

#ifndef GLQUAKE
    /* for adapters that can't stay mapped in for linear writes all the time */
    D_DisableBackBufferAccess();
    if (pconupdate) D_UpdateRects(pconupdate);
#endif

    V_UpdatePalette();

    GL_EndRendering();
}

#if !defined(GLQUAKE) && defined(_WIN32)
/*
==================
SCR_UpdateWholeScreen
FIXME - vid_win.c only?
==================
*/
void SCR_UpdateWholeScreen(void) {
    scr_fullupdate = 0;
    SCR_UpdateScreen();
}
#endif

//=============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init(void) {
    Cvar_RegisterVariable(&scr_fov);
    Cvar_RegisterVariable (&scr_fov_viewmodel);
    Cvar_RegisterVariable(&scr_viewsize);
    Cvar_RegisterVariable(&scr_conspeed);
    Cvar_RegisterVariable(&scr_centertime);
    Cvar_RegisterVariable(&scr_printspeed);
    Cvar_RegisterVariable (&scr_loadscreen);
#ifdef GLQUAKE
    Cvar_RegisterVariable(&gl_triplebuffer);
#endif

    Cmd_AddCommand("screenshot", SCR_ScreenShot_f);
    Cmd_AddCommand("sizeup", SCR_SizeUp_f);
    Cmd_AddCommand("sizedown", SCR_SizeDown_f);

    Cvar_RegisterVariable (&cl_crosshair_debug);

    hitmark = Image_LoadImage("gfx/hud/hit_marker", IMAGE_TGA, 0, true, false);

    scr_initialized = true;
}
