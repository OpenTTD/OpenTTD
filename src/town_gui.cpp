/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file town_gui.cpp GUI for towns. */

#include "stdafx.h"
#include "town.h"
#include "viewport_func.h"
#include "error.h"
#include "gui.h"
#include "command_func.h"
#include "company_func.h"
#include "company_base.h"
#include "company_gui.h"
#include "network/network.h"
#include "string_func.h"
#include "strings_func.h"
#include "sound_func.h"
#include "tilehighlight_func.h"
#include "sortlist_type.h"
#include "road_cmd.h"
#include "landscape.h"
#include "querystring_gui.h"
#include "window_func.h"
#include "townname_func.h"
#include "core/backup_type.hpp"
#include "core/geometry_func.hpp"
#include "genworld.h"
#include "stringfilter_type.h"
#include "widgets/dropdown_func.h"
#include "town_kdtree.h"
#include "town_cmd.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_window.h"
#include "zoom_func.h"
#include "hotkeys.h"

#include "widgets/town_widget.h"

#include "table/strings.h"

#include "safeguards.h"

TownKdtree _town_local_authority_kdtree(&Kdtree_TownXYFunc);

typedef GUIList<const Town*, const bool &> GUITownList;

static const NWidgetPart _nested_town_authority_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_TA_CAPTION), SetDataTip(STR_LOCAL_AUTHORITY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TA_ZONE_BUTTON), SetMinimalSize(50, 0), SetDataTip(STR_LOCAL_AUTHORITY_ZONE, STR_LOCAL_AUTHORITY_ZONE_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TA_RATING_INFO), SetMinimalSize(317, 92), SetResize(1, 1), EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TA_COMMAND_LIST), SetMinimalSize(317, 52), SetResize(1, 0), SetDataTip(0x0, STR_LOCAL_AUTHORITY_ACTIONS_TOOLTIP), EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TA_ACTION_INFO), SetMinimalSize(317, 52), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TA_EXECUTE),  SetMinimalSize(317, 12), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_LOCAL_AUTHORITY_DO_IT_BUTTON, STR_LOCAL_AUTHORITY_DO_IT_TOOLTIP),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer()
};

/** Town authority window. */
struct TownAuthorityWindow : Window {
private:
	Town *town;    ///< Town being displayed.
	int sel_index; ///< Currently selected town action, \c 0 to \c TACT_COUNT-1, \c -1 means no action selected.
	uint displayed_actions_on_previous_painting; ///< Actions that were available on the previous call to OnPaint()
	TownActions enabled_actions; ///< Actions that are enabled in settings.
	TownActions available_actions; ///< Actions that are available to execute for the current company.

	Dimension icon_size;      ///< Dimensions of company icon
	Dimension exclusive_size; ///< Dimensions of exlusive icon

	/**
	 * Get the position of the Nth set bit.
	 *
	 * If there is no Nth bit set return -1
	 *
	 * @param n The Nth set bit from which we want to know the position
	 * @return The position of the Nth set bit, or -1 if no Nth bit set.
	 */
	int GetNthSetBit(int n)
	{
		if (n >= 0) {
			for (uint i : SetBitIterator(this->enabled_actions)) {
				n--;
				if (n < 0) return i;
			}
		}
		return -1;
	}

