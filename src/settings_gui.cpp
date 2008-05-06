/* $Id$ */

/** @file settings_gui.cpp GUI for settings. */

#include "stdafx.h"
#include "openttd.h"
#include "currency.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "engine_func.h"
#include "screenshot.h"
#include "newgrf.h"
#include "network/network.h"
#include "town.h"
#include "variables.h"
#include "settings_internal.h"
#include "newgrf_townname.h"
#include "strings_func.h"
#include "functions.h"
#include "window_func.h"
#include "vehicle_base.h"
#include "core/alloc_func.hpp"
#include "string_func.h"
#include "gfx_func.h"
#include "waypoint.h"
#include "widgets/dropdown_type.h"
#include "widgets/dropdown_func.h"
#include "station_func.h"

#include "table/sprites.h"
#include "table/strings.h"

static const StringID _units_dropdown[] = {
	STR_UNITS_IMPERIAL,
	STR_UNITS_METRIC,
	STR_UNITS_SI,
	INVALID_STRING_ID
};

static const StringID _driveside_dropdown[] = {
	STR_02E9_DRIVE_ON_LEFT,
	STR_02EA_DRIVE_ON_RIGHT,
	INVALID_STRING_ID
};

static const StringID _autosave_dropdown[] = {
	STR_02F7_OFF,
	STR_AUTOSAVE_1_MONTH,
	STR_02F8_EVERY_3_MONTHS,
	STR_02F9_EVERY_6_MONTHS,
	STR_02FA_EVERY_12_MONTHS,
	INVALID_STRING_ID,
};

static const StringID _designnames_dropdown[] = {
	STR_02BE_DEFAULT,
	STR_02BF_CUSTOM,
	INVALID_STRING_ID
};

static StringID *BuildDynamicDropdown(StringID base, int num)
{
	static StringID buf[32 + 1];
	StringID *p = buf;
	while (--num >= 0) *p++ = base++;
	*p = INVALID_STRING_ID;
	return buf;
}

int _nb_orig_names = SPECSTR_TOWNNAME_LAST - SPECSTR_TOWNNAME_START + 1;
static StringID *_grf_names = NULL;
static int _nb_grf_names = 0;

void InitGRFTownGeneratorNames()
{
	free(_grf_names);
	_grf_names = GetGRFTownNameList();
	_nb_grf_names = 0;
	for (StringID *s = _grf_names; *s != INVALID_STRING_ID; s++) _nb_grf_names++;
}

static inline StringID TownName(int town_name)
{
	if (town_name < _nb_orig_names) return STR_TOWNNAME_ORIGINAL_ENGLISH + town_name;
	town_name -= _nb_orig_names;
	if (town_name < _nb_grf_names) return _grf_names[town_name];
	return STR_UNDEFINED;
}

static int GetCurRes()
{
	int i;

	for (i = 0; i != _num_resolutions; i++) {
		if (_resolutions[i][0] == _screen.width &&
				_resolutions[i][1] == _screen.height) {
			break;
		}
	}
	return i;
}

static inline bool RoadVehiclesAreBuilt()
{
	const Vehicle* v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_ROAD) return true;
	}
	return false;
}


enum GameOptionsWidgets {
	GAMEOPT_CURRENCY_BTN    =  4,
	GAMEOPT_DISTANCE_BTN    =  6,
	GAMEOPT_ROADSIDE_BTN    =  8,
	GAMEOPT_TOWNNAME_BTN    = 10,
	GAMEOPT_AUTOSAVE_BTN    = 12,
	GAMEOPT_VEHICLENAME_BTN = 14,
	GAMEOPT_VEHICLENAME_SAVE,
	GAMEOPT_LANG_BTN        = 17,
	GAMEOPT_RESOLUTION_BTN  = 19,
	GAMEOPT_FULLSCREEN,
	GAMEOPT_SCREENSHOT_BTN  = 22,
};

/**
 * Update/redraw the townnames dropdown
 * @param w   the window the dropdown belongs to
 * @param sel the currently selected townname generator
 */
static void ShowTownnameDropdown(Window *w, int sel)
{
	typedef std::map<StringID, int, StringIDCompare> TownList;
	TownList townnames;

	/* Add and sort original townnames generators */
	for (int i = 0; i < _nb_orig_names; i++) townnames[STR_TOWNNAME_ORIGINAL_ENGLISH + i] = i;

	/* Add and sort newgrf townnames generators */
	for (int i = 0; i < _nb_grf_names; i++) townnames[_grf_names[i]] = _nb_orig_names + i;

	DropDownList *list = new DropDownList();
	for (TownList::iterator it = townnames.begin(); it != townnames.end(); it++) {
		list->push_back(new DropDownListStringItem((*it).first, (*it).second, !(_game_mode == GM_MENU || (*it).second == sel)));
	}

	ShowDropDownList(w, list, sel, GAMEOPT_TOWNNAME_BTN);
}

/**
 * Update/redraw the languages dropdown
 * @param w   the window the dropdown belongs to
 */
static void ShowLangDropdown(Window *w)
{
	typedef std::map<StringID, int, StringIDCompare> LangList;

	/* Sort language names */
	LangList langs;
	for (int i = 0; i < _dynlang.num; i++) langs[SPECSTR_LANGUAGE_START + i] = i;

	DropDownList *list = new DropDownList();
	for (LangList::iterator it = langs.begin(); it != langs.end(); it++) {
		list->push_back(new DropDownListStringItem((*it).first, (*it).second, false));
	}

	ShowDropDownList(w, list, _dynlang.curr, GAMEOPT_LANG_BTN);
}

static void ShowCustCurrency();

