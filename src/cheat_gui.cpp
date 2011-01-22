/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cheat_gui.cpp GUI related to cheating. */

#include "stdafx.h"
#include "command_func.h"
#include "cheat_type.h"
#include "company_base.h"
#include "company_func.h"
#include "date_func.h"
#include "saveload/saveload.h"
#include "textbuf_gui.h"
#include "window_gui.h"
#include "newgrf.h"
#include "string_func.h"
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

/**
 * Handle cheating of money.
 * Note that the amount of money of a company must be changed through a command
 * rather than by setting a variable. Since the cheat data structure expects a
 * variable, the amount of given/taken money is used for this purpose.
 * @param p1 not used.
 * @param p2 is -1 or +1 (down/up)
 * @return Amount of money cheat.
 */
static int32 ClickMoneyCheat(int32 p1, int32 p2)
{
	DoCommandP(0, (uint32)(p2 * _money_cheat_amount), 0, CMD_MONEY_CHEAT);
	return _money_cheat_amount;
}

/**
 * Handle changing of company.
 * @param p1 company to set to
 * @param p2 is -1 or +1 (down/up)
 * @return The new company.
 */
static int32 ClickChangeCompanyCheat(int32 p1, int32 p2)
{
	while ((uint)p1 < Company::GetPoolSize()) {
		if (Company::IsValidID((CompanyID)p1)) {
			SetLocalCompany((CompanyID)p1);
			return _local_company;
		}
		p1 += p2;
	}

	return _local_company;
}

/**
 * Allow (or disallow) changing production of all industries.
 * @param p1 new value
 * @param p2 unused
 * @return New value allwing change of industry production.
 */
static int32 ClickSetProdCheat(int32 p1, int32 p2)
{
	_cheats.setup_prod.value = (p1 != 0);
	InvalidateWindowClassesData(WC_INDUSTRY_VIEW);
	return _cheats.setup_prod.value;
}

/**
 * Handle changing of climate.
 * @param p1 New climate.
 * @param p2 Unused.
 * @return New climate.
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
 * Handle changing of the current year.
 * @param p1 Unused.
 * @param p2 +1 (increase) or -1 (decrease).
 * @return New year.
 */
static int32 ClickChangeDateCheat(int32 p1, int32 p2)
{
	YearMonthDay ymd;
	ConvertDateToYMD(_date, &ymd);

	p1 = Clamp(p1, MIN_YEAR, MAX_YEAR);
	if (p1 == _cur_year) return _cur_year;

	SetDate(ConvertYMDToDate(p1, ymd.month, ymd.day), _date_fract);
	EnginesMonthlyLoop();
	SetWindowDirty(WC_STATUS_BAR, 0);
	InvalidateWindowClassesData(WC_BUILD_STATION, 0);
	ResetSignalVariant();
	return _cur_year;
}

/** Available cheats. */
enum CheatNumbers {
	CHT_MONEY,           ///< Change amount of money.
	CHT_CHANGE_COMPANY,  ///< Switch company.
	CHT_EXTRA_DYNAMITE,  ///< Dynamite anything.
	CHT_CROSSINGTUNNELS, ///< Allow tunnels to cross each other.
	CHT_NO_JETCRASH,     ///< Disable jet-airplane crashes.
	CHT_SETUP_PROD,      ///< Allow manually editing of industry production.
	CHT_SWITCH_CLIMATE,  ///< Switch climate.
	CHT_CHANGE_DATE,     ///< Do time traveling.

	CHT_NUM_CHEATS,      ///< Number of cheats.
};

/**
 * Signature of handler function when user clicks at a cheat.
 * @param p1 The new value.
 * @param p2 Change direction (+1, +1), \c 0 for boolean settings.
 */
typedef int32 CheckButtonClick(int32 p1, int32 p2);

/** Information of a cheat. */
struct CheatEntry {
	VarType type;          ///< type of selector
	StringID str;          ///< string with descriptive text
	void *variable;        ///< pointer to the variable
	bool *been_used;       ///< has this cheat been used before?
	CheckButtonClick *proc;///< procedure
};

/**
 * The available cheats.
 * Order matches with the values of #CheatNumbers
 */
