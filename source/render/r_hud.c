/*
 * Copyright (C) 1996-1997 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */
// r_hud.c -- heads-up display

#include "../nzportable_def.h"
#include "../menu/menu_defs.h"

#ifdef PSP_VFPU
#include <pspmath.h>
#endif

// HUD image defines
image_t sb_round[5];
image_t sb_round_num[10];
image_t sb_moneyback;
image_t instapic;
image_t x2pic;
image_t revivepic;
image_t jugpic;
image_t floppic;
image_t staminpic;
image_t doublepic;
image_t doublepic2;
image_t speedpic;
image_t deadpic;
image_t mulepic;
image_t fragpic;
image_t bettypic;

image_t fx_blood_lu;
image_t fx_blood_ru;
image_t fx_blood_ld;
image_t fx_blood_rd;

// Button image defines
image_t b_rightface;
image_t b_leftface;
image_t b_bottomface;
image_t b_topface;
image_t b_left;
image_t b_right;
image_t b_up;
image_t b_down;
image_t b_lt;
image_t b_rt;
image_t b_zlt;
image_t b_zrt;
image_t b_start;
image_t b_select;
image_t b_home;

qboolean has_chaptertitle;
qboolean doubletap_has_damage_buff;
int current_gamemode;

void
HUD_Scoreboard_Down(void);
void
HUD_Scoreboard_Up(void);

double HUD_Change_time;
double bettyprompt_time;
double nameprint_time;
double hud_maxammo_starttime;
double hud_maxammo_endtime;

int perk_order[8];
int current_perk_order;
int perk_orientation;

double crosshair_spread_time;
float cur_spread;
float crosshair_offset_step;

char player_name[16];

extern cvar_t waypoint_mode;
extern cvar_t sv_gamemode;

int screenflash_color;
double screenflash_duration;
int screenflash_type;
double screenflash_worktime;
double screenflash_starttime;

int old_points;
int current_points;
int point_change_interval;
int point_change_interval_neg;
int alphabling = 0;
float round_center_x;
float round_center_y;

vec3_t round_color_target;

typedef struct {
    int      difference; // the difference of points
    int      player;     // who does this element belong to?
    float    travel_x;   // distance moved on the x axis
    float    travel_y;   // distance moved on the y axis
    double   start_time;
    qboolean occupied; // is this array being used/occupied?
} point_change_t;

#define MAX_POINT_ELEMENTS 64 // the maximum amount of point differential elements that can be spawned before
                              // we iterate back from 0
point_change_t point_change[MAX_POINT_ELEMENTS];
static int hud_last_points[MAX_SCOREBOARD];
static qboolean hud_last_points_valid[MAX_SCOREBOARD];
static int next_point_change;
static double hud_map_start_time;
static double hud_round_start_time;
static int hud_stopwatch_round;

extern cvar_t scr_centertime;
extern cvar_t scr_printspeed;
extern cvar_t cl_textopacity;
extern cvar_t cl_colorblind;
extern cvar_t cl_cinematic;
extern cvar_t vid_ultrawide_limiter;
extern cvar_t cl_crosshairdot;
extern cvar_t cl_hitmarkers;
extern cvar_t scr_serverstopwatch;
extern cvar_t scr_playerdebuginfo;
extern cvar_t scr_playerdebuginfo_x;
extern cvar_t scr_playerdebuginfo_y;
extern cvar_t crosshair;
extern cvar_t cl_crosshair_debug;
extern qboolean crosshair_pulse_grenade;
extern qboolean croshhairmoving;

static image_t hud_sniper_scope;
static image_t hud_hitmarker;
static double hud_hitmarker_time;
static double hud_hitmarker_ignore_time;
static int hud_hitmarker_type;
static void
HUD_PlayerColor(int player, int * r, int * g, int * b);

static const char *
HUD_GetPerkName(int perk)
{
    switch (perk) {
        case 1: return "Quick Revive";

        case 2: return "Jugger-Nog";

        case 3: return "Speed Cola";

        case 4: return "Double Tap Root Beer";

        case 5: return "Stamin-Up";

        case 6: return "PhD Flopper";

        case 7: return "Deadshot Daiquiri";

        case 8: return "Mule Kick";

        default: return "NULL";
    }
}

static int
HUD_UltrawideOffset(void)
{
    int screen_width = (int) vid.width;
    int safe_width;

    if (!vid_ultrawide_limiter.value)
        return 0;

    safe_width = (vid.height * 4) / 3;
    return screen_width > safe_width ? (screen_width - safe_width) / 2 : 0;
}

static float
HUD_SmoothStep(float value)
{
    if (value < 0)
        value = 0;
    else if (value > 1)
        value = 1;
    return value * value * (3.0f - 2.0f * value);
}

static void
HUD_DrawTextBackdrop(int x, int y, const char * text, int r, int g, int b, int a, float scale)
{
    int pad    = (int) (2 * scale);
    int height = (int) (8 * scale);
    int alpha  = (int) (cl_textopacity.value * a);

    if (alpha > 0)
        Draw_FillByColor(x - pad, y - pad, getTextWidth((char *) text, scale) + pad * 2,
          height + pad * 2, 0, 0, 0, alpha);
    Draw_ColoredString(x, y, (char *) text, r, g, b, a, scale);
}

// Centerprints rise four base pixels while fading in and out.
static char hud_centerstring[1024];
static double hud_center_start;
static int hud_center_lines;
float scr_centertime_off;
float scr_usetime_off;

void
HUD_CenterPrint(char * str)
{
    char * scan;

    Q_strncpyz(hud_centerstring, str, sizeof(hud_centerstring));
    hud_center_start   = Sys_FloatTime();
    scr_centertime_off = scr_centertime.value + 0.5f;
    hud_center_lines   = 1;
    for (scan = hud_centerstring; *scan; scan++)
        if (*scan == '\n')
            hud_center_lines++;
}

static void
HUD_DrawCenterPrint(void)
{
    const float transition = 0.25f;
    const float travel     = 4.0f;
    float elapsed = (float) (Sys_FloatTime() - hud_center_start);
    float hold    = scr_centertime.value < 0 ? 0 : scr_centertime.value;
    float alpha, offset, t;
    char * start;
    char line[41];
    int len, y;

    if (!hud_centerstring[0] || key_dest != key_game || elapsed >= hold + transition * 2)
        return;

    if (elapsed < transition) {
        t      = HUD_SmoothStep((float) elapsed / transition);
        alpha  = t;
        offset = travel * (1.0f - t);
    } else if (elapsed < transition + hold) {
        alpha  = 1;
        offset = 0;
    } else {
        t      = HUD_SmoothStep(((float) elapsed - transition - hold) / transition);
        alpha  = 1.0f - t;
        offset = -travel * t;
    }

    y = (hud_center_lines <= 4 ? (int) (vid.height * 0.35f) : 48 * vid.scale)
      + (int) (offset * vid.scale);
    start = hud_centerstring;
    do {
        for (len = 0; len < 40 && start[len] && start[len] != '\n'; len++) { }
        memcpy(line, start, len);
        line[len] = 0;
        HUD_DrawTextBackdrop((vid.width - getTextWidth(line, vid.scale)) / 2, y,
          line, 255, 255, 255, (int) (alpha * 255), vid.scale);
        y     += 10 * vid.scale;
        start += len;
        if (*start == '\n')
            start++;
    } while (*start);
} /* HUD_DrawCenterPrint */

static char hud_usestring[128];
static char hud_usecost[64];
static double hud_use_until;
static int hud_use_button_x;
static int hud_use_key;
static int hud_use_type;

static int
HUD_GetBoundKey(const char * command)
{
    int key;
    size_t len = strlen(command);

    for (key = 0; key < 256; key++) {
        if ((keybindings[key] && !strncmp(keybindings[key], command, len)) ||
          (dtbindings[key] && !strncmp(dtbindings[key], command, len)) ||
          (holdbindings[key] && !strncmp(holdbindings[key], command, len)))
            return key;
    }
    return -1;
}

static const char *
HUD_UseKeyLabel(int key)
{
    static char label[32];
    int i;

    if (key < 0)
        return "UNBOUND";

    Q_strncpyz(label, Key_KeynumToString(key), sizeof(label));
    for (i = 0; label[i]; i++)
        label[i] = toupper((unsigned char) label[i]);
    return label;
}

static image_t
HUD_KeyIcon(int key)
{
    switch (key) {
        case K_UPARROW: return b_up;

        case K_DOWNARROW: return b_down;

        case K_LEFTARROW: return b_left;

        case K_RIGHTARROW: return b_right;

        case K_SELECT: return b_select;

        case K_RIGHTFACE: return b_rightface;

        case K_BOTTOMFACE: return b_bottomface;

        case K_TOPFACE: return b_topface;

        case K_LEFTFACE: return b_leftface;

        case K_LTRIGGER: return b_lt;

        case K_RTRIGGER: return b_rt;

        case K_ZLTRIGGER: return b_zlt;

        case K_ZRTRIGGER: return b_zrt;

        default: return -1;
    }
}

int
GetButtonIcon(char * command)
{
    image_t icon = HUD_KeyIcon(HUD_GetBoundKey(command));

    return icon >= 0 ? icon : b_rightface;
}

char *
GetUseButtonL(void)
{
    int key = HUD_GetBoundKey("+use");

    return HUD_KeyIcon(key) >= 0 ? "  " : (char *) HUD_UseKeyLabel(key);
}

char *
GetGrenadeButtonL(void)
{
    int key = HUD_GetBoundKey("+grenade");

    return HUD_KeyIcon(key) >= 0 ? "  " : (char *) HUD_UseKeyLabel(key);
}

