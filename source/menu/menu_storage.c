/*
 * Copyright (C) 2025-2026 NZ:P Team
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
 */
#include "../nzportable_def.h"
#include "menu_defs.h"

static void Menu_Storage_ApplyAssetCache (void)
{
	Cvar_SetValue("asset_cache", !asset_cache.value);
}

void Menu_Storage_Set (void)
{
	Menu_ResetMenuButtons();
	m_previous_state = m_configuration;
	m_state = m_storage;
}

static void Menu_Storage_BuildAssetCache (void)
{
	if (AssetCache_GetBuildState()->active)
		AssetCache_CancelBuild();
	else
		AssetCache_BeginBuild();
}

void Menu_Storage_Draw (void)
{
	const asset_cache_build_t *build = AssetCache_GetBuildState();
	char description[160];
	char progress[32];

	if (build->active && build->scanning)
		snprintf(description, sizeof(description), "%s Select to cancel.", build->status);
	else if (build->active)
		snprintf(description, sizeof(description), "Preparing %s. Select to cancel.", build->current);
	else if (build->status[0])
		Q_strlcpy(description, build->status, sizeof(description));
	else
		Q_strlcpy(description, "Prepare installed images, sounds, and models. Reuse valid cache entries.", sizeof(description));
	Menu_DrawCustomBackground(true);
	Menu_DrawTitle("STORAGE", MENU_COLOR_WHITE);
	Menu_DrawMapPanel();
	Menu_DrawButton(1, 0, "ASSET CACHE",
	  "Save prepared game assets to reduce future loading times. Uses storage space.",
	  Menu_Storage_ApplyAssetCache);
	Menu_DrawOptionButton(1, asset_cache.value ? "ON" : "OFF");
	Menu_DrawButton(2, 1, build->active ? "CANCEL CACHE BUILD" : "BUILD ASSET CACHE",
	  description, Menu_Storage_BuildAssetCache);
	if ((build->active && !build->scanning) || build->checked) {
		snprintf(progress, sizeof(progress), "%u/%u CACHED", build->cached, build->total);
		Menu_DrawOptionButton(2, progress);
	}
	Menu_DrawButton(3, 2, "CLEAR ASSET CACHE",
	  "Remove cached assets. They will be rebuilt as needed.", AssetCache_Clear);
	Menu_DrawButton(4, 3, "COMPACT CACHE",
	  "Reclaim unused cache space. Needs temporary free storage.", AssetCache_Compact);
	Menu_DrawDivider(-1.5);
	Menu_DrawButton(-1, 4, "BACK", "Return to Configuration Menu.", Menu_Configuration_Set);
}