static void GameOptionsWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			int i;
			StringID str = STR_02BE_DEFAULT;

			w->SetWidgetDisabledState(GAMEOPT_VEHICLENAME_SAVE, !(_vehicle_design_names & 1));
			if (!w->IsWidgetDisabled(GAMEOPT_VEHICLENAME_SAVE)) str = STR_02BF_CUSTOM;
			SetDParam(0, str);
			SetDParam(1, _currency_specs[_opt_ptr->currency].name);
			SetDParam(2, STR_UNITS_IMPERIAL + _opt_ptr->units);
			SetDParam(3, STR_02E9_DRIVE_ON_LEFT + _opt_ptr->road_side);
			SetDParam(4, TownName(_opt_ptr->town_name));
			SetDParam(5, _autosave_dropdown[_opt_ptr->autosave]);
			SetDParam(6, SPECSTR_LANGUAGE_START + _dynlang.curr);
			i = GetCurRes();
			SetDParam(7, i == _num_resolutions ? STR_RES_OTHER : SPECSTR_RESOLUTION_START + i);
			SetDParam(8, SPECSTR_SCREENSHOT_START + _cur_screenshot_format);
			w->SetWidgetLoweredState(GAMEOPT_FULLSCREEN, _fullscreen);

			DrawWindowWidgets(w);
			DrawString(20, 175, STR_OPTIONS_FULLSCREEN, TC_FROMSTRING); // fullscreen
		} break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case GAMEOPT_CURRENCY_BTN: /* Setup currencies dropdown */
					ShowDropDownMenu(w, BuildCurrencyDropdown(), _opt_ptr->currency, GAMEOPT_CURRENCY_BTN, _game_mode == GM_MENU ? 0 : ~GetMaskOfAllowedCurrencies(), 0);
					break;

				case GAMEOPT_DISTANCE_BTN: /* Setup distance unit dropdown */
					ShowDropDownMenu(w, _units_dropdown, _opt_ptr->units, GAMEOPT_DISTANCE_BTN, 0, 0);
					break;

				case GAMEOPT_ROADSIDE_BTN: { /* Setup road-side dropdown */
					int i = 0;

					/* You can only change the drive side if you are in the menu or ingame with
					 * no vehicles present. In a networking game only the server can change it */
					if ((_game_mode != GM_MENU && RoadVehiclesAreBuilt()) || (_networking && !_network_server))
						i = (-1) ^ (1 << _opt_ptr->road_side); // disable the other value

					ShowDropDownMenu(w, _driveside_dropdown, _opt_ptr->road_side, GAMEOPT_ROADSIDE_BTN, i, 0);
				} break;

				case GAMEOPT_TOWNNAME_BTN: /* Setup townname dropdown */
					ShowTownnameDropdown(w, _opt_ptr->town_name);
					break;

				case GAMEOPT_AUTOSAVE_BTN: /* Setup autosave dropdown */
					ShowDropDownMenu(w, _autosave_dropdown, _opt_ptr->autosave, GAMEOPT_AUTOSAVE_BTN, 0, 0);
					break;

				case GAMEOPT_VEHICLENAME_BTN: /* Setup customized vehicle-names dropdown */
					ShowDropDownMenu(w, _designnames_dropdown, (_vehicle_design_names & 1) ? 1 : 0, GAMEOPT_VEHICLENAME_BTN, (_vehicle_design_names & 2) ? 0 : 2, 0);
					break;

				case GAMEOPT_VEHICLENAME_SAVE: /* Save customized vehicle-names to disk */
					break;  // not implemented

				case GAMEOPT_LANG_BTN: /* Setup interface language dropdown */
					ShowLangDropdown(w);
					break;

				case GAMEOPT_RESOLUTION_BTN: /* Setup resolution dropdown */
					ShowDropDownMenu(w, BuildDynamicDropdown(SPECSTR_RESOLUTION_START, _num_resolutions), GetCurRes(), GAMEOPT_RESOLUTION_BTN, 0, 0);
					break;

				case GAMEOPT_FULLSCREEN: /* Click fullscreen on/off */
					/* try to toggle full-screen on/off */
					if (!ToggleFullScreen(!_fullscreen)) {
						ShowErrorMessage(INVALID_STRING_ID, STR_FULLSCREEN_FAILED, 0, 0);
					}
					w->SetWidgetLoweredState(GAMEOPT_FULLSCREEN, _fullscreen);
					SetWindowDirty(w);
					break;

				case GAMEOPT_SCREENSHOT_BTN: /* Setup screenshot format dropdown */
					ShowDropDownMenu(w, BuildDynamicDropdown(SPECSTR_SCREENSHOT_START, _num_screenshot_formats), _cur_screenshot_format, GAMEOPT_SCREENSHOT_BTN, 0, 0);
					break;
			}
			break;

		case WE_DROPDOWN_SELECT:
			switch (e->we.dropdown.button) {
				case GAMEOPT_VEHICLENAME_BTN: /* Vehicle design names */
					if (e->we.dropdown.index == 0) {
						DeleteCustomEngineNames();
						MarkWholeScreenDirty();
					} else if (!(_vehicle_design_names & 1)) {
						LoadCustomEngineNames();
						MarkWholeScreenDirty();
					}
					break;

				case GAMEOPT_CURRENCY_BTN: /* Currency */
					if (e->we.dropdown.index == CUSTOM_CURRENCY_ID) ShowCustCurrency();
					_opt_ptr->currency = e->we.dropdown.index;
					MarkWholeScreenDirty();
					break;

				case GAMEOPT_DISTANCE_BTN: /* Measuring units */
					_opt_ptr->units = e->we.dropdown.index;
					MarkWholeScreenDirty();
					break;

				case GAMEOPT_ROADSIDE_BTN: /* Road side */
					if (_opt_ptr->road_side != e->we.dropdown.index) { // only change if setting changed
						DoCommandP(0, e->we.dropdown.index, 0, NULL, CMD_SET_ROAD_DRIVE_SIDE | CMD_MSG(STR_00B4_CAN_T_DO_THIS));
						MarkWholeScreenDirty();
					}
					break;

				case GAMEOPT_TOWNNAME_BTN: /* Town names */
					if (_game_mode == GM_MENU) {
						_opt_ptr->town_name = e->we.dropdown.index;
						InvalidateWindow(WC_GAME_OPTIONS, 0);
					}
					break;

				case GAMEOPT_AUTOSAVE_BTN: /* Autosave options */
					_opt.autosave = _opt_newgame.autosave = e->we.dropdown.index;
					SetWindowDirty(w);
					break;

				case GAMEOPT_LANG_BTN: /* Change interface language */
					ReadLanguagePack(e->we.dropdown.index);
					CheckForMissingGlyphsInLoadedLanguagePack();
					UpdateAllStationVirtCoord();
					UpdateAllWaypointSigns();
					MarkWholeScreenDirty();
					break;

				case GAMEOPT_RESOLUTION_BTN: /* Change resolution */
					if (e->we.dropdown.index < _num_resolutions && ChangeResInGame(_resolutions[e->we.dropdown.index][0], _resolutions[e->we.dropdown.index][1]))
						SetWindowDirty(w);
					break;

				case GAMEOPT_SCREENSHOT_BTN: /* Change screenshot format */
					SetScreenshotFormat(e->we.dropdown.index);
					SetWindowDirty(w);
					break;
			}
			break;

		case WE_DESTROY:
			DeleteWindowById(WC_CUSTOM_CURRENCY, 0);
			break;
	}

}

/** Change the side of the road vehicles drive on (server only).
 * @param tile unused
 * @param flags operation to perform
 * @param p1 the side of the road; 0 = left side and 1 = right side
 * @param p2 unused
 */
CommandCost CmdSetRoadDriveSide(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	/* Check boundaries and you can only change this if NO vehicles have been built yet,
	 * except in the intro-menu where of course it's always possible to do so. */
	if (p1 > 1 || (_game_mode != GM_MENU && RoadVehiclesAreBuilt())) return CMD_ERROR;

	if (flags & DC_EXEC) {
		_opt_ptr->road_side = p1;
		InvalidateWindow(WC_GAME_OPTIONS, 0);
	}
	return CommandCost();
}

