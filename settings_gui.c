#include "stdafx.h"
#include "ttd.h"

#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "command.h"
#include "engine.h"

static uint32 _dropdown_disabled;
static const StringID *_dropdown_items;
static int _dropdown_selindex;
static uint _dropdown_item_count;
static byte _dropdown_button;
static WindowClass _dropdown_windowclass;
static WindowNumber _dropdown_windownum;
static byte _dropdown_var1;
static byte _dropdown_var2;

static uint32 _difficulty_click_a;
static uint32 _difficulty_click_b;
static byte _difficulty_timeout;

static Widget _dropdown_menu_widgets[] = {
{     WWT_IMGBTN,     0,     0, 0,     0, 0, 0x0},
{      WWT_LAST},
};

static int GetDropdownItem(Window *w)
{
	uint item;
	int y;

	if (GetWidgetFromPos(w, _cursor.pos.x - w->left, _cursor.pos.y - w->top) < 0)
		return -1;
	
	y = _cursor.pos.y - w->top - 2;

	if (y < 0)
		return - 1;

	item = y / 10;
	if (item >= _dropdown_item_count || HASBIT(_dropdown_disabled,item) || _dropdown_items[item] == 0)
		return - 1;

	return item;
}

void DropdownMenuWndProc(Window *w, WindowEvent *e)
{
	int item;

	switch(e->event) {
	case WE_PAINT: {
		int x,y,i,sel;
		uint32 dis;

		DrawWindowWidgets(w);

		x = 1;
		y = 2;
		sel = _dropdown_selindex;
		dis = _dropdown_disabled;

		for(i=0; _dropdown_items[i] != INVALID_STRING_ID; i++) {
			if (_dropdown_items[i] != 0) {
				if (sel == 0) {
					GfxFillRect(x+1, y, x+w->width-4, y + 9, 0);
				}
				DrawString(x+2, y, _dropdown_items[i], sel==0 ? 12 : 16);

				if (dis & 1) {
					GfxFillRect(x, y, x+w->width-3, y + 9, 0x8000 + 
						_color_list[_dropdown_menu_widgets[0].color].window_color_bga);
				}
			} else {
				int color_1 = _color_list[_dropdown_menu_widgets[0].color].window_color_1a;
				int color_2 = _color_list[_dropdown_menu_widgets[0].color].window_color_2;
				GfxFillRect(x+1, y+3, x+w->width-5, y+3, color_1);
				GfxFillRect(x+1, y+4, x+w->width-5, y+4, color_2);
			}
			y += 10;
			sel--;
			dis>>=1;
		}
	} break;

	case WE_CLICK: {
		item = GetDropdownItem(w);
		if (item >= 0) {
			_dropdown_var1 = 4;
			_dropdown_selindex = item;
			SetWindowDirty(w);
		}
	} break;

	case WE_MOUSELOOP: {
		Window *w2 = FindWindowById(_dropdown_windowclass, _dropdown_windownum);
		if (w2 == NULL) {
			DeleteWindow(w);
			return;
		}

		if (_dropdown_var1 != 0 && --_dropdown_var1 == 0) {
			WindowEvent e;
			e.event = WE_DROPDOWN_SELECT;
			e.dropdown.button = _dropdown_button;
			e.dropdown.index = _dropdown_selindex;
			w2->wndproc(w2, &e);
			DeleteWindow(w);
			return;
		}

		if (_dropdown_var2 != 0) {
			item = GetDropdownItem(w);

			if (!_left_button_clicked) {
				_dropdown_var2 = 0;
				if (item < 0)
					return;
				_dropdown_var1 = 2;
			} else {
				if (item < 0)
					return;
			}

			_dropdown_selindex = item;
			SetWindowDirty(w);
		}
	} break;
		
	case WE_DESTROY: {
		Window *w2 = FindWindowById(_dropdown_windowclass, _dropdown_windownum);
		if (w2 != NULL) {
			CLRBIT(w2->click_state, _dropdown_button);
			InvalidateWidget(w2, _dropdown_button);
		}
	} break;
	}
}

