/* $Id$ */

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
#include "gui.h"
#include "command_func.h"
#include "company_func.h"
#include "company_base.h"
#include "company_gui.h"
#include "network/network.h"
#include "strings_func.h"
#include "sound_func.h"
#include "economy_func.h"
#include "tilehighlight_func.h"
#include "sortlist_type.h"
#include "road_cmd.h"
#include "landscape.h"
#include "cargotype.h"
#include "querystring_gui.h"
#include "window_func.h"
#include "townname_func.h"
#include "townname_type.h"
#include "core/geometry_func.hpp"
#include "genworld.h"
#include "sprite.h"

#include "table/strings.h"

typedef GUIList<const Town*> GUITownList;

/** Widget numbers of the town authority window. */
enum TownAuthorityWidgets {
	TWA_CAPTION,
	TWA_RATING_INFO,  ///< Overview with ratings for each company.
	TWA_COMMAND_LIST, ///< List of commands for the player.
	TWA_SCROLLBAR,
	TWA_ACTION_INFO,  ///< Additional information about the action.
	TWA_EXECUTE,      ///< Do-it button.
};

static const NWidgetPart _nested_town_authority_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, TWA_CAPTION), SetDataTip(STR_LOCAL_AUTHORITY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, TWA_RATING_INFO), SetMinimalSize(317, 92), SetResize(1, 1), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN, TWA_COMMAND_LIST), SetMinimalSize(305, 52), SetResize(1, 0), SetDataTip(0x0, STR_LOCAL_AUTHORITY_ACTIONS_TOOLTIP), SetScrollbar(TWA_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, TWA_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, TWA_ACTION_INFO), SetMinimalSize(317, 52), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TWA_EXECUTE),  SetMinimalSize(317, 12), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_LOCAL_AUTHORITY_DO_IT_BUTTON, STR_LOCAL_AUTHORITY_DO_IT_TOOLTIP),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer()
};

/** Town authority window. */
struct TownAuthorityWindow : Window {
private:
	Town *town;    ///< Town being displayed.
	int sel_index; ///< Currently selected town action, \c 0 to \c TACT_COUNT-1, \c -1 means no action selected.
	Scrollbar *vscroll;

	/**
	 * Get the position of the Nth set bit.
	 *
	 * If there is no Nth bit set return -1
	 *
	 * @param bits The value to search in
	 * @param n The Nth set bit from which we want to know the position
	 * @return The position of the Nth set bit
	 */
	static int GetNthSetBit(uint32 bits, int n)
	{
		if (n >= 0) {
			uint i;
			FOR_EACH_SET_BIT(i, bits) {
				n--;
				if (n < 0) return i;
			}
		}
		return -1;
	}

public:
	TownAuthorityWindow(const WindowDesc *desc, WindowNumber window_number) : Window(), sel_index(-1)
	{
		this->town = Town::Get(window_number);
		this->InitNested(desc, window_number);
		this->vscroll = this->GetScrollbar(TWA_SCROLLBAR);
		this->vscroll->SetCapacity((this->GetWidget<NWidgetBase>(TWA_COMMAND_LIST)->current_y - WD_FRAMERECT_TOP - WD_FRAMERECT_BOTTOM) / FONT_HEIGHT_NORMAL);
	}

	virtual void OnPaint()
	{
		int numact;
		uint buttons = GetMaskOfTownActions(&numact, _local_company, this->town);

		this->vscroll->SetCount(numact + 1);

		if (this->sel_index != -1 && !HasBit(buttons, this->sel_index)) {
			this->sel_index = -1;
		}

		this->SetWidgetDisabledState(TWA_EXECUTE, this->sel_index == -1);

		this->DrawWidgets();
		if (!this->IsShaded()) this->DrawRatings();
	}

