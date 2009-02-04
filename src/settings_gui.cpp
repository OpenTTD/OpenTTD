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
#include "network/network.h"
#include "town.h"
#include "variables.h"
#include "settings_internal.h"
#include "newgrf_townname.h"
#include "strings_func.h"
#include "window_func.h"
#include "string_func.h"
#include "gfx_func.h"
#include "waypoint.h"
#include "widgets/dropdown_type.h"
#include "widgets/dropdown_func.h"
#include "station_func.h"
#include "highscore.h"
#include <map>

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
		list->push_back(new DropDownListStringItem((*it).first, (*it).second, !(_game_mode == GM_MENU || GetNumTowns() == 0 || (*it).second == sel)));
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
					uint i;
					if (GetPatchFromName("vehicle.road_side", &i) == NULL) NOT_REACHED();
					SetPatchValue(i, index);
					MarkWholeScreenDirty();
				}
				break;

			case GAMEOPT_TOWNNAME_BTN: // Town names
				if (_game_mode == GM_MENU || GetNumTowns() == 0) {
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
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   369,     0,    13, STR_00B1_GAME_OPTIONS,             STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   369,    14,   238, 0x0,                               STR_NULL},
{      WWT_FRAME,   RESIZE_NONE,  COLOUR_GREY,    10,   179,    20,    55, STR_02E0_CURRENCY_UNITS,           STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,  COLOUR_GREY,    20,   169,    34,    45, STR_02E1,                          STR_02E2_CURRENCY_UNITS_SELECTION},
{      WWT_FRAME,   RESIZE_NONE,  COLOUR_GREY,   190,   359,    20,    55, STR_MEASURING_UNITS,               STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,  COLOUR_GREY,   200,   349,    34,    45, STR_02E4,                          STR_MEASURING_UNITS_SELECTION},
{      WWT_FRAME,   RESIZE_NONE,  COLOUR_GREY,    10,   179,    62,    97, STR_02E6_ROAD_VEHICLES,            STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,  COLOUR_GREY,    20,   169,    76,    87, STR_02E7,                          STR_02E8_SELECT_SIDE_OF_ROAD_FOR},
{      WWT_FRAME,   RESIZE_NONE,  COLOUR_GREY,   190,   359,    62,    97, STR_02EB_TOWN_NAMES,               STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,  COLOUR_GREY,   200,   349,    76,    87, STR_02EC,                          STR_02ED_SELECT_STYLE_OF_TOWN_NAMES},
{      WWT_FRAME,   RESIZE_NONE,  COLOUR_GREY,    10,   179,   104,   139, STR_02F4_AUTOSAVE,                 STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,  COLOUR_GREY,    20,   169,   118,   129, STR_02F5,                          STR_02F6_SELECT_INTERVAL_BETWEEN},

{      WWT_FRAME,   RESIZE_NONE,  COLOUR_GREY,    10,   359,   194,   228, STR_02BC_VEHICLE_DESIGN_NAMES,     STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,  COLOUR_GREY,    20,   119,   207,   218, STR_02BD,                          STR_02C1_VEHICLE_DESIGN_NAMES_SELECTION},
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,   130,   349,   207,   218, STR_02C0_SAVE_CUSTOM_NAMES,        STR_02C2_SAVE_CUSTOMIZED_VEHICLE},

{      WWT_FRAME,   RESIZE_NONE,  COLOUR_GREY,   190,   359,   104,   139, STR_OPTIONS_LANG,                  STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,  COLOUR_GREY,   200,   349,   118,   129, STR_OPTIONS_LANG_CBO,              STR_OPTIONS_LANG_TIP},

{      WWT_FRAME,   RESIZE_NONE,  COLOUR_GREY,    10,   179,   146,   190, STR_OPTIONS_RES,                   STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,  COLOUR_GREY,    20,   169,   160,   171, STR_OPTIONS_RES_CBO,               STR_OPTIONS_RES_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,   149,   169,   176,   184, STR_EMPTY,                         STR_OPTIONS_FULLSCREEN_TIP},

{      WWT_FRAME,   RESIZE_NONE,  COLOUR_GREY,   190,   359,   146,   190, STR_OPTIONS_SCREENSHOT_FORMAT,     STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,  COLOUR_GREY,   200,   349,   160,   171, STR_OPTIONS_SCREENSHOT_FORMAT_CBO, STR_OPTIONS_SCREENSHOT_FORMAT_TIP},

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
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_MAUVE,      0,    10,     0,    13, STR_00C5,                     STR_018B_CLOSE_WINDOW},           // GDW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_MAUVE,     11,   369,     0,    13, STR_6800_DIFFICULTY_LEVEL,    STR_018C_WINDOW_TITLE_DRAG_THIS}, // GDW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_MAUVE,      0,   369,    14,    41, 0x0,                          STR_NULL},                        // GDW_UPPER_BG
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_YELLOW,    10,    96,    16,    27, STR_6801_EASY,                STR_NULL},                        // GDW_LVL_EASY
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_YELLOW,    97,   183,    16,    27, STR_6802_MEDIUM,              STR_NULL},                        // GDW_LVL_MEDIUM
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_YELLOW,   184,   270,    16,    27, STR_6803_HARD,                STR_NULL},                        // GDW_LVL_HARD
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_YELLOW,   271,   357,    16,    27, STR_6804_CUSTOM,              STR_NULL},                        // GDW_LVL_CUSTOM
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREEN,     10,   357,    28,    39, STR_6838_SHOW_HI_SCORE_CHART, STR_NULL},                        // GDW_HIGHSCORE
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_MAUVE,      0,   369,    42,   262, 0x0,                          STR_NULL},                        // GDW_SETTING_BG
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_MAUVE,      0,   369,   263,   278, 0x0,                          STR_NULL},                        // GDW_LOWER_BG
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_YELLOW,   105,   185,   265,   276, STR_OPTIONS_SAVE_CHANGES,     STR_NULL},                        // GDW_ACCEPT
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_YELLOW,   186,   266,   265,   276, STR_012E_CANCEL,              STR_NULL},                        // GDW_CANCEL
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
		StringID str = STR_6805_MAXIMUM_NO_COMPETITORS;
		for (i = 0; i < GAME_DIFFICULTY_NUM; i++, sd++) {
			const SettingDescBase *sdb = &sd->desc;
			/* skip deprecated difficulty options */
			if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
			int32 value = (int32)ReadValue(GetVariableAddress(&this->opt_mod_temp, &sd->save), sd->save.conv);
			bool editable = (_game_mode == GM_MENU || (sdb->flags & SGF_NEWGAME_ONLY) == 0);

			DrawArrowButtons(5, y, COLOUR_YELLOW,
					(this->clicked_button == i) ? 1 + !!this->clicked_increase : 0,
					editable && sdb->min != value,
					editable && sdb->max != value);

			value += sdb->str;
			SetDParam(0, value);
			DrawString(30, y, str, TC_FROMSTRING);

			y += GAMEDIFF_WND_ROWSIZE + 2; // space items apart a bit
			str++;
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
				uint8 btn = y / (GAMEDIFF_WND_ROWSIZE + 2);
				if (btn >= 1) btn++; // Skip the deprecated option "competitor start time"
				if (btn >= 8) btn++; // Skip the deprecated option "competitor intelligence"
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
						DoCommandP(0, i + btn, new_val, CMD_CHANGE_PATCH_SETTING);
					}
				}

				GetPatchFromName("difficulty.diff_level", &i);
				DoCommandP(0, i, this->opt_mod_temp.difficulty.diff_level, CMD_CHANGE_PATCH_SETTING);
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