	/**
	 * Gets all town authority actions enabled in settings.
	 *
	 * @return Bitmask of actions enabled in the settings.
	 */
	static TownActions GetEnabledActions()
	{
		TownActions enabled = TACT_ALL;

		if (!_settings_game.economy.fund_roads) CLRBITS(enabled, TACT_ROAD_REBUILD);
		if (!_settings_game.economy.fund_buildings) CLRBITS(enabled, TACT_FUND_BUILDINGS);
		if (!_settings_game.economy.exclusive_rights) CLRBITS(enabled, TACT_BUY_RIGHTS);
		if (!_settings_game.economy.bribe) CLRBITS(enabled, TACT_BRIBE);

		return enabled;
	}

public:
	TownAuthorityWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc), sel_index(-1), displayed_actions_on_previous_painting(0), available_actions(TACT_NONE)
	{
		this->town = Town::Get(window_number);
		this->enabled_actions = GetEnabledActions();
		this->InitNested(window_number);
	}

	void OnInit() override
	{
		this->icon_size      = GetSpriteSize(SPR_COMPANY_ICON);
		this->exclusive_size = GetSpriteSize(SPR_EXCLUSIVE_TRANSPORT);
	}

	void OnPaint() override
	{
		this->available_actions = GetMaskOfTownActions(_local_company, this->town);
		if (this->available_actions != displayed_actions_on_previous_painting) this->SetDirty();
		displayed_actions_on_previous_painting = this->available_actions;

		this->SetWidgetLoweredState(WID_TA_ZONE_BUTTON, this->town->show_zone);
		this->SetWidgetDisabledState(WID_TA_EXECUTE, (this->sel_index == -1) || !HasBit(this->available_actions, this->sel_index));

		this->DrawWidgets();
		if (!this->IsShaded())
		{
			this->DrawRatings();
			this->DrawActions();
		}
	}

	/** Draw the contents of the ratings panel. May request a resize of the window if the contents does not fit. */
	void DrawRatings()
	{
		Rect r = this->GetWidget<NWidgetBase>(WID_TA_RATING_INFO)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);

		int text_y_offset      = (this->resize.step_height - GetCharacterHeight(FS_NORMAL)) / 2;
		int icon_y_offset      = (this->resize.step_height - this->icon_size.height) / 2;
		int exclusive_y_offset = (this->resize.step_height - this->exclusive_size.height) / 2;

		DrawString(r.left, r.right, r.top + text_y_offset, STR_LOCAL_AUTHORITY_COMPANY_RATINGS);
		r.top += this->resize.step_height;

		bool rtl = _current_text_dir == TD_RTL;
		Rect icon      = r.WithWidth(this->icon_size.width, rtl);
		Rect exclusive = r.Indent(this->icon_size.width + WidgetDimensions::scaled.hsep_normal, rtl).WithWidth(this->exclusive_size.width, rtl);
		Rect text      = r.Indent(this->icon_size.width + WidgetDimensions::scaled.hsep_normal + this->exclusive_size.width + WidgetDimensions::scaled.hsep_normal, rtl);

		/* Draw list of companies */
		for (const Company *c : Company::Iterate()) {
			if ((HasBit(this->town->have_ratings, c->index) || this->town->exclusivity == c->index)) {
				DrawCompanyIcon(c->index, icon.left, text.top + icon_y_offset);

				SetDParam(0, c->index);
				SetDParam(1, c->index);

				int rating = this->town->ratings[c->index];
				StringID str = STR_CARGO_RATING_APPALLING;
				if (rating > RATING_APPALLING) str++;
				if (rating > RATING_VERYPOOR)  str++;
				if (rating > RATING_POOR)      str++;
				if (rating > RATING_MEDIOCRE)  str++;
				if (rating > RATING_GOOD)      str++;
				if (rating > RATING_VERYGOOD)  str++;
				if (rating > RATING_EXCELLENT) str++;

				SetDParam(2, str);
				if (this->town->exclusivity == c->index) {
					DrawSprite(SPR_EXCLUSIVE_TRANSPORT, COMPANY_SPRITE_COLOUR(c->index), exclusive.left, text.top + exclusive_y_offset);
				}

				DrawString(text.left, text.right, text.top + text_y_offset, STR_LOCAL_AUTHORITY_COMPANY_RATING);
				text.top += this->resize.step_height;
			}
		}

		text.bottom = text.top - 1;
		if (text.bottom > r.bottom) {
			/* If the company list is too big to fit, mark ourself dirty and draw again. */
			ResizeWindow(this, 0, text.bottom - r.bottom, false);
		}
	}

	/** Draws the contents of the actions panel. May re-initialise window to resize panel, if the list does not fit. */
	void DrawActions()
	{
		Rect r = this->GetWidget<NWidgetBase>(WID_TA_COMMAND_LIST)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);

		DrawString(r, STR_LOCAL_AUTHORITY_ACTIONS_TITLE);
		r.top += GetCharacterHeight(FS_NORMAL);

		/* Draw list of actions */
		for (int i = 0; i < TACT_COUNT; i++) {
			/* Don't show actions if disabled in settings. */
			if (!HasBit(this->enabled_actions, i)) continue;

			/* Set colour of action based on ability to execute and if selected. */
			TextColour action_colour = TC_GREY | TC_NO_SHADE;
			if (HasBit(this->available_actions, i)) action_colour = TC_ORANGE;
			if (this->sel_index == i) action_colour = TC_WHITE;

			DrawString(r, STR_LOCAL_AUTHORITY_ACTION_SMALL_ADVERTISING_CAMPAIGN + i, action_colour);
			r.top += GetCharacterHeight(FS_NORMAL);
		}
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_TA_CAPTION) SetDParam(0, this->window_number);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_TA_ACTION_INFO:
				if (this->sel_index != -1) {
					Money action_cost = _price[PR_TOWN_ACTION] * _town_action_costs[this->sel_index] >> 8;
					bool affordable = Company::IsValidID(_local_company) && action_cost < Company::Get(_local_company)->money;

					SetDParam(0, action_cost);
					DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.framerect),
						STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_SMALL_ADVERTISING + this->sel_index,
						affordable ? TC_YELLOW : TC_RED);
				}
				break;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_TA_ACTION_INFO: {
				assert(size->width > padding.width && size->height > padding.height);
				Dimension d = {0, 0};
				for (int i = 0; i < TACT_COUNT; i++) {
					SetDParam(0, _price[PR_TOWN_ACTION] * _town_action_costs[i] >> 8);
					d = maxdim(d, GetStringMultiLineBoundingBox(STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_SMALL_ADVERTISING + i, *size));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case WID_TA_COMMAND_LIST:
				size->height = (TACT_COUNT + 1) * GetCharacterHeight(FS_NORMAL) + padding.height;
				size->width = GetStringBoundingBox(STR_LOCAL_AUTHORITY_ACTIONS_TITLE).width;
				for (uint i = 0; i < TACT_COUNT; i++ ) {
					size->width = std::max(size->width, GetStringBoundingBox(STR_LOCAL_AUTHORITY_ACTION_SMALL_ADVERTISING_CAMPAIGN + i).width + padding.width);
				}
				size->width += padding.width;
				break;

			case WID_TA_RATING_INFO:
				resize->height = std::max({this->icon_size.height + WidgetDimensions::scaled.vsep_normal, this->exclusive_size.height + WidgetDimensions::scaled.vsep_normal, (uint)GetCharacterHeight(FS_NORMAL)});
				size->height = 9 * resize->height + padding.height;
				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_TA_ZONE_BUTTON: {
				bool new_show_state = !this->town->show_zone;
				TownID index = this->town->index;

				new_show_state ? _town_local_authority_kdtree.Insert(index) : _town_local_authority_kdtree.Remove(index);

				this->town->show_zone = new_show_state;
				this->SetWidgetLoweredState(widget, new_show_state);
				MarkWholeScreenDirty();
				break;
			}

			case WID_TA_COMMAND_LIST: {
				int y = this->GetRowFromWidget(pt.y, WID_TA_COMMAND_LIST, 1, GetCharacterHeight(FS_NORMAL)) - 1;

				y = GetNthSetBit(y);
				if (y >= 0) {
					this->sel_index = y;
					this->SetDirty();
				}

				/* When double-clicking, continue */
				if (click_count == 1 || y < 0 || !HasBit(this->available_actions, y)) break;
				FALLTHROUGH;
			}

			case WID_TA_EXECUTE:
				Command<CMD_DO_TOWN_ACTION>::Post(STR_ERROR_CAN_T_DO_THIS, this->town->xy, this->window_number, this->sel_index);
				break;
		}
	}

	/** Redraw the whole window on a regular interval. */
	IntervalTimer<TimerWindow> redraw_interval = {std::chrono::seconds(3), [this](auto) {
		this->SetDirty();
	}};

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;

		this->enabled_actions = this->GetEnabledActions();
		if (!HasBit(this->enabled_actions, this->sel_index)) {
			this->sel_index = -1;
		}
	}
};