static const Widget _game_options_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   369,     0,    13, STR_00B1_GAME_OPTIONS,             STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   369,    14,   238, 0x0,                               STR_NULL},
{      WWT_FRAME,   RESIZE_NONE,    14,    10,   179,    20,    55, STR_02E0_CURRENCY_UNITS,           STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,    14,    20,   169,    34,    45, STR_02E1,                          STR_02E2_CURRENCY_UNITS_SELECTION},
{      WWT_FRAME,   RESIZE_NONE,    14,   190,   359,    20,    55, STR_MEASURING_UNITS,               STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,    14,   200,   349,    34,    45, STR_02E4,                          STR_MEASURING_UNITS_SELECTION},
{      WWT_FRAME,   RESIZE_NONE,    14,    10,   179,    62,    97, STR_02E6_ROAD_VEHICLES,            STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,    14,    20,   169,    76,    87, STR_02E7,                          STR_02E8_SELECT_SIDE_OF_ROAD_FOR},
{      WWT_FRAME,   RESIZE_NONE,    14,   190,   359,    62,    97, STR_02EB_TOWN_NAMES,               STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,    14,   200,   349,    76,    87, STR_02EC,                          STR_02ED_SELECT_STYLE_OF_TOWN_NAMES},
{      WWT_FRAME,   RESIZE_NONE,    14,    10,   179,   104,   139, STR_02F4_AUTOSAVE,                 STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,    14,    20,   169,   118,   129, STR_02F5,                          STR_02F6_SELECT_INTERVAL_BETWEEN},

{      WWT_FRAME,   RESIZE_NONE,    14,    10,   359,   194,   228, STR_02BC_VEHICLE_DESIGN_NAMES,     STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,    14,    20,   119,   207,   218, STR_02BD,                          STR_02C1_VEHICLE_DESIGN_NAMES_SELECTION},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   130,   349,   207,   218, STR_02C0_SAVE_CUSTOM_NAMES,        STR_02C2_SAVE_CUSTOMIZED_VEHICLE},

{      WWT_FRAME,   RESIZE_NONE,    14,   190,   359,   104,   139, STR_OPTIONS_LANG,                  STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,    14,   200,   349,   118,   129, STR_OPTIONS_LANG_CBO,              STR_OPTIONS_LANG_TIP},

{      WWT_FRAME,   RESIZE_NONE,    14,    10,   179,   146,   190, STR_OPTIONS_RES,                   STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,    14,    20,   169,   160,   171, STR_OPTIONS_RES_CBO,               STR_OPTIONS_RES_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   149,   169,   176,   184, STR_EMPTY,                         STR_OPTIONS_FULLSCREEN_TIP},

{      WWT_FRAME,   RESIZE_NONE,    14,   190,   359,   146,   190, STR_OPTIONS_SCREENSHOT_FORMAT,     STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,    14,   200,   349,   160,   171, STR_OPTIONS_SCREENSHOT_FORMAT_CBO, STR_OPTIONS_SCREENSHOT_FORMAT_TIP},

{   WIDGETS_END},
};

static const WindowDesc _game_options_desc = {
	WDP_CENTER, WDP_CENTER, 370, 239, 370, 239,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_game_options_widgets,
	GameOptionsWndProc
};


void ShowGameOptions()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	AllocateWindowDesc(&_game_options_desc);
}

struct GameSettingData {
	int16 min;
	int16 max;
	int16 step;
	StringID str;
};

static const GameSettingData _game_setting_info[] = {
	{  0,   7,  1, STR_NULL},
	{  0,   3,  1, STR_6830_IMMEDIATE},
	{  0,   3,  1, STR_NUM_VERY_LOW},
	{  0,   4,  1, STR_NONE},
	{100, 500, 50, STR_NULL},
	{  2,   4,  1, STR_NULL},
	{  0,   2,  1, STR_6820_LOW},
	{  0,   4,  1, STR_681B_VERY_SLOW},
	{  0,   2,  1, STR_6820_LOW},
	{  0,   2,  1, STR_6823_NONE},
	{  0,   3,  1, STR_6826_X1_5},
	{  0,   2,  1, STR_6820_LOW},
	{  0,   3,  1, STR_682A_VERY_FLAT},
	{  0,   3,  1, STR_VERY_LOW},
	{  0,   1,  1, STR_682E_STEADY},
	{  0,   1,  1, STR_6834_AT_END_OF_LINE_AND_AT_STATIONS},
	{  0,   1,  1, STR_6836_OFF},
	{  0,   2,  1, STR_PERMISSIVE},
};

/*
 * A: competitors
 * B: start time in months / 3
 * C: town count (3 = high, 0 = very low)
 * D: industry count (4 = high, 0 = none)
 * E: inital loan / 1000 (in GBP)
 * F: interest rate
 * G: running costs (0 = low, 2 = high)
 * H: construction speed of competitors (0 = very slow, 4 = very fast)
 * I: intelligence (0-2)
 * J: breakdowns (0 = off, 2 = normal)
 * K: subsidy multiplier (0 = 1.5, 3 = 4.0)
 * L: construction cost (0-2)
 * M: terrain type (0 = very flat, 3 = mountainous)
 * N: amount of water (0 = very low, 3 = high)
 * O: economy (0 = steady, 1 = fluctuating)
 * P: Train reversing (0 = end of line + stations, 1 = end of line)
 * Q: disasters
 * R: area restructuring (0 = permissive, 2 = hostile)
 */
static const GDType _default_game_diff[3][GAME_DIFFICULTY_NUM] = { /*
	 A, B, C, D,   E, F, G, H, I, J, K, L, M, N, O, P, Q, R*/
	{2, 2, 2, 4, 300, 2, 0, 2, 0, 1, 2, 0, 1, 0, 0, 0, 0, 0}, ///< easy
	{4, 1, 2, 3, 150, 3, 1, 3, 1, 2, 1, 1, 2, 1, 1, 1, 1, 1}, ///< medium
	{7, 0, 3, 3, 100, 4, 1, 3, 2, 2, 0, 2, 3, 2, 1, 1, 1, 2}, ///< hard
};

void SetDifficultyLevel(int mode, GameOptions *gm_opt)
{
	int i;
	assert(mode <= 3);

	gm_opt->diff_level = mode;
	if (mode != 3) { // not custom
		for (i = 0; i != GAME_DIFFICULTY_NUM; i++)
			((GDType*)&gm_opt->diff)[i] = _default_game_diff[mode][i];
	}
}

/**
 * Checks the difficulty levels read from the configuration and
 * forces them to be correct when invalid.
 */