static const int SETTING_HEIGHT = 11; ///< Height of a single setting in the tree view in pixels
static const int LEVEL_WIDTH = 15;    ///< Indenting width of a sub-page in pixels

/**
 * Flags for #PatchEntry
 * @note The #PEF_BUTTONS_MASK matches expectations of the formal parameter 'state' of #DrawArrowButtons
 */
enum PatchEntryFlags {
	PEF_LEFT_DEPRESSED  = 0x01, ///< Of a numeric patch entry, the left button is depressed
	PEF_RIGHT_DEPRESSED = 0x02, ///< Of a numeric patch entry, the right button is depressed
	PEF_BUTTONS_MASK = (PEF_LEFT_DEPRESSED | PEF_RIGHT_DEPRESSED), ///< Bit-mask for button flags

	PEF_LAST_FIELD = 0x04, ///< This entry is the last one in a (sub-)page

	/* Entry kind */
	PEF_SETTING_KIND = 0x10, ///< Entry kind: Entry is a setting
	PEF_SUBTREE_KIND = 0x20, ///< Entry kind: Entry is a sub-tree
	PEF_KIND_MASK    = (PEF_SETTING_KIND | PEF_SUBTREE_KIND), ///< Bit-mask for fetching entry kind
};

struct PatchPage; // Forward declaration

/** Data fields for a sub-page (#PEF_SUBTREE_KIND kind)*/
struct PatchEntrySubtree {
	PatchPage *page; ///< Pointer to the sub-page
	bool folded;     ///< Sub-page is folded (not visible except for its title)
	StringID title;  ///< Title of the sub-page
};

/** Data fields for a single setting (#PEF_SETTING_KIND kind) */
struct PatchEntrySetting {
	const char *name;           ///< Name of the setting
	const SettingDesc *setting; ///< Setting description of the setting
	uint index;                 ///< Index of the setting in the settings table
};

/** Data structure describing a single patch in a tab */
struct PatchEntry {
	byte flags; ///< Flags of the patch entry. @see PatchEntryFlags
	byte level; ///< Nesting level of this patch entry
	union {
		PatchEntrySetting entry; ///< Data fields if entry is a setting
		PatchEntrySubtree sub;   ///< Data fields if entry is a sub-page
	} d; ///< Data fields for each kind

	PatchEntry(const char *nm);
	PatchEntry(PatchPage *sub, StringID title);

	void Init(byte level, bool last_field);
	void FoldAll();
	void SetButtons(byte new_val);

	uint Length() const;
	PatchEntry *FindEntry(uint row, uint *cur_row);

	uint Draw(GameSettings *patches_ptr, int base_x, int base_y, uint first_row, uint max_row, uint cur_row, uint parent_last);

private:
	void DrawPatch(GameSettings *patches_ptr, const SettingDesc *sd, int x, int y, int state);
};

/** Data structure describing one page of patches in the patch settings window. */
struct PatchPage {
	PatchEntry *entries; ///< Array of patch entries of the page.
	byte num;            ///< Number of entries on the page (statically filled).

	void Init(byte level = 0);
	void FoldAll();

	uint Length() const;
	PatchEntry *FindEntry(uint row, uint *cur_row) const;

	uint Draw(GameSettings *patches_ptr, int base_x, int base_y, uint first_row, uint max_row, uint cur_row = 0, uint parent_last = 0) const;
};


/* == PatchEntry methods == */

/**
 * Constructor for a single setting in the 'advanced settings' window
 * @param nm Name of the setting in the setting table
 */
PatchEntry::PatchEntry(const char *nm)
{
	this->flags = PEF_SETTING_KIND;
	this->level = 0;
	this->d.entry.name = nm;
	this->d.entry.setting = NULL;
	this->d.entry.index = 0;
}

/**
 * Constructor for a sub-page in the 'advanced settings' window
 * @param sub   Sub-page
 * @param title Title of the sub-page
 */
PatchEntry::PatchEntry(PatchPage *sub, StringID title)
{
	this->flags = PEF_SUBTREE_KIND;
	this->level = 0;
	this->d.sub.page = sub;
	this->d.sub.folded = true;
	this->d.sub.title = title;
}

/**
 * Initialization of a patch entry
 * @param level      Page nesting level of this entry
 * @param last_field Boolean indicating this entry is the last at the (sub-)page
 */
