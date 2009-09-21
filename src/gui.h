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

#include "window_type.h"
#include "vehicle_type.h"
#include "gfx_type.h"
#include "economy_type.h"
#include "tile_type.h"
#include "strings_type.h"
#include "transport_type.h"

/* main_gui.cpp */
void CcPlaySound10(bool success, TileIndex tile, uint32 p1, uint32 p2);
void CcBuildCanal(bool success, TileIndex tile, uint32 p1, uint32 p2);
void HandleOnEditText(const char *str);
void InitializeGUI();

/* settings_gui.cpp */
void ShowGameOptions();
void ShowGameDifficulty();
void ShowGameSettings();
void DrawArrowButtons(int x, int y, Colours button_colour, byte state, bool clickable_left, bool clickable_right);

/* graph_gui.cpp */
void ShowOperatingProfitGraph();
void ShowIncomeGraph();
void ShowDeliveredCargoGraph();
void ShowPerformanceHistoryGraph();
void ShowCompanyValueGraph();
void ShowCargoPaymentRates();
void ShowCompanyLeagueTable();
void ShowPerformanceRatingDetail();

/* train_gui.cpp */
void ShowOrdersWindow(const Vehicle *v);

/* dock_gui.cpp */
void ShowBuildDocksToolbar();
void ShowBuildDocksScenToolbar();

/* aircraft_gui.cpp */
void ShowBuildAirToolbar();

/* tgp_gui.cpp */
void ShowGenerateLandscape();
void ShowHeightmapLoad();

/* misc_gui.cpp */
void PlaceLandBlockInfo();
void ShowAboutWindow();
void ShowBuildTreesToolbar();
void ShowTownDirectory();
void ShowIndustryDirectory();
void ShowSubsidiesList();

void ShowEstimatedCostOrIncome(Money cost, int x, int y);
void ShowErrorMessage(StringID msg_1, StringID msg_2, int x, int y, bool no_timeout = false);

void ShowSmallMap();
void ShowExtraViewPortWindow(TileIndex tile = INVALID_TILE);

void BuildFileList();
void SetFiosType(const byte fiostype);

/* FIOS_TYPE_FILE, FIOS_TYPE_OLDFILE etc. different colours */
extern const TextColour _fios_colours[];

/* bridge_gui.cpp */
void ShowBuildBridgeWindow(TileIndex start, TileIndex end, TransportType transport_type, byte bridge_type);

void ShowBuildIndustryWindow();
void ShowFoundTownWindow();
void ShowMusicWindow();


#endif /* GUI_H */