void CheckDifficultyLevels()
{
	if (_opt_newgame.diff_level != 3) {
		SetDifficultyLevel(_opt_newgame.diff_level, &_opt_newgame);
	} else {
		for (uint i = 0; i < GAME_DIFFICULTY_NUM; i++) {
			GDType *diff = ((GDType*)&_opt_newgame.diff) + i;
			*diff = Clamp(*diff, _game_setting_info[i].min, _game_setting_info[i].max);
			*diff -= *diff % _game_setting_info[i].step;
		}
	}
}

extern void StartupEconomy();

enum {
	GAMEDIFF_WND_TOP_OFFSET = 45,
	GAMEDIFF_WND_ROWSIZE    = 9
};

/* Temporary holding place of values in the difficulty window until 'Save' is clicked */
static GameOptions _opt_mod_temp;
// 0x383E = (1 << 13) | (1 << 12) | (1 << 11) | (1 << 5) | (1 << 4) | (1 << 3) | (1 << 2) | (1 << 1)
#define DIFF_INGAME_DISABLED_BUTTONS 0x383E

#define NO_SETTINGS_BUTTON 0xFF

/** Carriage for the game settings window data */
struct difficulty_d {
	bool clicked_increase;
	uint8 clicked_button;
	uint8 timeout;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(difficulty_d));

/* Names of the game difficulty settings window */
enum GameDifficultyWidgets {
	GDW_CLOSEBOX = 0,
	GDW_CAPTION,
	GDW_UPPER_BG,
	GDW_LVL_EASY,
	GDW_LVL_MEDIUM,
	GDW_LVL_HARD,
	GDW_LVL_CUSTOM,
	GDW_HIGHSCORE,
	GDW_SETTING_BG,
	GDW_LOWER_BG,
	GDW_ACCEPT,
	GDW_CANCEL,
};

static void GameDifficultyWndProc(Window *w, WindowEvent *e)
{
	difficulty_d *diffic_d = &WP(w, difficulty_d);
	switch (e->event) {
		case WE_CREATE:
			diffic_d->clicked_increase = false;
			diffic_d->clicked_button = NO_SETTINGS_BUTTON;
			diffic_d->timeout = 0;
			/* Hide the closebox to make sure that the user aborts or confirms his changes */
			w->HideWidget(GDW_CLOSEBOX);
			w->widget[GDW_CAPTION].left = 0;
			/* Setup disabled buttons when creating window
			 * disable all other difficulty buttons during gameplay except for 'custom' */
			w->SetWidgetsDisabledState(_game_mode == GM_NORMAL,
				GDW_LVL_EASY,
				GDW_LVL_MEDIUM,
				GDW_LVL_HARD,
				GDW_LVL_CUSTOM,
				WIDGET_LIST_END);
			w->SetWidgetDisabledState(GDW_HIGHSCORE, _game_mode == GM_EDITOR || _networking); // highscore chart in multiplayer
			w->SetWidgetDisabledState(GDW_ACCEPT, _networking && !_network_server); // Save-button in multiplayer (and if client)
			w->LowerWidget(GDW_LVL_EASY + _opt_mod_temp.diff_level);
			break;

		case WE_PAINT: {
			DrawWindowWidgets(w);

			/* XXX - Disabled buttons in normal gameplay or during muliplayer as non server.
			 *       Bitshifted for each button to see if that bit is set. If it is set, the
			 *       button is disabled */
			uint32 disabled = 0;
			if (_networking && !_network_server) {
				disabled = MAX_UVALUE(uint32); // Disable all
			} else if (_game_mode == GM_NORMAL) {
				disabled = DIFF_INGAME_DISABLED_BUTTONS;
			}

			int value;
			int y = GAMEDIFF_WND_TOP_OFFSET;
			for (uint i = 0; i != GAME_DIFFICULTY_NUM; i++) {
				const GameSettingData *gsd = &_game_setting_info[i];
				value = ((GDType*)&_opt_mod_temp.diff)[i];

				DrawArrowButtons(5, y, 3,
						(diffic_d->clicked_button == i) ? 1 + !!diffic_d->clicked_increase : 0,
						!(HasBit(disabled, i) || gsd->min == value),
						!(HasBit(disabled, i) || gsd->max == value));

				value += _game_setting_info[i].str;
				if (i == 4) value *= 1000; // XXX - handle currency option
				SetDParam(0, value);
				DrawString(30, y, STR_6805_MAXIMUM_NO_COMPETITORS + i, TC_FROMSTRING);

				y += GAMEDIFF_WND_ROWSIZE + 2; // space items apart a bit
			}
		} break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case GDW_SETTING_BG: { /* Difficulty settings widget, decode click */
					/* Don't allow clients to make any changes */
					if (_networking && !_network_server) return;

					const int x = e->we.click.pt.x - 5;
					if (!IsInsideMM(x, 0, 21)) // Button area
						return;

					const int y = e->we.click.pt.y - GAMEDIFF_WND_TOP_OFFSET;
					if (y < 0) return;

					/* Get button from Y coord. */
					const uint8 btn = y / (GAMEDIFF_WND_ROWSIZE + 2);
					if (btn >= GAME_DIFFICULTY_NUM || y % (GAMEDIFF_WND_ROWSIZE + 2) >= 9)
						return;

					/* Clicked disabled button? */
					if (_game_mode == GM_NORMAL && HasBit(DIFF_INGAME_DISABLED_BUTTONS, btn))
						return;

					diffic_d->timeout = 5;

					int16 val = ((GDType*)&_opt_mod_temp.diff)[btn];

					const GameSettingData *info = &_game_setting_info[btn]; // get information about the difficulty setting
					if (x >= 10) {
						/* Increase button clicked */
						val = min(val + info->step, info->max);
						diffic_d->clicked_increase = true;
					} else {
						/* Decrease button clicked */
						val -= info->step;
						val = max(val,  info->min);
						diffic_d->clicked_increase = false;
					}
					diffic_d->clicked_button = btn;

					/* save value in temporary variable */
					((GDType*)&_opt_mod_temp.diff)[btn] = val;
					w->RaiseWidget(GDW_LVL_EASY + _opt_mod_temp.diff_level);
					SetDifficultyLevel(3, &_opt_mod_temp); // set difficulty level to custom
					w->LowerWidget(GDW_LVL_CUSTOM);
					SetWindowDirty(w);
				} break;

				case GDW_LVL_EASY:
				case GDW_LVL_MEDIUM:
				case GDW_LVL_HARD:
				case GDW_LVL_CUSTOM:
					/* temporarily change difficulty level */
					w->RaiseWidget(GDW_LVL_EASY + _opt_mod_temp.diff_level);
					SetDifficultyLevel(e->we.click.widget - GDW_LVL_EASY, &_opt_mod_temp);
					w->LowerWidget(GDW_LVL_EASY + _opt_mod_temp.diff_level);
					SetWindowDirty(w);
					break;

				case GDW_HIGHSCORE: // Highscore Table
					ShowHighscoreTable(_opt_mod_temp.diff_level, -1);
					break;

				case GDW_ACCEPT: { // Save button - save changes
					GDType btn, val;
					for (btn = 0; btn != GAME_DIFFICULTY_NUM; btn++) {
						val = ((GDType*)&_opt_mod_temp.diff)[btn];
						/* if setting has changed, change it */
						if (val != ((GDType*)&_opt_ptr->diff)[btn])
							DoCommandP(0, btn, val, NULL, CMD_CHANGE_DIFFICULTY_LEVEL);
					}
					DoCommandP(0, UINT_MAX, _opt_mod_temp.diff_level, NULL, CMD_CHANGE_DIFFICULTY_LEVEL);
					DeleteWindow(w);
					/* If we are in the editor, we should reload the economy.
					 * This way when you load a game, the max loan and interest rate
					 * are loaded correctly. */
					if (_game_mode == GM_EDITOR) StartupEconomy();
					break;
				}

				case GDW_CANCEL: // Cancel button - close window, abandon changes
					DeleteWindow(w);
					break;
			} break;

		case WE_MOUSELOOP: /* Handle the visual 'clicking' of the buttons */
			if (diffic_d->timeout != 0) {
				diffic_d->timeout--;
				if (diffic_d->timeout == 0) diffic_d->clicked_button = NO_SETTINGS_BUTTON;
				SetWindowDirty(w);
			}
			break;
	}
}
#undef DIFF_INGAME_DISABLED_BUTTONS

