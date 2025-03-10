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
#include "house.h"
#include "newgrf_cargo.h"
#include "newgrf_house.h"
#include "newgrf_text.h"
#include "picker_gui.h"
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
#include "fios.h"
#include "stringfilter_type.h"
#include "dropdown_func.h"
#include "town_kdtree.h"
#include "town_cmd.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_window.h"
#include "zoom_func.h"
#include "hotkeys.h"

#include "widgets/town_widget.h"

#include "table/strings.h"

#include <sstream>

#include "safeguards.h"

TownKdtree _town_local_authority_kdtree{};

typedef GUIList<const Town*, const bool &> GUITownList;

static constexpr NWidgetPart _nested_town_authority_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_TA_CAPTION),
		NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TA_ZONE_BUTTON), SetMinimalSize(50, 0), SetStringTip(STR_LOCAL_AUTHORITY_ZONE, STR_LOCAL_AUTHORITY_ZONE_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TA_RATING_INFO), SetMinimalSize(317, 92), SetResize(1, 1), EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TA_COMMAND_LIST), SetMinimalSize(317, 52), SetResize(1, 0), SetToolTip(STR_LOCAL_AUTHORITY_ACTIONS_TOOLTIP), EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_TA_ACTION_INFO), SetMinimalSize(317, 52), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TA_EXECUTE),  SetMinimalSize(317, 12), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_LOCAL_AUTHORITY_DO_IT_BUTTON, STR_LOCAL_AUTHORITY_DO_IT_TOOLTIP),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer()
};

/** Town authority window. */
struct TownAuthorityWindow : Window {
private:
	Town *town = nullptr; ///< Town being displayed.
	TownAction sel_action = TownAction::End; ///< Currently selected town action, TownAction::End means no action selected.
	TownActions displayed_actions_on_previous_painting{}; ///< Actions that were available on the previous call to OnPaint()
	TownActions enabled_actions{}; ///< Actions that are enabled in settings.
	TownActions available_actions{}; ///< Actions that are available to execute for the current company.
	std::array<StringID, to_underlying(TownAction::End)> action_tooltips{};

	Dimension icon_size{}; ///< Dimensions of company icon
	Dimension exclusive_size{}; ///< Dimensions of exclusive icon

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
			for (uint i : SetBitIterator(this->enabled_actions.base())) {
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
		TownActions enabled{};
		enabled.Set();

		if (!_settings_game.economy.fund_roads) enabled.Reset(TownAction::RoadRebuild);
		if (!_settings_game.economy.fund_buildings) enabled.Reset(TownAction::FundBuildings);
		if (!_settings_game.economy.exclusive_rights) enabled.Reset(TownAction::BuyRights);
		if (!_settings_game.economy.bribe) enabled.Reset(TownAction::Bribe);

		return enabled;
	}

public:
	TownAuthorityWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->town = Town::Get(window_number);
		this->enabled_actions = GetEnabledActions();

		auto realtime = TimerGameEconomy::UsingWallclockUnits();
		this->action_tooltips[0] = STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_SMALL_ADVERTISING;
		this->action_tooltips[1] = STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_MEDIUM_ADVERTISING;
		this->action_tooltips[2] = STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_LARGE_ADVERTISING;
		this->action_tooltips[3] = realtime ? STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_ROAD_RECONSTRUCTION_MINUTES : STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_ROAD_RECONSTRUCTION_MONTHS;
		this->action_tooltips[4] = STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_STATUE_OF_COMPANY;
		this->action_tooltips[5] = STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_NEW_BUILDINGS;
		this->action_tooltips[6] = realtime ? STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_EXCLUSIVE_TRANSPORT_MINUTES : STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_EXCLUSIVE_TRANSPORT_MONTHS;
		this->action_tooltips[7] = STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_BRIBE;

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
		this->SetWidgetDisabledState(WID_TA_EXECUTE, (this->sel_action == TownAction::End) || !this->available_actions.Test(this->sel_action));

		this->DrawWidgets();
		if (!this->IsShaded())
		{
			this->DrawRatings();
			this->DrawActions();
		}
	}

	StringID GetRatingString(int rating) const
	{
		if (rating > RATING_EXCELLENT) return STR_CARGO_RATING_OUTSTANDING;
		if (rating > RATING_VERYGOOD)  return STR_CARGO_RATING_EXCELLENT;
		if (rating > RATING_GOOD)      return STR_CARGO_RATING_VERY_GOOD;
		if (rating > RATING_MEDIOCRE)  return STR_CARGO_RATING_GOOD;
		if (rating > RATING_POOR)      return STR_CARGO_RATING_MEDIOCRE;
		if (rating > RATING_VERYPOOR)  return STR_CARGO_RATING_POOR;
		if (rating > RATING_APPALLING) return STR_CARGO_RATING_VERY_POOR;
		return STR_CARGO_RATING_APPALLING;
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
			if ((this->town->have_ratings.Test(c->index) || this->town->exclusivity == c->index)) {
				DrawCompanyIcon(c->index, icon.left, text.top + icon_y_offset);

				if (this->town->exclusivity == c->index) {
					DrawSprite(SPR_EXCLUSIVE_TRANSPORT, COMPANY_SPRITE_COLOUR(c->index), exclusive.left, text.top + exclusive_y_offset);
				}

				int rating = this->town->ratings[c->index];
				DrawString(text.left, text.right, text.top + text_y_offset, GetString(STR_LOCAL_AUTHORITY_COMPANY_RATING, c->index, c->index, GetRatingString(rating)));
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
		for (TownAction i = {}; i != TownAction::End; ++i) {
			/* Don't show actions if disabled in settings. */
			if (!this->enabled_actions.Test(i)) continue;

			/* Set colour of action based on ability to execute and if selected. */
			TextColour action_colour = TC_GREY | TC_NO_SHADE;
			if (this->available_actions.Test(i)) action_colour = TC_ORANGE;
			if (this->sel_action == i) action_colour = TC_WHITE;

			DrawString(r, STR_LOCAL_AUTHORITY_ACTION_SMALL_ADVERTISING_CAMPAIGN + to_underlying(i), action_colour);
			r.top += GetCharacterHeight(FS_NORMAL);
		}
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_TA_CAPTION) return GetString(STR_LOCAL_AUTHORITY_CAPTION, this->window_number);

		return this->Window::GetWidgetString(widget, stringid);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_TA_ACTION_INFO:
				if (this->sel_action != TownAction::End) {
					Money action_cost = _price[PR_TOWN_ACTION] * GetTownActionCost(this->sel_action) >> 8;
					bool affordable = Company::IsValidID(_local_company) && action_cost < GetAvailableMoney(_local_company);

					DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.framerect),
						GetString(this->action_tooltips[to_underlying(this->sel_action)], action_cost),
						affordable ? TC_YELLOW : TC_RED);
				}
				break;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_TA_ACTION_INFO: {
				assert(size.width > padding.width && size.height > padding.height);
				Dimension d = {0, 0};
				for (TownAction i = {}; i != TownAction::End; ++i) {
					Money price = _price[PR_TOWN_ACTION] * GetTownActionCost(i) >> 8;
					d = maxdim(d, GetStringMultiLineBoundingBox(GetString(this->action_tooltips[to_underlying(i)], price), size));
				}
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}

			case WID_TA_COMMAND_LIST:
				size.height = (to_underlying(TownAction::End) + 1) * GetCharacterHeight(FS_NORMAL) + padding.height;
				size.width = GetStringBoundingBox(STR_LOCAL_AUTHORITY_ACTIONS_TITLE).width;
				for (TownAction i = {}; i != TownAction::End; ++i) {
					size.width = std::max(size.width, GetStringBoundingBox(STR_LOCAL_AUTHORITY_ACTION_SMALL_ADVERTISING_CAMPAIGN + to_underlying(i)).width + padding.width);
				}
				size.width += padding.width;
				break;

			case WID_TA_RATING_INFO:
				resize.height = std::max({this->icon_size.height + WidgetDimensions::scaled.vsep_normal, this->exclusive_size.height + WidgetDimensions::scaled.vsep_normal, (uint)GetCharacterHeight(FS_NORMAL)});
				size.height = 9 * resize.height + padding.height;
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
					this->sel_action = static_cast<TownAction>(y);
					this->SetDirty();
				}

				/* When double-clicking, continue */
				if (click_count == 1 || y < 0 || !this->available_actions.Test(this->sel_action)) break;
				[[fallthrough]];
			}

			case WID_TA_EXECUTE:
				Command<CMD_DO_TOWN_ACTION>::Post(STR_ERROR_CAN_T_DO_THIS, this->town->xy, static_cast<TownID>(this->window_number), this->sel_action);
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
		if (!this->enabled_actions.Test(this->sel_action)) {
			this->sel_action = TownAction::End;
		}
	}
};

