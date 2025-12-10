/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file cheat_gui.cpp GUI related to cheating. */

#include "stdafx.h"
#include "command_func.h"
#include "cheat_type.h"
#include "company_base.h"
#include "company_func.h"
#include "currency.h"
#include "saveload/saveload.h"
#include "vehicle_base.h"
#include "textbuf_gui.h"
#include "window_gui.h"
#include "string_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "rail_gui.h"
#include "settings_gui.h"
#include "company_gui.h"
#include "linkgraph/linkgraphschedule.h"
#include "map_func.h"
#include "tile_map.h"
#include "newgrf.h"
#include "error.h"
#include "misc_cmd.h"
#include "settings_type.h"
#include "settings_internal.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_economy.h"
#include "core/string_consumer.hpp"

#include "widgets/cheat_widget.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "safeguards.h"


/**
 * The 'amount' to cheat with.
 * This variable is semantically a constant value, but because the cheat
 * code requires to be able to write to the variable it is not constified.
 */
static int32_t _money_cheat_amount = 10000000;

/**
 * Handle cheating of money.
 * Note that the amount of money of a company must be changed through a command
 * rather than by setting a variable. Since the cheat data structure expects a
 * variable, the amount of given/taken money is used for this purpose.
 * @param change_direction is -1 or +1 (down/up)
 * @return Amount of money cheat.
 */
static int32_t ClickMoneyCheat(int32_t, int32_t change_direction)
{
	Command<CMD_MONEY_CHEAT>::Post(Money(_money_cheat_amount) * change_direction);
	return _money_cheat_amount;
}

/**
 * Handle changing of company.
 * @param new_value company to set to
 * @param change_direction is -1 or +1 (down/up)
 * @return The new company.
 */
static int32_t ClickChangeCompanyCheat(int32_t new_value, int32_t change_direction)
{
	while ((uint)new_value < Company::GetPoolSize()) {
		if (Company::IsValidID((CompanyID)new_value)) {
			SetLocalCompany((CompanyID)new_value);
			return _local_company.base();
		}
		new_value += change_direction;
	}

	return _local_company.base();
}

/**
 * Allow (or disallow) changing production of all industries.
 * @param new_value new value
 * @return New value allowing change of industry production.
 */
static int32_t ClickSetProdCheat(int32_t new_value, int32_t)
{
	_cheats.setup_prod.value = (new_value != 0);
	InvalidateWindowClassesData(WC_INDUSTRY_VIEW);
	return _cheats.setup_prod.value;
}

extern void CalendarEnginesMonthlyLoop();

/**
 * Handle changing of the current year.
 * @param new_value The chosen year to change to.
 * @return New year.
 */
static int32_t ClickChangeDateCheat(int32_t new_value, int32_t)
{
	/* Don't allow changing to an invalid year, or the current year. */
	auto new_year = Clamp(TimerGameCalendar::Year(new_value), CalendarTime::MIN_YEAR, CalendarTime::MAX_YEAR);
	if (new_year == TimerGameCalendar::year) return TimerGameCalendar::year.base();

	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::date);
	TimerGameCalendar::Date new_calendar_date = TimerGameCalendar::ConvertYMDToDate(new_year, ymd.month, ymd.day);

	TimerGameCalendar::SetDate(new_calendar_date, TimerGameCalendar::date_fract);

	/* If not using wallclock units, we keep economy date in sync with calendar date and must change it also. */
	if (!TimerGameEconomy::UsingWallclockUnits()) {
		/* Keep economy and calendar dates synced. */
		TimerGameEconomy::Date new_economy_date{new_calendar_date.base()};

		/* Shift cached dates before we change the date. */
		for (auto v : Vehicle::Iterate()) v->ShiftDates(new_economy_date - TimerGameEconomy::date);
		LinkGraphSchedule::instance.ShiftDates(new_economy_date - TimerGameEconomy::date);

		/* Now it's safe to actually change the date. */
		TimerGameEconomy::SetDate(new_economy_date, TimerGameEconomy::date_fract);
	}

	CalendarEnginesMonthlyLoop();
	SetWindowDirty(WC_STATUS_BAR, 0);
	InvalidateWindowClassesData(WC_BUILD_STATION, 0);
	InvalidateWindowClassesData(WC_BUS_STATION, 0);
	InvalidateWindowClassesData(WC_TRUCK_STATION, 0);
	InvalidateWindowClassesData(WC_BUILD_OBJECT, 0);
	InvalidateWindowClassesData(WC_FINANCES, 0);
	ResetSignalVariant();
	return TimerGameCalendar::year.base();
}

