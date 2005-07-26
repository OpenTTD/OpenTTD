/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "string.h"
#include "strings.h" // XXX GetCurrentCurrencyRate()
#include "table/sprites.h"
#include "table/strings.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "command.h"
#include "engine.h"
#include "screenshot.h"
#include "newgrf.h"
#include "network.h"
#include "console.h"
#include "town.h"
#include "variables.h"

static uint32 _difficulty_click_a;
static uint32 _difficulty_click_b;
static byte _difficulty_timeout;

extern const StringID _currency_string_list[];
extern uint GetMaskOfAllowedCurrencies(void);

static const StringID _distances_dropdown[] = {
	STR_0139_IMPERIAL_MILES,
	STR_013A_METRIC_KILOMETERS,
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
	while (--num>=0) *p++ = base++;
	*p = INVALID_STRING_ID;
	return buf;
}

static int GetCurRes(void)
{
	int i;
	for(i = 0; i != _num_resolutions; i++)
		if (_resolutions[i][0] == _screen.width &&
				_resolutions[i][1] == _screen.height)
			break;
	return i;
}

static inline bool RoadVehiclesAreBuilt(void)
{
	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Road) return true;
	}
	return false;
}

static void GameOptionsWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		int i;
		StringID str = STR_02BE_DEFAULT;
		w->disabled_state = (_vehicle_design_names & 1) ? (++str, 0) : (1 << 21);
		SetDParam(0, str);
		SetDParam(1, _currency_string_list[_opt_ptr->currency]);
		SetDParam(2, _opt_ptr->kilometers + STR_0139_IMPERIAL_MILES);
		SetDParam(3, STR_02E9_DRIVE_ON_LEFT + _opt_ptr->road_side);
		SetDParam(4, STR_TOWNNAME_ORIGINAL_ENGLISH + _opt_ptr->town_name);
		SetDParam(5, _autosave_dropdown[_opt_ptr->autosave]);
		SetDParam(6, SPECSTR_LANGUAGE_START + _dynlang.curr);
		i = GetCurRes();
		SetDParam(7, i == _num_resolutions ? STR_RES_OTHER : SPECSTR_RESOLUTION_START + i);
		SetDParam(8, SPECSTR_SCREENSHOT_START + _cur_screenshot_format);
		(_fullscreen) ? SETBIT(w->click_state, 28) : CLRBIT(w->click_state, 28); // fullscreen button

		DrawWindowWidgets(w);
		DrawString(20, 175, STR_OPTIONS_FULLSCREEN, 0); // fullscreen
	}	break;

	case WE_CLICK:
		switch (e->click.widget) {
		case 4: case 5: /* Setup currencies dropdown */
			ShowDropDownMenu(w, _currency_string_list, _opt_ptr->currency, 5, _game_mode == GM_MENU ? 0 : ~GetMaskOfAllowedCurrencies(), 0);
			return;
		case 7: case 8: /* Setup distance unit dropdown */
			ShowDropDownMenu(w, _distances_dropdown, _opt_ptr->kilometers, 8, 0, 0);
			return;
		case 10: case 11: { /* Setup road-side dropdown */
			int i = 0;

			/* You can only change the drive side if you are in the menu or ingame with
			 * no vehicles present. In a networking game only the server can change it */
			if ((_game_mode != GM_MENU && RoadVehiclesAreBuilt()) || (_networking && !_network_server))
				i = (-1) ^ (1 << _opt_ptr->road_side); // disable the other value

			ShowDropDownMenu(w, _driveside_dropdown, _opt_ptr->road_side, 11, i, 0);
		} return;
		case 13: case 14: { /* Setup townname dropdown */
			int i = _opt_ptr->town_name;
			ShowDropDownMenu(w, BuildDynamicDropdown(STR_TOWNNAME_ORIGINAL_ENGLISH, SPECSTR_TOWNNAME_LAST - SPECSTR_TOWNNAME_START + 1), i, 14, (_game_mode == GM_MENU) ? 0 : (-1) ^ (1 << i), 0);
			return;
		}
		case 16: case 17: /* Setup autosave dropdown */
			ShowDropDownMenu(w, _autosave_dropdown, _opt_ptr->autosave, 17, 0, 0);
			return;
		case 19: case 20: /* Setup customized vehicle-names dropdown */
			ShowDropDownMenu(w, _designnames_dropdown, (_vehicle_design_names & 1) ? 1 : 0, 20, (_vehicle_design_names & 2) ? 0 : 2, 0);
			return;
		case 21: /* Save customized vehicle-names to disk */
			return;
		case 23: case 24: /* Setup interface language dropdown */
			ShowDropDownMenu(w, _dynlang.dropdown, _dynlang.curr, 24, 0, 0);
			return;
		case 26: case 27: /* Setup resolution dropdown */
			ShowDropDownMenu(w, BuildDynamicDropdown(SPECSTR_RESOLUTION_START, _num_resolutions), GetCurRes(), 27, 0, 0);
			return;
		case 28: /* Click fullscreen on/off */
			(_fullscreen) ? CLRBIT(w->click_state, 28) : SETBIT(w->click_state, 28);
			ToggleFullScreen(!_fullscreen); // toggle full-screen on/off
			SetWindowDirty(w);
			return;
		case 30: case 31: /* Setup screenshot format dropdown */
			ShowDropDownMenu(w, BuildDynamicDropdown(SPECSTR_SCREENSHOT_START, _num_screenshot_formats), _cur_screenshot_format, 31, 0, 0);
			return;
		}
		break;

	case WE_DROPDOWN_SELECT:
		switch (e->dropdown.button) {
		case 20: /* Vehicle design names */
			if (e->dropdown.index == 0) {
				DeleteCustomEngineNames();
				MarkWholeScreenDirty();
			} else if (!(_vehicle_design_names & 1)) {
				LoadCustomEngineNames();
				MarkWholeScreenDirty();
			}
			break;
		case 5: /* Currency */
			if (e->dropdown.index == 23)
				ShowCustCurrency();
			_opt_ptr->currency = e->dropdown.index;
			MarkWholeScreenDirty();
			break;
		case 8: /* Distance units */
			_opt_ptr->kilometers = e->dropdown.index;
			MarkWholeScreenDirty();
			break;
		case 11: /* Road side */
			if (_opt_ptr->road_side != e->dropdown.index) { // only change if setting changed
				DoCommandP(0, e->dropdown.index, 0, NULL, CMD_SET_ROAD_DRIVE_SIDE | CMD_MSG(STR_00B4_CAN_T_DO_THIS));
				MarkWholeScreenDirty();
			}
			break;
		case 14: /* Town names */
			if (_game_mode == GM_MENU) {
				_opt_ptr->town_name = e->dropdown.index;
				InvalidateWindow(WC_GAME_OPTIONS, 0);
			}
			break;
		case 17: /* Autosave options */
			_opt_ptr->autosave = e->dropdown.index;
			SetWindowDirty(w);
			break;
		case 24: /* Change interface language */
			ReadLanguagePack(e->dropdown.index);
			MarkWholeScreenDirty();
			break;
		case 27: /* Change resolution */
			if (e->dropdown.index < _num_resolutions && ChangeResInGame(_resolutions[e->dropdown.index][0],_resolutions[e->dropdown.index][1]))
				SetWindowDirty(w);
			break;
		case 31: /* Change screenshot format */
			SetScreenshotFormat(e->dropdown.index);
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
 * @param x,y unused
 * @param p1 the side of the road; 0 = left side and 1 = right side
 * @param p2 unused
 */
int32 CmdSetRoadDriveSide(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	/* Check boundaries and you can only change this if NO vehicles have been built yet,
	 * except in the intro-menu where of course it's always possible to do so. */
	if (p1 > 1 || (_game_mode != GM_MENU && RoadVehiclesAreBuilt())) return CMD_ERROR;

	if (flags & DC_EXEC) {
		_opt_ptr->road_side = p1;
		InvalidateWindow(WC_GAME_OPTIONS,0);
	}
	return 0;
}

static const Widget _game_options_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,								STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   369,     0,    13, STR_00B1_GAME_OPTIONS,		STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   369,    14,   238, 0x0,											STR_NULL},
{      WWT_FRAME,   RESIZE_NONE,    14,    10,   179,    20,    55, STR_02E0_CURRENCY_UNITS,	STR_NULL},
{          WWT_6,   RESIZE_NONE,    14,    20,   169,    34,    45, STR_02E1,								STR_02E2_CURRENCY_UNITS_SELECTION},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   158,   168,    35,    44, STR_0225,								STR_02E2_CURRENCY_UNITS_SELECTION},
{      WWT_FRAME,   RESIZE_NONE,    14,   190,   359,    20,    55, STR_02E3_DISTANCE_UNITS,	STR_NULL},
{          WWT_6,   RESIZE_NONE,    14,   200,   349,    34,    45, STR_02E4,								STR_02E5_DISTANCE_UNITS_SELECTION},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   338,   348,    35,    44, STR_0225,								STR_02E5_DISTANCE_UNITS_SELECTION},
{      WWT_FRAME,   RESIZE_NONE,    14,    10,   179,    62,    97, STR_02E6_ROAD_VEHICLES,	STR_NULL},
{          WWT_6,   RESIZE_NONE,    14,    20,   169,    76,    87, STR_02E7,								STR_02E8_SELECT_SIDE_OF_ROAD_FOR},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   158,   168,    77,    86, STR_0225,								STR_02E8_SELECT_SIDE_OF_ROAD_FOR},
{      WWT_FRAME,   RESIZE_NONE,    14,   190,   359,    62,    97, STR_02EB_TOWN_NAMES,			STR_NULL},
{          WWT_6,   RESIZE_NONE,    14,   200,   349,    76,    87, STR_02EC,								STR_02ED_SELECT_STYLE_OF_TOWN_NAMES},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   338,   348,    77,    86, STR_0225,								STR_02ED_SELECT_STYLE_OF_TOWN_NAMES},
{      WWT_FRAME,   RESIZE_NONE,    14,    10,   179,   104,   139, STR_02F4_AUTOSAVE,				STR_NULL},
{          WWT_6,   RESIZE_NONE,    14,    20,   169,   118,   129, STR_02F5,								STR_02F6_SELECT_INTERVAL_BETWEEN},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   158,   168,   119,   128, STR_0225,								STR_02F6_SELECT_INTERVAL_BETWEEN},

