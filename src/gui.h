/* $Id$ */

/** @file gui.h */

#ifndef GUI_H
#define GUI_H

#include "window_type.h"
#include "vehicle_type.h"
#include "gfx_type.h"
#include "economy_type.h"
#include "tile_type.h"
#include "strings_type.h"

/* main_gui.cpp */
void CcPlaySound10(bool success, TileIndex tile, uint32 p1, uint32 p2);
void CcBuildCanal(bool success, TileIndex tile, uint32 p1, uint32 p2);

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

/* news_gui.cpp */
void ShowLastNewsMessage();
void ShowMessageOptions();
void ShowMessageHistory();

/* train_gui.cpp */
void ShowOrdersWindow(const Vehicle *v);

/* dock_gui.cpp */
void ShowBuildDocksToolbar();

/* aircraft_gui.cpp */
void ShowBuildAirToolbar();

/* tgp_gui.cpp */
void ShowGenerateLandscape();
void ShowHeightmapLoad();

/** Drag and drop selection process, or, what to do with an area of land when
 * you've selected it. */
enum {
	DDSP_DEMOLISH_AREA,
	DDSP_RAISE_AND_LEVEL_AREA,
	DDSP_LOWER_AND_LEVEL_AREA,
	DDSP_LEVEL_AREA,
	DDSP_CREATE_DESERT,
	DDSP_CREATE_ROCKS,
	DDSP_CREATE_WATER,
	DDSP_CREATE_RIVER,
	DDSP_PLANT_TREES,
	DDSP_BUILD_BRIDGE,

	/* Rail specific actions */
	DDSP_PLACE_RAIL_NE,
	DDSP_PLACE_RAIL_NW,
	DDSP_PLACE_AUTORAIL,
	DDSP_BUILD_SIGNALS,
	DDSP_BUILD_STATION,
	DDSP_REMOVE_STATION,
	DDSP_CONVERT_RAIL,

	/* Road specific actions */
	DDSP_PLACE_ROAD_X_DIR,
	DDSP_PLACE_ROAD_Y_DIR,
	DDSP_PLACE_AUTOROAD,
};

/* misc_gui.cpp */
void PlaceLandBlockInfo();
void ShowAboutWindow();
void ShowBuildTreesToolbar();
void ShowBuildTreesScenToolbar();
void ShowTownDirectory();
void ShowIndustryDirectory();
void ShowSubsidiesList();

void ShowEstimatedCostOrIncome(Money cost, int x, int y);
void ShowErrorMessage(StringID msg_1, StringID msg_2, int x, int y);

void ShowSmallMap();
void ShowExtraViewPortWindow();
void SetVScrollCount(Window *w, int num);
void SetVScroll2Count(Window *w, int num);
void SetHScrollCount(Window *w, int num);

void ShowCheatWindow();

void BuildFileList();
void SetFiosType(const byte fiostype);

/* FIOS_TYPE_FILE, FIOS_TYPE_OLDFILE etc. different colours */
extern const TextColour _fios_colors[];

/* bridge_gui.cpp */
void ShowBuildBridgeWindow(TileIndex start, TileIndex end, TransportType Transport_type, byte type);

void ShowBuildIndustryWindow();
void ShowMusicWindow();

/* main_gui.cpp */
void HandleOnEditText(const char *str);

void InitializeGUI();

#endif /* GUI_H */