void ShowDropDownMenu(Window *w, const StringID *strings, int selected, int button, uint32 disabled_mask)
{
	WindowNumber num;
	WindowClass cls;
	int i,t1,t2;
	const Widget *wi;
	Window *w2;
	uint32 old_click_state = w->click_state;
	
	_dropdown_disabled = disabled_mask;

	cls = w->window_class;
	num = w->window_number;
	DeleteWindowById(WC_DROPDOWN_MENU, 0);
	w = FindWindowById(cls, num);

	if (HASBIT(old_click_state, button))
		return;

	SETBIT(w->click_state, button);

	InvalidateWidget(w, button);
	
	for(i=0;strings[i] != INVALID_STRING_ID;i++);
	if (i == 0)
		return;

	_dropdown_items = strings;
	_dropdown_item_count = i;
	_dropdown_selindex = selected;
	
	_dropdown_windowclass = w->window_class;
	_dropdown_windownum = w->window_number;
	_dropdown_button = button;
	
	_dropdown_var1 = 0;
	_dropdown_var2 = 1;

	wi = &w->widget[button];

	_dropdown_menu_widgets[0].color = wi->color;

	w2 = AllocateWindow(
		w->left + wi[-1].left + 1,
		w->top + wi->bottom + 2,
		(_dropdown_menu_widgets[0].right=t1=wi->right - wi[-1].left, t1 + 1), 
		(_dropdown_menu_widgets[0].bottom=t2=i*10+3, t2+1), 
		DropdownMenuWndProc,
		0x3F,
		_dropdown_menu_widgets);


	w2->flags4 &= ~WF_WHITE_BORDER_MASK;
}

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
	for(i=0; i!=_num_resolutions; i++)
		if (_resolutions[i][0] == _cur_resolution[0] && _resolutions[i][1] == _cur_resolution[1])
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
		SET_DPARAM16(0, str);
		SET_DPARAM16(1, _currency_string_list[_opt_mod_ptr->currency]);
		SET_DPARAM16(2, _opt_mod_ptr->kilometers + STR_0139_IMPERIAL_MILES);
		SET_DPARAM16(3, STR_02E9_DRIVE_ON_LEFT + _opt_mod_ptr->road_side);
		SET_DPARAM16(4, STR_TOWNNAME_ENGLISH + _opt_mod_ptr->town_name);
		SET_DPARAM16(5, _autosave_dropdown[_opt_mod_ptr->autosave]);
		SET_DPARAM16(6, SPECSTR_LANGUAGE_START + _dynlang.curr);
		i = GetCurRes();
		SET_DPARAM16(7, i == _num_resolutions ? STR_RES_OTHER : SPECSTR_RESOLUTION_START + i);
		SET_DPARAM16(8, SPECSTR_SCREENSHOT_START + _cur_screenshot_format);
		DrawWindowWidgets(w);
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
		case 30:
			// setup screenshot format dropdown
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
		case 30:
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
{   WWT_CLOSEBOX,    14,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   369,     0,    13, STR_00B1_GAME_OPTIONS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,    14,     0,   369,    14,   233, 0x0},
{      WWT_FRAME,    14,    10,   179,    20,    55, STR_02E0_CURRENCY_UNITS},
{          WWT_6,    14,    20,   169,    34,    45, STR_02E1, STR_02E2_CURRENCY_UNITS_SELECTION},
{   WWT_CLOSEBOX,    14,   158,   168,    35,    44, STR_0225, STR_02E2_CURRENCY_UNITS_SELECTION},
{      WWT_FRAME,    14,   190,   359,    20,    55, STR_02E3_DISTANCE_UNITS},
{          WWT_6,    14,   200,   349,    34,    45, STR_02E4, STR_02E5_DISTANCE_UNITS_SELECTION},
{   WWT_CLOSEBOX,    14,   338,   348,    35,    44, STR_0225, STR_02E5_DISTANCE_UNITS_SELECTION},
{      WWT_FRAME,    14,    10,   179,    62,    97, STR_02E6_ROAD_VEHICLES},
{          WWT_6,    14,    20,   169,    76,    87, STR_02E7, STR_02E8_SELECT_SIDE_OF_ROAD_FOR},
{   WWT_CLOSEBOX,    14,   158,   168,    77,    86, STR_0225, STR_02E8_SELECT_SIDE_OF_ROAD_FOR},
{      WWT_FRAME,    14,   190,   359,    62,    97, STR_02EB_TOWN_NAMES},
{          WWT_6,    14,   200,   349,    76,    87, STR_02EC, STR_02ED_SELECT_STYLE_OF_TOWN_NAMES},
{   WWT_CLOSEBOX,    14,   338,   348,    77,    86, STR_0225, STR_02ED_SELECT_STYLE_OF_TOWN_NAMES},
{      WWT_FRAME,    14,    10,   179,   104,   139, STR_02F4_AUTOSAVE},
{          WWT_6,    14,    20,   169,   118,   129, STR_02F5, STR_02F6_SELECT_INTERVAL_BETWEEN},
{   WWT_CLOSEBOX,    14,   158,   168,   119,   128, STR_0225, STR_02F6_SELECT_INTERVAL_BETWEEN},