void
HUD_UsePrint(int type, int cost, int weapon)
{
    const char * button;
    const char * item = weapon ? HUD_GetPerkName(weapon) : "";

    hud_use_key = HUD_GetBoundKey("+use");
    button      = HUD_KeyIcon(hud_use_key) >= 0 ? "  " : HUD_UseKeyLabel(hud_use_key);

    hud_usestring[0] = hud_usecost[0] = 0;
    switch (type) {
        case 0: break;
        case 1: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to open Door", button);
            break;
        case 2: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to remove Debris", button);
            break;
        case 3: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to buy Ammo for %s", button,
              PR_GetString(sv_player->v.Weapon_Name_Touch));
            break;
        case 4: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to buy %s", button,
              PR_GetString(sv_player->v.Weapon_Name_Touch));
            break;
        case 5: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to Rebuild Barrier", button);
            break;
        case 6: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s for Mystery Box", button);
            break;
        case 7: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s for %s", button,
              PR_GetString(sv_player->v.Weapon_Name_Touch));
            break;
        case 8: Q_strncpyz(hud_usestring, "The Power must be Activated first", sizeof(hud_usestring));
            break;
        case 9: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to buy %s", button, item);
            break;
        case 10: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to Turn On the Power", button);
            break;
        case 11: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to Activate the Trap", button);
            break;
        case 12: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to Pack-a-Punch", button);
            break;
        case 13: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to Fix your Code.. :)", button);
            break;
        case 14: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to use Teleporter", button);
            break;
        case 15: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to use Teleporter", button);
            break;
        case 16: Q_strncpyz(hud_usestring, "Teleporter is cooling down", sizeof(hud_usestring));
            break;
        case 17: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to initiate link to pad", button);
            break;
        case 18: Q_strncpyz(hud_usestring, "Link not active", sizeof(hud_usestring));
            break;
        case 19: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to link pad with core", button);
            break;
        case 20: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to End the Game", button);
            break;
        case 21: Q_strncpyz(hud_usestring, "...", sizeof(hud_usestring));
            break;
        case 22: Q_strncpyz(hud_usestring, "It doesn't seem to be working..", sizeof(hud_usestring));
            break;
        case 23: snprintf(hud_usestring, sizeof(hud_usestring), "Hold %s to Drop the Nuke", button);
            break;
        default: Con_Printf("No type defined in engine for useprint\n");
            return;
    }
    if (cost && (type == 1 || type == 2 || type == 3 || type == 4 || type == 6 ||
      type == 9 || type == 11 || type == 12 || type == 15 || type == 20))
        snprintf(hud_usecost, sizeof(hud_usecost), "[Cost: %i]", cost);
    hud_use_button_x = getTextWidth("Hold ", vid.scale);
    hud_use_type     = type;
    hud_use_until    = Sys_FloatTime() + 0.1;
    scr_usetime_off  = 0.1f;
} /* HUD_UsePrint */

/************************
*    HUD_Useprint       *
************************/

static void
HUD_DrawUsePrint(void)
{
    int y, x;

    if (Sys_FloatTime() >= hud_use_until || key_dest != key_game || cl.stats[STAT_HEALTH] <= 0) {
        scr_usetime_off = 0;
        return;
    }
    scr_usetime_off = (float) (hud_use_until - Sys_FloatTime());
    y = vid.height - 88 * vid.scale;
    x = (vid.width - getTextWidth(hud_usestring, vid.scale)) / 2;
    HUD_DrawTextBackdrop(x, y, hud_usestring, 255, hud_use_type == 22 ? 0 : 255,
      hud_use_type == 22 ? 0 : 255, 255, vid.scale);
    if (!strncmp(hud_usestring, "Hold ", 5) && HUD_KeyIcon(hud_use_key) >= 0)
        Draw_ColoredStretchPic(x + hud_use_button_x, y - 4 * vid.scale, HUD_KeyIcon(hud_use_key),
          16 * vid.scale, 16 * vid.scale, 255, 255, 255, 255);
    else if (!strncmp(hud_usestring, "Hold ", 5))
        Draw_ColoredString(x + hud_use_button_x, y, (char *) HUD_UseKeyLabel(hud_use_key),
          255, 255, 0, 255, vid.scale);
    if (hud_usecost[0])
        HUD_DrawTextBackdrop((vid.width - getTextWidth(hud_usecost, vid.scale)) / 2,
          y + 10 * vid.scale, hud_usecost, 255, 255, 255, 255, vid.scale);
    UNUSED(hud_use_button_x);
}

static void
HUD_DrawWaypointBinding(int x, int y, const char * command, const char * action)
{
    int key            = HUD_GetBoundKey(command);
    image_t icon       = HUD_KeyIcon(key);
    const char * label = HUD_UseKeyLabel(key);
    char text[128];
    int bind_x;

    if (icon >= 0)
        snprintf(text, sizeof(text), "Press    to %s", action);
    else
        snprintf(text, sizeof(text), "Press %s to %s", label, action);
    Draw_ColoredString(x, y, text, 255, 255, 255, 255, vid.scale);
    bind_x = x + getTextWidth("Press ", vid.scale);
    if (icon >= 0)
        Draw_StretchPic(bind_x, y - 3 * vid.scale, icon, 13 * vid.scale, 13 * vid.scale);
    else
        Draw_ColoredString(bind_x, y, (char *) label, 255, 255, 0, 255, vid.scale);
}

static void
HUD_Waypoint(void)
{
    int x     = 5 * vid.scale + HUD_UltrawideOffset();
    int y     = 5 * vid.scale;
    int width = 300 * vid.scale;
    int available_width = (int) vid.width - x * 2;

    if (width > available_width)
        width = available_width;
    Draw_FillByColor(x - 3 * vid.scale, y - 2 * vid.scale, width, 116 * vid.scale,
      0, 0, 0, cl_textopacity.value * 255);
    Draw_ColoredString(x, y, "WAYPOINT MODE", 255, 255, 255, 255, vid.scale);
    Draw_ColoredString(x, y + 10 * vid.scale, "Create/Tweak Zombie AI Paths", 255, 255, 0, 255, vid.scale);
    HUD_DrawWaypointBinding(x, y + 24 * vid.scale, "+attack", "Create a Waypoint");
    HUD_DrawWaypointBinding(x, y + 36 * vid.scale, "+use", "Select a Waypoint");
    HUD_DrawWaypointBinding(x, y + 48 * vid.scale, "+aim", "Link a Waypoint");
    HUD_DrawWaypointBinding(x, y + 60 * vid.scale, "+knife", "Remove a Waypoint");
    HUD_DrawWaypointBinding(x, y + 72 * vid.scale, "+switch", "Move selected Waypoint Here");
    HUD_DrawWaypointBinding(x, y + 84 * vid.scale, "+reload", "Create a Special Waypoint");
    HUD_DrawWaypointBinding(x, y + 96 * vid.scale, "impulse 22", "Load Waypoints");
    HUD_DrawWaypointBinding(x, y + 108 * vid.scale, "impulse 24", "Save Waypoints");
}

/*
 * ===============
 * HUD_Init
 * ===============
 */
void
HUD_Init(void)
{
    int i;

    has_chaptertitle = false;

    for (i = 0 ; i < 5 ; i++) {
        sb_round[i] = Image_LoadImage(va("gfx/hud/r%i", i + 1), IMAGE_TGA, 0, true, false);
    }

    for (i = 0 ; i < 10 ; i++) {
        sb_round_num[i] = Image_LoadImage(va("gfx/hud/r_num%i", i), IMAGE_TGA, 0, true, false);
    }

    sb_moneyback = Image_LoadImage("gfx/hud/moneyback", IMAGE_TGA, 0, true, false);
    instapic     = Image_LoadImage("gfx/hud/in_kill", IMAGE_TGA, 0, true, false);
    x2pic        = Image_LoadImage("gfx/hud/2x", IMAGE_TGA, 0, true, false);

    revivepic        = Image_LoadImage("gfx/hud/revive", IMAGE_TGA, 0, true, false);
    jugpic           = Image_LoadImage("gfx/hud/jug", IMAGE_TGA, 0, true, false);
    floppic          = Image_LoadImage("gfx/hud/flopper", IMAGE_TGA, 0, true, false);
    staminpic        = Image_LoadImage("gfx/hud/stamin", IMAGE_TGA, 0, true, false);
    doublepic        = Image_LoadImage("gfx/hud/double", IMAGE_TGA, 0, true, false);
    doublepic2       = Image_LoadImage("gfx/hud/double2", IMAGE_TGA, 0, true, false);
    speedpic         = Image_LoadImage("gfx/hud/speed", IMAGE_TGA, 0, true, false);
    deadpic          = Image_LoadImage("gfx/hud/dead", IMAGE_TGA, 0, true, false);
    mulepic          = Image_LoadImage("gfx/hud/mule", IMAGE_TGA, 0, true, false);
    fragpic          = Image_LoadImage("gfx/hud/frag", IMAGE_TGA, 0, true, false);
    bettypic         = Image_LoadImage("gfx/hud/betty", IMAGE_TGA, 0, true, false);
    hud_sniper_scope = Image_LoadImage("gfx/hud/scope_nb", IMAGE_TGA, 0, true, false);
    hud_hitmarker    = Image_LoadImage("gfx/hud/hit_marker", IMAGE_TGA, 0, true, false);

    b_rightface  = Image_LoadImage("gfx/butticons/rightface", IMAGE_TGA, 0, true, false);
    b_leftface   = Image_LoadImage("gfx/butticons/leftface", IMAGE_TGA, 0, true, false);
    b_bottomface = Image_LoadImage("gfx/butticons/bottomface", IMAGE_TGA, 0, true, false);
    b_topface    = Image_LoadImage("gfx/butticons/topface", IMAGE_TGA, 0, true, false);
    b_left       = Image_LoadImage("gfx/butticons/left", IMAGE_TGA, 0, true, false);
    b_right      = Image_LoadImage("gfx/butticons/right", IMAGE_TGA, 0, true, false);
    b_up         = Image_LoadImage("gfx/butticons/up", IMAGE_TGA, 0, true, false);
    b_down       = Image_LoadImage("gfx/butticons/down", IMAGE_TGA, 0, true, false);
    b_lt         = Image_LoadImage("gfx/butticons/lt", IMAGE_TGA, 0, true, false);
    b_rt         = Image_LoadImage("gfx/butticons/rt", IMAGE_TGA, 0, true, false);
    b_zlt        = Image_LoadImage("gfx/butticons/zlt", IMAGE_TGA, 0, true, false);
    b_zrt        = Image_LoadImage("gfx/butticons/zrt", IMAGE_TGA, 0, true, false);
    b_start      = Image_LoadImage("gfx/butticons/start", IMAGE_TGA, 0, true, false);
    b_select     = Image_LoadImage("gfx/butticons/select", IMAGE_TGA, 0, true, false);
    b_home       = Image_LoadImage("gfx/butticons/home", IMAGE_TGA, 0, true, false);

    fx_blood_lu = Image_LoadImage("gfx/hud/blood", IMAGE_TGA, 0, true, false);

    Cmd_AddCommand("+showscores", HUD_Scoreboard_Down);
    Cmd_AddCommand("-showscores", HUD_Scoreboard_Up);
} /* HUD_Init */