void PatchEntry::Init(byte level, bool last_field)
{
	this->level = level;
	if (last_field) this->flags |= PEF_LAST_FIELD;

	switch (this->flags & PEF_KIND_MASK) {
		case PEF_SETTING_KIND:
			this->d.entry.setting = GetPatchFromName(this->d.entry.name, &this->d.entry.index);
			assert(this->d.entry.setting != NULL);
			break;
		case PEF_SUBTREE_KIND:
			this->d.sub.page->Init(level + 1);
			break;
		default: NOT_REACHED();
	}
}

/** Recursively close all folds of sub-pages */
void PatchEntry::FoldAll()
{
	switch(this->flags & PEF_KIND_MASK) {
		case PEF_SETTING_KIND:
			break;

		case PEF_SUBTREE_KIND:
			this->d.sub.folded = true;
			this->d.sub.page->FoldAll();
			break;

		default: NOT_REACHED();
	}
}


/**
 * Set the button-depressed flags (#PEF_LEFT_DEPRESSED and #PEF_RIGHT_DEPRESSED) to a specified value
 * @param new_val New value for the button flags
 * @see PatchEntryFlags
 */
void PatchEntry::SetButtons(byte new_val)
{
	assert((new_val & ~PEF_BUTTONS_MASK) == 0); // Should not touch any flags outside the buttons
	this->flags = (this->flags & ~PEF_BUTTONS_MASK) | new_val;
}

/** Return numbers of rows needed to display the entry */
uint PatchEntry::Length() const
{
	switch (this->flags & PEF_KIND_MASK) {
		case PEF_SETTING_KIND:
			return 1;
		case PEF_SUBTREE_KIND:
			if (this->d.sub.folded) return 1; // Only displaying the title

			return 1 + this->d.sub.page->Length(); // 1 extra row for the title
		default: NOT_REACHED();
	}
}

/**
 * Find patch entry at row \a row_num
 * @param row_num Index of entry to return
 * @param cur_row Current row number
 * @return The requested patch entry or \c NULL if it not found
 */
PatchEntry *PatchEntry::FindEntry(uint row_num, uint *cur_row)
{
	if (row_num == *cur_row) return this;

	switch (this->flags & PEF_KIND_MASK) {
		case PEF_SETTING_KIND:
			(*cur_row)++;
			break;
		case PEF_SUBTREE_KIND:
			(*cur_row)++; // add one for row containing the title
			if (this->d.sub.folded) {
				break;
			}

			/* sub-page is visible => search it too */
			return this->d.sub.page->FindEntry(row_num, cur_row);
		default: NOT_REACHED();
	}
	return NULL;
}

/**
 * Draw a row in the settings panel.
 *
 * See PatchPage::Draw() for an explanation about how drawing is performed.
 *
 * The \a parent_last parameter ensures that the vertical lines at the left are
 * only drawn when another entry follows, that it prevents output like
 * \verbatim
 *  |-- setting
 *  |-- (-) - Title
 *  |    |-- setting
 *  |    |-- setting
 * \endverbatim
 * The left-most vertical line is not wanted. It is prevented by setting the
 * appropiate bit in the \a parent_last parameter.
 *
 * @param patches_ptr Pointer to current values of all settings
 * @param base_x      Left-most position in window/panel to start drawing \a first_row
 * @param base_y      Upper-most position in window/panel to start drawing \a first_row
 * @param first_row   First row number to draw
 * @param max_row     Row-number to stop drawing (the row-number of the row below the last row to draw)
 * @param cur_row     Current row number (internal variable)
 * @param parent_last Last-field booleans of parent page level (page level \e i sets bit \e i to 1 if it is its last field)
 * @return Row number of the next row to draw
 */
uint PatchEntry::Draw(GameSettings *patches_ptr, int base_x, int base_y, uint first_row, uint max_row, uint cur_row, uint parent_last)
{
	if (cur_row >= max_row) return cur_row;

	int x = base_x;
	int y = base_y;
	if (cur_row >= first_row) {
		int colour = _colour_gradient[COLOUR_ORANGE][4];
		y = base_y + (cur_row - first_row) * SETTING_HEIGHT; // Compute correct y start position

		/* Draw vertical for parent nesting levels */
		for (uint lvl = 0; lvl < this->level; lvl++) {
			if (!HasBit(parent_last, lvl)) GfxDrawLine(x + 4, y, x + 4, y + SETTING_HEIGHT - 1, colour);
			x += LEVEL_WIDTH;
		}
		/* draw own |- prefix */
		int halfway_y = y + SETTING_HEIGHT / 2;
		int bottom_y = (flags & PEF_LAST_FIELD) ? halfway_y : y + SETTING_HEIGHT - 1;
		GfxDrawLine(x + 4, y, x + 4, bottom_y, colour);
		/* Small horizontal line from the last vertical line */
		GfxDrawLine(x + 4, halfway_y, x + LEVEL_WIDTH - 4, halfway_y, colour);
		x += LEVEL_WIDTH;
	}

	switch(this->flags & PEF_KIND_MASK) {
		case PEF_SETTING_KIND:
			if (cur_row >= first_row) {
				DrawPatch(patches_ptr, this->d.entry.setting, x, y, this->flags & PEF_BUTTONS_MASK);
			}
			cur_row++;
			break;
		case PEF_SUBTREE_KIND:
			if (cur_row >= first_row) {
				DrawSprite((this->d.sub.folded ? SPR_CIRCLE_FOLDED : SPR_CIRCLE_UNFOLDED), PAL_NONE, x, y);
				DrawString(x + 12, y, this->d.sub.title, TC_FROMSTRING);
			}
			cur_row++;
			if (!this->d.sub.folded) {
				if (this->flags & PEF_LAST_FIELD) {
					assert(this->level < sizeof(parent_last));
					SetBit(parent_last, this->level); // Add own last-field state
				}

				cur_row = this->d.sub.page->Draw(patches_ptr, base_x, base_y, first_row, max_row, cur_row, parent_last);
			}
			break;
		default: NOT_REACHED();
	}
	return cur_row;
}

/**
 * Private function to draw setting value (button + text + current value)
 * @param patches_ptr Pointer to current values of all settings
 * @param sd          Pointer to value description of setting to draw
 * @param x           Left-most position in window/panel to start drawing
 * @param y           Upper-most position in window/panel to start drawing
 * @param state       State of the left + right arrow buttons to draw for the setting
 */
