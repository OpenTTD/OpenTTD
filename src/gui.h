/* $Id$ */

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
void ShowPatchesSelection();
void DrawArrowButtons(int x, int y, int ctab, byte state, bool clickable_left, bool clickable_right);

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
void ShowErrorMessage(StringID msg_1, StringID msg_2, int x, int y);

void ShowSmallMap();
void ShowExtraViewPortWindow(TileIndex tile = INVALID_TILE);
void SetVScrollCount(Window *w, int num);
void SetVScroll2Count(Window *w, int num);
void SetHScrollCount(Window *w, int num);

void BuildFileList();
void SetFiosType(const byte fiostype);

/* FIOS_TYPE_FILE, FIOS_TYPE_OLDFILE etc. different colours */
extern const TextColour _fios_colors[];

/* bridge_gui.cpp */
void ShowBuildBridgeWindow(TileIndex start, TileIndex end, TransportType transport_type, byte bridge_type);

void ShowBuildIndustryWindow();
void ShowBuildTownWindow();
void ShowMusicWindow();


#endif /* GUI_H */