/* Widget definition for the game difficulty settings window */
static const Widget _game_difficulty_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    10,     0,    10,     0,    13, STR_00C5,                     STR_018B_CLOSE_WINDOW},           // GDW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,    10,    11,   369,     0,    13, STR_6800_DIFFICULTY_LEVEL,    STR_018C_WINDOW_TITLE_DRAG_THIS}, // GDW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,    10,     0,   369,    14,    41, 0x0,                          STR_NULL},                        // GDW_UPPER_BG
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,    10,    96,    16,    27, STR_6801_EASY,                STR_NULL},                        // GDW_LVL_EASY
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,    97,   183,    16,    27, STR_6802_MEDIUM,              STR_NULL},                        // GDW_LVL_MEDIUM
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,   184,   270,    16,    27, STR_6803_HARD,                STR_NULL},                        // GDW_LVL_HARD
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,   271,   357,    16,    27, STR_6804_CUSTOM,              STR_NULL},                        // GDW_LVL_CUSTOM
{    WWT_TEXTBTN,   RESIZE_NONE,     6,    10,   357,    28,    39, STR_6838_SHOW_HI_SCORE_CHART, STR_NULL},                        // GDW_HIGHSCORE
{      WWT_PANEL,   RESIZE_NONE,    10,     0,   369,    42,   262, 0x0,                          STR_NULL},                        // GDW_SETTING_BG
{      WWT_PANEL,   RESIZE_NONE,    10,     0,   369,   263,   278, 0x0,                          STR_NULL},                        // GDW_LOWER_BG
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,   105,   185,   265,   276, STR_OPTIONS_SAVE_CHANGES,     STR_NULL},                        // GDW_ACCEPT
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,   186,   266,   265,   276, STR_012E_CANCEL,              STR_NULL},                        // GDW_CANCEL
{   WIDGETS_END},
};

/* Window definition for the game difficulty settings window */
static const WindowDesc _game_difficulty_desc = {
	WDP_CENTER, WDP_CENTER, 370, 279, 370, 279,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_game_difficulty_widgets,
	GameDifficultyWndProc
};

void ShowGameDifficulty()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	/* Copy current settings (ingame or in intro) to temporary holding place
	 * change that when setting stuff, copy back on clicking 'OK' */
	_opt_mod_temp = *_opt_ptr;
	AllocateWindowDesc(&_game_difficulty_desc);
}

static const char *_patches_ui[] = {
	"vehicle_speed",
	"status_long_date",
	"show_finances",
	"autoscroll",
	"reverse_scroll",
	"smooth_scroll",
	"errmsg_duration",
	"toolbar_pos",
	"measure_tooltip",
	"window_snap_radius",
	"population_in_label",
	"link_terraform_toolbar",
	"liveries",
	"prefer_teamchat",
	/* While the horizontal scrollwheel scrolling is written as general code, only
	 *  the cocoa (OSX) driver generates input for it.
	 *  Since it's also able to completely disable the scrollwheel will we display it on all platforms anyway */
	"scrollwheel_scrolling",
	"scrollwheel_multiplier",
#ifdef __APPLE__
	/* We might need to emulate a right mouse button on mac */
	"right_mouse_btn_emulation",
#endif
	"pause_on_newgame",
	"advanced_vehicle_list",
	"loading_indicators",
	"timetable_in_ticks",
	"default_rail_type",
	"always_build_infrastructure",
};

static const char *_patches_construction[] = {
	"build_on_slopes",
	"autoslope",
	"extra_dynamite",
	"longbridges",
	"signal_side",
	"always_small_airport",
	"enable_signal_gui",
	"drag_signals_density",
	"oil_refinery_limit",
	"semaphore_build_before",
};

static const char *_patches_stations[] = {
	"join_stations",
	"improved_load",
	"selectgoods",
	"new_nonstop",
	"nonuniform_stations",
	"station_spread",
	"serviceathelipad",
	"modified_catchment",
	"gradual_loading",
	"road_stop_on_town_road",
	"adjacent_stations",
};

static const char *_patches_economy[] = {
	"inflation",
	"raw_industry_construction",
	"multiple_industry_per_town",
	"same_industry_close",
	"bribe",
	"exclusive_rights",
	"give_money",
	"colored_news_year",
	"ending_year",
	"smooth_economy",
	"allow_shares",
	"town_layout",
	"mod_road_rebuild",
	"town_growth_rate",
	"larger_towns",
	"initial_city_size",
};

static const char *_patches_ai[] = {
	"ainew_active",
	"ai_in_multiplayer",
	"ai_disable_veh_train",
	"ai_disable_veh_roadveh",
	"ai_disable_veh_aircraft",
	"ai_disable_veh_ship",
};

static const char *_patches_vehicles[] = {
	"realistic_acceleration",
	"forbid_90_deg",
	"mammoth_trains",
	"gotodepot",
	"roadveh_queue",
	"pathfinder_for_trains",
	"pathfinder_for_roadvehs",
	"pathfinder_for_ships",
	"train_income_warn",
	"order_review_system",
	"never_expire_vehicles",
	"lost_train_warn",
	"autorenew",
	"autorenew_months",
	"autorenew_money",
	"max_trains",
	"max_roadveh",
	"max_aircraft",
	"max_ships",
	"servint_ispercent",
	"servint_trains",
	"servint_roadveh",
	"servint_ships",
	"servint_aircraft",
	"no_servicing_if_no_breakdowns",
	"wagon_speed_limits",
	"disable_elrails",
	"freight_trains",
	"plane_speed",
	"timetabling",
	"dynamic_engines",
};