	/** Draw the contents of the ratings panel. May request a resize of the window if the contents does not fit. */
	void DrawRatings()
	{
		NWidgetBase *nwid = this->GetWidget<NWidgetBase>(TWA_RATING_INFO);
		uint left = nwid->pos_x + WD_FRAMERECT_LEFT;
		uint right = nwid->pos_x + nwid->current_x - 1 - WD_FRAMERECT_RIGHT;

		uint y = nwid->pos_y + WD_FRAMERECT_TOP;

		DrawString(left, right, y, STR_LOCAL_AUTHORITY_COMPANY_RATINGS);
		y += FONT_HEIGHT_NORMAL;

		Dimension icon_size = GetSpriteSize(SPR_COMPANY_ICON);
		int icon_width      = icon_size.width;
		int icon_y_offset   = (FONT_HEIGHT_NORMAL - icon_size.height) / 2;

		Dimension exclusive_size = GetSpriteSize(SPR_EXCLUSIVE_TRANSPORT);
		int exclusive_width      = exclusive_size.width;
		int exclusive_y_offset   = (FONT_HEIGHT_NORMAL - exclusive_size.height) / 2;

		bool rtl = _current_text_dir == TD_RTL;
		uint text_left      = left  + (rtl ? 0 : icon_width + exclusive_width + 4);
		uint text_right     = right - (rtl ? icon_width + exclusive_width + 4 : 0);
		uint icon_left      = rtl ? right - icon_width : left;
		uint exclusive_left = rtl ? right - icon_width - exclusive_width - 2 : left + icon_width + 2;

		/* Draw list of companies */
		const Company *c;
		FOR_ALL_COMPANIES(c) {
			if ((HasBit(this->town->have_ratings, c->index) || this->town->exclusivity == c->index)) {
				DrawCompanyIcon(c->index, icon_left, y + icon_y_offset);

				SetDParam(0, c->index);
				SetDParam(1, c->index);

				int r = this->town->ratings[c->index];
				StringID str;
				(str = STR_CARGO_RATING_APPALLING, r <= RATING_APPALLING) || // Apalling
				(str++,                    r <= RATING_VERYPOOR)  || // Very Poor
				(str++,                    r <= RATING_POOR)      || // Poor
				(str++,                    r <= RATING_MEDIOCRE)  || // Mediocore
				(str++,                    r <= RATING_GOOD)      || // Good
				(str++,                    r <= RATING_VERYGOOD)  || // Very Good
				(str++,                    r <= RATING_EXCELLENT) || // Excellent
				(str++,                    true);                    // Outstanding

				SetDParam(2, str);
				if (this->town->exclusivity == c->index) {
					DrawSprite(SPR_EXCLUSIVE_TRANSPORT, COMPANY_SPRITE_COLOUR(c->index), exclusive_left, y + exclusive_y_offset);
				}

				DrawString(text_left, text_right, y, STR_LOCAL_AUTHORITY_COMPANY_RATING);
				y += FONT_HEIGHT_NORMAL;
			}
		}

		y = y + WD_FRAMERECT_BOTTOM - nwid->pos_y; // Compute needed size of the widget.
		if (y > nwid->current_y) {
			/* If the company list is too big to fit, mark ourself dirty and draw again. */
			ResizeWindow(this, 0, y - nwid->current_y);
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == TWA_CAPTION) SetDParam(0, this->window_number);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case TWA_ACTION_INFO:
				if (this->sel_index != -1) {
					SetDParam(0, _price[PR_TOWN_ACTION] * _town_action_costs[this->sel_index] >> 8);
					DrawStringMultiLine(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, r.bottom - WD_FRAMERECT_BOTTOM,
								STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_SMALL_ADVERTISING + this->sel_index);
				}
				break;
			case TWA_COMMAND_LIST: {
				int numact;
				uint buttons = GetMaskOfTownActions(&numact, _local_company, this->town);
				int y = r.top + WD_FRAMERECT_TOP;
				int pos = this->vscroll->GetPosition();

				if (--pos < 0) {
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_LOCAL_AUTHORITY_ACTIONS_TITLE);
					y += FONT_HEIGHT_NORMAL;
				}

				for (int i = 0; buttons; i++, buttons >>= 1) {
					if (pos <= -5) break; ///< Draw only the 5 fitting lines

					if ((buttons & 1) && --pos < 0) {
						DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y,
								STR_LOCAL_AUTHORITY_ACTION_SMALL_ADVERTISING_CAMPAIGN + i, this->sel_index == i ? TC_WHITE : TC_ORANGE);
						y += FONT_HEIGHT_NORMAL;
					}
				}
				break;
			}
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case TWA_ACTION_INFO: {
				assert(size->width > padding.width && size->height > padding.height);
				size->width -= WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				size->height -= WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				Dimension d = {0, 0};
				for (int i = 0; i < TACT_COUNT; i++) {
					SetDParam(0, _price[PR_TOWN_ACTION] * _town_action_costs[i] >> 8);
					d = maxdim(d, GetStringMultiLineBoundingBox(STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_SMALL_ADVERTISING + i, *size));
				}
				*size = maxdim(*size, d);
				size->width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				size->height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				break;
			}

			case TWA_COMMAND_LIST:
				size->height = WD_FRAMERECT_TOP + 5 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM;
				size->width = GetStringBoundingBox(STR_LOCAL_AUTHORITY_ACTIONS_TITLE).width;
				for (uint i = 0; i < TACT_COUNT; i++ ) {
					size->width = max(size->width, GetStringBoundingBox(STR_LOCAL_AUTHORITY_ACTION_SMALL_ADVERTISING_CAMPAIGN + i).width);
				}
				size->width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				break;

			case TWA_RATING_INFO:
				resize->height = FONT_HEIGHT_NORMAL;
				size->height = WD_FRAMERECT_TOP + 9 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM;
				break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case TWA_COMMAND_LIST: {
				int y = this->GetRowFromWidget(pt.y, TWA_COMMAND_LIST, 1, FONT_HEIGHT_NORMAL);
				if (!IsInsideMM(y, 0, 5)) return;

				y = GetNthSetBit(GetMaskOfTownActions(NULL, _local_company, this->town), y + this->vscroll->GetPosition() - 1);
				if (y >= 0) {
					this->sel_index = y;
					this->SetDirty();
				}
				/* FALL THROUGH, when double-clicking. */
				if (click_count == 1 || y < 0) break;
			}

			case TWA_EXECUTE:
				DoCommandP(this->town->xy, this->window_number, this->sel_index, CMD_DO_TOWN_ACTION | CMD_MSG(STR_ERROR_CAN_T_DO_THIS));
				break;
		}
	}

	virtual void OnHundredthTick()
	{
		this->SetDirty();
	}
};

static const WindowDesc _town_authority_desc(
	WDP_AUTO, 317, 222,
	WC_TOWN_AUTHORITY, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_town_authority_widgets, lengthof(_nested_town_authority_widgets)
);

