/*
Copyright (C) 2025-2026 NZ:P Team

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
#include "../nzportable_def.h"
#include "menu_defs.h"

#ifdef __PSP__

#include <pspkernel.h>
#include <pspwlan.h>
#include "../platform/psp/net_dgrm.h"

// defined in platform/psp/net_dgrm.c — restores this menu when a
// connect attempt fails
extern int		m_return_state;
extern qboolean	m_return_onerror;
extern char		m_return_reason[32];

//=============================================================================
/* COOPERATIVE (ADHOC) MENU */

static qboolean adhoc_failed;

/*
===============
Menu_Coop_EnableAdhoc

Bring up the AdHoc landriver, mirroring the v1 activation flow:
Datagram_Shutdown -> tcpipAvailable/tcpipAdhoc -> net_driver_to_use=1 ->
Datagram_Init. Returns true when the driver is (already) up.
===============
*/
static qboolean adhoc_starting;

static qboolean Menu_Coop_EnableAdhoc (void)
{
	if (net_landrivers[1].initialized)
		return true;

	// hardware WLAN switch must be on — adhocctl can otherwise
	// stall for the full join timeout
	if (!sceWlanDevIsPowerOn()) {
		adhoc_failed = true;
		Menu_SetSound(MENU_SND_BEEP);
		return false;
	}

	// joining the adhocctl group blocks for up to ~10s — push one
	// frame with the "STARTING ADHOC..." notice before stalling
	adhoc_starting = true;
	SCR_UpdateScreen();
	adhoc_starting = false;

	// two attempts: PPSSPP's built-in adhoc server starts alongside
	// the first network init, so the very first join can race it and
	// time out — the retry then lands on a warm server
	for (int attempt = 0; attempt < 2 && !net_landrivers[1].initialized; attempt++) {
		if (attempt > 0)
			sceKernelDelayThread(1000 * 1000); // let adhocctl teardown settle

		Datagram_Shutdown();

		tcpipAvailable = true;
		tcpipAdhoc = true;
		net_driver_to_use = 1; // landriver 1 = AdHoc

		Datagram_Init();
	}

	// Datagram_Init returns 0 even when the landriver failed, so
	// check the flag instead
	if (!net_landrivers[1].initialized) {
		tcpipAvailable = false;
		tcpipAdhoc = false;
		net_driver_to_use = 0;
		adhoc_failed = true;
		Menu_SetSound(MENU_SND_BEEP);
		return false;
	}

	adhoc_failed = false;
	return true;
}

static void Menu_Coop_HostGame (void)
{
	if (!Menu_Coop_EnableAdhoc())
		return;

	menu_is_solo = false; // flips map selection into COOP mode
	Menu_StockMaps_Set();
}

static void Menu_Coop_JoinGame (void)
{
	if (!Menu_Coop_EnableAdhoc())
		return;

	menu_is_solo = false;
	Menu_CoopJoin_Set();
}

/*
===============
Menu_Coop_Set
===============
*/
void Menu_Coop_Set (void)
{
	Menu_ResetMenuButtons();
	Menu_SetSound(MENU_SND_ENTER);

	adhoc_failed = false;

	key_dest = key_menu;
	m_previous_state = m_main;
	m_state = m_coop;
}

/*
===============
Menu_Coop_Draw
===============
*/
void Menu_Coop_Draw (void)
{
	// Background
	Menu_DrawCustomBackground (true);

	// Header
	Menu_DrawTitle ("COOPERATIVE", MENU_COLOR_WHITE);

	// Version String
	Menu_DrawBuildDate();

	Menu_DrawButton(1, 0, "HOST GAME", "Create an AdHoc Game for nearby PSPs.", Menu_Coop_HostGame);
	Menu_DrawButton(2, 1, "JOIN GAME", "Search for AdHoc Games near you.", Menu_Coop_JoinGame);

	Menu_DrawButton(-1, 2, "BACK", "Return to Main Menu.", Menu_Main_Set);

	if (adhoc_starting) {
		Menu_DrawGreyButton(4, "STARTING ADHOC...");
	} else if (adhoc_failed) {
		if (!sceWlanDevIsPowerOn())
			Menu_DrawGreyButton(4, "TURN ON THE WLAN SWITCH!");
		else
			Menu_DrawGreyButton(4, "COULD NOT START ADHOC.");
	}
}