{      WWT_FRAME,    14,    10,   359,   188,   223, STR_02BC_VEHICLE_DESIGN_NAMES},
{          WWT_6,    14,    20,   119,   202,   213, STR_02BD, STR_02C1_VEHICLE_DESIGN_NAMES_SELECTION},
{   WWT_CLOSEBOX,    14,   108,   118,   203,   212, STR_0225, STR_02C1_VEHICLE_DESIGN_NAMES_SELECTION},
{   WWT_CLOSEBOX,    14,   130,   349,   202,   213, STR_02C0_SAVE_CUSTOM_NAMES_TO_DISK, STR_02C2_SAVE_CUSTOMIZED_VEHICLE},

{      WWT_FRAME,    14,   190,   359,   104,   139, STR_OPTIONS_LANG},
{          WWT_6,    14,   200,   349,   118,   129, STR_OPTIONS_LANG_CBO, STR_OPTIONS_LANG_TIP}, 
{   WWT_CLOSEBOX,    14,   338,   348,   119,   128, STR_0225, STR_OPTIONS_LANG_TIP},
{      WWT_FRAME,    14,    10,   179,   146,   181, STR_OPTIONS_RES},
{          WWT_6,    14,    20,   169,   160,   171, STR_OPTIONS_RES_CBO, STR_OPTIONS_RES_TIP},
{   WWT_CLOSEBOX,    14,   158,   168,   161,   170, STR_0225, STR_OPTIONS_RES_TIP},

{      WWT_FRAME,    14,   190,   359,   146,   181, STR_OPTIONS_SCREENSHOT_FORMAT},
{          WWT_6,    14,   200,   349,   160,   171, STR_OPTIONS_SCREENSHOT_FORMAT_CBO, STR_OPTIONS_SCREENSHOT_FORMAT_TIP},
{   WWT_CLOSEBOX,    14,   338,   348,   161,   170, STR_0225, STR_OPTIONS_SCREENSHOT_FORMAT_TIP},

{      WWT_LAST},
};

