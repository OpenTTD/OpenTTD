/* $Id$ */

/** @file cheat_gui.cpp GUI related to cheating. */

#include "stdafx.h"
#include "command_func.h"
#include "cheat_type.h"
#include "company_base.h"
#include "company_func.h"
#include "gfx_func.h"
#include "date_func.h"
#include "saveload/saveload.h"
#include "window_gui.h"
#include "newgrf.h"
#include "settings_type.h"
#include "strings_func.h"
#include "window_func.h"
#include "rail_gui.h"
#include "gui.h"
#include "company_gui.h"
#include "gamelog.h"

#include "table/strings.h"
#include "table/sprites.h"


/**
 * The 'amount' to cheat with.
 * This variable is semantically a constant value, but because the cheat
 * code requires to be able to write to the variable it is not constified.
 */
static int32 _money_cheat_amount = 10000000;

static int32 ClickMoneyCheat(int32 p1, int32 p2)
{
	DoCommandP(0, (uint32)(p2 * _money_cheat_amount), 0, CMD_MONEY_CHEAT);
	return _money_cheat_amount;
}

/**
 * @param p1 company to set to
 * @param p2 is -1 or +1 (down/up)
 */
static int32 ClickChangeCompanyCheat(int32 p1, int32 p2)
{
	while ((uint)p1 < GetCompanyPoolSize()) {
		if (IsValidCompanyID((CompanyID)p1)) {
			SetLocalCompany((CompanyID)p1);
			return _local_company;
		}
		p1 += p2;
	}

	return _local_company;
}

/**
 * @param p1 new value
 * @param p2 unused
 */
static int32 ClickSetProdCheat(int32 p1, int32 p2)
{
	InvalidateWindowClasses(WC_INDUSTRY_VIEW);
	return p1;
}

/**
 * @param p1 new climate
 * @param p2 unused
 */
static int32 ClickChangeClimateCheat(int32 p1, int32 p2)
{
	if (p1 == -1) p1 = 3;
	if (p1 ==  4) p1 = 0;
	_settings_game.game_creation.landscape = p1;

	GamelogStartAction(GLAT_CHEAT);
	GamelogTestMode();
	ReloadNewGRFData();
	GamelogStopAction();

	return _settings_game.game_creation.landscape;
}

extern void EnginesMonthlyLoop();

/**
 * @param p1 unused
 * @param p2 1 (increase) or -1 (decrease)
 */
static int32 ClickChangeDateCheat(int32 p1, int32 p2)
{
	YearMonthDay ymd;
	ConvertDateToYMD(_date, &ymd);

	if ((ymd.year == MIN_YEAR && p2 == -1) || (ymd.year == MAX_YEAR && p2 == 1)) return _cur_year;

	SetDate(ConvertYMDToDate(_cur_year + p2, ymd.month, ymd.day));
	EnginesMonthlyLoop();
	SetWindowDirty(FindWindowById(WC_STATUS_BAR, 0));
	ResetSignalVariant();
	return _cur_year;
}

typedef int32 CheckButtonClick(int32, int32);

struct CheatEntry {
	VarType type;          ///< type of selector
	StringID str;          ///< string with descriptive text
	void *variable;        ///< pointer to the variable
	bool *been_used;       ///< has this cheat been used before?
	CheckButtonClick *proc;///< procedure
};

static const CheatEntry _cheats_ui[] = {
	{SLE_INT32, STR_CHEAT_MONEY,           &_money_cheat_amount,                    &_cheats.money.been_used,            &ClickMoneyCheat         },
	{SLE_UINT8, STR_CHEAT_CHANGE_COMPANY,  &_local_company,                         &_cheats.switch_company.been_used,   &ClickChangeCompanyCheat },
	{SLE_BOOL,  STR_CHEAT_EXTRA_DYNAMITE,  &_cheats.magic_bulldozer.value,          &_cheats.magic_bulldozer.been_used,  NULL                     },
	{SLE_BOOL,  STR_CHEAT_CROSSINGTUNNELS, &_cheats.crossing_tunnels.value,         &_cheats.crossing_tunnels.been_used, NULL                     },
	{SLE_BOOL,  STR_CHEAT_BUILD_IN_PAUSE,  &_cheats.build_in_pause.value,           &_cheats.build_in_pause.been_used,   NULL                     },
	{SLE_BOOL,  STR_CHEAT_NO_JETCRASH,     &_cheats.no_jetcrash.value,              &_cheats.no_jetcrash.been_used,      NULL                     },
	{SLE_BOOL,  STR_CHEAT_SETUP_PROD,      &_cheats.setup_prod.value,               &_cheats.setup_prod.been_used,       &ClickSetProdCheat       },
	{SLE_UINT8, STR_CHEAT_SWITCH_CLIMATE,  &_settings_game.game_creation.landscape, &_cheats.switch_climate.been_used,   &ClickChangeClimateCheat },
	{SLE_INT32, STR_CHEAT_CHANGE_DATE,     &_cur_year,                              &_cheats.change_date.been_used,      &ClickChangeDateCheat    },
};