static WindowDesc _town_authority_desc(
	WDP_AUTO, "view_town_authority", 317, 222,
	WC_TOWN_AUTHORITY, WC_NONE,
	{},
	_nested_town_authority_widgets
);

static void ShowTownAuthorityWindow(uint town)
{
	AllocateWindowDescFront<TownAuthorityWindow>(_town_authority_desc, town);
}


/* Town view window. */
struct TownViewWindow : Window {
private:
	Town *town = nullptr; ///< Town displayed by the window.

public:
	static const int WID_TV_HEIGHT_NORMAL = 150;

	TownViewWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();

		this->town = Town::Get(window_number);
		if (this->town->larger_town) this->GetWidget<NWidgetCore>(WID_TV_CAPTION)->SetString(STR_TOWN_VIEW_CITY_CAPTION);

		this->FinishInitNested(window_number);

		this->flags.Set(WindowFlag::DisableVpScroll);
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

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_TV_CAPTION) return GetString(STR_TOWN_VIEW_TOWN_CAPTION, this->town->index);

		return this->Window::GetWidgetString(widget, stringid);
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

		DrawString(tr, GetString(STR_TOWN_VIEW_POPULATION_HOUSES, this->town->cache.population, this->town->cache.num_houses));
		tr.top += GetCharacterHeight(FS_NORMAL);

		StringID str_last_period = TimerGameEconomy::UsingWallclockUnits() ? STR_TOWN_VIEW_CARGO_LAST_MINUTE_MAX : STR_TOWN_VIEW_CARGO_LAST_MONTH_MAX;

		for (auto tpe : {TPE_PASSENGERS, TPE_MAIL}) {
			for (const CargoSpec *cs : CargoSpec::town_production_cargoes[tpe]) {
				CargoType cargo_type = cs->Index();
				DrawString(tr, GetString(str_last_period, 1ULL << cargo_type, this->town->supplied[cargo_type].old_act, this->town->supplied[cargo_type].old_max));
				tr.top += GetCharacterHeight(FS_NORMAL);
			}
		}

		bool first = true;
		for (int i = TAE_BEGIN; i < TAE_END; i++) {
			if (this->town->goal[i] == 0) continue;
			if (this->town->goal[i] == TOWN_GROWTH_WINTER && (TileHeight(this->town->xy) < LowestSnowLine() || this->town->cache.population <= 90)) continue;
			if (this->town->goal[i] == TOWN_GROWTH_DESERT && (GetTropicZone(this->town->xy) != TROPICZONE_DESERT || this->town->cache.population <= 60)) continue;

			if (first) {
				DrawString(tr, STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH);
				tr.top += GetCharacterHeight(FS_NORMAL);
				first = false;
			}

			bool rtl = _current_text_dir == TD_RTL;

			const CargoSpec *cargo = FindFirstCargoWithTownAcceptanceEffect((TownAcceptanceEffect)i);
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

				DrawString(tr.Indent(20, rtl), GetString(string, cargo->name));
			} else {
				string = STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_DELIVERED;
				if (this->town->received[i].old_act < this->town->goal[i]) {
					string = STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_REQUIRED;
				}
				DrawString(tr.Indent(20, rtl), GetString(string, cargo->Index(), this->town->received[i].old_act, cargo->Index(), this->town->goal[i]));
			}
			tr.top += GetCharacterHeight(FS_NORMAL);
		}

		if (HasBit(this->town->flags, TOWN_IS_GROWING)) {
			DrawString(tr, GetString(this->town->fund_buildings_months == 0 ? STR_TOWN_VIEW_TOWN_GROWS_EVERY : STR_TOWN_VIEW_TOWN_GROWS_EVERY_FUNDED, RoundDivSU(this->town->growth_rate + 1, Ticks::DAY_TICKS)));
			tr.top += GetCharacterHeight(FS_NORMAL);
		} else {
			DrawString(tr, STR_TOWN_VIEW_TOWN_GROW_STOPPED);
			tr.top += GetCharacterHeight(FS_NORMAL);
		}

		/* only show the town noise, if the noise option is activated. */
		if (_settings_game.economy.station_noise_level) {
			DrawString(tr, GetString(STR_TOWN_VIEW_NOISE_IN_TOWN, this->town->noise_reached, this->town->MaxTownNoise()));
			tr.top += GetCharacterHeight(FS_NORMAL);
		}

		if (!this->town->text.empty()) {
			tr.top = DrawStringMultiLine(tr, this->town->text.GetDecodedString(), TC_BLACK);
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
				ShowQueryString(GetString(STR_TOWN_NAME, this->window_number), STR_TOWN_VIEW_RENAME_TOWN_BUTTON, MAX_LENGTH_TOWN_NAME_CHARS, this, CS_ALPHANUMERAL, {QueryStringFlag::EnableDefault, QueryStringFlag::LengthIsInChars});
				break;

			case WID_TV_CATCHMENT:
				SetViewportCatchmentTown(Town::Get(this->window_number), !this->IsWidgetLowered(WID_TV_CATCHMENT));
				break;

			case WID_TV_EXPAND: { // expand town - only available on Scenario editor
				Command<CMD_EXPAND_TOWN>::Post(STR_ERROR_CAN_T_EXPAND_TOWN, static_cast<TownID>(this->window_number), 0);
				break;
			}

			case WID_TV_DELETE: // delete town - only available on Scenario editor
				Command<CMD_DELETE_TOWN>::Post(STR_ERROR_TOWN_CAN_T_DELETE, static_cast<TownID>(this->window_number));
				break;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_TV_INFO:
				size.height = GetDesiredInfoHeight(size.width) + padding.height;
				break;
		}
	}

	/**
	 * Gets the desired height for the information panel.
	 * @return the desired height in pixels.
	 */
	uint GetDesiredInfoHeight(int width) const
	{
		uint aimed_height = static_cast<uint>(1 + CargoSpec::town_production_cargoes[TPE_PASSENGERS].size() + CargoSpec::town_production_cargoes[TPE_MAIL].size()) * GetCharacterHeight(FS_NORMAL);

		bool first = true;
		for (int i = TAE_BEGIN; i < TAE_END; i++) {
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
			aimed_height += GetStringHeight(this->town->text.GetDecodedString(), width - WidgetDimensions::scaled.framerect.Horizontal());
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

	void OnMouseWheel(int wheel) override
	{
		if (_settings_client.gui.scrollwheel_scrolling != SWS_OFF) {
			DoZoomInOutWindow(wheel < 0 ? ZOOM_IN : ZOOM_OUT, this);
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

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value()) return;

		Command<CMD_RENAME_TOWN>::Post(STR_ERROR_CAN_T_RENAME_TOWN, static_cast<TownID>(this->window_number), *str);
	}

	IntervalTimer<TimerGameCalendar> daily_interval = {{TimerGameCalendar::DAY, TimerGameCalendar::Priority::NONE}, [this](auto) {
		/* Refresh after possible snowline change */
		this->SetDirty();
	}};
};

static constexpr NWidgetPart _nested_town_game_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_TV_CHANGE_NAME), SetAspect(WidgetDimensions::ASPECT_RENAME), SetSpriteTip(SPR_RENAME, STR_TOWN_VIEW_RENAME_TOOLTIP),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_TV_CAPTION),
		NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_TV_CENTER_VIEW), SetAspect(WidgetDimensions::ASPECT_LOCATION), SetSpriteTip(SPR_GOTO_LOCATION, STR_TOWN_VIEW_CENTER_TOOLTIP),
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
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_SHOW_AUTHORITY), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_TOWN_VIEW_LOCAL_AUTHORITY_BUTTON, STR_TOWN_VIEW_LOCAL_AUTHORITY_TOOLTIP),
		NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TV_CATCHMENT), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_BUTTON_CATCHMENT, STR_TOOLTIP_CATCHMENT),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