static void ShowTownAuthorityWindow(uint town)
{
	AllocateWindowDescFront<TownAuthorityWindow>(&_town_authority_desc, town);
}

/** Widget numbers of the town view window. */
enum TownViewWidgets {
	TVW_CAPTION,
	TVW_VIEWPORT,
	TVW_INFOPANEL,
	TVW_CENTERVIEW,
	TVW_SHOWAUTHORITY,
	TVW_CHANGENAME,
	TVW_EXPAND,
	TVW_DELETE,
};

/* Town view window. */
struct TownViewWindow : Window {
private:
	Town *town; ///< Town displayed by the window.

public:
	static const int TVW_HEIGHT_NORMAL = 150;

	TownViewWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->CreateNestedTree(desc);

		this->town = Town::Get(window_number);
		if (this->town->larger_town) this->GetWidget<NWidgetCore>(TVW_CAPTION)->widget_data = STR_TOWN_VIEW_CITY_CAPTION;

		this->FinishInitNested(desc, window_number);

		this->flags4 |= WF_DISABLE_VP_SCROLL;
		NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(TVW_VIEWPORT);
		nvp->InitializeViewport(this, this->town->xy, ZOOM_LVL_NEWS);

		/* disable renaming town in network games if you are not the server */
		this->SetWidgetDisabledState(TVW_CHANGENAME, _networking && !_network_server);
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == TVW_CAPTION) SetDParam(0, this->town->index);
	}

	/**
	 * Determines the first cargo with a certain town effect
	 * @param effect Town effect of interest
	 * @return first active cargo slot with that effect
	 */
	const CargoSpec *FindFirstCargoWithTownEffect(TownEffect effect) const
	{
		const CargoSpec *cs;
		FOR_ALL_CARGOSPECS(cs) {
			if (cs->town_effect == effect) return cs;
		}
		return NULL;
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != TVW_INFOPANEL) return;

		uint y = r.top + WD_FRAMERECT_TOP;

		SetDParam(0, this->town->population);
		SetDParam(1, this->town->num_houses);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y, STR_TOWN_VIEW_POPULATION_HOUSES);

		SetDParam(0, this->town->act_pass);
		SetDParam(1, this->town->max_pass);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += FONT_HEIGHT_NORMAL, STR_TOWN_VIEW_PASSENGERS_LAST_MONTH_MAX);

		SetDParam(0, this->town->act_mail);
		SetDParam(1, this->town->max_mail);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += FONT_HEIGHT_NORMAL, STR_TOWN_VIEW_MAIL_LAST_MONTH_MAX);

		StringID required_text = STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_REQUIRED;
		uint cargo_needed_for_growth = 0;
		switch (_settings_game.game_creation.landscape) {
			case LT_ARCTIC:
				if (TilePixelHeight(this->town->xy) >= LowestSnowLine()) cargo_needed_for_growth = 1;
				if (TilePixelHeight(this->town->xy) < GetSnowLine()) required_text = STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_REQUIRED_WINTER;
				break;

			case LT_TROPIC:
				if (GetTropicZone(this->town->xy) == TROPICZONE_DESERT) cargo_needed_for_growth = 2;
				break;

			default: break;
		}

		if (cargo_needed_for_growth > 0) {
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += FONT_HEIGHT_NORMAL, STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH);

			bool rtl = _current_text_dir == TD_RTL;
			uint cargo_text_left = r.left + WD_FRAMERECT_LEFT + (rtl ? 0 : 20);
			uint cargo_text_right = r.right - WD_FRAMERECT_RIGHT - (rtl ? 20 : 0);

			const CargoSpec *food = FindFirstCargoWithTownEffect(TE_FOOD);
			CargoID first_food_cargo = (food != NULL) ? food->Index() : (CargoID)CT_INVALID;
			StringID food_name       = (food != NULL) ? food->name    : STR_CARGO_PLURAL_FOOD;

			const CargoSpec *water = FindFirstCargoWithTownEffect(TE_WATER);
			CargoID first_water_cargo = (water != NULL) ? water->Index() : (CargoID)CT_INVALID;
			StringID water_name       = (water != NULL) ? water->name    : STR_CARGO_PLURAL_WATER;

			if (first_food_cargo != CT_INVALID && this->town->act_food > 0) {
				SetDParam(0, first_food_cargo);
				SetDParam(1, this->town->act_food);
				DrawString(cargo_text_left, cargo_text_right, y += FONT_HEIGHT_NORMAL, STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_LAST_MONTH);
			} else {
				SetDParam(0, food_name);
				DrawString(cargo_text_left, cargo_text_right, y += FONT_HEIGHT_NORMAL, required_text);
			}

			if (cargo_needed_for_growth > 1) {
				if (first_water_cargo != CT_INVALID && this->town->act_water > 0) {
					SetDParam(0, first_water_cargo);
					SetDParam(1, this->town->act_water);
					DrawString(cargo_text_left, cargo_text_right, y += FONT_HEIGHT_NORMAL, STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_LAST_MONTH);
				} else {
					SetDParam(0, water_name);
					DrawString(cargo_text_left, cargo_text_right, y += FONT_HEIGHT_NORMAL, required_text);
				}
			}
		}

		/* only show the town noise, if the noise option is activated. */
		if (_settings_game.economy.station_noise_level) {
			SetDParam(0, this->town->noise_reached);
			SetDParam(1, this->town->MaxTownNoise());
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_LEFT, y += FONT_HEIGHT_NORMAL, STR_TOWN_VIEW_NOISE_IN_TOWN);
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case TVW_CENTERVIEW: // scroll to location
				if (_ctrl_pressed) {
					ShowExtraViewPortWindow(this->town->xy);
				} else {
					ScrollMainWindowToTile(this->town->xy);
				}
				break;

			case TVW_SHOWAUTHORITY: // town authority
				ShowTownAuthorityWindow(this->window_number);
				break;

			case TVW_CHANGENAME: // rename
				SetDParam(0, this->window_number);
				ShowQueryString(STR_TOWN_NAME, STR_TOWN_VIEW_RENAME_TOWN_BUTTON, MAX_LENGTH_TOWN_NAME_CHARS, MAX_LENGTH_TOWN_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT | QSF_LEN_IN_CHARS);
				break;

			case TVW_EXPAND: { // expand town - only available on Scenario editor
				/* Warn the user if towns are not allowed to build roads, but do this only once per OpenTTD run. */
				static bool _warn_town_no_roads = false;

				if (!_settings_game.economy.allow_town_roads && !_warn_town_no_roads) {
					ShowErrorMessage(STR_ERROR_TOWN_EXPAND_WARN_NO_ROADS, INVALID_STRING_ID, WL_WARNING);
					_warn_town_no_roads = true;
				}

				DoCommandP(0, this->window_number, 0, CMD_EXPAND_TOWN | CMD_MSG(STR_ERROR_CAN_T_EXPAND_TOWN));
				break;
			}

			case TVW_DELETE: // delete town - only available on Scenario editor
				DoCommandP(0, this->window_number, 0, CMD_DELETE_TOWN | CMD_MSG(STR_ERROR_TOWN_CAN_T_DELETE));
				break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case TVW_INFOPANEL:
				size->height = GetDesiredInfoHeight();
				break;
		}
	}

	/**
	 * Gets the desired height for the information panel.
	 * @return the desired height in pixels.
	 */
	uint GetDesiredInfoHeight() const
	{
		uint aimed_height = 3 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;

		switch (_settings_game.game_creation.landscape) {
			case LT_ARCTIC:
				if (TilePixelHeight(this->town->xy) >= LowestSnowLine()) aimed_height += 2 * FONT_HEIGHT_NORMAL;
				break;

			case LT_TROPIC:
				if (GetTropicZone(this->town->xy) == TROPICZONE_DESERT) aimed_height += 3 * FONT_HEIGHT_NORMAL;
				break;

			default: break;
		}

		if (_settings_game.economy.station_noise_level) aimed_height += FONT_HEIGHT_NORMAL;

		return aimed_height;
	}

	void ResizeWindowAsNeeded()
	{
		const NWidgetBase *nwid_info = this->GetWidget<NWidgetBase>(TVW_INFOPANEL);
		uint aimed_height = GetDesiredInfoHeight();
		if (aimed_height > nwid_info->current_y || (aimed_height < nwid_info->current_y && nwid_info->current_y > nwid_info->smallest_y)) {
			this->ReInit();
		}
	}

	virtual void OnResize()
	{
		if (this->viewport != NULL) {
			NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(TVW_VIEWPORT);
			nvp->UpdateViewportCoordinates(this);

			ScrollWindowToTile(this->town->xy, this, true); // Re-center viewport.
		}
	}

	virtual void OnInvalidateData(int data = 0)
	{
		/* Called when setting station noise have changed, in order to resize the window */
		this->SetDirty(); // refresh display for current size. This will allow to avoid glitches when downgrading
		this->ResizeWindowAsNeeded();
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		DoCommandP(0, this->window_number, 0, CMD_RENAME_TOWN | CMD_MSG(STR_ERROR_CAN_T_RENAME_TOWN), NULL, str);
	}
};

