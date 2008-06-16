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
		if (_resolutions[i].width == _screen.width &&
				_resolutions[i].height == _screen.height) {
			break;
		}
	}
	return i;
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

static void ShowCustCurrency();

struct GameOptionsWindow : Window {
	GameSettings *opt;

	GameOptionsWindow(const WindowDesc *desc) : Window(desc)
	{
		this->opt = (_game_mode == GM_MENU) ? &_settings_newgame : &_settings_game;
		this->FindWindowPlacementAndResize(desc);
	}

	~GameOptionsWindow()
	{
		DeleteWindowById(WC_CUSTOM_CURRENCY, 0);
	}

	virtual void OnPaint()
	{
		StringID str = STR_02BE_DEFAULT;

		this->SetWidgetDisabledState(GAMEOPT_VEHICLENAME_SAVE, !(_vehicle_design_names & 1));
		if (!this->IsWidgetDisabled(GAMEOPT_VEHICLENAME_SAVE)) str = STR_02BF_CUSTOM;
		SetDParam(0, str);
		SetDParam(1, _currency_specs[this->opt->locale.currency].name);
		SetDParam(2, STR_UNITS_IMPERIAL + this->opt->locale.units);
		SetDParam(3, STR_02E9_DRIVE_ON_LEFT + this->opt->vehicle.road_side);
		SetDParam(4, TownName(this->opt->game_creation.town_name));
		SetDParam(5, _autosave_dropdown[_settings_client.gui.autosave]);
		SetDParam(6, SPECSTR_LANGUAGE_START + _dynlang.curr);
		int i = GetCurRes();
		SetDParam(7, i == _num_resolutions ? STR_RES_OTHER : SPECSTR_RESOLUTION_START + i);
		SetDParam(8, SPECSTR_SCREENSHOT_START + _cur_screenshot_format);
		this->SetWidgetLoweredState(GAMEOPT_FULLSCREEN, _fullscreen);

		this->DrawWidgets();
		DrawString(20, 175, STR_OPTIONS_FULLSCREEN, TC_FROMSTRING); // fullscreen
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case GAMEOPT_CURRENCY_BTN: // Setup currencies dropdown
				ShowDropDownMenu(this, BuildCurrencyDropdown(), this->opt->locale.currency, GAMEOPT_CURRENCY_BTN, _game_mode == GM_MENU ? 0 : ~GetMaskOfAllowedCurrencies(), 0);
				break;

			case GAMEOPT_DISTANCE_BTN: // Setup distance unit dropdown
				ShowDropDownMenu(this, _units_dropdown, this->opt->locale.units, GAMEOPT_DISTANCE_BTN, 0, 0);
				break;

			case GAMEOPT_ROADSIDE_BTN: { // Setup road-side dropdown
				int i = 0;
				extern bool RoadVehiclesAreBuilt();

				/* You can only change the drive side if you are in the menu or ingame with
				 * no vehicles present. In a networking game only the server can change it */
				if ((_game_mode != GM_MENU && RoadVehiclesAreBuilt()) || (_networking && !_network_server)) {
					i = (-1) ^ (1 << this->opt->vehicle.road_side); // disable the other value
				}

				ShowDropDownMenu(this, _driveside_dropdown, this->opt->vehicle.road_side, GAMEOPT_ROADSIDE_BTN, i, 0);
			} break;

			case GAMEOPT_TOWNNAME_BTN: // Setup townname dropdown
				ShowTownnameDropdown(this, this->opt->game_creation.town_name);
				break;

			case GAMEOPT_AUTOSAVE_BTN: // Setup autosave dropdown
				ShowDropDownMenu(this, _autosave_dropdown, _settings_client.gui.autosave, GAMEOPT_AUTOSAVE_BTN, 0, 0);
				break;

			case GAMEOPT_VEHICLENAME_BTN: // Setup customized vehicle-names dropdown
				ShowDropDownMenu(this, _designnames_dropdown, (_vehicle_design_names & 1) ? 1 : 0, GAMEOPT_VEHICLENAME_BTN, (_vehicle_design_names & 2) ? 0 : 2, 0);
				break;

			case GAMEOPT_VEHICLENAME_SAVE: // Save customized vehicle-names to disk
				break;  // not implemented

			case GAMEOPT_LANG_BTN: { // Setup interface language dropdown
				typedef std::map<StringID, int, StringIDCompare> LangList;

				/* Sort language names */
				LangList langs;
				for (int i = 0; i < _dynlang.num; i++) langs[SPECSTR_LANGUAGE_START + i] = i;

				DropDownList *list = new DropDownList();
				for (LangList::iterator it = langs.begin(); it != langs.end(); it++) {
					list->push_back(new DropDownListStringItem((*it).first, (*it).second, false));
				}

				ShowDropDownList(this, list, _dynlang.curr, GAMEOPT_LANG_BTN);
			} break;

			case GAMEOPT_RESOLUTION_BTN: // Setup resolution dropdown
				ShowDropDownMenu(this, BuildDynamicDropdown(SPECSTR_RESOLUTION_START, _num_resolutions), GetCurRes(), GAMEOPT_RESOLUTION_BTN, 0, 0);
				break;

			case GAMEOPT_FULLSCREEN: // Click fullscreen on/off
				/* try to toggle full-screen on/off */
				if (!ToggleFullScreen(!_fullscreen)) {
					ShowErrorMessage(INVALID_STRING_ID, STR_FULLSCREEN_FAILED, 0, 0);
				}
				this->SetWidgetLoweredState(GAMEOPT_FULLSCREEN, _fullscreen);
				this->SetDirty();
				break;

			case GAMEOPT_SCREENSHOT_BTN: // Setup screenshot format dropdown
				ShowDropDownMenu(this, BuildDynamicDropdown(SPECSTR_SCREENSHOT_START, _num_screenshot_formats), _cur_screenshot_format, GAMEOPT_SCREENSHOT_BTN, 0, 0);
				break;
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case GAMEOPT_VEHICLENAME_BTN: // Vehicle design names
				if (index == 0) {
					DeleteCustomEngineNames();
					MarkWholeScreenDirty();
				} else if (!(_vehicle_design_names & 1)) {
					LoadCustomEngineNames();
					MarkWholeScreenDirty();
				}
				break;

			case GAMEOPT_CURRENCY_BTN: /* Currency */
				if (index == CUSTOM_CURRENCY_ID) ShowCustCurrency();
				this->opt->locale.currency = index;
				MarkWholeScreenDirty();
				break;

			case GAMEOPT_DISTANCE_BTN: // Measuring units
				this->opt->locale.units = index;
				MarkWholeScreenDirty();
				break;

			case GAMEOPT_ROADSIDE_BTN: // Road side
				if (this->opt->vehicle.road_side != index) { // only change if setting changed
					DoCommandP(0, index, 0, NULL, CMD_SET_ROAD_DRIVE_SIDE | CMD_MSG(STR_00B4_CAN_T_DO_THIS));
					MarkWholeScreenDirty();
				}
				break;

			case GAMEOPT_TOWNNAME_BTN: // Town names
				if (_game_mode == GM_MENU) {
					this->opt->game_creation.town_name = index;
					InvalidateWindow(WC_GAME_OPTIONS, 0);
				}
				break;

			case GAMEOPT_AUTOSAVE_BTN: // Autosave options
				_settings_client.gui.autosave = index;
				this->SetDirty();
				break;

			case GAMEOPT_LANG_BTN: // Change interface language
				ReadLanguagePack(index);
				CheckForMissingGlyphsInLoadedLanguagePack();
				UpdateAllStationVirtCoord();
				UpdateAllWaypointSigns();
				MarkWholeScreenDirty();
				break;

			case GAMEOPT_RESOLUTION_BTN: // Change resolution
				if (index < _num_resolutions && ChangeResInGame(_resolutions[index].width, _resolutions[index].height)) {
					this->SetDirty();
				}
				break;

			case GAMEOPT_SCREENSHOT_BTN: // Change screenshot format
				SetScreenshotFormat(index);
				this->SetDirty();
				break;
		}
	}
};

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
};