/*
 * ===============
 * HUD_NewMap
 * ===============
 */
void
HUD_NewMap(void)
{
    alphabling = 0;

    memset(point_change, 0, sizeof(point_change));
    memset(hud_last_points, 0, sizeof(hud_last_points));
    memset(hud_last_points_valid, 0, sizeof(hud_last_points_valid));
    next_point_change   = 0;
    hud_map_start_time  = hud_round_start_time = Sys_FloatTime();
    hud_stopwatch_round = cl.stats[STAT_ROUNDS];

    old_points                = 500;
    current_points            = 500;
    point_change_interval     = 0;
    point_change_interval_neg = 0;

    round_center_x = (vid.width - 11) / 2.0f;
    round_center_y = (vid.height - 48) / 2.0f;

    bettyprompt_time          = 0;
    nameprint_time            = 0;
    hud_hitmarker_time        = 0;
    hud_hitmarker_ignore_time = 0;
    hud_hitmarker_type        = HITMARK_NORMAL;

    hud_maxammo_starttime = 0;
    hud_maxammo_endtime   = 0;

    perk_order[0]         = 0;
    perk_order[1]         = 0;
    perk_order[2]         = 0;
    perk_order[3]         = 0;
    perk_order[4]         = 0;
    perk_order[5]         = 0;
    perk_order[6]         = 0;
    perk_order[7]         = 0;
    cl.perks              = 0;
    perk_orientation      = 0;
    current_perk_order    = 0;
    crosshair_spread_time = 0;
    crosshair_offset_step = 0;
    cur_spread            = 0;

    round_color_target[0] = 107;
    round_color_target[1] = 1;
    round_color_target[2] = 0;
} /* HUD_NewMap */

/*
 * =============
 * HUD_itoa
 * =============
 */
int
HUD_itoa(int num, char * buf)
{
    char * str;
    int pow10;
    int dig;

    str = buf;

    if (num < 0) {
        *str++ = '-';
        num    = -num;
    }

    for (pow10 = 10 ; num >= pow10 ; pow10 *= 10);

    do{
        pow10 /= 10;
        dig    = num / pow10;
        *str++ = '0' + dig;
        num   -= dig * pow10;
    } while (pow10 != 1);

    *str = 0;

    return str - buf;
}

// =============================================================================

int pointsort[MAX_SCOREBOARD];

char scoreboardtext[MAX_SCOREBOARD][20];
int scoreboardtop[MAX_SCOREBOARD];
int scoreboardbottom[MAX_SCOREBOARD];
int scoreboardcount[MAX_SCOREBOARD];
int scoreboardlines;

/*
 * ===============
 * HUD_Sorpoints
 * ===============
 */
void
HUD_Sortpoints(void)
{
    int i, j, k;

    // sort by points
    scoreboardlines = 0;
    for (i = 0 ; i < cl.maxclients ; i++) {
        if (cl.scores[i].name[0]) {
            pointsort[scoreboardlines] = i;
            scoreboardlines++;
        }
    }

    for (i = 0 ; i < scoreboardlines ; i++)
        for (j = 0 ; j < scoreboardlines - 1 - i ; j++)
            if (cl.scores[pointsort[j]].points < cl.scores[pointsort[j + 1]].points) {
                k = pointsort[j];
                pointsort[j]     = pointsort[j + 1];
                pointsort[j + 1] = k;
            }
}

qboolean showscoreboard = false;
void
HUD_Scoreboard_Down(void)
{
    if (key_dest == key_game)
        showscoreboard = true;
}

void
HUD_Scoreboard_Up(void)
{
    showscoreboard = false;
}

/*******************
*    HUD Scores    *
*******************/
static const char *
HUD_GetPrettyMapName(void)
{
    static char bsp_name[MAX_QPATH];
    int i;

    if (map_loadname_pretty && map_loadname_pretty[0])
        return map_loadname_pretty;

    if (cl.worldmodel) {
        COM_FileBase(cl.worldmodel->name, bsp_name);
        for (i = 0; i < num_user_maps; i++) {
            if (custom_maps[i].occupied && custom_maps[i].map_name &&
              !strcmp(custom_maps[i].map_name, bsp_name) &&
              custom_maps[i].map_name_pretty && custom_maps[i].map_name_pretty[0])
                return custom_maps[i].map_name_pretty;
        }
    }
    if (cl.levelname[0])
        return cl.levelname;

    return bsp_name;
}

void
HUD_EndScreen(void)
{
    scoreboard_t * score;
    char text[96];
    int i, player, column;
    int screen_width   = (int) vid.width;
    qboolean condensed = screen_width <= 320 * vid.scale;
    int panel_width    = (condensed ? 320 : 400) * vid.scale;
    int panel_x        = (screen_width - panel_width) / 2;
    int header_y       = 89 * vid.scale;
    int header_height  = 10 * vid.scale;
    int header_gap     = 2 * vid.scale;
    int row_height     = 11 * vid.scale;
    float text_scale   = vid.scale;
    static const char * full_headers[]      = { "Score", "Kills", "Downs", "Revives", "Headshots" };
    static const char * condensed_headers[] = { "PTS", "K", "D", "R", "HS" };
    static const float condensed_offsets[]  = { -44.0f, 2.0f, 42.0f, 82.0f, 123.0f };
    static const float condensed_widths[]   = { 52.0f, 40.0f, 40.0f, 40.0f, 42.0f };
    static const float full_offsets[]       = { -87.0f, -31.0f, 25.0f, 81.0f, 137.0f };
    static const float full_widths[]        = { 56.0f, 56.0f, 56.0f, 56.0f, 56.0f };
    const char ** headers      = condensed ? condensed_headers : full_headers;
    const float * stat_offsets = condensed ? condensed_offsets : full_offsets;
    const float * stat_widths  = condensed ? condensed_widths : full_widths;
    const char * map_title     = HUD_GetPrettyMapName();

    if (panel_width > screen_width)
        panel_width = screen_width;
    panel_x = (screen_width - panel_width) / 2;

    HUD_Sortpoints();

    if (!showscoreboard) {
        HUD_DrawTextBackdrop((vid.width - getTextWidth("GAME OVER", 2 * vid.scale)) / 2,
          50 * vid.scale, "GAME OVER", 255, 255, 255, 255, 2 * vid.scale);
        snprintf(text, sizeof(text), "You Survived %i Round%s", cl.stats[STAT_ROUNDS],
          cl.stats[STAT_ROUNDS] == 1 ? "" : "s");
        HUD_DrawTextBackdrop((vid.width - getTextWidth(text, 1.5f * vid.scale)) / 2,
          68 * vid.scale, text, 255, 255, 255, 255, 1.5f * vid.scale);
    }

    // Scoreboard header
    Draw_FillByColor(panel_x, header_y, panel_width, header_height, 0, 0, 0, 204);
    Draw_FillByColor(panel_x - vid.scale, header_y - vid.scale,
      panel_width + 2 * vid.scale, vid.scale, 160, 160, 160, 255);
    Draw_FillByColor(panel_x - vid.scale, header_y + header_height,
      panel_width + 2 * vid.scale, vid.scale, 160, 160, 160, 255);
    Draw_FillByColor(panel_x - vid.scale, header_y, vid.scale,
      header_height, 160, 160, 160, 255);
    Draw_FillByColor(panel_x + panel_width, header_y, vid.scale,
      header_height, 160, 160, 160, 255);
    Draw_ColoredString(panel_x + 3 * vid.scale, header_y + 2 * vid.scale,
      (char *) map_title, 255, 255, 255, 255, text_scale);
    for (column = 0; column < 5; column++) {
        int center = vid.width / 2 + stat_offsets[column] * vid.scale;
        Draw_ColoredString(center - getTextWidth((char *) headers[column], text_scale) / 2,
          header_y + 2 * vid.scale, (char *) headers[column], 255, 255, 255, 255, text_scale);
    }
    Draw_FillByColor(panel_x - vid.scale, header_y + header_height + header_gap - vid.scale,
      panel_width + 2 * vid.scale, vid.scale, 160, 160, 160, 255);

    for (i = 0; i < scoreboardlines && i < 8; i++) {
        int r, g, b;
        int row_y = header_y + header_height + header_gap + i * row_height;
        int values[5];
        player = pointsort[i];
        score  = &cl.scores[player];
        HUD_PlayerColor(player, &r, &g, &b);

        // Fill
        Draw_FillByColor(panel_x, row_y, panel_width, row_height, 0, 0, 0, 204);
        for (column = 0; column < 5; column += 2) {
            int center = vid.width / 2 + stat_offsets[column] * vid.scale;
            Draw_FillByColor(center - stat_widths[column] * vid.scale / 2, row_y,
              stat_widths[column] * vid.scale, row_height, 160, 0, 0, 90);
        }
        Draw_FillByColor(panel_x - vid.scale, row_y + row_height,
          panel_width + 2 * vid.scale, vid.scale, 160, 160, 160, 255);
        Draw_FillByColor(panel_x - vid.scale, row_y, vid.scale,
          row_height, 160, 160, 160, 255);
        Draw_FillByColor(panel_x + panel_width, row_y, vid.scale,
          row_height, 160, 160, 160, 255);

        Draw_ColoredString(panel_x + 3 * vid.scale, row_y + 2 * vid.scale,
          score->name, r, g, b, 255, text_scale);
        values[0] = score->points;
        values[1] = score->kills;
        values[2] = score->downs;
        values[3] = score->revives;
        values[4] = score->headshots;
        for (column = 0; column < 5; column++) {
            int center = vid.width / 2 + stat_offsets[column] * vid.scale;
            snprintf(text, sizeof(text), "%i", values[column]);
            Draw_ColoredString(center - getTextWidth(text, text_scale) / 2,
              row_y + 2 * vid.scale, text, r, g, b, 255, text_scale);
        }

        // Ping
        {
            int bars   = score->ping < 100 ? 4 : score->ping < 200 ? 3 : score->ping < 300 ? 2 : 1;
            int ping_r = score->ping < 300 ? (score->ping < 200 ? 0 : 255) : 255;
            int ping_g = score->ping < 300 ? (score->ping < 200 ? 255 : 128) : 0;
            int ping_x = vid.width / 2 + (condensed ? 145 : 164.81f) * vid.scale;
            int bar;
            for (bar = 0; bar < bars; bar++)
                Draw_FillByColor(ping_x + bar * 2 * vid.scale,
                  row_y + row_height - (2 + bar * 2) * vid.scale, vid.scale,
                  (2 + bar * 2) * vid.scale, ping_r, ping_g, 0, 255);
            if (!condensed) {
                snprintf(text, sizeof(text), "%ims", score->ping);
                Draw_ColoredString(vid.width / 2 + 185.56f * vid.scale - getTextWidth(text, text_scale) / 2,
                  row_y + 2 * vid.scale, text, r, g, b, 255, text_scale);
            }
        }
    }

    snprintf(text, sizeof(text), "Nazi Zombies: Portable %s", game_build_date);
    Draw_ColoredString(2 * vid.scale, vid.height - 9 * vid.scale,
      text, 255, 255, 255, 255, vid.scale);
} /* HUD_EndScreen */