/**
 * Allow (or disallow) a change of the maximum allowed heightlevel.
 * @param new_value new value
 * @return New value (or unchanged old value) of the maximum
 *         allowed heightlevel value.
 */
static int32_t ClickChangeMaxHlCheat(int32_t new_value, int32_t)
{
	new_value = Clamp(new_value, MIN_MAP_HEIGHT_LIMIT, MAX_MAP_HEIGHT_LIMIT);

	/* Check if at least one mountain on the map is higher than the new value.
	 * If yes, disallow the change. */
	for (const auto t : Map::Iterate()) {
		if ((int32_t)TileHeight(t) > new_value) {
			ShowErrorMessage(GetEncodedString(STR_CONFIG_SETTING_TOO_HIGH_MOUNTAIN), {}, WL_ERROR);
			/* Return old, unchanged value */
			return _settings_game.construction.map_height_limit;
		}
	}

	/* Execute the change and reload GRF Data */
	_settings_game.construction.map_height_limit = new_value;
	ReloadNewGRFData();

	/* The smallmap uses an index from heightlevels to colours. Trigger rebuilding it. */
	InvalidateWindowClassesData(WC_SMALLMAP, 2);

	return _settings_game.construction.map_height_limit;
}

/** Available cheats. */
enum CheatNumbers : uint8_t {
	CHT_MONEY,           ///< Change amount of money.
	CHT_CHANGE_COMPANY,  ///< Switch company.
	CHT_EXTRA_DYNAMITE,  ///< Dynamite anything.
	CHT_CROSSINGTUNNELS, ///< Allow tunnels to cross each other.
	CHT_NO_JETCRASH,     ///< Disable jet-airplane crashes.
	CHT_SETUP_PROD,      ///< Allow manually editing of industry production.
	CHT_STATION_RATING,  ///< Fix station ratings at 100%.
	CHT_EDIT_MAX_HL,     ///< Edit maximum allowed heightlevel
	CHT_CHANGE_DATE,     ///< Do time traveling.

	CHT_NUM_CHEATS,      ///< Number of cheats.
};

/**
 * Signature of handler function when user clicks at a cheat.
 * @param new_value The new value.
 * @param change_direction Change direction (+1, +1), \c 0 for boolean settings.
 */
typedef int32_t CheckButtonClick(int32_t new_value, int32_t change_direction);

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
	{SLE_INT32, STR_CHEAT_MONEY,           &_money_cheat_amount,                          &_cheats.money.been_used,            &ClickMoneyCheat         },
	{SLE_UINT8, STR_CHEAT_CHANGE_COMPANY,  &_local_company,                               &_cheats.switch_company.been_used,   &ClickChangeCompanyCheat },
	{SLE_BOOL,  STR_CHEAT_EXTRA_DYNAMITE,  &_cheats.magic_bulldozer.value,                &_cheats.magic_bulldozer.been_used,  nullptr                  },
	{SLE_BOOL,  STR_CHEAT_CROSSINGTUNNELS, &_cheats.crossing_tunnels.value,               &_cheats.crossing_tunnels.been_used, nullptr                  },
	{SLE_BOOL,  STR_CHEAT_NO_JETCRASH,     &_cheats.no_jetcrash.value,                    &_cheats.no_jetcrash.been_used,      nullptr                  },
	{SLE_BOOL,  STR_CHEAT_SETUP_PROD,      &_cheats.setup_prod.value,                     &_cheats.setup_prod.been_used,       &ClickSetProdCheat       },
	{SLE_BOOL,  STR_CHEAT_STATION_RATING,  &_cheats.station_rating.value,                 &_cheats.station_rating.been_used,   nullptr                  },
	{SLE_UINT8, STR_CHEAT_EDIT_MAX_HL,     &_settings_game.construction.map_height_limit, &_cheats.edit_max_hl.been_used,      &ClickChangeMaxHlCheat   },
	{SLE_INT32, STR_CHEAT_CHANGE_DATE,     &TimerGameCalendar::year,                      &_cheats.change_date.been_used,      &ClickChangeDateCheat    },
};

static_assert(CHT_NUM_CHEATS == lengthof(_cheats_ui));