static const CheatEntry _cheats_ui[] = {
	{SLE_INT32, STR_CHEAT_MONEY,           &_money_cheat_amount,                    &_cheats.money.been_used,            &ClickMoneyCheat         },
	{SLE_UINT8, STR_CHEAT_CHANGE_COMPANY,  &_local_company,                         &_cheats.switch_company.been_used,   &ClickChangeCompanyCheat },
	{SLE_BOOL,  STR_CHEAT_EXTRA_DYNAMITE,  &_cheats.magic_bulldozer.value,          &_cheats.magic_bulldozer.been_used,  NULL                     },
	{SLE_BOOL,  STR_CHEAT_CROSSINGTUNNELS, &_cheats.crossing_tunnels.value,         &_cheats.crossing_tunnels.been_used, NULL                     },
	{SLE_BOOL,  STR_CHEAT_NO_JETCRASH,     &_cheats.no_jetcrash.value,              &_cheats.no_jetcrash.been_used,      NULL                     },
	{SLE_BOOL,  STR_CHEAT_SETUP_PROD,      &_cheats.setup_prod.value,               &_cheats.setup_prod.been_used,       &ClickSetProdCheat       },
	{SLE_UINT8, STR_CHEAT_SWITCH_CLIMATE,  &_settings_game.game_creation.landscape, &_cheats.switch_climate.been_used,   &ClickChangeClimateCheat },
	{SLE_INT32, STR_CHEAT_CHANGE_DATE,     &_cur_year,                              &_cheats.change_date.been_used,      &ClickChangeDateCheat    },
};

assert_compile(CHT_NUM_CHEATS == lengthof(_cheats_ui));

/** Names of the cheat window widgets. */
enum CheatWidgets {
	CW_PANEL,
};

/** Widget definitions of the cheat GUI. */
static const NWidgetPart _nested_cheat_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_CHEATS, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, CW_PANEL), SetDataTip(0x0, STR_CHEATS_TOOLTIP), EndContainer(),
};

/** GUI for the cheats. */
struct CheatWindow : Window {
	int clicked;
	int header_height;

	CheatWindow(const WindowDesc *desc) : Window()
	{
		this->InitNested(desc);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != CW_PANEL) return;

		int y = r.top + WD_FRAMERECT_TOP + this->header_height;
		DrawStringMultiLine(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, r.top + WD_FRAMERECT_TOP, y, STR_CHEATS_WARNING, TC_FROMSTRING, SA_CENTER);

		bool rtl = _current_text_dir == TD_RTL;
		uint box_left    = rtl ? r.right - 12 : r.left + 5;
		uint button_left = rtl ? r.right - 40 : r.left + 20;
		uint text_left   = r.left + (rtl ? WD_FRAMERECT_LEFT: 50);
		uint text_right  = r.right - (rtl ? 50 : WD_FRAMERECT_RIGHT);

		for (int i = 0; i != lengthof(_cheats_ui); i++) {
			const CheatEntry *ce = &_cheats_ui[i];

			DrawSprite((*ce->been_used) ? SPR_BOX_CHECKED : SPR_BOX_EMPTY, PAL_NONE, box_left, y + 2);

			switch (ce->type) {
				case SLE_BOOL: {
					bool on = (*(bool*)ce->variable);

					DrawFrameRect(button_left, y + 1, button_left + 20 - 1, y + FONT_HEIGHT_NORMAL - 1, on ? COLOUR_GREEN : COLOUR_RED, on ? FR_LOWERED : FR_NONE);
					SetDParam(0, on ? STR_CONFIG_SETTING_ON : STR_CONFIG_SETTING_OFF);
					break;
				}

				default: {
					int32 val = (int32)ReadValue(ce->variable, ce->type);
					char buf[512];

					/* Draw [<][>] boxes for settings of an integer-type */
					DrawArrowButtons(button_left, y, COLOUR_YELLOW, clicked - (i * 2), true, true);

					switch (ce->str) {
						/* Display date for change date cheat */
						case STR_CHEAT_CHANGE_DATE: SetDParam(0, _date); break;

						/* Draw coloured flag for change company cheat */
						case STR_CHEAT_CHANGE_COMPANY: {
							SetDParam(0, val + 1);
							GetString(buf, STR_CHEAT_CHANGE_COMPANY, lastof(buf));
							uint offset = 10 + GetStringBoundingBox(buf).width;
							DrawCompanyIcon(_local_company, rtl ? text_right - offset - 10 : text_left + offset, y + 2);
							break;
						}

						/* Set correct string for switch climate cheat */
						case STR_CHEAT_SWITCH_CLIMATE: val += STR_CHEAT_SWITCH_CLIMATE_TEMPERATE_LANDSCAPE;
							/* FALL THROUGH */

						default: SetDParam(0, val);
					}
					break;
				}
			}

			DrawString(text_left, text_right, y + 1, ce->str);

			y += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != CW_PANEL) return;