static const Widget _cheat_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,   STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   399,     0,    13, STR_CHEATS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   399,    14,   169, 0x0,        STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   399,    14,   169, 0x0,        STR_CHEATS_TIP},
{   WIDGETS_END},
};

struct CheatWindow : Window {
	int clicked;

	CheatWindow(const WindowDesc *desc) : Window(desc)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
		DrawStringMultiCenter(200, 25, STR_CHEATS_WARNING, width - 50);

		for (int i = 0, x = 0, y = 45; i != lengthof(_cheats_ui); i++) {
			const CheatEntry *ce = &_cheats_ui[i];

			DrawSprite((*ce->been_used) ? SPR_BOX_CHECKED : SPR_BOX_EMPTY, PAL_NONE, x + 5, y + 2);

			switch (ce->type) {
				case SLE_BOOL: {
					bool on = (*(bool*)ce->variable);

					DrawFrameRect(x + 20, y + 1, x + 30 + 9, y + 9, on ? COLOUR_GREEN : COLOUR_RED, on ? FR_LOWERED : FR_NONE);
					SetDParam(0, on ? STR_CONFIG_SETTING_ON : STR_CONFIG_SETTING_OFF);
				} break;

				default: {
					int32 val = (int32)ReadValue(ce->variable, ce->type);
					char buf[512];

					/* Draw [<][>] boxes for settings of an integer-type */
					DrawArrowButtons(x + 20, y, COLOUR_YELLOW, clicked - (i * 2), true, true);

					switch (ce->str) {
						/* Display date for change date cheat */
						case STR_CHEAT_CHANGE_DATE: SetDParam(0, _date); break;

						/* Draw coloured flag for change company cheat */
						case STR_CHEAT_CHANGE_COMPANY:
							SetDParam(0, val + 1);
							GetString(buf, STR_CHEAT_CHANGE_COMPANY, lastof(buf));
							DrawCompanyIcon(_local_company, 60 + GetStringBoundingBox(buf).width, y + 2);
							break;

						/* Set correct string for switch climate cheat */
						case STR_CHEAT_SWITCH_CLIMATE: val += STR_TEMPERATE_LANDSCAPE;

						/* Fallthrough */
						default: SetDParam(0, val);
					}
				} break;
			}

			DrawString(50, y + 1, ce->str, TC_FROMSTRING);

			y += 12;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		uint btn = (pt.y - 46) / 12;
		uint x = pt.x;

		/* Not clicking a button? */
		if (!IsInsideMM(x, 20, 40) || btn >= lengthof(_cheats_ui)) return;

		const CheatEntry *ce = &_cheats_ui[btn];
		int value = (int32)ReadValue(ce->variable, ce->type);
		int oldvalue = value;

		*ce->been_used = true;

		switch (ce->type) {
			case SLE_BOOL:
				value ^= 1;
				if (ce->proc != NULL) ce->proc(value, 0);
				break;

			default:
				/* Take whatever the function returns */
				value = ce->proc(value + ((x >= 30) ? 1 : -1), (x >= 30) ? 1 : -1);

				/* The first cheat (money), doesn't return a different value. */
				if (value != oldvalue || btn == 0) this->clicked = btn * 2 + 1 + ((x >= 30) ? 1 : 0);
				break;
		}

		if (value != oldvalue) WriteValue(ce->variable, ce->type, (int64)value);

		this->flags4 |= WF_TIMEOUT_BEGIN;

		SetDirty();
	}

	virtual void OnTimeout()
	{
		this->clicked = 0;
		this->SetDirty();
	}
};

static const WindowDesc _cheats_desc(
	240, 22, 400, 170, 400, 170,
	WC_CHEATS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_cheat_widgets
);


void ShowCheatWindow()
{
	DeleteWindowById(WC_CHEATS, 0);
	new CheatWindow(&_cheats_desc);
}