/** Widget definitions of the cheat GUI. */
static constexpr std::initializer_list<NWidgetPart> _nested_cheat_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetStringTip(STR_CHEATS, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPadding(WidgetDimensions::unscaled.framerect),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_C_PANEL),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_C_SETTINGS),
		EndContainer(),
	EndContainer(),
};

/** GUI for the cheats. */
struct CheatWindow : Window {
	int clicked = 0;
	int clicked_cheat = 0;
	uint line_height = 0;
	Dimension icon{}; ///< Dimension of company icon sprite

	std::vector<const SettingDesc *> sandbox_settings{};
	const SettingDesc *clicked_setting = nullptr;
	const SettingDesc *last_clicked_setting = nullptr;
	const SettingDesc *valuewindow_entry = nullptr;

	CheatWindow(WindowDesc &desc) : Window(desc)
	{
		this->sandbox_settings = GetFilteredSettingCollection([](const SettingDesc &sd) { return sd.flags.Test(SettingFlag::Sandbox); });
		this->InitNested();
	}

	void OnInit() override
	{
		this->icon = GetSpriteSize(SPR_COMPANY_ICON);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_C_PANEL: DrawCheatWidget(r); break;
			case WID_C_SETTINGS: DrawSettingsWidget(r); break;
		}
	}

	void DrawCheatWidget(const Rect &r) const
	{
		const Rect ir = r;
		int y = ir.top;

		bool rtl = _current_text_dir == TD_RTL;
		uint button_left = rtl ? ir.right - SETTING_BUTTON_WIDTH : ir.left;
		uint text_left   = ir.left + (rtl ? 0 : WidgetDimensions::scaled.hsep_wide + SETTING_BUTTON_WIDTH);
		uint text_right  = ir.right - (rtl ? WidgetDimensions::scaled.hsep_wide + SETTING_BUTTON_WIDTH : 0);

		int text_y_offset = (this->line_height - GetCharacterHeight(FS_NORMAL)) / 2;
		int button_y_offset = (this->line_height - SETTING_BUTTON_HEIGHT) / 2;
		int icon_y_offset = (this->line_height - this->icon.height) / 2;

		for (int i = 0; i != lengthof(_cheats_ui); i++) {
			const CheatEntry *ce = &_cheats_ui[i];

			std::string str;
			switch (ce->type) {
				case SLE_BOOL: {
					bool on = (*(bool*)ce->variable);

					DrawBoolButton(button_left, y + button_y_offset, COLOUR_YELLOW, COLOUR_GREY, on, true);
					str = GetString(ce->str, on ? STR_CONFIG_SETTING_ON : STR_CONFIG_SETTING_OFF);
					break;
				}

				default: {
					int32_t val = static_cast<int32_t>(ReadValue(ce->variable, ce->type));

					/* Draw [<][>] boxes for settings of an integer-type */
					DrawArrowButtons(button_left, y + button_y_offset, COLOUR_YELLOW, clicked - (i * 2), true, true);

					switch (ce->str) {
						/* Display date for change date cheat */
						case STR_CHEAT_CHANGE_DATE:
							str = GetString(ce->str, TimerGameCalendar::date);
							break;

						/* Draw coloured flag for change company cheat */
						case STR_CHEAT_CHANGE_COMPANY: {
							str = GetString(ce->str, val + 1);
							uint offset = WidgetDimensions::scaled.hsep_indent + GetStringBoundingBox(str).width;
							DrawCompanyIcon(_local_company, rtl ? text_right - offset - WidgetDimensions::scaled.hsep_indent : text_left + offset, y + icon_y_offset);
							break;
						}

						default:
							str = GetString(ce->str, val);
							break;
					}
					break;
				}
			}

			DrawString(text_left, text_right, y + text_y_offset, str);

			y += this->line_height;
		}
	}

	void DrawSettingsWidget(const Rect &r) const
	{
		Rect ir = r.WithHeight(this->line_height);

		for (const auto &desc : this->sandbox_settings) {
			DrawSetting(ir, desc);
			ir = ir.Translate(0, this->line_height);
		}
	}

	void DrawSetting(const Rect r, const SettingDesc *desc) const
	{
		const IntSettingDesc *sd = desc->AsIntSetting();
		int state = this->clicked_setting == sd ? this->clicked : 0;

		bool rtl = _current_text_dir == TD_RTL;

		Rect buttons = r.WithWidth(SETTING_BUTTON_WIDTH, rtl);
		Rect text = r.Indent(SETTING_BUTTON_WIDTH + WidgetDimensions::scaled.hsep_wide, rtl);
		buttons.top += (r.Height() - SETTING_BUTTON_HEIGHT) / 2;
		text.top += (r.Height() - GetCharacterHeight(FS_NORMAL)) / 2;

		/* We do not allow changes of some items when we are a client in a network game */
		bool editable = sd->IsEditable();

		auto [min_val, max_val] = sd->GetRange();
		int32_t value = sd->Read(&GetGameSettings());
		if (sd->IsBoolSetting()) {
			/* Draw checkbox for boolean-value either on/off */
			DrawBoolButton(buttons.left, buttons.top, COLOUR_YELLOW, COLOUR_GREY, value != 0, editable);
		} else if (sd->flags.Test(SettingFlag::GuiDropdown)) {
			/* Draw [v] button for settings of an enum-type */
			DrawDropDownButton(buttons.left, buttons.top, COLOUR_YELLOW, state != 0, editable);
		} else {
			/* Draw [<][>] boxes for settings of an integer-type */
			DrawArrowButtons(buttons.left, buttons.top, COLOUR_YELLOW, state,
					editable && value != (sd->flags.Test(SettingFlag::GuiZeroIsSpecial) ? 0 : min_val), editable && static_cast<uint32_t>(value) != max_val);
		}
		auto [param1, param2] = sd->GetValueParams(value);
		DrawString(text.left, text.right, text.top, GetString(sd->GetTitle(), STR_CONFIG_SETTING_VALUE, param1, param2), TC_LIGHT_BLUE);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_C_PANEL: UpdateCheatPanelSize(size); break;
			case WID_C_SETTINGS: UpdateSettingsPanelSize(size); break;
		}
	}

	void UpdateCheatPanelSize(Dimension &size)
	{
		uint width = 0;
		for (const auto &ce : _cheats_ui) {
			switch (ce.type) {
				case SLE_BOOL:
					width = std::max(width, GetStringBoundingBox(GetString(ce.str, STR_CONFIG_SETTING_ON)).width);
					width = std::max(width, GetStringBoundingBox(GetString(ce.str, STR_CONFIG_SETTING_OFF)).width);
					break;

				default:
					switch (ce.str) {
						/* Display date for change date cheat */
						case STR_CHEAT_CHANGE_DATE:
							width = std::max(width, GetStringBoundingBox(GetString(ce.str, TimerGameCalendar::ConvertYMDToDate(CalendarTime::MAX_YEAR, 11, 31))).width);
							break;

						/* Draw coloured flag for change company cheat */
						case STR_CHEAT_CHANGE_COMPANY:
							width = std::max(width, GetStringBoundingBox(GetString(ce.str, MAX_COMPANIES)).width + WidgetDimensions::scaled.hsep_wide);
							break;

						default:
							width = std::max(width, GetStringBoundingBox(GetString(ce.str, INT64_MAX)).width);
							break;
					}
					break;
			}
		}

		this->line_height = std::max<uint>(this->icon.height, SETTING_BUTTON_HEIGHT);
		this->line_height = std::max<uint>(this->line_height, GetCharacterHeight(FS_NORMAL)) + WidgetDimensions::scaled.framerect.Vertical();

		size.width = width + WidgetDimensions::scaled.hsep_wide * 2 + SETTING_BUTTON_WIDTH;
		size.height = this->line_height * lengthof(_cheats_ui);
	}

	void UpdateSettingsPanelSize(Dimension &size)
	{
		uint width = 0;
		for (const auto &desc : this->sandbox_settings) {
			const IntSettingDesc *sd = desc->AsIntSetting();

			auto [param1, param2] = sd->GetValueParams(sd->GetDefaultValue());
			width = std::max(width, GetStringBoundingBox(GetString(sd->GetTitle(), STR_CONFIG_SETTING_VALUE, param1, param2)).width);
		}

		size.width = width + WidgetDimensions::scaled.hsep_wide * 2 + SETTING_BUTTON_WIDTH;
		size.height = this->line_height * static_cast<uint>(std::size(this->sandbox_settings));
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_C_PANEL: CheatPanelClick(pt); break;
			case WID_C_SETTINGS: SettingsPanelClick(pt); break;
		}
	}

	void CheatPanelClick(Point pt)
	{
		Rect r = this->GetWidget<NWidgetBase>(WID_C_PANEL)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
		uint btn = (pt.y - r.top) / this->line_height;
		int x = pt.x - r.left;
		bool rtl = _current_text_dir == TD_RTL;
		if (rtl) x = r.Width() - 1 - x;

		if (btn >= lengthof(_cheats_ui)) return;

		const CheatEntry *ce = &_cheats_ui[btn];
		int value = static_cast<int32_t>(ReadValue(ce->variable, ce->type));
		int oldvalue = value;

		if (btn == CHT_CHANGE_DATE && x >= SETTING_BUTTON_WIDTH) {
			/* Click at the date text directly. */
			clicked_cheat = CHT_CHANGE_DATE;
			ShowQueryString(GetString(STR_JUST_INT, value), STR_CHEAT_CHANGE_DATE_QUERY_CAPT, 8, this, CS_NUMERAL, QueryStringFlag::AcceptUnchanged);
			return;
		} else if (btn == CHT_EDIT_MAX_HL && x >= SETTING_BUTTON_WIDTH) {
			clicked_cheat = CHT_EDIT_MAX_HL;
			ShowQueryString(GetString(STR_JUST_INT, value), STR_CHEAT_EDIT_MAX_HL_QUERY_CAPT, 8, this, CS_NUMERAL, QueryStringFlag::AcceptUnchanged);
			return;
		}

		/* Not clicking a button? */
		if (!IsInsideMM(x, 0, SETTING_BUTTON_WIDTH)) return;

		this->clicked_setting = nullptr;
		*ce->been_used = true;

		switch (ce->type) {
			case SLE_BOOL:
				value ^= 1;
				if (ce->proc != nullptr) ce->proc(value, 0);
				break;

			default:
				/* Take whatever the function returns */
				value = ce->proc(value + ((x >= SETTING_BUTTON_WIDTH / 2) ? 1 : -1), (x >= SETTING_BUTTON_WIDTH / 2) ? 1 : -1);

				/* The first cheat (money), doesn't return a different value. */
				if (value != oldvalue || btn == CHT_MONEY) this->clicked = btn * 2 + 1 + ((x >= SETTING_BUTTON_WIDTH / 2) != rtl ? 1 : 0);
				break;
		}

		if (value != oldvalue) WriteValue(ce->variable, ce->type, static_cast<int64_t>(value));

		this->SetTimeout();

		this->SetDirty();
	}

	void SettingsPanelClick(Point pt)
	{
		int row = this->GetRowFromWidget(pt.y, WID_C_SETTINGS, WidgetDimensions::scaled.framerect.top, this->line_height);
		if (row == INT_MAX) return;

		const SettingDesc *desc = this->sandbox_settings[row];
		const IntSettingDesc *sd = desc->AsIntSetting();

		if (!sd->IsEditable()) return;

		Rect r = this->GetWidget<NWidgetBase>(WID_C_SETTINGS)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
		int x = pt.x - r.left;
		bool rtl = _current_text_dir == TD_RTL;
		if (rtl) x = r.Width() - 1 - x;

		if (x < SETTING_BUTTON_WIDTH) {
			ChangeSettingValue(sd, x);
		} else {
			/* Only open editbox if clicked for the second time, and only for types where it is sensible for. */
			if (this->last_clicked_setting == sd && !sd->IsBoolSetting() && !sd->flags.Test(SettingFlag::GuiDropdown)) {
				int64_t value64 = sd->Read(&GetGameSettings());

				/* Show the correct currency-translated value */
				if (sd->flags.Test(SettingFlag::GuiCurrency)) value64 *= GetCurrency().rate;

				CharSetFilter charset_filter = CS_NUMERAL; //default, only numeric input allowed
				if (sd->min < 0) charset_filter = CS_NUMERAL_SIGNED; // special case, also allow '-' sign for negative input

				this->valuewindow_entry = sd;

				/* Limit string length to 14 so that MAX_INT32 * max currency rate doesn't exceed MAX_INT64. */
				ShowQueryString(GetString(STR_JUST_INT, value64), STR_CONFIG_SETTING_QUERY_CAPTION, 15, this, charset_filter, QueryStringFlag::EnableDefault);
			}

			this->clicked_setting = sd;
		}
	}

	void ChangeSettingValue(const IntSettingDesc *sd, int x)
	{
		int32_t value = sd->Read(&GetGameSettings());
		int32_t oldvalue = value;
		if (sd->IsBoolSetting()) {
			value ^= 1;
		} else {
			/* don't allow too fast scrolling */
			if (this->flags.Test(WindowFlag::Timeout) && this->timeout_timer > 1) {
				_left_button_clicked = false;
				return;
			}

			/* Add a dynamic step-size to the scroller. In a maximum of
			 * 50-steps you should be able to get from min to max,
			 * unless specified otherwise in the 'interval' variable
			 * of the current setting. */
			uint32_t step = (sd->interval == 0) ? ((sd->max - sd->min) / 50) : sd->interval;
			if (step == 0) step = 1;

			/* Increase or decrease the value and clamp it to extremes */
			if (x >= SETTING_BUTTON_WIDTH / 2) {
				value += step;
				if (sd->min < 0) {
					assert(static_cast<int32_t>(sd->max) >= 0);
					if (value > static_cast<int32_t>(sd->max)) value = static_cast<int32_t>(sd->max);
				} else {
					if (static_cast<uint32_t>(value) > sd->max) value = static_cast<int32_t>(sd->max);
				}
				if (value < sd->min) value = sd->min; // skip between "disabled" and minimum
			} else {
				value -= step;
				if (value < sd->min) value = sd->flags.Test(SettingFlag::GuiZeroIsSpecial) ? 0 : sd->min;
			}

			/* Set up scroller timeout for numeric values */
			if (value != oldvalue) {
				this->last_clicked_setting = nullptr;
				this->clicked_setting = sd;
				this->clicked =  (x >= SETTING_BUTTON_WIDTH / 2) != (_current_text_dir == TD_RTL) ? 2 : 1;
				this->SetTimeout();
				_left_button_clicked = false;
			}
		}

		if (value != oldvalue) {
			SetSettingValue(sd, value);
			this->SetDirty();
		}
	}

	bool OnTooltip([[maybe_unused]] Point pt, WidgetID widget, TooltipCloseCondition close_cond) override
	{
		if (widget != WID_C_SETTINGS) return false;

		int row = GetRowFromWidget(pt.y, widget, WidgetDimensions::scaled.framerect.top, this->line_height);
		if (row == INT_MAX) return false;

		const SettingDesc *desc = this->sandbox_settings[row];
		const IntSettingDesc *sd = desc->AsIntSetting();
		GuiShowTooltips(this, GetEncodedString(sd->GetHelp()), close_cond);

		return true;
	}

	void OnTimeout() override
	{
		this->clicked_setting = nullptr;
		this->clicked = 0;
		this->SetDirty();
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		/* Was 'cancel' pressed or nothing entered? */
		if (!str.has_value() || str->empty()) return;

		if (this->valuewindow_entry != nullptr) {
			const IntSettingDesc *sd = this->valuewindow_entry->AsIntSetting();

			int32_t value;
			if (!str->empty()) {
				auto llvalue = ParseInteger<int64_t>(*str, 10, true);
				if (!llvalue.has_value()) return;

				/* Save the correct currency-translated value */
				if (sd->flags.Test(SettingFlag::GuiCurrency)) llvalue = *llvalue / GetCurrency().rate;

				value = ClampTo<int32_t>(*llvalue);
			} else {
				value = sd->GetDefaultValue();
			}

			SetSettingValue(sd, value);
		} else {
			const CheatEntry *ce = &_cheats_ui[clicked_cheat];
			int oldvalue = static_cast<int32_t>(ReadValue(ce->variable, ce->type));
			auto value = ParseInteger<int32_t>(*str, 10, true);
			if (!value.has_value()) return;
			*ce->been_used = true;
			value = ce->proc(*value, *value - oldvalue);

			if (*value != oldvalue) WriteValue(ce->variable, ce->type, static_cast<int64_t>(*value));
		}

		this->valuewindow_entry = nullptr;
		this->SetDirty();
	}

	const IntervalTimer<TimerGameCalendar> daily_interval = {{TimerGameCalendar::MONTH, TimerGameCalendar::Priority::NONE}, [this](auto) {
		this->SetDirty();
	}};
};

/** Window description of the cheats GUI. */
static WindowDesc _cheats_desc(
	WDP_AUTO, "cheats", 0, 0,
	WC_CHEATS, WC_NONE,
	{},
	_nested_cheat_widgets
);

/** Open cheat window. */
void ShowCheatWindow()
{
	CloseWindowById(WC_CHEATS, 0);
	new CheatWindow(_cheats_desc);
}
