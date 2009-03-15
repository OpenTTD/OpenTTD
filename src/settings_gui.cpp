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
#include "gfxinit.h"
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
	GAMEOPT_LANG_BTN        = 14,
	GAMEOPT_RESOLUTION_BTN  = 16,
	GAMEOPT_FULLSCREEN,
	GAMEOPT_SCREENSHOT_BTN  = 19,
	GAMEOPT_BASE_GRF_BTN    = 21,
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

static void ShowGraphicsSetMenu(Window *w)
{
	int n = GetNumGraphicsSets();
	int current = GetIndexOfCurrentGraphicsSet();

	DropDownList *list = new DropDownList();
	for (int i = 0; i < n; i++) {
		list->push_back(new DropDownListCharStringItem(GetGraphicsSetName(i), i, (_game_mode == GM_MENU) ? false : (current != i)));
	}

	ShowDropDownList(w, list, current, GAMEOPT_BASE_GRF_BTN);
}

struct GameOptionsWindow : Window {
	GameSettings *opt;
	bool reload;

	GameOptionsWindow(const WindowDesc *desc) : Window(desc)
	{
		this->opt = (_game_mode == GM_MENU) ? &_settings_newgame : &_settings_game;
		this->reload = false;
		this->FindWindowPlacementAndResize(desc);
	}

	~GameOptionsWindow()
	{
		DeleteWindowById(WC_CUSTOM_CURRENCY, 0);
		if (this->reload) _switch_mode = SM_MENU;
	}

	virtual void OnPaint()
	{
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
		SetDParamStr(9, GetGraphicsSetName(GetIndexOfCurrentGraphicsSet()));

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

			case GAMEOPT_BASE_GRF_BTN:
				ShowGraphicsSetMenu(this);
				break;
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case GAMEOPT_CURRENCY_BTN: // Currency
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
					if (GetSettingFromName("vehicle.road_side", &i) == NULL) NOT_REACHED();
					SetSettingValue(i, index);
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

			case GAMEOPT_BASE_GRF_BTN:
				if (_game_mode == GM_MENU) {
					const char *name = GetGraphicsSetName(index);

					free(_ini_graphics_set);
					_ini_graphics_set = strdup(name);

					SetGraphicsSet(name);
					this->reload = true;
				}
				break;
		}
	}
};

static const Widget _game_options_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   369,     0,    13, STR_00B1_GAME_OPTIONS,             STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   369,    14,   242, 0x0,                               STR_NULL},
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

{      WWT_FRAME,   RESIZE_NONE,  COLOUR_GREY,   190,   359,   104,   139, STR_OPTIONS_LANG,                  STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,  COLOUR_GREY,   200,   349,   118,   129, STR_OPTIONS_LANG_CBO,              STR_OPTIONS_LANG_TIP},

{      WWT_FRAME,   RESIZE_NONE,  COLOUR_GREY,    10,   179,   146,   190, STR_OPTIONS_RES,                   STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,  COLOUR_GREY,    20,   169,   160,   171, STR_OPTIONS_RES_CBO,               STR_OPTIONS_RES_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,   149,   169,   176,   184, STR_EMPTY,                         STR_OPTIONS_FULLSCREEN_TIP},

{      WWT_FRAME,   RESIZE_NONE,  COLOUR_GREY,   190,   359,   146,   190, STR_OPTIONS_SCREENSHOT_FORMAT,     STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,  COLOUR_GREY,   200,   349,   160,   171, STR_OPTIONS_SCREENSHOT_FORMAT_CBO, STR_OPTIONS_SCREENSHOT_FORMAT_TIP},

{      WWT_FRAME,   RESIZE_NONE,  COLOUR_GREY,    10,   179,   197,   232, STR_OPTIONS_BASE_GRF,              STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,  COLOUR_GREY,    20,   169,   211,   222, STR_OPTIONS_BASE_GRF_CBO,          STR_OPTIONS_BASE_GRF_TIP},

{   WIDGETS_END},
};

