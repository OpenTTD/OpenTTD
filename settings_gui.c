#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "command.h"
#include "engine.h"
#include "screenshot.h"
#include "newgrf.h"
#include "network.h"

static uint32 _difficulty_click_a;
static uint32 _difficulty_click_b;
static byte _difficulty_timeout;

extern const StringID _currency_string_list[];
extern uint GetMaskOfAllowedCurrencies();

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

static int GetCurRes()
{
	int i;
	for(i = 0; i != _num_resolutions; i++)
		if (_resolutions[i][0] == _screen.width &&
				_resolutions[i][1] == _screen.height)
			break;
	return i;
}

static void GameOptionsWndProc(Window *w, WindowEvent *e)
{
	int i;

	switch(e->event) {
	case WE_PAINT: {
		StringID str = STR_02BE_DEFAULT;
		w->disabled_state = (_vehicle_design_names & 1) ? (++str, 0) : (1 << 21);
		SetDParam(0, str);
		SetDParam(1, _currency_string_list[_opt_mod_ptr->currency]);
		SetDParam(2, _opt_mod_ptr->kilometers + STR_0139_IMPERIAL_MILES);
		SetDParam(3, STR_02E9_DRIVE_ON_LEFT + _opt_mod_ptr->road_side);
		SetDParam(4, STR_TOWNNAME_ENGLISH + _opt_mod_ptr->town_name);
		SetDParam(5, _autosave_dropdown[_opt_mod_ptr->autosave]);
		SetDParam(6, SPECSTR_LANGUAGE_START + _dynlang.curr);
		i = GetCurRes();
		SetDParam(7, i == _num_resolutions ? STR_RES_OTHER : SPECSTR_RESOLUTION_START + i);
		SetDParam(8, SPECSTR_SCREENSHOT_START + _cur_screenshot_format);
		(_fullscreen) ? SETBIT(w->click_state, 28) : CLRBIT(w->click_state, 28); // fullscreen button

		DrawWindowWidgets(w);
		DrawString(20, 175, STR_OPTIONS_FULLSCREEN, 0); // fullscreen
	}	break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 5:
			ShowDropDownMenu(w, _currency_string_list, _opt_mod_ptr->currency, e->click.widget, _game_mode == GM_MENU ? 0 : ~GetMaskOfAllowedCurrencies());
			return;
		case 8:
			ShowDropDownMenu(w, _distances_dropdown, _opt_mod_ptr->kilometers, e->click.widget, 0);
			return;
		case 11: {
			int i = _opt_mod_ptr->road_side;
			ShowDropDownMenu(w, _driveside_dropdown, i, e->click.widget, (_game_mode == GM_MENU) ? 0 : (-1) ^ (1 << i));
			return;
		}
		case 14: {
			int i = _opt_mod_ptr->town_name;
			ShowDropDownMenu(w, BuildDynamicDropdown(STR_TOWNNAME_ENGLISH, SPECSTR_TOWNNAME_LAST - SPECSTR_TOWNNAME_START + 1), i, e->click.widget, (_game_mode == GM_MENU) ? 0 : (-1) ^ (1 << i));
			return;
		}
		case 17:
			ShowDropDownMenu(w, _autosave_dropdown, _opt_mod_ptr->autosave, e->click.widget, 0);
			return;
		case 20:
			ShowDropDownMenu(w, _designnames_dropdown, (_vehicle_design_names&1)?1:0, e->click.widget, (_vehicle_design_names&2)?0:2);
			return;
		case 21:
			return;
		case 24:
			ShowDropDownMenu(w, _dynlang.dropdown, _dynlang.curr, e->click.widget, 0);
			return;
		case 27:
			// setup resolution dropdown
			ShowDropDownMenu(w, BuildDynamicDropdown(SPECSTR_RESOLUTION_START, _num_resolutions), GetCurRes(), e->click.widget, 0);
			return;
		case 28: /* Click fullscreen on/off */
			(_fullscreen) ? CLRBIT(w->click_state, 28) : SETBIT(w->click_state, 28);
			ToggleFullScreen(!_fullscreen); // toggle full-screen on/off
			SetWindowDirty(w);
			return;
		case 31: /* Setup screenshot format dropdown */
			ShowDropDownMenu(w, BuildDynamicDropdown(SPECSTR_SCREENSHOT_START, _num_screenshot_formats), _cur_screenshot_format, e->click.widget, 0);
			return;

		}
		break;

	case WE_DROPDOWN_SELECT:
		switch(e->dropdown.button) {
		case 20:
			if (e->dropdown.index == 0) {
				DeleteCustomEngineNames();
				MarkWholeScreenDirty();
			} else if (!(_vehicle_design_names&1)) {
				LoadCustomEngineNames();
				MarkWholeScreenDirty();
			}
			break;
		case 5:
			_opt_mod_ptr->currency = _opt.currency = e->dropdown.index;
			MarkWholeScreenDirty();
			break;
		case 8:
			_opt_mod_ptr->kilometers = e->dropdown.index;
			MarkWholeScreenDirty();
			break;
		case 11:
			if (_game_mode == GM_MENU)
				DoCommandP(0, e->dropdown.index, 0, NULL, CMD_SET_ROAD_DRIVE_SIDE | CMD_MSG(STR_EMPTY));
			break;
		case 14:
			if (_game_mode == GM_MENU)
				DoCommandP(0, e->dropdown.index, 0, NULL, CMD_SET_TOWN_NAME_TYPE | CMD_MSG(STR_EMPTY));
			break;
		case 17:
			_opt_mod_ptr->autosave = e->dropdown.index;
			SetWindowDirty(w);
			break;

		// change interface language
		case 24:
			ReadLanguagePack(e->dropdown.index);
			MarkWholeScreenDirty();
			break;

		// change resolution
		case 27:
			if (e->dropdown.index < _num_resolutions && ChangeResInGame(_resolutions[e->dropdown.index][0],_resolutions[e->dropdown.index][1]))
				SetWindowDirty(w);
			break;

		// change screenshot format
		case 31:
			SetScreenshotFormat(e->dropdown.index);
			SetWindowDirty(w);
			break;
		}
		break;
	}
}

