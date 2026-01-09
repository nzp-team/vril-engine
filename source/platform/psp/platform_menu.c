/*
Copyright (C) 2025 NZ:P Team

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
#include "../../nzportable_def.h"
// needed for Map_Finder()
#include <pspkernel.h>
#include <psputility.h>
#include <pspiofilemgr.h>
#include "../../menu/menu_defs.h"

// =============
// Custom maps
// =============

//==================== Map Find System By Crow_bar =============================

// UGHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH
// fuck windows
char* remove_windows_newlines(char* line)
{
	char *p = strchr(line, '\r');
	while (p) {
		*p = '\0';
		strcat(line, ++p);
		p = strchr(p, '\r');
	}

	return line;
}


void Platform_Menu_MapFinder(void)
{

#ifdef KERNEL_MODE
	SceUID dir = sceIoDopen(va("%s/maps", com_gamedir));
#else
	SceUID dir = sceIoDopen(va("nzp/maps"));
#endif // KERNEL_MODE

	if(dir < 0)
	{
		Sys_Error ("Map_Finder");
		return;
	}

	SceIoDirent dirent;

    memset(&dirent, 0, sizeof(SceIoDirent));

	for (int i = 0; i < 50; i++) {
		custom_maps[i].occupied = false;
	}

	while(sceIoDread(dir, &dirent) > 0)
	{
		if(dirent.d_name[0] == '.')
		{
			continue;
		}

		if(!strcmp(COM_FileExtension(dirent.d_name),"bsp")|| !strcmp(COM_FileExtension(dirent.d_name),"BSP"))
	    {
			char ntype[32];

			COM_StripExtension(dirent.d_name, ntype);
			custom_maps[num_user_maps].occupied = true;
			custom_maps[num_user_maps].map_name = Q_malloc(sizeof(char)*32);
			sprintf(custom_maps[num_user_maps].map_name, "%s", ntype);

			char* 		setting_path;
			int 		setting_file;
			SceIoStat	setting_info;

			setting_path 								  	= Q_malloc(sizeof(char)*64);
			custom_maps[num_user_maps].map_thumbnail_path 	= Q_malloc(sizeof(char)*64);
#ifdef KERNEL_MODE
			strcpy(setting_path, 									va("%s/maps/", com_gamedir));
#else
			strcpy(setting_path, 									va("nzp/maps/"));
#endif // KERNEL_MODE
			strcpy(custom_maps[num_user_maps].map_thumbnail_path, 	"gfx/menu/custom/");
			strcat(setting_path, 									custom_maps[num_user_maps].map_name);
			strcat(custom_maps[num_user_maps].map_thumbnail_path, 	custom_maps[num_user_maps].map_name);
			strcat(setting_path, ".txt");

			sceIoGetstat(setting_path, &setting_info);
			setting_file = sceIoOpen(setting_path, PSP_O_RDONLY, 0);

			if (setting_file >= 0) {

				int state;
				state = 0;
				int value;

				custom_maps[num_user_maps].map_name_pretty = Q_malloc(sizeof(char)*32);
				custom_maps[num_user_maps].map_desc[0] = Q_malloc(sizeof(char)*40);
				custom_maps[num_user_maps].map_desc[1] = Q_malloc(sizeof(char)*40);
				custom_maps[num_user_maps].map_desc[2] = Q_malloc(sizeof(char)*40);
				custom_maps[num_user_maps].map_desc[3] = Q_malloc(sizeof(char)*40);
				custom_maps[num_user_maps].map_desc[4] = Q_malloc(sizeof(char)*40);
				custom_maps[num_user_maps].map_desc[5] = Q_malloc(sizeof(char)*40);
				custom_maps[num_user_maps].map_desc[6] = Q_malloc(sizeof(char)*40);
				custom_maps[num_user_maps].map_desc[7] = Q_malloc(sizeof(char)*40);
				custom_maps[num_user_maps].map_author = Q_malloc(sizeof(char)*40);

				char* buffer = (char*)calloc(setting_info.st_size+1, sizeof(char));
				sceIoRead(setting_file, buffer, setting_info.st_size);

				strtok(buffer, "\n");
				while(buffer != NULL) {
					switch(state) {
						case 0: strcpy(custom_maps[num_user_maps].map_name_pretty, remove_windows_newlines(buffer)); break;
						case 1: strcpy(custom_maps[num_user_maps].map_desc[0], remove_windows_newlines(buffer)); break;
						case 2: strcpy(custom_maps[num_user_maps].map_desc[1], remove_windows_newlines(buffer)); break;
						case 3: strcpy(custom_maps[num_user_maps].map_desc[2], remove_windows_newlines(buffer)); break;
						case 4: strcpy(custom_maps[num_user_maps].map_desc[3], remove_windows_newlines(buffer)); break;
						case 5: strcpy(custom_maps[num_user_maps].map_desc[4], remove_windows_newlines(buffer)); break;
						case 6: strcpy(custom_maps[num_user_maps].map_desc[5], remove_windows_newlines(buffer)); break;
						case 7: strcpy(custom_maps[num_user_maps].map_desc[6], remove_windows_newlines(buffer)); break;
						case 8: strcpy(custom_maps[num_user_maps].map_desc[7], remove_windows_newlines(buffer)); break;
						case 9: strcpy(custom_maps[num_user_maps].map_author, remove_windows_newlines(buffer)); break;
						case 10: value = 0; sscanf(remove_windows_newlines(buffer), "%d", &value); custom_maps[num_user_maps].map_use_thumbnail = value; break;
						case 11: value = 0; sscanf(remove_windows_newlines(buffer), "%d", &value); custom_maps[num_user_maps].map_allow_game_settings = value; break;
						default: break;
					}
					state++;
					buffer = strtok(NULL, "\n");
				}
				free(buffer);
				buffer = 0;
				sceIoClose(setting_file);
			}
			num_user_maps++;
		}
	    memset(&dirent, 0, sizeof(SceIoDirent));
	}
    sceIoDclose(dir);

	custom_map_pages = (int)ceil((double)(num_user_maps + 1)/15);
}

//=============================================================================
/* ACHIEVEMENTS */