{      WWT_FRAME,   RESIZE_NONE,    14,    10,   359,   194,   228, STR_02BC_VEHICLE_DESIGN_NAMES,				STR_NULL},
{          WWT_6,   RESIZE_NONE,    14,    20,   119,   207,   218, STR_02BD,								STR_02C1_VEHICLE_DESIGN_NAMES_SELECTION},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   108,   118,   208,   217, STR_0225,								STR_02C1_VEHICLE_DESIGN_NAMES_SELECTION},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   130,   349,   207,   218, STR_02C0_SAVE_CUSTOM_NAMES,	STR_02C2_SAVE_CUSTOMIZED_VEHICLE},

{      WWT_FRAME,   RESIZE_NONE,    14,   190,   359,   104,   139, STR_OPTIONS_LANG,				STR_NULL},
{          WWT_6,   RESIZE_NONE,    14,   200,   349,   118,   129, STR_OPTIONS_LANG_CBO,		STR_OPTIONS_LANG_TIP},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   338,   348,   119,   128, STR_0225,								STR_OPTIONS_LANG_TIP},

{      WWT_FRAME,   RESIZE_NONE,    14,    10,   179,   146,   190, STR_OPTIONS_RES,					STR_NULL},
{          WWT_6,   RESIZE_NONE,    14,    20,   169,   160,   171, STR_OPTIONS_RES_CBO,			STR_OPTIONS_RES_TIP},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   158,   168,   161,   170, STR_0225,								STR_OPTIONS_RES_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   149,   169,   176,   184, STR_EMPTY,								STR_OPTIONS_FULLSCREEN_TIP},

{      WWT_FRAME,   RESIZE_NONE,    14,   190,   359,   146,   190, STR_OPTIONS_SCREENSHOT_FORMAT,				STR_NULL},
{          WWT_6,   RESIZE_NONE,    14,   200,   349,   160,   171, STR_OPTIONS_SCREENSHOT_FORMAT_CBO,		STR_OPTIONS_SCREENSHOT_FORMAT_TIP},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   338,   348,   161,   170, STR_0225,								STR_OPTIONS_SCREENSHOT_FORMAT_TIP},

{   WIDGETS_END},
};

static const WindowDesc _game_options_desc = {
	WDP_CENTER, WDP_CENTER, 370, 239,
	WC_GAME_OPTIONS,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_game_options_widgets,
	GameOptionsWndProc
};


void ShowGameOptions(void)
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	AllocateWindowDesc(&_game_options_desc);
}

typedef struct {
	int16 min;
	int16 max;
	int16 step;
	StringID str;
} GameSettingData;

static const GameSettingData _game_setting_info[] = {
	{  0,   7,  1, STR_NULL},
	{  0,   3,  1, STR_6830_IMMEDIATE},
	{  0,   2,  1, STR_6816_LOW},
	{  0,   3,  1, STR_26816_NONE},
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
	{  0,   2,  1, STR_6839_PERMISSIVE},
};

static inline bool GetBitAndShift(uint32 *b)
{
	uint32 x = *b;
	*b >>= 1;
	return HASBIT(x, 0);
}

/*
	A: competitors
	B: start time in months / 3
	C: town count (2 = high, 0 = low)
	D: industry count (3 = high, 0 = none)
	E: inital loan / 1000 (in GBP)
	F: interest rate
	G: running costs (0 = low, 2 = high)
	H: construction speed of competitors (0 = very slow, 4 = very fast)
	I: intelligence (0-2)
	J: breakdowns(0 = off, 2 = normal)
	K: subsidy multiplier (0 = 1.5, 3 = 4.0)
	L: construction cost (0-2)
	M: terrain type (0 = very flat, 3 = mountainous)
	N: amount of water (0 = very low, 3 = high)
	O: economy (0 = steady, 1 = fluctuating)
	P: Train reversing (0 = end of line + stations, 1 = end of line)
	Q: disasters
	R: area restructuring (0 = permissive, 2 = hostile)
*/
static const int16 _default_game_diff[3][GAME_DIFFICULTY_NUM] = { /*
	 A, B, C, D,   E, F, G, H, I, J, K, L, M, N, O, P, Q, R*/
	{2, 2, 1, 3, 300, 2, 0, 2, 0, 1, 2, 0, 1, 0, 0, 0, 0, 0},	//easy
	{4, 1, 1, 2, 150, 3, 1, 3, 1, 2, 1, 1, 2, 1, 1, 1, 1, 1},	//medium
	{7, 0, 2, 2, 100, 4, 1, 3, 2, 2, 0, 2, 3, 2, 1, 1, 1, 2},	//hard
};

void SetDifficultyLevel(int mode, GameOptions *gm_opt)
{
	int i;
	assert(mode <= 3);

	gm_opt->diff_level = mode;
	if (mode != 3) { // not custom
		for (i = 0; i != GAME_DIFFICULTY_NUM; i++)
			((int*)&gm_opt->diff)[i] = _default_game_diff[mode][i];
	}
}

extern void StartupEconomy(void);

enum {
	GAMEDIFF_WND_TOP_OFFSET = 45,
	GAMEDIFF_WND_ROWSIZE    = 9
};

// Temporary holding place of values in the difficulty window until 'Save' is clicked
static GameOptions _opt_mod_temp;
// 0x383E = (1 << 13) | (1 << 12) | (1 << 11) | (1 << 5) | (1 << 4) | (1 << 3) | (1 << 2) | (1 << 1)
#define DIFF_INGAME_DISABLED_BUTTONS 0x383E