int32 CmdSetRoadDriveSide(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (flags & DC_EXEC) {
		_opt_mod_ptr->road_side = p1;
		InvalidateWindow(WC_GAME_OPTIONS,0);
	}
	return 0;
}

int32 CmdSetTownNameType(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (flags & DC_EXEC) {
		_opt_mod_ptr->town_name = p1;
		InvalidateWindow(WC_GAME_OPTIONS,0);
	}
	return 0;
}


static const Widget _game_options_widgets[] = {
{   WWT_CLOSEBOX,    14,     0,    10,     0,    13, STR_00C5,								STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   369,     0,    13, STR_00B1_GAME_OPTIONS,		STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,    14,     0,   369,    14,   238, 0x0,											STR_NULL},
{      WWT_FRAME,    14,    10,   179,    20,    55, STR_02E0_CURRENCY_UNITS,	STR_NULL},
{          WWT_6,    14,    20,   169,    34,    45, STR_02E1,								STR_02E2_CURRENCY_UNITS_SELECTION},
{   WWT_CLOSEBOX,    14,   158,   168,    35,    44, STR_0225,								STR_02E2_CURRENCY_UNITS_SELECTION},
{      WWT_FRAME,    14,   190,   359,    20,    55, STR_02E3_DISTANCE_UNITS,	STR_NULL},
{          WWT_6,    14,   200,   349,    34,    45, STR_02E4,								STR_02E5_DISTANCE_UNITS_SELECTION},
{   WWT_CLOSEBOX,    14,   338,   348,    35,    44, STR_0225,								STR_02E5_DISTANCE_UNITS_SELECTION},
{      WWT_FRAME,    14,    10,   179,    62,    97, STR_02E6_ROAD_VEHICLES,	STR_NULL},
{          WWT_6,    14,    20,   169,    76,    87, STR_02E7,								STR_02E8_SELECT_SIDE_OF_ROAD_FOR},
{   WWT_CLOSEBOX,    14,   158,   168,    77,    86, STR_0225,								STR_02E8_SELECT_SIDE_OF_ROAD_FOR},
{      WWT_FRAME,    14,   190,   359,    62,    97, STR_02EB_TOWN_NAMES,			STR_NULL},
{          WWT_6,    14,   200,   349,    76,    87, STR_02EC,								STR_02ED_SELECT_STYLE_OF_TOWN_NAMES},
{   WWT_CLOSEBOX,    14,   338,   348,    77,    86, STR_0225,								STR_02ED_SELECT_STYLE_OF_TOWN_NAMES},
{      WWT_FRAME,    14,    10,   179,   104,   139, STR_02F4_AUTOSAVE,				STR_NULL},
{          WWT_6,    14,    20,   169,   118,   129, STR_02F5,								STR_02F6_SELECT_INTERVAL_BETWEEN},
{   WWT_CLOSEBOX,    14,   158,   168,   119,   128, STR_0225,								STR_02F6_SELECT_INTERVAL_BETWEEN},

{      WWT_FRAME,    14,    10,   359,   194,   228, STR_02BC_VEHICLE_DESIGN_NAMES,				STR_NULL},
{          WWT_6,    14,    20,   119,   207,   218, STR_02BD,								STR_02C1_VEHICLE_DESIGN_NAMES_SELECTION},
{   WWT_CLOSEBOX,    14,   108,   118,   208,   217, STR_0225,								STR_02C1_VEHICLE_DESIGN_NAMES_SELECTION},
{   WWT_CLOSEBOX,    14,   130,   349,   207,   218, STR_02C0_SAVE_CUSTOM_NAMES_TO_DISK,	STR_02C2_SAVE_CUSTOMIZED_VEHICLE},

{      WWT_FRAME,    14,   190,   359,   104,   139, STR_OPTIONS_LANG,				STR_NULL},
{          WWT_6,    14,   200,   349,   118,   129, STR_OPTIONS_LANG_CBO,		STR_OPTIONS_LANG_TIP},
{   WWT_CLOSEBOX,    14,   338,   348,   119,   128, STR_0225,								STR_OPTIONS_LANG_TIP},

{      WWT_FRAME,    14,    10,   179,   146,   190, STR_OPTIONS_RES,					STR_NULL},
{          WWT_6,    14,    20,   169,   160,   171, STR_OPTIONS_RES_CBO,			STR_OPTIONS_RES_TIP},
{   WWT_CLOSEBOX,    14,   158,   168,   161,   170, STR_0225,								STR_OPTIONS_RES_TIP},
{    WWT_TEXTBTN,    14,   149,   169,   176,   184, STR_EMPTY,								STR_OPTIONS_FULLSCREEN_TIP},

{      WWT_FRAME,    14,   190,   359,   146,   190, STR_OPTIONS_SCREENSHOT_FORMAT,				STR_NULL},
{          WWT_6,    14,   200,   349,   160,   171, STR_OPTIONS_SCREENSHOT_FORMAT_CBO,		STR_OPTIONS_SCREENSHOT_FORMAT_TIP},
{   WWT_CLOSEBOX,    14,   338,   348,   161,   170, STR_0225,								STR_OPTIONS_SCREENSHOT_FORMAT_TIP},

{   WIDGETS_END},
};