struct PatchEntry {
	const SettingDesc *setting;
	uint index;
};

struct PatchPage {
	const char **names;
	PatchEntry *entries;
	byte num;
};

/* PatchPage holds the categories, the number of elements in each category
 * and (in NULL) a dynamic array of settings based on the string-representations
 * of the settings. This way there is no worry about indeces, and such */
static PatchPage _patches_page[] = {
	{_patches_ui,           NULL, lengthof(_patches_ui)},
	{_patches_construction, NULL, lengthof(_patches_construction)},
	{_patches_vehicles,     NULL, lengthof(_patches_vehicles)},
	{_patches_stations,     NULL, lengthof(_patches_stations)},
	{_patches_economy,      NULL, lengthof(_patches_economy)},
	{_patches_ai,           NULL, lengthof(_patches_ai)},
};

enum PatchesSelectionWidgets {
	PATCHSEL_OPTIONSPANEL = 3,
	PATCHSEL_INTERFACE,
	PATCHSEL_CONSTRUCTION,
	PATCHSEL_VEHICLES,
	PATCHSEL_STATIONS,
	PATCHSEL_ECONOMY,
	PATCHSEL_COMPETITORS
};

/** The main patches window. Shows a number of categories on top and
 * a selection of patches in that category.
 * Uses WP(w, def_d) macro - data_1, data_2, data_3 */
static void PatchesSelectionWndProc(Window *w, WindowEvent *e)
{
	static Patches *patches_ptr;
	static int patches_max = 0;

	switch (e->event) {
		case WE_CREATE: {
			static bool first_time = true;

			patches_ptr = (_game_mode == GM_MENU) ? &_patches_newgame : &_patches;

			/* Build up the dynamic settings-array only once per OpenTTD session */
			if (first_time) {
				PatchPage *page;
				for (page = &_patches_page[0]; page != endof(_patches_page); page++) {
					uint i;

					if (patches_max < page->num) patches_max = page->num;

					page->entries = MallocT<PatchEntry>(page->num);
					for (i = 0; i != page->num; i++) {
						uint index;
						const SettingDesc *sd = GetPatchFromName(page->names[i], &index);
						assert(sd != NULL);

						page->entries[i].setting = sd;
						page->entries[i].index = index;
					}
				}
				first_time = false;
			}

			/* Resize the window to fit the largest patch tab */
			ResizeWindowForWidget(w, PATCHSEL_OPTIONSPANEL, 0, patches_max * 11);

			/* Recentre the window for the new size */
			w->top = w->top - (patches_max * 11) / 2;

			w->LowerWidget(4);
		} break;

		case WE_PAINT: {
			int x, y;
			const PatchPage *page = &_patches_page[WP(w, def_d).data_1];
			uint i;

			/* Set up selected category */
			DrawWindowWidgets(w);

			x = 5;
			y = 47;
			for (i = 0; i != page->num; i++) {
				const SettingDesc *sd = page->entries[i].setting;
				const SettingDescBase *sdb = &sd->desc;
				const void *var = GetVariableAddress(patches_ptr, &sd->save);
				bool editable = true;
				bool disabled = false;

				// We do not allow changes of some items when we are a client in a networkgame
				if (!(sd->save.conv & SLF_NETWORK_NO) && _networking && !_network_server) editable = false;
				if ((sdb->flags & SGF_NETWORK_ONLY) && !_networking) editable = false;
				if ((sdb->flags & SGF_NO_NETWORK) && _networking) editable = false;

				if (sdb->cmd == SDT_BOOLX) {
					static const int _bool_ctabs[2][2] = {{9, 4}, {7, 6}};
					/* Draw checkbox for boolean-value either on/off */
					bool on = (*(bool*)var);

					DrawFrameRect(x, y, x + 19, y + 8, _bool_ctabs[!!on][!!editable], on ? FR_LOWERED : FR_NONE);
					SetDParam(0, on ? STR_CONFIG_PATCHES_ON : STR_CONFIG_PATCHES_OFF);
				} else {
					int32 value;

					value = (int32)ReadValue(var, sd->save.conv);

					/* Draw [<][>] boxes for settings of an integer-type */
					DrawArrowButtons(x, y, 3, WP(w, def_d).data_2 - (i * 2), (editable && value != sdb->min), (editable && value != sdb->max));

					disabled = (value == 0) && (sdb->flags & SGF_0ISDISABLED);
					if (disabled) {
						SetDParam(0, STR_CONFIG_PATCHES_DISABLED);
					} else {
						if (sdb->flags & SGF_CURRENCY) {
							SetDParam(0, STR_CONFIG_PATCHES_CURRENCY);
						} else if (sdb->flags & SGF_MULTISTRING) {
							SetDParam(0, sdb->str + value + 1);
						} else {
							SetDParam(0, (sdb->flags & SGF_NOCOMMA) ? STR_CONFIG_PATCHES_INT32 : STR_7024);
						}
						SetDParam(1, value);
					}
				}
				DrawString(30, y, (sdb->str) + disabled, TC_FROMSTRING);
				y += 11;
			}
		} break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case PATCHSEL_OPTIONSPANEL: {
					const PatchPage *page = &_patches_page[WP(w, def_d).data_1];
					const SettingDesc *sd;
					void *var;
					int32 value;
					int x, y;
					byte btn;

					y = e->we.click.pt.y - 46 - 1;
					if (y < 0) return;

					x = e->we.click.pt.x - 5;
					if (x < 0) return;

					btn = y / 11;
					if (y % 11 > 9) return;
					if (btn >= page->num) return;

					sd = page->entries[btn].setting;

					/* return if action is only active in network, or only settable by server */
					if (!(sd->save.conv & SLF_NETWORK_NO) && _networking && !_network_server) return;
					if ((sd->desc.flags & SGF_NETWORK_ONLY) && !_networking) return;
					if ((sd->desc.flags & SGF_NO_NETWORK) && _networking) return;

					var = GetVariableAddress(patches_ptr, &sd->save);
					value = (int32)ReadValue(var, sd->save.conv);

					/* clicked on the icon on the left side. Either scroller or bool on/off */
					if (x < 21) {
						const SettingDescBase *sdb = &sd->desc;
						int32 oldvalue = value;

						switch (sdb->cmd) {
						case SDT_BOOLX: value ^= 1; break;
						case SDT_NUMX: {
							/* Add a dynamic step-size to the scroller. In a maximum of
							 * 50-steps you should be able to get from min to max,
							 * unless specified otherwise in the 'interval' variable
							 * of the current patch. */
							uint32 step = (sdb->interval == 0) ? ((sdb->max - sdb->min) / 50) : sdb->interval;
							if (step == 0) step = 1;

							// don't allow too fast scrolling
							if ((w->flags4 & WF_TIMEOUT_MASK) > 2 << WF_TIMEOUT_SHL) {
								_left_button_clicked = false;
								return;
							}

							/* Increase or decrease the value and clamp it to extremes */
							if (x >= 10) {
								value += step;
								if (value > sdb->max) value = sdb->max;
							} else {
								value -= step;
								if (value < sdb->min) value = (sdb->flags & SGF_0ISDISABLED) ? 0 : sdb->min;
							}

							/* Set up scroller timeout for numeric values */
							if (value != oldvalue && !(sd->desc.flags & SGF_MULTISTRING)) {
								WP(w, def_d).data_2 = btn * 2 + 1 + ((x >= 10) ? 1 : 0);
								w->flags4 |= 5 << WF_TIMEOUT_SHL;
								_left_button_clicked = false;
							}
						} break;
						default: NOT_REACHED();
						}

						if (value != oldvalue) {
							SetPatchValue(page->entries[btn].index, patches_ptr, value);
							SetWindowDirty(w);
						}
					} else {
						/* only open editbox for types that its sensible for */
						if (sd->desc.cmd != SDT_BOOLX && !(sd->desc.flags & SGF_MULTISTRING)) {
							/* Show the correct currency-translated value */
							if (sd->desc.flags & SGF_CURRENCY) value *= _currency->rate;

							WP(w, def_d).data_3 = btn;
							SetDParam(0, value);
							ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_CONFIG_PATCHES_QUERY_CAPT, 10, 100, w, CS_NUMERAL);
						}
					}
				} break;

				case PATCHSEL_INTERFACE: case PATCHSEL_CONSTRUCTION: case PATCHSEL_VEHICLES:
				case PATCHSEL_STATIONS:  case PATCHSEL_ECONOMY:      case PATCHSEL_COMPETITORS:
					w->RaiseWidget(WP(w, def_d).data_1 + PATCHSEL_INTERFACE);
					WP(w, def_d).data_1 = e->we.click.widget - PATCHSEL_INTERFACE;
					w->LowerWidget(WP(w, def_d).data_1 + PATCHSEL_INTERFACE);
					DeleteWindowById(WC_QUERY_STRING, 0);
					SetWindowDirty(w);
					break;
			}
			break;

		case WE_TIMEOUT:
			WP(w, def_d).data_2 = 0;
			SetWindowDirty(w);
			break;

		case WE_ON_EDIT_TEXT:
			if (e->we.edittext.str != NULL) {
				const PatchEntry *pe = &_patches_page[WP(w, def_d).data_1].entries[WP(w, def_d).data_3];
				const SettingDesc *sd = pe->setting;
				int32 value = atoi(e->we.edittext.str);

				/* Save the correct currency-translated value */
				if (sd->desc.flags & SGF_CURRENCY) value /= _currency->rate;

				SetPatchValue(pe->index, patches_ptr, value);
				SetWindowDirty(w);
			}
			break;

		case WE_DESTROY:
			DeleteWindowById(WC_QUERY_STRING, 0);
			break;
	}
}