// =============================================================================

// =============================================================================//
// ===============================DRAW FUNCTIONS================================//
// =============================================================================//

/*******************
*    HUD_Points    *
*******************/


void
HUD_Parse_Point_Change(int points, int negative, int player, int unused_y)
{
    point_change_t * change = &point_change[next_point_change];
    float random_x = (float) rand() / (float) RAND_MAX;
    float random_y = (float) rand() / (float) RAND_MAX;

    UNUSED(unused_y);
    memset(change, 0, sizeof(*change));
    change->difference = negative ? -points : points;
    change->player     = player;
    // Black Ops uses 20..59 horizontal and -14..15 vertical pixels at 640x480.
    // Vril's HUD coordinates use a 320x240 base, so those distances are halved.
    change->travel_x   = (10.0f + random_x * 19.5f) * vid.scale;
    change->travel_y   = (-7.0f + random_y * 14.5f) * vid.scale;
    change->start_time = Sys_FloatTime();
    change->occupied   = true;
    next_point_change  = (next_point_change + 1) % MAX_POINT_ELEMENTS;
}

static void
HUD_PlayerColor(int player, int * r, int * g, int * b)
{
    static const byte colors[8][3] = {
        { 255, 255, 255 }, { 0,   117, 179  }, { 235, 189, 0   }, { 0,   230, 33 },
        { 255, 145, 163 }, { 204, 51,  140  }, { 107, 209, 209 }, { 255, 222, 33 }
    };
    static const byte colorblind[8][3] = {
        { 255, 255, 255 }, { 99,  194, 237  }, { 237, 107, 0   }, { 0,   176, 133 },
        { 255, 117, 138 }, { 186, 74,  138  }, { 92,  214, 245 }, { 219, 199, 64  }
    };

    const byte(*palette)[3] = cl_colorblind.value ? colorblind : colors;
    int index = player & 7;

    *r = palette[index][0];
    *g = palette[index][1];
    *b = palette[index][2];
}

void
HUD_Points(void)
{
    int i, k, l;
    int x, y, f, xplus, r, g, b;
    scoreboard_t * s;

    // scores
    HUD_Sortpoints();

    // draw the text
    l = scoreboardlines;


    x = 6 * vid.scale + HUD_UltrawideOffset();
    for (i = 0 ; i < l ; i++) {
        k = pointsort[i];
        s = &cl.scores[k];
        if (!s->name[0])
            continue;
        y = vid.height - (72 * vid.scale) - (k * 18 * vid.scale);

        // draw background

        f = s->points;
        Draw_StretchPic(x, y, sb_moneyback, 64 * vid.scale, 16 * vid.scale);
        xplus = getTextWidth(va("%i", f), vid.scale);
        HUD_PlayerColor(k, &r, &g, &b);
        HUD_DrawTextBackdrop((((64 * vid.scale) - xplus) / 2) + x, y + (3 * vid.scale),
          va("%i", f), r, g, b, 255, vid.scale);

        if (hud_last_points_valid[k] && hud_last_points[k] != f)
            HUD_Parse_Point_Change(abs(f - hud_last_points[k]), f < hud_last_points[k], k, 0);
        hud_last_points[k]       = f;
        hud_last_points_valid[k] = true;
    }
} /* HUD_Points */

/*
 * ==================
 * HUD_Point_Change
 *
 * ==================
 */
void
HUD_Point_Change(void)
{
    int i;
    double now = Sys_FloatTime();

    for (i = 0; i < MAX_POINT_ELEMENTS; i++) {
        point_change_t * change = &point_change[i];
        float elapsed, progress, alpha;
        int base_x, base_y, x, y;

        if (!change->occupied)
            continue;
        elapsed = (float) (now - change->start_time);
        if (elapsed >= 0.5f) {
            change->occupied = false;
            continue;
        }
        progress = elapsed / 0.5f;
        alpha    = elapsed < 0.25f ? 1.0f : 1.0f - ((elapsed - 0.25f) / 0.25f);
        base_x   = 70 * vid.scale + HUD_UltrawideOffset();
        base_y   = vid.height - (69 * vid.scale) - change->player * (18 * vid.scale);
        x        = base_x + (int) (change->travel_x * progress);
        y        = base_y + (int) (change->travel_y * progress);
        if (change->difference < 0)
            Draw_ColoredString(x, y, va("%i", change->difference), 255, 0, 0, alpha * 255, vid.scale);
        else
            Draw_ColoredString(x, y, va("+%i", change->difference), 255, 255, 0, alpha * 255, vid.scale);
    }
}

/*
 * ==================
 * HUD_Blood
 *
 * ==================
 */
void
HUD_Blood(void)
{
    float alpha;

    // blubswillrule:
    // this function scales linearly from health = 0 to health = 100
    // alpha = (100.0 - (float)cl.stats[STAT_HEALTH])/100*255;
    // but we want the picture to be fully visible at health = 20, so use this function instead
    alpha = (100.0f - ((1.25f * cl.stats[STAT_HEALTH]) - 25)) / 100 * 255;

    if (alpha <= 0.0f)
        return;

    #ifdef PSP_VFPU
    float modifier = (vfpu_sinf(cl.time * 10) * 20) - 20;// always negative
    #else
    float modifier = (sin(cl.time * 10) * 20) - 20;// always negative
    #endif // PSP_VFPU

    if (modifier < -35.0f)
        modifier = -35.0f;

    alpha += modifier;

    if (alpha < 0.0f)
        return;

    float color = 255.0f + modifier;

    Draw_ColoredStretchPic(0, 0, fx_blood_lu, vid.width, vid.height, color, color, color, alpha);
}

/*
 * ===============
 * HUD_GetWorldText
 * ===============
 */

void
HUD_GameModeText(int alpha)
{
    char * mode_title;
    char * subtitle;

    switch (current_gamemode) {
        case GAMEMODE_HARDCORE:
            mode_title = "HARDCORE";
            subtitle   = "";
            break;
        case GAMEMODE_GUNGAME:
            mode_title = "GUN GAME";
            subtitle   = "Cycle all Weapons to WIN!";
            break;
        case GAMEMODE_STICKSNSTONES:
            mode_title = "STICKS & STONES";
            subtitle   = "Ballistic Knife FTW!";
            break;
        case GAMEMODE_WILDWEST:
            mode_title = "WILD WEST";
            subtitle   = "It's a stand-off!";
            break;
        default:
            mode_title = "";
            subtitle   = "";
            break;
    }

    has_chaptertitle = true;
    Draw_ColoredString(6 * vid.scale, vid.height / 2.0f + (10 * vid.scale), mode_title, 255, 255, 255, alpha,
      vid.scale);
    Draw_ColoredString(6 * vid.scale, vid.height / 2.0f + (20 * vid.scale), subtitle, 255, 255, 255, alpha, vid.scale);
}

// modified from scatter's worldspawn parser
// FIXME - unoptimized, could probably save a bit of
// memory here in the future.
void
HUD_WorldText(int alpha)
{
    if (current_gamemode != 0) {
        HUD_GameModeText(alpha);
        return;
    }

    // for parser
    char key[128], value[4096];
    char * data;

    // first, parse worldspawn
    data = COM_Parse(cl.worldmodel->entities);

    if (!data)
        return;  // err

    if (com_token[0] != '{')
        return;  // err

    while (1) {
        data = COM_Parse(data);

        if (!data)
            return;  // err

        if (com_token[0] == '}')
            break;  // end of worldspawn

        if (com_token[0] == '_')
            strcpy(key, com_token + 1);
        else
            strcpy(key, com_token);

        while (key[strlen(key) - 1] == ' ') // remove trailing spaces
            key[strlen(key) - 1] = 0;

        data = COM_Parse(data);
        if (!data)
            return;  // err

        strcpy(value, com_token);

        if (!strcmp("chaptertitle", key)) { // search for chaptertitle key
            has_chaptertitle = true;
            Draw_ColoredString(6 * vid.scale, vid.height / 2.0f + (10 * vid.scale), value, 255, 255, 255, alpha,
              vid.scale);
        }
        if (!strcmp("location", key)) { // search for location key
            Draw_ColoredString(6 * vid.scale, vid.height / 2.0f + (20 * vid.scale), value, 255, 255, 255, alpha,
              vid.scale);
        }
        if (!strcmp("date", key)) { // search for date key
            Draw_ColoredString(6 * vid.scale, vid.height / 2.0f + (30 * vid.scale), value, 255, 255, 255, alpha,
              vid.scale);
        }
        if (!strcmp("person", key)) { // search for person key
            Draw_ColoredString(6 * vid.scale, vid.height / 2.0f + (40 * vid.scale), value, 255, 255, 255, alpha,
              vid.scale);
        }
    }
} /* HUD_WorldText */

/*
 * ===============
 * HUD_MaxAmmo
 * ===============
 */
static int hud_toast_powerup = 4;

void
HUD_PowerupToast(int powerup)
{
    hud_toast_powerup     = powerup;
    hud_maxammo_starttime = sv.time;
    hud_maxammo_endtime   = sv.time + 2;
}

static const char *
HUD_PowerupToastText(int powerup)
{
    switch (powerup) {
        case 0: return "Ka-Boom!";

        case 1: return "Insta-kill!";

        case 2: return "Double Points!";

        case 3: return "Carpenter!";

        case 4: return "Max Ammo!";

        case 5: return "Random Perk!";

        case 6: return "Weapon Upgrade!";

        case 7: return "Bonus Points!";

        default: return "";
    }
}