static const WindowDesc _game_options_desc(
	WDP_CENTER, WDP_CENTER, 370, 243, 370, 243,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_game_options_widgets
);


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
static const WindowDesc _game_difficulty_desc(
	WDP_CENTER, WDP_CENTER, 370, 279, 370, 279,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_game_difficulty_widgets
);

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
		const SettingDesc *sd = GetSettingFromName("difficulty.max_no_competitors", &i);
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
			case GDW_SETTING_BG: { // Difficulty settings widget, decode click
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
				const SettingDesc *sd = GetSettingFromName("difficulty.max_no_competitors", &i) + btn;
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
				const SettingDesc *sd = GetSettingFromName("difficulty.max_no_competitors", &i);
				for (uint btn = 0; btn != GAME_DIFFICULTY_NUM; btn++, sd++) {
					int32 new_val = (int32)ReadValue(GetVariableAddress(&this->opt_mod_temp, &sd->save), sd->save.conv);
					int32 cur_val = (int32)ReadValue(GetVariableAddress(opt_ptr, &sd->save), sd->save.conv);
					/* if setting has changed, change it */
					if (new_val != cur_val) {
						DoCommandP(0, i + btn, new_val, CMD_CHANGE_SETTING);
					}
				}

				GetSettingFromName("difficulty.diff_level", &i);
				DoCommandP(0, i, this->opt_mod_temp.difficulty.diff_level, CMD_CHANGE_SETTING);
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
 * Flags for #SettingEntry
 * @note The #SEF_BUTTONS_MASK matches expectations of the formal parameter 'state' of #DrawArrowButtons
 */
enum SettingEntryFlags {
	SEF_LEFT_DEPRESSED  = 0x01, ///< Of a numeric setting entry, the left button is depressed
	SEF_RIGHT_DEPRESSED = 0x02, ///< Of a numeric setting entry, the right button is depressed
	SEF_BUTTONS_MASK = (SEF_LEFT_DEPRESSED | SEF_RIGHT_DEPRESSED), ///< Bit-mask for button flags

	SEF_LAST_FIELD = 0x04, ///< This entry is the last one in a (sub-)page

	/* Entry kind */
	SEF_SETTING_KIND = 0x10, ///< Entry kind: Entry is a setting
	SEF_SUBTREE_KIND = 0x20, ///< Entry kind: Entry is a sub-tree
	SEF_KIND_MASK    = (SEF_SETTING_KIND | SEF_SUBTREE_KIND), ///< Bit-mask for fetching entry kind
};

struct SettingsPage; // Forward declaration

/** Data fields for a sub-page (#SEF_SUBTREE_KIND kind)*/
struct SettingEntrySubtree {
	SettingsPage *page; ///< Pointer to the sub-page
	bool folded;        ///< Sub-page is folded (not visible except for its title)
	StringID title;     ///< Title of the sub-page
};

/** Data fields for a single setting (#SEF_SETTING_KIND kind) */
struct SettingEntrySetting {
	const char *name;           ///< Name of the setting
	const SettingDesc *setting; ///< Setting description of the setting
	uint index;                 ///< Index of the setting in the settings table
};

/** Data structure describing a single setting in a tab */
struct SettingEntry {
	byte flags; ///< Flags of the setting entry. @see SettingEntryFlags
	byte level; ///< Nesting level of this setting entry
	union {
		SettingEntrySetting entry; ///< Data fields if entry is a setting
		SettingEntrySubtree sub;   ///< Data fields if entry is a sub-page
	} d; ///< Data fields for each kind

	SettingEntry(const char *nm);
	SettingEntry(SettingsPage *sub, StringID title);

	void Init(byte level, bool last_field);
	void FoldAll();
	void SetButtons(byte new_val);

	uint Length() const;
	SettingEntry *FindEntry(uint row, uint *cur_row);

	uint Draw(GameSettings *settings_ptr, int base_x, int base_y, int max_x, uint first_row, uint max_row, uint cur_row, uint parent_last);

private:
	void DrawSetting(GameSettings *settings_ptr, const SettingDesc *sd, int x, int y, int max_x, int state);
};

/** Data structure describing one page of settings in the settings window. */
struct SettingsPage {
	SettingEntry *entries; ///< Array of setting entries of the page.
	byte num;              ///< Number of entries on the page (statically filled).

	void Init(byte level = 0);
	void FoldAll();

	uint Length() const;
	SettingEntry *FindEntry(uint row, uint *cur_row) const;

	uint Draw(GameSettings *settings_ptr, int base_x, int base_y, int max_x, uint first_row, uint max_row, uint cur_row = 0, uint parent_last = 0) const;
};


/* == SettingEntry methods == */

/**
 * Constructor for a single setting in the 'advanced settings' window
 * @param nm Name of the setting in the setting table
 */
SettingEntry::SettingEntry(const char *nm)
{
	this->flags = SEF_SETTING_KIND;
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
SettingEntry::SettingEntry(SettingsPage *sub, StringID title)
{
	this->flags = SEF_SUBTREE_KIND;
	this->level = 0;
	this->d.sub.page = sub;
	this->d.sub.folded = true;
	this->d.sub.title = title;
}

/**
 * Initialization of a setting entry
 * @param level      Page nesting level of this entry
 * @param last_field Boolean indicating this entry is the last at the (sub-)page
 */
void SettingEntry::Init(byte level, bool last_field)
{
	this->level = level;
	if (last_field) this->flags |= SEF_LAST_FIELD;

	switch (this->flags & SEF_KIND_MASK) {
		case SEF_SETTING_KIND:
			this->d.entry.setting = GetSettingFromName(this->d.entry.name, &this->d.entry.index);
			assert(this->d.entry.setting != NULL);
			break;
		case SEF_SUBTREE_KIND:
			this->d.sub.page->Init(level + 1);
			break;
		default: NOT_REACHED();
	}
}

/** Recursively close all folds of sub-pages */
void SettingEntry::FoldAll()
{
	switch(this->flags & SEF_KIND_MASK) {
		case SEF_SETTING_KIND:
			break;

		case SEF_SUBTREE_KIND:
			this->d.sub.folded = true;
			this->d.sub.page->FoldAll();
			break;

		default: NOT_REACHED();
	}
}


/**
 * Set the button-depressed flags (#SEF_LEFT_DEPRESSED and #SEF_RIGHT_DEPRESSED) to a specified value
 * @param new_val New value for the button flags
 * @see SettingEntryFlags
 */
void SettingEntry::SetButtons(byte new_val)
{
	assert((new_val & ~SEF_BUTTONS_MASK) == 0); // Should not touch any flags outside the buttons
	this->flags = (this->flags & ~SEF_BUTTONS_MASK) | new_val;
}

/** Return numbers of rows needed to display the entry */
uint SettingEntry::Length() const
{
	switch (this->flags & SEF_KIND_MASK) {
		case SEF_SETTING_KIND:
			return 1;
		case SEF_SUBTREE_KIND:
			if (this->d.sub.folded) return 1; // Only displaying the title

			return 1 + this->d.sub.page->Length(); // 1 extra row for the title
		default: NOT_REACHED();
	}
}

/**
 * Find setting entry at row \a row_num
 * @param row_num Index of entry to return
 * @param cur_row Current row number
 * @return The requested setting entry or \c NULL if it not found
 */
SettingEntry *SettingEntry::FindEntry(uint row_num, uint *cur_row)
{
	if (row_num == *cur_row) return this;

	switch (this->flags & SEF_KIND_MASK) {
		case SEF_SETTING_KIND:
			(*cur_row)++;
			break;
		case SEF_SUBTREE_KIND:
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
 * See SettingsPage::Draw() for an explanation about how drawing is performed.
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
 * @param settings_ptr Pointer to current values of all settings
 * @param base_x       Left-most position in window/panel to start drawing \a first_row
 * @param base_y       Upper-most position in window/panel to start drawing \a first_row
 * @param max_x        The maximum x position to draw strings add.
 * @param first_row    First row number to draw
 * @param max_row      Row-number to stop drawing (the row-number of the row below the last row to draw)
 * @param cur_row      Current row number (internal variable)
 * @param parent_last  Last-field booleans of parent page level (page level \e i sets bit \e i to 1 if it is its last field)
 * @return Row number of the next row to draw
 */
uint SettingEntry::Draw(GameSettings *settings_ptr, int base_x, int base_y, int max_x, uint first_row, uint max_row, uint cur_row, uint parent_last)
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
		int bottom_y = (flags & SEF_LAST_FIELD) ? halfway_y : y + SETTING_HEIGHT - 1;
		GfxDrawLine(x + 4, y, x + 4, bottom_y, colour);
		/* Small horizontal line from the last vertical line */
		GfxDrawLine(x + 4, halfway_y, x + LEVEL_WIDTH - 4, halfway_y, colour);
		x += LEVEL_WIDTH;
	}

	switch(this->flags & SEF_KIND_MASK) {
		case SEF_SETTING_KIND:
			if (cur_row >= first_row) {
				DrawSetting(settings_ptr, this->d.entry.setting, x, y, max_x, this->flags & SEF_BUTTONS_MASK);
			}
			cur_row++;
			break;
		case SEF_SUBTREE_KIND:
			if (cur_row >= first_row) {
				DrawSprite((this->d.sub.folded ? SPR_CIRCLE_FOLDED : SPR_CIRCLE_UNFOLDED), PAL_NONE, x, y);
				DrawStringTruncated(x + 12, y, this->d.sub.title, TC_FROMSTRING, max_x - x - 12);
			}
			cur_row++;
			if (!this->d.sub.folded) {
				if (this->flags & SEF_LAST_FIELD) {
					assert(this->level < sizeof(parent_last));
					SetBit(parent_last, this->level); // Add own last-field state
				}

				cur_row = this->d.sub.page->Draw(settings_ptr, base_x, base_y, max_x, first_row, max_row, cur_row, parent_last);
			}
			break;
		default: NOT_REACHED();
	}
	return cur_row;
}

/**
 * Private function to draw setting value (button + text + current value)
 * @param settings_ptr Pointer to current values of all settings
 * @param sd           Pointer to value description of setting to draw
 * @param x            Left-most position in window/panel to start drawing
 * @param y            Upper-most position in window/panel to start drawing
 * @param max_x        The maximum x position to draw strings add.
 * @param state        State of the left + right arrow buttons to draw for the setting
 */
void SettingEntry::DrawSetting(GameSettings *settings_ptr, const SettingDesc *sd, int x, int y, int max_x, int state)
{
	const SettingDescBase *sdb = &sd->desc;
	const void *var = GetVariableAddress(settings_ptr, &sd->save);
	bool editable = true;
	bool disabled = false;

	/* We do not allow changes of some items when we are a client in a networkgame */
	if (!(sd->save.conv & SLF_NETWORK_NO) && _networking && !_network_server) editable = false;
	if ((sdb->flags & SGF_NETWORK_ONLY) && !_networking) editable = false;
	if ((sdb->flags & SGF_NO_NETWORK) && _networking) editable = false;

	if (sdb->cmd == SDT_BOOLX) {
		static const Colours _bool_ctabs[2][2] = {{COLOUR_CREAM, COLOUR_RED}, {COLOUR_DARK_GREEN, COLOUR_GREEN}};
		/* Draw checkbox for boolean-value either on/off */
		bool on = (*(bool*)var);

		DrawFrameRect(x, y, x + 19, y + 8, _bool_ctabs[!!on][!!editable], on ? FR_LOWERED : FR_NONE);
		SetDParam(0, on ? STR_CONFIG_SETTING_ON : STR_CONFIG_SETTING_OFF);
	} else {
		int32 value;

		value = (int32)ReadValue(var, sd->save.conv);

		/* Draw [<][>] boxes for settings of an integer-type */
		DrawArrowButtons(x, y, COLOUR_YELLOW, state, editable && value != (sdb->flags & SGF_0ISDISABLED ? 0 : sdb->min), editable && value != sdb->max);

		disabled = (value == 0) && (sdb->flags & SGF_0ISDISABLED);
		if (disabled) {
			SetDParam(0, STR_CONFIG_SETTING_DISABLED);
		} else {
			if (sdb->flags & SGF_CURRENCY) {
				SetDParam(0, STR_CONFIG_SETTING_CURRENCY);
			} else if (sdb->flags & SGF_MULTISTRING) {
				SetDParam(0, sdb->str + value + 1);
			} else {
				SetDParam(0, (sdb->flags & SGF_NOCOMMA) ? STR_CONFIG_SETTING_INT32 : STR_7024);
			}
			SetDParam(1, value);
		}
	}
	DrawStringTruncated(x + 25, y, (sdb->str) + disabled, TC_FROMSTRING, max_x - x - 25);
}


/* == SettingsPage methods == */

/**
 * Initialization of an entire setting page
 * @param level Nesting level of this page (internal variable, do not provide a value for it when calling)
 */
void SettingsPage::Init(byte level)
{
	for (uint field = 0; field < this->num; field++) {
		this->entries[field].Init(level, field + 1 == num);
	}
}

/** Recursively close all folds of sub-pages */
void SettingsPage::FoldAll()
{
	for (uint field = 0; field < this->num; field++) {
		this->entries[field].FoldAll();
	}
}

/** Return number of rows needed to display the whole page */
uint SettingsPage::Length() const
{
	uint length = 0;
	for (uint field = 0; field < this->num; field++) {
		length += this->entries[field].Length();
	}
	return length;
}

/**
 * Find the setting entry at row number \a row_num
 * @param row_num Index of entry to return
 * @param cur_row Variable used for keeping track of the current row number. Should point to memory initialized to \c 0 when first called.
 * @return The requested setting entry or \c NULL if it does not exist
 */
SettingEntry *SettingsPage::FindEntry(uint row_num, uint *cur_row) const
{
	SettingEntry *pe = NULL;

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
 * The scrollbar uses rows of the page, while the page data strucure is a tree of #SettingsPage and #SettingEntry objects.
 * As a result, the drawing routing traverses the tree from top to bottom, counting rows in \a cur_row until it reaches \a first_row.
 * Then it enables drawing rows while traversing until \a max_row is reached, at which point drawing is terminated.
 *
 * @param settings_ptr Pointer to current values of all settings
 * @param base_x       Left-most position in window/panel to start drawing of each setting row
 * @param base_y       Upper-most position in window/panel to start drawing of row number \a first_row
 * @param max_x        The maximum x position to draw strings add.
 * @param first_row    Number of first row to draw
 * @param max_row      Row-number to stop drawing (the row-number of the row below the last row to draw)
 * @param cur_row      Current row number (internal variable)
 * @param parent_last  Last-field booleans of parent page level (page level \e i sets bit \e i to 1 if it is its last field)
 * @return Row number of the next row to draw
 */
uint SettingsPage::Draw(GameSettings *settings_ptr, int base_x, int base_y, int max_x, uint first_row, uint max_row, uint cur_row, uint parent_last) const
{
	if (cur_row >= max_row) return cur_row;

	for (uint i = 0; i < this->num; i++) {
		cur_row = this->entries[i].Draw(settings_ptr, base_x, base_y, max_x, first_row, max_row, cur_row, parent_last);
		if (cur_row >= max_row) {
			break;
		}
	}
	return cur_row;
}


static SettingEntry _settings_ui_display[] = {
	SettingEntry("gui.vehicle_speed"),
	SettingEntry("gui.status_long_date"),
	SettingEntry("gui.date_format_in_default_names"),
	SettingEntry("gui.population_in_label"),
	SettingEntry("gui.measure_tooltip"),
	SettingEntry("gui.loading_indicators"),
	SettingEntry("gui.liveries"),
	SettingEntry("gui.show_track_reservation"),
	SettingEntry("gui.expenses_layout"),
};
/** Display options sub-page */
static SettingsPage _settings_ui_display_page = {_settings_ui_display, lengthof(_settings_ui_display)};

static SettingEntry _settings_ui_interaction[] = {
	SettingEntry("gui.window_snap_radius"),
	SettingEntry("gui.window_soft_limit"),
	SettingEntry("gui.link_terraform_toolbar"),
	SettingEntry("gui.prefer_teamchat"),
	SettingEntry("gui.autoscroll"),
	SettingEntry("gui.reverse_scroll"),
	SettingEntry("gui.smooth_scroll"),
	SettingEntry("gui.left_mouse_btn_scrolling"),
	/* While the horizontal scrollwheel scrolling is written as general code, only
	 *  the cocoa (OSX) driver generates input for it.
	 *  Since it's also able to completely disable the scrollwheel will we display it on all platforms anyway */
	SettingEntry("gui.scrollwheel_scrolling"),
	SettingEntry("gui.scrollwheel_multiplier"),
#ifdef __APPLE__
	/* We might need to emulate a right mouse button on mac */
	SettingEntry("gui.right_mouse_btn_emulation"),
#endif
};
/** Interaction sub-page */
static SettingsPage _settings_ui_interaction_page = {_settings_ui_interaction, lengthof(_settings_ui_interaction)};

static SettingEntry _settings_ui[] = {
	SettingEntry(&_settings_ui_display_page, STR_CONFIG_SETTING_DISPLAY_OPTIONS),
	SettingEntry(&_settings_ui_interaction_page, STR_CONFIG_SETTING_INTERACTION),
	SettingEntry("gui.show_finances"),
	SettingEntry("gui.errmsg_duration"),
	SettingEntry("gui.toolbar_pos"),
	SettingEntry("gui.pause_on_newgame"),
	SettingEntry("gui.advanced_vehicle_list"),
	SettingEntry("gui.timetable_in_ticks"),
	SettingEntry("gui.quick_goto"),
	SettingEntry("gui.default_rail_type"),
	SettingEntry("gui.always_build_infrastructure"),
	SettingEntry("gui.persistent_buildingtools"),
	SettingEntry("gui.coloured_news_year"),
};
/** Interface subpage */
static SettingsPage _settings_ui_page = {_settings_ui, lengthof(_settings_ui)};

static SettingEntry _settings_construction_signals[] = {
	SettingEntry("construction.signal_side"),
	SettingEntry("gui.enable_signal_gui"),
	SettingEntry("gui.drag_signals_density"),
	SettingEntry("gui.semaphore_build_before"),
	SettingEntry("gui.default_signal_type"),
	SettingEntry("gui.cycle_signal_types"),
};
/** Signals subpage */
static SettingsPage _settings_construction_signals_page = {_settings_construction_signals, lengthof(_settings_construction_signals)};

static SettingEntry _settings_construction[] = {
	SettingEntry(&_settings_construction_signals_page, STR_CONFIG_SETTING_CONSTRUCTION_SIGNALS),
	SettingEntry("construction.build_on_slopes"),
	SettingEntry("construction.autoslope"),
	SettingEntry("construction.extra_dynamite"),
	SettingEntry("construction.longbridges"),
	SettingEntry("station.always_small_airport"),
	SettingEntry("construction.freeform_edges"),
};
/** Construction sub-page */
static SettingsPage _settings_construction_page = {_settings_construction, lengthof(_settings_construction)};

static SettingEntry _settings_stations_cargo[] = {
	SettingEntry("order.improved_load"),
	SettingEntry("order.gradual_loading"),
	SettingEntry("order.selectgoods"),
};
/** Cargo handling sub-page */
static SettingsPage _settings_stations_cargo_page = {_settings_stations_cargo, lengthof(_settings_stations_cargo)};

static SettingEntry _settings_stations[] = {
	SettingEntry(&_settings_stations_cargo_page, STR_CONFIG_SETTING_STATIONS_CARGOHANDLING),
	SettingEntry("station.join_stations"),
	SettingEntry("station.nonuniform_stations"),
	SettingEntry("station.adjacent_stations"),
	SettingEntry("station.distant_join_stations"),
	SettingEntry("station.station_spread"),
	SettingEntry("economy.station_noise_level"),
	SettingEntry("station.modified_catchment"),
	SettingEntry("construction.road_stop_on_town_road"),
	SettingEntry("construction.road_stop_on_competitor_road"),
};
/** Stations sub-page */
static SettingsPage _settings_stations_page = {_settings_stations, lengthof(_settings_stations)};

static SettingEntry _settings_economy_towns[] = {
	SettingEntry("economy.bribe"),
	SettingEntry("economy.exclusive_rights"),
	SettingEntry("economy.town_layout"),
	SettingEntry("economy.allow_town_roads"),
	SettingEntry("economy.mod_road_rebuild"),
	SettingEntry("economy.town_growth_rate"),
	SettingEntry("economy.larger_towns"),
	SettingEntry("economy.initial_city_size"),
};
/** Towns sub-page */
static SettingsPage _settings_economy_towns_page = {_settings_economy_towns, lengthof(_settings_economy_towns)};

static SettingEntry _settings_economy_industries[] = {
	SettingEntry("construction.raw_industry_construction"),
	SettingEntry("economy.multiple_industry_per_town"),
	SettingEntry("economy.same_industry_close"),
	SettingEntry("game_creation.oil_refinery_limit"),
};
/** Industries sub-page */
static SettingsPage _settings_economy_industries_page = {_settings_economy_industries, lengthof(_settings_economy_industries)};

static SettingEntry _settings_economy[] = {
	SettingEntry(&_settings_economy_towns_page, STR_CONFIG_SETTING_ECONOMY_TOWNS),
	SettingEntry(&_settings_economy_industries_page, STR_CONFIG_SETTING_ECONOMY_INDUSTRIES),
	SettingEntry("economy.inflation"),
	SettingEntry("economy.smooth_economy"),
};
/** Economy sub-page */
static SettingsPage _settings_economy_page = {_settings_economy, lengthof(_settings_economy)};

static SettingEntry _settings_ai_npc[] = {
	SettingEntry("ai.ai_in_multiplayer"),
	SettingEntry("ai.ai_disable_veh_train"),
	SettingEntry("ai.ai_disable_veh_roadveh"),
	SettingEntry("ai.ai_disable_veh_aircraft"),
	SettingEntry("ai.ai_disable_veh_ship"),
	SettingEntry("ai.ai_max_opcode_till_suspend"),
};
/** Computer players sub-page */
static SettingsPage _settings_ai_npc_page = {_settings_ai_npc, lengthof(_settings_ai_npc)};

static SettingEntry _settings_ai[] = {
	SettingEntry(&_settings_ai_npc_page, STR_CONFIG_SETTING_AI_NPC),
	SettingEntry("economy.give_money"),
	SettingEntry("economy.allow_shares"),
};
/** AI sub-page */
static SettingsPage _settings_ai_page = {_settings_ai, lengthof(_settings_ai)};

static SettingEntry _settings_vehicles_routing[] = {
	SettingEntry("pf.pathfinder_for_trains"),
	SettingEntry("pf.forbid_90_deg"),
	SettingEntry("pf.pathfinder_for_roadvehs"),
	SettingEntry("pf.roadveh_queue"),
	SettingEntry("pf.pathfinder_for_ships"),
};
/** Autorenew sub-page */
static SettingsPage _settings_vehicles_routing_page = {_settings_vehicles_routing, lengthof(_settings_vehicles_routing)};

static SettingEntry _settings_vehicles_autorenew[] = {
	SettingEntry("gui.autorenew"),
	SettingEntry("gui.autorenew_months"),
	SettingEntry("gui.autorenew_money"),
};
/** Autorenew sub-page */
static SettingsPage _settings_vehicles_autorenew_page = {_settings_vehicles_autorenew, lengthof(_settings_vehicles_autorenew)};

static SettingEntry _settings_vehicles_servicing[] = {
	SettingEntry("vehicle.servint_ispercent"),
	SettingEntry("vehicle.servint_trains"),
	SettingEntry("vehicle.servint_roadveh"),
	SettingEntry("vehicle.servint_ships"),
	SettingEntry("vehicle.servint_aircraft"),
	SettingEntry("order.no_servicing_if_no_breakdowns"),
	SettingEntry("order.serviceathelipad"),
};
/** Servicing sub-page */
static SettingsPage _settings_vehicles_servicing_page = {_settings_vehicles_servicing, lengthof(_settings_vehicles_servicing)};

static SettingEntry _settings_vehicles_trains[] = {
	SettingEntry("vehicle.train_acceleration_model"),
	SettingEntry("vehicle.mammoth_trains"),
	SettingEntry("gui.lost_train_warn"),
	SettingEntry("vehicle.wagon_speed_limits"),
	SettingEntry("vehicle.disable_elrails"),
	SettingEntry("vehicle.freight_trains"),
};
/** Trains sub-page */
static SettingsPage _settings_vehicles_trains_page = {_settings_vehicles_trains, lengthof(_settings_vehicles_trains)};

static SettingEntry _settings_vehicles[] = {
	SettingEntry(&_settings_vehicles_routing_page, STR_CONFIG_SETTING_VEHICLES_ROUTING),
	SettingEntry(&_settings_vehicles_autorenew_page, STR_CONFIG_SETTING_VEHICLES_AUTORENEW),
	SettingEntry(&_settings_vehicles_servicing_page, STR_CONFIG_SETTING_VEHICLES_SERVICING),
	SettingEntry(&_settings_vehicles_trains_page, STR_CONFIG_SETTING_VEHICLES_TRAINS),
	SettingEntry("order.gotodepot"),
	SettingEntry("gui.new_nonstop"),
	SettingEntry("gui.order_review_system"),
	SettingEntry("gui.vehicle_income_warn"),
	SettingEntry("vehicle.never_expire_vehicles"),
	SettingEntry("vehicle.max_trains"),
	SettingEntry("vehicle.max_roadveh"),
	SettingEntry("vehicle.max_aircraft"),
	SettingEntry("vehicle.max_ships"),
	SettingEntry("vehicle.plane_speed"),
	SettingEntry("order.timetabling"),
	SettingEntry("vehicle.dynamic_engines"),
};
/** Vehicles sub-page */
static SettingsPage _settings_vehicles_page = {_settings_vehicles, lengthof(_settings_vehicles)};

static SettingEntry _settings_main[] = {
	SettingEntry(&_settings_ui_page,           STR_CONFIG_SETTING_GUI),
	SettingEntry(&_settings_construction_page, STR_CONFIG_SETTING_CONSTRUCTION),
	SettingEntry(&_settings_vehicles_page,     STR_CONFIG_SETTING_VEHICLES),
	SettingEntry(&_settings_stations_page,     STR_CONFIG_SETTING_STATIONS),
	SettingEntry(&_settings_economy_page,      STR_CONFIG_SETTING_ECONOMY),
	SettingEntry(&_settings_ai_page,           STR_CONFIG_SETTING_AI),
};

/** Main page, holding all advanced settings */
static SettingsPage _settings_main_page = {_settings_main, lengthof(_settings_main)};

/** Widget numbers of settings window */
enum GameSettingsWidgets {
	SETTINGSEL_OPTIONSPANEL = 2, ///< Panel widget containing the option lists
	SETTINGSEL_SCROLLBAR,        ///< Scrollbar
	SETTINGSEL_RESIZE,           ///< Resize button
};

struct GameSettingsWindow : Window {
	static const int SETTINGTREE_LEFT_OFFSET; ///< Position of left edge of setting values
	static const int SETTINGTREE_TOP_OFFSET;  ///< Position of top edge of setting values

	static GameSettings *settings_ptr;  ///< Pointer to the game settings being displayed and modified

	SettingEntry *valuewindow_entry; ///< If non-NULL, pointer to setting for which a value-entering window has been opened
	SettingEntry *clicked_entry; ///< If non-NULL, pointer to a clicked numeric setting (with a depressed left or right button)

	GameSettingsWindow(const WindowDesc *desc) : Window(desc)
	{
		/* Check that the widget doesn't get moved without adapting the constant as well.
		 *  - SETTINGTREE_LEFT_OFFSET should be 5 pixels to the right of the left edge of the panel
		 *  - SETTINGTREE_TOP_OFFSET should be 5 pixels below the top edge of the panel
		 */
		assert(this->widget[SETTINGSEL_OPTIONSPANEL].left + 5 == SETTINGTREE_LEFT_OFFSET);
		assert(this->widget[SETTINGSEL_OPTIONSPANEL].top + 5 == SETTINGTREE_TOP_OFFSET);

		static bool first_time = true;

		settings_ptr = (_game_mode == GM_MENU) ? &_settings_newgame : &_settings_game;

		/* Build up the dynamic settings-array only once per OpenTTD session */
		if (first_time) {
			_settings_main_page.Init();
			first_time = false;
		} else {
			_settings_main_page.FoldAll(); // Close all sub-pages
		}

		this->valuewindow_entry = NULL; // No setting entry for which a entry window is opened
		this->clicked_entry = NULL; // No numeric setting buttons are depressed
		this->vscroll.pos = 0;
		this->vscroll.cap = (this->widget[SETTINGSEL_OPTIONSPANEL].bottom - this->widget[SETTINGSEL_OPTIONSPANEL].top - 8) / SETTING_HEIGHT;
		SetVScrollCount(this, _settings_main_page.Length());

		this->resize.step_height = SETTING_HEIGHT;
		this->resize.height = this->height;
		this->resize.step_width = 1;
		this->resize.width = this->width;

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
		_settings_main_page.Draw(settings_ptr, SETTINGTREE_LEFT_OFFSET, SETTINGTREE_TOP_OFFSET,
				this->width - 13, this->vscroll.pos, this->vscroll.pos + this->vscroll.cap);
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget != SETTINGSEL_OPTIONSPANEL) return;

		int y = pt.y - SETTINGTREE_TOP_OFFSET;  // Shift y coordinate
		if (y < 0) return;  // Clicked above first entry

		byte btn = this->vscroll.pos + y / SETTING_HEIGHT;  // Compute which setting is selected
		if (y % SETTING_HEIGHT > SETTING_HEIGHT - 2) return;  // Clicked too low at the setting

		uint cur_row = 0;
		SettingEntry *pe = _settings_main_page.FindEntry(btn, &cur_row);

		if (pe == NULL) return;  // Clicked below the last setting of the page

		int x = pt.x - SETTINGTREE_LEFT_OFFSET - (pe->level + 1) * LEVEL_WIDTH;  // Shift x coordinate
		if (x < 0) return;  // Clicked left of the entry

		if ((pe->flags & SEF_KIND_MASK) == SEF_SUBTREE_KIND) {
			pe->d.sub.folded = !pe->d.sub.folded; // Flip 'folded'-ness of the sub-page

			SetVScrollCount(this, _settings_main_page.Length());
			this->SetDirty();
			return;
		}

		assert((pe->flags & SEF_KIND_MASK) == SEF_SETTING_KIND);
		const SettingDesc *sd = pe->d.entry.setting;

		/* return if action is only active in network, or only settable by server */
		if (!(sd->save.conv & SLF_NETWORK_NO) && _networking && !_network_server) return;
		if ((sd->desc.flags & SGF_NETWORK_ONLY) && !_networking) return;
		if ((sd->desc.flags & SGF_NO_NETWORK) && _networking) return;

		void *var = GetVariableAddress(settings_ptr, &sd->save);
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
					 * of the current setting. */
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
						this->clicked_entry->SetButtons((x >= 10) ? SEF_RIGHT_DEPRESSED : SEF_LEFT_DEPRESSED);
						this->flags4 |= WF_TIMEOUT_BEGIN;
						_left_button_clicked = false;
					}
				} break;

				default: NOT_REACHED();
			}

			if (value != oldvalue) {
				SetSettingValue(pe->d.entry.index, value);
				this->SetDirty();
			}
		} else {
			/* only open editbox for types that its sensible for */
			if (sd->desc.cmd != SDT_BOOLX && !(sd->desc.flags & SGF_MULTISTRING)) {
				/* Show the correct currency-translated value */
				if (sd->desc.flags & SGF_CURRENCY) value *= _currency->rate;

				this->valuewindow_entry = pe;
				SetDParam(0, value);
				ShowQueryString(STR_CONFIG_SETTING_INT32, STR_CONFIG_SETTING_QUERY_CAPT, 10, 100, this, CS_NUMERAL, QSF_NONE);
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
			assert((this->valuewindow_entry->flags & SEF_KIND_MASK) == SEF_SETTING_KIND);
			const SettingDesc *sd = this->valuewindow_entry->d.entry.setting;
			int32 value = atoi(str);

			/* Save the correct currency-translated value */
			if (sd->desc.flags & SGF_CURRENCY) value /= _currency->rate;

			SetSettingValue(this->valuewindow_entry->d.entry.index, value);
			this->SetDirty();
		}
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		this->vscroll.cap += delta.y / SETTING_HEIGHT;
		SetVScrollCount(this, _settings_main_page.Length());
	}
};