static WindowDesc _town_authority_desc(__FILE__, __LINE__,
	WDP_AUTO, "view_town_authority", 317, 222,
	WC_TOWN_AUTHORITY, WC_NONE,
	0,
	std::begin(_nested_town_authority_widgets), std::end(_nested_town_authority_widgets)
);

static void ShowTownAuthorityWindow(uint town)
{
	AllocateWindowDescFront<TownAuthorityWindow>(&_town_authority_desc, town);
}


/* Town view window. */
struct TownViewWindow : Window {
private:
	Town *town; ///< Town displayed by the window.

public:
	static const int WID_TV_HEIGHT_NORMAL = 150;

	TownViewWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();

		this->town = Town::Get(window_number);
		if (this->town->larger_town) this->GetWidget<NWidgetCore>(WID_TV_CAPTION)->widget_data = STR_TOWN_VIEW_CITY_CAPTION;

		this->FinishInitNested(window_number);

		this->flags |= WF_DISABLE_VP_SCROLL;
		NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WID_TV_VIEWPORT);
		nvp->InitializeViewport(this, this->town->xy, ScaleZoomGUI(ZOOM_LVL_TOWN));

		/* disable renaming town in network games if you are not the server */
		this->SetWidgetDisabledState(WID_TV_CHANGE_NAME, _networking && !_network_server);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		SetViewportCatchmentTown(Town::Get(this->window_number), false);
		this->Window::Close();
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_TV_CAPTION) SetDParam(0, this->town->index);
	}

	void OnPaint() override
	{
		extern const Town *_viewport_highlight_town;
		this->SetWidgetLoweredState(WID_TV_CATCHMENT, _viewport_highlight_town == this->town);

		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_TV_INFO) return;

		Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);

		SetDParam(0, this->town->cache.population);
		SetDParam(1, this->town->cache.num_houses);
		DrawString(tr, STR_TOWN_VIEW_POPULATION_HOUSES);
		tr.top += GetCharacterHeight(FS_NORMAL);

		SetDParam(0, 1 << CT_PASSENGERS);
		SetDParam(1, this->town->supplied[CT_PASSENGERS].old_act);
		SetDParam(2, this->town->supplied[CT_PASSENGERS].old_max);
		DrawString(tr, STR_TOWN_VIEW_CARGO_LAST_MONTH_MAX);
		tr.top += GetCharacterHeight(FS_NORMAL);

		SetDParam(0, 1 << CT_MAIL);
		SetDParam(1, this->town->supplied[CT_MAIL].old_act);
		SetDParam(2, this->town->supplied[CT_MAIL].old_max);
		DrawString(tr, STR_TOWN_VIEW_CARGO_LAST_MONTH_MAX);
		tr.top += GetCharacterHeight(FS_NORMAL);

		bool first = true;
		for (int i = TE_BEGIN; i < TE_END; i++) {
			if (this->town->goal[i] == 0) continue;
			if (this->town->goal[i] == TOWN_GROWTH_WINTER && (TileHeight(this->town->xy) < LowestSnowLine() || this->town->cache.population <= 90)) continue;
			if (this->town->goal[i] == TOWN_GROWTH_DESERT && (GetTropicZone(this->town->xy) != TROPICZONE_DESERT || this->town->cache.population <= 60)) continue;

			if (first) {
				DrawString(tr, STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH);
				tr.top += GetCharacterHeight(FS_NORMAL);
				first = false;
			}

			bool rtl = _current_text_dir == TD_RTL;

			const CargoSpec *cargo = FindFirstCargoWithTownEffect((TownEffect)i);
			assert(cargo != nullptr);

			StringID string;

			if (this->town->goal[i] == TOWN_GROWTH_DESERT || this->town->goal[i] == TOWN_GROWTH_WINTER) {
				/* For 'original' gameplay, don't show the amount required (you need 1 or more ..) */
				string = STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_DELIVERED_GENERAL;
				if (this->town->received[i].old_act == 0) {
					string = STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_REQUIRED_GENERAL;

					if (this->town->goal[i] == TOWN_GROWTH_WINTER && TileHeight(this->town->xy) < GetSnowLine()) {
						string = STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_REQUIRED_WINTER;
					}
				}

				SetDParam(0, cargo->name);
			} else {
				string = STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_DELIVERED;
				if (this->town->received[i].old_act < this->town->goal[i]) {
					string = STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_REQUIRED;
				}

				SetDParam(0, cargo->Index());
				SetDParam(1, this->town->received[i].old_act);
				SetDParam(2, cargo->Index());
				SetDParam(3, this->town->goal[i]);
			}
			DrawString(tr.Indent(20, rtl), string);
			tr.top += GetCharacterHeight(FS_NORMAL);
		}

		if (HasBit(this->town->flags, TOWN_IS_GROWING)) {
			SetDParam(0, RoundDivSU(this->town->growth_rate + 1, Ticks::DAY_TICKS));
			DrawString(tr, this->town->fund_buildings_months == 0 ? STR_TOWN_VIEW_TOWN_GROWS_EVERY : STR_TOWN_VIEW_TOWN_GROWS_EVERY_FUNDED);
			tr.top += GetCharacterHeight(FS_NORMAL);
		} else {
			DrawString(tr, STR_TOWN_VIEW_TOWN_GROW_STOPPED);
			tr.top += GetCharacterHeight(FS_NORMAL);
		}

		/* only show the town noise, if the noise option is activated. */
		if (_settings_game.economy.station_noise_level) {
			SetDParam(0, this->town->noise_reached);
			SetDParam(1, this->town->MaxTownNoise());
			DrawString(tr, STR_TOWN_VIEW_NOISE_IN_TOWN);
			tr.top += GetCharacterHeight(FS_NORMAL);
		}

		if (!this->town->text.empty()) {
			SetDParamStr(0, this->town->text);
			tr.top = DrawStringMultiLine(tr, STR_JUST_RAW_STRING, TC_BLACK);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_TV_CENTER_VIEW: // scroll to location
				if (_ctrl_pressed) {
					ShowExtraViewportWindow(this->town->xy);
				} else {
					ScrollMainWindowToTile(this->town->xy);
				}
				break;

			case WID_TV_SHOW_AUTHORITY: // town authority
				ShowTownAuthorityWindow(this->window_number);
				break;

			case WID_TV_CHANGE_NAME: // rename
				SetDParam(0, this->window_number);
				ShowQueryString(STR_TOWN_NAME, STR_TOWN_VIEW_RENAME_TOWN_BUTTON, MAX_LENGTH_TOWN_NAME_CHARS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT | QSF_LEN_IN_CHARS);
				break;

			case WID_TV_CATCHMENT:
				SetViewportCatchmentTown(Town::Get(this->window_number), !this->IsWidgetLowered(WID_TV_CATCHMENT));
				break;

			case WID_TV_EXPAND: { // expand town - only available on Scenario editor
				Command<CMD_EXPAND_TOWN>::Post(STR_ERROR_CAN_T_EXPAND_TOWN, this->window_number, 0);
				break;
			}

			case WID_TV_DELETE: // delete town - only available on Scenario editor
				Command<CMD_DELETE_TOWN>::Post(STR_ERROR_TOWN_CAN_T_DELETE, this->window_number);
				break;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_TV_INFO:
				size->height = GetDesiredInfoHeight(size->width) + padding.height;
				break;
		}
	}

	/**
	 * Gets the desired height for the information panel.
	 * @return the desired height in pixels.
	 */
	uint GetDesiredInfoHeight(int width) const
	{
		uint aimed_height = 3 * GetCharacterHeight(FS_NORMAL);

		bool first = true;
		for (int i = TE_BEGIN; i < TE_END; i++) {
			if (this->town->goal[i] == 0) continue;
			if (this->town->goal[i] == TOWN_GROWTH_WINTER && (TileHeight(this->town->xy) < LowestSnowLine() || this->town->cache.population <= 90)) continue;
			if (this->town->goal[i] == TOWN_GROWTH_DESERT && (GetTropicZone(this->town->xy) != TROPICZONE_DESERT || this->town->cache.population <= 60)) continue;

			if (first) {
				aimed_height += GetCharacterHeight(FS_NORMAL);
				first = false;
			}
			aimed_height += GetCharacterHeight(FS_NORMAL);
		}
		aimed_height += GetCharacterHeight(FS_NORMAL);

		if (_settings_game.economy.station_noise_level) aimed_height += GetCharacterHeight(FS_NORMAL);

		if (!this->town->text.empty()) {
			SetDParamStr(0, this->town->text);
			aimed_height += GetStringHeight(STR_JUST_RAW_STRING, width - WidgetDimensions::scaled.framerect.Horizontal());
		}

		return aimed_height;
	}

	void ResizeWindowAsNeeded()
	{
		const NWidgetBase *nwid_info = this->GetWidget<NWidgetBase>(WID_TV_INFO);
		uint aimed_height = GetDesiredInfoHeight(nwid_info->current_x);
		if (aimed_height > nwid_info->current_y || (aimed_height < nwid_info->current_y && nwid_info->current_y > nwid_info->smallest_y)) {
			this->ReInit();
		}
	}

	void OnResize() override
	{
		if (this->viewport != nullptr) {
			NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WID_TV_VIEWPORT);
			nvp->UpdateViewportCoordinates(this);

			ScrollWindowToTile(this->town->xy, this, true); // Re-center viewport.
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		/* Called when setting station noise or required cargoes have changed, in order to resize the window */
		this->SetDirty(); // refresh display for current size. This will allow to avoid glitches when downgrading
		this->ResizeWindowAsNeeded();
	}

	void OnQueryTextFinished(char *str) override
	{
		if (str == nullptr) return;

		Command<CMD_RENAME_TOWN>::Post(STR_ERROR_CAN_T_RENAME_TOWN, this->window_number, str);
	}

	IntervalTimer<TimerGameCalendar> daily_interval = {{TimerGameCalendar::DAY, TimerGameCalendar::Priority::NONE}, [this](auto) {
		/* Refresh after possible snowline change */
		this->SetDirty();
	}};
};

