#ifndef GUI_H
#define GUI_H

#include "window.h"

/* main_gui.c */
void SetupColorsAndInitialWindow();
void CcPlaySound10(bool success, uint tile, uint32 p1, uint32 p2);
void PlaceProc_Sign(uint tile);

/* settings_gui.c */
void ShowGameOptions();
void ShowGameDifficulty();
void ShowPatchesSelection();
void ShowNewgrf();

/* graph_gui.c */
void ShowOperatingProfitGraph();
void ShowIncomeGraph();
void ShowDeliveredCargoGraph();
void ShowPerformanceHistoryGraph();
void ShowCompanyValueGraph();
void ShowCargoPaymentRates();
void ShowCompanyLeagueTable();
void ShowPerformanceRatingDetail();

/* news_gui.c */
void ShowLastNewsMessage();
void ShowMessageOptions();
void ShowMessageHistory();

/* traintoolb_gui.c */
void ShowBuildRailToolbar(int index, int button);
void PlaceProc_BuyLand(uint tile);

/* train_gui.c */
void ShowPlayerTrains(int player, int station);
void ShowTrainViewWindow(Vehicle *v);
void ShowTrainDetailsWindow(Vehicle *v);
void ShowOrdersWindow(Vehicle *v);

void ShowRoadVehViewWindow(Vehicle *v);

/* road_gui.c */
void ShowBuildRoadToolbar();
void ShowBuildRoadScenToolbar();
void ShowPlayerRoadVehicles(int player, int station);

/* dock_gui.c */
void ShowBuildDocksToolbar();
void ShowPlayerShips(int player, int station);

void ShowShipViewWindow(Vehicle *v);

/* aircraft_gui.c */
void ShowBuildAirToolbar();
void ShowPlayerAircraft(int player, int station);

/* terraform_gui.c */
void PlaceProc_DemolishArea(uint tile);
void PlaceProc_LowerLand(uint tile);
void PlaceProc_RaiseLand(uint tile);
void PlaceProc_LevelLand(uint tile);
void ShowTerraformToolbar();

/* misc_gui.c */
void PlaceLandBlockInfo();
void ShowAboutWindow();
void ShowBuildTreesToolbar();
void ShowBuildTreesScenToolbar();
void ShowTownDirectory();
void ShowIndustryDirectory();
void ShowSubsidiesList();
void ShowPlayerStations(int player);
void ShowPlayerFinances(int player);
void ShowPlayerCompany(int player);

void ShowEstimatedCostOrIncome(int32 cost, int x, int y);
void ShowErrorMessage(StringID msg_1, StringID msg_2, int x, int y);

void DrawStationCoverageAreaText(int sx, int sy, uint mask,int rad);
void CheckRedrawStationCoverage(Window *w);

void ShowSmallMap();
void ShowExtraViewPortWindow();
void SetVScrollCount(Window *w, int num);
void SetHScrollCount(Window *w, int num);
int HandleEditBoxKey(Window *w, int wid, WindowEvent *we);

void ShowCheatWindow();
void AskForNewGameToStart();

void DrawEditBox(Window *w, int wid);
void HandleEditBox(Window *w, int wid);

void BuildFileList();
void SetFiosType(const byte fiostype);

/*	FIOS_TYPE_FILE, FIOS_TYPE_OLDFILE etc. different colours */
static const byte _fios_colors[] = {13, 9, 9, 6, 5, 6, 5};


/* network gui */
void ShowNetworkGameWindow();
void ShowChatWindow(StringID str, StringID caption, int maxlen, int maxwidth, byte window_class, uint16 window_number);

/* bridge_gui.c */
void ShowBuildBridgeWindow(uint start, uint end, byte type);

enum {
	ZOOM_IN = 0,
	ZOOM_OUT = 1,
	ZOOM_NONE = 2, // hack, used to update the button status
};

bool DoZoomInOutWindow(int how, Window * w);
void ShowBuildIndustryWindow();
void ShowQueryString(StringID str, StringID caption, int maxlen, int maxwidth, byte window_class, uint16 window_number);
void ShowMusicWindow();

/* main_gui.c */
VARDEF byte _newspaper_flag;
VARDEF byte _construct_mode;
VARDEF byte _station_show_coverage;
VARDEF PlaceProc *_place_proc;
VARDEF bool _no_button_sound;

#endif /* GUI_H */