static const WindowDesc _game_options_desc = {
	WDP_CENTER, WDP_CENTER, 370, 239,
	WC_GAME_OPTIONS,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_RESTORE_DPARAM | WDF_UNCLICK_BUTTONS,
	_game_options_widgets,
	GameOptionsWndProc
};


void ShowGameOptions()
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
	{0,7,1,0},
	{0,3,1,STR_6830_IMMEDIATE},
	{0,2,1,STR_6816_LOW},
	{0,3,1,STR_26816_NONE},
	{100,500,50,0},
	{2,4,1,0},
	{0,2,1,STR_6820_LOW},
	{0,4,1,STR_681B_VERY_SLOW},
	{0,2,1,STR_6820_LOW},
	{0,2,1,STR_6823_NONE},
	{0,3,1,STR_6826_X1_5},
	{0,2,1,STR_6820_LOW},
	{0,3,1,STR_682A_VERY_FLAT},
	{0,3,1,STR_VERY_LOW},
	{0,1,1,STR_682E_STEADY},
	{0,1,1,STR_6834_AT_END_OF_LINE_AND_AT_STATIONS},
	{0,1,1,STR_6836_OFF},
	{0,2,1,STR_6839_PERMISSIVE},
};

static inline bool GetBitAndShift(uint32 *b)
{
	uint32 x = *b;
	*b >>= 1;
	return (x&1) != 0;
}

static const int16 _default_game_diff[3][GAME_DIFFICULTY_NUM] = {
	{2, 2, 1, 3, 300, 2, 0, 2, 0, 1, 2, 0, 1, 0, 0, 0, 0, 0},
	{4, 1, 1, 2, 150, 3, 1, 3, 1, 2, 1, 1, 2, 1, 1, 1, 1, 1},
	{7, 0, 2, 2, 100, 4, 1, 3, 2, 2, 0, 2, 3, 2, 1, 1, 1, 2},
};

void SetDifficultyLevel(int mode, GameOptions *gm_opt)
{
	int i;
	assert(mode <= 3);

	gm_opt->diff_level = mode;
	if (mode != 3) { // not custom
		for(i = 0; i != GAME_DIFFICULTY_NUM; i++)
			((int*)&gm_opt->diff)[i] = _default_game_diff[mode][i];
	}
}

extern void StartupEconomy();