void ShowGameOptions()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	new GameOptionsWindow(&_game_options_desc);
}

extern void StartupEconomy();

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
};

void SetDifficultyLevel(int mode, DifficultySettings *gm_opt);

struct GameDifficultyWindow : public Window {
private:
	static const uint GAME_DIFFICULTY_NUM = 18;
	bool clicked_increase;
	uint8 clicked_button;
	uint8 timeout;

	/* Temporary holding place of values in the difficulty window until 'Save' is clicked */
	GameSettings opt_mod_temp;

	enum {
		GAMEDIFF_WND_TOP_OFFSET = 45,
		GAMEDIFF_WND_ROWSIZE    = 9,
		NO_SETTINGS_BUTTON = 0xFF,
	};

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

public:
	GameDifficultyWindow() : Window(&_game_difficulty_desc)
	{
		/* Copy current settings (ingame or in intro) to temporary holding place
		 * change that when setting stuff, copy back on clicking 'OK' */
		this->opt_mod_temp = (_game_mode == GM_MENU) ? _settings_newgame : _settings_game;
		this->clicked_increase = false;
		this->clicked_button = NO_SETTINGS_BUTTON;
		this->timeout = 0;
		/* Hide the closebox to make sure that the user aborts or confirms his changes */
		this->HideWidget(GDW_CLOSEBOX);
		this->widget[GDW_CAPTION].left = 0;
		/* Setup disabled buttons when creating window
		 * disable all other difficulty buttons during gameplay except for 'custom' */
		this->SetWidgetsDisabledState(_game_mode == GM_NORMAL,
			GDW_LVL_EASY,
			GDW_LVL_MEDIUM,
			GDW_LVL_HARD,
			GDW_LVL_CUSTOM,
			WIDGET_LIST_END);
		this->SetWidgetDisabledState(GDW_HIGHSCORE, _game_mode == GM_EDITOR || _networking); // highscore chart in multiplayer
		this->SetWidgetDisabledState(GDW_ACCEPT, _networking && !_network_server); // Save-button in multiplayer (and if client)
		this->LowerWidget(GDW_LVL_EASY + this->opt_mod_temp.difficulty.diff_level);
		this->FindWindowPlacementAndResize(&_game_difficulty_desc);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		uint i;
		const SettingDesc *sd = GetPatchFromName("difficulty.max_no_competitors", &i);
		int y = GAMEDIFF_WND_TOP_OFFSET;
		for (i = 0; i < GAME_DIFFICULTY_NUM; i++, sd++) {
			const SettingDescBase *sdb = &sd->desc;
			int32 value = (int32)ReadValue(GetVariableAddress(&this->opt_mod_temp, &sd->save), sd->save.conv);
			bool editable = (_game_mode == GM_MENU || (sdb->flags & SGF_NEWGAME_ONLY) == 0);

			DrawArrowButtons(5, y, 3,
					(this->clicked_button == i) ? 1 + !!this->clicked_increase : 0,
					editable && sdb->min != value,
					editable && sdb->max != value);

			value += sdb->str;
			SetDParam(0, value);
			DrawString(30, y, STR_6805_MAXIMUM_NO_COMPETITORS + i, TC_FROMSTRING);

			y += GAMEDIFF_WND_ROWSIZE + 2; // space items apart a bit
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case GDW_SETTING_BG: { /* Difficulty settings widget, decode click */
				/* Don't allow clients to make any changes */
				if (_networking && !_network_server) return;

				const int x = pt.x - 5;
				if (!IsInsideMM(x, 0, 21)) return; // Button area

				const int y = pt.y - GAMEDIFF_WND_TOP_OFFSET;
				if (y < 0) return;

				/* Get button from Y coord. */
				const uint8 btn = y / (GAMEDIFF_WND_ROWSIZE + 2);
				if (btn >= GAME_DIFFICULTY_NUM || y % (GAMEDIFF_WND_ROWSIZE + 2) >= 9) return;

				uint i;
				const SettingDesc *sd = GetPatchFromName("difficulty.max_no_competitors", &i) + btn;
				const SettingDescBase *sdb = &sd->desc;

				/* Clicked disabled button? */
				bool editable = (_game_mode == GM_MENU || (sdb->flags & SGF_NEWGAME_ONLY) == 0);
				if (!editable) return;

				this->timeout = 5;
				int32 val = (int32)ReadValue(GetVariableAddress(&this->opt_mod_temp, &sd->save), sd->save.conv);

				if (x >= 10) {
					/* Increase button clicked */
					val = min(val + sdb->interval, sdb->max);
					this->clicked_increase = true;
				} else {
					/* Decrease button clicked */
					val -= sdb->interval;
					val = max(val, sdb->min);
					this->clicked_increase = false;
				}
				this->clicked_button = btn;

				/* save value in temporary variable */
				WriteValue(GetVariableAddress(&this->opt_mod_temp, &sd->save), sd->save.conv, val);
				this->RaiseWidget(GDW_LVL_EASY + this->opt_mod_temp.difficulty.diff_level);
				SetDifficultyLevel(3, &this->opt_mod_temp.difficulty); // set difficulty level to custom
				this->LowerWidget(GDW_LVL_CUSTOM);
				this->SetDirty();
			} break;

			case GDW_LVL_EASY:
			case GDW_LVL_MEDIUM:
			case GDW_LVL_HARD:
			case GDW_LVL_CUSTOM:
				/* temporarily change difficulty level */
				this->RaiseWidget(GDW_LVL_EASY + this->opt_mod_temp.difficulty.diff_level);
				SetDifficultyLevel(widget - GDW_LVL_EASY, &this->opt_mod_temp.difficulty);
				this->LowerWidget(GDW_LVL_EASY + this->opt_mod_temp.difficulty.diff_level);
				this->SetDirty();
				break;

			case GDW_HIGHSCORE: // Highscore Table
				ShowHighscoreTable(this->opt_mod_temp.difficulty.diff_level, -1);
				break;

			case GDW_ACCEPT: { // Save button - save changes
				GameSettings *opt_ptr = (_game_mode == GM_MENU) ? &_settings_newgame : &_settings_game;

				uint i;
				const SettingDesc *sd = GetPatchFromName("difficulty.max_no_competitors", &i);
				for (uint btn = 0; btn != GAME_DIFFICULTY_NUM; btn++, sd++) {
					int32 new_val = (int32)ReadValue(GetVariableAddress(&this->opt_mod_temp, &sd->save), sd->save.conv);
					int32 cur_val = (int32)ReadValue(GetVariableAddress(opt_ptr, &sd->save), sd->save.conv);
					/* if setting has changed, change it */
					if (new_val != cur_val) {
						DoCommandP(0, i + btn, new_val, NULL, CMD_CHANGE_PATCH_SETTING);
					}
				}

				GetPatchFromName("difficulty.diff_level", &i);
				DoCommandP(0, i, this->opt_mod_temp.difficulty.diff_level, NULL, CMD_CHANGE_PATCH_SETTING);
				delete this;
				/* If we are in the editor, we should reload the economy.
				 * This way when you load a game, the max loan and interest rate
				 * are loaded correctly. */
				if (_game_mode == GM_EDITOR) StartupEconomy();
				break;
			}

			case GDW_CANCEL: // Cancel button - close window, abandon changes
				delete this;
				break;
		}
	}