void
HUD_MaxAmmo(void)
{
    const char * maxammo_string = HUD_PowerupToastText(hud_toast_powerup);

    int start_y = 55 * vid.scale;
    int end_y   = 45 * vid.scale;
    int diff_y  = end_y - start_y;

    float text_alpha = 1.0f;

    int pos_y;

    double start_time, end_time;

    // For the first 0.5s, stay still while we fade in
    if (hud_maxammo_endtime > sv.time + 1.5) {
        start_time = hud_maxammo_starttime;
        end_time   = hud_maxammo_starttime + 0.5;

        text_alpha = (sv.time - start_time) / (end_time - start_time);
        pos_y      = start_y;
    }
    // For the remaining 1.5s, fade out while we fly upwards.
    else {
        start_time = hud_maxammo_starttime + 0.5;
        end_time   = hud_maxammo_endtime;

        float percent_time = (sv.time - start_time) / (end_time - start_time);

        pos_y      = start_y + diff_y * percent_time;
        text_alpha = 1 - percent_time;
    }

    HUD_DrawTextBackdrop((vid.width - getTextWidth((char *) maxammo_string, vid.scale)) / 2,
      pos_y, maxammo_string, 255, 255, 255, (int) (255 * text_alpha), vid.scale);
} /* HUD_MaxAmmo */

/*******************
*    HUD_Rounds    *
*******************/

static void
HUD_DrawRoundCounter(int round, const vec3_t color, int alpha)
{
    int i, x = 5 * vid.scale + HUD_UltrawideOffset();
    int y = vid.height - 48 * vid.scale - 4;

    if (round <= 0) return;

    if (round <= 10) {
        int groups    = round / 5;
        int remainder = round % 5;
        for (i = 0; i < groups; i++) {
            int group_x = x;
            int mark;
            for (mark = 0; mark < 4; mark++) {
                Draw_ColoredStretchPic(x, y, sb_round[mark], 11 * vid.scale, 48 * vid.scale,
                  color[0], color[1], color[2], alpha);
                x += 11 * vid.scale + 3;
            }
            Draw_ColoredStretchPic(group_x, y, sb_round[4], 60 * vid.scale, 48 * vid.scale,
              color[0], color[1], color[2], alpha);
            x = group_x + 60 * vid.scale + 3;
        }
        for (i = 0; i < remainder; i++) {
            Draw_ColoredStretchPic(x, y, sb_round[i], 11 * vid.scale, 48 * vid.scale,
              color[0], color[1], color[2], alpha);
            x += 11 * vid.scale + 3;
        }
        return;
    }
    {
        char digits[12];
        snprintf(digits, sizeof(digits), "%i", round);
        for (i = 0; digits[i]; i++) {
            int digit = digits[i] - '0';
            Draw_ColoredStretchPic(x, y, sb_round_num[digit], 32 * vid.scale, 48 * vid.scale,
              color[0], color[1], color[2], alpha);
            x += 24 * vid.scale;
        }
    }
} /* HUD_DrawRoundCounter */

static void
HUD_DrawRoundIntro(void)
{
    static int last_state;
    static float rcolor;
    static float ralpha;
    static float localpha;
    float frame_time = (float) host_frametime;
    int state        = cl.stats[STAT_ROUNDCHANGE];
    int title_alpha;

    if (state != last_state) {
        last_state = state;
        if (state == 1) {
            rcolor   = 1;
            ralpha   = 1;
            localpha = 0;
        }
    }
    if (state != 1 && state != 2)
        return;

    title_alpha = (int) (ralpha * 255);
    Draw_ColoredStringCentered(85 * vid.scale, "Round", 255,
      (int) (rcolor * 255), (int) (rcolor * 255), title_alpha, 2.0f * vid.scale);

    rcolor -= frame_time / 2.5f;
    if (rcolor < 0) {
        rcolor  = 0;
        ralpha -= frame_time / 2.5f;
        if (ralpha > 0) {
            localpha += frame_time * 0.4f;
            if (localpha > 1) localpha = 1;
        } else {
            ralpha    = 0;
            localpha -= frame_time * 0.4f;
            if (localpha < 0) localpha = 0;
        }
    }

    HUD_WorldText((int) (localpha * 255));
    if (!has_chaptertitle)
        Draw_ColoredString(6 * vid.scale + HUD_UltrawideOffset(), vid.height / 2 + 10 * vid.scale,
          "'Nazi Zombies'", 255, 255, 255, (int) (localpha * 255), vid.scale);
} /* HUD_DrawRoundIntro */

void
HUD_Rounds(void)
{
    static int last_state = -1;
    static float center_alpha;
    static float blinking;
    static double endroundchange;
    static vec3_t shifted_color;
    int state = cl.stats[STAT_ROUNDCHANGE];
    vec3_t color;
    int alpha = 255;
    int i;
    float frame_time       = (float) host_frametime;
    qboolean state_changed = state != last_state;

    if (state_changed) {
        last_state = state;
        if (state == 1) {
            center_alpha   = 0;
            round_center_x = vid.width / 2 - 6 * vid.scale;
            round_center_y = 102 * vid.scale;
        } else if (state == 3) {
            VectorCopy(round_color_target, shifted_color);
        } else if (state == 4) {
            endroundchange = 0;
            blinking       = 0;
        } else if (state == 7) {
            VectorSet(shifted_color, 255, 255, 255);
        }
    }

    HUD_DrawRoundIntro();
    VectorCopy(round_color_target, color);

    switch (state) {
        case 1: // this is the rounds icon at the middle of the screen
            center_alpha += frame_time * 500;
            if (center_alpha > 255) center_alpha = 255;
            Draw_ColoredStretchPic(round_center_x, round_center_y, sb_round[0], 11 * vid.scale,
              48 * vid.scale, color[0], color[1], color[2], (int) center_alpha);
            return;

        case 2: // this is the rounds icon moving from middle
            round_center_x -= (((229.0f / 108.0f) * 2 - 0.2f)
              * ((vid.width - HUD_UltrawideOffset()) / (480.0f * vid.scale)) / 8)
              * (frame_time * 250) * vid.scale;
            round_center_y += ((2 * (vid.height / (272.0f * vid.scale))) / 8)
              * (frame_time * 250) * vid.scale;
            if (round_center_x < 3 * vid.scale + HUD_UltrawideOffset())
                round_center_x = 3 * vid.scale + HUD_UltrawideOffset();
            if (round_center_y > vid.height - 1 - 48 * vid.scale)
                round_center_y = vid.height - 1 - 48 * vid.scale;
            Draw_ColoredStretchPic(round_center_x, round_center_y, sb_round[0], 11 * vid.scale,
              48 * vid.scale, color[0], color[1], color[2], 255);
            return;

        case 3: // shift to white
            for (i = 0; i < 3; i++) {
                shifted_color[i] += (255 - shifted_color[i]) * frame_time * (100.0f / 60.0f);
                if (shifted_color[i] > 255) shifted_color[i] = 255;
            }
            VectorCopy(shifted_color, color);
            break;
        case 4: // blink white
            VectorSet(color, 255, 255, 255);
            if (endroundchange > cl.time) {
                alpha = ((int) (realtime * 475) & 510) - 255;
                if (alpha < 0) alpha = -alpha;
                blinking = alpha;
            } else {
                if (blinking > 0) blinking -= 1;
                if (blinking < 0) blinking = 0;
                alpha = (int) blinking;
            }
            if (!endroundchange) {
                endroundchange = cl.time + 7.5;
                blinking       = 0;
                alpha = 0;
            }
            break;
        case 5: // blink white
            VectorSet(color, 255, 255, 255);
            if (blinking > 0)
                blinking -= 10;
            if (blinking < 0) blinking = 0;
            alpha = (int) blinking;
            break;
        case 6: // blink white while fading back
            VectorSet(color, 255, 255, 255);
            if (endroundchange) {
                endroundchange = 0;
                blinking       = 0;
            }
            blinking += (int) (host_frametime * 475);
            if (blinking > 255) blinking = 255;
            alpha = (int) blinking;
            break;
        case 7: // blink white while fading back
            for (i = 0; i < 3; i++) {
                shifted_color[i] -= (shifted_color[i] - round_color_target[i])
                  * frame_time * 1.5f;
                if (shifted_color[i] < round_color_target[i])
                    shifted_color[i] = round_color_target[i];
            }
            VectorCopy(shifted_color, color);
            break;
        default:
            break;
    }
    HUD_DrawRoundCounter(cl.stats[STAT_ROUNDS], color, alpha);
} /* HUD_Rounds */

/*
 * ===============
 * HUD_Perks
 * ===============
 */
#define     P_JUG    1
#define     P_DOUBLE 2
#define     P_SPEED  4
#define     P_REVIVE 8
#define     P_FLOP   16
#define     P_STAMIN 32
#define     P_DEAD   64
#define     P_MULE   128

void
HUD_DrawPerksDefault(void)
{
    int x, y, scale;

    x     = 18 * vid.scale;
    y     = 2 * vid.scale;
    scale = 22 * vid.scale;

    // Double-Tap 2.0 specialty icon
    int double_tap_icon;
    if (doubletap_has_damage_buff)
        double_tap_icon = doublepic2;
    else
        double_tap_icon = doublepic;

    // Draw second column first -- these need to be
    // overlayed below the first column.
    for (int i = 4; i < 8; i++) {
        if (perk_order[i]) {
            if (perk_order[i] == P_JUG) { Draw_StretchPic(x, y, jugpic, scale, scale); }
            if (perk_order[i] == P_DOUBLE) { Draw_StretchPic(x, y, double_tap_icon, scale, scale); }
            if (perk_order[i] == P_SPEED) { Draw_StretchPic(x, y, speedpic, scale, scale); }
            if (perk_order[i] == P_REVIVE) { Draw_StretchPic(x, y, revivepic, scale, scale); }
            if (perk_order[i] == P_FLOP) { Draw_StretchPic(x, y, floppic, scale, scale); }
            if (perk_order[i] == P_STAMIN) { Draw_StretchPic(x, y, staminpic, scale, scale); }
            if (perk_order[i] == P_DEAD) { Draw_StretchPic(x, y, deadpic, scale, scale); }
            if (perk_order[i] == P_MULE) { Draw_StretchPic(x, y, mulepic, scale, scale); }
        }
        y += scale;
    }

    x = 6 * vid.scale;
    y = 2 * vid.scale;

    // Now the first column.
    for (int i = 0; i < 4; i++) {
        if (perk_order[i]) {
            if (perk_order[i] == P_JUG) { Draw_StretchPic(x, y, jugpic, scale, scale); }
            if (perk_order[i] == P_DOUBLE) { Draw_StretchPic(x, y, double_tap_icon, scale, scale); }
            if (perk_order[i] == P_SPEED) { Draw_StretchPic(x, y, speedpic, scale, scale); }
            if (perk_order[i] == P_REVIVE) { Draw_StretchPic(x, y, revivepic, scale, scale); }
            if (perk_order[i] == P_FLOP) { Draw_StretchPic(x, y, floppic, scale, scale); }
            if (perk_order[i] == P_STAMIN) { Draw_StretchPic(x, y, staminpic, scale, scale); }
            if (perk_order[i] == P_DEAD) { Draw_StretchPic(x, y, deadpic, scale, scale); }
            if (perk_order[i] == P_MULE) { Draw_StretchPic(x, y, mulepic, scale, scale); }
        }
        y += scale;
    }
} /* HUD_DrawPerksDefault */