void PatchEntry::DrawPatch(GameSettings *patches_ptr, const SettingDesc *sd, int x, int y, int state)
{
	const SettingDescBase *sdb = &sd->desc;
	const void *var = GetVariableAddress(patches_ptr, &sd->save);
	bool editable = true;
	bool disabled = false;

	/* We do not allow changes of some items when we are a client in a networkgame */
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
		DrawArrowButtons(x, y, COLOUR_YELLOW, state, editable && value != (sdb->flags & SGF_0ISDISABLED ? 0 : sdb->min), editable && value != sdb->max);

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
	DrawString(x + 25, y, (sdb->str) + disabled, TC_FROMSTRING);
}


/* == PatchPage methods == */

/**
 * Initialization of an entire setting page
 * @param level Nesting level of this page (internal variable, do not provide a value for it when calling)
 */
void PatchPage::Init(byte level)
{
	for (uint field = 0; field < this->num; field++) {
		this->entries[field].Init(level, field + 1 == num);
	}
}

/** Recursively close all folds of sub-pages */
void PatchPage::FoldAll()
{
	for (uint field = 0; field < this->num; field++) {
		this->entries[field].FoldAll();
	}
}

/** Return number of rows needed to display the whole page */
uint PatchPage::Length() const
{
	uint length = 0;
	for (uint field = 0; field < this->num; field++) {
		length += this->entries[field].Length();
	}
	return length;
}

/**
 * Find the patch entry at row number \a row_num
 * @param row_num Index of entry to return
 * @param cur_row Variable used for keeping track of the current row number. Should point to memory initialized to \c 0 when first called.
 * @return The requested patch entry or \c NULL if it does not exist
 */
PatchEntry *PatchPage::FindEntry(uint row_num, uint *cur_row) const
{
	PatchEntry *pe = NULL;

	for (uint field = 0; field < this->num; field++) {
		pe = this->entries[field].FindEntry(row_num, cur_row);
		if (pe != NULL) {
			break;
		}
	}
	return pe;
}

/**
 * Draw a selected part of the settings page.
 *
 * The scrollbar uses rows of the page, while the page data strucure is a tree of #PatchPage and #PatchEntry objects.
 * As a result, the drawing routing traverses the tree from top to bottom, counting rows in \a cur_row until it reaches \a first_row.
 * Then it enables drawing rows while traversing until \a max_row is reached, at which point drawing is terminated.
 *
 * @param patches_ptr Pointer to current values of all settings
 * @param base_x      Left-most position in window/panel to start drawing of each setting row
 * @param base_y      Upper-most position in window/panel to start drawing of row number \a first_row
 * @param first_row   Number of first row to draw
 * @param max_row     Row-number to stop drawing (the row-number of the row below the last row to draw)
 * @param cur_row     Current row number (internal variable)
 * @param parent_last Last-field booleans of parent page level (page level \e i sets bit \e i to 1 if it is its last field)
 * @return Row number of the next row to draw
 */
uint PatchPage::Draw(GameSettings *patches_ptr, int base_x, int base_y, uint first_row, uint max_row, uint cur_row, uint parent_last) const
{
	if (cur_row >= max_row) return cur_row;

	for (uint i = 0; i < this->num; i++) {
		cur_row = this->entries[i].Draw(patches_ptr, base_x, base_y, first_row, max_row, cur_row, parent_last);
		if (cur_row >= max_row) {
			break;
		}
	}
	return cur_row;
}


static PatchEntry _patches_ui_display[] = {
	PatchEntry("gui.vehicle_speed"),
	PatchEntry("gui.status_long_date"),
	PatchEntry("gui.date_format_in_default_names"),
	PatchEntry("gui.population_in_label"),
	PatchEntry("gui.measure_tooltip"),
	PatchEntry("gui.loading_indicators"),
	PatchEntry("gui.liveries"),
	PatchEntry("gui.show_track_reservation"),
	PatchEntry("gui.expenses_layout"),
};
/** Display options sub-page */
static PatchPage _patches_ui_display_page = {_patches_ui_display, lengthof(_patches_ui_display)};

static PatchEntry _patches_ui_interaction[] = {
	PatchEntry("gui.window_snap_radius"),
	PatchEntry("gui.window_soft_limit"),
	PatchEntry("gui.link_terraform_toolbar"),
	PatchEntry("gui.prefer_teamchat"),
	PatchEntry("gui.autoscroll"),
	PatchEntry("gui.reverse_scroll"),
	PatchEntry("gui.smooth_scroll"),
	PatchEntry("gui.left_mouse_btn_scrolling"),
	/* While the horizontal scrollwheel scrolling is written as general code, only
	 *  the cocoa (OSX) driver generates input for it.
	 *  Since it's also able to completely disable the scrollwheel will we display it on all platforms anyway */
	PatchEntry("gui.scrollwheel_scrolling"),
	PatchEntry("gui.scrollwheel_multiplier"),
#ifdef __APPLE__
	/* We might need to emulate a right mouse button on mac */
	PatchEntry("gui.right_mouse_btn_emulation"),
#endif
};
/** Interaction sub-page */
static PatchPage _patches_ui_interaction_page = {_patches_ui_interaction, lengthof(_patches_ui_interaction)};

static PatchEntry _patches_ui[] = {
	PatchEntry(&_patches_ui_display_page, STR_CONFIG_PATCHES_DISPLAY_OPTIONS),
	PatchEntry(&_patches_ui_interaction_page, STR_CONFIG_PATCHES_INTERACTION),
	PatchEntry("gui.show_finances"),
	PatchEntry("gui.errmsg_duration"),
	PatchEntry("gui.toolbar_pos"),
	PatchEntry("gui.pause_on_newgame"),
	PatchEntry("gui.advanced_vehicle_list"),
	PatchEntry("gui.timetable_in_ticks"),
	PatchEntry("gui.quick_goto"),
	PatchEntry("gui.default_rail_type"),
	PatchEntry("gui.always_build_infrastructure"),
	PatchEntry("gui.persistent_buildingtools"),
	PatchEntry("gui.colored_news_year"),
};
/** Interface subpage */
static PatchPage _patches_ui_page = {_patches_ui, lengthof(_patches_ui)};