	virtual void OnTick()
	{
		if (this->timeout != 0) {
			this->timeout--;
			if (this->timeout == 0) this->clicked_button = NO_SETTINGS_BUTTON;
			this->SetDirty();
		}
	}
};

void ShowGameDifficulty()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	new GameDifficultyWindow();
}

static const char *_patches_ui[] = {
	"gui.vehicle_speed",
	"gui.status_long_date",
	"gui.show_finances",
	"gui.autoscroll",
	"gui.reverse_scroll",
	"gui.smooth_scroll",
	"gui.errmsg_duration",
	"gui.toolbar_pos",
	"gui.measure_tooltip",
	"gui.window_snap_radius",
	"gui.population_in_label",
	"gui.link_terraform_toolbar",
	"gui.liveries",
	"gui.prefer_teamchat",
	/* While the horizontal scrollwheel scrolling is written as general code, only
	 *  the cocoa (OSX) driver generates input for it.
	 *  Since it's also able to completely disable the scrollwheel will we display it on all platforms anyway */
	"gui.scrollwheel_scrolling",
	"gui.scrollwheel_multiplier",
#ifdef __APPLE__
	/* We might need to emulate a right mouse button on mac */
	"gui.right_mouse_btn_emulation",
#endif
	"gui.pause_on_newgame",
	"gui.advanced_vehicle_list",
	"gui.loading_indicators",
	"gui.timetable_in_ticks",
	"gui.default_rail_type",
	"gui.always_build_infrastructure",
};