static const WindowDesc _game_options_desc = {
	WDP_CENTER, WDP_CENTER, 370, 234,
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
	{0,2,1,STR_6816_LOW},
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

static bool FORCEINLINE GetBitAndShift(uint32 *b)
{
	uint32 x = *b;
	*b >>= 1;
	return (x&1) != 0;
}

static GameOptions _opt_mod_temp;

static const int16 _default_game_diff[3][GAME_DIFFICULTY_NUM] = {
	{2, 2, 1, 2, 300, 2, 0, 2, 0, 1, 2, 0, 1, 0, 0, 0, 0, 0},
	{4, 1, 1, 1, 150, 3, 1, 3, 1, 2, 1, 1, 2, 1, 1, 1, 1, 1},
	{7, 0, 2, 1, 100, 4, 1, 3, 2, 2, 0, 2, 3, 2, 1, 1, 1, 2},
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

static void GameDifficultyWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		uint32 click_a, click_b, disabled;
		int i;
		int x,y,value;

		w->click_state = (1 << 4) << _opt_mod_temp.diff_level;
		w->disabled_state = (_game_mode != GM_NORMAL) ? 0 : (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7);
		DrawWindowWidgets(w);

		click_a = _difficulty_click_a;
		click_b = _difficulty_click_b;

		disabled = _game_mode == GM_NORMAL ? 0x383E : 0;
		// XXX

		x = 0;
		y = 32;
		for (i = 0; i != GAME_DIFFICULTY_NUM; i++) {
			DrawFrameRect(x+5, y+1, x+5+9, y+9, 3, GetBitAndShift(&click_a)?0x20:0);
			DrawFrameRect(x+15, y+1, x+15+9, y+9, 3, GetBitAndShift(&click_b)?0x20:0);
			if (GetBitAndShift(&disabled)) {
				int color = 0x8000 | _color_list[3].unk2;
				GfxFillRect(x+6, y+2, x+6+8, y+9, color);
				GfxFillRect(x+16, y+2, x+16+8, y+9, color);
			}

			DrawStringCentered(x+10, y+1, STR_6819, 0);
			DrawStringCentered(x+20, y+1, STR_681A, 0);


			value = _game_setting_info[i].str + ((int*)&_opt_mod_temp.diff)[i];
			if (i == 4) value *= 1000; // handle currency option
			SET_DPARAM32(0, value);
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
{   WWT_CLOSEBOX,    10,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    10,    11,   369,     0,    13, STR_6800_DIFFICULTY_LEVEL, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,    10,     0,   369,    14,    29, 0x0, 0},
{      WWT_PANEL,    10,     0,   369,    30,   276, 0x0, 0},
{ WWT_PUSHTXTBTN,     3,    10,    96,    16,    27, STR_6801_EASY, 0},
{ WWT_PUSHTXTBTN,     3,    97,   183,    16,    27, STR_6802_MEDIUM, 0},
{ WWT_PUSHTXTBTN,     3,   184,   270,    16,    27, STR_6803_HARD, 0},
{ WWT_PUSHTXTBTN,     3,   271,   357,    16,    27, STR_6804_CUSTOM, 0},
{      WWT_EMPTY,       10,     0,   369,   251,   262, 0x0, 0},
//{   WWT_CLOSEBOX,    10,     0,   369,   251,   262, STR_6838_SHOW_HI_SCORE_CHART, 0},
{      WWT_PANEL,    10,     0,   369,   263,   278, 0x0, 0},
{ WWT_PUSHTXTBTN,     3,   105,   185,   265,   276, STR_OPTIONS_SAVE_CHANGES, 0},
{ WWT_PUSHTXTBTN,     3,   186,   266,   265,   276, STR_012E_CANCEL, 0},
{      WWT_LAST},
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

typedef struct PatchEntry {
	byte type;    // type of selector
	byte flags;		// selector flags
	StringID str; // string with descriptive text
	void *variable; // pointer to the variable
	int16 min,max; // range for spinbox setting
	uint16 step;   // step for spinbox
} PatchEntry;

enum {
	PE_BOOL = 0,
	PE_UINT8 = 1,
	PE_INT16 = 2,
	PE_UINT16 = 3,
	PE_INT32 = 4,

	PF_0ISDIS = 1,
	PF_NOCOMMA = 2,
};

static const PatchEntry _patches_ui[] = {
	{PE_BOOL, 0, STR_CONFIG_PATCHES_VEHICLESPEED, &_patches.vehicle_speed},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_LONGDATE, &_patches.status_long_date},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_SHOWFINANCES, &_patches.show_finances},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_AUTOSCROLL, &_patches.autoscroll},

	{PE_UINT8, 0, STR_CONFIG_PATCHES_ERRMSG_DURATION, &_patches.errmsg_duration, 0, 20, 1},
	
};

static const PatchEntry _patches_construction[] = {
	{PE_BOOL, 0, STR_CONFIG_PATCHES_BUILDONSLOPES, &_patches.build_on_slopes},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_EXTRADYNAMITE, &_patches.extra_dynamite},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_LONGBRIDGES, &_patches.longbridges},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_SIGNALSIDE, &_patches.signal_side},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_BUILD_IN_PAUSE, &_patches.build_in_pause},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_SMALL_AIRPORTS, &_patches.always_small_airport},

};