static void GameDifficultyWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_CREATE: /* Setup disabled buttons when creating window */
		// disable all other difficulty buttons during gameplay except for 'custom'
		w->disabled_state = (_game_mode != GM_NORMAL) ? 0 : (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6);

		if (_game_mode == GM_EDITOR) SETBIT(w->disabled_state, 7);

		if (_networking) {
			SETBIT(w->disabled_state, 7); // disable highscore chart in multiplayer
			if (!_network_server)
				SETBIT(w->disabled_state, 10); // Disable save-button in multiplayer (and if client)
		}
		break;
	case WE_PAINT: {
		uint32 click_a, click_b, disabled;
		int i;
		int y, value;

		w->click_state = (1 << 3) << _opt_mod_temp.diff_level; // have current difficulty button clicked
		DrawWindowWidgets(w);

		click_a = _difficulty_click_a;
		click_b = _difficulty_click_b;

		/* XXX - Disabled buttons in normal gameplay. Bitshifted for each button to see if
		 * that bit is set. If it is set, the button is disabled */
		disabled = (_game_mode == GM_NORMAL) ? DIFF_INGAME_DISABLED_BUTTONS : 0;

		y = GAMEDIFF_WND_TOP_OFFSET;
		for (i = 0; i != GAME_DIFFICULTY_NUM; i++) {
			DrawFrameRect( 5, y,  5 + 8, y + 8, 3, GetBitAndShift(&click_a) ? (1 << 5) : 0);
			DrawFrameRect(15, y, 15 + 8, y + 8, 3, GetBitAndShift(&click_b) ? (1 << 5) : 0);
			if (GetBitAndShift(&disabled) || (_networking && !_network_server)) {
				int color = PALETTE_MODIFIER_COLOR | _color_list[3].unk2;
				GfxFillRect( 6, y + 1,  6 + 8, y + 8, color);
				GfxFillRect(16, y + 1, 16 + 8, y + 8, color);
			}

			DrawStringCentered(10, y, STR_6819, 0);
			DrawStringCentered(20, y, STR_681A, 0);


			value = _game_setting_info[i].str + ((int*)&_opt_mod_temp.diff)[i];
			if (i == 4) value *= 1000; // XXX - handle currency option
			SetDParam(0, value);
			DrawString(30, y, STR_6805_MAXIMUM_NO_COMPETITORS + i, 0);

			y += GAMEDIFF_WND_ROWSIZE + 2; // space items apart a bit
		}
	} break;

	case WE_CLICK:
		switch (e->click.widget) {
		case 8: { /* Difficulty settings widget, decode click */
			const GameSettingData *info;
			int x, y;
			uint btn, dis;
			int val;

			// Don't allow clients to make any changes
			if  (_networking && !_network_server)
				return;

			x = e->click.pt.x - 5;
			if (!IS_INT_INSIDE(x, 0, 21)) // Button area
				return;

			y = e->click.pt.y - GAMEDIFF_WND_TOP_OFFSET;
			if (y < 0)
				return;

			// Get button from Y coord.
			btn = y / (GAMEDIFF_WND_ROWSIZE + 2);
			if (btn >= GAME_DIFFICULTY_NUM || y % (GAMEDIFF_WND_ROWSIZE + 2) >= 9)
				return;

			// Clicked disabled button?
			dis = (_game_mode == GM_NORMAL) ? DIFF_INGAME_DISABLED_BUTTONS : 0;

			if (HASBIT(dis, btn))
				return;

			_difficulty_timeout = 5;

			val = ((int*)&_opt_mod_temp.diff)[btn];

			info = &_game_setting_info[btn]; // get information about the difficulty setting
			if (x >= 10) {
				// Increase button clicked
				val = min(val + info->step, info->max);
				SETBIT(_difficulty_click_b, btn);
			} else {
				// Decrease button clicked
				val = max(val - info->step, info->min);
				SETBIT(_difficulty_click_a, btn);
			}

			// save value in temporary variable
			((int*)&_opt_mod_temp.diff)[btn] = val;
			SetDifficultyLevel(3, &_opt_mod_temp); // set difficulty level to custom
			SetWindowDirty(w);
		}	break;
		case 3: case 4: case 5: case 6: /* Easy / Medium / Hard / Custom */
			// temporarily change difficulty level
			SetDifficultyLevel(e->click.widget - 3, &_opt_mod_temp);
			SetWindowDirty(w);
			break;
		case 7: /* Highscore Table */
			ShowHighscoreTable(_opt_mod_temp.diff_level, -1);
			break;
		case 10: { /* Save button - save changes */
			int btn, val;
			for (btn = 0; btn != GAME_DIFFICULTY_NUM; btn++) {
				val = ((int*)&_opt_mod_temp.diff)[btn];
				// if setting has changed, change it
				if (val != ((int*)&_opt_ptr->diff)[btn])
					DoCommandP(0, btn, val, NULL, CMD_CHANGE_DIFFICULTY_LEVEL);
			}
			DoCommandP(0, -1, _opt_mod_temp.diff_level, NULL, CMD_CHANGE_DIFFICULTY_LEVEL);
			DeleteWindow(w);
			// If we are in the editor, we should reload the economy.
			//  This way when you load a game, the max loan and interest rate
			//  are loaded correctly.
			if (_game_mode == GM_EDITOR)
				StartupEconomy();
			break;
		}
		case 11: /* Cancel button - close window, abandon changes */
			DeleteWindow(w);
			break;
	} break;

	case WE_MOUSELOOP: /* Handle the visual 'clicking' of the buttons */
		if (_difficulty_timeout != 0 && !--_difficulty_timeout) {
			_difficulty_click_a = 0;
			_difficulty_click_b = 0;
			SetWindowDirty(w);
		}
		break;
	}
}

#undef DIFF_INGAME_DISABLED_BUTTONS

static const Widget _game_difficulty_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    10,     0,    10,     0,    13, STR_00C5,									STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    10,    11,   369,     0,    13, STR_6800_DIFFICULTY_LEVEL,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    10,     0,   369,    14,    29, 0x0,												STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,    10,    96,    16,    27, STR_6801_EASY,							STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,    97,   183,    16,    27, STR_6802_MEDIUM,						STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,   184,   270,    16,    27, STR_6803_HARD,							STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,   271,   357,    16,    27, STR_6804_CUSTOM,						STR_NULL},
{   WWT_CLOSEBOX,   RESIZE_NONE,    10,     0,   369,    30,    41, STR_6838_SHOW_HI_SCORE_CHART,STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    10,     0,   369,    42,   262, 0x0,												STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    10,     0,   369,   263,   278, 0x0,												STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,   105,   185,   265,   276, STR_OPTIONS_SAVE_CHANGES,	STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,   186,   266,   265,   276, STR_012E_CANCEL,						STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _game_difficulty_desc = {
	WDP_CENTER, WDP_CENTER, 370, 279,
	WC_GAME_OPTIONS,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_game_difficulty_widgets,
	GameDifficultyWndProc
};

void ShowGameDifficulty(void)
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	/* Copy current settings (ingame or in intro) to temporary holding place
	 * change that when setting stuff, copy back on clicking 'OK' */
	memcpy(&_opt_mod_temp, _opt_ptr, sizeof(GameOptions));
	AllocateWindowDesc(&_game_difficulty_desc);
}

// virtual PositionMainToolbar function, calls the right one.
static int32 v_PositionMainToolbar(int32 p1)
{
	if (_game_mode != GM_MENU)
		PositionMainToolbar(NULL);

	return 0;
}

static int32 AiNew_PatchActive_Warning(int32 p1)
{
  if (p1 == 1)
    ShowErrorMessage(-1, TEMP_AI_ACTIVATED, 0, 0);

  return 0;
}

static int32 PopulationInLabelActive(int32 p1)
{
	Town *t;

	FOR_ALL_TOWNS(t) {
		if (t->xy) {
			UpdateTownVirtCoord(t);
		}
	}

	return 0;
}

static int32 InvisibleTreesActive(int32 p1)
{
	MarkWholeScreenDirty();
	return 0;
}

static int32 InValidateDetailsWindow(int32 p1)
{
	InvalidateWindowClasses(WC_VEHICLE_DETAILS);
	return 0;
}

static int32 InvalidateStationBuildWindow(int32 p1)
{
	InvalidateWindow(WC_BUILD_STATION, 0);
	return 0;
}