static const NWidgetPart _nested_town_game_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, TVW_CAPTION), SetDataTip(STR_TOWN_VIEW_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(WWT_INSET, COLOUR_BROWN), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, INVALID_COLOUR, TVW_VIEWPORT), SetMinimalSize(254, 86), SetFill(1, 0), SetResize(1, 1), SetPadding(1, 1, 1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, TVW_INFOPANEL), SetMinimalSize(260, 32), SetResize(1, 0), SetFill(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TVW_CENTERVIEW), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_BUTTON_LOCATION, STR_TOWN_VIEW_CENTER_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TVW_SHOWAUTHORITY), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TOWN_VIEW_LOCAL_AUTHORITY_BUTTON, STR_TOWN_VIEW_LOCAL_AUTHORITY_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TVW_CHANGENAME), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_BUTTON_RENAME, STR_TOWN_VIEW_RENAME_TOOLTIP),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

static const WindowDesc _town_game_view_desc(
	WDP_AUTO, 260, TownViewWindow::TVW_HEIGHT_NORMAL,
	WC_TOWN_VIEW, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_town_game_view_widgets, lengthof(_nested_town_game_view_widgets)
);

static const NWidgetPart _nested_town_editor_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, TVW_CAPTION), SetDataTip(STR_TOWN_VIEW_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TVW_CHANGENAME), SetMinimalSize(76, 14), SetDataTip(STR_BUTTON_RENAME, STR_TOWN_VIEW_RENAME_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(WWT_INSET, COLOUR_BROWN), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, INVALID_COLOUR, TVW_VIEWPORT), SetMinimalSize(254, 86), SetFill(1, 1), SetResize(1, 1), SetPadding(1, 1, 1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, TVW_INFOPANEL), SetMinimalSize(260, 32), SetResize(1, 0), SetFill(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TVW_CENTERVIEW), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_BUTTON_LOCATION, STR_TOWN_VIEW_CENTER_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TVW_EXPAND), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TOWN_VIEW_EXPAND_BUTTON, STR_TOWN_VIEW_EXPAND_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TVW_DELETE), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TOWN_VIEW_DELETE_BUTTON, STR_TOWN_VIEW_DELETE_TOOLTIP),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