static const char *_patches_construction[] = {
	"construction.build_on_slopes",
	"construction.autoslope",
	"construction.extra_dynamite",
	"construction.longbridges",
	"construction.signal_side",
	"station.always_small_airport",
	"gui.enable_signal_gui",
	"gui.drag_signals_density",
	"game_creation.oil_refinery_limit",
	"gui.semaphore_build_before",
};

static const char *_patches_stations[] = {
	"station.join_stations",
	"order.improved_load",
	"order.selectgoods",
	"gui.new_nonstop",
	"station.nonuniform_stations",
	"station.station_spread",
	"order.serviceathelipad",
	"station.modified_catchment",
	"order.gradual_loading",
	"construction.road_stop_on_town_road",
	"station.adjacent_stations",
	"economy.station_noise_level",
};

static const char *_patches_economy[] = {
	"economy.inflation",
	"construction.raw_industry_construction",
	"economy.multiple_industry_per_town",
	"economy.same_industry_close",
	"economy.bribe",
	"economy.exclusive_rights",
	"economy.give_money",
	"gui.colored_news_year",
	"gui.ending_year",
	"economy.smooth_economy",
	"economy.allow_shares",
	"economy.town_layout",
	"economy.mod_road_rebuild",
	"economy.town_growth_rate",
	"economy.larger_towns",
	"economy.initial_city_size",
};

