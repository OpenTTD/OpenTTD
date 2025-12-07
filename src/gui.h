/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gui.h GUI functions that shouldn't be here. */

#ifndef GUI_H
#define GUI_H

#include "strings_type.h"
#include "vehicle_type.h"
#include "economy_type.h"
#include "tile_type.h"
#include "transport_type.h"
#include "story_type.h"
#include "company_type.h"

struct Window;

/* main_gui.cpp */
void InitializeGUI();

/* settings_gui.cpp */
void ShowGameOptions();

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
void ShowEstimatedCostOrIncome(Money cost, int x, int y);

/* tree_gui.cpp */
void ShowBuildTreesToolbar();

/* town_gui.cpp */
void ShowTownDirectory();
void ShowFoundTownWindow();

/* industry_gui.cpp */
void ShowIndustryDirectory();
void ShowIndustryCargoesWindow();
void ShowBuildIndustryWindow();

/* subsidy_gui.cpp */
void ShowSubsidiesList();

/* goal_gui.cpp */
void ShowGoalsList(CompanyID company);
void ShowGoalQuestion(uint16_t id, uint8_t type, uint32_t button_mask, const EncodedString &question);

/* story_gui.cpp */
void ShowStoryBook(CompanyID company, StoryPageID page_id = StoryPageID::Invalid(), bool centered = false);

/* viewport_gui.cpp */
void ShowExtraViewportWindow(TileIndex tile = INVALID_TILE);
void ShowExtraViewportWindowForTileUnderCursor();

/* bridge_gui.cpp */
void ShowBuildBridgeWindow(TileIndex start, TileIndex end, TransportType transport_type, uint8_t bridge_type);

/* music_gui.cpp */
void ShowMusicWindow();

#endif /* GUI_H */