static const WindowDesc _town_editor_view_desc(
	WDP_AUTO, 260, TownViewWindow::TVW_HEIGHT_NORMAL,
	WC_TOWN_VIEW, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_town_editor_view_widgets, lengthof(_nested_town_editor_view_widgets)
);

void ShowTownViewWindow(TownID town)
{
	if (_game_mode == GM_EDITOR) {
		AllocateWindowDescFront<TownViewWindow>(&_town_editor_view_desc, town);
	} else {
		AllocateWindowDescFront<TownViewWindow>(&_town_game_view_desc, town);
	}
}

/** Widget numbers of town directory window. */
enum TownDirectoryWidgets {
	TDW_SORTNAME,
	TDW_SORTPOPULATION,
	TDW_CENTERTOWN,
	TDW_SCROLLBAR,
	TDW_BOTTOM_PANEL,
	TDW_BOTTOM_TEXT,
};

static const NWidgetPart _nested_town_directory_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_TOWN_DIRECTORY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TDW_SORTNAME), SetMinimalSize(99, 12), SetDataTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TDW_SORTPOPULATION), SetMinimalSize(97, 12), SetDataTip(STR_SORT_BY_CAPTION_POPULATION, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_BROWN, TDW_CENTERTOWN), SetMinimalSize(196, 0), SetDataTip(0x0, STR_TOWN_DIRECTORY_LIST_TOOLTIP),
							SetFill(1, 0), SetResize(0, 10), SetScrollbar(TDW_SCROLLBAR), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_BROWN, TDW_BOTTOM_PANEL),
				NWidget(WWT_TEXT, COLOUR_BROWN, TDW_BOTTOM_TEXT), SetPadding(2, 0, 0, 2), SetMinimalSize(196, 12), SetFill(1, 0), SetDataTip(STR_TOWN_POPULATION, STR_NULL),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, TDW_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

/** Town directory window class. */
struct TownDirectoryWindow : public Window {
private:
	/* Runtime saved values */
	static Listing last_sorting;
	static const Town *last_town;

	/* Constants for sorting towns */
	static GUITownList::SortFunction * const sorter_funcs[];

	GUITownList towns;

	Scrollbar *vscroll;

	void BuildSortTownList()
	{
		if (this->towns.NeedRebuild()) {
			this->towns.Clear();

			const Town *t;
			FOR_ALL_TOWNS(t) {
				*this->towns.Append() = t;
			}

			this->towns.Compact();
			this->towns.RebuildDone();
			this->vscroll->SetCount(this->towns.Length()); // Update scrollbar as well.
		}
		/* Always sort the towns. */
		this->last_town = NULL;
		this->towns.Sort();
	}

	/** Sort by town name */
	static int CDECL TownNameSorter(const Town * const *a, const Town * const *b)
	{
		static char buf_cache[64];
		const Town *ta = *a;
		const Town *tb = *b;
		char buf[64];

		SetDParam(0, ta->index);
		GetString(buf, STR_TOWN_NAME, lastof(buf));

		/* If 'b' is the same town as in the last round, use the cached value
		 * We do this to speed stuff up ('b' is called with the same value a lot of
		 * times after eachother) */
		if (tb != last_town) {
			last_town = tb;
			SetDParam(0, tb->index);
			GetString(buf_cache, STR_TOWN_NAME, lastof(buf_cache));
		}

		return strnatcmp(buf, buf_cache); // Sort by name (natural sorting).
	}

	/** Sort by population */
	static int CDECL TownPopulationSorter(const Town * const *a, const Town * const *b)
	{
		return (*a)->population - (*b)->population;
	}

public:
	TownDirectoryWindow(const WindowDesc *desc) : Window()
	{
		this->CreateNestedTree(desc);

		this->vscroll = this->GetScrollbar(TDW_SCROLLBAR);

		this->towns.SetListing(this->last_sorting);
		this->towns.SetSortFuncs(TownDirectoryWindow::sorter_funcs);
		this->towns.ForceRebuild();
		this->BuildSortTownList();

		this->FinishInitNested(desc, 0);
	}