static const NWidgetPart _nested_town_game_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_TV_CHANGE_NAME), SetMinimalSize(12, 14), SetDataTip(SPR_RENAME, STR_TOWN_VIEW_RENAME_TOOLTIP),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_TV_CAPTION), SetDataTip(STR_TOWN_VIEW_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_TV_CENTER_VIEW), SetMinimalSize(12, 14), SetDataTip(SPR_GOTO_LOCATION, STR_TOWN_VIEW_CENTER_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(WWT_INSET, COLOUR_BROWN), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, INVALID_COLOUR, WID_TV_VIEWPORT), SetMinimalSize(254, 86), SetFill(1, 0), SetResize(1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TV_INFO), SetMinimalSize(260, 32), SetResize(1, 0), SetFill(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_SHOW_AUTHORITY), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TOWN_VIEW_LOCAL_AUTHORITY_BUTTON, STR_TOWN_VIEW_LOCAL_AUTHORITY_TOOLTIP),
		NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TV_CATCHMENT), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_BUTTON_CATCHMENT, STR_TOOLTIP_CATCHMENT),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

static WindowDesc _town_game_view_desc(__FILE__, __LINE__,
	WDP_AUTO, "view_town", 260, TownViewWindow::WID_TV_HEIGHT_NORMAL,
	WC_TOWN_VIEW, WC_NONE,
	0,
	std::begin(_nested_town_game_view_widgets), std::end(_nested_town_game_view_widgets)
);

