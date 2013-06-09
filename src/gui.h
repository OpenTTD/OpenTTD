/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gui.h GUI functions that shouldn't be here. */

#ifndef GUI_H
#define GUI_H

#include "vehicle_type.h"
#include "economy_type.h"
#include "tile_type.h"
#include "transport_type.h"
#include "story_type.h"
#include "company_type.h"

struct Window;

/* main_gui.cpp */
void HandleOnEditText(const char *str);
void InitializeGUI();

/* settings_gui.cpp */
void ShowGameOptions();
void ShowGameSettings();

/* train_gui.cpp */
void ShowOrdersWindow(const Vehicle *v);

/* dock_gui.cpp */
Window *ShowBuildDocksToolbar();
Window *ShowBuildDocksScenToolbar();

/* airport_gui.cpp */
Window *ShowBuildAirToolbar();

/* tgp_gui.cpp */
void ShowGenerateLandscape();
void ShowHeightmapLoad();

/* misc_gui.cpp */
void ShowLandInfo(TileIndex tile);
void ShowAboutWindow();
void ShowBuildTreesToolbar();
void ShowTownDirectory();
void ShowIndustryDirectory();
void ShowIndustryCargoesWindow();
void ShowSubsidiesList();
void ShowGoalsList(CompanyID company);
void ShowGoalQuestion(uint16 id, byte type, uint32 button_mask, const char *question);
void ShowStoryBook(CompanyID company, uint16 page_id = INVALID_STORY_PAGE);

void ShowEstimatedCostOrIncome(Money cost, int x, int y);

void ShowExtraViewPortWindow(TileIndex tile = INVALID_TILE);
void ShowExtraViewPortWindowForTileUnderCursor();

/* bridge_gui.cpp */
void ShowBuildBridgeWindow(TileIndex start, TileIndex end, TransportType transport_type, byte bridge_type);

void ShowBuildIndustryWindow();
void ShowFoundTownWindow();
void ShowMusicWindow();

#endif /* GUI_H */