void
HUD_DrawPerksCenter(void)
{
    int scale;
    int gap;
    int x, y;

    scale = 14 * vid.scale;
    gap   = 2 * vid.scale;

    // Double-Tap 2.0 specialty icon
    int double_tap_icon;
    if (doubletap_has_damage_buff)
        double_tap_icon = doublepic2;
    else
        double_tap_icon = doublepic;

    y = vid.height - scale - (2 * vid.scale);

    int num_perks = 0;
    for (int i = 0; i < 8; i++) {
        if (perk_order[i] != 0)
            num_perks++;
    }

    int total_width = (num_perks * (scale + gap)) - gap;
    x = (vid.width / 2) - (total_width / 2);

    for (int i = 0; i < 8; i++) {
        if (!perk_order[i])
            continue;

        if (perk_order[i] == P_JUG) { Draw_StretchPic(x, y, jugpic, scale, scale); }
        if (perk_order[i] == P_DOUBLE) { Draw_StretchPic(x, y, double_tap_icon, scale, scale); }
        if (perk_order[i] == P_SPEED) { Draw_StretchPic(x, y, speedpic, scale, scale); }
        if (perk_order[i] == P_REVIVE) { Draw_StretchPic(x, y, revivepic, scale, scale); }
        if (perk_order[i] == P_FLOP) { Draw_StretchPic(x, y, floppic, scale, scale); }
        if (perk_order[i] == P_STAMIN) { Draw_StretchPic(x, y, staminpic, scale, scale); }
        if (perk_order[i] == P_DEAD) { Draw_StretchPic(x, y, deadpic, scale, scale); }
        if (perk_order[i] == P_MULE) { Draw_StretchPic(x, y, mulepic, scale, scale); }
        x += scale + gap;
    }
} /* HUD_DrawPerksCenter */

void
HUD_Perks(void)
{
    switch (perk_orientation) {
        case HUD_PERK_ORI_CW:
            HUD_DrawPerksCenter();
            break;
        default:
            HUD_DrawPerksDefault();
            break;
    }
}

/*
 * ===============
 * HUD_Powerups
 * ===============
 */
void
HUD_Powerups(void)
{
    int count = 0;
    int y     = vid.height - (29 * vid.scale);
    float scale;

    scale = 26 * vid.scale;

    if (perk_orientation == HUD_PERK_ORI_CW)
        y -= (10 * vid.scale);

    if (cl.stats[STAT_X2])
        count++;

    if (cl.stats[STAT_INSTA])
        count++;

    // both are avail draw fixed order
    if (count == 2) {
        Draw_StretchPic((vid.width / 2.0f) - (27 * vid.scale), y, x2pic, scale, scale);
        Draw_StretchPic((vid.width / 2.0f) + (3 * vid.scale), y, instapic, scale, scale);
    } else {
        if (cl.stats[STAT_X2])
            Draw_StretchPic((vid.width / 2.0f) - (13 * vid.scale), y, x2pic, scale, scale);
        if (cl.stats[STAT_INSTA])
            Draw_StretchPic((vid.width / 2.0f) - (13 * vid.scale), y, instapic, scale, scale);
    }
}

/*
 * ===============
 * HUD_ProgressBar
 * ===============
 */
void
HUD_ProgressBar(void)
{
    float progressbar;

    if (cl.progress_bar) {
        progressbar = 100 - ((cl.progress_bar - sv.time) * 10);
        if (progressbar >= 100)
            progressbar = 100;
        Draw_FillByColor((vid.width) / 2 - 51, vid.height * 0.75 - 1, 102, 5, 0, 0, 0, 100);
        Draw_FillByColor((vid.width) / 2 - 50, vid.height * 0.75, progressbar, 3, 255, 255, 255, 100);

        Draw_String((vid.width - (88)) / 2, vid.height * 0.75 + 10, "Reviving...");
    }
}

/*******************
*    HUD_Ammo      *
*******************/

//
// HUD_GetAlphaForWeaponObjects()
// Used for objects on the ammo-corner, where
// they linger for X seconds and then fade after.
//
static int
HUD_GetAlphaForWeaponObjects(void)
{
    double remaining = HUD_Change_time - Sys_FloatTime();

    if (remaining <= 0)
        return 0;

    if (remaining > 1)
        return 255;

    return (int) (remaining * 255);
}

int
GetLowAmmo(int weapon, int type)
{
    switch (weapon) {
        case W_COLT: if (type) return 2; else return 16;

        case W_KAR: if (type) return 1; else return 10;

        case W_KAR_SCOPE: if (type) return 1; else return 10;

        case W_M1A1: if (type) return 4; else return 24;

        case W_SAWNOFF: if (type) return 1; else return 12;

        case W_DB: if (type) return 1; else return 12;

        case W_THOMPSON: if (type) return 6; else return 40;

        case W_BAR: if (type) return 6; else return 28;

        default: return 0;
    }
}

int
IsDualWeapon(int weapon)
{
    switch (weapon) {
        case W_BIATCH:
        case W_SNUFF:
            return 1;

        default:
            return 0;
    }

    return 0;
}

void
HUD_Ammo(void)
{
    char mag[32], reserve[32], full[64];
    int x, y, alpha, mag_r, reserve_r;

    alpha = HUD_GetAlphaForWeaponObjects();
    if (!alpha) return;

    y         = vid.height - 25 * vid.scale;
    mag_r     = GetLowAmmo(cl.stats[STAT_ACTIVEWEAPON], 1) >= cl.stats[STAT_CURRENTMAG] ? 215 : 255;
    reserve_r = GetLowAmmo(cl.stats[STAT_ACTIVEWEAPON], 0) >= cl.stats[STAT_AMMO] ? 215 : 255;
    if (IsDualWeapon(cl.stats[STAT_ACTIVEWEAPON])) {
        snprintf(full, sizeof(full), "%i %i/%i", cl.stats[STAT_CURRENTMAG2], cl.stats[STAT_CURRENTMAG],
          cl.stats[STAT_AMMO]);
        x = vid.width - HUD_UltrawideOffset() - 55 * vid.scale - getTextWidth(full, vid.scale);
        HUD_DrawTextBackdrop(x, y, full, 255, 255, 255, alpha, vid.scale);
        return;
    }
    snprintf(mag, sizeof(mag), "%i", cl.stats[STAT_CURRENTMAG]);
    snprintf(reserve, sizeof(reserve), "/%i", cl.stats[STAT_AMMO]);
    snprintf(full, sizeof(full), "%s%s", mag, reserve);
    x = vid.width - HUD_UltrawideOffset() - 55 * vid.scale - getTextWidth(full, vid.scale);
    HUD_DrawTextBackdrop(x, y, mag, mag_r, mag_r == 215 ? 0 : 255, mag_r == 215 ? 0 : 255, alpha, vid.scale);
    HUD_DrawTextBackdrop(x + getTextWidth(mag, vid.scale), y, reserve,
      reserve_r, reserve_r == 215 ? 0 : 255, reserve_r == 215 ? 0 : 255, alpha, vid.scale);
}

//
// HUD_AmmoString()
// Draws the "LOW AMMO", "Reload", etc. text
//

void
HUD_AmmoString(void)
{
    static float pulse = 1;
    static int pulse_down;
    const char * message = NULL;
    int r = 255, g = 255, b = 255;

    if (GetLowAmmo(cl.stats[STAT_ACTIVEWEAPON], 1) >= cl.stats[STAT_CURRENTMAG]) {
        if (cl.stats[STAT_CURRENTMAG] <= 0 && cl.stats[STAT_AMMO] <= 0) {
            message = "NO AMMO";
            r       = 215;
            g       = b = 0;
        } else if (cl.stats[STAT_AMMO] <= 0) {
            message = "LOW AMMO";
            r       = 219;
            g       = 203;
            b       = 19;
        } else {
            message = "Reload";
        }
    }
    if (!message) { pulse = 1; pulse_down = 1; return; }
    // Blink the text and draw it.
    pulse += (pulse_down ? -1 : 1) * (float) host_frametime;
    if (pulse <= 0.5f) { pulse = 0.5f; pulse_down = 0; }
    if (pulse >= 1) { pulse = 1; pulse_down = 1; }
    HUD_DrawTextBackdrop((vid.width - getTextWidth((char *) message, vid.scale)) / 2,
      vid.height / 2 + 40 * vid.scale, message, r, g, b, pulse * 255, vid.scale);
}

/*******************
*    HUD_Grenades  *
*******************/
#define     UI_FRAG  1
#define     UI_BETTY 2

void
HUD_Grenades(void)
{
    int alpha = HUD_GetAlphaForWeaponObjects();

    if (!alpha)
        return;

    Draw_ColoredStretchPic(vid.width - HUD_UltrawideOffset() - (53 * vid.scale), vid.height - (40 * vid.scale), fragpic,
      22 * vid.scale, 22 * vid.scale, 255, 255, 255, alpha);

    if (cl.stats[STAT_GRENADES] & UI_FRAG) {
        if (cl.stats[STAT_PRIGRENADES] <= 0)
            Draw_ColoredString(vid.width - HUD_UltrawideOffset() - (40 * vid.scale), vid.height - (25 * vid.scale),
              va("%i", cl.stats[STAT_PRIGRENADES]), 255, 0, 0, alpha, vid.scale);
        else
            Draw_ColoredString(vid.width - HUD_UltrawideOffset() - (40 * vid.scale), vid.height - (25 * vid.scale),
              va("%i", cl.stats[STAT_PRIGRENADES]), 255, 255, 255, alpha, vid.scale);
    }

    if (cl.stats[STAT_GRENADES] & UI_BETTY) {
        Draw_ColoredStretchPic(vid.width - HUD_UltrawideOffset() - (32 * vid.scale), vid.height - (40 * vid.scale),
          bettypic,
          22 * vid.scale, 22 * vid.scale, 255, 255, 255, alpha);
        if (cl.stats[STAT_PRIGRENADES] <= 0)
            Draw_ColoredString(vid.width - HUD_UltrawideOffset() - (17 * vid.scale), vid.height - (25 * vid.scale),
              va("%i", cl.stats[STAT_SECGRENADES]), 255, 0, 0, alpha, vid.scale);
        else
            Draw_ColoredString(vid.width - HUD_UltrawideOffset() - (17 * vid.scale), vid.height - (25 * vid.scale),
              va("%i", cl.stats[STAT_SECGRENADES]), 255, 255, 255, alpha, vid.scale);
    }
}