static const NWidgetPart _nested_town_editor_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_TV_CHANGE_NAME), SetMinimalSize(12, 14), SetDataTip(SPR_RENAME, STR_TOWN_VIEW_RENAME_TOOLTIP),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_TV_CAPTION), SetDataTip(STR_TOWN_VIEW_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_TV_CENTER_VIEW), SetMinimalSize(12, 14), SetDataTip(SPR_GOTO_LOCATION, STR_TOWN_VIEW_CENTER_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(WWT_INSET, COLOUR_BROWN), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, INVALID_COLOUR, WID_TV_VIEWPORT), SetMinimalSize(254, 86), SetFill(1, 1), SetResize(1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TV_INFO), SetMinimalSize(260, 32), SetResize(1, 0), SetFill(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_EXPAND), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TOWN_VIEW_EXPAND_BUTTON, STR_TOWN_VIEW_EXPAND_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_DELETE), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TOWN_VIEW_DELETE_BUTTON, STR_TOWN_VIEW_DELETE_TOOLTIP),
		NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TV_CATCHMENT), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_BUTTON_CATCHMENT, STR_TOOLTIP_CATCHMENT),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

static WindowDesc _town_editor_view_desc(__FILE__, __LINE__,
	WDP_AUTO, "view_town_scen", 260, TownViewWindow::WID_TV_HEIGHT_NORMAL,
	WC_TOWN_VIEW, WC_NONE,
	0,
	std::begin(_nested_town_editor_view_widgets), std::end(_nested_town_editor_view_widgets)
);

void ShowTownViewWindow(TownID town)
{
	if (_game_mode == GM_EDITOR) {
		AllocateWindowDescFront<TownViewWindow>(&_town_editor_view_desc, town);
	} else {
		AllocateWindowDescFront<TownViewWindow>(&_town_game_view_desc, town);
	}
}

static const NWidgetPart _nested_town_directory_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_TOWN_DIRECTORY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TD_SORT_ORDER), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
				NWidget(WWT_DROPDOWN, COLOUR_BROWN, WID_TD_SORT_CRITERIA), SetDataTip(STR_JUST_STRING, STR_TOOLTIP_SORT_CRITERIA),
				NWidget(WWT_EDITBOX, COLOUR_BROWN, WID_TD_FILTER), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_BROWN, WID_TD_LIST), SetDataTip(0x0, STR_TOWN_DIRECTORY_LIST_TOOLTIP),
							SetFill(1, 0), SetResize(1, 1), SetScrollbar(WID_TD_SCROLLBAR), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_BROWN),
				NWidget(WWT_TEXT, COLOUR_BROWN, WID_TD_WORLD_POPULATION), SetPadding(2, 0, 2, 2), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TOWN_POPULATION, STR_NULL),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_TD_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

/** Enum referring to the Hotkeys in the town directory window */
enum TownDirectoryHotkeys {
	TDHK_FOCUS_FILTER_BOX, ///< Focus the filter box
};

/** Town directory window class. */
struct TownDirectoryWindow : public Window {
private:
	/* Runtime saved values */
	static Listing last_sorting;

	/* Constants for sorting towns */
	static const StringID sorter_names[];
	static GUITownList::SortFunction * const sorter_funcs[];

	StringFilter string_filter;             ///< Filter for towns
	QueryString townname_editbox;           ///< Filter editbox

	GUITownList towns{TownDirectoryWindow::last_sorting.order};

	Scrollbar *vscroll;

	void BuildSortTownList()
	{
		if (this->towns.NeedRebuild()) {
			this->towns.clear();

			for (const Town *t : Town::Iterate()) {
				if (this->string_filter.IsEmpty()) {
					this->towns.push_back(t);
					continue;
				}
				this->string_filter.ResetState();
				this->string_filter.AddLine(t->GetCachedName());
				if (this->string_filter.GetState()) this->towns.push_back(t);
			}

			this->towns.shrink_to_fit();
			this->towns.RebuildDone();
			this->vscroll->SetCount(this->towns.size()); // Update scrollbar as well.
		}
		/* Always sort the towns. */
		this->towns.Sort();
		this->SetWidgetDirty(WID_TD_LIST); // Force repaint of the displayed towns.
	}

	/** Sort by town name */
	static bool TownNameSorter(const Town * const &a, const Town * const &b, const bool &)
	{
		return StrNaturalCompare(a->GetCachedName(), b->GetCachedName()) < 0; // Sort by name (natural sorting).
	}

	/** Sort by population (default descending, as big towns are of the most interest). */
	static bool TownPopulationSorter(const Town * const &a, const Town * const &b, const bool &order)
	{
		uint32_t a_population = a->cache.population;
		uint32_t b_population = b->cache.population;
		if (a_population == b_population) return TownDirectoryWindow::TownNameSorter(a, b, order);
		return a_population < b_population;
	}

	/** Sort by town rating */
	static bool TownRatingSorter(const Town * const &a, const Town * const &b, const bool &order)
	{
		bool before = !order; // Value to get 'a' before 'b'.

		/* Towns without rating are always after towns with rating. */
		if (HasBit(a->have_ratings, _local_company)) {
			if (HasBit(b->have_ratings, _local_company)) {
				int16_t a_rating = a->ratings[_local_company];
				int16_t b_rating = b->ratings[_local_company];
				if (a_rating == b_rating) return TownDirectoryWindow::TownNameSorter(a, b, order);
				return a_rating < b_rating;
			}
			return before;
		}
		if (HasBit(b->have_ratings, _local_company)) return !before;

		/* Sort unrated towns always on ascending town name. */
		if (before) return TownDirectoryWindow::TownNameSorter(a, b, order);
		return TownDirectoryWindow::TownNameSorter(b, a, order);
	}

public:
	TownDirectoryWindow(WindowDesc *desc) : Window(desc), townname_editbox(MAX_LENGTH_TOWN_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_TOWN_NAME_CHARS)
	{
		this->CreateNestedTree();

		this->vscroll = this->GetScrollbar(WID_TD_SCROLLBAR);

		this->towns.SetListing(this->last_sorting);
		this->towns.SetSortFuncs(TownDirectoryWindow::sorter_funcs);
		this->towns.ForceRebuild();
		this->BuildSortTownList();

		this->FinishInitNested(0);

		this->querystrings[WID_TD_FILTER] = &this->townname_editbox;
		this->townname_editbox.cancel_button = QueryString::ACTION_CLEAR;
	}

	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case WID_TD_WORLD_POPULATION:
				SetDParam(0, GetWorldPopulation());
				break;