//=============================================================================
/* JOIN GAME (host browser over the slist host cache) */

static int		join_last_count = -1;
static qboolean	join_last_scanning;
static char		join_rows[HOSTCACHESIZE][48];

static void Menu_CoopJoin_StartScan (void)
{
	if (slistInProgress)
		return;

	m_return_reason[0] = 0;
	hostCacheCount = 0;
	slistSilent = true; // no console spam
	slistLocal = false; // skip the loopback driver
	NET_Slist_f(); // NET_Poll (host.c) drives the scan every frame
}

static void Menu_CoopJoin_Connect (void)
{
	int idx = current_menu.cursor;

	if (idx < 0 || idx >= hostCacheCount)
		return;

	Menu_SetSound(MENU_SND_ENTER);

	// on a failed/rejected connect, _Datagram_Connect restores this
	// menu instead of leaving a dead scene
	m_return_state = m_state;
	m_return_onerror = true;

	key_dest = key_game;
	m_state = m_none;

	// connect by cname ("xx:xx:xx:xx:xx:xx:port") — hostnames can
	// collide, MACs cannot
	Cbuf_AddText (va("connect \"%s\"\n", hostcache[idx].cname));
}

/*
===============
Menu_CoopJoin_Set
===============
*/
void Menu_CoopJoin_Set (void)
{
	Menu_ResetMenuButtons();

	key_dest = key_menu;
	m_previous_state = m_coop;
	m_state = m_coopjoin;

	join_last_count = -1;
	Menu_CoopJoin_StartScan();
}

/*
===============
Menu_CoopJoin_Draw
===============
*/
void Menu_CoopJoin_Draw (void)
{
	int i;

	// Background
	Menu_DrawCustomBackground (true);

	// Header
	Menu_DrawTitle ("JOIN GAME", MENU_COLOR_WHITE);

	// Version String
	Menu_DrawBuildDate();

	// the host cache fills asynchronously while the scan runs —
	// rebuild the button slots whenever it grows AND when the scan
	// finishes (the button layout changes between the two states)
	if (join_last_count != hostCacheCount || join_last_scanning != slistInProgress) {
		Menu_ResetMenuButtons();
		join_last_count = hostCacheCount;
		join_last_scanning = slistInProgress;
	}

	for (i = 0; i < hostCacheCount; i++) {
		if (hostcache[i].maxusers)
			snprintf(join_rows[i], sizeof(join_rows[i]), "%.15s (%.15s) %d/%d", hostcache[i].name, hostcache[i].map, hostcache[i].users, hostcache[i].maxusers);
		else
			snprintf(join_rows[i], sizeof(join_rows[i]), "%.15s (%.15s)", hostcache[i].name, hostcache[i].map);

		Menu_DrawButton(i + 1, i, join_rows[i], "Join this Game.", Menu_CoopJoin_Connect);
	}

	if (slistInProgress) {
		Menu_DrawGreyButton(hostCacheCount + 2, "SEARCHING...");
		Menu_DrawButton(-1, hostCacheCount, "BACK", "Return to Cooperative Menu.", Menu_Coop_Set);
	} else {
		if (hostCacheCount == 0)
			Menu_DrawGreyButton(2, "NO GAMES FOUND");

		Menu_DrawButton(hostCacheCount + 3, hostCacheCount, "RESCAN", "Search again.", Menu_CoopJoin_StartScan);
		Menu_DrawButton(-1, hostCacheCount + 1, "BACK", "Return to Cooperative Menu.", Menu_Coop_Set);
	}

	// why the last connect attempt bounced us back here
	if (m_return_reason[0])
		Menu_DrawGreyButton(hostCacheCount + 4, m_return_reason);
}

#endif // __PSP__