static const PatchEntry _patches_vehicles[] = {
	{PE_BOOL, 0, STR_CONFIG_PATCHES_REALISTICACCEL, &_patches.realistic_acceleration},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_MAMMOTHTRAINS, &_patches.mammoth_trains},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_GOTODEPOT, &_patches.gotodepot},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_ROADVEH_QUEUE, &_patches.roadveh_queue},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_NEW_DEPOT_FINDING, &_patches.new_depot_finding},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_NEW_TRAIN_PATHFIND, &_patches.new_pathfinding},

	{PE_BOOL, 0, STR_CONFIG_PATCHES_WARN_INCOME_LESS, &_patches.train_income_warn},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_NEVER_EXPIRE_VEHICLES, &_patches.never_expire_vehicles},

	{PE_UINT16, PF_0ISDIS, STR_CONFIG_PATCHES_LOST_TRAIN_DAYS, &_patches.lost_train_days, 180, 720, 60},
	{PE_BOOL, 0, STR_CONFIG_AUTORENEW_VEHICLE, &_patches.autorenew},

	{PE_UINT8, 0, STR_CONFIG_PATCHES_MAX_TRAINS, &_patches.max_trains, 0, 240, 10},
	{PE_UINT8, 0, STR_CONFIG_PATCHES_MAX_ROADVEH, &_patches.max_roadveh, 0, 240, 10},
	{PE_UINT8, 0, STR_CONFIG_PATCHES_MAX_AIRCRAFT, &_patches.max_aircraft, 0, 240, 10},
	{PE_UINT8, 0, STR_CONFIG_PATCHES_MAX_SHIPS, &_patches.max_ships, 0, 240, 10},

	{PE_UINT16, PF_0ISDIS, STR_CONFIG_PATCHES_SERVINT_TRAINS, &_patches.servint_trains, 30, 1200, 10},
	{PE_UINT16, PF_0ISDIS, STR_CONFIG_PATCHES_SERVINT_ROADVEH, &_patches.servint_roadveh, 30, 1200, 10},
	{PE_UINT16, PF_0ISDIS, STR_CONFIG_PATCHES_SERVINT_AIRCRAFT, &_patches.servint_aircraft, 30, 1200, 10},
	{PE_UINT16, PF_0ISDIS, STR_CONFIG_PATCHES_SERVINT_SHIPS, &_patches.servint_ships, 30, 1200, 10},

	{PE_BOOL, 0, STR_CONFIG_PATCHES_AI_BUILDS_TRAINS, &_patches.ai_disable_veh_train},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_AI_BUILDS_ROADVEH, &_patches.ai_disable_veh_roadveh},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_AI_BUILDS_AIRCRAFT, &_patches.ai_disable_veh_aircraft},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_AI_BUILDS_SHIPS, &_patches.ai_disable_veh_ship},
};

static const PatchEntry _patches_stations[] = {
	{PE_BOOL, 0, STR_CONFIG_PATCHES_JOINSTATIONS, &_patches.join_stations},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_FULLLOADANY, &_patches.full_load_any},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_SELECTGOODS, &_patches.selectgoods},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_NEW_NONSTOP, &_patches.new_nonstop},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_NONUNIFORM_STATIONS, &_patches.nonuniform_stations},
	{PE_UINT8, 0, STR_CONFIG_PATCHES_STATION_SPREAD, &_patches.station_spread, 4, 64, 1},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_SERVICEATHELIPAD, &_patches.serviceathelipad},
};

static const PatchEntry _patches_economy[] = {
	{PE_BOOL, 0, STR_CONFIG_PATCHES_INFLATION, &_patches.inflation},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_BUILDXTRAIND, &_patches.build_rawmaterial_ind},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_MULTIPINDTOWN, &_patches.multiple_industry_per_town},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_SAMEINDCLOSE, &_patches.same_industry_close},
	{PE_BOOL, 0, STR_CONFIG_PATCHES_BRIBE, &_patches.bribe},
	{PE_UINT8, 0, STR_CONFIG_PATCHES_SNOWLINE_HEIGHT, &_patches.snow_line_height, 2, 13, 1},
	
	{PE_INT32, PF_NOCOMMA, STR_CONFIG_PATCHES_COLORED_NEWS_DATE, &_patches.colored_news_date, 1900, 2200, 5},
	{PE_INT32, PF_NOCOMMA, STR_CONFIG_PATCHES_STARTING_DATE, &_patches.starting_date, 1920, 2100, 1},

	{PE_BOOL, 0, STR_CONFIG_PATCHES_SMOOTH_ECONOMY, &_patches.smooth_economy},
};

typedef struct PatchPage {
	const PatchEntry *entries;
	uint num;
} PatchPage;