static void GameDifficultyWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		uint32 click_a, click_b, disabled;
		int i;
		int x,y,value;

		w->click_state = (1 << 4) << _opt_mod_temp.diff_level;
		w->disabled_state = (_game_mode != GM_NORMAL) ? 0 : (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7);
		// Disable save-button in multiplayer (and if client)
		if (_networking && !_network_server)
			w->disabled_state |= (1 << 10);
		DrawWindowWidgets(w);

		click_a = _difficulty_click_a;
		click_b = _difficulty_click_b;

		/* XXX - This is most likely the worst way I have ever seen
		     to disable some buttons and to enable others.
		     What the value means, is this:
		       if bit1 is enabled, setting 1 is disabled
		       then it is shifted to the left, and the story
		       repeats....
		   -- TrueLight */
		disabled = _game_mode == GM_NORMAL ? 0x383E : 0;

		x = 0;
		y = 32;
		for (i = 0; i != GAME_DIFFICULTY_NUM; i++) {
			DrawFrameRect(x+5, y+1, x+5+9, y+9, 3, GetBitAndShift(&click_a)?0x20:0);
			DrawFrameRect(x+15, y+1, x+15+9, y+9, 3, GetBitAndShift(&click_b)?0x20:0);
			if (GetBitAndShift(&disabled) || (_networking && !_network_server)) {
				int color = 0x8000 | _color_list[3].unk2;
				GfxFillRect(x+6, y+2, x+6+8, y+9, color);
				GfxFillRect(x+16, y+2, x+16+8, y+9, color);
			}

			DrawStringCentered(x+10, y+1, STR_6819, 0);
			DrawStringCentered(x+20, y+1, STR_681A, 0);


			value = _game_setting_info[i].str + ((int*)&_opt_mod_temp.diff)[i];
			if (i == 4) value *= 1000; // handle currency option
			SetDParam(0, value);
			DrawString(x+30, y+1, STR_6805_MAXIMUM_NO_COMPETITORS + i, 0);

			y += 11;
		}
	} break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 3: {
			int x,y;
			uint btn, dis;
			int val;
			const GameSettingData *info;

			// Don't allow clients to make any changes
			if  (_networking && !_network_server)
				return;

			x = e->click.pt.x - 5;
			if (!IS_INT_INSIDE(x, 0, 21))
				return;

			y = e->click.pt.y - 33;
			if (y < 0)
				return;

			// Get button from Y coord.
			btn = y / 11;
			if (btn >= GAME_DIFFICULTY_NUM || y % 11 > 9)
				return;

			// Clicked disabled button?
			dis = 0;
			if (_game_mode == GM_NORMAL)
				dis |= 0x383E;
			if (HASBIT(dis, btn))
				return;

			_difficulty_timeout = 5;

			val = ((int*)&_opt_mod_temp.diff)[btn];

			info = &_game_setting_info[btn];
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
			break;
		}
		case 4: case 5: case 6: case 7: // easy/medium/hard/custom
			// temporarily change difficulty level
			SetDifficultyLevel(e->click.widget - 4, &_opt_mod_temp);
			SetWindowDirty(w);
			break;
		case 8:
			ShowHighscoreTable(_opt_mod_ptr->diff_level);
			break;
		case 10: { // Save button - save changes
			int btn, val;
			for (btn = 0; btn != GAME_DIFFICULTY_NUM; btn++) {
				val = ((int*)&_opt_mod_temp.diff)[btn];
				// if setting has changed, change it
				if (val != ((int*)&_opt_mod_ptr->diff)[btn])
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
		case 11: // Cancel button - close window
			DeleteWindow(w);
			break;
		}
		break;

	case WE_MOUSELOOP:
		if (_difficulty_timeout != 0 && !--_difficulty_timeout) {
			_difficulty_click_a = 0;
			_difficulty_click_b = 0;
			SetWindowDirty(w);
		}
		break;
	}
}

static const Widget _game_difficulty_widgets[] = {
{   WWT_CLOSEBOX,    10,     0,    10,     0,    13, STR_00C5,									STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    10,    11,   369,     0,    13, STR_6800_DIFFICULTY_LEVEL,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,    10,     0,   369,    14,    29, 0x0,												STR_NULL},
{      WWT_PANEL,    10,     0,   369,    30,   276, 0x0,												STR_NULL},
{ WWT_PUSHTXTBTN,     3,    10,    96,    16,    27, STR_6801_EASY,							STR_NULL},
{ WWT_PUSHTXTBTN,     3,    97,   183,    16,    27, STR_6802_MEDIUM,						STR_NULL},
{ WWT_PUSHTXTBTN,     3,   184,   270,    16,    27, STR_6803_HARD,							STR_NULL},
{ WWT_PUSHTXTBTN,     3,   271,   357,    16,    27, STR_6804_CUSTOM,						STR_NULL},
{      WWT_EMPTY,    10,     0,   369,   251,   262, 0x0,												STR_NULL},
//{   WWT_CLOSEBOX,    10,     0,   369,   251,   262, STR_6838_SHOW_HI_SCORE_CHART,STR_NULL},
{      WWT_PANEL,    10,     0,   369,   263,   278, 0x0,												STR_NULL},
{ WWT_PUSHTXTBTN,     3,   105,   185,   265,   276, STR_OPTIONS_SAVE_CHANGES,	STR_NULL},
{ WWT_PUSHTXTBTN,     3,   186,   266,   265,   276, STR_012E_CANCEL,						STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _game_difficulty_desc = {
	WDP_CENTER, WDP_CENTER, 370, 279,
	WC_GAME_OPTIONS,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_game_difficulty_widgets,
	GameDifficultyWndProc
};

void ShowGameDifficulty()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	/*	copy current settings to temporary holding place
	 *	change that when setting stuff, copy back on clicking 'OK'
	 */
	memcpy(&_opt_mod_temp, _opt_mod_ptr, sizeof(GameOptions));
	AllocateWindowDesc(&_game_difficulty_desc);
}

void ShowHighscoreTable(int tbl)
{
	ShowInfoF("ShowHighscoreTable(%d) not implemented", tbl);
}

// virtual PositionMainToolbar function, calls the right one.
int32 v_PositionMainToolbar(int32 p1)
{
	if (_game_mode != GM_MENU)
		PositionMainToolbar(NULL);

	return 0;
}

int32 AiNew_PatchActive_Warning(int32 p1)
{
  if (p1 == 1)
    ShowErrorMessage(-1, TEMP_AI_ACTIVATED, 0, 0);

  return 0;
}

int32 InvisibleTreesActive(int32 p1)
{
	MarkWholeScreenDirty();
	return 0;
}

int32 InValidateDetailsWindow(int32 p1)
{
	InvalidateWindowClasses(WC_VEHICLE_DETAILS);
	return 0;
}

/* Check service intervals of vehicles, p1 is value of % or day based servicing */
int32 CheckInterval(int32 p1)
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
	byte type;										// type of selector
	byte flags;										// selector flags
	StringID str;									// string with descriptive text
	void *variable;								// pointer to the variable
	int32 min,max;								// range for spinbox setting
	uint32 step;									// step for spinbox
	PatchButtonClick *click_proc;	// callback procedure
} PatchEntry;