static WindowDesc _town_game_view_desc(
	WDP_AUTO, "view_town", 260, TownViewWindow::WID_TV_HEIGHT_NORMAL,
	WC_TOWN_VIEW, WC_NONE,
	{},
	_nested_town_game_view_widgets
);

static constexpr NWidgetPart _nested_town_editor_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_TV_CHANGE_NAME), SetAspect(WidgetDimensions::ASPECT_RENAME), SetSpriteTip(SPR_RENAME, STR_TOWN_VIEW_RENAME_TOOLTIP),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_TV_CAPTION), SetStringTip(STR_TOWN_VIEW_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_TV_CENTER_VIEW), SetAspect(WidgetDimensions::ASPECT_LOCATION), SetSpriteTip(SPR_GOTO_LOCATION, STR_TOWN_VIEW_CENTER_TOOLTIP),
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
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_EXPAND), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_TOWN_VIEW_EXPAND_BUTTON, STR_TOWN_VIEW_EXPAND_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_TV_DELETE), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_TOWN_VIEW_DELETE_BUTTON, STR_TOWN_VIEW_DELETE_TOOLTIP),
		NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TV_CATCHMENT), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_BUTTON_CATCHMENT, STR_TOOLTIP_CATCHMENT),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

static WindowDesc _town_editor_view_desc(
	WDP_AUTO, "view_town_scen", 260, TownViewWindow::WID_TV_HEIGHT_NORMAL,
	WC_TOWN_VIEW, WC_NONE,
	{},
	_nested_town_editor_view_widgets
);

void ShowTownViewWindow(TownID town)
{
	if (_game_mode == GM_EDITOR) {
		AllocateWindowDescFront<TownViewWindow>(_town_editor_view_desc, town);
	} else {
		AllocateWindowDescFront<TownViewWindow>(_town_game_view_desc, town);
	}
}

static constexpr NWidgetPart _nested_town_directory_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_TD_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_TD_SORT_ORDER), SetStringTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
				NWidget(WWT_DROPDOWN, COLOUR_BROWN, WID_TD_SORT_CRITERIA), SetToolTip(STR_TOOLTIP_SORT_CRITERIA),
				NWidget(WWT_EDITBOX, COLOUR_BROWN, WID_TD_FILTER), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_BROWN, WID_TD_LIST), SetToolTip(STR_TOWN_DIRECTORY_LIST_TOOLTIP),
							SetFill(1, 0), SetResize(1, 1), SetScrollbar(WID_TD_SCROLLBAR), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_BROWN),
				NWidget(WWT_TEXT, INVALID_COLOUR, WID_TD_WORLD_POPULATION), SetPadding(2, 0, 2, 2), SetFill(1, 0), SetResize(1, 0),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_TD_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

/** Enum referring to the Hotkeys in the town directory window */
enum TownDirectoryHotkeys : int32_t {
	TDHK_FOCUS_FILTER_BOX, ///< Focus the filter box
};

/** Town directory window class. */
struct TownDirectoryWindow : public Window {
private:
	/* Runtime saved values */
	static Listing last_sorting;

	/* Constants for sorting towns */
	static inline const StringID sorter_names[] = {
		STR_SORT_BY_NAME,
		STR_SORT_BY_POPULATION,
		STR_SORT_BY_RATING,
	};
	static const std::initializer_list<GUITownList::SortFunction * const> sorter_funcs;

	StringFilter string_filter{}; ///< Filter for towns
	QueryString townname_editbox; ///< Filter editbox

	GUITownList towns{TownDirectoryWindow::last_sorting.order};

	Scrollbar *vscroll = nullptr;