/* Check service intervals of vehicles, p1 is value of % or day based servicing */
static int32 CheckInterval(int32 p1)
{
	bool warning;
	if (p1) {
		warning = ( (IS_INT_INSIDE(_patches.servint_trains,   5, 90+1) || _patches.servint_trains   == 0) &&
								(IS_INT_INSIDE(_patches.servint_roadveh,  5, 90+1) || _patches.servint_roadveh  == 0) &&
								(IS_INT_INSIDE(_patches.servint_aircraft, 5, 90+1) || _patches.servint_aircraft == 0) &&
								(IS_INT_INSIDE(_patches.servint_ships,    5, 90+1) || _patches.servint_ships    == 0) );
	} else {
		warning = ( (IS_INT_INSIDE(_patches.servint_trains,   30, 800+1) || _patches.servint_trains   == 0) &&
								(IS_INT_INSIDE(_patches.servint_roadveh,  30, 800+1) || _patches.servint_roadveh  == 0) &&
								(IS_INT_INSIDE(_patches.servint_aircraft, 30, 800+1) || _patches.servint_aircraft == 0) &&
								(IS_INT_INSIDE(_patches.servint_ships,    30, 800+1) || _patches.servint_ships    == 0) );
	}

	if (!warning)
		ShowErrorMessage(-1, STR_CONFIG_PATCHES_SERVICE_INTERVAL_INCOMPATIBLE, 0, 0);

	return InValidateDetailsWindow(0);
}

typedef int32 PatchButtonClick(int32);

typedef struct PatchEntry {
  byte type;                    // type of selector
  byte flags;                   // selector flags
  StringID str;                 // string with descriptive text
  char console_name[40];        // the name this patch has in console
  void *variable;               // pointer to the variable
  int32 min, max;               // range for spinbox setting
  uint32 step;                  // step for spinbox
  PatchButtonClick *click_proc; // callback procedure
} PatchEntry;

enum {
	PE_BOOL			= 0,
	PE_UINT8		= 1,
	PE_INT16		= 2,
	PE_UINT16		= 3,
	PE_INT32		= 4,
	PE_CURRENCY	= 5,
	// selector flags
	PF_0ISDIS				= 1 << 0,
	PF_NOCOMMA			= 1 << 1,
	PF_MULTISTRING	= 1 << 2,
	PF_PLAYERBASED	= 1 << 3, // This has to match the entries that are in settings.c, patch_player_settings
	PF_NETWORK_ONLY = 1 << 4, // this setting only applies to network games
};

static const PatchEntry _patches_ui[] = {
	{PE_BOOL,		PF_PLAYERBASED, STR_CONFIG_PATCHES_VEHICLESPEED,		"vehicle_speed",		&_patches.vehicle_speed,						0,  0,  0, NULL},
	{PE_BOOL,		PF_PLAYERBASED, STR_CONFIG_PATCHES_LONGDATE,				"long_date",				&_patches.status_long_date,					0,  0,  0, NULL},
	{PE_BOOL,		PF_PLAYERBASED, STR_CONFIG_PATCHES_SHOWFINANCES,		"show_finances",		&_patches.show_finances,						0,  0,  0, NULL},
	{PE_BOOL,		PF_PLAYERBASED, STR_CONFIG_PATCHES_AUTOSCROLL,			"autoscroll",				&_patches.autoscroll,								0,  0,  0, NULL},

	{PE_UINT8,	PF_PLAYERBASED, STR_CONFIG_PATCHES_ERRMSG_DURATION,	"errmsg_duration",	&_patches.errmsg_duration,					0, 20,  1, NULL},

	{PE_UINT8,	PF_MULTISTRING | PF_PLAYERBASED, STR_CONFIG_PATCHES_TOOLBAR_POS, "toolbar_pos", &_patches.toolbar_pos,			0,  2,  1, &v_PositionMainToolbar},
	{PE_UINT8,	PF_0ISDIS | PF_PLAYERBASED, STR_CONFIG_PATCHES_SNAP_RADIUS, "window_snap_radius", &_patches.window_snap_radius,     1, 32,  1, NULL},
	{PE_BOOL,		PF_PLAYERBASED, STR_CONFIG_PATCHES_INVISIBLE_TREES,	"invisible_trees", &_patches.invisible_trees,					0,  1,  1, &InvisibleTreesActive},
	{PE_BOOL,		PF_PLAYERBASED, STR_CONFIG_PATCHES_POPULATION_IN_LABEL, "population_in_label", &_patches.population_in_label, 0, 1, 1, &PopulationInLabelActive},

	{PE_INT32, 0, STR_CONFIG_PATCHES_MAP_X, "map_x", &_patches.map_x, 6, 11, 1, NULL},
	{PE_INT32, 0, STR_CONFIG_PATCHES_MAP_Y, "map_y", &_patches.map_y, 6, 11, 1, NULL},
};

static const PatchEntry _patches_construction[] = {
	{PE_BOOL,		0, STR_CONFIG_PATCHES_BUILDONSLOPES,		"build_on_slopes",	&_patches.build_on_slopes,					0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_EXTRADYNAMITE,		"extra_dynamite",		&_patches.extra_dynamite,						0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_LONGBRIDGES,			"long_bridges",			&_patches.longbridges,							0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_SIGNALSIDE,				"signal_side",			&_patches.signal_side,							0,  0,  0, NULL},

	{PE_BOOL,		0, STR_CONFIG_PATCHES_SMALL_AIRPORTS,		"always_small_airport", &_patches.always_small_airport,			0,  0,  0, NULL},
	{PE_UINT8,	PF_PLAYERBASED, STR_CONFIG_PATCHES_DRAG_SIGNALS_DENSITY, "drag_signals_density", &_patches.drag_signals_density, 1, 20,  1, NULL},
	{PE_BOOL,		0, STR_CONFIG_AUTO_PBS_PLACEMENT, "auto_pbs_placement", &_patches.auto_pbs_placement, 1, 20,  1, NULL},

};

static const PatchEntry _patches_vehicles[] = {
	{PE_BOOL,		0, STR_CONFIG_PATCHES_REALISTICACCEL,		"realistic_acceleration", &_patches.realistic_acceleration,		0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_FORBID_90_DEG,		"forbid_90_deg", 		&_patches.forbid_90_deg,						0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_MAMMOTHTRAINS,		"mammoth_trains", 	&_patches.mammoth_trains,						0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_GOTODEPOT,				"goto_depot", 			&_patches.gotodepot,								0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_ROADVEH_QUEUE,		"roadveh_queue", 		&_patches.roadveh_queue,						0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_NEW_PATHFINDING_ALL, "new_pathfinding_all", &_patches.new_pathfinding_all,		0,  0,  0, NULL},

	{PE_BOOL,		PF_PLAYERBASED, STR_CONFIG_PATCHES_WARN_INCOME_LESS, "train_income_warn", &_patches.train_income_warn,				0,  0,  0, NULL},
	{PE_UINT8,	PF_MULTISTRING | PF_PLAYERBASED, STR_CONFIG_PATCHES_ORDER_REVIEW, "order_review_system", &_patches.order_review_system,0,2,  1, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_NEVER_EXPIRE_VEHICLES, "never_expire_vehicles", &_patches.never_expire_vehicles,0,0,0, NULL},

	{PE_UINT16, PF_0ISDIS | PF_PLAYERBASED, STR_CONFIG_PATCHES_LOST_TRAIN_DAYS, "lost_train_days", &_patches.lost_train_days,	180,720, 60, NULL},
	{PE_BOOL,		PF_PLAYERBASED, STR_CONFIG_PATCHES_AUTORENEW_VEHICLE,"autorenew", &_patches.autorenew,								0,  0,  0, NULL},
	{PE_INT16,	PF_PLAYERBASED, STR_CONFIG_PATCHES_AUTORENEW_MONTHS, "autorenew_months", &_patches.autorenew_months,				-12, 12,  1, NULL},
	{PE_CURRENCY, PF_PLAYERBASED, STR_CONFIG_PATCHES_AUTORENEW_MONEY,"autorenew_money", &_patches.autorenew_money,					0, 2000000, 100000, NULL},

	{PE_UINT16,	0, STR_CONFIG_PATCHES_MAX_TRAINS,				"max_trains", &_patches.max_trains,								0,5000, 50, NULL},
	{PE_UINT16,	0, STR_CONFIG_PATCHES_MAX_ROADVEH,			"max_roadveh", &_patches.max_roadveh,							0,5000, 50, NULL},
	{PE_UINT16,	0, STR_CONFIG_PATCHES_MAX_AIRCRAFT,			"max_aircraft", &_patches.max_aircraft,						0,5000, 50, NULL},
	{PE_UINT16,	0, STR_CONFIG_PATCHES_MAX_SHIPS,				"max_ships", &_patches.max_ships,									0,5000, 50, NULL},

	{PE_BOOL,		0, STR_CONFIG_PATCHES_SERVINT_ISPERCENT,"servint_isperfect",&_patches.servint_ispercent,				0,  0,  0, &CheckInterval},
	{PE_UINT16, PF_0ISDIS, STR_CONFIG_PATCHES_SERVINT_TRAINS,		"servint_trains",   &_patches.servint_trains,		5,800,  5, &InValidateDetailsWindow},
	{PE_UINT16, PF_0ISDIS, STR_CONFIG_PATCHES_SERVINT_ROADVEH,	"servint_roadveh",  &_patches.servint_roadveh,	5,800,  5, &InValidateDetailsWindow},
	{PE_UINT16, PF_0ISDIS, STR_CONFIG_PATCHES_SERVINT_AIRCRAFT, "servint_aircraft", &_patches.servint_aircraft, 5,800,  5, &InValidateDetailsWindow},
	{PE_UINT16, PF_0ISDIS, STR_CONFIG_PATCHES_SERVINT_SHIPS,		"servint_ships",    &_patches.servint_ships,		5,800,  5, &InValidateDetailsWindow},
	{PE_BOOL,   0,         STR_CONFIG_PATCHES_NOSERVICE,        "no_servicing_if_no_breakdowns", &_patches.no_servicing_if_no_breakdowns, 0, 0, 0, NULL},
};