int m_achievement_cursor;
int m_achievement_selected;
int m_achievement_scroll[2];
int total_unlocked_achievements;
int total_locked_achievements;
int achievement_locked;

void Achievement_Init (void)
{
	achievement_list[0].img = Image_LoadImage("gfx/achievement/ready", IMAGE_TGA, 0, true, false);
	achievement_list[0].unlocked = 0;
	achievement_list[0].progress = 0;
	strcpy(achievement_list[0].name, "Ready..");
	strcpy(achievement_list[0].description, "Reach round 5");

	achievement_list[1].img = Image_LoadImage("gfx/achievement/steady", IMAGE_TGA, 0, true, false);
	achievement_list[1].unlocked = 0;
	achievement_list[1].progress = 0;
	strcpy(achievement_list[1].name, "Steady..");
	strcpy(achievement_list[1].description, "Reach round 10");

	achievement_list[2].img = Image_LoadImage("gfx/achievement/go_hell_no", IMAGE_TGA, 0, true, false);
	achievement_list[2].unlocked = 0;
	achievement_list[2].progress = 0;
	strcpy(achievement_list[2].name, "Go? Hell No...");
	strcpy(achievement_list[2].description, "Reach round 15");

	achievement_list[3].img = Image_LoadImage("gfx/achievement/where_legs_go", IMAGE_TGA, 0, true, false);
	achievement_list[3].unlocked = 0;
	achievement_list[3].progress = 0;
	strcpy(achievement_list[3].name, "Where Did Legs Go?");
	strcpy(achievement_list[3].description, "Turn a zombie into a crawler");

	achievement_list[4].img = Image_LoadImage("gfx/achievement/the_f_bomb", IMAGE_TGA, 0, true, false);
	achievement_list[4].unlocked = 0;
	achievement_list[4].progress = 0;
	strcpy(achievement_list[4].name, "The F Bomb");
	strcpy(achievement_list[4].description, "Use the Nuke power-up to kill a single Zombie");

	/*achievement_list[3].img = Draw_CachePic("gfx/achievement/beast");
	achievement_list[3].unlocked = 0;
	achievement_list[3].progress = 0;
	strcpy(achievement_list[3].name, "Beast");
	strcpy(achievement_list[3].description, "Survive round after round 5 with knife and grenades only");

	achievement_list[4].img = Draw_CachePic("gfx/achievement/survival");
	achievement_list[4].unlocked = 0;
	achievement_list[4].progress = 0;
	strcpy(achievement_list[4].name, "Survival Expert");
	strcpy(achievement_list[4].description, "Use pistol and knife only to reach round 10");

	achievement_list[5].img = Draw_CachePic("gfx/achievement/boomstick");
	achievement_list[5].unlocked = 0;
	achievement_list[5].progress = 0;
	strcpy(achievement_list[5].name, "Boomstick");
	strcpy(achievement_list[5].description, "3 for 1 with shotgun blast");

	achievement_list[6].img = Draw_CachePic("gfx/achievement/boom_headshots");
	achievement_list[6].unlocked = 0;
	achievement_list[6].progress = 0;
	strcpy(achievement_list[6].name, "Boom Headshots");
	strcpy(achievement_list[6].description, "Get 10 headshots");

	achievement_list[7].img = Draw_CachePic("gfx/achievement/where_did");
	achievement_list[7].unlocked = 0;
	achievement_list[7].progress = 0;
	strcpy(achievement_list[7].name, "Where Did Legs Go?");
	strcpy(achievement_list[7].description, "Make a crawler zombie");

	achievement_list[8].img = Draw_CachePic("gfx/achievement/keep_the_change");
	achievement_list[8].unlocked = 0;
	achievement_list[8].progress = 0;
	strcpy(achievement_list[8].name, "Keep The Change");
	strcpy(achievement_list[8].description, "Purchase everything");

	achievement_list[9].img = Draw_CachePic("gfx/achievement/big_thanks");
	achievement_list[9].unlocked = 0;
	achievement_list[9].progress = 0;
	strcpy(achievement_list[9].name, "Big Thanks To Explosion");
	strcpy(achievement_list[9].description, "Get 10 kills with one grenade");

	achievement_list[10].img = Draw_CachePic("gfx/achievement/warmed-up");
	achievement_list[10].unlocked = 0;
	achievement_list[10].progress = 0;
	strcpy(achievement_list[10].name, "Getting Warmed-Up");
	strcpy(achievement_list[10].description, "Achieve 10 achievements");

	achievement_list[11].img = Draw_CachePic("gfx/achievement/mysterybox_maniac");
	achievement_list[11].unlocked = 0;
	achievement_list[11].progress = 0;
	strcpy(achievement_list[11].name, "Mysterybox Maniac");
	strcpy(achievement_list[11].description, "use mysterybox 20 times");

	achievement_list[12].img = Draw_CachePic("gfx/achievement/instant_help");
	achievement_list[12].unlocked = 0;
	achievement_list[12].progress = 0;
	strcpy(achievement_list[12].name, "Instant Help");
	strcpy(achievement_list[12].description, "Kill 100 zombies with insta-kill");

	achievement_list[13].img = Draw_CachePic("gfx/achievement/blow_the_bank");
	achievement_list[13].unlocked = 0;
	achievement_list[13].progress = 0;
	strcpy(achievement_list[13].name, "Blow The Bank");
	strcpy(achievement_list[13].description, "earn 1,000,000");

	achievement_list[14].img = Draw_CachePic("gfx/achievement/ammo_cost");
	achievement_list[14].unlocked = 0;
	achievement_list[14].progress = 0;
	strcpy(achievement_list[14].name, "Ammo Cost Too Much");
	strcpy(achievement_list[14].description, "After round 5, don't fire a bullet for whole round");

	achievement_list[15].img = Draw_CachePic("gfx/achievement/the_f_bomb");
	achievement_list[15].unlocked = 0;
	achievement_list[15].progress = 0;
	strcpy(achievement_list[15].name, "The F Bomb");
	strcpy(achievement_list[15].description, "Only nuke one zombie");

	achievement_list[16].img = Draw_CachePic("gfx/achievement/why_are");
	achievement_list[16].unlocked = 0;
	achievement_list[16].progress = 0;
	strcpy(achievement_list[16].name, "Why Are We Waiting?");
	strcpy(achievement_list[16].description, "Stand still for 2 minutes");

	achievement_list[17].img = Draw_CachePic("gfx/achievement/never_missed");
	achievement_list[17].unlocked = 0;
	achievement_list[17].progress = 0;
	strcpy(achievement_list[17].name, "Never Missed A Shot");
	strcpy(achievement_list[17].description, "Make it to round 5 without missing a shot");

	achievement_list[18].img = Draw_CachePic("gfx/achievement/300_bastards_less");
	achievement_list[18].unlocked = 0;
	achievement_list[18].progress = 0;
	strcpy(achievement_list[18].name, "300 Bastards less");
	strcpy(achievement_list[18].description, "Kill 300 zombies");

	achievement_list[19].img = Draw_CachePic("gfx/achievement/music_fan");
	achievement_list[19].unlocked = 0;
	achievement_list[19].progress = 0;
	strcpy(achievement_list[19].name, "Music Fan");
	strcpy(achievement_list[19].description, "Turn on radio 20 times");

	achievement_list[20].img = Draw_CachePic("gfx/achievement/one_clip");
	achievement_list[20].unlocked = 0;
	achievement_list[20].progress = 0;
	strcpy(achievement_list[20].name, "One Clip");
	strcpy(achievement_list[20].description, "Complete a round with mg42 without reloading");

	achievement_list[21].img = Draw_CachePic("gfx/achievement/one_20_20");
	achievement_list[21].unlocked = 0;
	achievement_list[21].progress = 0;
	strcpy(achievement_list[21].name, "One Clip, 20 Bullets, 20 Headshots");
	strcpy(achievement_list[21].description, "Score 20 headshots, with 20 bullets, and don't reload");

	achievement_list[22].img = Draw_CachePic("gfx/achievement/and_stay_out");
	achievement_list[22].unlocked = 0;
	achievement_list[22].progress = 0;
	strcpy(achievement_list[22].name, "And Stay out");
	strcpy(achievement_list[22].description, "Don't let zombies in for 2 rounds");*/

	achievement_locked = Image_LoadImage("gfx/achievement/achievement_locked", IMAGE_TGA, 0, true, false);

	m_achievement_scroll[0] = 0;
	m_achievement_scroll[1] = 0;
}