	void BuildSortTownList()
	{
		if (this->towns.NeedRebuild()) {
			this->towns.clear();
			this->towns.reserve(Town::GetNumItems());

			for (const Town *t : Town::Iterate()) {
				if (this->string_filter.IsEmpty()) {
					this->towns.push_back(t);
					continue;
				}
				this->string_filter.ResetState();
				this->string_filter.AddLine(t->GetCachedName());
				if (this->string_filter.GetState()) this->towns.push_back(t);
			}

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
		if (a->have_ratings.Test(_local_company)) {
			if (b->have_ratings.Test(_local_company)) {
				int16_t a_rating = a->ratings[_local_company];
				int16_t b_rating = b->ratings[_local_company];
				if (a_rating == b_rating) return TownDirectoryWindow::TownNameSorter(a, b, order);
				return a_rating < b_rating;
			}
			return before;
		}
		if (b->have_ratings.Test(_local_company)) return !before;

		/* Sort unrated towns always on ascending town name. */
		if (before) return TownDirectoryWindow::TownNameSorter(a, b, order);
		return TownDirectoryWindow::TownNameSorter(b, a, order);
	}

public:
	TownDirectoryWindow(WindowDesc &desc) : Window(desc), townname_editbox(MAX_LENGTH_TOWN_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_TOWN_NAME_CHARS)
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

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_TD_CAPTION:
				return GetString(STR_TOWN_DIRECTORY_CAPTION, this->vscroll->GetCount(), Town::GetNumItems());

			case WID_TD_WORLD_POPULATION:
				return GetString(STR_TOWN_POPULATION, GetWorldPopulation());

			case WID_TD_SORT_CRITERIA:
				return GetString(TownDirectoryWindow::sorter_names[this->towns.SortType()]);

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	/**
	 * Get the string to draw the town name.
	 * @param t Town to draw.
	 * @return The string to use.
	 */
	static std::string GetTownString(const Town *t, uint64_t population)
	{
		return GetString(t->larger_town ? STR_TOWN_DIRECTORY_CITY : STR_TOWN_DIRECTORY_TOWN, t->index, population);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_TD_SORT_ORDER:
				this->DrawSortButtonState(widget, this->towns.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;

			case WID_TD_LIST: {
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

				auto [first, last] = this->vscroll->GetVisibleRangeIterators(this->towns);
				for (auto it = first; it != last; ++it) {
					const Town *t = *it;
					assert(t->xy != INVALID_TILE);

					/* Draw rating icon. */
					if (_game_mode == GM_EDITOR || !t->have_ratings.Test(_local_company)) {
						DrawSprite(SPR_TOWN_RATING_NA, PAL_NONE, icon_x, tr.top + (this->resize.step_height - icon_size.height) / 2);
					} else {
						SpriteID icon = SPR_TOWN_RATING_APALLING;
						if (t->ratings[_local_company] > RATING_VERYPOOR) icon = SPR_TOWN_RATING_MEDIOCRE;
						if (t->ratings[_local_company] > RATING_GOOD)     icon = SPR_TOWN_RATING_GOOD;
						DrawSprite(icon, PAL_NONE, icon_x, tr.top + (this->resize.step_height - icon_size.height) / 2);
					}

					DrawString(tr.left, tr.right, tr.top + (this->resize.step_height - GetCharacterHeight(FS_NORMAL)) / 2, GetTownString(t, t->cache.population));

					tr.top += this->resize.step_height;
				}
				break;
			}
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_TD_SORT_ORDER: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->GetString());
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}
			case WID_TD_SORT_CRITERIA: {
				Dimension d = GetStringListBoundingBox(TownDirectoryWindow::sorter_names);
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}
			case WID_TD_LIST: {
				Dimension d = GetStringBoundingBox(STR_TOWN_DIRECTORY_NONE);
				uint64_t max_value = GetParamMaxDigits(8);
				for (uint i = 0; i < this->towns.size(); i++) {
					const Town *t = this->towns[i];

					assert(t != nullptr);

					d = maxdim(d, GetStringBoundingBox(GetTownString(t, max_value)));
				}
				Dimension icon_size = GetSpriteSize(SPR_TOWN_RATING_GOOD);
				d.width += icon_size.width + 2;
				d.height = std::max(d.height, icon_size.height);
				resize.height = d.height;
				d.height *= 5;
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}
			case WID_TD_WORLD_POPULATION: {
				Dimension d = GetStringBoundingBox(GetString(STR_TOWN_POPULATION, GetParamMaxDigits(10)));
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
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
		this->vscroll->SetCapacityFromWidget(this, WID_TD_LIST, WidgetDimensions::scaled.framerect.Vertical());
	}

	void OnEditboxChanged(WidgetID wid) override
	{
		if (wid == WID_TD_FILTER) {
			this->string_filter.SetFilterTerm(this->townname_editbox.text.GetText());
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

/** Available town directory sorting functions. */
const std::initializer_list<GUITownList::SortFunction * const> TownDirectoryWindow::sorter_funcs = {
	&TownNameSorter,
	&TownPopulationSorter,
	&TownRatingSorter,
};

static WindowDesc _town_directory_desc(
	WDP_AUTO, "list_towns", 208, 202,
	WC_TOWN_DIRECTORY, WC_NONE,
	{},
	_nested_town_directory_widgets,
	&TownDirectoryWindow::hotkeys
);

void ShowTownDirectory()
{
	if (BringWindowToFrontById(WC_TOWN_DIRECTORY, 0)) return;
	new TownDirectoryWindow(_town_directory_desc);
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

static constexpr NWidgetPart _nested_found_town_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetStringTip(STR_FOUND_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	/* Construct new town(s) buttons. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0), SetPadding(WidgetDimensions::unscaled.picker),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_NEW_TOWN), SetStringTip(STR_FOUND_TOWN_NEW_TOWN_BUTTON, STR_FOUND_TOWN_NEW_TOWN_TOOLTIP), SetFill(1, 0),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_TF_TOWN_ACTION_SEL),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_RANDOM_TOWN), SetStringTip(STR_FOUND_TOWN_RANDOM_TOWN_BUTTON, STR_FOUND_TOWN_RANDOM_TOWN_TOOLTIP), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_MANY_RANDOM_TOWNS), SetStringTip(STR_FOUND_TOWN_MANY_RANDOM_TOWNS, STR_FOUND_TOWN_RANDOM_TOWNS_TOOLTIP), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_LOAD_FROM_FILE), SetStringTip(STR_FOUND_TOWN_LOAD_FROM_FILE, STR_FOUND_TOWN_LOAD_FROM_FILE_TOOLTIP), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_EXPAND_ALL_TOWNS), SetStringTip(STR_FOUND_TOWN_EXPAND_ALL_TOWNS, STR_FOUND_TOWN_EXPAND_ALL_TOWNS_TOOLTIP), SetFill(1, 0),
				EndContainer(),
			EndContainer(),

			/* Town name selection. */
			NWidget(WWT_LABEL, INVALID_COLOUR), SetStringTip(STR_FOUND_TOWN_NAME_TITLE),
			NWidget(WWT_EDITBOX, COLOUR_GREY, WID_TF_TOWN_NAME_EDITBOX), SetStringTip(STR_FOUND_TOWN_NAME_EDITOR_TITLE, STR_FOUND_TOWN_NAME_EDITOR_TOOLTIP), SetFill(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TF_TOWN_NAME_RANDOM), SetStringTip(STR_FOUND_TOWN_NAME_RANDOM_BUTTON, STR_FOUND_TOWN_NAME_RANDOM_TOOLTIP), SetFill(1, 0),

			/* Town size selection. */
			NWidget(WWT_LABEL, INVALID_COLOUR), SetStringTip(STR_FOUND_TOWN_INITIAL_SIZE_TITLE),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_SMALL), SetStringTip(STR_FOUND_TOWN_INITIAL_SIZE_SMALL_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP), SetFill(1, 0),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_MEDIUM), SetStringTip(STR_FOUND_TOWN_INITIAL_SIZE_MEDIUM_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
					NWidget(NWID_SELECTION, INVALID_COLOUR, WID_TF_SIZE_SEL),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_LARGE), SetStringTip(STR_FOUND_TOWN_INITIAL_SIZE_LARGE_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP), SetFill(1, 0),
					EndContainer(),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_SIZE_RANDOM), SetStringTip(STR_FOUND_TOWN_SIZE_RANDOM, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP), SetFill(1, 0),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_CITY), SetStringTip(STR_FOUND_TOWN_CITY, STR_FOUND_TOWN_CITY_TOOLTIP), SetFill(1, 0),

			/* Town roads selection. */
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_TF_ROAD_LAYOUT_SEL),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
					NWidget(WWT_LABEL, INVALID_COLOUR), SetStringTip(STR_FOUND_TOWN_ROAD_LAYOUT),
					NWidget(NWID_VERTICAL),
						NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_ORIGINAL), SetStringTip(STR_FOUND_TOWN_SELECT_LAYOUT_ORIGINAL, STR_FOUND_TOWN_SELECT_LAYOUT_TOOLTIP), SetFill(1, 0),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_BETTER), SetStringTip(STR_FOUND_TOWN_SELECT_LAYOUT_BETTER_ROADS, STR_FOUND_TOWN_SELECT_LAYOUT_TOOLTIP), SetFill(1, 0),
						EndContainer(),
						NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_GRID2), SetStringTip(STR_FOUND_TOWN_SELECT_LAYOUT_2X2_GRID, STR_FOUND_TOWN_SELECT_LAYOUT_TOOLTIP), SetFill(1, 0),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_GRID3), SetStringTip(STR_FOUND_TOWN_SELECT_LAYOUT_3X3_GRID, STR_FOUND_TOWN_SELECT_LAYOUT_TOOLTIP), SetFill(1, 0),
						EndContainer(),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_TF_LAYOUT_RANDOM), SetStringTip(STR_FOUND_TOWN_SELECT_LAYOUT_RANDOM, STR_FOUND_TOWN_SELECT_LAYOUT_TOOLTIP), SetFill(1, 0),
					EndContainer(),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** Found a town window class. */