static const PatchEntry _patches_stations[] = {
	{PE_BOOL,		0, STR_CONFIG_PATCHES_JOINSTATIONS,			"join_stations", &_patches.join_stations,						0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_FULLLOADANY,			"full_load_any", &_patches.full_load_any,						0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_IMPROVEDLOAD,			"improved_load", &_patches.improved_load,						0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_SELECTGOODS,			"select_goods",  &_patches.selectgoods,							0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_NEW_NONSTOP,			"new_nonstop", &_patches.new_nonstop,							0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_NONUNIFORM_STATIONS, "nonuniform_stations", &_patches.nonuniform_stations,		0,  0,  0, NULL},
	{PE_UINT8,	0, STR_CONFIG_PATCHES_STATION_SPREAD,		"station_spread", &_patches.station_spread,						4, 64,  1, &InvalidateStationBuildWindow},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_SERVICEATHELIPAD, "service_at_helipad", &_patches.serviceathelipad,					0,  0,  0, NULL},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_CATCHMENT, "modified_catchment", &_patches.modified_catchment, 0, 0, 0, NULL},

};

static const PatchEntry _patches_economy[] = {
	{PE_BOOL,		0, STR_CONFIG_PATCHES_INFLATION,				"inflation", &_patches.inflation,								0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_BUILDXTRAIND,			"build_rawmaterial", &_patches.build_rawmaterial_ind,		0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_MULTIPINDTOWN,		"multiple_industry_per_town", &_patches.multiple_industry_per_town,0, 0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_SAMEINDCLOSE,			"same_industry_close", &_patches.same_industry_close,			0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_BRIBE,						"bribe", &_patches.bribe,										0,  0,  0, NULL},
	{PE_UINT8,	0, STR_CONFIG_PATCHES_SNOWLINE_HEIGHT,	"snow_line_height", &_patches.snow_line_height,					2, 13,  1, NULL},

	{PE_INT32,	PF_NOCOMMA, STR_CONFIG_PATCHES_COLORED_NEWS_DATE, "colored_new_data", &_patches.colored_news_date, 1900, 2200, 5, NULL},
	{PE_INT32,	PF_NOCOMMA, STR_CONFIG_PATCHES_STARTING_DATE, "starting_date", &_patches.starting_date,	 MAX_YEAR_BEGIN_REAL, MAX_YEAR_END_REAL, 1, NULL},
	{PE_INT32,	PF_NOCOMMA | PF_NETWORK_ONLY, STR_CONFIG_PATCHES_ENDING_DATE, "ending_date", &_patches.ending_date,	 MAX_YEAR_BEGIN_REAL, MAX_YEAR_END_REAL, 1, NULL},

	{PE_BOOL,		0, STR_CONFIG_PATCHES_SMOOTH_ECONOMY,		"smooth_economy", &_patches.smooth_economy,						0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_ALLOW_SHARES,			"allow_shares", &_patches.allow_shares,						0,  0,  0, NULL},
};

static const PatchEntry _patches_ai[] = {
	{PE_BOOL,		0, STR_CONFIG_PATCHES_AINEW_ACTIVE, "ainew_active", &_patches.ainew_active, 0, 1, 1, &AiNew_PatchActive_Warning},

	{PE_BOOL,		0, STR_CONFIG_PATCHES_AI_BUILDS_TRAINS, "ai_disable_veh_train", &_patches.ai_disable_veh_train,			0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_AI_BUILDS_ROADVEH,"ai_disable_veh_roadveh",&_patches.ai_disable_veh_roadveh,		0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_AI_BUILDS_AIRCRAFT,"ai_disable_veh_aircraft",&_patches.ai_disable_veh_aircraft,0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_AI_BUILDS_SHIPS,"ai_disable_veh_ship",&_patches.ai_disable_veh_ship,			0,  0,  0, NULL},
};

typedef struct PatchPage {
	const PatchEntry *entries;
	uint num;
} PatchPage;

static const PatchPage _patches_page[] = {
	{_patches_ui,						lengthof(_patches_ui) },
	{_patches_construction, lengthof(_patches_construction) },
	{_patches_vehicles,			lengthof(_patches_vehicles) },
	{_patches_stations,			lengthof(_patches_stations) },
	{_patches_economy,			lengthof(_patches_economy) },
	{_patches_ai,						lengthof(_patches_ai) },
};


static int32 ReadPE(const PatchEntry*pe)
{
	switch (pe->type) {
	case PE_BOOL:   return *(bool*)pe->variable;
	case PE_UINT8:  return *(uint8*)pe->variable;
	case PE_INT16:  return *(int16*)pe->variable;
	case PE_UINT16: return *(uint16*)pe->variable;
	case PE_INT32:  return *(int32*)pe->variable;
	case PE_CURRENCY:  return (*(int32*)pe->variable) * GetCurrentCurrencyRate();
	default: NOT_REACHED();
	}

	/* useless, but avoids compiler warning this way */
	return 0;
}

static void WritePE(const PatchEntry *pe, int32 val)
{
	if ((pe->flags & PF_0ISDIS) && val <= 0) {
		switch (pe->type) {
			case PE_BOOL: case PE_UINT8:
				*(bool*)pe->variable = 0;
				break;
			case PE_INT16: case PE_UINT16:
				*(int16*)pe->variable = 0;
				break;
			case PE_CURRENCY: case PE_INT32:
				*(int32*)pe->variable = 0;
				break;
		}
		return;
	}

	// "clamp" 'disabled' value to smallest type
	switch (pe->type) {
		case PE_BOOL:
			*(bool*)pe->variable = (bool)val;
			break;
		case PE_UINT8:
			*(uint8*)pe->variable = (uint8)clamp((uint8)val, (uint8)pe->min, (uint8)pe->max);
			break;
		case PE_INT16:
			*(int16*)pe->variable = (int16)clamp((int16)val, (int16)pe->min, (int16)pe->max);
			break;
		case PE_UINT16:
			*(uint16*)pe->variable = (uint16)clamp((uint16)val, (uint16)pe->min, (uint16)pe->max);
			break;
		case PE_CURRENCY: case PE_INT32:
			*(int32*)pe->variable = (int32)clamp((int32)val, (int32)pe->min, (int32)pe->max);
			break;
		default: NOT_REACHED();
	}
}