static const Widget _patches_selection_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    10,     0,    10,     0,    13, STR_00C5,                        STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    10,    11,   369,     0,    13, STR_CONFIG_PATCHES_CAPTION,      STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    10,     0,   369,    14,    41, 0x0,                             STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    10,     0,   369,    42,    50, 0x0,                             STR_NULL},

{    WWT_TEXTBTN,   RESIZE_NONE,     3,    10,    96,    16,    27, STR_CONFIG_PATCHES_GUI,          STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,     3,    97,   183,    16,    27, STR_CONFIG_PATCHES_CONSTRUCTION, STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,     3,   184,   270,    16,    27, STR_CONFIG_PATCHES_VEHICLES,     STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,     3,   271,   357,    16,    27, STR_CONFIG_PATCHES_STATIONS,     STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,     3,    10,    96,    28,    39, STR_CONFIG_PATCHES_ECONOMY,      STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,     3,    97,   183,    28,    39, STR_CONFIG_PATCHES_AI,           STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _patches_selection_desc = {
	WDP_CENTER, WDP_CENTER, 370, 51, 370, 51,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_patches_selection_widgets,
	PatchesSelectionWndProc,
};

void ShowPatchesSelection()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	AllocateWindowDesc(&_patches_selection_desc);
}


/**
 * Draw [<][>] boxes.
 * @param x the x position to draw
 * @param y the y position to draw
 * @param ctab the color of the buttons
 * @param state 0 = none clicked, 1 = first clicked, 2 = second clicked
 * @param clickable_left is the left button clickable?
 * @param clickable_right is the right button clickable?
 */
void DrawArrowButtons(int x, int y, int ctab, byte state, bool clickable_left, bool clickable_right)
{
	int color = (1 << PALETTE_MODIFIER_GREYOUT) | _colour_gradient[COLOUR_YELLOW][2];

	DrawFrameRect(x,      y + 1, x +  9, y + 9, ctab, (state == 1) ? FR_LOWERED : FR_NONE);
	DrawFrameRect(x + 10, y + 1, x + 19, y + 9, ctab, (state == 2) ? FR_LOWERED : FR_NONE);
	DrawStringCentered(x +  5, y + 1, STR_6819, TC_FROMSTRING); // [<]
	DrawStringCentered(x + 15, y + 1, STR_681A, TC_FROMSTRING); // [>]

	/* Grey out the buttons that aren't clickable */
	if (!clickable_left)
		GfxFillRect(x +  1, y + 1, x +  1 + 8, y + 8, color);
	if (!clickable_right)
		GfxFillRect(x + 11, y + 1, x + 11 + 8, y + 8, color);
}

/** These are not, strickly speaking, widget enums,
 *  since they have been changed as line coordinates.
 *  So, rather, they are more like order of appearance */
enum CustomCurrenciesWidgets {
	CUSTCURR_EXCHANGERATE = 0,
	CUSTCURR_SEPARATOR,
	CUSTCURR_PREFIX,
	CUSTCURR_SUFFIX,
	CUSTCURR_TO_EURO,
};

static char _str_separator[2];