static const char *_patches_ai[] = {
	"ai.ainew_active",
	"ai.ai_in_multiplayer",
	"ai.ai_disable_veh_train",
	"ai.ai_disable_veh_roadveh",
	"ai.ai_disable_veh_aircraft",
	"ai.ai_disable_veh_ship",
};

static const char *_patches_vehicles[] = {
	"vehicle.realistic_acceleration",
	"pf.forbid_90_deg",
	"vehicle.mammoth_trains",
	"order.gotodepot",
	"pf.roadveh_queue",
	"pf.pathfinder_for_trains",
	"pf.pathfinder_for_roadvehs",
	"pf.pathfinder_for_ships",
	"gui.train_income_warn",
	"gui.order_review_system",
	"vehicle.never_expire_vehicles",
	"gui.lost_train_warn",
	"gui.autorenew",
	"gui.autorenew_months",
	"gui.autorenew_money",
	"vehicle.max_trains",
	"vehicle.max_roadveh",
	"vehicle.max_aircraft",
	"vehicle.max_ships",
	"vehicle.servint_ispercent",
	"vehicle.servint_trains",
	"vehicle.servint_roadveh",
	"vehicle.servint_ships",
	"vehicle.servint_aircraft",
	"order.no_servicing_if_no_breakdowns",
	"vehicle.wagon_speed_limits",
	"vehicle.disable_elrails",
	"vehicle.freight_trains",
	"vehicle.plane_speed",
	"order.timetabling",
	"vehicle.dynamic_engines",
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

struct PatchesSelectionWindow : Window {
	static GameSettings *patches_ptr;
	static int patches_max;

	int page;
	int entry;
	int click;

	PatchesSelectionWindow(const WindowDesc *desc) : Window(desc)
	{
		static bool first_time = true;

		patches_ptr = (_game_mode == GM_MENU) ? &_settings_newgame : &_settings_game;

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
		ResizeWindowForWidget(this, PATCHSEL_OPTIONSPANEL, 0, patches_max * 11);

		/* Recentre the window for the new size */
		this->top = this->top - (patches_max * 11) / 2;

		this->LowerWidget(4);

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		int x, y;
		const PatchPage *page = &_patches_page[this->page];
		uint i;

		/* Set up selected category */
		this->DrawWidgets();

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
				DrawArrowButtons(x, y, 3, this->click - (i * 2), (editable && value != sdb->min), (editable && value != sdb->max));

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
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case PATCHSEL_OPTIONSPANEL: {
				const PatchPage *page = &_patches_page[this->page];
				const SettingDesc *sd;
				void *var;
				int32 value;
				int x, y;
				byte btn;

				y = pt.y - 46 - 1;
				if (y < 0) return;

				x = pt.x - 5;
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
						if ((this->flags4 & WF_TIMEOUT_MASK) > 2 << WF_TIMEOUT_SHL) {
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
							this->click = btn * 2 + 1 + ((x >= 10) ? 1 : 0);
							this->flags4 |= 5 << WF_TIMEOUT_SHL;
							_left_button_clicked = false;
						}
					} break;
					default: NOT_REACHED();
					}

					if (value != oldvalue) {
						SetPatchValue(page->entries[btn].index, value);
						this->SetDirty();
					}
				} else {
					/* only open editbox for types that its sensible for */
					if (sd->desc.cmd != SDT_BOOLX && !(sd->desc.flags & SGF_MULTISTRING)) {
						/* Show the correct currency-translated value */
						if (sd->desc.flags & SGF_CURRENCY) value *= _currency->rate;

						this->entry = btn;
						SetDParam(0, value);
						ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_CONFIG_PATCHES_QUERY_CAPT, 10, 100, this, CS_NUMERAL);
					}
				}
			} break;

			case PATCHSEL_INTERFACE: case PATCHSEL_CONSTRUCTION: case PATCHSEL_VEHICLES:
			case PATCHSEL_STATIONS:  case PATCHSEL_ECONOMY:      case PATCHSEL_COMPETITORS:
				this->RaiseWidget(this->page + PATCHSEL_INTERFACE);
				this->page = widget - PATCHSEL_INTERFACE;
				this->LowerWidget(this->page + PATCHSEL_INTERFACE);
				DeleteWindowById(WC_QUERY_STRING, 0);
				this->SetDirty();
				break;
		}
	}

	virtual void OnTimeout()
	{
		this->click = 0;
		this->SetDirty();
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (!StrEmpty(str)) {
			const PatchEntry *pe = &_patches_page[this->page].entries[this->entry];
			const SettingDesc *sd = pe->setting;
			int32 value = atoi(str);

			/* Save the correct currency-translated value */
			if (sd->desc.flags & SGF_CURRENCY) value /= _currency->rate;

			SetPatchValue(pe->index, value);
			this->SetDirty();
		}
	}
};