static void PatchesSelectionWndProc(Window *w, WindowEvent *e)
{
	uint i;
	switch (e->event) {
	case WE_PAINT: {
		int x,y;
		const PatchEntry *pe;
		const PatchPage *page;
		uint clk;
		int32 val;

		w->click_state = 1 << (WP(w,def_d).data_1 + 4);

		DrawWindowWidgets(w);

		x = 0;
		y = 46;
		clk = WP(w,def_d).data_2;
		page = &_patches_page[WP(w,def_d).data_1];
		for (i = 0, pe = page->entries; i != page->num; i++, pe++) {
			bool disabled = false;
			bool editable = true;

			if ((pe->flags & PF_NETWORK_ONLY) && !_networking)
				editable = false;

			// We do not allow changes of some items when we are a client in a networkgame
			if (!(pe->flags & PF_PLAYERBASED) && _networking && !_network_server)
				editable = false;
			if (pe->type == PE_BOOL) {
				if (editable)
					DrawFrameRect(x+5, y+1, x+15+9, y+9, (*(bool*)pe->variable) ? 6 : 4, (*(bool*)pe->variable) ? FR_LOWERED : 0);
				else
					DrawFrameRect(x+5, y+1, x+15+9, y+9, (*(bool*)pe->variable) ? 7 : 9, (*(bool*)pe->variable) ? FR_LOWERED : 0);
				SetDParam(0, *(bool*)pe->variable ? STR_CONFIG_PATCHES_ON : STR_CONFIG_PATCHES_OFF);
			} else {
				DrawFrameRect(x+5, y+1, x+5+9, y+9, 3, clk == i*2+1 ? FR_LOWERED : 0);
				DrawFrameRect(x+15, y+1, x+15+9, y+9, 3, clk == i*2+2 ? FR_LOWERED : 0);
				if (!editable) {
					int color = 0x8000 | _color_list[3].unk2;
					GfxFillRect(x+6, y+2, x+6+8, y+9, color);
					GfxFillRect(x+16, y+2, x+16+8, y+9, color);
				}
				DrawStringCentered(x+10, y+1, STR_6819, 0);
				DrawStringCentered(x+20, y+1, STR_681A, 0);

				val = ReadPE(pe);
				if (pe->type == PE_CURRENCY)
					val /= GetCurrentCurrencyRate();
				disabled = ((val == 0) && (pe->flags & PF_0ISDIS));
				if (disabled) {
					SetDParam(0, STR_CONFIG_PATCHES_DISABLED);
				} else {
					SetDParam(1, val);
					if (pe->type == PE_CURRENCY)
						SetDParam(0, STR_CONFIG_PATCHES_CURRENCY);
					else {
						if (pe->flags & PF_MULTISTRING)
							SetDParam(0, pe->str + val + 1);
						else
							SetDParam(0, pe->flags & PF_NOCOMMA ? STR_CONFIG_PATCHES_INT32 : STR_7024);
					}
				}
			}
			DrawString(30, y+1, (pe->str)+disabled, 0);
			y += 11;
		}
		break;
	}

	case WE_CLICK:
		switch(e->click.widget) {
		case 3: {
			int x,y;
			uint btn;
			const PatchPage *page;
			const PatchEntry *pe;

			y = e->click.pt.y - 46 - 1;
			if (y < 0) return;

			btn = y / 11;
			if (y % 11 > 9) return;

			page = &_patches_page[WP(w,def_d).data_1];
			if (btn >= page->num) return;
			pe = &page->entries[btn];

			x = e->click.pt.x - 5;
			if (x < 0) return;

			if (((pe->flags & PF_NETWORK_ONLY) && !_networking) || // return if action is only active in network
					(!(pe->flags & PF_PLAYERBASED) && _networking && !_network_server)) // return if only server can change it
				return;

			if (x < 21) { // clicked on the icon on the left side. Either scroller or bool on/off
				int32 val = ReadPE(pe), oval = val;

				switch(pe->type) {
				case PE_BOOL:
					val ^= 1;
					break;
				case PE_UINT8:
				case PE_INT16:
				case PE_UINT16:
				case PE_INT32:
				case PE_CURRENCY:
					// don't allow too fast scrolling
					if ((w->flags4 & WF_TIMEOUT_MASK) > 2 << WF_TIMEOUT_SHL) {
						_left_button_clicked = false;
						return;
					}

					if (x >= 10) {
						//increase
						if (pe->flags & PF_0ISDIS && val == 0)
							val = pe->min;
						else
							val += pe->step;
						if (val > pe->max) val = pe->max;
					} else {
						// decrease
						if (val <= pe->min && pe->flags & PF_0ISDIS) {
							val = 0;
						} else {
							val -= pe->step;
							if (val < pe->min) val = pe->min;
						}
					}

					if (val != oval) {
						WP(w,def_d).data_2 = btn * 2 + 1 + ((x>=10) ? 1 : 0);
						w->flags4 |= 5 << WF_TIMEOUT_SHL;
						_left_button_clicked = false;
					}
					break;
				}
				if (val != oval) {
					// To make patch-changes network-safe
					if (pe->type == PE_CURRENCY) {
						val /= GetCurrentCurrencyRate();
					}
					// If an item is playerbased, we do not send it over the network (if any)
					if (pe->flags & PF_PLAYERBASED) {
						WritePE(pe, val);
					} else {
						// Else we do
						DoCommandP(0, (byte)WP(w,def_d).data_1 + ((byte)btn << 8), val, NULL, CMD_CHANGE_PATCH_SETTING);
					}
					SetWindowDirty(w);

					if (pe->click_proc != NULL) // call callback function
						pe->click_proc(val);
				}
			} else {
				if (pe->type != PE_BOOL && !(pe->flags & PF_MULTISTRING)) { // do not open editbox
					WP(w,def_d).data_3 = btn;
					SetDParam(0, ReadPE(pe));
					ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_CONFIG_PATCHES_QUERY_CAPT, 10, 100, WC_GAME_OPTIONS, 0);
				}
			}

			break;
		}
		case 4: case 5: case 6: case 7: case 8: case 9:
			WP(w,def_d).data_1 = e->click.widget - 4;
			DeleteWindowById(WC_QUERY_STRING, 0);
			SetWindowDirty(w);
			break;
		}
		break;

	case WE_TIMEOUT:
		WP(w,def_d).data_2 = 0;
		SetWindowDirty(w);
		break;

	case WE_ON_EDIT_TEXT: {
		if (*e->edittext.str) {
			const PatchPage *page = &_patches_page[WP(w,def_d).data_1];
			const PatchEntry *pe = &page->entries[WP(w,def_d).data_3];
			int32 val;
			val = atoi(e->edittext.str);
			if (pe->type == PE_CURRENCY) {
				val /= GetCurrentCurrencyRate();
			}
			// If an item is playerbased, we do not send it over the network (if any)
			if (pe->flags & PF_PLAYERBASED) {
				WritePE(pe, val);
			} else {
				// Else we do
				DoCommandP(0, (byte)WP(w,def_d).data_1 + ((byte)WP(w,def_d).data_3 << 8), val, NULL, CMD_CHANGE_PATCH_SETTING);
			}
			SetWindowDirty(w);

			if (pe->click_proc != NULL) // call callback function
				pe->click_proc(*(int32*)pe->variable);
		}
		break;
	}

	case WE_DESTROY:
		DeleteWindowById(WC_QUERY_STRING, 0);
		break;
	}
}

/** Network-safe changing of patch-settings.
 * @param p1 various bitstuffed elements
 * - p1 = (bit 0- 7) - the patches type (page) that is being changed (construction, network, ai)
 * - p2 = (bit 8-15) - the actual patch (entry) being set inside the category
 * @param p2 the new value for the patch
 * @todo check that the new value is a valid one. Awful lot of work, but since only
 * the server is allowed to do this, we trust it on this one :)
 */
int32 CmdChangePatchSetting(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	byte pcat = GB(p1, 0, 8);
	byte pel  = GB(p1, 8, 8);

	if (pcat >= lengthof(_patches_page)) return CMD_ERROR;
	if (pel >= _patches_page[pcat].num) return CMD_ERROR;

	if (flags & DC_EXEC) {
		const PatchEntry *pe = &_patches_page[pcat].entries[pel];
		WritePE(pe, (int32)p2);

		InvalidateWindow(WC_GAME_OPTIONS, 0);
	}

	return 0;
}

static const PatchEntry *IConsoleGetPatch(const char *name, uint *page, uint *entry)
{
	const PatchPage *pp;
	const PatchEntry *pe;

	for (*page = 0; *page < lengthof(_patches_page); (*page)++) {
		pp = &_patches_page[*page];
		for (*entry = 0; *entry < pp->num; (*entry)++) {
			pe = &pp->entries[*entry];
			if (strncmp(pe->console_name, name, sizeof(pe->console_name)) == 0)
				return pe;
		}
	}

	return NULL;
}