enum {
	PE_BOOL			= 0,
	PE_UINT8		= 1,
	PE_INT16		= 2,
	PE_UINT16		= 3,
	PE_INT32		= 4,
	PE_CURRENCY	= 5,

	PF_0ISDIS				= 1,
	PF_NOCOMMA			= 2,
	PF_MULTISTRING	= 4,
	PF_PLAYERBASED	= 8, // This has to match the entries that are in settings.c, patch_player_settings
};

static const PatchEntry _patches_ui[] = {
	{PE_BOOL,		PF_PLAYERBASED, STR_CONFIG_PATCHES_VEHICLESPEED,			&_patches.vehicle_speed,						0,  0,  0, NULL},
	{PE_BOOL,		PF_PLAYERBASED, STR_CONFIG_PATCHES_LONGDATE,					&_patches.status_long_date,					0,  0,  0, NULL},
	{PE_BOOL,		PF_PLAYERBASED, STR_CONFIG_PATCHES_SHOWFINANCES,			&_patches.show_finances,						0,  0,  0, NULL},
	{PE_BOOL,		PF_PLAYERBASED, STR_CONFIG_PATCHES_AUTOSCROLL,				&_patches.autoscroll,								0,  0,  0, NULL},

	{PE_UINT8,	PF_PLAYERBASED, STR_CONFIG_PATCHES_ERRMSG_DURATION,	&_patches.errmsg_duration,					0, 20,  1, NULL},

	{PE_UINT8,	PF_MULTISTRING | PF_PLAYERBASED, STR_CONFIG_PATCHES_TOOLBAR_POS, &_patches.toolbar_pos,			0,  2,  1, &v_PositionMainToolbar},
	{PE_UINT8,	PF_0ISDIS | PF_PLAYERBASED, STR_CONFIG_PATCHES_SNAP_RADIUS, &_patches.window_snap_radius,     1, 32,  1, NULL},
	{PE_BOOL,		PF_PLAYERBASED, STR_CONFIG_PATCHES_INVISIBLE_TREES,	&_patches.invisible_trees,					0,  1,  1, &InvisibleTreesActive},
};

static const PatchEntry _patches_construction[] = {
	{PE_BOOL,		0, STR_CONFIG_PATCHES_BUILDONSLOPES,		&_patches.build_on_slopes,					0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_EXTRADYNAMITE,		&_patches.extra_dynamite,						0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_LONGBRIDGES,			&_patches.longbridges,							0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_SIGNALSIDE,				&_patches.signal_side,							0,  0,  0, NULL},

	{PE_BOOL,		0, STR_CONFIG_PATCHES_SMALL_AIRPORTS,		&_patches.always_small_airport,			0,  0,  0, NULL},
	{PE_UINT8,	PF_PLAYERBASED, STR_CONFIG_PATCHES_DRAG_SIGNALS_DENSITY, &_patches.drag_signals_density, 1, 20,  1, NULL},

};

