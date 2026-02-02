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
#include "../../menu/menu_defs.h"

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