GameSettings *PatchesSelectionWindow::patches_ptr = NULL;
int PatchesSelectionWindow::patches_max = 0;

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
};

void ShowPatchesSelection()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	new PatchesSelectionWindow(&_patches_selection_desc);
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

struct CustomCurrencyWindow : Window {
	char separator[2];
	int click;
	int query_widget;

	CustomCurrencyWindow(const WindowDesc *desc) : Window(desc)
	{
		this->separator[0] = _custom_currency.separator;
		this->separator[1] = '\0';
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		int x;
		int y = 20;
		this->DrawWidgets();

		/* exchange rate */
		DrawArrowButtons(10, y, 3, GB(this->click, 0, 2), true, true);
		SetDParam(0, 1);
		SetDParam(1, 1);
		DrawString(35, y + 1, STR_CURRENCY_EXCHANGE_RATE, TC_FROMSTRING);
		y += 12;

		/* separator */
		DrawFrameRect(10, y + 1, 29, y + 9, 0, GB(this->click, 2, 2) ? FR_LOWERED : FR_NONE);
		x = DrawString(35, y + 1, STR_CURRENCY_SEPARATOR, TC_FROMSTRING);
		DoDrawString(this->separator, x + 4, y + 1, TC_ORANGE);
		y += 12;

		/* prefix */
		DrawFrameRect(10, y + 1, 29, y + 9, 0, GB(this->click, 4, 2) ? FR_LOWERED : FR_NONE);
		x = DrawString(35, y + 1, STR_CURRENCY_PREFIX, TC_FROMSTRING);
		DoDrawString(_custom_currency.prefix, x + 4, y + 1, TC_ORANGE);
		y += 12;

		/* suffix */
		DrawFrameRect(10, y + 1, 29, y + 9, 0, GB(this->click, 6, 2) ? FR_LOWERED : FR_NONE);
		x = DrawString(35, y + 1, STR_CURRENCY_SUFFIX, TC_FROMSTRING);
		DoDrawString(_custom_currency.suffix, x + 4, y + 1, TC_ORANGE);
		y += 12;

		/* switch to euro */
		DrawArrowButtons(10, y, 3, GB(this->click, 8, 2), true, true);
		SetDParam(0, _custom_currency.to_euro);
		DrawString(35, y + 1, (_custom_currency.to_euro != CF_NOEURO) ? STR_CURRENCY_SWITCH_TO_EURO : STR_CURRENCY_SWITCH_TO_EURO_NEVER, TC_FROMSTRING);
		y += 12;

		/* Preview */
		y += 12;
		SetDParam(0, 10000);
		DrawString(35, y + 1, STR_CURRENCY_PREVIEW, TC_FROMSTRING);
	}