struct FoundTownWindow : Window {
private:
	TownSize town_size = TSZ_MEDIUM; ///< Selected town size
	TownLayout town_layout{}; ///< Selected town layout
	bool city = false; ///< Are we building a city?
	QueryString townname_editbox; ///< Townname editbox
	bool townnamevalid = false; ///< Is generated town name valid?
	uint32_t townnameparts = 0; ///< Generated town name
	TownNameParams params; ///< Town name parameters

public:
	FoundTownWindow(WindowDesc &desc, WindowNumber window_number) :
			Window(desc),
			town_layout(_settings_game.economy.town_layout),
			townname_editbox(MAX_LENGTH_TOWN_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_TOWN_NAME_CHARS),
			params(_settings_game.game_creation.town_name)
	{
		this->InitNested(window_number);
		this->querystrings[WID_TF_TOWN_NAME_EDITBOX] = &this->townname_editbox;
		this->RandomTownName();
		this->UpdateButtons(true);
	}

	void OnInit() override
	{
		if (_game_mode == GM_EDITOR) return;

		this->GetWidget<NWidgetStacked>(WID_TF_TOWN_ACTION_SEL)->SetDisplayedPlane(SZSP_HORIZONTAL);
		this->GetWidget<NWidgetStacked>(WID_TF_SIZE_SEL)->SetDisplayedPlane(SZSP_VERTICAL);
		if (_settings_game.economy.found_town != TF_CUSTOM_LAYOUT) {
			this->GetWidget<NWidgetStacked>(WID_TF_ROAD_LAYOUT_SEL)->SetDisplayedPlane(SZSP_HORIZONTAL);
		} else {
			this->GetWidget<NWidgetStacked>(WID_TF_ROAD_LAYOUT_SEL)->SetDisplayedPlane(0);
		}
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
			if (_settings_game.economy.found_town != TF_CUSTOM_LAYOUT) this->town_layout = _settings_game.economy.town_layout;
			this->ReInit();
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
			name = this->townname_editbox.text.GetText();
		} else {
			/* If user changed the name, send it */
			std::string original_name = GetTownName(&this->params, this->townnameparts);
			if (original_name != this->townname_editbox.text.GetText()) name = this->townname_editbox.text.GetText();
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
				this->ExecuteFoundTownCommand(TileIndex{}, true, STR_ERROR_CAN_T_GENERATE_TOWN, CcFoundRandomTown);
				break;

			case WID_TF_TOWN_NAME_RANDOM:
				this->RandomTownName();
				this->SetFocusedWidget(WID_TF_TOWN_NAME_EDITBOX);
				break;

			case WID_TF_MANY_RANDOM_TOWNS: {
				Backup<bool> old_generating_world(_generating_world, true);
				UpdateNearestTownForRoadTiles(true);
				if (!GenerateTowns(this->town_layout)) {
					ShowErrorMessage(GetEncodedString(STR_ERROR_CAN_T_GENERATE_TOWN), GetEncodedString(STR_ERROR_NO_SPACE_FOR_TOWN), WL_INFO);
				}
				UpdateNearestTownForRoadTiles(false);
				old_generating_world.Restore();
				break;
			}

			case WID_TF_LOAD_FROM_FILE:
				ShowSaveLoadDialog(FT_TOWN_DATA, SLO_LOAD);
				break;

			case WID_TF_EXPAND_ALL_TOWNS:
				for (Town *t : Town::Iterate()) {
					Command<CMD_EXPAND_TOWN>::Do(DoCommandFlag::Execute, t->index, 0);
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

				/* If we are in the editor, sync the settings of the current game to the chosen layout,
				 * so that importing towns from file uses the selected layout. */
				if (_game_mode == GM_EDITOR) _settings_game.economy.town_layout = this->town_layout;

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

static WindowDesc _found_town_desc(
	WDP_AUTO, "build_town", 160, 162,
	WC_FOUND_TOWN, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_found_town_widgets
);

void ShowFoundTownWindow()
{
	if (_game_mode != GM_EDITOR && !Company::IsValidID(_local_company)) return;
	AllocateWindowDescFront<FoundTownWindow>(_found_town_desc, 0);
}

void InitializeTownGui()
{
	_town_local_authority_kdtree.Clear();
}

/**
 * Draw representation of a house tile for GUI purposes.
 * @param x Position x of image.
 * @param y Position y of image.
 * @param spec House spec to draw.
 * @param house_id House ID to draw.
 * @param view The house's 'view'.
 */
void DrawNewHouseTileInGUI(int x, int y, const HouseSpec *spec, HouseID house_id, int view)
{
	HouseResolverObject object(house_id, INVALID_TILE, nullptr, CBID_NO_CALLBACK, 0, 0, true, view);
	const SpriteGroup *group = object.Resolve();
	if (group == nullptr || group->type != SGT_TILELAYOUT) return;

	uint8_t stage = TOWN_HOUSE_COMPLETED;
	const DrawTileSprites *dts = reinterpret_cast<const TileLayoutSpriteGroup *>(group)->ProcessRegisters(&stage);

	PaletteID palette = GENERAL_SPRITE_COLOUR(spec->random_colour[0]);
	if (spec->callback_mask.Test(HouseCallbackMask::Colour)) {
		uint16_t callback = GetHouseCallback(CBID_HOUSE_COLOUR, 0, 0, house_id, nullptr, INVALID_TILE, true, view);
		if (callback != CALLBACK_FAILED) {
			/* If bit 14 is set, we should use a 2cc colour map, else use the callback value. */
			palette = HasBit(callback, 14) ? GB(callback, 0, 8) + SPR_2CCMAP_BASE : callback;
		}
	}

	SpriteID image = dts->ground.sprite;
	PaletteID pal  = dts->ground.pal;

	if (HasBit(image, SPRITE_MODIFIER_CUSTOM_SPRITE)) image += stage;
	if (HasBit(pal, SPRITE_MODIFIER_CUSTOM_SPRITE)) pal += stage;

	if (GB(image, 0, SPRITE_WIDTH) != 0) {
		DrawSprite(image, GroundSpritePaletteTransform(image, pal, palette), x, y);
	}

	DrawNewGRFTileSeqInGUI(x, y, dts, stage, palette);
}

/**
 * Draw a house that does not exist.
 * @param x Position x of image.
 * @param y Position y of image.
 * @param house_id House ID to draw.
 * @param view The house's 'view'.
 */
void DrawHouseInGUI(int x, int y, HouseID house_id, int view)
{
	auto draw = [](int x, int y, HouseID house_id, int view) {
		if (house_id >= NEW_HOUSE_OFFSET) {
			/* Houses don't necessarily need new graphics. If they don't have a
			 * spritegroup associated with them, then the sprite for the substitute
			 * house id is drawn instead. */
			const HouseSpec *spec = HouseSpec::Get(house_id);
			if (spec->grf_prop.GetSpriteGroup() != nullptr) {
				DrawNewHouseTileInGUI(x, y, spec, house_id, view);
				return;
			} else {
				house_id = HouseSpec::Get(house_id)->grf_prop.subst_id;
			}
		}

		/* Retrieve data from the draw town tile struct */
		const DrawBuildingsTileStruct &dcts = GetTownDrawTileData()[house_id << 4 | view << 2 | TOWN_HOUSE_COMPLETED];
		DrawSprite(dcts.ground.sprite, dcts.ground.pal, x, y);

		/* Add a house on top of the ground? */
		if (dcts.building.sprite != 0) {
			Point pt = RemapCoords(dcts.subtile_x, dcts.subtile_y, 0);
			DrawSprite(dcts.building.sprite, dcts.building.pal, x + UnScaleGUI(pt.x), y + UnScaleGUI(pt.y));
		}
	};

	/* Houses can have 1x1, 1x2, 2x1 and 2x2 layouts which are individual HouseIDs. For the GUI we need
	 * draw all of the tiles with appropriate positions. */
	int x_delta = ScaleGUITrad(TILE_PIXELS);
	int y_delta = ScaleGUITrad(TILE_PIXELS / 2);

	const HouseSpec *hs = HouseSpec::Get(house_id);
	if (hs->building_flags.Test(BuildingFlag::Size2x2)) {
		draw(x, y - y_delta - y_delta, house_id, view); // North corner.
		draw(x + x_delta, y - y_delta, house_id + 1, view); // West corner.
		draw(x - x_delta, y - y_delta, house_id + 2, view); // East corner.
		draw(x, y, house_id + 3, view); // South corner.
	} else if (hs->building_flags.Test(BuildingFlag::Size2x1)) {
		draw(x + x_delta / 2, y - y_delta, house_id, view); // North east tile.
		draw(x - x_delta / 2, y, house_id + 1, view); // South west tile.
	} else if (hs->building_flags.Test(BuildingFlag::Size1x2)) {
		draw(x - x_delta / 2, y - y_delta, house_id, view); // North west tile.
		draw(x + x_delta / 2, y, house_id + 1, view); // South east tile.
	} else {
		draw(x, y, house_id, view);
	}
}

/**
 * Get name for a prototype house.
 * @param hs HouseSpec of house.
 * @return StringID of name for house.
 */
static StringID GetHouseName(const HouseSpec *hs)
{
	uint16_t callback_res = GetHouseCallback(CBID_HOUSE_CUSTOM_NAME, 1, 0, hs->Index(), nullptr, INVALID_TILE, true);
	if (callback_res != CALLBACK_FAILED && callback_res != 0x400) {
		if (callback_res > 0x400) {
			ErrorUnknownCallbackResult(hs->grf_prop.grffile->grfid, CBID_HOUSE_CUSTOM_NAME, callback_res);
		} else {
			StringID new_name = GetGRFStringID(hs->grf_prop.grffile->grfid, GRFSTR_MISC_GRF_TEXT + callback_res);
			if (new_name != STR_NULL && new_name != STR_UNDEFINED) {
				return new_name;
			}
		}
	}

	return hs->building_name;
}

class HousePickerCallbacks : public PickerCallbacks {
public:
	HousePickerCallbacks() : PickerCallbacks("fav_houses") {}

	/**
	 * Set climate mask for filtering buildings from current landscape.
	 */
	void SetClimateMask()
	{
		switch (_settings_game.game_creation.landscape) {
			case LandscapeType::Temperate: this->climate_mask = HZ_TEMP; break;
			case LandscapeType::Arctic:    this->climate_mask = HZ_SUBARTC_ABOVE | HZ_SUBARTC_BELOW; break;
			case LandscapeType::Tropic:    this->climate_mask = HZ_SUBTROPIC; break;
			case LandscapeType::Toyland:   this->climate_mask = HZ_TOYLND; break;
			default: NOT_REACHED();
		}

		/* In some cases, not all 'classes' (house zones) have distinct houses, so we need to disable those.
		 * As we need to check all types, and this cannot change with the picker window open, pre-calculate it.
		 * This loop calls GetTypeName() instead of directly checking properties so that there is no discrepancy. */
		this->class_mask = 0;

		int num_classes = this->GetClassCount();
		for (int cls_id = 0; cls_id < num_classes; ++cls_id) {
			int num_types = this->GetTypeCount(cls_id);
			for (int id = 0; id < num_types; ++id) {
				if (this->GetTypeName(cls_id, id) != INVALID_STRING_ID) {
					SetBit(this->class_mask, cls_id);
					break;
				}
			}
		}
	}

	HouseZones climate_mask{};
	uint8_t class_mask = 0; ///< Mask of available 'classes'.

	static inline int sel_class; ///< Currently selected 'class'.
	static inline int sel_type; ///< Currently selected HouseID.
	static inline int sel_view; ///< Currently selected 'view'. This is not controllable as its based on random data.

	/* Houses do not have classes like NewGRFClass. We'll make up fake classes based on town zone
	 * availability instead. */
	static inline const std::array<StringID, HZB_END> zone_names = {
		STR_HOUSE_PICKER_CLASS_ZONE1,
		STR_HOUSE_PICKER_CLASS_ZONE2,
		STR_HOUSE_PICKER_CLASS_ZONE3,
		STR_HOUSE_PICKER_CLASS_ZONE4,
		STR_HOUSE_PICKER_CLASS_ZONE5,
	};

	GrfSpecFeature GetFeature() const override { return GSF_HOUSES; }

	StringID GetClassTooltip() const override { return STR_PICKER_HOUSE_CLASS_TOOLTIP; }
	StringID GetTypeTooltip() const override { return STR_PICKER_HOUSE_TYPE_TOOLTIP; }
	bool IsActive() const override { return true; }

	bool HasClassChoice() const override { return true; }
	int GetClassCount() const override { return static_cast<int>(zone_names.size()); }

	void Close([[maybe_unused]] int data) override { ResetObjectToPlace(); }

	int GetSelectedClass() const override { return HousePickerCallbacks::sel_class; }
	void SetSelectedClass(int cls_id) const override { HousePickerCallbacks::sel_class = cls_id; }

	StringID GetClassName(int id) const override
	{
		if (id >= GetClassCount()) return INVALID_STRING_ID;
		if (!HasBit(this->class_mask, id)) return INVALID_STRING_ID;
		return zone_names[id];
	}

	int GetTypeCount(int cls_id) const override
	{
		if (cls_id < GetClassCount()) return static_cast<int>(HouseSpec::Specs().size());
		return 0;
	}

	PickerItem GetPickerItem(int cls_id, int id) const override
	{
		const auto *spec = HouseSpec::Get(id);
		if (!spec->grf_prop.HasGrfFile()) return {0, spec->Index(), cls_id, id};
		return {spec->grf_prop.grfid, spec->grf_prop.local_id, cls_id, id};
	}

	int GetSelectedType() const override { return sel_type; }
	void SetSelectedType(int id) const override { sel_type = id; }

	StringID GetTypeName(int cls_id, int id) const override
	{
		const HouseSpec *spec = HouseSpec::Get(id);
		if (spec == nullptr) return INVALID_STRING_ID;
		if (!spec->enabled) return INVALID_STRING_ID;
		if ((spec->building_availability & climate_mask) == 0) return INVALID_STRING_ID;
		if (!HasBit(spec->building_availability, cls_id)) return INVALID_STRING_ID;
		for (int i = 0; i < cls_id; i++) {
			/* Don't include if it's already included in an earlier zone. */
			if (HasBit(spec->building_availability, i)) return INVALID_STRING_ID;
		}

		return GetHouseName(spec);
	}

	std::span<const BadgeID> GetTypeBadges(int cls_id, int id) const override
	{
		const auto *spec = HouseSpec::Get(id);
		if (spec == nullptr) return {};
		if (!spec->enabled) return {};
		if ((spec->building_availability & climate_mask) == 0) return {};
		if (!HasBit(spec->building_availability, cls_id)) return {};
		for (int i = 0; i < cls_id; i++) {
			/* Don't include if it's already included in an earlier zone. */
			if (HasBit(spec->building_availability, i)) return {};
		}

		return spec->badges;
	}

	bool IsTypeAvailable(int, int id) const override
	{
		const HouseSpec *hs = HouseSpec::Get(id);
		return hs->enabled;
	}

	void DrawType(int x, int y, int, int id) const override
	{
		DrawHouseInGUI(x, y, id, HousePickerCallbacks::sel_view);
	}

	void FillUsedItems(std::set<PickerItem> &items) override
	{
		auto id_count = GetBuildingHouseIDCounts();
		for (auto it = id_count.begin(); it != id_count.end(); ++it) {
			if (*it == 0) continue;
			HouseID house = static_cast<HouseID>(std::distance(id_count.begin(), it));
			const HouseSpec *hs = HouseSpec::Get(house);
			int class_index = FindFirstBit(hs->building_availability & HZ_ZONALL);
			items.insert({0, house, class_index, house});
		}
	}

	std::set<PickerItem> UpdateSavedItems(const std::set<PickerItem> &src) override
	{
		if (src.empty()) return src;

		const auto &specs = HouseSpec::Specs();
		std::set<PickerItem> dst;
		for (const auto &item : src) {
			if (item.grfid == 0) {
				dst.insert(item);
			} else {
				/* Search for spec by grfid and local index. */
				auto it = std::ranges::find_if(specs, [&item](const HouseSpec &spec) { return spec.grf_prop.grfid == item.grfid && spec.grf_prop.local_id == item.local_id; });
				if (it == specs.end()) {
					/* Not preset, hide from UI. */
					dst.insert({item.grfid, item.local_id, -1, -1});
				} else {
					int class_index = FindFirstBit(it->building_availability & HZ_ZONALL);
					dst.insert( {item.grfid, item.local_id, class_index, it->Index()});
				}
			}
		}

		return dst;
	}

	static HousePickerCallbacks instance;
};
/* static */ HousePickerCallbacks HousePickerCallbacks::instance;

/**
 * Get the cargo types produced by a house.
 * @param hs HouseSpec of the house.
 * @returns Mask of cargo types produced by the house.
 */
static CargoTypes GetProducedCargoOfHouse(const HouseSpec *hs)
{
	CargoTypes produced{};
	if (hs->callback_mask.Test(HouseCallbackMask::ProduceCargo)) {
		for (uint i = 0; i < 256; i++) {
			uint16_t callback = GetHouseCallback(CBID_HOUSE_PRODUCE_CARGO, i, 0, hs->Index(), nullptr, INVALID_TILE, true);

			if (callback == CALLBACK_FAILED || callback == CALLBACK_HOUSEPRODCARGO_END) break;

			CargoType cargo = GetCargoTranslation(GB(callback, 8, 7), hs->grf_prop.grffile);
			if (!IsValidCargoType(cargo)) continue;

			uint amt = GB(callback, 0, 8);
			if (amt == 0) continue;

			SetBit(produced, cargo);
		}
	} else {
		/* Cargo is not controlled by NewGRF, town production effect is used instead. */
		for (const CargoSpec *cs : CargoSpec::town_production_cargoes[TPE_PASSENGERS]) SetBit(produced, cs->Index());
		for (const CargoSpec *cs : CargoSpec::town_production_cargoes[TPE_MAIL]) SetBit(produced, cs->Index());
	}
	return produced;
}

struct BuildHouseWindow : public PickerWindow {
	std::string house_info{};
	bool house_protected = false;

	BuildHouseWindow(WindowDesc &desc, Window *parent) : PickerWindow(desc, parent, 0, HousePickerCallbacks::instance)
	{
		HousePickerCallbacks::instance.SetClimateMask();
		this->ConstructWindow();
	}

	void UpdateSelectSize(const HouseSpec *spec)
	{
		if (spec == nullptr) {
			SetTileSelectSize(1, 1);
			ResetObjectToPlace();
		} else {
			SetObjectToPlaceWnd(SPR_CURSOR_TOWN, PAL_NONE, HT_RECT | HT_DIAGONAL, this);
			if (spec->building_flags.Test(BuildingFlag::Size2x2)) {
				SetTileSelectSize(2, 2);
			} else if (spec->building_flags.Test(BuildingFlag::Size2x1)) {
				SetTileSelectSize(2, 1);
			} else if (spec->building_flags.Test(BuildingFlag::Size1x2)) {
				SetTileSelectSize(1, 2);
			} else if (spec->building_flags.Test(BuildingFlag::Size1x1)) {
				SetTileSelectSize(1, 1);
			}
		}
	}

	/**
	 * Get a date range string for house availability year.
	 * @param min_year Earliest year house can be built.
	 * @param max_year Latest year house can be built.
	 * @return Formatted string with the date range formatted appropriately.
	 */
	static std::string GetHouseYear(TimerGameCalendar::Year min_year, TimerGameCalendar::Year max_year)
	{
		if (min_year == CalendarTime::MIN_YEAR) {
			if (max_year == CalendarTime::MAX_YEAR) {
				return GetString(STR_HOUSE_PICKER_YEARS_ANY);
			}
			return GetString(STR_HOUSE_PICKER_YEARS_UNTIL, max_year);
		}
		if (max_year == CalendarTime::MAX_YEAR) {
			return GetString(STR_HOUSE_PICKER_YEARS_FROM, min_year);
		}
		return GetString(STR_HOUSE_PICKER_YEARS, min_year, max_year);
	}

	/**
	 * Get information string for a house.
	 * @param hs HouseSpec to get information string for.
	 * @return Formatted string with information for house.
	 */
	static std::string GetHouseInformation(const HouseSpec *hs)
	{
		std::stringstream line;

		line << GetString(STR_HOUSE_PICKER_NAME, GetHouseName(hs));
		line << "\n";

		line << GetString(STR_HOUSE_PICKER_POPULATION, hs->population);
		line << "\n";

		line << GetHouseYear(hs->min_year, hs->max_year);
		line << "\n";

		uint8_t size = 0;
		if (hs->building_flags.Test(BuildingFlag::Size1x1)) size = 0x11;
		if (hs->building_flags.Test(BuildingFlag::Size2x1)) size = 0x21;
		if (hs->building_flags.Test(BuildingFlag::Size1x2)) size = 0x12;
		if (hs->building_flags.Test(BuildingFlag::Size2x2)) size = 0x22;
		line << GetString(STR_HOUSE_PICKER_SIZE, GB(size, 0, 4), GB(size, 4, 4));

		auto cargo_string = BuildCargoAcceptanceString(GetAcceptedCargoOfHouse(hs), STR_HOUSE_PICKER_CARGO_ACCEPTED);
		if (cargo_string.has_value()) {
			line << "\n";
			line << *cargo_string;
		}

		CargoTypes produced = GetProducedCargoOfHouse(hs);
		if (produced != 0) {
			line << "\n";
			line << GetString(STR_HOUSE_PICKER_CARGO_PRODUCED, produced);
		}

		return line.str();
	}

	void OnInit() override
	{
		this->SetWidgetLoweredState(WID_BH_PROTECT_OFF, !this->house_protected);
		this->SetWidgetLoweredState(WID_BH_PROTECT_ON, this->house_protected);

		this->PickerWindow::OnInit();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget == WID_BH_INFO) {
			if (!this->house_info.empty()) DrawStringMultiLine(r, this->house_info);
		} else {
			this->PickerWindow::DrawWidget(r, widget);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BH_PROTECT_OFF:
			case WID_BH_PROTECT_ON:
				this->house_protected = (widget == WID_BH_PROTECT_ON);
				this->SetWidgetLoweredState(WID_BH_PROTECT_OFF, !this->house_protected);
				this->SetWidgetLoweredState(WID_BH_PROTECT_ON, this->house_protected);

				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;

			default:
				this->PickerWindow::OnClick(pt, widget, click_count);
				break;
		}
	}

	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		this->PickerWindow::OnInvalidateData(data, gui_scope);
		if (!gui_scope) return;

		const HouseSpec *spec = HouseSpec::Get(HousePickerCallbacks::sel_type);

		PickerInvalidations pi(data);
		if (pi.Test(PickerInvalidation::Position)) {
			UpdateSelectSize(spec);
			this->house_info = GetHouseInformation(spec);
		}

		/* If house spec already has the protected flag, handle it automatically and disable the buttons. */
		bool hasflag = spec->extra_flags.Test(HouseExtraFlag::BuildingIsProtected);
		if (hasflag) this->house_protected = true;

		this->SetWidgetLoweredState(WID_BH_PROTECT_OFF, !this->house_protected);
		this->SetWidgetLoweredState(WID_BH_PROTECT_ON, this->house_protected);

		this->SetWidgetDisabledState(WID_BH_PROTECT_OFF, hasflag);
		this->SetWidgetDisabledState(WID_BH_PROTECT_ON, hasflag);
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		const HouseSpec *spec = HouseSpec::Get(HousePickerCallbacks::sel_type);
		Command<CMD_PLACE_HOUSE>::Post(STR_ERROR_CAN_T_BUILD_HOUSE, CcPlaySound_CONSTRUCTION_OTHER, tile, spec->Index(), this->house_protected);
	}

	IntervalTimer<TimerWindow> view_refresh_interval = {std::chrono::milliseconds(2500), [this](auto) {
		/* There are four different 'views' that are random based on house tile position. As this is not
		 * user-controllable, instead we automatically cycle through them. */
		HousePickerCallbacks::sel_view = (HousePickerCallbacks::sel_view + 1) % 4;
		this->SetDirty();
	}};

	static inline HotkeyList hotkeys{"buildhouse", {
		Hotkey('F', "focus_filter_box", PCWHK_FOCUS_FILTER_BOX),
	}};
};

/** Nested widget definition for the build NewGRF rail waypoint window */
static constexpr NWidgetPart _nested_build_house_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetStringTip(STR_HOUSE_PICKER_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidgetFunction(MakePickerClassWidgets),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_picker, 0), SetPadding(WidgetDimensions::unscaled.picker),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_BH_INFO), SetFill(1, 1), SetMinimalTextLines(10, 0),
					NWidget(WWT_LABEL, INVALID_COLOUR), SetStringTip(STR_HOUSE_PICKER_PROTECT_TITLE, STR_NULL), SetFill(1, 0),
					NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BH_PROTECT_OFF), SetMinimalSize(60, 12), SetStringTip(STR_HOUSE_PICKER_PROTECT_OFF, STR_HOUSE_PICKER_PROTECT_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BH_PROTECT_ON), SetMinimalSize(60, 12), SetStringTip(STR_HOUSE_PICKER_PROTECT_ON, STR_HOUSE_PICKER_PROTECT_TOOLTIP),
					EndContainer(),
				EndContainer(),
			EndContainer(),

		EndContainer(),
		NWidgetFunction(MakePickerTypeWidgets),
	EndContainer(),
};

static WindowDesc _build_house_desc(
	WDP_AUTO, "build_house", 0, 0,
	WC_BUILD_HOUSE, WC_BUILD_TOOLBAR,
	WindowDefaultFlag::Construction,
	_nested_build_house_widgets,
	&BuildHouseWindow::hotkeys
);

void ShowBuildHousePicker(Window *parent)
{
	if (BringWindowToFrontById(WC_BUILD_HOUSE, 0)) return;
	new BuildHouseWindow(_build_house_desc, parent);
}