void Load_Achivements (void)
{
	int achievement_file;
	achievement_file = sceIoOpen(va("%s/data/ach.dat",com_gamedir), PSP_O_RDONLY, 0);

	if (achievement_file >= 0) {
		char* buffer = (char*)calloc(2, sizeof(char));
		for (int i = 0; i < MAX_ACHIEVEMENTS; i++) {
			sceIoRead(achievement_file, buffer, sizeof(char)*2);
			achievement_list[i].unlocked = atoi(buffer);
			sceIoRead(achievement_file, buffer, sizeof(char)*2);
			achievement_list[i].progress = atoi(buffer);
		}
	} else {
		achievement_file = sceIoOpen(va("%s/data/ach.dat",com_gamedir), PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0);

		for (int i = 0; i < MAX_ACHIEVEMENTS; i++) {
			sceIoWrite(achievement_file, "0\n", sizeof(char)*2);
			sceIoWrite(achievement_file, "0\n", sizeof(char)*2);
		}
	}
	sceIoClose(achievement_file);
}

void Save_Achivements (void)
{
	int achievement_file;
	achievement_file = sceIoOpen(va("%s/data/ach.dat",com_gamedir), PSP_O_WRONLY, 0);

	if (achievement_file >= 0) {
		for (int i = 0; i < MAX_ACHIEVEMENTS; i++) {
			char* buffer = va("%i\n", achievement_list[i].unlocked);
			char* buffer2 = va("%i\n", achievement_list[i].progress);
			sceIoWrite(achievement_file, va("%i\n", achievement_list[i].unlocked), strlen(buffer));
			sceIoWrite(achievement_file, va("%i\n", achievement_list[i].progress), strlen(buffer2));
		}
	} else {
		Load_Achivements();
	}
}