static PatchEntry _patches_construction_signals[] = {
	PatchEntry("construction.signal_side"),
	PatchEntry("gui.enable_signal_gui"),
	PatchEntry("gui.drag_signals_density"),
	PatchEntry("gui.semaphore_build_before"),
	PatchEntry("gui.default_signal_type"),
	PatchEntry("gui.cycle_signal_types"),
};
/** Signals subpage */
static PatchPage _patches_construction_signals_page = {_patches_construction_signals, lengthof(_patches_construction_signals)};

static PatchEntry _patches_construction[] = {
	PatchEntry(&_patches_construction_signals_page, STR_CONFIG_PATCHES_CONSTRUCTION_SIGNALS),
	PatchEntry("construction.build_on_slopes"),
	PatchEntry("construction.autoslope"),
	PatchEntry("construction.extra_dynamite"),
	PatchEntry("construction.longbridges"),
	PatchEntry("station.always_small_airport"),
	PatchEntry("construction.freeform_edges"),
};
/** Construction sub-page */
static PatchPage _patches_construction_page = {_patches_construction, lengthof(_patches_construction)};

static PatchEntry _patches_stations_cargo[] = {
	PatchEntry("order.improved_load"),
	PatchEntry("order.gradual_loading"),
	PatchEntry("order.selectgoods"),
};
/** Cargo handling sub-page */
static PatchPage _patches_stations_cargo_page = {_patches_stations_cargo, lengthof(_patches_stations_cargo)};

static PatchEntry _patches_stations[] = {
	PatchEntry(&_patches_stations_cargo_page, STR_CONFIG_PATCHES_STATIONS_CARGOHANDLING),
	PatchEntry("station.join_stations"),
	PatchEntry("station.nonuniform_stations"),
	PatchEntry("station.adjacent_stations"),
	PatchEntry("station.distant_join_stations"),
	PatchEntry("station.station_spread"),
	PatchEntry("economy.station_noise_level"),
	PatchEntry("station.modified_catchment"),
	PatchEntry("construction.road_stop_on_town_road"),
};
/** Stations sub-page */
static PatchPage _patches_stations_page = {_patches_stations, lengthof(_patches_stations)};

static PatchEntry _patches_economy_towns[] = {
	PatchEntry("economy.bribe"),
	PatchEntry("economy.exclusive_rights"),
	PatchEntry("economy.town_layout"),
	PatchEntry("economy.mod_road_rebuild"),
	PatchEntry("economy.town_growth_rate"),
	PatchEntry("economy.larger_towns"),
	PatchEntry("economy.initial_city_size"),
};
/** Towns sub-page */
static PatchPage _patches_economy_towns_page = {_patches_economy_towns, lengthof(_patches_economy_towns)};

static PatchEntry _patches_economy_industries[] = {
	PatchEntry("construction.raw_industry_construction"),
	PatchEntry("economy.multiple_industry_per_town"),
	PatchEntry("economy.same_industry_close"),
	PatchEntry("game_creation.oil_refinery_limit"),
};
/** Industries sub-page */
static PatchPage _patches_economy_industries_page = {_patches_economy_industries, lengthof(_patches_economy_industries)};

static PatchEntry _patches_economy[] = {
	PatchEntry(&_patches_economy_towns_page, STR_CONFIG_PATCHES_ECONOMY_TOWNS),
	PatchEntry(&_patches_economy_industries_page, STR_CONFIG_PATCHES_ECONOMY_INDUSTRIES),
	PatchEntry("economy.inflation"),
	PatchEntry("economy.smooth_economy"),
};
/** Economy sub-page */
static PatchPage _patches_economy_page = {_patches_economy, lengthof(_patches_economy)};

static PatchEntry _patches_ai_npc[] = {
	PatchEntry("ai.ai_in_multiplayer"),
	PatchEntry("ai.ai_disable_veh_train"),
	PatchEntry("ai.ai_disable_veh_roadveh"),
	PatchEntry("ai.ai_disable_veh_aircraft"),
	PatchEntry("ai.ai_disable_veh_ship"),
	PatchEntry("ai.ai_max_opcode_till_suspend"),
};
/** Computer players sub-page */
static PatchPage _patches_ai_npc_page = {_patches_ai_npc, lengthof(_patches_ai_npc)};

static PatchEntry _patches_ai[] = {
	PatchEntry(&_patches_ai_npc_page, STR_CONFIG_PATCHES_AI_NPC),
	PatchEntry("economy.give_money"),
	PatchEntry("economy.allow_shares"),
};
/** AI sub-page */
static PatchPage _patches_ai_page = {_patches_ai, lengthof(_patches_ai)};

static PatchEntry _patches_vehicles_routing[] = {
	PatchEntry("pf.pathfinder_for_trains"),
	PatchEntry("pf.forbid_90_deg"),
	PatchEntry("pf.pathfinder_for_roadvehs"),
	PatchEntry("pf.roadveh_queue"),
	PatchEntry("pf.pathfinder_for_ships"),
};
/** Autorenew sub-page */
static PatchPage _patches_vehicles_routing_page = {_patches_vehicles_routing, lengthof(_patches_vehicles_routing)};

static PatchEntry _patches_vehicles_autorenew[] = {
	PatchEntry("gui.autorenew"),
	PatchEntry("gui.autorenew_months"),
	PatchEntry("gui.autorenew_money"),
};
/** Autorenew sub-page */
static PatchPage _patches_vehicles_autorenew_page = {_patches_vehicles_autorenew, lengthof(_patches_vehicles_autorenew)};

static PatchEntry _patches_vehicles_servicing[] = {
	PatchEntry("vehicle.servint_ispercent"),
	PatchEntry("vehicle.servint_trains"),
	PatchEntry("vehicle.servint_roadveh"),
	PatchEntry("vehicle.servint_ships"),
	PatchEntry("vehicle.servint_aircraft"),
	PatchEntry("order.no_servicing_if_no_breakdowns"),
	PatchEntry("order.serviceathelipad"),
};
/** Servicing sub-page */
static PatchPage _patches_vehicles_servicing_page = {_patches_vehicles_servicing, lengthof(_patches_vehicles_servicing)};

