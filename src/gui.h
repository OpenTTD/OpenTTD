/* $Id$ */

#ifndef GUI_H
#define GUI_H

#include "station.h"
#include "window.h"
#include "string.h"

/* main_gui.c */
void SetupColorsAndInitialWindow(void);
void CcPlaySound10(bool success, TileIndex tile, uint32 p1, uint32 p2);
void CcBuildCanal(bool success, TileIndex tile, uint32 p1, uint32 p2);
void CcTerraform(bool success, TileIndex tile, uint32 p1, uint32 p2);

/* settings_gui.c */
void ShowGameOptions(void);
void ShowGameDifficulty(void);
void ShowPatchesSelection(void);
void DrawArrowButtons(int x, int y, int ctab, byte state, bool clickable_left, bool clickable_right);

/* graph_gui.c */
extern const byte _cargo_colours[NUM_CARGO];
void ShowOperatingProfitGraph(void);
void ShowIncomeGraph(void);
void ShowDeliveredCargoGraph(void);
void ShowPerformanceHistoryGraph(void);
void ShowCompanyValueGraph(void);
void ShowCargoPaymentRates(void);
void ShowCompanyLeagueTable(void);
void ShowPerformanceRatingDetail(void);

/* news_gui.c */
void ShowLastNewsMessage(void);
void ShowMessageOptions(void);
void ShowMessageHistory(void);

/* rail_gui.c */
void ShowBuildRailToolbar(RailType railtype, int button);
void PlaceProc_BuyLand(TileIndex tile);
void ReinitGuiAfterToggleElrail(bool disable);

/* train_gui.c */
void ShowTrainViewWindow(const Vehicle *v);
void ShowOrdersWindow(const Vehicle *v);

/* road_gui.c */
void ShowBuildRoadToolbar(void);
void ShowBuildRoadScenToolbar(void);
void ShowRoadVehViewWindow(const Vehicle *v);

/* dock_gui.c */
void ShowBuildDocksToolbar(void);
void ShowShipViewWindow(const Vehicle *v);

/* aircraft_gui.c */
void ShowBuildAirToolbar(void);

/* terraform_gui.c */
void ShowTerraformToolbar(void);

/* tgp_gui.c */
void ShowGenerateLandscape(void);
void ShowHeightmapLoad(void);

void PlaceProc_DemolishArea(TileIndex tile);
void PlaceProc_LevelLand(TileIndex tile);
bool GUIPlaceProcDragXY(const WindowEvent *e);

enum { // max 32 - 4 = 28 types
	GUI_PlaceProc_DemolishArea    = 0 << 4,
	GUI_PlaceProc_LevelArea       = 1 << 4,
	GUI_PlaceProc_DesertArea      = 2 << 4,
	GUI_PlaceProc_WaterArea       = 3 << 4,
	GUI_PlaceProc_ConvertRailArea = 4 << 4,
	GUI_PlaceProc_RockyArea       = 5 << 4,
};

/* misc_gui.c */
void PlaceLandBlockInfo(void);
void ShowAboutWindow(void);
void ShowBuildTreesToolbar(void);
void ShowBuildTreesScenToolbar(void);
void ShowTownDirectory(void);
void ShowIndustryDirectory(void);
void ShowSubsidiesList(void);
void ShowPlayerStations(PlayerID player);
void ShowPlayerFinances(PlayerID player);
void ShowPlayerCompany(PlayerID player);

void ShowEstimatedCostOrIncome(int32 cost, int x, int y);
void ShowErrorMessage(StringID msg_1, StringID msg_2, int x, int y);

void DrawStationCoverageAreaText(int sx, int sy, uint mask,int rad);
void CheckRedrawStationCoverage(const Window *w);

void ShowSmallMap(void);
void ShowExtraViewPortWindow(void);
void SetVScrollCount(Window *w, int num);
void SetVScroll2Count(Window *w, int num);
void SetHScrollCount(Window *w, int num);

void ShowCheatWindow(void);

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

void BuildFileList(void);
void SetFiosType(const byte fiostype);

/* FIOS_TYPE_FILE, FIOS_TYPE_OLDFILE etc. different colours */
extern const byte _fios_colors[];

/* bridge_gui.c */
void ShowBuildBridgeWindow(uint start, uint end, byte type);

void ShowBuildIndustryWindow(void);
void ShowQueryString(StringID str, StringID caption, uint maxlen, uint maxwidth, Window *parent, CharSetFilter afilter);
void ShowQuery(StringID caption, StringID message, Window *w, void (*callback)(Window*, bool));
void ShowMusicWindow(void);

/* main_gui.c */
void HandleOnEditText(const char *str);
VARDEF bool _station_show_coverage;
VARDEF PlaceProc *_place_proc;

/* vehicle_gui.c */
void InitializeGUI(void);

#endif /* GUI_H */