/*******************
*    HUD Weapons   *
*******************/
void
HUD_Weapon(void)
{
    static char last_weapon_name[32];
    static double weapon_name_time;
    char str[32];
    double remaining;
    int alpha;
    int x;
    int y = vid.height - (40 * vid.scale);

    strcpy(str, PR_GetString(sv_player->v.Weapon_Name));
    if (strcmp(str, last_weapon_name)) {
        Q_strncpyz(last_weapon_name, str, sizeof(last_weapon_name));
        weapon_name_time = Sys_FloatTime() + 3;
    }
    remaining = weapon_name_time - Sys_FloatTime();
    if (remaining <= 0)
        return;

    alpha = remaining > 2 ? 255 : (int) (remaining * 255 / 2);

    x = (vid.width - HUD_UltrawideOffset() - (55 * vid.scale)) - getTextWidth(str, vid.scale);
    HUD_DrawTextBackdrop(x, y, str, 255, 255, 255, alpha, vid.scale);
}

/*
 * ===============
 * HUD_BettyPrompt
 * ===============
 */
void
HUD_BettyPrompt(void)
{
    char str[64];
    char str2[32];
    int x;

    Q_strncpyz(str, va("Double-tap  %s  then press  %s", GetUseButtonL(), GetGrenadeButtonL()), sizeof(str));
    Q_strncpyz(str2, "to place a Bouncing Betty", sizeof(str2));
    x = (vid.width - getTextWidth(str, vid.scale)) / 2;

    Draw_ColoredStringCentered(60 * vid.scale, str, 255, 255, 255, 255, vid.scale);
    Draw_ColoredStringCentered(72 * vid.scale, str2, 255, 255, 255, 255, vid.scale);
    Draw_Pic(x + getTextWidth("Double-tap  ", vid.scale) - 4 * vid.scale,
      60 * vid.scale, GetButtonIcon("+use"));
    Draw_Pic(x + getTextWidth("Double-tap     then press   ", vid.scale) - 4 * vid.scale,
      60 * vid.scale, GetButtonIcon("+grenade"));
}

/*
 * ===============
 * HUD_PlayerName
 * ===============
 */
void
HUD_PlayerName(void)
{
    int alpha = 255;

    if (nameprint_time - sv.time < 1)
        alpha = (int) ((nameprint_time - sv.time) * 255);

    Draw_ColoredString(70 * vid.scale, vid.height - (70 * vid.scale), player_name, 255, 255, 255, alpha, vid.scale);
}

/*
 * ===============
 * HUD_Screenflash
 * ===============
 */

//
// invert float takes in float value between 0 and 1, inverts position
// eg: 0.1 returns 0.9, 0.34 returns 0.66
float
invertfloat(float input)
{
    if (input < 0)
        return 0;  // adjust to lower boundary
    else if (input > 1)
        return 1;  // adjust to upper boundary
    else
        return (1 - input);
}

void
HUD_Screenflash(void)
{
    // Screenflash can get in the way of debugging.
    if (sys_testmode.value > 0) {
        return;
    }

    int r, g, b, a;
    float flash_alpha;

    double percentage_complete = screenflash_worktime / (screenflash_duration - screenflash_starttime);

    // Fade Out
    if (screenflash_type == SCREENFLASH_FADE_OUT) {
        flash_alpha = invertfloat((float) percentage_complete);
    }
    // Fade In
    else if (screenflash_type == SCREENFLASH_FADE_IN) {
        flash_alpha = (float) percentage_complete;
    }
    // Fade In + Fade Out
    else {
        // Fade In
        if (percentage_complete < 0.5) {
            flash_alpha = (float) percentage_complete * 2;
        }
        // Fade Out
        else {
            flash_alpha = invertfloat((float) percentage_complete) * 2;
        }
    }

    // Obtain the flash color
    switch (screenflash_color) {
        case SCREENFLASH_COLOR_BLACK: r = 0;
            g = 0;
            b = 0;
            a = (int) (flash_alpha * 255);
            break;
        case SCREENFLASH_COLOR_WHITE: r = 255;
            g = 255;
            b = 255;
            a = (int) (flash_alpha * 255);
            break;
        default: r = 255;
            g      = 0;
            b      = 0;
            a      = 255;
            break;
    }

    screenflash_worktime += host_frametime;
    Draw_FillByColor(0, 0, vid.width, vid.height, r, g, b, a);
} /* HUD_Screenflash */

/*******************
*   HUD Crosshair  *
*******************/

void
HUD_Hitmark(int type)
{
    if (type == HITMARK_DEATH) {
        hud_hitmarker_type        = HITMARK_DEATH;
        hud_hitmarker_time        = sv.time + 0.2;
        hud_hitmarker_ignore_time = sv.time + 0.2;
    } else if (hud_hitmarker_ignore_time <= sv.time) {
        hud_hitmarker_type = HITMARK_NORMAL;
        hud_hitmarker_time = sv.time + 0.3;
    }
}

static void
HUD_DrawHitmark(void)
{
    float remaining;
    float alpha;
    int color;
    int size;

    if (!cl_hitmarkers.value || hud_hitmarker_time <= sv.time)
        return;

    remaining = (float) (hud_hitmarker_time - sv.time);
    if (hud_hitmarker_type == HITMARK_DEATH) {
        alpha = remaining * 5.0f;
        color = 191;
    } else {
        alpha = remaining * 3.3f;
        color = 255;
    }
    if (alpha > 1)
        alpha = 1;

    size = 16 * vid.scale;
    Draw_ColoredStretchPic((vid.width - size) / 2, (vid.height - size) / 2,
      hud_hitmarker, size, size, color, hud_hitmarker_type == HITMARK_DEATH ? 0 : color,
      hud_hitmarker_type == HITMARK_DEATH ? 0 : color, (int) (alpha * 255));
}

static qboolean
HUD_DrawSniperScope(void)
{
    int side_x;

    if (cl.stats[STAT_ZOOM] != 2)
        return false;

    side_x = (vid.width - vid.height) / 2;
    if (side_x > 0) {
        Draw_FillByColor(0, 0, side_x, vid.height, 0, 0, 0, 255);
        Draw_FillByColor(side_x + vid.height, 0, side_x, vid.height, 0, 0, 0, 255);
    }
    Draw_StretchPic(side_x, 0, hud_sniper_scope, vid.height, vid.height);
    return true;
}

static int
HUD_CrosshairSpread(int spread)
{
    spread = (int) (spread * 0.68f) + 6;
    if (cl.perks & 64)
        spread = (int) (spread * 0.65f);
    return spread;
}

static int
HUD_CrosshairWeapon(void)
{
    switch (cl.stats[STAT_ACTIVEWEAPON]) {
        case W_COLT:
        case W_BIATCH:
        case W_357:
        case W_KILLU: return HUD_CrosshairSpread(22);

        case W_PTRS:
        case W_PENETRATOR:
        case W_KAR_SCOPE:
        case W_HEADCRACKER:
        case W_KAR:
        case W_ARMAGEDDON:
        case W_SPRING:
        case W_PULVERIZER: return HUD_CrosshairSpread(65);

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
        case W_KOLLIDER: return HUD_CrosshairSpread(10);

        case W_BROWNING:
        case W_ACCELERATOR:
        case W_MG:
        case W_BARRACUDA: return HUD_CrosshairSpread(30);

        case W_SAWNOFF:
        case W_SNUFF: return HUD_CrosshairSpread(50);

        case W_TRENCH:
        case W_GUT:
        case W_DB:
        case W_BORE: return HUD_CrosshairSpread(35);

        case W_GEWEHR:
        case W_COMPRESSOR:
        case W_M1:
        case W_M1000:
        case W_M1A1:
        case W_WIDDER: return HUD_CrosshairSpread(5);

        default: return HUD_CrosshairSpread(0);
    }
} /* HUD_CrosshairWeapon */

static int
HUD_CrosshairMaxSpread(void)
{
    switch (cl.stats[STAT_ACTIVEWEAPON]) {
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
        case W_SAMURAI: return HUD_CrosshairSpread(48);

        case W_PTRS:
        case W_PENETRATOR:
        case W_KAR_SCOPE:
        case W_HEADCRACKER:
        case W_KAR:
        case W_ARMAGEDDON:
        case W_SPRING:
        case W_PULVERIZER: return HUD_CrosshairSpread(75);

        case W_SAWNOFF:
        case W_SNUFF: return HUD_CrosshairSpread(50);

        case W_DB:
        case W_BORE:
        case W_TRENCH:
        case W_GUT:
        case W_GEWEHR:
        case W_COMPRESSOR:
        case W_M1:
        case W_M1000:
        case W_M1A1:
        case W_WIDDER: return HUD_CrosshairSpread(35);

        default: return HUD_CrosshairSpread(0);
    }
} /* HUD_CrosshairMaxSpread */

