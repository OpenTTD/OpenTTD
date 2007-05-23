/* $Id$ */

/** @file gui.h */

#ifndef GUI_H
#define GUI_H

#include "station.h"
#include "window.h"
#include "string.h"

/* main_gui.cpp */
void SetupColorsAndInitialWindow();
void CcPlaySound10(bool success, TileIndex tile, uint32 p1, uint32 p2);
void CcBuildCanal(bool success, TileIndex tile, uint32 p1, uint32 p2);
void CcTerraform(bool success, TileIndex tile, uint32 p1, uint32 p2);

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

/* rail_gui.cpp */
void ShowBuildRailToolbar(RailType railtype, int button);
void PlaceProc_BuyLand(TileIndex tile);
void ReinitGuiAfterToggleElrail(bool disable);

/* train_gui.cpp */
void ShowTrainViewWindow(const Vehicle *v);
void ShowOrdersWindow(const Vehicle *v);

/* road_gui.cpp */
void ShowBuildRoadToolbar(RoadType roadtype);
void ShowBuildRoadScenToolbar();
void ShowRoadVehViewWindow(const Vehicle *v);

/* dock_gui.cpp */
void ShowBuildDocksToolbar();
void ShowShipViewWindow(const Vehicle *v);

/* aircraft_gui.cpp */
void ShowBuildAirToolbar();

/* terraform_gui.cpp */
void ShowTerraformToolbar(Window *link = NULL);

/* tgp_gui.cpp */
void ShowGenerateLandscape();
void ShowHeightmapLoad();

void PlaceProc_DemolishArea(TileIndex tile);
void PlaceProc_LevelLand(TileIndex tile);
bool GUIPlaceProcDragXY(const WindowEvent *e);

enum {
	GUI_PlaceProc_None,
	GUI_PlaceProc_DemolishArea,
	GUI_PlaceProc_LevelArea,
	GUI_PlaceProc_DesertArea,
	GUI_PlaceProc_WaterArea,
	GUI_PlaceProc_ConvertRailArea,
	GUI_PlaceProc_RockyArea,
	GUI_PlaceProc_RemoveFromStation,
};

/* misc_gui.cpp */
void PlaceLandBlockInfo();
void ShowAboutWindow();
void ShowBuildTreesToolbar();
void ShowBuildTreesScenToolbar();
void ShowTownDirectory();
void ShowIndustryDirectory();
void ShowSubsidiesList();
void ShowPlayerStations(PlayerID player);
void ShowPlayerFinances(PlayerID player);
void ShowPlayerCompany(PlayerID player);

void ShowEstimatedCostOrIncome(int32 cost, int x, int y);
void ShowErrorMessage(StringID msg_1, StringID msg_2, int x, int y);

void DrawStationCoverageAreaText(int sx, int sy, uint mask,int rad);
void CheckRedrawStationCoverage(const Window *w);

void ShowSmallMap();
void ShowExtraViewPortWindow();
void SetVScrollCount(Window *w, int num);
void SetVScroll2Count(Window *w, int num);
void SetHScrollCount(Window *w, int num);

void ShowCheatWindow();

void DrawEditBox(Window *w, querystr_d *string, int wid);
void HandleEditBox(Window *w, querystr_d *string, int wid);
int HandleEditBoxKey(Window *w, querystr_d *string, int wid, WindowEvent *we);
bool HandleCaret(Textbuf *tb);

void DeleteTextBufferAll(Textbuf *tb);
bool DeleteTextBufferChar(Textbuf *tb, int delmode);
bool InsertTextBufferChar(Textbuf *tb, uint32 key);
bool InsertTextBufferClipboard(Textbuf *tb);
bool MoveTextBufferPos(Textbuf *tb, int navmode);
void InitializeTextBuffer(Textbuf *tb, const char *buf, uint16 maxlength, uint16 maxwidth);
void UpdateTextBufferSize(Textbuf *tb);

void BuildFileList();
void SetFiosType(const byte fiostype);

/* FIOS_TYPE_FILE, FIOS_TYPE_OLDFILE etc. different colours */
extern const byte _fios_colors[];

/* bridge_gui.cpp */
void ShowBuildBridgeWindow(uint start, uint end, byte type);

void ShowBuildIndustryWindow();
void ShowQueryString(StringID str, StringID caption, uint maxlen, uint maxwidth, Window *parent, CharSetFilter afilter);
void ShowQuery(StringID caption, StringID message, Window *w, void (*callback)(Window*, bool));
void ShowMusicWindow();

/* main_gui.cpp */
void HandleOnEditText(const char *str);
VARDEF bool _station_show_coverage;
VARDEF PlaceProc *_place_proc;

/* vehicle_gui.cpp */
void InitializeGUI();

void ShowPlayerGroup(PlayerID player, VehicleType veh);

#endif /* GUI_H */