			case WID_TD_SORT_CRITERIA:
				SetDParam(0, TownDirectoryWindow::sorter_names[this->towns.SortType()]);
				break;
		}
	}

	/**
	 * Get the string to draw the town name.
	 * @param t Town to draw.
	 * @return The string to use.
	 */
	static StringID GetTownString(const Town *t)
	{
		return t->larger_town ? STR_TOWN_DIRECTORY_CITY : STR_TOWN_DIRECTORY_TOWN;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_TD_SORT_ORDER:
				this->DrawSortButtonState(widget, this->towns.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;

			case WID_TD_LIST: {
				int n = 0;
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				if (this->towns.empty()) { // No towns available.
					DrawString(tr, STR_TOWN_DIRECTORY_NONE);
					break;
				}

				/* At least one town available. */
				bool rtl = _current_text_dir == TD_RTL;
				Dimension icon_size = GetSpriteSize(SPR_TOWN_RATING_GOOD);
				int icon_x = tr.WithWidth(icon_size.width, rtl).left;
				tr = tr.Indent(icon_size.width + WidgetDimensions::scaled.hsep_normal, rtl);

				for (uint i = this->vscroll->GetPosition(); i < this->towns.size(); i++) {
					const Town *t = this->towns[i];
					assert(t->xy != INVALID_TILE);

					/* Draw rating icon. */
					if (_game_mode == GM_EDITOR || !HasBit(t->have_ratings, _local_company)) {
						DrawSprite(SPR_TOWN_RATING_NA, PAL_NONE, icon_x, tr.top + (this->resize.step_height - icon_size.height) / 2);
					} else {
						SpriteID icon = SPR_TOWN_RATING_APALLING;
						if (t->ratings[_local_company] > RATING_VERYPOOR) icon = SPR_TOWN_RATING_MEDIOCRE;
						if (t->ratings[_local_company] > RATING_GOOD)     icon = SPR_TOWN_RATING_GOOD;
						DrawSprite(icon, PAL_NONE, icon_x, tr.top + (this->resize.step_height - icon_size.height) / 2);
					}

					SetDParam(0, t->index);
					SetDParam(1, t->cache.population);
					DrawString(tr.left, tr.right, tr.top + (this->resize.step_height - GetCharacterHeight(FS_NORMAL)) / 2, GetTownString(t));

					tr.top += this->resize.step_height;
					if (++n == this->vscroll->GetCapacity()) break; // max number of towns in 1 window
				}
				break;
			}
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_TD_SORT_ORDER: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
			case WID_TD_SORT_CRITERIA: {
				Dimension d = {0, 0};
				for (uint i = 0; TownDirectoryWindow::sorter_names[i] != INVALID_STRING_ID; i++) {
					d = maxdim(d, GetStringBoundingBox(TownDirectoryWindow::sorter_names[i]));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
			case WID_TD_LIST: {
				Dimension d = GetStringBoundingBox(STR_TOWN_DIRECTORY_NONE);
				for (uint i = 0; i < this->towns.size(); i++) {
					const Town *t = this->towns[i];

					assert(t != nullptr);

					SetDParam(0, t->index);
					SetDParamMaxDigits(1, 8);
					d = maxdim(d, GetStringBoundingBox(GetTownString(t)));
				}
				Dimension icon_size = GetSpriteSize(SPR_TOWN_RATING_GOOD);
				d.width += icon_size.width + 2;
				d.height = std::max(d.height, icon_size.height);
				resize->height = d.height;
				d.height *= 5;
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
			case WID_TD_WORLD_POPULATION: {
				SetDParamMaxDigits(0, 10);
				Dimension d = GetStringBoundingBox(STR_TOWN_POPULATION);
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_TD_SORT_ORDER: // Click on sort order button
				if (this->towns.SortType() != 2) { // A different sort than by rating.
					this->towns.ToggleSortOrder();
					this->last_sorting = this->towns.GetListing(); // Store new sorting order.
				} else {
					/* Some parts are always sorted ascending on name. */
					this->last_sorting.order = !this->last_sorting.order;
					this->towns.SetListing(this->last_sorting);
					this->towns.ForceResort();
					this->towns.Sort();
				}
				this->SetDirty();
				break;

			case WID_TD_SORT_CRITERIA: // Click on sort criteria dropdown
				ShowDropDownMenu(this, TownDirectoryWindow::sorter_names, this->towns.SortType(), WID_TD_SORT_CRITERIA, 0, 0);
				break;

			case WID_TD_LIST: { // Click on Town Matrix
				auto it = this->vscroll->GetScrolledItemFromWidget(this->towns, pt.y, this, WID_TD_LIST, WidgetDimensions::scaled.framerect.top);
				if (it == this->towns.end()) return; // click out of town bounds

				const Town *t = *it;
				assert(t != nullptr);
				if (_ctrl_pressed) {
					ShowExtraViewportWindow(t->xy);
				} else {
					ScrollMainWindowToTile(t->xy);
				}
				break;
			}
		}
	}

	void OnDropdownSelect(WidgetID widget, int index) override
	{
		if (widget != WID_TD_SORT_CRITERIA) return;

		if (this->towns.SortType() != index) {
			this->towns.SetSortType(index);
			this->last_sorting = this->towns.GetListing(); // Store new sorting order.
			this->BuildSortTownList();
		}
	}

	void OnPaint() override
	{
		if (this->towns.NeedRebuild()) this->BuildSortTownList();
		this->DrawWidgets();
	}

	/** Redraw the whole window on a regular interval. */
	IntervalTimer<TimerWindow> rebuild_interval = {std::chrono::seconds(3), [this](auto) {
		this->BuildSortTownList();
		this->SetDirty();
	}};

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_TD_LIST);
	}

	void OnEditboxChanged(WidgetID wid) override
	{
		if (wid == WID_TD_FILTER) {
			this->string_filter.SetFilterTerm(this->townname_editbox.text.buf);
			this->InvalidateData(TDIWD_FORCE_REBUILD);
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		switch (data) {
			case TDIWD_FORCE_REBUILD:
				/* This needs to be done in command-scope to enforce rebuilding before resorting invalid data */
				this->towns.ForceRebuild();
				break;

			case TDIWD_POPULATION_CHANGE:
				if (this->towns.SortType() == 1) this->towns.ForceResort();
				break;

			default:
				this->towns.ForceResort();
		}
	}

	EventState OnHotkey(int hotkey) override
	{
		switch (hotkey) {
			case TDHK_FOCUS_FILTER_BOX:
				this->SetFocusedWidget(WID_TD_FILTER);
				SetFocusedWindow(this); // The user has asked to give focus to the text box, so make sure this window is focused.
				break;
			default:
				return ES_NOT_HANDLED;
		}
		return ES_HANDLED;
	}

	static inline HotkeyList hotkeys {"towndirectory", {
		Hotkey('F', "focus_filter_box", TDHK_FOCUS_FILTER_BOX),
	}};
};

Listing TownDirectoryWindow::last_sorting = {false, 0};

/** Names of the sorting functions. */
const StringID TownDirectoryWindow::sorter_names[] = {
	STR_SORT_BY_NAME,
	STR_SORT_BY_POPULATION,
	STR_SORT_BY_RATING,
	INVALID_STRING_ID
};

/** Available town directory sorting functions. */
GUITownList::SortFunction * const TownDirectoryWindow::sorter_funcs[] = {
	&TownNameSorter,
	&TownPopulationSorter,
	&TownRatingSorter,
};

static WindowDesc _town_directory_desc(__FILE__, __LINE__,
	WDP_AUTO, "list_towns", 208, 202,
	WC_TOWN_DIRECTORY, WC_NONE,
	0,
	std::begin(_nested_town_directory_widgets), std::end(_nested_town_directory_widgets),
	&TownDirectoryWindow::hotkeys
);

void ShowTownDirectory()
{
	if (BringWindowToFrontById(WC_TOWN_DIRECTORY, 0)) return;
	new TownDirectoryWindow(&_town_directory_desc);
}

void CcFoundTown(Commands, const CommandCost &result, TileIndex tile)
{
	if (result.Failed()) return;

	if (_settings_client.sound.confirm) SndPlayTileFx(SND_1F_CONSTRUCTION_OTHER, tile);
	if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
}

void CcFoundRandomTown(Commands, const CommandCost &result, Money, TownID town_id)
{
	if (result.Succeeded()) ScrollMainWindowToTile(Town::Get(town_id)->xy);
}

static const NWidgetPart _nested_found_town_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_FOUND_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	/* Construct new town(s) buttons. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0), SetPadding(WidgetDimensions::unscaled.picker),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_NEW_TOWN), SetDataTip(STR_FOUND_TOWN_NEW_TOWN_BUTTON, STR_FOUND_TOWN_NEW_TOWN_TOOLTIP), SetFill(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_RANDOM_TOWN), SetDataTip(STR_FOUND_TOWN_RANDOM_TOWN_BUTTON, STR_FOUND_TOWN_RANDOM_TOWN_TOOLTIP), SetFill(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_MANY_RANDOM_TOWNS), SetDataTip(STR_FOUND_TOWN_MANY_RANDOM_TOWNS, STR_FOUND_TOWN_RANDOM_TOWNS_TOOLTIP), SetFill(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_EXPAND_ALL_TOWNS), SetDataTip(STR_FOUND_TOWN_EXPAND_ALL_TOWNS, STR_FOUND_TOWN_EXPAND_ALL_TOWNS_TOOLTIP), SetFill(1, 0),

			/* Town name selection. */
			NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_FOUND_TOWN_NAME_TITLE, STR_NULL),
			NWidget(WWT_EDITBOX, COLOUR_GREY, WID_TF_TOWN_NAME_EDITBOX), SetDataTip(STR_FOUND_TOWN_NAME_EDITOR_TITLE, STR_FOUND_TOWN_NAME_EDITOR_HELP), SetFill(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_TOWN_NAME_RANDOM), SetDataTip(STR_FOUND_TOWN_NAME_RANDOM_BUTTON, STR_FOUND_TOWN_NAME_RANDOM_TOOLTIP), SetFill(1, 0),

			/* Town size selection. */
			NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_TITLE, STR_NULL),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_SMALL), SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_SMALL_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP), SetFill(1, 0),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_MEDIUM), SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_MEDIUM_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_LARGE), SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_LARGE_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP), SetFill(1, 0),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_RANDOM), SetDataTip(STR_FOUND_TOWN_SIZE_RANDOM, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP), SetFill(1, 0),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_CITY), SetDataTip(STR_FOUND_TOWN_CITY, STR_FOUND_TOWN_CITY_TOOLTIP), SetFill(1, 0),

			/* Town roads selection. */
			NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_FOUND_TOWN_ROAD_LAYOUT, STR_NULL),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_ORIGINAL), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_ORIGINAL, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT), SetFill(1, 0),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_BETTER), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_BETTER_ROADS, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_GRID2), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_2X2_GRID, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT), SetFill(1, 0),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_GRID3), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_3X3_GRID, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT), SetFill(1, 0),
				EndContainer(),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_RANDOM), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_RANDOM, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** Found a town window class. */