static const PatchEntry _patches_vehicles[] = {
	{PE_BOOL,		0, STR_CONFIG_PATCHES_REALISTICACCEL,		&_patches.realistic_acceleration,		0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_MAMMOTHTRAINS,		&_patches.mammoth_trains,						0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_GOTODEPOT,				&_patches.gotodepot,								0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_ROADVEH_QUEUE,		&_patches.roadveh_queue,						0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_NEW_DEPOT_FINDING,&_patches.new_depot_finding,				0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_NEW_TRAIN_PATHFIND,				&_patches.new_pathfinding,	0,  0,  0, NULL},

	{PE_BOOL,		PF_PLAYERBASED, STR_CONFIG_PATCHES_WARN_INCOME_LESS, &_patches.train_income_warn,				0,  0,  0, NULL},
	{PE_UINT8,	PF_MULTISTRING | PF_PLAYERBASED, STR_CONFIG_PATCHES_ORDER_REVIEW,&_patches.order_review_system,0,2,  1, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_NEVER_EXPIRE_VEHICLES,		&_patches.never_expire_vehicles,0,0,0, NULL},

	{PE_UINT16, PF_0ISDIS | PF_PLAYERBASED, STR_CONFIG_PATCHES_LOST_TRAIN_DAYS, &_patches.lost_train_days,	180,720, 60, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_AUTORENEW_VEHICLE,&_patches.autorenew,								0,  0,  0, NULL},
	{PE_INT16,	0, STR_CONFIG_PATCHES_AUTORENEW_MONTHS, &_patches.autorenew_months,				-12, 12,  1, NULL},
	{PE_CURRENCY, 0, STR_CONFIG_PATCHES_AUTORENEW_MONEY,&_patches.autorenew_money,					0, 2000000, 100000, NULL},

	{PE_UINT8,	0, STR_CONFIG_PATCHES_MAX_TRAINS,				&_patches.max_trains,								0,240, 10, NULL},
	{PE_UINT8,	0, STR_CONFIG_PATCHES_MAX_ROADVEH,			&_patches.max_roadveh,							0,240, 10, NULL},
	{PE_UINT8,	0, STR_CONFIG_PATCHES_MAX_AIRCRAFT,			&_patches.max_aircraft,							0,240, 10, NULL},
	{PE_UINT8,	0, STR_CONFIG_PATCHES_MAX_SHIPS,				&_patches.max_ships,								0,240, 10, NULL},

	{PE_BOOL,		0, STR_CONFIG_PATCHES_SERVINT_ISPERCENT,&_patches.servint_ispercent,				0,  0,  0, &CheckInterval},
	{PE_UINT16, PF_0ISDIS, STR_CONFIG_PATCHES_SERVINT_TRAINS,		&_patches.servint_trains,		5,800,  5, &InValidateDetailsWindow},
	{PE_UINT16, PF_0ISDIS, STR_CONFIG_PATCHES_SERVINT_ROADVEH,	&_patches.servint_roadveh,	5,800,  5, &InValidateDetailsWindow},
	{PE_UINT16, PF_0ISDIS, STR_CONFIG_PATCHES_SERVINT_AIRCRAFT, &_patches.servint_aircraft, 5,800,  5, &InValidateDetailsWindow},
	{PE_UINT16, PF_0ISDIS, STR_CONFIG_PATCHES_SERVINT_SHIPS,		&_patches.servint_ships,		5,800,  5, &InValidateDetailsWindow},
};

static const PatchEntry _patches_stations[] = {
	{PE_BOOL,		0, STR_CONFIG_PATCHES_JOINSTATIONS,			&_patches.join_stations,						0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_FULLLOADANY,			&_patches.full_load_any,						0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_IMPROVEDLOAD,			&_patches.improved_load,						0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_SELECTGOODS,			&_patches.selectgoods,							0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_NEW_NONSTOP,			&_patches.new_nonstop,							0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_NONUNIFORM_STATIONS, &_patches.nonuniform_stations,		0,  0,  0, NULL},
	{PE_UINT8,	0, STR_CONFIG_PATCHES_STATION_SPREAD,		&_patches.station_spread,						4, 64,  1, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_SERVICEATHELIPAD, &_patches.serviceathelipad,					0,  0,  0, NULL},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_CATCHMENT, &_patches.modified_catchment},

};

static const PatchEntry _patches_economy[] = {
	{PE_BOOL,		0, STR_CONFIG_PATCHES_INFLATION,				&_patches.inflation,								0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_BUILDXTRAIND,			&_patches.build_rawmaterial_ind,		0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_MULTIPINDTOWN,		&_patches.multiple_industry_per_town,0, 0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_SAMEINDCLOSE,			&_patches.same_industry_close,			0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_BRIBE,						&_patches.bribe,										0,  0,  0, NULL},
	{PE_UINT8,	0, STR_CONFIG_PATCHES_SNOWLINE_HEIGHT,	&_patches.snow_line_height,					2, 13,  1, NULL},

	{PE_INT32,	PF_NOCOMMA, STR_CONFIG_PATCHES_COLORED_NEWS_DATE, &_patches.colored_news_date, 1900, 2200, 5, NULL},
	{PE_INT32,	PF_NOCOMMA, STR_CONFIG_PATCHES_STARTING_DATE, &_patches.starting_date,	 1920,2100, 1, NULL},

	{PE_BOOL,		0, STR_CONFIG_PATCHES_SMOOTH_ECONOMY,		&_patches.smooth_economy,						0,  0,  0, NULL},
};