static void
HUD_Crosshair(void)
{
    static int last_weapon = -1;
    static float opacity   = 255;
    int weapon = cl.stats[STAT_ACTIVEWEAPON];
    int color  = sv_player && sv_player->v.facingenemy ? 0 : 255;
    int spread, maxspread, thickness, length;
    int cx = vid.width / 2;
    int cy = vid.height / 2;
    qboolean moving = croshhairmoving || fabsf(cl.velocity[0]) > 1 || fabsf(cl.velocity[1]) > 1;

    if (HUD_DrawSniperScope())
        return;

    if (cl.stats[STAT_HEALTH] <= 20 || cl.stats[STAT_ZOOM] == 1 || !crosshair.value)
        return;

    // Reset crosshair state when swapping weapons.
    if (weapon != last_weapon) {
        last_weapon = weapon;
        cur_spread  = 0;
        crosshair_offset_step = HUD_CrosshairWeapon();
    }
    if (cl_crosshair_debug.value) {
        Draw_FillByColor(cx, 0, 1, vid.height, 255, 0, 0, 128);
        Draw_FillByColor(0, cy, vid.width, 1, 0, 255, 0, 128);
    }
    if (crosshair_spread_time > sv.time && crosshair_spread_time) {
        cur_spread += 10;
    } else {
        cur_spread -= 4;
        if (cur_spread < 0) cur_spread = 0;
    }
    if (moving) {
        opacity -= (float) host_frametime * 255.0f;
        if (opacity < 128) opacity = 128;
    } else {
        opacity += (float) host_frametime * 255.0f;
        if (opacity > 255) opacity = 255;
    }

    thickness = vid.scale >= 1 ? (int) vid.scale : 1;
    length    = (int) (3 * vid.scale);
    if (length < 3) length = 3;
    // Standard crosshair (+)
    if ((int) crosshair.value == 1 || (int) crosshair.value == 4) {
        if ((int) crosshair.value == 4 && crosshair_pulse_grenade) {
            crosshair_offset_step = 0;
            cur_spread = 2;
        }
        crosshair_pulse_grenade = false;
        maxspread = HUD_CrosshairMaxSpread();
        spread    = ((int) crosshair.value == 4 ? 12 : HUD_CrosshairWeapon()) + (int) cur_spread;
        if (moving && (int) crosshair.value == 1 && spread < maxspread)
            spread += (maxspread - spread) * 0.5f;
        if ((int) crosshair.value == 1 && spread > maxspread) spread = maxspread;
        if (sv_player->v.view_ofs[2] == 8) spread *= 0.80f;
        else if (sv_player->v.view_ofs[2] == -10) spread *= 0.65f;
        spread *= 1.875f;
        crosshair_offset_step += (spread - crosshair_offset_step) * ((int) crosshair.value == 4 ? 0.05f : 0.5f);
        spread = (int) crosshair_offset_step;
        if (cl_crosshairdot.value && (int) crosshair.value == 1) {
            int dot = vid.scale >= 1 ? (int) vid.scale : 1;
            Draw_FillByColor(cx - dot / 2, cy - dot / 2, dot, dot, 255, color, color, opacity);
        }
        Draw_FillByColor(cx - thickness / 2, cy - spread - length, thickness, length, 255, color, color, opacity);
        Draw_FillByColor(cx - thickness / 2, cy + spread, thickness, length, 255, color, color, opacity);
        Draw_FillByColor(cx - spread - length, cy - thickness / 2, length, thickness, 255, color, color, opacity);
        Draw_FillByColor(cx + spread, cy - thickness / 2, length, thickness, 255, color, color, opacity);
    } else if ((int) crosshair.value == 2) {
        Draw_CharacterRGBA(cx - 4 * vid.scale, cy - 4 * vid.scale, 'O', 255, color, color, opacity, vid.scale);
    } else if ((int) crosshair.value == 3) {
        int dot = 4 * vid.scale;
        Draw_FillByColor(cx - dot / 2, cy - dot / 2, dot, dot, 255, color, color, opacity);
    }
} /* HUD_Crosshair */

/*
 * ===============
 * HUD_GunGame
 * ===============
 */
void
HUD_GunGame(void)
{
    char weapon_id[64];
    char point_info[64];

    int client_points = 0;

    for (int i = 0; i < svs.maxclients; i++) {
        if (i == cl.viewentity - 1) {
            client_points = cl.scores[i].points;
            break;
        }
    }

    Draw_FillByColor(60 * vid.scale, 2 * vid.scale, vid.width - (120 * vid.scale), 28 * vid.scale, 0, 0, 0, 100);

    if (cl.stats[STAT_GUNGAME_IDX] >= 100) {
        sprintf(weapon_id, "You've passed all weapons!");
        sprintf(point_info, "The Winner can choose to End the Game");
    } else {
        sprintf(weapon_id, "%s [%d/32]", PR_GetString(sv_player->v.Weapon_Name), cl.stats[STAT_GUNGAME_IDX] + 1);
        sprintf(point_info, "[%d] Score until next Weapon", cl.stats[STAT_GUNGAME_SCOREGOAL] - client_points);
    }


    Draw_ColoredString(vid.width / 2 - getTextWidth(weapon_id, vid.scale) / 2, 6 * vid.scale, weapon_id, 255, 255, 255,
      255, vid.scale);
    Draw_ColoredString(vid.width / 2 - getTextWidth(point_info, vid.scale) / 2, 18 * vid.scale, point_info, 255, 255, 0,
      255, vid.scale);
}

static void
HUD_FormatStopwatch(double seconds, char * buffer, size_t size)
{
    int hours, minutes;

    if (seconds < 0) seconds = 0;
    hours   = (int) (seconds / 3600.0);
    minutes = ((int) seconds / 60) % 60;
    snprintf(buffer, size, "%02d:%02d:%04.1f", hours, minutes, fmod(seconds, 60.0));
}

static void
HUD_Stopwatches(void)
{
    char text[32];
    int x;
    double now = Sys_FloatTime();

    if (hud_stopwatch_round != cl.stats[STAT_ROUNDS]) {
        hud_stopwatch_round  = cl.stats[STAT_ROUNDS];
        hud_round_start_time = now;
    }
    if (scr_serverstopwatch.value < 1)
        return;

    HUD_FormatStopwatch(now - hud_map_start_time, text, sizeof(text));
    x = vid.width - HUD_UltrawideOffset() - getTextWidth(text, vid.scale) - 2 * vid.scale;
    HUD_DrawTextBackdrop(x, 2 * vid.scale, text, 255, 165, 0, 255, vid.scale);
    if (scr_serverstopwatch.value >= 2) {
        HUD_FormatStopwatch(now - hud_round_start_time, text, sizeof(text));
        x = vid.width - HUD_UltrawideOffset() - getTextWidth(text, vid.scale) - 2 * vid.scale;
        HUD_DrawTextBackdrop(x, 13 * vid.scale, text, 255, 255, 255, 255, vid.scale);
    }
}

static void
HUD_PlayerDebugInfo(void)
{
    char text[96];
    int x, y;
    float speed;

    if (scr_playerdebuginfo.value < 1)
        return;

    x     = scr_playerdebuginfo_x.value * vid.scale + HUD_UltrawideOffset();
    y     = scr_playerdebuginfo_y.value * vid.scale;
    speed = sqrtf(cl.velocity[0] * cl.velocity[0] + cl.velocity[1] * cl.velocity[1] + cl.velocity[2] * cl.velocity[2]);
    Draw_FillByColor(x - 4 * vid.scale, y - 4 * vid.scale, 150 * vid.scale,
      (scr_playerdebuginfo.value >= 2 ? 45 : 14) * vid.scale, 0, 0, 0, 191);
    snprintf(text, sizeof(text), "Speed: %.1f qu/s", (double) speed);
    Draw_ColoredString(x, y, text, 255, 255, 255, 255, vid.scale);
    if (scr_playerdebuginfo.value >= 2) {
        snprintf(text, sizeof(text), "Angles: %.1f %.1f %.1f", (double) r_refdef.viewangles[0],
          (double) r_refdef.viewangles[1], (double) r_refdef.viewangles[2]);
        Draw_ColoredString(x, y + 12 * vid.scale, text, 255, 255, 255, 255, vid.scale);
        snprintf(text, sizeof(text), "Origin: %.1f %.1f %.1f", (double) r_refdef.vieworg[0],
          (double) r_refdef.vieworg[1], (double) r_refdef.vieworg[2]);
        Draw_ColoredString(x, y + 24 * vid.scale, text, 255, 255, 255, 255, vid.scale);
    }
}

/*
 * ===============
 * HUD_Draw
 * ===============
 */
void
HUD_Draw(void)
{
    if (scr_con_current == vid.height)
        return;  // console is full screen

    if (key_dest == key_menu_pause) {
        // Make sure we still draw the screen flash.
        if (screenflash_duration > sv.time)
            HUD_Screenflash();
        return;
    }

    scr_copyeverything = 1;
    HUD_DrawCenterPrint();


    if (waypoint_mode.value) {
        HUD_Waypoint();
        HUD_PlayerDebugInfo();
        return;
    }

    if (cl_cinematic.value) {
        if (screenflash_duration > sv.time)
            HUD_Screenflash();
        return;
    }

    // We shouldn't draw anything during the game intro fade.
    if (screenflash_color == SCREENFLASH_COLOR_BLACK && screenflash_duration > sv.time) {
        HUD_Screenflash();
        return;
    }

    if (cl.stats[STAT_HEALTH] <= 0 || showscoreboard == true) {
        HUD_EndScreen();

        // Make sure we still draw the screen flash.
        if (screenflash_duration > sv.time)
            HUD_Screenflash();

        return;
    }

    HUD_DrawHitmark();

    if (sv_gamemode.value == 3) {
        HUD_Blood();
        HUD_Points();
        HUD_Point_Change();

        if (screenflash_duration > sv.time)
            HUD_Screenflash();

        return;
    }

    if (bettyprompt_time > sv.time)
        HUD_BettyPrompt();

    if (nameprint_time > sv.time)
        HUD_PlayerName();

    HUD_Blood();
    HUD_Crosshair();
    if (cl.stats[STAT_ZOOM] == 2) {
        if (screenflash_duration > sv.time)
            HUD_Screenflash();
        return;
    }
    HUD_Rounds();
    HUD_Perks();
    HUD_Powerups();
    HUD_ProgressBar();
    HUD_DrawUsePrint();
    if (sys_testmode.value <= 0 &&
      (HUD_Change_time > Sys_FloatTime() || GetLowAmmo(cl.stats[STAT_ACTIVEWEAPON],
      1) >= cl.stats[STAT_CURRENTMAG] || GetLowAmmo(cl.stats[STAT_ACTIVEWEAPON], 0) >= cl.stats[STAT_AMMO]) &&
      cl.stats[STAT_HEALTH] >= 20)
    { // these elements are only drawn when relevant for few seconds
        HUD_Ammo();
        HUD_Grenades();
        HUD_Weapon();
        HUD_AmmoString();
    }
    HUD_Points();
    HUD_Point_Change();
    if (hud_maxammo_endtime > sv.time)
        HUD_MaxAmmo();

    switch (current_gamemode) {
        case GAMEMODE_GUNGAME: HUD_GunGame();
        default: break;
    }
    HUD_Stopwatches();
    HUD_PlayerDebugInfo();

    // This should always come last!
    if (screenflash_duration > sv.time)
        HUD_Screenflash();
} /* HUD_Draw */