//=============================================================================
/* LOADSCREEN TEXT */

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

char *Platform_ReturnLoadingText (void)
{
    int StringNum = Random_Int(61);
    switch(StringNum)
    {
        case 1:
			return  "Released in 1996, Quake is now over 25 years old!";
            break;
        case 2:
            return  "Use the Kar98-k to be the hero we want you to be!";
            break;
        case 3:
            return  "There is a huge number of modern engines based on Quake!";
            break;
        case 4:
            return  "Development for NZ:P officially began on September 27, 2009";
            break;
        case 5:
            return  "NZ:P was first released on December 25, 2010!";
            break;
        case 6:
            return  "The 1.1 release of NZ:P has over 90,000 downloads!";
            break;
        case 7:
            return  "NZ:P has been downloaded over 400,000 times!";
            break;
        case 8:
            return  "The original NZP was made mainly by 3 guys around the world.";
            break;
        case 9:
            return  "Blubswillrule: known as \"blubs\", is from the USA.";
            break;
        case 10:
            return  "Jukki is from Finland.";
            break;
        case 11:
            return  "Ju[s]tice, or \"tom\" is from Lithuania.";
            break;
        case 12:
            return  "This game is the reason that we have bad sleeping habits!";
            break;
        case 13:
            return  "We had a lot of fun making this game.";
            break;
        case 14:
            return  "Did you know you can make your own Custom Map?";
            break;
        case 15:
            return  "Try Retro Mode, it's in the Graphics Settings!";
            break;
        case 16:
			return  "Tired of the base maps? Make your own or try some online!";
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
            return  "Don't play for too long, or zombies will eat you.";
            break;
        case 22:
            return  "That was epic... EPIC FOR THE WIIIN!"; //why
            break;
        case 23:
            return  "PPSSPP is an awesome PSP emulator!";
            break;
        case 24:
            return  "You dead yet?";
            break;
        case 25:
            return  "Now 21% cooler!";
            break;
        case 26:
            return  "your lg is nothink on the lan"; //what
            break;
        case 27:
            return  "I'm not your chaotic on dm6!"; 
            break;
        case 28:
            return  "Shoot zombies to kill them. Or knife them. You choose.";
            break;
        case 29:
            return 	"How many people forgot to Compile today?";
            break;
        case 30:
            return  "ggnore";
            break;
        case 31:
            return  "Have you tried NZ:P on PC or NX?";
            break;
        case 32:
            return  "Submerge your device in water for godmode!";
            break;
        case 33:
            return  "10/10/10 was a good day.";
            break;
        case 34:
            return  "Also check out \"No Bugs Allowed\" for the PSP!";
            break;
        case 35:
            return 	"Cypress, or \"Ivy\", is from the USA.";
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
			return 	"Pack-a-Punch the Kar98k to get to round infinity.";
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
			return  "NZ:P Discord is ONLY for Thomas the Tank Engine Roleplay!";
			break;
		case 45:
			return 	"Get rid of the 21% cooler tip, it's an MLP reference.";
			break;
		case 46:
			return 	"You're playing on a PSP!";
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
			return  "If a Nazi Zombie bites you, are you a Nazi, or a Zombie?";
			break;
		case 54:
			return  "Play some Custom Maps!";
			break;
		case 55:
			return  "Real OGs play on a PSP-1000!";
			break;
		case 56:
			return  "Adding this tip improved framerate by 39%!";
			break;
		case 57:
			return  "The NZ in NZP stands for New Zealand!";
			break;
		case 58:
			return  "The P in NZP stands for Professional!";
			break;
		case 59:
			return  "Many people have worked on this game!";
			break;
		case 60:
			return  "Remember to stay hydrated!";
			break;
		case 61:
			return  "cofe";
			break;
    }
    return "wut wut";
}