static const PatchEntry _patches_ai[] = {
	{PE_BOOL,		0, STR_CONFIG_PATCHES_AINEW_ACTIVE, &_patches.ainew_active, 0, 1, 1, &AiNew_PatchActive_Warning},

	{PE_BOOL,		0, STR_CONFIG_PATCHES_AI_BUILDS_TRAINS, &_patches.ai_disable_veh_train,			0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_AI_BUILDS_ROADVEH,&_patches.ai_disable_veh_roadveh,		0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_AI_BUILDS_AIRCRAFT, &_patches.ai_disable_veh_aircraft,0,  0,  0, NULL},
	{PE_BOOL,		0, STR_CONFIG_PATCHES_AI_BUILDS_SHIPS,	&_patches.ai_disable_veh_ship,			0,  0,  0, NULL},
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

extern uint GetCurrentCurrencyRate();

static int32 ReadPE(const PatchEntry*pe)
{
	switch(pe->type) {
	case PE_BOOL:   return *(bool*)pe->variable;
	case PE_UINT8:  return *(uint8*)pe->variable;
	case PE_INT16:  return *(int16*)pe->variable;
	case PE_UINT16: return *(uint16*)pe->variable;
	case PE_INT32:  return *(int32*)pe->variable;
	case PE_CURRENCY:  return (*(int64*)pe->variable) * GetCurrentCurrencyRate();
	default:
		NOT_REACHED();
	}

	/* useless, but avoids compiler warning this way */
	return 0;
}

static void WritePE(const PatchEntry *pe, int32 val)
{

	if ((pe->flags & PF_0ISDIS) && val <= 0) {
		*(bool*)pe->variable = 0; // "clamp" 'disabled' value to smallest type, PE_BOOL
		return;
	}

	switch(pe->type) {
	case PE_BOOL: *(bool*)pe->variable = (bool)val; break;

	case PE_UINT8: if ((uint8)val > (uint8)pe->max)
									*(uint8*)pe->variable = (uint8)pe->max;
								else if ((uint8)val < (uint8)pe->min)
									*(uint8*)pe->variable = (uint8)pe->min;
								else
									*(uint8*)pe->variable = (uint8)val;
								break;

	case PE_INT16: if ((int16)val > (int16)pe->max)
									*(int16*)pe->variable = (int16)pe->max;
								else if ((int16)val < (int16)pe->min)
									*(int16*)pe->variable = (int16)pe->min;
								else
									*(int16*)pe->variable = (int16)val;
								break;

	case PE_UINT16: if ((uint16)val > (uint16)pe->max)
									*(uint16*)pe->variable = (uint16)pe->max;
								else if ((uint16)val < (uint16)pe->min)
									*(uint16*)pe->variable = (uint16)pe->min;
								else
									*(uint16*)pe->variable = (uint16)val;
								break;

	case PE_INT32: if ((int32)val > (int32)pe->max)
									*(int32*)pe->variable = (int32)pe->max;
								else if ((int32)val < (int32)pe->min)
									*(int32*)pe->variable = (int32)pe->min;
								else
									*(int32*)pe->variable = val;
								break;

	case PE_CURRENCY: if ((int64)val > (int64)pe->max)
									*(int64*)pe->variable = (int64)pe->max;
								else if ((int64)val < (int64)pe->min)
									*(int64*)pe->variable = (int64)pe->min;
								else
									*(int64*)pe->variable = val;
								break;
	default:
		NOT_REACHED();
	}
}

static void PatchesSelectionWndProc(Window *w, WindowEvent *e)
{
	uint i;
	switch(e->event) {
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
		for(i=0,pe=page->entries; i!=page->num; i++,pe++) {
			bool disabled = false;
			bool editable = true;
			// We do not allow changes of some items when we are a client in a networkgame
			if (!(pe->flags & PF_PLAYERBASED) && _networking && !_network_server)
				editable = false;
			if (pe->type == PE_BOOL) {
				if (editable)
					DrawFrameRect(x+5, y+1, x+15+9, y+9, (*(bool*)pe->variable)?6:4, (*(bool*)pe->variable)?0x20:0);
				else
					DrawFrameRect(x+5, y+1, x+15+9, y+9, (*(bool*)pe->variable)?7:9, (*(bool*)pe->variable)?0x20:0);
				SetDParam(0, *(bool*)pe->variable ? STR_CONFIG_PATCHES_ON : STR_CONFIG_PATCHES_OFF);
			} else {
				DrawFrameRect(x+5, y+1, x+5+9, y+9, 3, clk == i*2+1 ? 0x20 : 0);
				DrawFrameRect(x+15, y+1, x+15+9, y+9, 3, clk == i*2+2 ? 0x20 : 0);
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

			if (!(pe->flags & PF_PLAYERBASED) && _networking && !_network_server)
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

int32 CmdChangePatchSetting(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	const PatchPage *page;
	const PatchEntry *pe;

	if (flags & DC_EXEC) {
		page = &_patches_page[(byte)p1];
		if (page == NULL) return 0;
		pe = &page->entries[(byte)(p1 >> 8)];
		if (pe == NULL) return 0;

		WritePE(pe, (int32)p2);

		InvalidateWindow(WC_GAME_OPTIONS, 0);
	}

	return 0;
}

static const Widget _patches_selection_widgets[] = {
{   WWT_CLOSEBOX,    10,     0,    10,     0,    13, STR_00C5,												STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    10,    11,   369,     0,    13, STR_CONFIG_PATCHES_CAPTION,			STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,    10,     0,   369,    14,    41, 0x0,															STR_NULL},
{      WWT_PANEL,    10,     0,   369,    42,   320, 0x0,															STR_NULL},

{   WWT_CLOSEBOX,     3,    10,    96,    16,    27, STR_CONFIG_PATCHES_GUI,					STR_NULL},
{   WWT_CLOSEBOX,     3,    97,   183,    16,    27, STR_CONFIG_PATCHES_CONSTRUCTION,	STR_NULL},
{   WWT_CLOSEBOX,     3,   184,   270,    16,    27, STR_CONFIG_PATCHES_VEHICLES,			STR_NULL},
{   WWT_CLOSEBOX,     3,   271,   357,    16,    27, STR_CONFIG_PATCHES_STATIONS,			STR_NULL},
{   WWT_CLOSEBOX,     3,    10,    96,    28,    39, STR_CONFIG_PATCHES_ECONOMY,			STR_NULL},
{   WWT_CLOSEBOX,     3,    97,   183,    28,    39, STR_CONFIG_PATCHES_AI,						STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _patches_selection_desc = {
	WDP_CENTER, WDP_CENTER, 370, 321,
	WC_GAME_OPTIONS,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_patches_selection_widgets,
	PatchesSelectionWndProc,
};

void ShowPatchesSelection()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	AllocateWindowDesc(&_patches_selection_desc);
}

struct GRFFile *_sel_grffile;

enum {
	NEWGRF_WND_PROC_OFFSET_TOP_WIDGET = 14,
	NEWGRF_WND_PROC_ROWSIZE = 14
};

static void NewgrfWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		int x, y = NEWGRF_WND_PROC_OFFSET_TOP_WIDGET;
		uint16 i = 0;
		struct GRFFile *c = _first_grffile;

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
				DrawSprite(SPRITE_PALETTE(SPR_SQUARE | 0x30b8000), 5, y + 2);
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
			snprintf(_userstring, USERSTRING_LEN, "%08X", _sel_grffile->grfid);
			DrawString(x + 2, 209, STR_SPEC_USERSTRING, 0x01);
		}
	} break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 2: { // select a grf file
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
{   WWT_CLOSEBOX,    14,     0,    10,     0,    13, STR_00C5,										STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   279,     0,    13, STR_NEWGRF_SETTINGS_CAPTION,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX,    14,     0,   268,    14,   182, 0xC01,/*small rows*/					STR_NEWGRF_TIP},
{      WWT_PANEL,    14,     0,   279,   183,   276, 0x0,													STR_NULL},

{  WWT_SCROLLBAR,    14,   269,   279,    14,   182, 0x0,													STR_0190_SCROLL_BAR_SCROLLS_LIST},

{   WWT_CLOSEBOX,    14,   147,   158,   244,   255, STR_0188,	STR_NULL},
{   WWT_CLOSEBOX,    14,   159,   170,   244,   255, STR_0189,	STR_NULL},
{   WWT_CLOSEBOX,    14,   175,   274,   244,   255, STR_NEWGRF_SET_PARAMETERS,		STR_NULL},

{   WWT_CLOSEBOX,     3,     5,   138,   261,   272, STR_NEWGRF_APPLY_CHANGES,		STR_NULL},
{   WWT_CLOSEBOX,     3,   142,   274,   261,   272, STR_012E_CANCEL,							STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _newgrf_desc = {
	WDP_CENTER, WDP_CENTER, 280, 277,
	WC_GAME_OPTIONS,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_newgrf_widgets,
	NewgrfWndProc,
};

void ShowNewgrf()
{
	Window *w;
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	w = AllocateWindowDesc(&_newgrf_desc);

	{ // little helper function to calculate _grffile_count
	  // should be REMOVED once _grffile_count is calculated at loading
		struct GRFFile *c = _first_grffile;
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