	virtual void OnClick(Point pt, int widget)
	{
		int line = (pt.y - 20) / 12;
		int len = 0;
		int x = pt.x;
		StringID str = 0;
		CharSetFilter afilter = CS_ALPHANUMERAL;

		switch (line) {
			case CUSTCURR_EXCHANGERATE:
				if (IsInsideMM(x, 10, 30)) { // clicked buttons
					if (x < 20) {
						if (_custom_currency.rate > 1) _custom_currency.rate--;
						this->click = 1 << (line * 2 + 0);
					} else {
						if (_custom_currency.rate < 5000) _custom_currency.rate++;
						this->click = 1 << (line * 2 + 1);
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
					this->click = 1 << (line * 2 + 1);
				}
				str = BindCString(this->separator);
				len = 1;
				break;

			case CUSTCURR_PREFIX:
				if (IsInsideMM(x, 10, 30)) { // clicked button
					this->click = 1 << (line * 2 + 1);
				}
				str = BindCString(_custom_currency.prefix);
				len = 12;
				break;

			case CUSTCURR_SUFFIX:
				if (IsInsideMM(x, 10, 30)) { // clicked button
					this->click = 1 << (line * 2 + 1);
				}
				str = BindCString(_custom_currency.suffix);
				len = 12;
				break;

			case CUSTCURR_TO_EURO:
				if (IsInsideMM(x, 10, 30)) { // clicked buttons
					if (x < 20) {
						_custom_currency.to_euro = (_custom_currency.to_euro <= 2000) ?
							CF_NOEURO : _custom_currency.to_euro - 1;
						this->click = 1 << (line * 2 + 0);
					} else {
						_custom_currency.to_euro =
							Clamp(_custom_currency.to_euro + 1, 2000, MAX_YEAR);
						this->click = 1 << (line * 2 + 1);
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
			this->query_widget = line;
			ShowQueryString(str, STR_CURRENCY_CHANGE_PARAMETER, len + 1, 250, this, afilter);
		}

		this->flags4 |= 5 << WF_TIMEOUT_SHL;
		this->SetDirty();
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		switch (this->query_widget) {
			case CUSTCURR_EXCHANGERATE:
				_custom_currency.rate = Clamp(atoi(str), 1, 5000);
				break;

			case CUSTCURR_SEPARATOR: /* Thousands seperator */
				_custom_currency.separator = StrEmpty(str) ? ' ' : str[0];
				ttd_strlcpy(this->separator, str, lengthof(this->separator));
				break;

			case CUSTCURR_PREFIX:
				ttd_strlcpy(_custom_currency.prefix, str, lengthof(_custom_currency.prefix));
				break;

			case CUSTCURR_SUFFIX:
				ttd_strlcpy(_custom_currency.suffix, str, lengthof(_custom_currency.suffix));
				break;

			case CUSTCURR_TO_EURO: { /* Year to switch to euro */
				int val = atoi(str);

				_custom_currency.to_euro = (val < 2000 ? CF_NOEURO : min(val, MAX_YEAR));
				break;
			}
		}
		MarkWholeScreenDirty();
	}

	virtual void OnTimeout()
	{
		this->click = 0;
		this->SetDirty();
	}
};

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
};

static void ShowCustCurrency()
{
	DeleteWindowById(WC_CUSTOM_CURRENCY, 0);
	new CustomCurrencyWindow(&_cust_currency_desc);
}