static PatchEntry _patches_vehicles_trains[] = {
	PatchEntry("vehicle.train_acceleration_model"),
	PatchEntry("vehicle.mammoth_trains"),
	PatchEntry("gui.lost_train_warn"),
	PatchEntry("vehicle.wagon_speed_limits"),
	PatchEntry("vehicle.disable_elrails"),
	PatchEntry("vehicle.freight_trains"),
};
/** Trains sub-page */
static PatchPage _patches_vehicles_trains_page = {_patches_vehicles_trains, lengthof(_patches_vehicles_trains)};

static PatchEntry _patches_vehicles[] = {
	PatchEntry(&_patches_vehicles_routing_page, STR_CONFIG_PATCHES_VEHICLES_ROUTING),
	PatchEntry(&_patches_vehicles_autorenew_page, STR_CONFIG_PATCHES_VEHICLES_AUTORENEW),
	PatchEntry(&_patches_vehicles_servicing_page, STR_CONFIG_PATCHES_VEHICLES_SERVICING),
	PatchEntry(&_patches_vehicles_trains_page, STR_CONFIG_PATCHES_VEHICLES_TRAINS),
	PatchEntry("order.gotodepot"),
	PatchEntry("gui.new_nonstop"),
	PatchEntry("gui.order_review_system"),
	PatchEntry("gui.vehicle_income_warn"),
	PatchEntry("vehicle.never_expire_vehicles"),
	PatchEntry("vehicle.max_trains"),
	PatchEntry("vehicle.max_roadveh"),
	PatchEntry("vehicle.max_aircraft"),
	PatchEntry("vehicle.max_ships"),
	PatchEntry("vehicle.plane_speed"),
	PatchEntry("order.timetabling"),
	PatchEntry("vehicle.dynamic_engines"),
};
/** Vehicles sub-page */
static PatchPage _patches_vehicles_page = {_patches_vehicles, lengthof(_patches_vehicles)};

static PatchEntry _patches_main[] = {
	PatchEntry(&_patches_ui_page,           STR_CONFIG_PATCHES_GUI),
	PatchEntry(&_patches_construction_page, STR_CONFIG_PATCHES_CONSTRUCTION),
	PatchEntry(&_patches_vehicles_page,     STR_CONFIG_PATCHES_VEHICLES),
	PatchEntry(&_patches_stations_page,     STR_CONFIG_PATCHES_STATIONS),
	PatchEntry(&_patches_economy_page,      STR_CONFIG_PATCHES_ECONOMY),
	PatchEntry(&_patches_ai_page,           STR_CONFIG_PATCHES_AI),
};

/** Main page, holding all advanced settings */
static PatchPage _patches_main_page = {_patches_main, lengthof(_patches_main)};

/** Widget numbers of config patches window */
enum PatchesSelectionWidgets {
	PATCHSEL_OPTIONSPANEL = 2, ///< Panel widget containing the option lists
	PATCHSEL_SCROLLBAR,        ///< Scrollbar
	PATCHSEL_RESIZE,           ///< Resize button
};

struct PatchesSelectionWindow : Window {
	static const int SETTINGTREE_LEFT_OFFSET; ///< Position of left edge of patch values
	static const int SETTINGTREE_TOP_OFFSET;  ///< Position of top edge of patch values

	static GameSettings *patches_ptr;  ///< Pointer to the game settings being displayed and modified

	PatchEntry *valuewindow_entry; ///< If non-NULL, pointer to patch setting for which a value-entering window has been opened
	PatchEntry *clicked_entry; ///< If non-NULL, pointer to a clicked numeric patch setting (with a depressed left or right button)

	PatchesSelectionWindow(const WindowDesc *desc) : Window(desc)
	{
		/* Check that the widget doesn't get moved without adapting the constant as well.
		 *  - SETTINGTREE_LEFT_OFFSET should be 5 pixels to the right of the left edge of the panel
		 *  - SETTINGTREE_TOP_OFFSET should be 5 pixels below the top edge of the panel
		 */
		assert(this->widget[PATCHSEL_OPTIONSPANEL].left + 5 == SETTINGTREE_LEFT_OFFSET);
		assert(this->widget[PATCHSEL_OPTIONSPANEL].top + 5 == SETTINGTREE_TOP_OFFSET);

		static bool first_time = true;

		patches_ptr = (_game_mode == GM_MENU) ? &_settings_newgame : &_settings_game;

		/* Build up the dynamic settings-array only once per OpenTTD session */
		if (first_time) {
			_patches_main_page.Init();
			first_time = false;
		} else {
			_patches_main_page.FoldAll(); // Close all sub-pages
		}

		this->valuewindow_entry = NULL; // No patch entry for which a entry window is opened
		this->clicked_entry = NULL; // No numeric patch setting buttons are depressed
		this->vscroll.pos = 0;
		this->vscroll.cap = (this->widget[PATCHSEL_OPTIONSPANEL].bottom - this->widget[PATCHSEL_OPTIONSPANEL].top - 8) / SETTING_HEIGHT;
		SetVScrollCount(this, _patches_main_page.Length());

		this->resize.step_height = SETTING_HEIGHT;
		this->resize.height = this->height;
		this->resize.step_width = 1;
		this->resize.width = this->width;

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
		_patches_main_page.Draw(patches_ptr, SETTINGTREE_LEFT_OFFSET, SETTINGTREE_TOP_OFFSET,
						this->vscroll.pos, this->vscroll.pos + this->vscroll.cap);
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget != PATCHSEL_OPTIONSPANEL) return;

		int y = pt.y - SETTINGTREE_TOP_OFFSET;  // Shift y coordinate
		if (y < 0) return;  // Clicked above first entry

		byte btn = this->vscroll.pos + y / SETTING_HEIGHT;  // Compute which setting is selected
		if (y % SETTING_HEIGHT > SETTING_HEIGHT - 2) return;  // Clicked too low at the setting