static void CustCurrencyWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			int x;
			int y = 20;
			int clk = WP(w, def_d).data_1;
			DrawWindowWidgets(w);

			/* exchange rate */
			DrawArrowButtons(10, y, 3, GB(clk, 0, 2), true, true);
			SetDParam(0, 1);
			SetDParam(1, 1);
			DrawString(35, y + 1, STR_CURRENCY_EXCHANGE_RATE, TC_FROMSTRING);
			y += 12;

			/* separator */
			DrawFrameRect(10, y + 1, 29, y + 9, 0, GB(clk, 2, 2) ? FR_LOWERED : FR_NONE);
			x = DrawString(35, y + 1, STR_CURRENCY_SEPARATOR, TC_FROMSTRING);
			DoDrawString(_str_separator, x + 4, y + 1, TC_ORANGE);
			y += 12;

			/* prefix */
			DrawFrameRect(10, y + 1, 29, y + 9, 0, GB(clk, 4, 2) ? FR_LOWERED : FR_NONE);
			x = DrawString(35, y + 1, STR_CURRENCY_PREFIX, TC_FROMSTRING);
			DoDrawString(_custom_currency.prefix, x + 4, y + 1, TC_ORANGE);
			y += 12;

			/* suffix */
			DrawFrameRect(10, y + 1, 29, y + 9, 0, GB(clk, 6, 2) ? FR_LOWERED : FR_NONE);
			x = DrawString(35, y + 1, STR_CURRENCY_SUFFIX, TC_FROMSTRING);
			DoDrawString(_custom_currency.suffix, x + 4, y + 1, TC_ORANGE);
			y += 12;

			/* switch to euro */
			DrawArrowButtons(10, y, 3, GB(clk, 8, 2), true, true);
			SetDParam(0, _custom_currency.to_euro);
			DrawString(35, y + 1, (_custom_currency.to_euro != CF_NOEURO) ? STR_CURRENCY_SWITCH_TO_EURO : STR_CURRENCY_SWITCH_TO_EURO_NEVER, TC_FROMSTRING);
			y += 12;

			/* Preview */
			y += 12;
			SetDParam(0, 10000);
			DrawString(35, y + 1, STR_CURRENCY_PREVIEW, TC_FROMSTRING);
		} break;

		case WE_CLICK: {
			int line = (e->we.click.pt.y - 20) / 12;
			int len = 0;
			int x = e->we.click.pt.x;
			StringID str = 0;
			CharSetFilter afilter = CS_ALPHANUMERAL;

			switch (line) {
				case CUSTCURR_EXCHANGERATE:
					if (IsInsideMM(x, 10, 30)) { // clicked buttons
						if (x < 20) {
							if (_custom_currency.rate > 1) _custom_currency.rate--;
							WP(w, def_d).data_1 = 1 << (line * 2 + 0);
						} else {
							if (_custom_currency.rate < 5000) _custom_currency.rate++;
							WP(w, def_d).data_1 = 1 << (line * 2 + 1);
						}
					} else { // enter text
						SetDParam(0, _custom_currency.rate);
						str = STR_CONFIG_PATCHES_INT32;
						len = 4;
						afilter = CS_NUMERAL;
					}
					break;

				case CUSTCURR_SEPARATOR:
					if (IsInsideMM(x, 10, 30)) { // clicked button
						WP(w, def_d).data_1 = 1 << (line * 2 + 1);
					}
					str = BindCString(_str_separator);
					len = 1;
					break;

				case CUSTCURR_PREFIX:
					if (IsInsideMM(x, 10, 30)) { // clicked button
						WP(w, def_d).data_1 = 1 << (line * 2 + 1);
					}
					str = BindCString(_custom_currency.prefix);
					len = 12;
					break;

				case CUSTCURR_SUFFIX:
					if (IsInsideMM(x, 10, 30)) { // clicked button
						WP(w, def_d).data_1 = 1 << (line * 2 + 1);
					}
					str = BindCString(_custom_currency.suffix);
					len = 12;
					break;

				case CUSTCURR_TO_EURO:
					if (IsInsideMM(x, 10, 30)) { // clicked buttons
						if (x < 20) {
							_custom_currency.to_euro = (_custom_currency.to_euro <= 2000) ?
								CF_NOEURO : _custom_currency.to_euro - 1;
							WP(w, def_d).data_1 = 1 << (line * 2 + 0);
						} else {
							_custom_currency.to_euro =
								Clamp(_custom_currency.to_euro + 1, 2000, MAX_YEAR);
							WP(w, def_d).data_1 = 1 << (line * 2 + 1);
						}
					} else { // enter text
						SetDParam(0, _custom_currency.to_euro);
						str = STR_CONFIG_PATCHES_INT32;
						len = 4;
						afilter = CS_NUMERAL;
					}
					break;
			}

			if (len != 0) {
				WP(w, def_d).data_2 = line;
				ShowQueryString(str, STR_CURRENCY_CHANGE_PARAMETER, len + 1, 250, w, afilter);
			}

			w->flags4 |= 5 << WF_TIMEOUT_SHL;
			SetWindowDirty(w);
		} break;

		case WE_ON_EDIT_TEXT: {
			const char *b = e->we.edittext.str;

			switch (WP(w, def_d).data_2) {
				case CUSTCURR_EXCHANGERATE:
					_custom_currency.rate = Clamp(atoi(b), 1, 5000);
					break;

				case CUSTCURR_SEPARATOR: /* Thousands seperator */
					_custom_currency.separator = (b[0] == '\0') ? ' ' : b[0];
					ttd_strlcpy(_str_separator, b, lengthof(_str_separator));
					break;

				case CUSTCURR_PREFIX:
					ttd_strlcpy(_custom_currency.prefix, b, lengthof(_custom_currency.prefix));
					break;

				case CUSTCURR_SUFFIX:
					ttd_strlcpy(_custom_currency.suffix, b, lengthof(_custom_currency.suffix));
					break;

				case CUSTCURR_TO_EURO: { /* Year to switch to euro */
					int val = atoi(b);

					_custom_currency.to_euro = (val < 2000 ? CF_NOEURO : min(val, MAX_YEAR));
					break;
				}
			}
			MarkWholeScreenDirty();
		} break;

		case WE_TIMEOUT:
			WP(w, def_d).data_1 = 0;
			SetWindowDirty(w);
			break;

		case WE_DESTROY:
			DeleteWindowById(WC_QUERY_STRING, 0);
			MarkWholeScreenDirty();
			break;
	}
}

static const Widget _cust_currency_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,            STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   229,     0,    13, STR_CURRENCY_WINDOW, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   229,    14,   119, 0x0,                 STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _cust_currency_desc = {
	WDP_CENTER, WDP_CENTER, 230, 120, 230, 120,
	WC_CUSTOM_CURRENCY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_cust_currency_widgets,
	CustCurrencyWndProc,
};

static void ShowCustCurrency()
{
	_str_separator[0] = _custom_currency.separator;
	_str_separator[1] = '\0';

	DeleteWindowById(WC_CUSTOM_CURRENCY, 0);
	AllocateWindowDesc(&_cust_currency_desc);
}