	~TownDirectoryWindow()
	{
		this->last_sorting = this->towns.GetListing();
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == TDW_BOTTOM_TEXT) SetDParam(0, GetWorldPopulation());
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case TDW_SORTNAME:
				if (this->towns.SortType() == 0) this->DrawSortButtonState(widget, this->towns.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;

			case TDW_SORTPOPULATION:
				if (this->towns.SortType() != 0) this->DrawSortButtonState(widget, this->towns.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;

			case TDW_CENTERTOWN: {
				int n = 0;
				int y = r.top + WD_FRAMERECT_TOP;
				if (this->towns.Length() == 0) { // No towns available.
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right, y, STR_TOWN_DIRECTORY_NONE);
					break;
				}
				/* At least one town available. */
				for (uint i = this->vscroll->GetPosition(); i < this->towns.Length(); i++) {
					const Town *t = this->towns[i];

					assert(t->xy != INVALID_TILE);

					SetDParam(0, t->index);
					SetDParam(1, t->population);
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_TOWN_DIRECTORY_TOWN);

					y += this->resize.step_height;
					if (++n == this->vscroll->GetCapacity()) break; // max number of towns in 1 window
				}
				break;
			}
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case TDW_SORTNAME:
			case TDW_SORTPOPULATION: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width + WD_SORTBUTTON_ARROW_WIDTH * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
			case TDW_CENTERTOWN: {
				Dimension d = GetStringBoundingBox(STR_TOWN_DIRECTORY_NONE);
				for (uint i = 0; i < this->towns.Length(); i++) {
					const Town *t = this->towns[i];

					assert(t != NULL);

					SetDParam(0, t->index);
					SetDParam(1, 10000000); // 10^7
					d = maxdim(d, GetStringBoundingBox(STR_TOWN_DIRECTORY_TOWN));
				}
				resize->height = d.height;
				d.height *= 5;
				d.width += padding.width + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				d.height += padding.height + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				*size = maxdim(*size, d);
				break;
			}
			case TDW_BOTTOM_TEXT: {
				SetDParam(0, 1000000000); // 10^9
				Dimension d = GetStringBoundingBox(STR_TOWN_POPULATION);
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case TDW_SORTNAME: // Sort by Name ascending/descending
				if (this->towns.SortType() == 0) {
					this->towns.ToggleSortOrder();
				} else {
					this->towns.SetSortType(0);
				}
				this->BuildSortTownList();
				this->SetDirty();
				break;

			case TDW_SORTPOPULATION: // Sort by Population ascending/descending
				if (this->towns.SortType() == 1) {
					this->towns.ToggleSortOrder();
				} else {
					this->towns.SetSortType(1);
				}
				this->BuildSortTownList();
				this->SetDirty();
				break;

			case TDW_CENTERTOWN: { // Click on Town Matrix
				uint id_v = this->vscroll->GetScrolledRowFromWidget(pt.y, this, TDW_CENTERTOWN, WD_FRAMERECT_TOP);
				if (id_v >= this->towns.Length()) return; // click out of town bounds

				const Town *t = this->towns[id_v];
				assert(t != NULL);
				if (_ctrl_pressed) {
					ShowExtraViewPortWindow(t->xy);
				} else {
					ScrollMainWindowToTile(t->xy);
				}
				break;
			}
		}
	}

	virtual void OnHundredthTick()
	{
		this->BuildSortTownList();
		this->SetDirty();
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, TDW_CENTERTOWN);
	}

	virtual void OnInvalidateData(int data)
	{
		if (data == 0) {
			this->towns.ForceRebuild();
		} else {
			this->towns.ForceResort();
		}
		this->BuildSortTownList();
	}
};

Listing TownDirectoryWindow::last_sorting = {false, 0};
const Town *TownDirectoryWindow::last_town = NULL;

/* Available town directory sorting functions */
GUITownList::SortFunction * const TownDirectoryWindow::sorter_funcs[] = {
	&TownNameSorter,
	&TownPopulationSorter,
};

static const WindowDesc _town_directory_desc(
	WDP_AUTO, 208, 202,
	WC_TOWN_DIRECTORY, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_town_directory_widgets, lengthof(_nested_town_directory_widgets)
);

void ShowTownDirectory()
{
	if (BringWindowToFrontById(WC_TOWN_DIRECTORY, 0)) return;
	new TownDirectoryWindow(&_town_directory_desc);
}

void CcFoundTown(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	if (result.Failed()) return;

	SndPlayTileFx(SND_1F_SPLAT, tile);
	if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
}

void CcFoundRandomTown(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	if (result.Succeeded()) ScrollMainWindowToTile(Town::Get(_new_town_id)->xy);
}

/** Widget numbers of town scenario editor window. */
enum TownScenarioEditorWidgets {
	TSEW_BACKGROUND,
	TSEW_NEWTOWN,
	TSEW_RANDOMTOWN,
	TSEW_MANYRANDOMTOWNS,
	TSEW_TOWNNAME_TEXT,
	TSEW_TOWNNAME_EDITBOX,
	TSEW_TOWNNAME_RANDOM,
	TSEW_TOWNSIZE,
	TSEW_SIZE_SMALL,
	TSEW_SIZE_MEDIUM,
	TSEW_SIZE_LARGE,
	TSEW_SIZE_RANDOM,
	TSEW_CITY,
	TSEW_TOWNLAYOUT,
	TSEW_LAYOUT_ORIGINAL,
	TSEW_LAYOUT_BETTER,
	TSEW_LAYOUT_GRID2,
	TSEW_LAYOUT_GRID3,
	TSEW_LAYOUT_RANDOM,
};