		uint cur_row = 0;
		PatchEntry *pe = _patches_main_page.FindEntry(btn, &cur_row);

		if (pe == NULL) return;  // Clicked below the last setting of the page

		int x = pt.x - SETTINGTREE_LEFT_OFFSET - (pe->level + 1) * LEVEL_WIDTH;  // Shift x coordinate
		if (x < 0) return;  // Clicked left of the entry

		if ((pe->flags & PEF_KIND_MASK) == PEF_SUBTREE_KIND) {
			pe->d.sub.folded = !pe->d.sub.folded; // Flip 'folded'-ness of the sub-page

			SetVScrollCount(this, _patches_main_page.Length());
			this->SetDirty();
			return;
		}

		assert((pe->flags & PEF_KIND_MASK) == PEF_SETTING_KIND);
		const SettingDesc *sd = pe->d.entry.setting;

		/* return if action is only active in network, or only settable by server */
		if (!(sd->save.conv & SLF_NETWORK_NO) && _networking && !_network_server) return;
		if ((sd->desc.flags & SGF_NETWORK_ONLY) && !_networking) return;
		if ((sd->desc.flags & SGF_NO_NETWORK) && _networking) return;

		void *var = GetVariableAddress(patches_ptr, &sd->save);
		int32 value = (int32)ReadValue(var, sd->save.conv);

		/* clicked on the icon on the left side. Either scroller or bool on/off */
		if (x < 21) {
			const SettingDescBase *sdb = &sd->desc;
			int32 oldvalue = value;

			switch (sdb->cmd) {
				case SDT_BOOLX: value ^= 1; break;
				case SDT_ONEOFMANY:
				case SDT_NUMX: {
					/* Add a dynamic step-size to the scroller. In a maximum of
					 * 50-steps you should be able to get from min to max,
					 * unless specified otherwise in the 'interval' variable
					 * of the current patch. */
					uint32 step = (sdb->interval == 0) ? ((sdb->max - sdb->min) / 50) : sdb->interval;
					if (step == 0) step = 1;

					/* don't allow too fast scrolling */
					if ((this->flags4 & WF_TIMEOUT_MASK) > WF_TIMEOUT_TRIGGER) {
						_left_button_clicked = false;
						return;
					}

					/* Increase or decrease the value and clamp it to extremes */
					if (x >= 10) {
						value += step;
						if (value > sdb->max) value = sdb->max;
						if (value < sdb->min) value = sdb->min; // skip between "disabled" and minimum
					} else {
						value -= step;
						if (value < sdb->min) value = (sdb->flags & SGF_0ISDISABLED) ? 0 : sdb->min;
					}

					/* Set up scroller timeout for numeric values */
					if (value != oldvalue && !(sd->desc.flags & SGF_MULTISTRING)) {
						if (this->clicked_entry != NULL) { // Release previous buttons if any
							this->clicked_entry->SetButtons(0);
						}
						this->clicked_entry = pe;
						this->clicked_entry->SetButtons((x >= 10) ? PEF_RIGHT_DEPRESSED : PEF_LEFT_DEPRESSED);
						this->flags4 |= WF_TIMEOUT_BEGIN;
						_left_button_clicked = false;
					}
				} break;

				default: NOT_REACHED();
			}

			if (value != oldvalue) {
				SetPatchValue(pe->d.entry.index, value);
				this->SetDirty();
			}
		} else {
			/* only open editbox for types that its sensible for */
			if (sd->desc.cmd != SDT_BOOLX && !(sd->desc.flags & SGF_MULTISTRING)) {
				/* Show the correct currency-translated value */
				if (sd->desc.flags & SGF_CURRENCY) value *= _currency->rate;

				this->valuewindow_entry = pe;
				SetDParam(0, value);
				ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_CONFIG_PATCHES_QUERY_CAPT, 10, 100, this, CS_NUMERAL, QSF_NONE);
			}
		}
	}

	virtual void OnTimeout()
	{
		if (this->clicked_entry != NULL) { // On timeout, release any depressed buttons
			this->clicked_entry->SetButtons(0);
			this->clicked_entry = NULL;
			this->SetDirty();
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (!StrEmpty(str)) {
			assert(this->valuewindow_entry != NULL);
			assert((this->valuewindow_entry->flags & PEF_KIND_MASK) == PEF_SETTING_KIND);
			const SettingDesc *sd = this->valuewindow_entry->d.entry.setting;
			int32 value = atoi(str);

			/* Save the correct currency-translated value */
			if (sd->desc.flags & SGF_CURRENCY) value /= _currency->rate;

			SetPatchValue(this->valuewindow_entry->d.entry.index, value);
			this->SetDirty();
		}
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		this->vscroll.cap += delta.y / SETTING_HEIGHT;
		SetVScrollCount(this, _patches_main_page.Length());
	}
};

GameSettings *PatchesSelectionWindow::patches_ptr = NULL;
const int PatchesSelectionWindow::SETTINGTREE_LEFT_OFFSET = 5;
const int PatchesSelectionWindow::SETTINGTREE_TOP_OFFSET = 19;

static const Widget _patches_selection_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_MAUVE,     0,    10,     0,    13, STR_00C5,                        STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,  COLOUR_MAUVE,    11,   411,     0,    13, STR_CONFIG_PATCHES_CAPTION,      STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,     RESIZE_RB,  COLOUR_MAUVE,     0,   399,    14,   187, 0x0,                             STR_NULL}, // PATCHSEL_OPTIONSPANEL
{  WWT_SCROLLBAR,    RESIZE_LRB,  COLOUR_MAUVE,   400,   411,    14,   175, 0x0,                             STR_0190_SCROLL_BAR_SCROLLS_LIST}, // PATCHSEL_SCROLLBAR
{  WWT_RESIZEBOX,   RESIZE_LRTB,  COLOUR_MAUVE,   400,   411,   176,   187, 0x0,                             STR_RESIZE_BUTTON}, // PATCHSEL_RESIZE
{   WIDGETS_END},
};