/* Those 2 functions need to be here, else we have to make some stuff non-static
    and besides, it is also better to keep stuff like this at the same place */
void IConsoleSetPatchSetting(char *name, const char *value)
{
	const PatchEntry *pe;
	uint page, entry;
	int val;

	pe = IConsoleGetPatch(name, &page, &entry);

	if (pe == NULL) {
		IConsolePrintF(_icolour_warn, "'%s' is an unknown patch setting.", name);
		return;
	}

	sscanf(value, "%d", &val);

	if (pe->type == PE_CURRENCY) // currency can be different on each client
		val /= GetCurrentCurrencyRate();

	// If an item is playerbased, we do not send it over the network (if any)
	if (pe->flags & PF_PLAYERBASED) {
		WritePE(pe, val);
	} else // Else we do
		DoCommandP(0, page + (entry << 8), val, NULL, CMD_CHANGE_PATCH_SETTING);

	{
		char tval[20];
		const char *tval2 = value;
		if (pe->type == PE_BOOL) {
			snprintf(tval, sizeof(tval), (val == 1) ? "on" : "off");
			tval2 = tval;
		}

		IConsolePrintF(_icolour_warn, "'%s' changed to:  %s", name, tval2);
	}
}

void IConsoleGetPatchSetting(const char *name)
{
	char value[20];
	uint page, entry;
	const PatchEntry *pe = IConsoleGetPatch(name, &page, &entry);

	/* We did not find the patch setting */
	if (pe == NULL) {
		IConsolePrintF(_icolour_warn, "'%s' is an unknown patch setting.", name);
		return;
	}

	if (pe->type == PE_BOOL) {
		snprintf(value, sizeof(value), (ReadPE(pe) == 1) ? "on" : "off");
	} else
		snprintf(value, sizeof(value), "%d", ReadPE(pe));

	IConsolePrintF(_icolour_warn, "Current value for '%s' is: '%s'", name, value);
}

static const Widget _patches_selection_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    10,     0,    10,     0,    13, STR_00C5,												STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    10,    11,   369,     0,    13, STR_CONFIG_PATCHES_CAPTION,			STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    10,     0,   369,    14,    41, 0x0,															STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    10,     0,   369,    42,   320, 0x0,															STR_NULL},

{   WWT_CLOSEBOX,   RESIZE_NONE,     3,    10,    96,    16,    27, STR_CONFIG_PATCHES_GUI,					STR_NULL},
{   WWT_CLOSEBOX,   RESIZE_NONE,     3,    97,   183,    16,    27, STR_CONFIG_PATCHES_CONSTRUCTION,	STR_NULL},
{   WWT_CLOSEBOX,   RESIZE_NONE,     3,   184,   270,    16,    27, STR_CONFIG_PATCHES_VEHICLES,			STR_NULL},
{   WWT_CLOSEBOX,   RESIZE_NONE,     3,   271,   357,    16,    27, STR_CONFIG_PATCHES_STATIONS,			STR_NULL},
{   WWT_CLOSEBOX,   RESIZE_NONE,     3,    10,    96,    28,    39, STR_CONFIG_PATCHES_ECONOMY,			STR_NULL},
{   WWT_CLOSEBOX,   RESIZE_NONE,     3,    97,   183,    28,    39, STR_CONFIG_PATCHES_AI,						STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _patches_selection_desc = {
	WDP_CENTER, WDP_CENTER, 370, 321,
	WC_GAME_OPTIONS,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_patches_selection_widgets,
	PatchesSelectionWndProc,
};

void ShowPatchesSelection(void)
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	AllocateWindowDesc(&_patches_selection_desc);
}

enum {
	NEWGRF_WND_PROC_OFFSET_TOP_WIDGET = 14,
	NEWGRF_WND_PROC_ROWSIZE = 14
};

static void NewgrfWndProc(Window *w, WindowEvent *e)
{
	static GRFFile *_sel_grffile;
	switch (e->event) {
	case WE_PAINT: {
		int x, y = NEWGRF_WND_PROC_OFFSET_TOP_WIDGET;
		uint16 i = 0;
		GRFFile *c = _first_grffile;

		DrawWindowWidgets(w);

		if (_first_grffile == NULL) { // no grf sets installed
			DrawStringMultiCenter(140, 210, STR_NEWGRF_NO_FILES_INSTALLED, 250);
			break;
		}

		// draw list of all grf files
		while (c != NULL) {
			if (i >= w->vscroll.pos) { // draw files according to scrollbar position
				bool h = (_sel_grffile==c);
				// show highlighted item with a different background and highlighted text
				if(h) GfxFillRect(1, y + 1, 267, y + 12, 156);
				// XXX - will be grf name later
				DoDrawString(c->filename, 25, y + 2, h ? 0xC : 0x10);
				DrawSprite(SPRITE_PALETTE(SPR_SQUARE | PALETTE_TO_RED), 5, y + 2);
				y += NEWGRF_WND_PROC_ROWSIZE;
			}

			c = c->next;
			if (++i == w->vscroll.cap + w->vscroll.pos) break; // stop after displaying 12 items
		}

// 		DoDrawString(_sel_grffile->setname, 120, 200, 0x01); // draw grf name

		if (_sel_grffile == NULL) { // no grf file selected yet
			DrawStringMultiCenter(140, 210, STR_NEWGRF_TIP, 250);
		} else {
			// draw filename
			x = DrawString(5, 199, STR_NEWGRF_FILENAME, 0);
			DoDrawString(_sel_grffile->filename, x + 2, 199, 0x01);

			// draw grf id
			x = DrawString(5, 209, STR_NEWGRF_GRF_ID, 0);
			snprintf(_userstring, lengthof(_userstring), "%08X", _sel_grffile->grfid);
			DrawString(x + 2, 209, STR_SPEC_USERSTRING, 0x01);
		}
	} break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 3: { // select a grf file
			int y = (e->click.pt.y - NEWGRF_WND_PROC_OFFSET_TOP_WIDGET) / NEWGRF_WND_PROC_ROWSIZE;

			if (y >= w->vscroll.cap) { return;} // click out of bounds

			y += w->vscroll.pos;

			if (y >= _grffile_count) return;

			_sel_grffile = _first_grffile;
			// get selected grf-file
			while (y-- != 0) _sel_grffile = _sel_grffile->next;

			SetWindowDirty(w);
		} break;
		case 9: /* Cancel button */
			DeleteWindowById(WC_GAME_OPTIONS, 0);
			break;
		} break;

/* Parameter edit box not used yet
	case WE_TIMEOUT:
		WP(w,def_d).data_2 = 0;
		SetWindowDirty(w);
		break;

	case WE_ON_EDIT_TEXT: {
		if (*e->edittext.str) {
			SetWindowDirty(w);
		}
		break;
	}
*/
	case WE_DESTROY:
		_sel_grffile = NULL;
		DeleteWindowById(WC_QUERY_STRING, 0);
		break;
	}
}

static const Widget _newgrf_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,										STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   279,     0,    13, STR_NEWGRF_SETTINGS_CAPTION,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   279,   183,   276, 0x0,													STR_NULL},

{     WWT_MATRIX,   RESIZE_NONE,    14,     0,   267,    14,   182, 0xC01,/*small rows*/					STR_NEWGRF_TIP},
{  WWT_SCROLLBAR,   RESIZE_NONE,    14,   268,   279,    14,   182, 0x0,													STR_0190_SCROLL_BAR_SCROLLS_LIST},

{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   147,   158,   244,   255, STR_0188,	STR_NULL},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   159,   170,   244,   255, STR_0189,	STR_NULL},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   175,   274,   244,   255, STR_NEWGRF_SET_PARAMETERS,		STR_NULL},