		uint width = 0;
		for (int i = 0; i != lengthof(_cheats_ui); i++) {
			const CheatEntry *ce = &_cheats_ui[i];
			switch (ce->type) {
				case SLE_BOOL:
					SetDParam(0, STR_CONFIG_SETTING_ON);
					width = max(width, GetStringBoundingBox(ce->str).width);
					SetDParam(0, STR_CONFIG_SETTING_OFF);
					width = max(width, GetStringBoundingBox(ce->str).width);
					break;

				default:
					switch (ce->str) {
						/* Display date for change date cheat */
						case STR_CHEAT_CHANGE_DATE:
							SetDParam(0, ConvertYMDToDate(MAX_YEAR, 11, 31));
							width = max(width, GetStringBoundingBox(ce->str).width);
							break;

						/* Draw coloured flag for change company cheat */
						case STR_CHEAT_CHANGE_COMPANY:
							SetDParam(0, 15);
							width = max(width, GetStringBoundingBox(ce->str).width + 10 + 10);
							break;

						/* Set correct string for switch climate cheat */
						case STR_CHEAT_SWITCH_CLIMATE:
							for (StringID i = STR_CHEAT_SWITCH_CLIMATE_TEMPERATE_LANDSCAPE; i <= STR_CHEAT_SWITCH_CLIMATE_TOYLAND_LANDSCAPE; i++) {
								SetDParam(0, i);
								width = max(width, GetStringBoundingBox(ce->str).width);
							}
							break;

						default:
							SetDParam(0, INT64_MAX);
							width = max(width, GetStringBoundingBox(ce->str).width);
							break;
					}
					break;
			}
		}

		size->width = width + 50 /* stuff on the left */ + 10 /* extra spacing on right */;
		this->header_height = GetStringHeight(STR_CHEATS_WARNING, size->width - WD_FRAMERECT_LEFT - WD_FRAMERECT_RIGHT) + WD_PAR_VSEP_WIDE;
		size->height = this->header_height + WD_FRAMERECT_TOP + WD_PAR_VSEP_NORMAL + WD_FRAMERECT_BOTTOM + (FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL) * lengthof(_cheats_ui);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		const NWidgetBase *wid = this->GetWidget<NWidgetBase>(CW_PANEL);
		uint btn = (pt.y - wid->pos_y - WD_FRAMERECT_TOP - this->header_height) / (FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL);
		uint x = pt.x - wid->pos_x;
		bool rtl = _current_text_dir == TD_RTL;
		if (rtl) x = wid->current_x - x;

		if (btn >= lengthof(_cheats_ui)) return;

		const CheatEntry *ce = &_cheats_ui[btn];
		int value = (int32)ReadValue(ce->variable, ce->type);
		int oldvalue = value;

		if (btn == CHT_CHANGE_DATE && x >= 40) {
			/* Click at the date text directly. */
			SetDParam(0, value);
			ShowQueryString(STR_JUST_INT, STR_CHEAT_CHANGE_DATE_QUERY_CAPT, 8, 100, this, CS_NUMERAL, QSF_ACCEPT_UNCHANGED);
			return;
		}

		/* Not clicking a button? */
		if (!IsInsideMM(x, 20, 40)) return;

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
				if (value != oldvalue || btn == CHT_MONEY) this->clicked = btn * 2 + 1 + ((x >= 30) != rtl ? 1 : 0);
				break;
		}

		if (value != oldvalue) WriteValue(ce->variable, ce->type, (int64)value);

		this->flags4 |= WF_TIMEOUT_BEGIN;

		this->SetDirty();
	}

	virtual void OnTimeout()
	{
		this->clicked = 0;
		this->SetDirty();
	}

	virtual void OnQueryTextFinished(char *str)
	{
		/* Was 'cancel' pressed or nothing entered? */
		if (str == NULL || StrEmpty(str)) return;

		const CheatEntry *ce = &_cheats_ui[CHT_CHANGE_DATE];
		int oldvalue = (int32)ReadValue(ce->variable, ce->type);
		int value = atoi(str);
		*ce->been_used = true;
		value = ce->proc(value, value - oldvalue);

		if (value != oldvalue) WriteValue(ce->variable, ce->type, (int64)value);
		this->SetDirty();
	}
};

/** Window description of the cheats GUI. */
static const WindowDesc _cheats_desc(
	WDP_AUTO, 0, 0,
	WC_CHEATS, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_cheat_widgets, lengthof(_nested_cheat_widgets)
);

/** Open cheat window. */
void ShowCheatWindow()
{
	DeleteWindowById(WC_CHEATS, 0);
	new CheatWindow(&_cheats_desc);
}