static const WindowDesc _patches_selection_desc = {
	WDP_CENTER, WDP_CENTER, 412, 188, 450, 397,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_RESIZABLE,
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
 * @param button_colour the colour of the button
 * @param state 0 = none clicked, 1 = first clicked, 2 = second clicked
 * @param clickable_left is the left button clickable?
 * @param clickable_right is the right button clickable?
 */
void DrawArrowButtons(int x, int y, Colours button_colour, byte state, bool clickable_left, bool clickable_right)
{
	int colour = _colour_gradient[button_colour][2];

	DrawFrameRect(x,      y + 1, x +  9, y + 9, button_colour, (state == 1) ? FR_LOWERED : FR_NONE);
	DrawFrameRect(x + 10, y + 1, x + 19, y + 9, button_colour, (state == 2) ? FR_LOWERED : FR_NONE);
	DrawStringCentered(x +  5, y + 1, STR_6819, TC_FROMSTRING); // [<]
	DrawStringCentered(x + 15, y + 1, STR_681A, TC_FROMSTRING); // [>]

	/* Grey out the buttons that aren't clickable */
	if (!clickable_left) {
		GfxFillRect(x +  1, y + 1, x +  1 + 8, y + 8, colour, FILLRECT_CHECKER);
	}
	if (!clickable_right) {
		GfxFillRect(x + 11, y + 1, x + 11 + 8, y + 8, colour, FILLRECT_CHECKER);
	}
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
		DrawArrowButtons(10, y, COLOUR_YELLOW, GB(this->click, 0, 2), true, true);
		SetDParam(0, 1);
		SetDParam(1, 1);
		DrawString(35, y + 1, STR_CURRENCY_EXCHANGE_RATE, TC_FROMSTRING);
		y += 12;

		/* separator */
		DrawFrameRect(10, y + 1, 29, y + 9, COLOUR_DARK_BLUE, GB(this->click, 2, 2) ? FR_LOWERED : FR_NONE);
		x = DrawString(35, y + 1, STR_CURRENCY_SEPARATOR, TC_FROMSTRING);
		DoDrawString(this->separator, x + 4, y + 1, TC_ORANGE);
		y += 12;

		/* prefix */
		DrawFrameRect(10, y + 1, 29, y + 9, COLOUR_DARK_BLUE, GB(this->click, 4, 2) ? FR_LOWERED : FR_NONE);
		x = DrawString(35, y + 1, STR_CURRENCY_PREFIX, TC_FROMSTRING);
		DoDrawString(_custom_currency.prefix, x + 4, y + 1, TC_ORANGE);
		y += 12;

		/* suffix */
		DrawFrameRect(10, y + 1, 29, y + 9, COLOUR_DARK_BLUE, GB(this->click, 6, 2) ? FR_LOWERED : FR_NONE);
		x = DrawString(35, y + 1, STR_CURRENCY_SUFFIX, TC_FROMSTRING);
		DoDrawString(_custom_currency.suffix, x + 4, y + 1, TC_ORANGE);
		y += 12;

		/* switch to euro */
		DrawArrowButtons(10, y, COLOUR_YELLOW, GB(this->click, 8, 2), true, true);
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
						if (_custom_currency.rate < UINT16_MAX) _custom_currency.rate++;
						this->click = 1 << (line * 2 + 1);
					}
				} else { // enter text
					SetDParam(0, _custom_currency.rate);
					str = STR_CONFIG_PATCHES_INT32;
					len = 5;
					afilter = CS_NUMERAL;
				}
				break;

			case CUSTCURR_SEPARATOR:
				if (IsInsideMM(x, 10, 30)) { // clicked button
					this->click = 1 << (line * 2 + 1);
				}
				SetDParamStr(0, this->separator);
				str = STR_JUST_RAW_STRING;
				len = 1;
				break;

			case CUSTCURR_PREFIX:
				if (IsInsideMM(x, 10, 30)) { // clicked button
					this->click = 1 << (line * 2 + 1);
				}
				SetDParamStr(0, _custom_currency.prefix);
				str = STR_JUST_RAW_STRING;
				len = 12;
				break;

			case CUSTCURR_SUFFIX:
				if (IsInsideMM(x, 10, 30)) { // clicked button
					this->click = 1 << (line * 2 + 1);
				}
				SetDParamStr(0, _custom_currency.suffix);
				str = STR_JUST_RAW_STRING;
				len = 12;
				break;

			case CUSTCURR_TO_EURO:
				if (IsInsideMM(x, 10, 30)) { // clicked buttons
					if (x < 20) {
						_custom_currency.to_euro = (_custom_currency.to_euro <= 2000) ? CF_NOEURO : _custom_currency.to_euro - 1;
						this->click = 1 << (line * 2 + 0);
					} else {
						_custom_currency.to_euro = Clamp(_custom_currency.to_euro + 1, 2000, MAX_YEAR);
						this->click = 1 << (line * 2 + 1);
					}
				} else { // enter text
					SetDParam(0, _custom_currency.to_euro);
					str = STR_CONFIG_PATCHES_INT32;
					len = 7;
					afilter = CS_NUMERAL;
				}
				break;
		}

		if (len != 0) {
			this->query_widget = line;
			ShowQueryString(str, STR_CURRENCY_CHANGE_PARAMETER, len + 1, 250, this, afilter, QSF_NONE);
		}

		this->flags4 |= WF_TIMEOUT_BEGIN;
		this->SetDirty();
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		switch (this->query_widget) {
			case CUSTCURR_EXCHANGERATE:
				_custom_currency.rate = Clamp(atoi(str), 1, UINT16_MAX);
				break;

			case CUSTCURR_SEPARATOR: /* Thousands seperator */
				_custom_currency.separator = StrEmpty(str) ? ' ' : str[0];
				strecpy(this->separator, str, lastof(this->separator));
				break;

			case CUSTCURR_PREFIX:
				strecpy(_custom_currency.prefix, str, lastof(_custom_currency.prefix));
				break;

			case CUSTCURR_SUFFIX:
				strecpy(_custom_currency.suffix, str, lastof(_custom_currency.suffix));
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
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,            STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   229,     0,    13, STR_CURRENCY_WINDOW, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   229,    14,   119, 0x0,                 STR_NULL},
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