static const NWidgetPart _nested_found_town_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_FOUND_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	/* Construct new town(s) buttons. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, TSEW_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 2),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_NEWTOWN), SetMinimalSize(156, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_NEW_TOWN_BUTTON, STR_FOUND_TOWN_NEW_TOWN_TOOLTIP), SetPadding(0, 2, 1, 2),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_RANDOMTOWN), SetMinimalSize(156, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_RANDOM_TOWN_BUTTON, STR_FOUND_TOWN_RANDOM_TOWN_TOOLTIP), SetPadding(0, 2, 1, 2),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_MANYRANDOMTOWNS), SetMinimalSize(156, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_MANY_RANDOM_TOWNS, STR_FOUND_TOWN_RANDOM_TOWNS_TOOLTIP), SetPadding(0, 2, 0, 2),
		/* Town name selection. */
		NWidget(WWT_LABEL, COLOUR_DARK_GREEN, TSEW_TOWNSIZE), SetMinimalSize(156, 14), SetPadding(0, 2, 0, 2), SetDataTip(STR_FOUND_TOWN_NAME_TITLE, STR_NULL),
		NWidget(WWT_EDITBOX, COLOUR_WHITE, TSEW_TOWNNAME_EDITBOX), SetMinimalSize(156, 12), SetPadding(0, 2, 3, 2),
										SetDataTip(STR_FOUND_TOWN_NAME_EDITOR_TITLE, STR_FOUND_TOWN_NAME_EDITOR_HELP),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_TOWNNAME_RANDOM), SetMinimalSize(78, 12), SetPadding(0, 2, 0, 2), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_NAME_RANDOM_BUTTON, STR_FOUND_TOWN_NAME_RANDOM_TOOLTIP),
		/* Town size selection. */
		NWidget(NWID_HORIZONTAL), SetPIP(2, 0, 2),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_LABEL, COLOUR_DARK_GREEN, TSEW_TOWNSIZE), SetMinimalSize(148, 14), SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_TITLE, STR_NULL),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(2, 0, 2),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_SIZE_SMALL), SetMinimalSize(78, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_SMALL_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_SIZE_MEDIUM), SetMinimalSize(78, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_MEDIUM_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 1),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(2, 0, 2),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_SIZE_LARGE), SetMinimalSize(78, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_LARGE_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_SIZE_RANDOM), SetMinimalSize(78, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_SIZE_RANDOM, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_CITY), SetPadding(0, 2, 0, 2), SetMinimalSize(156, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_CITY, STR_FOUND_TOWN_CITY_TOOLTIP), SetFill(1, 0),
		/* Town roads selection. */
		NWidget(NWID_HORIZONTAL), SetPIP(2, 0, 2),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_LABEL, COLOUR_DARK_GREEN, TSEW_TOWNLAYOUT), SetMinimalSize(148, 14), SetDataTip(STR_FOUND_TOWN_ROAD_LAYOUT, STR_NULL),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(2, 0, 2),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_LAYOUT_ORIGINAL), SetMinimalSize(78, 12), SetFill(1, 0), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_ORIGINAL, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_LAYOUT_BETTER), SetMinimalSize(78, 12), SetFill(1, 0), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_BETTER_ROADS, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 1),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(2, 0, 2),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_LAYOUT_GRID2), SetMinimalSize(78, 12), SetFill(1, 0), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_2X2_GRID, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_LAYOUT_GRID3), SetMinimalSize(78, 12), SetFill(1, 0), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_3X3_GRID, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 1),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_LAYOUT_RANDOM), SetPadding(0, 2, 0, 2), SetMinimalSize(0, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_RANDOM, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT), SetFill(1, 0),
		NWidget(NWID_SPACER), SetMinimalSize(0, 2),
	EndContainer(),
};