{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,     5,   138,   261,   272, STR_NEWGRF_APPLY_CHANGES,		STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,   142,   274,   261,   272, STR_012E_CANCEL,							STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _newgrf_desc = {
	WDP_CENTER, WDP_CENTER, 280, 277,
	WC_GAME_OPTIONS,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_newgrf_widgets,
	NewgrfWndProc,
};

void ShowNewgrf(void)
{
	Window *w;
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	w = AllocateWindowDesc(&_newgrf_desc);

	{ // little helper function to calculate _grffile_count
	  // should be REMOVED once _grffile_count is calculated at loading
		GRFFile *c = _first_grffile;

		_grffile_count = 0;
		while (c != NULL) {
			_grffile_count++;
			c = c->next;
		}
	}

	w->vscroll.cap = 12;
	w->vscroll.count = _grffile_count;
	w->vscroll.pos = 0;
	w->disabled_state = (1 << 5) | (1 << 6) | (1 << 7);
}

/* state: 0 = none clicked, 0x01 = first clicked, 0x02 = second clicked */
void DrawArrowButtons(int x, int y, int state)
{
	DrawFrameRect(x, y+1, x+9, y+9, 3, (state & 0x01) ? FR_LOWERED : 0);
	DrawFrameRect(x+10, y+1, x+19, y+9, 3, (state & 0x02) ? FR_LOWERED : 0);
	DrawStringCentered(x+5, y+1, STR_6819, 0);
	DrawStringCentered(x+15, y+1, STR_681A, 0);
}

char _str_separator[2];

static void CustCurrencyWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		int x=35, y=20, i=0;
		int clk = WP(w,def_d).data_1;
		DrawWindowWidgets(w);

		// exchange rate
		DrawArrowButtons(10, y, (clk >> (i*2)) & 0x03);
		SetDParam(0, 1);
		SetDParam(1, 1);
		DrawString(x, y + 1, STR_CURRENCY_EXCHANGE_RATE, 0);
		x = 35;
		y+=12;
		i++;

		// separator
		DrawFrameRect(10, y+1, 29, y+9, 0, ((clk >> (i*2)) & 0x03) ? FR_LOWERED : 0);
		x = DrawString(x, y + 1, STR_CURRENCY_SEPARATOR, 0);
		DoDrawString(_str_separator, x + 4, y + 1, 6);
		x = 35;
		y+=12;
		i++;

		// prefix
		DrawFrameRect(10, y+1, 29, y+9, 0, ((clk >> (i*2)) & 0x03) ? FR_LOWERED : 0);
		x = DrawString(x, y + 1, STR_CURRENCY_PREFIX, 0);
		DoDrawString(_currency_specs[23].prefix, x + 4, y + 1, 6);
		x = 35;
		y+=12;
		i++;

		// suffix
		DrawFrameRect(10, y+1, 29, y+9, 0, ((clk >> (i*2)) & 0x03) ? FR_LOWERED : 0);
		x = DrawString(x, y + 1, STR_CURRENCY_SUFFIX, 0);
		DoDrawString(_currency_specs[23].suffix, x + 4, y + 1, 6);
		x = 35;
		y+=12;
		i++;

		// switch to euro
		DrawArrowButtons(10, y, (clk >> (i*2)) & 0x03);
		SetDParam(0, _currency_specs[23].to_euro);
		DrawString(x, y + 1, (_currency_specs[23].to_euro)?STR_CURRENCY_SWITCH_TO_EURO:STR_CURRENCY_SWITCH_TO_EURO_NEVER, 0);
		x = 35;
		y+=12;
		i++;

		// Preview
		y+=12;
		SetDParam(0, 10000);
		DrawString(x, y + 1, STR_CURRENCY_PREVIEW, 0);
	} break;

	case WE_CLICK: {
		bool edittext = false;
		int line = (e->click.pt.y - 20)/12;
		int len = 0;
		int x = e->click.pt.x;
		StringID str = 0;

		switch ( line ) {
			case 0: // rate
				if ( IS_INT_INSIDE(x, 10, 30) ) { // clicked buttons
					if (x < 20) {
						_currency_specs[23].rate = max(1, _currency_specs[23].rate-1);
						WP(w,def_d).data_1 =  (1 << (line * 2 + 0));
					} else {
						_currency_specs[23].rate = min(5000, _currency_specs[23].rate+1);
						WP(w,def_d).data_1 =  (1 << (line * 2 + 1));
					}
				} else { // enter text
					SetDParam(0, _currency_specs[23].rate);
					str = STR_CONFIG_PATCHES_INT32;
					len = 4;
					edittext = true;
				}
			break;
			case 1: // separator
				if ( IS_INT_INSIDE(x, 10, 30) )  // clicked button
					WP(w,def_d).data_1 =  (1 << (line * 2 + 1));
				str = BindCString(_str_separator);
				len = 1;
				edittext = true;
			break;
			case 2: // prefix
				if ( IS_INT_INSIDE(x, 10, 30) )  // clicked button
					WP(w,def_d).data_1 =  (1 << (line * 2 + 1));
				str = BindCString(_currency_specs[23].prefix);
				len = 12;
				edittext = true;
			break;
			case 3: // suffix
				if ( IS_INT_INSIDE(x, 10, 30) )  // clicked button
					WP(w,def_d).data_1 =  (1 << (line * 2 + 1));
				str = BindCString(_currency_specs[23].suffix);
				len = 12;
				edittext = true;
			break;
			case 4: // to euro
				if ( IS_INT_INSIDE(x, 10, 30) ) { // clicked buttons
					if (x < 20) {
						if(_currency_specs[23].to_euro <= 2000) _currency_specs[23].to_euro = 0;
						else _currency_specs[23].to_euro--;
						WP(w,def_d).data_1 = (1 << (line * 2 + 0));
					} else {
						if(_currency_specs[23].to_euro == 0) _currency_specs[23].to_euro = 2000;
						else _currency_specs[23].to_euro++;
						_currency_specs[23].to_euro = min(MAX_YEAR_END_REAL, _currency_specs[23].to_euro);
						WP(w,def_d).data_1 = (1 << (line * 2 + 1));
					}
				} else { // enter text
					SetDParam(0, _currency_specs[23].to_euro);
					str = STR_CONFIG_PATCHES_INT32;
					len = 4;
					edittext = true;
				}
			break;
		}

		if (edittext) {
			WP(w,def_d).data_2 = line;
			ShowQueryString(
			str,
			STR_CURRENCY_CHANGE_PARAMETER,
			len + 1, // maximum number of characters OR
			250, // characters up to this width pixels, whichever is satisfied first
			w->window_class,
			w->window_number);
		}

		w->flags4 |= 5 << WF_TIMEOUT_SHL;
		SetWindowDirty(w);
	} break;

	case WE_ON_EDIT_TEXT: {
			int val;
			const char *b = e->edittext.str;
			switch (WP(w,def_d).data_2) {
				case 0: /* Exchange rate */
					val = atoi(b);
					val = clamp(val, 1, 5000);
					_currency_specs[23].rate = val;
				break;
				case 1: /* Thousands seperator */
					_currency_specs[23].separator = (b[0] == '\0') ? ' ' : b[0];
					ttd_strlcpy(_str_separator, b, 16);
				break;
				case 2: /* Currency prefix */
					ttd_strlcpy(_currency_specs[23].prefix, b, 16);
				break;
				case 3: /* Currency suffix */
					ttd_strlcpy(_currency_specs[23].suffix, b, 16);
				break;
				case 4: /* Year to switch to euro */
					val = atoi(b);
					val = clamp(val, 1999, MAX_YEAR_END_REAL);
					if (val == 1999) val = 0;
					_currency_specs[23].to_euro = val;
				break;
			}
		MarkWholeScreenDirty();


	} break;

	case WE_TIMEOUT:
		WP(w,def_d).data_1 = 0;
		SetWindowDirty(w);
		break;

	case WE_DESTROY:
		DeleteWindowById(WC_QUERY_STRING, 0);
		MarkWholeScreenDirty();
		break;
	}
}

static const Widget _cust_currency_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,						STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   229,     0,    13, STR_CURRENCY_WINDOW,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   229,    14,   119, 0x0,									STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _cust_currency_desc = {
	WDP_CENTER, WDP_CENTER, 230, 120,
	WC_CUSTOM_CURRENCY, 0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_cust_currency_widgets,
	CustCurrencyWndProc,
};

void ShowCustCurrency(void)
{
	Window *w;

	_str_separator[0] = _currency_specs[23].separator;
	_str_separator[1] = '\0';

	DeleteWindowById(WC_CUSTOM_CURRENCY, 0);
	w = AllocateWindowDesc(&_cust_currency_desc);
}