struct FoundTownWindow : Window {
private:
	TownSize town_size;     ///< Selected town size
	TownLayout town_layout; ///< Selected town layout
	bool city;              ///< Are we building a city?
	QueryString townname_editbox; ///< Townname editbox
	bool townnamevalid;     ///< Is generated town name valid?
	uint32_t townnameparts;   ///< Generated town name
	TownNameParams params;  ///< Town name parameters

public:
	FoundTownWindow(WindowDesc *desc, WindowNumber window_number) :
			Window(desc),
			town_size(TSZ_MEDIUM),
			town_layout(_settings_game.economy.town_layout),
			townname_editbox(MAX_LENGTH_TOWN_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_TOWN_NAME_CHARS),
			params(_settings_game.game_creation.town_name)
	{
		this->InitNested(window_number);
		this->querystrings[WID_TF_TOWN_NAME_EDITBOX] = &this->townname_editbox;
		this->RandomTownName();
		this->UpdateButtons(true);
	}

	void RandomTownName()
	{
		this->townnamevalid = GenerateTownName(_interactive_random, &this->townnameparts);

		if (!this->townnamevalid) {
			this->townname_editbox.text.DeleteAll();
		} else {
			this->townname_editbox.text.Assign(GetTownName(&this->params, this->townnameparts));
		}
		UpdateOSKOriginalText(this, WID_TF_TOWN_NAME_EDITBOX);

		this->SetWidgetDirty(WID_TF_TOWN_NAME_EDITBOX);
	}