static const PatchPage _patches_page[] = {
	{_patches_ui, lengthof(_patches_ui) },
	{_patches_construction, lengthof(_patches_construction) },
	{_patches_vehicles, lengthof(_patches_vehicles) },
	{_patches_stations, lengthof(_patches_stations) },
	{_patches_economy, lengthof(_patches_economy) },
};

static int32 ReadPE(const PatchEntry*pe)
{
	switch(pe->type) {
	case PE_BOOL:   return *(bool*)pe->variable;
	case PE_UINT8:  return *(uint8*)pe->variable;
	case PE_INT16:  return *(int16*)pe->variable;
	case PE_UINT16: return *(uint16*)pe->variable;
	case PE_INT32:  return *(int32*)pe->variable;
	default:
		NOT_REACHED();
	}
  
	/* useless, but avoids compiler warning this way */
	return 0;
}

static void WritePE(const PatchEntry *pe, int32 val)
{
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
			if (pe->type == PE_BOOL) {
				DrawFrameRect(x+5, y+1, x+15+9, y+9, (*(bool*)pe->variable)?6:4, (*(bool*)pe->variable)?0x20:0);
				SET_DPARAM16(0, *(bool*)pe->variable ? STR_CONFIG_PATCHES_ON : STR_CONFIG_PATCHES_OFF);
			} else {
				DrawFrameRect(x+5, y+1, x+5+9, y+9, 3, clk == i*2+1 ? 0x20 : 0);
				DrawFrameRect(x+15, y+1, x+15+9, y+9, 3, clk == i*2+2 ? 0x20 : 0);
				DrawStringCentered(x+10, y+1, STR_6819, 0);
				DrawStringCentered(x+20, y+1, STR_681A, 0);

				val = ReadPE(pe);
				disabled = ((val == 0) && (pe->flags & PF_0ISDIS));
				if (disabled) {
					SET_DPARAM16(0, STR_CONFIG_PATCHES_DISABLED);
				} else {
					SET_DPARAM32(1, val);
					SET_DPARAM16(0, pe->flags & PF_NOCOMMA ? STR_CONFIG_PATCHES_INT32 : STR_7024);
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

			if (x < 21) {
				int32 val = ReadPE(pe), oval = val;

				switch(pe->type) {
				case PE_BOOL:
					val ^= 1;
					break;
				case PE_UINT8:
				case PE_INT16:
				case PE_UINT16:
				case PE_INT32:
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
						if (val <= pe->min && pe->flags & PF_0ISDIS) {
							val = 0;
						} else {
							// decrease
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
					WritePE(pe, val);
					SetWindowDirty(w);
				}
			} else {
				if (pe->type != PE_BOOL) {
					WP(w,def_d).data_3 = btn;
					SET_DPARAM32(0, ReadPE(pe));
					ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_CONFIG_PATCHES_QUERY_CAPT, 10, 100, WC_GAME_OPTIONS, 0);
				}
			}
			
			break;
		}
		case 4: case 5: case 6: case 7: case 8:
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
			WritePE(&page->entries[WP(w,def_d).data_3], atoi(e->edittext.str)); 
			SetWindowDirty(w);
		}
		break;
	}

	case WE_DESTROY:
		DeleteWindowById(WC_QUERY_STRING, 0);
		break;
	}
}

static const Widget _patches_selection_widgets[] = {
{   WWT_CLOSEBOX,    10,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    10,    11,   369,     0,    13, STR_CONFIG_PATCHES_CAPTION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,    10,     0,   369,    14,    41, 0x0, 0},
{      WWT_PANEL,    10,     0,   369,    42,   320, 0x0, 0},

{   WWT_CLOSEBOX,     3,    10,    96,    16,    27, STR_CONFIG_PATCHES_GUI, 0},
{   WWT_CLOSEBOX,     3,    97,   183,    16,    27, STR_CONFIG_PATCHES_CONSTRUCTION, 0},
{   WWT_CLOSEBOX,     3,   184,   270,    16,    27, STR_CONFIG_PATCHES_VEHICLES, 0},
{   WWT_CLOSEBOX,     3,   271,   357,    16,    27, STR_CONFIG_PATCHES_STATIONS, 0},
{   WWT_CLOSEBOX,     3,    10,    96,    28,    39, STR_CONFIG_PATCHES_ECONOMY, 0},
{      WWT_LAST},
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
