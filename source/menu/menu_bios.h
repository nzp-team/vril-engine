#ifndef _MENU_BIOS_H_
#define _MENU_BIOS_H_

#include "menu_defs.h"

struct menu_bio_entry 
{
    char* name;
    char* map;
    int portrait;
    vec2_t portrait_coords;
    char* description[17];
};

struct menu_bio_entry menu_bios[12] =
{
    {
        "TANK DEMPSEY",
        "VERRUCKT",
        0,
        {0, 0},
        {
            "Great American Hero: Tank Dempsey is a legend among men. Rigid",
            "and well disciplined, Tank is a man of strength and sheer will.",
            "Of those who have tried none of his enemies have ever been",
            "able to keep this monster down for long. Personally chosen by",
            "Purnell to lead the rescue mission of missing operative Peter",
            "McCain; after displaying immense skill during prior missions",
            "it's no wonder why the choice was made. His skills in hand to",
            "hand combat, weapons expertise, and headstrong nature make him",
            "the perfect candidate for this expedition. Known for his",
            "unwavering determination, tales of his exploits are widespread;",
            "it's said that he once took out an entire cohort of enemies",
            "armed only with a bobby pin and his medal of honor, that's just",
            "one of many stories spun about his heroics. No one knows the",
            "full extent of what he could be capable of, but if the rumors",
            "have any merrit, he isn't a man you want to make an enemy of.", 
            "Only one thing is known for certain, Dempsey may one day perish,",
            "but his legend will never die."
        }
    },
    {
        "??????????",
        "??????",
        0,
        {0.5, 0},
        {
            "???",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            ""
        }
    },
    {
        "??????????",
        "??????",
        0,
        {0, 0.5},
        {
            "???",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            ""
        }
    },
    {
        "??????????",
        "??????",
        0,
        {0.5, 0.5},
        {
            "???",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            ""
        }
    },
    {
        "??????????",
        "??????",
        1,
        {0, 0},
        {
            "???",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            ""
        }
    },
    {
        "??????????",
        "??????",
        1,
        {0, 0},
        {
            "???",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            ""
        }
    },
    {
        "??????????",
        "??????",
        1,
        {0, 0},
        {
            "???",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            ""
        }
    },
    {
        "??????????",
        "??????",
        1,
        {0, 0},
        {
            "???",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            ""
        }
    },
    {
        "??????????",
        "??????",
        2,
        {0, 0},
        {
            "???",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            ""
        }
    },
    {
        "??????????",
        "??????",
        2,
        {0, 0},
        {
            "???",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            ""
        }
    },
    {
        "??????????",
        "??????",
        2,
        {0, 0},
        {
            "???",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            ""
        }
    },
    {
        "??????????",
        "??????",
        2,
        {0, 0},
        {
            "???",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            ""
        }
    }
};

#endif // _MENU_BIOS_H_