	void UpdateButtons(bool check_availability)
	{
		if (check_availability && _game_mode != GM_EDITOR) {
			this->SetWidgetsDisabledState(true, WID_TF_RANDOM_TOWN, WID_TF_MANY_RANDOM_TOWNS, WID_TF_EXPAND_ALL_TOWNS, WID_TF_SIZE_LARGE);
			this->SetWidgetsDisabledState(_settings_game.economy.found_town != TF_CUSTOM_LAYOUT,
					WID_TF_LAYOUT_ORIGINAL, WID_TF_LAYOUT_BETTER, WID_TF_LAYOUT_GRID2, WID_TF_LAYOUT_GRID3, WID_TF_LAYOUT_RANDOM);
			if (_settings_game.economy.found_town != TF_CUSTOM_LAYOUT) town_layout = _settings_game.economy.town_layout;
		}

		for (WidgetID i = WID_TF_SIZE_SMALL; i <= WID_TF_SIZE_RANDOM; i++) {
			this->SetWidgetLoweredState(i, i == WID_TF_SIZE_SMALL + this->town_size);
		}

		this->SetWidgetLoweredState(WID_TF_CITY, this->city);

		for (WidgetID i = WID_TF_LAYOUT_ORIGINAL; i <= WID_TF_LAYOUT_RANDOM; i++) {
			this->SetWidgetLoweredState(i, i == WID_TF_LAYOUT_ORIGINAL + this->town_layout);
		}

		this->SetDirty();
	}

	template <typename Tcallback>
	void ExecuteFoundTownCommand(TileIndex tile, bool random, StringID errstr, Tcallback cc)
	{
		std::string name;

		if (!this->townnamevalid) {
			name = this->townname_editbox.text.buf;
		} else {
			/* If user changed the name, send it */
			std::string original_name = GetTownName(&this->params, this->townnameparts);
			if (original_name != this->townname_editbox.text.buf) name = this->townname_editbox.text.buf;
		}

		bool success = Command<CMD_FOUND_TOWN>::Post(errstr, cc,
				tile, this->town_size, this->city, this->town_layout, random, townnameparts, name);

		/* Rerandomise name, if success and no cost-estimation. */
		if (success && !_shift_pressed) this->RandomTownName();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_TF_NEW_TOWN:
				HandlePlacePushButton(this, WID_TF_NEW_TOWN, SPR_CURSOR_TOWN, HT_RECT);
				break;

			case WID_TF_RANDOM_TOWN:
				this->ExecuteFoundTownCommand(0, true, STR_ERROR_CAN_T_GENERATE_TOWN, CcFoundRandomTown);
				break;

			case WID_TF_TOWN_NAME_RANDOM:
				this->RandomTownName();
				this->SetFocusedWidget(WID_TF_TOWN_NAME_EDITBOX);
				break;

			case WID_TF_MANY_RANDOM_TOWNS: {
				Backup<bool> old_generating_world(_generating_world, true, FILE_LINE);
				UpdateNearestTownForRoadTiles(true);
				if (!GenerateTowns(this->town_layout)) {
					ShowErrorMessage(STR_ERROR_CAN_T_GENERATE_TOWN, STR_ERROR_NO_SPACE_FOR_TOWN, WL_INFO);
				}
				UpdateNearestTownForRoadTiles(false);
				old_generating_world.Restore();
				break;
			}

			case WID_TF_EXPAND_ALL_TOWNS:
				for (Town *t : Town::Iterate()) {
					Command<CMD_EXPAND_TOWN>::Do(DC_EXEC, t->index, 0);
				}
				break;

			case WID_TF_SIZE_SMALL: case WID_TF_SIZE_MEDIUM: case WID_TF_SIZE_LARGE: case WID_TF_SIZE_RANDOM:
				this->town_size = (TownSize)(widget - WID_TF_SIZE_SMALL);
				this->UpdateButtons(false);
				break;

			case WID_TF_CITY:
				this->city ^= true;
				this->SetWidgetLoweredState(WID_TF_CITY, this->city);
				this->SetDirty();
				break;

			case WID_TF_LAYOUT_ORIGINAL: case WID_TF_LAYOUT_BETTER: case WID_TF_LAYOUT_GRID2:
			case WID_TF_LAYOUT_GRID3: case WID_TF_LAYOUT_RANDOM:
				this->town_layout = (TownLayout)(widget - WID_TF_LAYOUT_ORIGINAL);
				this->UpdateButtons(false);
				break;
		}
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		this->ExecuteFoundTownCommand(tile, false, STR_ERROR_CAN_T_FOUND_TOWN_HERE, CcFoundTown);
	}

	void OnPlaceObjectAbort() override
	{
		this->RaiseButtons();
		this->UpdateButtons(false);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->UpdateButtons(true);
	}
};

static WindowDesc _found_town_desc(__FILE__, __LINE__,
	WDP_AUTO, "build_town", 160, 162,
	WC_FOUND_TOWN, WC_NONE,
	WDF_CONSTRUCTION,
	std::begin(_nested_found_town_widgets), std::end(_nested_found_town_widgets)
);

void ShowFoundTownWindow()
{
	if (_game_mode != GM_EDITOR && !Company::IsValidID(_local_company)) return;
	AllocateWindowDescFront<FoundTownWindow>(&_found_town_desc, 0);
}

void InitializeTownGui()
{
	_town_local_authority_kdtree.Clear();
}