GameSettings *GameSettingsWindow::settings_ptr = NULL;
const int GameSettingsWindow::SETTINGTREE_LEFT_OFFSET = 5;
const int GameSettingsWindow::SETTINGTREE_TOP_OFFSET = 19;

static const Widget _settings_selection_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_MAUVE,     0,    10,     0,    13, STR_00C5,                        STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,  COLOUR_MAUVE,    11,   411,     0,    13, STR_CONFIG_SETTING_CAPTION,      STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,     RESIZE_RB,  COLOUR_MAUVE,     0,   399,    14,   187, 0x0,                             STR_NULL}, // SETTINGSEL_OPTIONSPANEL
{  WWT_SCROLLBAR,    RESIZE_LRB,  COLOUR_MAUVE,   400,   411,    14,   175, 0x0,                             STR_0190_SCROLL_BAR_SCROLLS_LIST}, // SETTINGSEL_SCROLLBAR
{  WWT_RESIZEBOX,   RESIZE_LRTB,  COLOUR_MAUVE,   400,   411,   176,   187, 0x0,                             STR_RESIZE_BUTTON}, // SETTINGSEL_RESIZE
{   WIDGETS_END},
};

static const WindowDesc _settings_selection_desc(
	WDP_CENTER, WDP_CENTER, 412, 188, 450, 397,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_RESIZABLE,
	_settings_selection_widgets
);

void ShowGameSettings()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	new GameSettingsWindow(&_settings_selection_desc);
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
					str = STR_CONFIG_SETTING_INT32;
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
					str = STR_CONFIG_SETTING_INT32;
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

			case CUSTCURR_SEPARATOR: // Thousands seperator
				_custom_currency.separator = StrEmpty(str) ? ' ' : str[0];
				strecpy(this->separator, str, lastof(this->separator));
				break;

			case CUSTCURR_PREFIX:
				strecpy(_custom_currency.prefix, str, lastof(_custom_currency.prefix));
				break;

			case CUSTCURR_SUFFIX:
				strecpy(_custom_currency.suffix, str, lastof(_custom_currency.suffix));
				break;

			case CUSTCURR_TO_EURO: { // Year to switch to euro
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

static const WindowDesc _cust_currency_desc(
	WDP_CENTER, WDP_CENTER, 230, 120, 230, 120,
	WC_CUSTOM_CURRENCY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_cust_currency_widgets
);

static void ShowCustCurrency()
{
	DeleteWindowById(WC_CUSTOM_CURRENCY, 0);
	new CustomCurrencyWindow(&_cust_currency_desc);
}