/** Found a town window class. */
struct FoundTownWindow : QueryStringBaseWindow {
private:
	TownSize town_size;     ///< Selected town size
	TownLayout town_layout; ///< Selected town layout
	bool city;              ///< Are we building a city?
	bool townnamevalid;     ///< Is generated town name valid?
	uint32 townnameparts;   ///< Generated town name
	TownNameParams params;  ///< Town name parameters

public:
	FoundTownWindow(const WindowDesc *desc, WindowNumber window_number) :
			QueryStringBaseWindow(MAX_LENGTH_TOWN_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_TOWN_NAME_CHARS),
			town_size(TSZ_MEDIUM),
			town_layout(_settings_game.economy.town_layout),
			params(_settings_game.game_creation.town_name)
	{
		this->InitNested(desc, window_number);
		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, this->max_chars, MAX_LENGTH_TOWN_NAME_PIXELS);
		this->RandomTownName();
		this->UpdateButtons(true);
	}

	void RandomTownName()
	{
		this->townnamevalid = GenerateTownName(&this->townnameparts);

		if (!this->townnamevalid) {
			this->edit_str_buf[0] = '\0';
		} else {
			GetTownName(this->edit_str_buf, &this->params, this->townnameparts, &this->edit_str_buf[this->edit_str_size - 1]);
		}
		UpdateTextBufferSize(&this->text);
		UpdateOSKOriginalText(this, TSEW_TOWNNAME_EDITBOX);

		this->SetWidgetDirty(TSEW_TOWNNAME_EDITBOX);
	}

	void UpdateButtons(bool check_availability)
	{
		if (check_availability && _game_mode != GM_EDITOR) {
			this->SetWidgetsDisabledState(true, TSEW_RANDOMTOWN, TSEW_MANYRANDOMTOWNS, TSEW_SIZE_LARGE, WIDGET_LIST_END);
			this->SetWidgetsDisabledState(_settings_game.economy.found_town != TF_CUSTOM_LAYOUT,
					TSEW_LAYOUT_ORIGINAL, TSEW_LAYOUT_BETTER, TSEW_LAYOUT_GRID2, TSEW_LAYOUT_GRID3, TSEW_LAYOUT_RANDOM, WIDGET_LIST_END);
			if (_settings_game.economy.found_town != TF_CUSTOM_LAYOUT) town_layout = _settings_game.economy.town_layout;
		}

		for (int i = TSEW_SIZE_SMALL; i <= TSEW_SIZE_RANDOM; i++) {
			this->SetWidgetLoweredState(i, i == TSEW_SIZE_SMALL + this->town_size);
		}

		this->SetWidgetLoweredState(TSEW_CITY, this->city);

		for (int i = TSEW_LAYOUT_ORIGINAL; i <= TSEW_LAYOUT_RANDOM; i++) {
			this->SetWidgetLoweredState(i, i == TSEW_LAYOUT_ORIGINAL + this->town_layout);
		}

		this->SetDirty();
	}

	void ExecuteFoundTownCommand(TileIndex tile, bool random, StringID errstr, CommandCallback cc)
	{
		const char *name = NULL;

		if (!this->townnamevalid) {
			name = this->edit_str_buf;
		} else {
			/* If user changed the name, send it */
			char buf[MAX_LENGTH_TOWN_NAME_CHARS * MAX_CHAR_LENGTH];
			GetTownName(buf, &this->params, this->townnameparts, lastof(buf));
			if (strcmp(buf, this->edit_str_buf) != 0) name = this->edit_str_buf;
		}

		bool success = DoCommandP(tile, this->town_size | this->city << 2 | this->town_layout << 3 | random << 6,
				townnameparts, CMD_FOUND_TOWN | CMD_MSG(errstr), cc, name);

		if (success) this->RandomTownName();
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
		if (!this->IsShaded()) this->DrawEditBox(TSEW_TOWNNAME_EDITBOX);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case TSEW_NEWTOWN:
				HandlePlacePushButton(this, TSEW_NEWTOWN, SPR_CURSOR_TOWN, HT_RECT);
				break;

			case TSEW_RANDOMTOWN:
				this->HandleButtonClick(TSEW_RANDOMTOWN);
				this->ExecuteFoundTownCommand(0, true, STR_ERROR_CAN_T_GENERATE_TOWN, CcFoundRandomTown);
				break;

			case TSEW_TOWNNAME_RANDOM:
				this->RandomTownName();
				this->SetFocusedWidget(TSEW_TOWNNAME_EDITBOX);
				break;

			case TSEW_MANYRANDOMTOWNS:
				this->HandleButtonClick(TSEW_MANYRANDOMTOWNS);

				_generating_world = true;
				UpdateNearestTownForRoadTiles(true);
				if (!GenerateTowns(this->town_layout)) {
					ShowErrorMessage(STR_ERROR_CAN_T_GENERATE_TOWN, STR_ERROR_NO_SPACE_FOR_TOWN, WL_INFO);
				}
				UpdateNearestTownForRoadTiles(false);
				_generating_world = false;
				break;

			case TSEW_SIZE_SMALL: case TSEW_SIZE_MEDIUM: case TSEW_SIZE_LARGE: case TSEW_SIZE_RANDOM:
				this->town_size = (TownSize)(widget - TSEW_SIZE_SMALL);
				this->UpdateButtons(false);
				break;

			case TSEW_CITY:
				this->city ^= true;
				this->SetWidgetLoweredState(TSEW_CITY, this->city);
				this->SetDirty();
				break;

			case TSEW_LAYOUT_ORIGINAL: case TSEW_LAYOUT_BETTER: case TSEW_LAYOUT_GRID2:
			case TSEW_LAYOUT_GRID3: case TSEW_LAYOUT_RANDOM:
				this->town_layout = (TownLayout)(widget - TSEW_LAYOUT_ORIGINAL);
				this->UpdateButtons(false);
				break;
		}
	}

	virtual void OnTimeout()
	{
		this->RaiseWidget(TSEW_RANDOMTOWN);
		this->RaiseWidget(TSEW_MANYRANDOMTOWNS);
		this->SetDirty();
	}

	virtual void OnMouseLoop()
	{
		this->HandleEditBox(TSEW_TOWNNAME_EDITBOX);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		EventState state = ES_NOT_HANDLED;
		if (this->HandleEditBoxKey(TSEW_TOWNNAME_EDITBOX, key, keycode, state) == HEBR_CANCEL) {
			this->UnfocusFocusedWidget();
		}
		return state;
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		this->ExecuteFoundTownCommand(tile, false, STR_ERROR_CAN_T_FOUND_TOWN_HERE, CcFoundTown);
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseButtons();
		this->UpdateButtons(false);
	}

	virtual void OnInvalidateData(int)
	{
		this->UpdateButtons(true);
	}
};

static const WindowDesc _found_town_desc(
	WDP_AUTO, 160, 162,
	WC_FOUND_TOWN, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_found_town_widgets, lengthof(_nested_found_town_widgets)
);

void ShowFoundTownWindow()
{
	if (_game_mode != GM_EDITOR && !Company::IsValidID(_local_company)) return;
	AllocateWindowDescFront<FoundTownWindow>(&_found_town_desc, 0);
}
