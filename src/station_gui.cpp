/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file station_gui.cpp The GUI for stations. */

#include "stdafx.h"
#include "debug.h"
#include "gui.h"
#include "textbuf_gui.h"
#include "company_func.h"
#include "command_func.h"
#include "vehicle_gui.h"
#include "cargotype.h"
#include "station_gui.h"
#include "strings_func.h"
#include "string_func.h"
#include "window_func.h"
#include "viewport_func.h"
#include "widgets/dropdown_func.h"
#include "station_base.h"
#include "waypoint_base.h"
#include "tilehighlight_func.h"
#include "company_base.h"
#include "sortlist_type.h"
#include "core/geometry_func.hpp"
#include "vehiclelist.h"
#include "town.h"
#include "linkgraph/linkgraph.h"
#include "zoom_func.h"

#include "widgets/station_widget.h"

#include "table/strings.h"

#include <set>
#include <vector>

#include "safeguards.h"

/**
 * Calculates and draws the accepted or supplied cargo around the selected tile(s)
 * @param left x position where the string is to be drawn
 * @param right the right most position to draw on
 * @param top y position where the string is to be drawn
 * @param sct which type of cargo is to be displayed (passengers/non-passengers)
 * @param rad radius around selected tile(s) to be searched
 * @param supplies if supplied cargoes should be drawn, else accepted cargoes
 * @return Returns the y value below the string that was drawn
 */
int DrawStationCoverageAreaText(int left, int right, int top, StationCoverageType sct, int rad, bool supplies)
{
	TileIndex tile = TileVirtXY(_thd.pos.x, _thd.pos.y);
	CargoTypes cargo_mask = 0;
	if (_thd.drawstyle == HT_RECT && tile < MapSize()) {
		CargoArray cargoes;
		if (supplies) {
			cargoes = GetProductionAroundTiles(tile, _thd.size.x / TILE_SIZE, _thd.size.y / TILE_SIZE, rad);
		} else {
			cargoes = GetAcceptanceAroundTiles(tile, _thd.size.x / TILE_SIZE, _thd.size.y / TILE_SIZE, rad);
		}

		/* Convert cargo counts to a set of cargo bits, and draw the result. */
		for (CargoID i = 0; i < NUM_CARGO; i++) {
			switch (sct) {
				case SCT_PASSENGERS_ONLY: if (!IsCargoInClass(i, CC_PASSENGERS)) continue; break;
				case SCT_NON_PASSENGERS_ONLY: if (IsCargoInClass(i, CC_PASSENGERS)) continue; break;
				case SCT_ALL: break;
				default: NOT_REACHED();
			}
			if (cargoes[i] >= (supplies ? 1U : 8U)) SetBit(cargo_mask, i);
		}
	}
	SetDParam(0, cargo_mask);
	return DrawStringMultiLine(left, right, top, INT32_MAX, supplies ? STR_STATION_BUILD_SUPPLIES_CARGO : STR_STATION_BUILD_ACCEPTS_CARGO);
}

/**
 * Find stations adjacent to the current tile highlight area, so that existing coverage
 * area can be drawn.
 */
static void FindStationsAroundSelection()
{
	/* With distant join we don't know which station will be selected, so don't show any */
	if (_ctrl_pressed) {
		SetViewportCatchmentStation(nullptr, true);
		return;
	}

	/* Tile area for TileHighlightData */
	TileArea location(TileVirtXY(_thd.pos.x, _thd.pos.y), _thd.size.x / TILE_SIZE - 1, _thd.size.y / TILE_SIZE - 1);

	/* Extended area by one tile */
	uint x = TileX(location.tile);
	uint y = TileY(location.tile);

	int max_c = 1;
	TileArea ta(TileXY(max<int>(0, x - max_c), max<int>(0, y - max_c)), TileXY(min<int>(MapMaxX(), x + location.w + max_c), min<int>(MapMaxY(), y + location.h + max_c)));

	Station *adjacent = nullptr;

	/* Direct loop instead of ForAllStationsAroundTiles as we are not interested in catchment area */
	TILE_AREA_LOOP(tile, ta) {
		if (IsTileType(tile, MP_STATION) && GetTileOwner(tile) == _local_company) {
			Station *st = Station::GetByTile(tile);
			if (st == nullptr) continue;
			if (adjacent != nullptr && st != adjacent) {
				/* Multiple nearby, distant join is required. */
				adjacent = nullptr;
				break;
			}
			adjacent = st;
		}
	}
	SetViewportCatchmentStation(adjacent, true);
}

/**
 * Check whether we need to redraw the station coverage text.
 * If it is needed actually make the window for redrawing.
 * @param w the window to check.
 */
void CheckRedrawStationCoverage(const Window *w)
{
	/* Test if ctrl state changed */
	static bool _last_ctrl_pressed;
	if (_ctrl_pressed != _last_ctrl_pressed) {
		_thd.dirty = 0xff;
		_last_ctrl_pressed = _ctrl_pressed;
	}

	if (_thd.dirty & 1) {
		_thd.dirty &= ~1;
		w->SetDirty();

		if (_settings_client.gui.station_show_coverage && _thd.drawstyle == HT_RECT) {
			FindStationsAroundSelection();
		}
	}
}

/**
 * Draw small boxes of cargo amount and ratings data at the given
 * coordinates. If amount exceeds 576 units, it is shown 'full', same
 * goes for the rating: at above 90% orso (224) it is also 'full'
 *
 * @param left   left most coordinate to draw the box at
 * @param right  right most coordinate to draw the box at
 * @param y      coordinate to draw the box at
 * @param type   Cargo type
 * @param amount Cargo amount
 * @param rating ratings data for that particular cargo
 *
 * @note Each cargo-bar is 16 pixels wide and 6 pixels high
 * @note Each rating 14 pixels wide and 1 pixel high and is 1 pixel below the cargo-bar
 */
static void StationsWndShowStationRating(int left, int right, int y, CargoID type, uint amount, byte rating)
{
	static const uint units_full  = 576; ///< number of units to show station as 'full'
	static const uint rating_full = 224; ///< rating needed so it is shown as 'full'

	const CargoSpec *cs = CargoSpec::Get(type);
	if (!cs->IsValid()) return;

	int colour = cs->rating_colour;
	TextColour tc = GetContrastColour(colour);
	uint w = (minu(amount, units_full) + 5) / 36;

	int height = GetCharacterHeight(FS_SMALL);

	/* Draw total cargo (limited) on station (fits into 16 pixels) */
	if (w != 0) GfxFillRect(left, y, left + w - 1, y + height, colour);

	/* Draw a one pixel-wide bar of additional cargo meter, useful
	 * for stations with only a small amount (<=30) */
	if (w == 0) {
		uint rest = amount / 5;
		if (rest != 0) {
			w += left;
			GfxFillRect(w, y + height - rest, w, y + height, colour);
		}
	}

	DrawString(left + 1, right, y, cs->abbrev, tc);

	/* Draw green/red ratings bar (fits into 14 pixels) */
	y += height + 2;
	GfxFillRect(left + 1, y, left + 14, y, PC_RED);
	rating = minu(rating, rating_full) / 16;
	if (rating != 0) GfxFillRect(left + 1, y, left + rating, y, PC_GREEN);
}

typedef GUIList<const Station*> GUIStationList;

/**
 * The list of stations per company.
 */
class CompanyStationsWindow : public Window
{
protected:
	/* Runtime saved values */
	static Listing last_sorting;
	static byte facilities;               // types of stations of interest
	static bool include_empty;            // whether we should include stations without waiting cargo
	static const CargoTypes cargo_filter_max;
	static CargoTypes cargo_filter;           // bitmap of cargo types to include

	/* Constants for sorting stations */
	static const StringID sorter_names[];
	static GUIStationList::SortFunction * const sorter_funcs[];

	GUIStationList stations;
	Scrollbar *vscroll;

	/**
	 * (Re)Build station list
	 *
	 * @param owner company whose stations are to be in list
	 */
	void BuildStationsList(const Owner owner)
	{
		if (!this->stations.NeedRebuild()) return;

		DEBUG(misc, 3, "Building station list for company %d", owner);

		this->stations.clear();

		for (const Station *st : Station::Iterate()) {
			if (st->owner == owner || (st->owner == OWNER_NONE && HasStationInUse(st->index, true, owner))) {
				if (this->facilities & st->facilities) { // only stations with selected facilities
					int num_waiting_cargo = 0;
					for (CargoID j = 0; j < NUM_CARGO; j++) {
						if (st->goods[j].HasRating()) {
							num_waiting_cargo++; // count number of waiting cargo
							if (HasBit(this->cargo_filter, j)) {
								this->stations.push_back(st);
								break;
							}
						}
					}
					/* stations without waiting cargo */
					if (num_waiting_cargo == 0 && this->include_empty) {
						this->stations.push_back(st);
					}
				}
			}
		}

		this->stations.shrink_to_fit();
		this->stations.RebuildDone();

		this->vscroll->SetCount((uint)this->stations.size()); // Update the scrollbar
	}

	/** Sort stations by their name */
	static bool StationNameSorter(const Station * const &a, const Station * const &b)
	{
		int r = strnatcmp(a->GetCachedName(), b->GetCachedName()); // Sort by name (natural sorting).
		if (r == 0) return a->index < b->index;
		return r < 0;
	}

	/** Sort stations by their type */
	static bool StationTypeSorter(const Station * const &a, const Station * const &b)
	{
		return a->facilities < b->facilities;
	}

	/** Sort stations by their waiting cargo */
	static bool StationWaitingTotalSorter(const Station * const &a, const Station * const &b)
	{
		int diff = 0;

		CargoID j;
		FOR_EACH_SET_CARGO_ID(j, cargo_filter) {
			diff += a->goods[j].cargo.TotalCount() - b->goods[j].cargo.TotalCount();
		}

		return diff < 0;
	}

	/** Sort stations by their available waiting cargo */
	static bool StationWaitingAvailableSorter(const Station * const &a, const Station * const &b)
	{
		int diff = 0;

		CargoID j;
		FOR_EACH_SET_CARGO_ID(j, cargo_filter) {
			diff += a->goods[j].cargo.AvailableCount() - b->goods[j].cargo.AvailableCount();
		}

		return diff < 0;
	}

	/** Sort stations by their rating */
	static bool StationRatingMaxSorter(const Station * const &a, const Station * const &b)
	{
		byte maxr1 = 0;
		byte maxr2 = 0;

		CargoID j;
		FOR_EACH_SET_CARGO_ID(j, cargo_filter) {
			if (a->goods[j].HasRating()) maxr1 = max(maxr1, a->goods[j].rating);
			if (b->goods[j].HasRating()) maxr2 = max(maxr2, b->goods[j].rating);
		}

		return maxr1 < maxr2;
	}

	/** Sort stations by their rating */
	static bool StationRatingMinSorter(const Station * const &a, const Station * const &b)
	{
		byte minr1 = 255;
		byte minr2 = 255;

		for (CargoID j = 0; j < NUM_CARGO; j++) {
			if (!HasBit(cargo_filter, j)) continue;
			if (a->goods[j].HasRating()) minr1 = min(minr1, a->goods[j].rating);
			if (b->goods[j].HasRating()) minr2 = min(minr2, b->goods[j].rating);
		}

		return minr1 > minr2;
	}

	/** Sort the stations list */
	void SortStationsList()
	{
		if (!this->stations.Sort()) return;

		/* Set the modified widget dirty */
		this->SetWidgetDirty(WID_STL_LIST);
	}

public:
	CompanyStationsWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->stations.SetListing(this->last_sorting);
		this->stations.SetSortFuncs(this->sorter_funcs);
		this->stations.ForceRebuild();
		this->stations.NeedResort();
		this->SortStationsList();

		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_STL_SCROLLBAR);
		this->FinishInitNested(window_number);
		this->owner = (Owner)this->window_number;

		const CargoSpec *cs;
		FOR_ALL_SORTED_STANDARD_CARGOSPECS(cs) {
			if (!HasBit(this->cargo_filter, cs->Index())) continue;
			this->LowerWidget(WID_STL_CARGOSTART + index);
		}

		if (this->cargo_filter == this->cargo_filter_max) this->cargo_filter = _cargo_mask;

		for (uint i = 0; i < 5; i++) {
			if (HasBit(this->facilities, i)) this->LowerWidget(i + WID_STL_TRAIN);
		}
		this->SetWidgetLoweredState(WID_STL_NOCARGOWAITING, this->include_empty);

		this->GetWidget<NWidgetCore>(WID_STL_SORTDROPBTN)->widget_data = this->sorter_names[this->stations.SortType()];
	}

	~CompanyStationsWindow()
	{
		this->last_sorting = this->stations.GetListing();
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		switch (widget) {
			case WID_STL_SORTBY: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case WID_STL_SORTDROPBTN: {
				Dimension d = {0, 0};
				for (int i = 0; this->sorter_names[i] != INVALID_STRING_ID; i++) {
					d = maxdim(d, GetStringBoundingBox(this->sorter_names[i]));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case WID_STL_LIST:
				resize->height = FONT_HEIGHT_NORMAL;
				size->height = WD_FRAMERECT_TOP + 5 * resize->height + WD_FRAMERECT_BOTTOM;
				break;

			case WID_STL_TRAIN:
			case WID_STL_TRUCK:
			case WID_STL_BUS:
			case WID_STL_AIRPLANE:
			case WID_STL_SHIP:
				size->height = max<uint>(FONT_HEIGHT_SMALL, 10) + padding.height;
				break;

			case WID_STL_CARGOALL:
			case WID_STL_FACILALL:
			case WID_STL_NOCARGOWAITING: {
				Dimension d = GetStringBoundingBox(widget == WID_STL_NOCARGOWAITING ? STR_ABBREV_NONE : STR_ABBREV_ALL);
				d.width  += padding.width + 2;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			default:
				if (widget >= WID_STL_CARGOSTART) {
					Dimension d = GetStringBoundingBox(_sorted_cargo_specs[widget - WID_STL_CARGOSTART]->abbrev);
					d.width  += padding.width + 2;
					d.height += padding.height;
					*size = maxdim(*size, d);
				}
				break;
		}
	}

	void OnPaint() override
	{
		this->BuildStationsList((Owner)this->window_number);
		this->SortStationsList();

		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		switch (widget) {
			case WID_STL_SORTBY:
				/* draw arrow pointing up/down for ascending/descending sorting */
				this->DrawSortButtonState(WID_STL_SORTBY, this->stations.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;

			case WID_STL_LIST: {
				bool rtl = _current_text_dir == TD_RTL;
				int max = min(this->vscroll->GetPosition() + this->vscroll->GetCapacity(), (uint)this->stations.size());
				int y = r.top + WD_FRAMERECT_TOP;
				for (int i = this->vscroll->GetPosition(); i < max; ++i) { // do until max number of stations of owner
					const Station *st = this->stations[i];
					assert(st->xy != INVALID_TILE);

					/* Do not do the complex check HasStationInUse here, it may be even false
					 * when the order had been removed and the station list hasn't been removed yet */
					assert(st->owner == owner || st->owner == OWNER_NONE);

					SetDParam(0, st->index);
					SetDParam(1, st->facilities);
					int x = DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_STATION_LIST_STATION);
					x += rtl ? -5 : 5;

					/* show cargo waiting and station ratings */
					for (uint j = 0; j < _sorted_standard_cargo_specs_size; j++) {
						CargoID cid = _sorted_cargo_specs[j]->Index();
						if (st->goods[cid].cargo.TotalCount() > 0) {
							/* For RTL we work in exactly the opposite direction. So
							 * decrement the space needed first, then draw to the left
							 * instead of drawing to the left and then incrementing
							 * the space. */
							if (rtl) {
								x -= 20;
								if (x < r.left + WD_FRAMERECT_LEFT) break;
							}
							StationsWndShowStationRating(x, x + 16, y, cid, st->goods[cid].cargo.TotalCount(), st->goods[cid].rating);
							if (!rtl) {
								x += 20;
								if (x > r.right - WD_FRAMERECT_RIGHT) break;
							}
						}
					}
					y += FONT_HEIGHT_NORMAL;
				}

				if (this->vscroll->GetCount() == 0) { // company has no stations
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_STATION_LIST_NONE);
					return;
				}
				break;
			}

			case WID_STL_NOCARGOWAITING: {
				int cg_ofst = this->IsWidgetLowered(widget) ? 2 : 1;
				DrawString(r.left + cg_ofst, r.right + cg_ofst, r.top + cg_ofst, STR_ABBREV_NONE, TC_BLACK, SA_HOR_CENTER);
				break;
			}

			case WID_STL_CARGOALL: {
				int cg_ofst = this->IsWidgetLowered(widget) ? 2 : 1;
				DrawString(r.left + cg_ofst, r.right + cg_ofst, r.top + cg_ofst, STR_ABBREV_ALL, TC_BLACK, SA_HOR_CENTER);
				break;
			}

			case WID_STL_FACILALL: {
				int cg_ofst = this->IsWidgetLowered(widget) ? 2 : 1;
				DrawString(r.left + cg_ofst, r.right + cg_ofst, r.top + cg_ofst, STR_ABBREV_ALL, TC_BLACK, SA_HOR_CENTER);
				break;
			}

			default:
				if (widget >= WID_STL_CARGOSTART) {
					const CargoSpec *cs = _sorted_cargo_specs[widget - WID_STL_CARGOSTART];
					int cg_ofst = HasBit(this->cargo_filter, cs->Index()) ? 2 : 1;
					GfxFillRect(r.left + cg_ofst, r.top + cg_ofst, r.right - 2 + cg_ofst, r.bottom - 2 + cg_ofst, cs->rating_colour);
					TextColour tc = GetContrastColour(cs->rating_colour);
					DrawString(r.left + cg_ofst, r.right + cg_ofst, r.top + cg_ofst, cs->abbrev, tc, SA_HOR_CENTER);
				}
				break;
		}
	}

	void SetStringParameters(int widget) const override
	{
		if (widget == WID_STL_CAPTION) {
			SetDParam(0, this->window_number);
			SetDParam(1, this->vscroll->GetCount());
		}
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		switch (widget) {
			case WID_STL_LIST: {
				uint id_v = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_STL_LIST, 0, FONT_HEIGHT_NORMAL);
				if (id_v >= this->stations.size()) return; // click out of list bound

				const Station *st = this->stations[id_v];
				/* do not check HasStationInUse - it is slow and may be invalid */
				assert(st->owner == (Owner)this->window_number || st->owner == OWNER_NONE);

				if (_ctrl_pressed) {
					ShowExtraViewportWindow(st->xy);
				} else {
					ScrollMainWindowToTile(st->xy);
				}
				break;
			}

			case WID_STL_TRAIN:
			case WID_STL_TRUCK:
			case WID_STL_BUS:
			case WID_STL_AIRPLANE:
			case WID_STL_SHIP:
				if (_ctrl_pressed) {
					ToggleBit(this->facilities, widget - WID_STL_TRAIN);
					this->ToggleWidgetLoweredState(widget);
				} else {
					uint i;
					FOR_EACH_SET_BIT(i, this->facilities) {
						this->RaiseWidget(i + WID_STL_TRAIN);
					}
					this->facilities = 1 << (widget - WID_STL_TRAIN);
					this->LowerWidget(widget);
				}
				this->stations.ForceRebuild();
				this->SetDirty();
				break;

			case WID_STL_FACILALL:
				for (uint i = WID_STL_TRAIN; i <= WID_STL_SHIP; i++) {
					this->LowerWidget(i);
				}

				this->facilities = FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK;
				this->stations.ForceRebuild();
				this->SetDirty();
				break;

			case WID_STL_CARGOALL: {
				for (uint i = 0; i < _sorted_standard_cargo_specs_size; i++) {
					this->LowerWidget(WID_STL_CARGOSTART + i);
				}
				this->LowerWidget(WID_STL_NOCARGOWAITING);

				this->cargo_filter = _cargo_mask;
				this->include_empty = true;
				this->stations.ForceRebuild();
				this->SetDirty();
				break;
			}

			case WID_STL_SORTBY: // flip sorting method asc/desc
				this->stations.ToggleSortOrder();
				this->SetDirty();
				break;

			case WID_STL_SORTDROPBTN: // select sorting criteria dropdown menu
				ShowDropDownMenu(this, this->sorter_names, this->stations.SortType(), WID_STL_SORTDROPBTN, 0, 0);
				break;

			case WID_STL_NOCARGOWAITING:
				if (_ctrl_pressed) {
					this->include_empty = !this->include_empty;
					this->ToggleWidgetLoweredState(WID_STL_NOCARGOWAITING);
				} else {
					for (uint i = 0; i < _sorted_standard_cargo_specs_size; i++) {
						this->RaiseWidget(WID_STL_CARGOSTART + i);
					}

					this->cargo_filter = 0;
					this->include_empty = true;

					this->LowerWidget(WID_STL_NOCARGOWAITING);
				}
				this->stations.ForceRebuild();
				this->SetDirty();
				break;

			default:
				if (widget >= WID_STL_CARGOSTART) { // change cargo_filter
					/* Determine the selected cargo type */
					const CargoSpec *cs = _sorted_cargo_specs[widget - WID_STL_CARGOSTART];

					if (_ctrl_pressed) {
						ToggleBit(this->cargo_filter, cs->Index());
						this->ToggleWidgetLoweredState(widget);
					} else {
						for (uint i = 0; i < _sorted_standard_cargo_specs_size; i++) {
							this->RaiseWidget(WID_STL_CARGOSTART + i);
						}
						this->RaiseWidget(WID_STL_NOCARGOWAITING);

						this->cargo_filter = 0;
						this->include_empty = false;

						SetBit(this->cargo_filter, cs->Index());
						this->LowerWidget(widget);
					}
					this->stations.ForceRebuild();
					this->SetDirty();
				}
				break;
		}
	}

	void OnDropdownSelect(int widget, int index) override
	{
		if (this->stations.SortType() != index) {
			this->stations.SetSortType(index);

			/* Display the current sort variant */
			this->GetWidget<NWidgetCore>(WID_STL_SORTDROPBTN)->widget_data = this->sorter_names[this->stations.SortType()];

			this->SetDirty();
		}
	}

	void OnGameTick() override
	{
		if (this->stations.NeedResort()) {
			DEBUG(misc, 3, "Periodic rebuild station list company %d", this->window_number);
			this->SetDirty();
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_STL_LIST, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		if (data == 0) {
			/* This needs to be done in command-scope to enforce rebuilding before resorting invalid data */
			this->stations.ForceRebuild();
		} else {
			this->stations.ForceResort();
		}
	}
};

Listing CompanyStationsWindow::last_sorting = {false, 0};
byte CompanyStationsWindow::facilities = FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK;
bool CompanyStationsWindow::include_empty = true;
const CargoTypes CompanyStationsWindow::cargo_filter_max = ALL_CARGOTYPES;
CargoTypes CompanyStationsWindow::cargo_filter = ALL_CARGOTYPES;

/* Available station sorting functions */
GUIStationList::SortFunction * const CompanyStationsWindow::sorter_funcs[] = {
	&StationNameSorter,
	&StationTypeSorter,
	&StationWaitingTotalSorter,
	&StationWaitingAvailableSorter,
	&StationRatingMaxSorter,
	&StationRatingMinSorter
};

/* Names of the sorting functions */
const StringID CompanyStationsWindow::sorter_names[] = {
	STR_SORT_BY_NAME,
	STR_SORT_BY_FACILITY,
	STR_SORT_BY_WAITING_TOTAL,
	STR_SORT_BY_WAITING_AVAILABLE,
	STR_SORT_BY_RATING_MAX,
	STR_SORT_BY_RATING_MIN,
	INVALID_STRING_ID
};

/**
 * Make a horizontal row of cargo buttons, starting at widget #WID_STL_CARGOSTART.
 * @param biggest_index Pointer to store biggest used widget number of the buttons.
 * @return Horizontal row.
 */
static NWidgetBase *CargoWidgets(int *biggest_index)
{
	NWidgetHorizontal *container = new NWidgetHorizontal();

	for (uint i = 0; i < _sorted_standard_cargo_specs_size; i++) {
		NWidgetBackground *panel = new NWidgetBackground(WWT_PANEL, COLOUR_GREY, WID_STL_CARGOSTART + i);
		panel->SetMinimalSize(14, 11);
		panel->SetResize(0, 0);
		panel->SetFill(0, 1);
		panel->SetDataTip(0, STR_STATION_LIST_USE_CTRL_TO_SELECT_MORE);
		container->Add(panel);
	}
	*biggest_index = WID_STL_CARGOSTART + _sorted_standard_cargo_specs_size;
	return container;
}

static const NWidgetPart _nested_company_stations_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_STL_CAPTION), SetDataTip(STR_STATION_LIST_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_STL_TRAIN), SetMinimalSize(14, 11), SetDataTip(STR_TRAIN, STR_STATION_LIST_USE_CTRL_TO_SELECT_MORE), SetFill(0, 1),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_STL_TRUCK), SetMinimalSize(14, 11), SetDataTip(STR_LORRY, STR_STATION_LIST_USE_CTRL_TO_SELECT_MORE), SetFill(0, 1),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_STL_BUS), SetMinimalSize(14, 11), SetDataTip(STR_BUS, STR_STATION_LIST_USE_CTRL_TO_SELECT_MORE), SetFill(0, 1),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_STL_SHIP), SetMinimalSize(14, 11), SetDataTip(STR_SHIP, STR_STATION_LIST_USE_CTRL_TO_SELECT_MORE), SetFill(0, 1),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_STL_AIRPLANE), SetMinimalSize(14, 11), SetDataTip(STR_PLANE, STR_STATION_LIST_USE_CTRL_TO_SELECT_MORE), SetFill(0, 1),
		NWidget(WWT_PUSHBTN, COLOUR_GREY, WID_STL_FACILALL), SetMinimalSize(14, 11), SetDataTip(0x0, STR_STATION_LIST_SELECT_ALL_FACILITIES), SetFill(0, 1),
		NWidget(WWT_PANEL, COLOUR_GREY), SetMinimalSize(5, 11), SetFill(0, 1), EndContainer(),
		NWidgetFunction(CargoWidgets),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_STL_NOCARGOWAITING), SetMinimalSize(14, 11), SetDataTip(0x0, STR_STATION_LIST_NO_WAITING_CARGO), SetFill(0, 1), EndContainer(),
		NWidget(WWT_PUSHBTN, COLOUR_GREY, WID_STL_CARGOALL), SetMinimalSize(14, 11), SetDataTip(0x0, STR_STATION_LIST_SELECT_ALL_TYPES), SetFill(0, 1),
		NWidget(WWT_PANEL, COLOUR_GREY), SetDataTip(0x0, STR_NULL), SetResize(1, 0), SetFill(1, 1), EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_STL_SORTBY), SetMinimalSize(81, 12), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
		NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_STL_SORTDROPBTN), SetMinimalSize(163, 12), SetDataTip(STR_SORT_BY_NAME, STR_TOOLTIP_SORT_CRITERIA), // widget_data gets overwritten.
		NWidget(WWT_PANEL, COLOUR_GREY), SetDataTip(0x0, STR_NULL), SetResize(1, 0), SetFill(1, 1), EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_STL_LIST), SetMinimalSize(346, 125), SetResize(1, 10), SetDataTip(0x0, STR_STATION_LIST_TOOLTIP), SetScrollbar(WID_STL_SCROLLBAR), EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_STL_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _company_stations_desc(
	WDP_AUTO, "list_stations", 358, 162,
	WC_STATION_LIST, WC_NONE,
	0,
	_nested_company_stations_widgets, lengthof(_nested_company_stations_widgets)
);

/**
 * Opens window with list of company's stations
 *
 * @param company whose stations' list show
 */
void ShowCompanyStations(CompanyID company)
{
	if (!Company::IsValidID(company)) return;

	AllocateWindowDescFront<CompanyStationsWindow>(&_company_stations_desc, company);
}

static const NWidgetPart _nested_station_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_SV_CAPTION), SetDataTip(STR_STATION_VIEW_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_SV_GROUP), SetMinimalSize(81, 12), SetFill(1, 1), SetDataTip(STR_STATION_VIEW_GROUP, 0x0),
		NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_SV_GROUP_BY), SetMinimalSize(168, 12), SetResize(1, 0), SetFill(0, 1), SetDataTip(0x0, STR_TOOLTIP_GROUP_ORDER),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SV_SORT_ORDER), SetMinimalSize(81, 12), SetFill(1, 1), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
		NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_SV_SORT_BY), SetMinimalSize(168, 12), SetResize(1, 0), SetFill(0, 1), SetDataTip(0x0, STR_TOOLTIP_SORT_CRITERIA),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_SV_WAITING), SetMinimalSize(237, 44), SetResize(1, 10), SetScrollbar(WID_SV_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_SV_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_SV_ACCEPT_RATING_LIST), SetMinimalSize(249, 23), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SV_LOCATION), SetMinimalSize(45, 12), SetResize(1, 0), SetFill(1, 1),
					SetDataTip(STR_BUTTON_LOCATION, STR_STATION_VIEW_CENTER_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SV_ACCEPTS_RATINGS), SetMinimalSize(46, 12), SetResize(1, 0), SetFill(1, 1),
					SetDataTip(STR_STATION_VIEW_RATINGS_BUTTON, STR_STATION_VIEW_RATINGS_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SV_RENAME), SetMinimalSize(45, 12), SetResize(1, 0), SetFill(1, 1),
					SetDataTip(STR_BUTTON_RENAME, STR_STATION_VIEW_RENAME_TOOLTIP),
		EndContainer(),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_SV_CLOSE_AIRPORT), SetMinimalSize(45, 12), SetResize(1, 0), SetFill(1, 1),
				SetDataTip(STR_STATION_VIEW_CLOSE_AIRPORT, STR_STATION_VIEW_CLOSE_AIRPORT_TOOLTIP),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_SV_CATCHMENT), SetMinimalSize(14, 12), SetFill(0, 1), SetDataTip(STR_BUTTON_CATCHMENT, STR_TOOLTIP_CATCHMENT),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SV_TRAINS), SetMinimalSize(14, 12), SetFill(0, 1), SetDataTip(STR_TRAIN, STR_STATION_VIEW_SCHEDULED_TRAINS_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SV_ROADVEHS), SetMinimalSize(14, 12), SetFill(0, 1), SetDataTip(STR_LORRY, STR_STATION_VIEW_SCHEDULED_ROAD_VEHICLES_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SV_SHIPS), SetMinimalSize(14, 12), SetFill(0, 1), SetDataTip(STR_SHIP, STR_STATION_VIEW_SCHEDULED_SHIPS_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SV_PLANES),  SetMinimalSize(14, 12), SetFill(0, 1), SetDataTip(STR_PLANE, STR_STATION_VIEW_SCHEDULED_AIRCRAFT_TOOLTIP),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

/**
 * Draws icons of waiting cargo in the StationView window
 *
 * @param i type of cargo
 * @param waiting number of waiting units
 * @param left  left most coordinate to draw on
 * @param right right most coordinate to draw on
 * @param y y coordinate
 */
static void DrawCargoIcons(CargoID i, uint waiting, int left, int right, int y)
{
	int width = ScaleGUITrad(10);
	uint num = min((waiting + (width / 2)) / width, (right - left) / width); // maximum is width / 10 icons so it won't overflow
	if (num == 0) return;

	SpriteID sprite = CargoSpec::Get(i)->GetCargoIcon();

	int x = _current_text_dir == TD_RTL ? left : right - num * width;
	do {
		DrawSprite(sprite, PAL_NONE, x, y);
		x += width;
	} while (--num);
}

enum SortOrder {
	SO_DESCENDING,
	SO_ASCENDING
};

class CargoDataEntry;

enum CargoSortType {
	ST_AS_GROUPING,    ///< by the same principle the entries are being grouped
	ST_COUNT,          ///< by amount of cargo
	ST_STATION_STRING, ///< by station name
	ST_STATION_ID,     ///< by station id
	ST_CARGO_ID,       ///< by cargo id
};

class CargoSorter {
public:
	CargoSorter(CargoSortType t = ST_STATION_ID, SortOrder o = SO_ASCENDING) : type(t), order(o) {}
	CargoSortType GetSortType() {return this->type;}
	bool operator()(const CargoDataEntry *cd1, const CargoDataEntry *cd2) const;

private:
	CargoSortType type;
	SortOrder order;

	template<class Tid>
	bool SortId(Tid st1, Tid st2) const;
	bool SortCount(const CargoDataEntry *cd1, const CargoDataEntry *cd2) const;
	bool SortStation (StationID st1, StationID st2) const;
};

typedef std::set<CargoDataEntry *, CargoSorter> CargoDataSet;

/**
 * A cargo data entry representing one possible row in the station view window's
 * top part. Cargo data entries form a tree where each entry can have several
 * children. Parents keep track of the sums of their childrens' cargo counts.
 */
class CargoDataEntry {
public:
	CargoDataEntry();
	~CargoDataEntry();

	/**
	 * Insert a new child or retrieve an existing child using a station ID as ID.
	 * @param station ID of the station for which an entry shall be created or retrieved
	 * @return a child entry associated with the given station.
	 */
	CargoDataEntry *InsertOrRetrieve(StationID station)
	{
		return this->InsertOrRetrieve<StationID>(station);
	}

	/**
	 * Insert a new child or retrieve an existing child using a cargo ID as ID.
	 * @param cargo ID of the cargo for which an entry shall be created or retrieved
	 * @return a child entry associated with the given cargo.
	 */
	CargoDataEntry *InsertOrRetrieve(CargoID cargo)
	{
		return this->InsertOrRetrieve<CargoID>(cargo);
	}

	void Update(uint count);

	/**
	 * Remove a child associated with the given station.
	 * @param station ID of the station for which the child should be removed.
	 */
	void Remove(StationID station)
	{
		CargoDataEntry t(station);
		this->Remove(&t);
	}

	/**
	 * Remove a child associated with the given cargo.
	 * @param cargo ID of the cargo for which the child should be removed.
	 */
	void Remove(CargoID cargo)
	{
		CargoDataEntry t(cargo);
		this->Remove(&t);
	}

	/**
	 * Retrieve a child for the given station. Return nullptr if it doesn't exist.
	 * @param station ID of the station the child we're looking for is associated with.
	 * @return a child entry for the given station or nullptr.
	 */
	CargoDataEntry *Retrieve(StationID station) const
	{
		CargoDataEntry t(station);
		return this->Retrieve(this->children->find(&t));
	}

	/**
	 * Retrieve a child for the given cargo. Return nullptr if it doesn't exist.
	 * @param cargo ID of the cargo the child we're looking for is associated with.
	 * @return a child entry for the given cargo or nullptr.
	 */
	CargoDataEntry *Retrieve(CargoID cargo) const
	{
		CargoDataEntry t(cargo);
		return this->Retrieve(this->children->find(&t));
	}

	void Resort(CargoSortType type, SortOrder order);

	/**
	 * Get the station ID for this entry.
	 */
	StationID GetStation() const { return this->station; }

	/**
	 * Get the cargo ID for this entry.
	 */
	CargoID GetCargo() const { return this->cargo; }

	/**
	 * Get the cargo count for this entry.
	 */
	uint GetCount() const { return this->count; }

	/**
	 * Get the parent entry for this entry.
	 */
	CargoDataEntry *GetParent() const { return this->parent; }

	/**
	 * Get the number of children for this entry.
	 */
	uint GetNumChildren() const { return this->num_children; }

	/**
	 * Get an iterator pointing to the begin of the set of children.
	 */
	CargoDataSet::iterator Begin() const { return this->children->begin(); }

	/**
	 * Get an iterator pointing to the end of the set of children.
	 */
	CargoDataSet::iterator End() const { return this->children->end(); }

	/**
	 * Has this entry transfers.
	 */
	bool HasTransfers() const { return this->transfers; }

	/**
	 * Set the transfers state.
	 */
	void SetTransfers(bool value) { this->transfers = value; }

	void Clear();
private:

	CargoDataEntry(StationID st, uint c, CargoDataEntry *p);
	CargoDataEntry(CargoID car, uint c, CargoDataEntry *p);
	CargoDataEntry(StationID st);
	CargoDataEntry(CargoID car);

	CargoDataEntry *Retrieve(CargoDataSet::iterator i) const;

	template<class Tid>
	CargoDataEntry *InsertOrRetrieve(Tid s);

	void Remove(CargoDataEntry *comp);
	void IncrementSize();

	CargoDataEntry *parent;   ///< the parent of this entry.
	const union {
		StationID station;    ///< ID of the station this entry is associated with.
		struct {
			CargoID cargo;    ///< ID of the cargo this entry is associated with.
			bool transfers;   ///< If there are transfers for this cargo.
		};
	};
	uint num_children;        ///< the number of subentries belonging to this entry.
	uint count;               ///< sum of counts of all children or amount of cargo for this entry.
	CargoDataSet *children;   ///< the children of this entry.
};

CargoDataEntry::CargoDataEntry() :
	parent(nullptr),
	station(INVALID_STATION),
	num_children(0),
	count(0),
	children(new CargoDataSet(CargoSorter(ST_CARGO_ID)))
{}

CargoDataEntry::CargoDataEntry(CargoID cargo, uint count, CargoDataEntry *parent) :
	parent(parent),
	cargo(cargo),
	num_children(0),
	count(count),
	children(new CargoDataSet)
{}

CargoDataEntry::CargoDataEntry(StationID station, uint count, CargoDataEntry *parent) :
	parent(parent),
	station(station),
	num_children(0),
	count(count),
	children(new CargoDataSet)
{}

CargoDataEntry::CargoDataEntry(StationID station) :
	parent(nullptr),
	station(station),
	num_children(0),
	count(0),
	children(nullptr)
{}

CargoDataEntry::CargoDataEntry(CargoID cargo) :
	parent(nullptr),
	cargo(cargo),
	num_children(0),
	count(0),
	children(nullptr)
{}

CargoDataEntry::~CargoDataEntry()
{
	this->Clear();
	delete this->children;
}

/**
 * Delete all subentries, reset count and num_children and adapt parent's count.
 */
void CargoDataEntry::Clear()
{
	if (this->children != nullptr) {
		for (CargoDataSet::iterator i = this->children->begin(); i != this->children->end(); ++i) {
			assert(*i != this);
			delete *i;
		}
		this->children->clear();
	}
	if (this->parent != nullptr) this->parent->count -= this->count;
	this->count = 0;
	this->num_children = 0;
}

/**
 * Remove a subentry from this one and delete it.
 * @param child the entry to be removed. This may also be a synthetic entry
 * which only contains the ID of the entry to be removed. In this case child is
 * not deleted.
 */
void CargoDataEntry::Remove(CargoDataEntry *child)
{
	CargoDataSet::iterator i = this->children->find(child);
	if (i != this->children->end()) {
		delete *i;
		this->children->erase(i);
	}
}

/**
 * Retrieve a subentry or insert it if it doesn't exist, yet.
 * @tparam ID type of ID: either StationID or CargoID
 * @param child_id ID of the child to be inserted or retrieved.
 * @return the new or retrieved subentry
 */
template<class Tid>
CargoDataEntry *CargoDataEntry::InsertOrRetrieve(Tid child_id)
{
	CargoDataEntry tmp(child_id);
	CargoDataSet::iterator i = this->children->find(&tmp);
	if (i == this->children->end()) {
		IncrementSize();
		return *(this->children->insert(new CargoDataEntry(child_id, 0, this)).first);
	} else {
		CargoDataEntry *ret = *i;
		assert(this->children->value_comp().GetSortType() != ST_COUNT);
		return ret;
	}
}

/**
 * Update the count for this entry and propagate the change to the parent entry
 * if there is one.
 * @param count the amount to be added to this entry
 */
void CargoDataEntry::Update(uint count)
{
	this->count += count;
	if (this->parent != nullptr) this->parent->Update(count);
}

/**
 * Increment
 */
void CargoDataEntry::IncrementSize()
{
	 ++this->num_children;
	 if (this->parent != nullptr) this->parent->IncrementSize();
}

void CargoDataEntry::Resort(CargoSortType type, SortOrder order)
{
	CargoDataSet *new_subs = new CargoDataSet(this->children->begin(), this->children->end(), CargoSorter(type, order));
	delete this->children;
	this->children = new_subs;
}

CargoDataEntry *CargoDataEntry::Retrieve(CargoDataSet::iterator i) const
{
	if (i == this->children->end()) {
		return nullptr;
	} else {
		assert(this->children->value_comp().GetSortType() != ST_COUNT);
		return *i;
	}
}

bool CargoSorter::operator()(const CargoDataEntry *cd1, const CargoDataEntry *cd2) const
{
	switch (this->type) {
		case ST_STATION_ID:
			return this->SortId<StationID>(cd1->GetStation(), cd2->GetStation());
		case ST_CARGO_ID:
			return this->SortId<CargoID>(cd1->GetCargo(), cd2->GetCargo());
		case ST_COUNT:
			return this->SortCount(cd1, cd2);
		case ST_STATION_STRING:
			return this->SortStation(cd1->GetStation(), cd2->GetStation());
		default:
			NOT_REACHED();
	}
}

template<class Tid>
bool CargoSorter::SortId(Tid st1, Tid st2) const
{
	return (this->order == SO_ASCENDING) ? st1 < st2 : st2 < st1;
}

bool CargoSorter::SortCount(const CargoDataEntry *cd1, const CargoDataEntry *cd2) const
{
	uint c1 = cd1->GetCount();
	uint c2 = cd2->GetCount();
	if (c1 == c2) {
		return this->SortStation(cd1->GetStation(), cd2->GetStation());
	} else if (this->order == SO_ASCENDING) {
		return c1 < c2;
	} else {
		return c2 < c1;
	}
}

bool CargoSorter::SortStation(StationID st1, StationID st2) const
{
	if (!Station::IsValidID(st1)) {
		return Station::IsValidID(st2) ? this->order == SO_ASCENDING : this->SortId(st1, st2);
	} else if (!Station::IsValidID(st2)) {
		return order == SO_DESCENDING;
	}

	int res = strnatcmp(Station::Get(st1)->GetCachedName(), Station::Get(st2)->GetCachedName()); // Sort by name (natural sorting).
	if (res == 0) {
		return this->SortId(st1, st2);
	} else {
		return (this->order == SO_ASCENDING) ? res < 0 : res > 0;
	}
}

/**
 * The StationView window
 */
struct StationViewWindow : public Window {
	/**
	 * A row being displayed in the cargo view (as opposed to being "hidden" behind a plus sign).
	 */
	struct RowDisplay {
		RowDisplay(CargoDataEntry *f, StationID n) : filter(f), next_station(n) {}
		RowDisplay(CargoDataEntry *f, CargoID n) : filter(f), next_cargo(n) {}

		/**
		 * Parent of the cargo entry belonging to the row.
		 */
		CargoDataEntry *filter;
		union {
			/**
			 * ID of the station belonging to the entry actually displayed if it's to/from/via.
			 */
			StationID next_station;

			/**
			 * ID of the cargo belonging to the entry actually displayed if it's cargo.
			 */
			CargoID next_cargo;
		};
	};

	typedef std::vector<RowDisplay> CargoDataVector;

	static const int NUM_COLUMNS = 4; ///< Number of "columns" in the cargo view: cargo, from, via, to

	/**
	 * Type of data invalidation.
	 */
	enum Invalidation {
		INV_FLOWS = 0x100, ///< The planned flows have been recalculated and everything has to be updated.
		INV_CARGO = 0x200  ///< Some cargo has been added or removed.
	};

	/**
	 * Type of grouping used in each of the "columns".
	 */
	enum Grouping {
		GR_SOURCE,      ///< Group by source of cargo ("from").
		GR_NEXT,        ///< Group by next station ("via").
		GR_DESTINATION, ///< Group by estimated final destination ("to").
		GR_CARGO,       ///< Group by cargo type.
	};

	/**
	 * Display mode of the cargo view.
	 */
	enum Mode {
		MODE_WAITING, ///< Show cargo waiting at the station.
		MODE_PLANNED  ///< Show cargo planned to pass through the station.
	};

	uint expand_shrink_width;     ///< The width allocated to the expand/shrink 'button'
	int rating_lines;             ///< Number of lines in the cargo ratings view.
	int accepts_lines;            ///< Number of lines in the accepted cargo view.
	Scrollbar *vscroll;

	/** Height of the #WID_SV_ACCEPT_RATING_LIST widget for different views. */
	enum AcceptListHeight {
		ALH_RATING  = 13, ///< Height of the cargo ratings view.
		ALH_ACCEPTS = 3,  ///< Height of the accepted cargo view.
	};

	static const StringID _sort_names[];  ///< Names of the sorting options in the dropdown.
	static const StringID _group_names[]; ///< Names of the grouping options in the dropdown.

	/**
	 * Sort types of the different 'columns'.
	 * In fact only ST_COUNT and ST_AS_GROUPING are active and you can only
	 * sort all the columns in the same way. The other options haven't been
	 * included in the GUI due to lack of space.
	 */
	CargoSortType sortings[NUM_COLUMNS];

	/** Sort order (ascending/descending) for the 'columns'. */
	SortOrder sort_orders[NUM_COLUMNS];

	int scroll_to_row;                  ///< If set, scroll the main viewport to the station pointed to by this row.
	int grouping_index;                 ///< Currently selected entry in the grouping drop down.
	Mode current_mode;                  ///< Currently selected display mode of cargo view.
	Grouping groupings[NUM_COLUMNS];    ///< Grouping modes for the different columns.

	CargoDataEntry expanded_rows;       ///< Parent entry of currently expanded rows.
	CargoDataEntry cached_destinations; ///< Cache for the flows passing through this station.
	CargoDataVector displayed_rows;     ///< Parent entry of currently displayed rows (including collapsed ones).

	StationViewWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc),
		scroll_to_row(INT_MAX), grouping_index(0)
	{
		this->rating_lines  = ALH_RATING;
		this->accepts_lines = ALH_ACCEPTS;

		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_SV_SCROLLBAR);
		/* Nested widget tree creation is done in two steps to ensure that this->GetWidget<NWidgetCore>(WID_SV_ACCEPTS_RATINGS) exists in UpdateWidgetSize(). */
		this->FinishInitNested(window_number);

		this->groupings[0] = GR_CARGO;
		this->sortings[0] = ST_AS_GROUPING;
		this->SelectGroupBy(_settings_client.gui.station_gui_group_order);
		this->SelectSortBy(_settings_client.gui.station_gui_sort_by);
		this->sort_orders[0] = SO_ASCENDING;
		this->SelectSortOrder((SortOrder)_settings_client.gui.station_gui_sort_order);
		this->owner = Station::Get(window_number)->owner;
	}

	~StationViewWindow()
	{
		DeleteWindowById(WC_TRAINS_LIST,   VehicleListIdentifier(VL_STATION_LIST, VEH_TRAIN,    this->owner, this->window_number).Pack(), false);
		DeleteWindowById(WC_ROADVEH_LIST,  VehicleListIdentifier(VL_STATION_LIST, VEH_ROAD,     this->owner, this->window_number).Pack(), false);
		DeleteWindowById(WC_SHIPS_LIST,    VehicleListIdentifier(VL_STATION_LIST, VEH_SHIP,     this->owner, this->window_number).Pack(), false);
		DeleteWindowById(WC_AIRCRAFT_LIST, VehicleListIdentifier(VL_STATION_LIST, VEH_AIRCRAFT, this->owner, this->window_number).Pack(), false);

		SetViewportCatchmentStation(Station::Get(this->window_number), false);
	}

	/**
	 * Show a certain cargo entry characterized by source/next/dest station, cargo ID and amount of cargo at the
	 * right place in the cargo view. I.e. update as many rows as are expanded following that characterization.
	 * @param data Root entry of the tree.
	 * @param cargo Cargo ID of the entry to be shown.
	 * @param source Source station of the entry to be shown.
	 * @param next Next station the cargo to be shown will visit.
	 * @param dest Final destination of the cargo to be shown.
	 * @param count Amount of cargo to be shown.
	 */
	void ShowCargo(CargoDataEntry *data, CargoID cargo, StationID source, StationID next, StationID dest, uint count)
	{
		if (count == 0) return;
		bool auto_distributed = _settings_game.linkgraph.GetDistributionType(cargo) != DT_MANUAL;
		const CargoDataEntry *expand = &this->expanded_rows;
		for (int i = 0; i < NUM_COLUMNS && expand != nullptr; ++i) {
			switch (groupings[i]) {
				case GR_CARGO:
					assert(i == 0);
					data = data->InsertOrRetrieve(cargo);
					data->SetTransfers(source != this->window_number);
					expand = expand->Retrieve(cargo);
					break;
				case GR_SOURCE:
					if (auto_distributed || source != this->window_number) {
						data = data->InsertOrRetrieve(source);
						expand = expand->Retrieve(source);
					}
					break;
				case GR_NEXT:
					if (auto_distributed) {
						data = data->InsertOrRetrieve(next);
						expand = expand->Retrieve(next);
					}
					break;
				case GR_DESTINATION:
					if (auto_distributed) {
						data = data->InsertOrRetrieve(dest);
						expand = expand->Retrieve(dest);
					}
					break;
			}
		}
		data->Update(count);
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		switch (widget) {
			case WID_SV_WAITING:
				resize->height = FONT_HEIGHT_NORMAL;
				size->height = WD_FRAMERECT_TOP + 4 * resize->height + WD_FRAMERECT_BOTTOM;
				this->expand_shrink_width = max(GetStringBoundingBox("-").width, GetStringBoundingBox("+").width) + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				break;

			case WID_SV_ACCEPT_RATING_LIST:
				size->height = WD_FRAMERECT_TOP + ((this->GetWidget<NWidgetCore>(WID_SV_ACCEPTS_RATINGS)->widget_data == STR_STATION_VIEW_RATINGS_BUTTON) ? this->accepts_lines : this->rating_lines) * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM;
				break;

			case WID_SV_CLOSE_AIRPORT:
				if (!(Station::Get(this->window_number)->facilities & FACIL_AIRPORT)) {
					/* Hide 'Close Airport' button if no airport present. */
					size->width = 0;
					resize->width = 0;
					fill->width = 0;
				}
				break;
		}
	}

	void OnPaint() override
	{
		const Station *st = Station::Get(this->window_number);
		CargoDataEntry cargo;
		BuildCargoList(&cargo, st);

		this->vscroll->SetCount(cargo.GetNumChildren()); // update scrollbar

		/* disable some buttons */
		this->SetWidgetDisabledState(WID_SV_RENAME,   st->owner != _local_company);
		this->SetWidgetDisabledState(WID_SV_TRAINS,   !(st->facilities & FACIL_TRAIN));
		this->SetWidgetDisabledState(WID_SV_ROADVEHS, !(st->facilities & FACIL_TRUCK_STOP) && !(st->facilities & FACIL_BUS_STOP));
		this->SetWidgetDisabledState(WID_SV_SHIPS,    !(st->facilities & FACIL_DOCK));
		this->SetWidgetDisabledState(WID_SV_PLANES,   !(st->facilities & FACIL_AIRPORT));
		this->SetWidgetDisabledState(WID_SV_CLOSE_AIRPORT, !(st->facilities & FACIL_AIRPORT) || st->owner != _local_company || st->owner == OWNER_NONE); // Also consider SE, where _local_company == OWNER_NONE
		this->SetWidgetLoweredState(WID_SV_CLOSE_AIRPORT, (st->facilities & FACIL_AIRPORT) && (st->airport.flags & AIRPORT_CLOSED_block) != 0);

		extern const Station *_viewport_highlight_station;
		this->SetWidgetDisabledState(WID_SV_CATCHMENT, st->facilities == FACIL_NONE);
		this->SetWidgetLoweredState(WID_SV_CATCHMENT, _viewport_highlight_station == st);

		this->DrawWidgets();

		if (!this->IsShaded()) {
			/* Draw 'accepted cargo' or 'cargo ratings'. */
			const NWidgetBase *wid = this->GetWidget<NWidgetBase>(WID_SV_ACCEPT_RATING_LIST);
			const Rect r = {(int)wid->pos_x, (int)wid->pos_y, (int)(wid->pos_x + wid->current_x - 1), (int)(wid->pos_y + wid->current_y - 1)};
			if (this->GetWidget<NWidgetCore>(WID_SV_ACCEPTS_RATINGS)->widget_data == STR_STATION_VIEW_RATINGS_BUTTON) {
				int lines = this->DrawAcceptedCargo(r);
				if (lines > this->accepts_lines) { // Resize the widget, and perform re-initialization of the window.
					this->accepts_lines = lines;
					this->ReInit();
					return;
				}
			} else {
				int lines = this->DrawCargoRatings(r);
				if (lines > this->rating_lines) { // Resize the widget, and perform re-initialization of the window.
					this->rating_lines = lines;
					this->ReInit();
					return;
				}
			}

			/* Draw arrow pointing up/down for ascending/descending sorting */
			this->DrawSortButtonState(WID_SV_SORT_ORDER, sort_orders[1] == SO_ASCENDING ? SBS_UP : SBS_DOWN);

			int pos = this->vscroll->GetPosition();

			int maxrows = this->vscroll->GetCapacity();

			displayed_rows.clear();

			/* Draw waiting cargo. */
			NWidgetBase *nwi = this->GetWidget<NWidgetBase>(WID_SV_WAITING);
			Rect waiting_rect = { (int)nwi->pos_x, (int)nwi->pos_y, (int)(nwi->pos_x + nwi->current_x - 1), (int)(nwi->pos_y + nwi->current_y - 1)};
			this->DrawEntries(&cargo, waiting_rect, pos, maxrows, 0);
			scroll_to_row = INT_MAX;
		}
	}

	void SetStringParameters(int widget) const override
	{
		const Station *st = Station::Get(this->window_number);
		SetDParam(0, st->index);
		SetDParam(1, st->facilities);
	}

	/**
	 * Rebuild the cache for estimated destinations which is used to quickly show the "destination" entries
	 * even if we actually don't know the destination of a certain packet from just looking at it.
	 * @param i Cargo to recalculate the cache for.
	 */
	void RecalcDestinations(CargoID i)
	{
		const Station *st = Station::Get(this->window_number);
		CargoDataEntry *cargo_entry = cached_destinations.InsertOrRetrieve(i);
		cargo_entry->Clear();

		const FlowStatMap &flows = st->goods[i].flows;
		for (FlowStatMap::const_iterator it = flows.begin(); it != flows.end(); ++it) {
			StationID from = it->first;
			CargoDataEntry *source_entry = cargo_entry->InsertOrRetrieve(from);
			const FlowStat::SharesMap *shares = it->second.GetShares();
			uint32 prev_count = 0;
			for (FlowStat::SharesMap::const_iterator flow_it = shares->begin(); flow_it != shares->end(); ++flow_it) {
				StationID via = flow_it->second;
				CargoDataEntry *via_entry = source_entry->InsertOrRetrieve(via);
				if (via == this->window_number) {
					via_entry->InsertOrRetrieve(via)->Update(flow_it->first - prev_count);
				} else {
					EstimateDestinations(i, from, via, flow_it->first - prev_count, via_entry);
				}
				prev_count = flow_it->first;
			}
		}
	}

	/**
	 * Estimate the amounts of cargo per final destination for a given cargo, source station and next hop and
	 * save the result as children of the given CargoDataEntry.
	 * @param cargo ID of the cargo to estimate destinations for.
	 * @param source Source station of the given batch of cargo.
	 * @param next Intermediate hop to start the calculation at ("next hop").
	 * @param count Size of the batch of cargo.
	 * @param dest CargoDataEntry to save the results in.
	 */
	void EstimateDestinations(CargoID cargo, StationID source, StationID next, uint count, CargoDataEntry *dest)
	{
		if (Station::IsValidID(next) && Station::IsValidID(source)) {
			CargoDataEntry tmp;
			const FlowStatMap &flowmap = Station::Get(next)->goods[cargo].flows;
			FlowStatMap::const_iterator map_it = flowmap.find(source);
			if (map_it != flowmap.end()) {
				const FlowStat::SharesMap *shares = map_it->second.GetShares();
				uint32 prev_count = 0;
				for (FlowStat::SharesMap::const_iterator i = shares->begin(); i != shares->end(); ++i) {
					tmp.InsertOrRetrieve(i->second)->Update(i->first - prev_count);
					prev_count = i->first;
				}
			}

			if (tmp.GetCount() == 0) {
				dest->InsertOrRetrieve(INVALID_STATION)->Update(count);
			} else {
				uint sum_estimated = 0;
				while (sum_estimated < count) {
					for (CargoDataSet::iterator i = tmp.Begin(); i != tmp.End() && sum_estimated < count; ++i) {
						CargoDataEntry *child = *i;
						uint estimate = DivideApprox(child->GetCount() * count, tmp.GetCount());
						if (estimate == 0) estimate = 1;

						sum_estimated += estimate;
						if (sum_estimated > count) {
							estimate -= sum_estimated - count;
							sum_estimated = count;
						}

						if (estimate > 0) {
							if (child->GetStation() == next) {
								dest->InsertOrRetrieve(next)->Update(estimate);
							} else {
								EstimateDestinations(cargo, source, child->GetStation(), estimate, dest);
							}
						}
					}

				}
			}
		} else {
			dest->InsertOrRetrieve(INVALID_STATION)->Update(count);
		}
	}

	/**
	 * Build up the cargo view for PLANNED mode and a specific cargo.
	 * @param i Cargo to show.
	 * @param flows The current station's flows for that cargo.
	 * @param cargo The CargoDataEntry to save the results in.
	 */
	void BuildFlowList(CargoID i, const FlowStatMap &flows, CargoDataEntry *cargo)
	{
		const CargoDataEntry *source_dest = this->cached_destinations.Retrieve(i);
		for (FlowStatMap::const_iterator it = flows.begin(); it != flows.end(); ++it) {
			StationID from = it->first;
			const CargoDataEntry *source_entry = source_dest->Retrieve(from);
			const FlowStat::SharesMap *shares = it->second.GetShares();
			for (FlowStat::SharesMap::const_iterator flow_it = shares->begin(); flow_it != shares->end(); ++flow_it) {
				const CargoDataEntry *via_entry = source_entry->Retrieve(flow_it->second);
				for (CargoDataSet::iterator dest_it = via_entry->Begin(); dest_it != via_entry->End(); ++dest_it) {
					CargoDataEntry *dest_entry = *dest_it;
					ShowCargo(cargo, i, from, flow_it->second, dest_entry->GetStation(), dest_entry->GetCount());
				}
			}
		}
	}

	/**
	 * Build up the cargo view for WAITING mode and a specific cargo.
	 * @param i Cargo to show.
	 * @param packets The current station's cargo list for that cargo.
	 * @param cargo The CargoDataEntry to save the result in.
	 */
	void BuildCargoList(CargoID i, const StationCargoList &packets, CargoDataEntry *cargo)
	{
		const CargoDataEntry *source_dest = this->cached_destinations.Retrieve(i);
		for (StationCargoList::ConstIterator it = packets.Packets()->begin(); it != packets.Packets()->end(); it++) {
			const CargoPacket *cp = *it;
			StationID next = it.GetKey();

			const CargoDataEntry *source_entry = source_dest->Retrieve(cp->SourceStation());
			if (source_entry == nullptr) {
				this->ShowCargo(cargo, i, cp->SourceStation(), next, INVALID_STATION, cp->Count());
				continue;
			}

			const CargoDataEntry *via_entry = source_entry->Retrieve(next);
			if (via_entry == nullptr) {
				this->ShowCargo(cargo, i, cp->SourceStation(), next, INVALID_STATION, cp->Count());
				continue;
			}

			for (CargoDataSet::iterator dest_it = via_entry->Begin(); dest_it != via_entry->End(); ++dest_it) {
				CargoDataEntry *dest_entry = *dest_it;
				uint val = DivideApprox(cp->Count() * dest_entry->GetCount(), via_entry->GetCount());
				this->ShowCargo(cargo, i, cp->SourceStation(), next, dest_entry->GetStation(), val);
			}
		}
		this->ShowCargo(cargo, i, NEW_STATION, NEW_STATION, NEW_STATION, packets.ReservedCount());
	}

	/**
	 * Build up the cargo view for all cargoes.
	 * @param cargo The root cargo entry to save all results in.
	 * @param st The station to calculate the cargo view from.
	 */
	void BuildCargoList(CargoDataEntry *cargo, const Station *st)
	{
		for (CargoID i = 0; i < NUM_CARGO; i++) {

			if (this->cached_destinations.Retrieve(i) == nullptr) {
				this->RecalcDestinations(i);
			}

			if (this->current_mode == MODE_WAITING) {
				this->BuildCargoList(i, st->goods[i].cargo, cargo);
			} else {
				this->BuildFlowList(i, st->goods[i].flows, cargo);
			}
		}
	}

	/**
	 * Mark a specific row, characterized by its CargoDataEntry, as expanded.
	 * @param data The row to be marked as expanded.
	 */
	void SetDisplayedRow(const CargoDataEntry *data)
	{
		std::list<StationID> stations;
		const CargoDataEntry *parent = data->GetParent();
		if (parent->GetParent() == nullptr) {
			this->displayed_rows.push_back(RowDisplay(&this->expanded_rows, data->GetCargo()));
			return;
		}

		StationID next = data->GetStation();
		while (parent->GetParent()->GetParent() != nullptr) {
			stations.push_back(parent->GetStation());
			parent = parent->GetParent();
		}

		CargoID cargo = parent->GetCargo();
		CargoDataEntry *filter = this->expanded_rows.Retrieve(cargo);
		while (!stations.empty()) {
			filter = filter->Retrieve(stations.back());
			stations.pop_back();
		}

		this->displayed_rows.push_back(RowDisplay(filter, next));
	}

	/**
	 * Select the correct string for an entry referring to the specified station.
	 * @param station Station the entry is showing cargo for.
	 * @param here String to be shown if the entry refers to the same station as this station GUI belongs to.
	 * @param other_station String to be shown if the entry refers to a specific other station.
	 * @param any String to be shown if the entry refers to "any station".
	 * @return One of the three given strings or STR_STATION_VIEW_RESERVED, depending on what station the entry refers to.
	 */
	StringID GetEntryString(StationID station, StringID here, StringID other_station, StringID any)
	{
		if (station == this->window_number) {
			return here;
		} else if (station == INVALID_STATION) {
			return any;
		} else if (station == NEW_STATION) {
			return STR_STATION_VIEW_RESERVED;
		} else {
			SetDParam(2, station);
			return other_station;
		}
	}

	/**
	 * Determine if we need to show the special "non-stop" string.
	 * @param cd Entry we are going to show.
	 * @param station Station the entry refers to.
	 * @param column The "column" the entry will be shown in.
	 * @return either STR_STATION_VIEW_VIA or STR_STATION_VIEW_NONSTOP.
	 */
	StringID SearchNonStop(CargoDataEntry *cd, StationID station, int column)
	{
		CargoDataEntry *parent = cd->GetParent();
		for (int i = column - 1; i > 0; --i) {
			if (this->groupings[i] == GR_DESTINATION) {
				if (parent->GetStation() == station) {
					return STR_STATION_VIEW_NONSTOP;
				} else {
					return STR_STATION_VIEW_VIA;
				}
			}
			parent = parent->GetParent();
		}

		if (this->groupings[column + 1] == GR_DESTINATION) {
			CargoDataSet::iterator begin = cd->Begin();
			CargoDataSet::iterator end = cd->End();
			if (begin != end && ++(cd->Begin()) == end && (*(begin))->GetStation() == station) {
				return STR_STATION_VIEW_NONSTOP;
			} else {
				return STR_STATION_VIEW_VIA;
			}
		}

		return STR_STATION_VIEW_VIA;
	}

	/**
	 * Draw the given cargo entries in the station GUI.
	 * @param entry Root entry for all cargo to be drawn.
	 * @param r Screen rectangle to draw into.
	 * @param pos Current row to be drawn to (counted down from 0 to -maxrows, same as vscroll->GetPosition()).
	 * @param maxrows Maximum row to be drawn.
	 * @param column Current "column" being drawn.
	 * @param cargo Current cargo being drawn (if cargo column has been passed).
	 * @return row (in "pos" counting) after the one we have last drawn to.
	 */
	int DrawEntries(CargoDataEntry *entry, Rect &r, int pos, int maxrows, int column, CargoID cargo = CT_INVALID)
	{
		if (this->sortings[column] == ST_AS_GROUPING) {
			if (this->groupings[column] != GR_CARGO) {
				entry->Resort(ST_STATION_STRING, this->sort_orders[column]);
			}
		} else {
			entry->Resort(ST_COUNT, this->sort_orders[column]);
		}
		for (CargoDataSet::iterator i = entry->Begin(); i != entry->End(); ++i) {
			CargoDataEntry *cd = *i;

			Grouping grouping = this->groupings[column];
			if (grouping == GR_CARGO) cargo = cd->GetCargo();
			bool auto_distributed = _settings_game.linkgraph.GetDistributionType(cargo) != DT_MANUAL;

			if (pos > -maxrows && pos <= 0) {
				StringID str = STR_EMPTY;
				int y = r.top + WD_FRAMERECT_TOP - pos * FONT_HEIGHT_NORMAL;
				SetDParam(0, cargo);
				SetDParam(1, cd->GetCount());

				if (this->groupings[column] == GR_CARGO) {
					str = STR_STATION_VIEW_WAITING_CARGO;
					DrawCargoIcons(cd->GetCargo(), cd->GetCount(), r.left + WD_FRAMERECT_LEFT + this->expand_shrink_width, r.right - WD_FRAMERECT_RIGHT - this->expand_shrink_width, y);
				} else {
					if (!auto_distributed) grouping = GR_SOURCE;
					StationID station = cd->GetStation();

					switch (grouping) {
						case GR_SOURCE:
							str = this->GetEntryString(station, STR_STATION_VIEW_FROM_HERE, STR_STATION_VIEW_FROM, STR_STATION_VIEW_FROM_ANY);
							break;
						case GR_NEXT:
							str = this->GetEntryString(station, STR_STATION_VIEW_VIA_HERE, STR_STATION_VIEW_VIA, STR_STATION_VIEW_VIA_ANY);
							if (str == STR_STATION_VIEW_VIA) str = this->SearchNonStop(cd, station, column);
							break;
						case GR_DESTINATION:
							str = this->GetEntryString(station, STR_STATION_VIEW_TO_HERE, STR_STATION_VIEW_TO, STR_STATION_VIEW_TO_ANY);
							break;
						default:
							NOT_REACHED();
					}
					if (pos == -this->scroll_to_row && Station::IsValidID(station)) {
						ScrollMainWindowToTile(Station::Get(station)->xy);
					}
				}

				bool rtl = _current_text_dir == TD_RTL;
				int text_left    = rtl ? r.left + this->expand_shrink_width : r.left + WD_FRAMERECT_LEFT + column * this->expand_shrink_width;
				int text_right   = rtl ? r.right - WD_FRAMERECT_LEFT - column * this->expand_shrink_width : r.right - this->expand_shrink_width;
				int shrink_left  = rtl ? r.left + WD_FRAMERECT_LEFT : r.right - this->expand_shrink_width + WD_FRAMERECT_LEFT;
				int shrink_right = rtl ? r.left + this->expand_shrink_width - WD_FRAMERECT_RIGHT : r.right - WD_FRAMERECT_RIGHT;

				DrawString(text_left, text_right, y, str);

				if (column < NUM_COLUMNS - 1) {
					const char *sym = nullptr;
					if (cd->GetNumChildren() > 0) {
						sym = "-";
					} else if (auto_distributed && str != STR_STATION_VIEW_RESERVED) {
						sym = "+";
					} else {
						/* Only draw '+' if there is something to be shown. */
						const StationCargoList &list = Station::Get(this->window_number)->goods[cargo].cargo;
						if (grouping == GR_CARGO && (list.ReservedCount() > 0 || cd->HasTransfers())) {
							sym = "+";
						}
					}
					if (sym) DrawString(shrink_left, shrink_right, y, sym, TC_YELLOW);
				}
				this->SetDisplayedRow(cd);
			}
			--pos;
			if (auto_distributed || column == 0) {
				pos = this->DrawEntries(cd, r, pos, maxrows, column + 1, cargo);
			}
		}
		return pos;
	}

	/**
	 * Draw accepted cargo in the #WID_SV_ACCEPT_RATING_LIST widget.
	 * @param r Rectangle of the widget.
	 * @return Number of lines needed for drawing the accepted cargo.
	 */
	int DrawAcceptedCargo(const Rect &r) const
	{
		const Station *st = Station::Get(this->window_number);

		CargoTypes cargo_mask = 0;
		for (CargoID i = 0; i < NUM_CARGO; i++) {
			if (HasBit(st->goods[i].status, GoodsEntry::GES_ACCEPTANCE)) SetBit(cargo_mask, i);
		}
		SetDParam(0, cargo_mask);
		int bottom = DrawStringMultiLine(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, INT32_MAX, STR_STATION_VIEW_ACCEPTS_CARGO);
		return CeilDiv(bottom - r.top - WD_FRAMERECT_TOP, FONT_HEIGHT_NORMAL);
	}

	/**
	 * Draw cargo ratings in the #WID_SV_ACCEPT_RATING_LIST widget.
	 * @param r Rectangle of the widget.
	 * @return Number of lines needed for drawing the cargo ratings.
	 */
	int DrawCargoRatings(const Rect &r) const
	{
		const Station *st = Station::Get(this->window_number);
		int y = r.top + WD_FRAMERECT_TOP;

		if (st->town->exclusive_counter > 0) {
			SetDParam(0, st->town->exclusivity);
			y = DrawStringMultiLine(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, r.bottom, st->town->exclusivity == st->owner ? STR_STATION_VIEW_EXCLUSIVE_RIGHTS_SELF : STR_STATION_VIEW_EXCLUSIVE_RIGHTS_COMPANY);
			y += WD_PAR_VSEP_WIDE;
		}

		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_STATION_VIEW_SUPPLY_RATINGS_TITLE);
		y += FONT_HEIGHT_NORMAL;

		const CargoSpec *cs;
		FOR_ALL_SORTED_STANDARD_CARGOSPECS(cs) {
			const GoodsEntry *ge = &st->goods[cs->Index()];
			if (!ge->HasRating()) continue;

			const LinkGraph *lg = LinkGraph::GetIfValid(ge->link_graph);
			SetDParam(0, cs->name);
			SetDParam(1, lg != nullptr ? lg->Monthly((*lg)[ge->node].Supply()) : 0);
			SetDParam(2, STR_CARGO_RATING_APPALLING + (ge->rating >> 5));
			SetDParam(3, ToPercent8(ge->rating));
			DrawString(r.left + WD_FRAMERECT_LEFT + 6, r.right - WD_FRAMERECT_RIGHT - 6, y, STR_STATION_VIEW_CARGO_SUPPLY_RATING);
			y += FONT_HEIGHT_NORMAL;
		}
		return CeilDiv(y - r.top - WD_FRAMERECT_TOP, FONT_HEIGHT_NORMAL);
	}

	/**
	 * Expand or collapse a specific row.
	 * @param filter Parent of the row.
	 * @param next ID pointing to the row.
	 */
	template<class Tid>
	void HandleCargoWaitingClick(CargoDataEntry *filter, Tid next)
	{
		if (filter->Retrieve(next) != nullptr) {
			filter->Remove(next);
		} else {
			filter->InsertOrRetrieve(next);
		}
	}

	/**
	 * Handle a click on a specific row in the cargo view.
	 * @param row Row being clicked.
	 */
	void HandleCargoWaitingClick(int row)
	{
		if (row < 0 || (uint)row >= this->displayed_rows.size()) return;
		if (_ctrl_pressed) {
			this->scroll_to_row = row;
		} else {
			RowDisplay &display = this->displayed_rows[row];
			if (display.filter == &this->expanded_rows) {
				this->HandleCargoWaitingClick<CargoID>(display.filter, display.next_cargo);
			} else {
				this->HandleCargoWaitingClick<StationID>(display.filter, display.next_station);
			}
		}
		this->SetWidgetDirty(WID_SV_WAITING);
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		switch (widget) {
			case WID_SV_WAITING:
				this->HandleCargoWaitingClick(this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_SV_WAITING, WD_FRAMERECT_TOP, FONT_HEIGHT_NORMAL) - this->vscroll->GetPosition());
				break;

			case WID_SV_CATCHMENT:
				SetViewportCatchmentStation(Station::Get(this->window_number), !this->IsWidgetLowered(WID_SV_CATCHMENT));
				break;

			case WID_SV_LOCATION:
				if (_ctrl_pressed) {
					ShowExtraViewportWindow(Station::Get(this->window_number)->xy);
				} else {
					ScrollMainWindowToTile(Station::Get(this->window_number)->xy);
				}
				break;

			case WID_SV_ACCEPTS_RATINGS: {
				/* Swap between 'accepts' and 'ratings' view. */
				int height_change;
				NWidgetCore *nwi = this->GetWidget<NWidgetCore>(WID_SV_ACCEPTS_RATINGS);
				if (this->GetWidget<NWidgetCore>(WID_SV_ACCEPTS_RATINGS)->widget_data == STR_STATION_VIEW_RATINGS_BUTTON) {
					nwi->SetDataTip(STR_STATION_VIEW_ACCEPTS_BUTTON, STR_STATION_VIEW_ACCEPTS_TOOLTIP); // Switch to accepts view.
					height_change = this->rating_lines - this->accepts_lines;
				} else {
					nwi->SetDataTip(STR_STATION_VIEW_RATINGS_BUTTON, STR_STATION_VIEW_RATINGS_TOOLTIP); // Switch to ratings view.
					height_change = this->accepts_lines - this->rating_lines;
				}
				this->ReInit(0, height_change * FONT_HEIGHT_NORMAL);
				break;
			}

			case WID_SV_RENAME:
				SetDParam(0, this->window_number);
				ShowQueryString(STR_STATION_NAME, STR_STATION_VIEW_RENAME_STATION_CAPTION, MAX_LENGTH_STATION_NAME_CHARS,
						this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT | QSF_LEN_IN_CHARS);
				break;

			case WID_SV_CLOSE_AIRPORT:
				DoCommandP(0, this->window_number, 0, CMD_OPEN_CLOSE_AIRPORT);
				break;

			case WID_SV_TRAINS:   // Show list of scheduled trains to this station
			case WID_SV_ROADVEHS: // Show list of scheduled road-vehicles to this station
			case WID_SV_SHIPS:    // Show list of scheduled ships to this station
			case WID_SV_PLANES: { // Show list of scheduled aircraft to this station
				Owner owner = Station::Get(this->window_number)->owner;
				ShowVehicleListWindow(owner, (VehicleType)(widget - WID_SV_TRAINS), (StationID)this->window_number);
				break;
			}

			case WID_SV_SORT_BY: {
				/* The initial selection is composed of current mode and
				 * sorting criteria for columns 1, 2, and 3. Column 0 is always
				 * sorted by cargo ID. The others can theoretically be sorted
				 * by different things but there is no UI for that. */
				ShowDropDownMenu(this, _sort_names,
						this->current_mode * 2 + (this->sortings[1] == ST_COUNT ? 1 : 0),
						WID_SV_SORT_BY, 0, 0);
				break;
			}

			case WID_SV_GROUP_BY: {
				ShowDropDownMenu(this, _group_names, this->grouping_index, WID_SV_GROUP_BY, 0, 0);
				break;
			}

			case WID_SV_SORT_ORDER: { // flip sorting method asc/desc
				this->SelectSortOrder(this->sort_orders[1] == SO_ASCENDING ? SO_DESCENDING : SO_ASCENDING);
				this->SetTimeout();
				this->LowerWidget(WID_SV_SORT_ORDER);
				break;
			}
		}
	}

	/**
	 * Select a new sort order for the cargo view.
	 * @param order New sort order.
	 */
	void SelectSortOrder(SortOrder order)
	{
		this->sort_orders[1] = this->sort_orders[2] = this->sort_orders[3] = order;
		_settings_client.gui.station_gui_sort_order = this->sort_orders[1];
		this->SetDirty();
	}

	/**
	 * Select a new sort criterium for the cargo view.
	 * @param index Row being selected in the sort criteria drop down.
	 */
	void SelectSortBy(int index)
	{
		_settings_client.gui.station_gui_sort_by = index;
		switch (_sort_names[index]) {
			case STR_STATION_VIEW_WAITING_STATION:
				this->current_mode = MODE_WAITING;
				this->sortings[1] = this->sortings[2] = this->sortings[3] = ST_AS_GROUPING;
				break;
			case STR_STATION_VIEW_WAITING_AMOUNT:
				this->current_mode = MODE_WAITING;
				this->sortings[1] = this->sortings[2] = this->sortings[3] = ST_COUNT;
				break;
			case STR_STATION_VIEW_PLANNED_STATION:
				this->current_mode = MODE_PLANNED;
				this->sortings[1] = this->sortings[2] = this->sortings[3] = ST_AS_GROUPING;
				break;
			case STR_STATION_VIEW_PLANNED_AMOUNT:
				this->current_mode = MODE_PLANNED;
				this->sortings[1] = this->sortings[2] = this->sortings[3] = ST_COUNT;
				break;
			default:
				NOT_REACHED();
		}
		/* Display the current sort variant */
		this->GetWidget<NWidgetCore>(WID_SV_SORT_BY)->widget_data = _sort_names[index];
		this->SetDirty();
	}

	/**
	 * Select a new grouping mode for the cargo view.
	 * @param index Row being selected in the grouping drop down.
	 */
	void SelectGroupBy(int index)
	{
		this->grouping_index = index;
		_settings_client.gui.station_gui_group_order = index;
		this->GetWidget<NWidgetCore>(WID_SV_GROUP_BY)->widget_data = _group_names[index];
		switch (_group_names[index]) {
			case STR_STATION_VIEW_GROUP_S_V_D:
				this->groupings[1] = GR_SOURCE;
				this->groupings[2] = GR_NEXT;
				this->groupings[3] = GR_DESTINATION;
				break;
			case STR_STATION_VIEW_GROUP_S_D_V:
				this->groupings[1] = GR_SOURCE;
				this->groupings[2] = GR_DESTINATION;
				this->groupings[3] = GR_NEXT;
				break;
			case STR_STATION_VIEW_GROUP_V_S_D:
				this->groupings[1] = GR_NEXT;
				this->groupings[2] = GR_SOURCE;
				this->groupings[3] = GR_DESTINATION;
				break;
			case STR_STATION_VIEW_GROUP_V_D_S:
				this->groupings[1] = GR_NEXT;
				this->groupings[2] = GR_DESTINATION;
				this->groupings[3] = GR_SOURCE;
				break;
			case STR_STATION_VIEW_GROUP_D_S_V:
				this->groupings[1] = GR_DESTINATION;
				this->groupings[2] = GR_SOURCE;
				this->groupings[3] = GR_NEXT;
				break;
			case STR_STATION_VIEW_GROUP_D_V_S:
				this->groupings[1] = GR_DESTINATION;
				this->groupings[2] = GR_NEXT;
				this->groupings[3] = GR_SOURCE;
				break;
		}
		this->SetDirty();
	}

	void OnDropdownSelect(int widget, int index) override
	{
		if (widget == WID_SV_SORT_BY) {
			this->SelectSortBy(index);
		} else {
			this->SelectGroupBy(index);
		}
	}

	void OnQueryTextFinished(char *str) override
	{
		if (str == nullptr) return;

		DoCommandP(0, this->window_number, 0, CMD_RENAME_STATION | CMD_MSG(STR_ERROR_CAN_T_RENAME_STATION), nullptr, str);
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_SV_WAITING, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM);
	}

	/**
	 * Some data on this window has become invalid. Invalidate the cache for the given cargo if necessary.
	 * @param data Information about the changed data. If it's a valid cargo ID, invalidate the cargo data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		if (gui_scope) {
			if (data >= 0 && data < NUM_CARGO) {
				this->cached_destinations.Remove((CargoID)data);
			} else {
				this->ReInit();
			}
		}
	}
};

const StringID StationViewWindow::_sort_names[] = {
	STR_STATION_VIEW_WAITING_STATION,
	STR_STATION_VIEW_WAITING_AMOUNT,
	STR_STATION_VIEW_PLANNED_STATION,
	STR_STATION_VIEW_PLANNED_AMOUNT,
	INVALID_STRING_ID
};

const StringID StationViewWindow::_group_names[] = {
	STR_STATION_VIEW_GROUP_S_V_D,
	STR_STATION_VIEW_GROUP_S_D_V,
	STR_STATION_VIEW_GROUP_V_S_D,
	STR_STATION_VIEW_GROUP_V_D_S,
	STR_STATION_VIEW_GROUP_D_S_V,
	STR_STATION_VIEW_GROUP_D_V_S,
	INVALID_STRING_ID
};

static WindowDesc _station_view_desc(
	WDP_AUTO, "view_station", 249, 117,
	WC_STATION_VIEW, WC_NONE,
	0,
	_nested_station_view_widgets, lengthof(_nested_station_view_widgets)
);

/**
 * Opens StationViewWindow for given station
 *
 * @param station station which window should be opened
 */
void ShowStationViewWindow(StationID station)
{
	AllocateWindowDescFront<StationViewWindow>(&_station_view_desc, station);
}

/** Struct containing TileIndex and StationID */
struct TileAndStation {
	TileIndex tile;    ///< TileIndex
	StationID station; ///< StationID
};

static std::vector<TileAndStation> _deleted_stations_nearby;
static std::vector<StationID> _stations_nearby_list;

/**
 * Add station on this tile to _stations_nearby_list if it's fully within the
 * station spread.
 * @param tile Tile just being checked
 * @param user_data Pointer to TileArea context
 * @tparam T the type of station to look for
 */
template <class T>
static bool AddNearbyStation(TileIndex tile, void *user_data)
{
	TileArea *ctx = (TileArea *)user_data;

	/* First check if there were deleted stations here */
	for (uint i = 0; i < _deleted_stations_nearby.size(); i++) {
		auto ts = _deleted_stations_nearby.begin() + i;
		if (ts->tile == tile) {
			_stations_nearby_list.push_back(_deleted_stations_nearby[i].station);
			_deleted_stations_nearby.erase(ts);
			i--;
		}
	}

	/* Check if own station and if we stay within station spread */
	if (!IsTileType(tile, MP_STATION)) return false;

	StationID sid = GetStationIndex(tile);

	/* This station is (likely) a waypoint */
	if (!T::IsValidID(sid)) return false;

	T *st = T::Get(sid);
	if (st->owner != _local_company || std::find(_stations_nearby_list.begin(), _stations_nearby_list.end(), sid) != _stations_nearby_list.end()) return false;

	if (st->rect.BeforeAddRect(ctx->tile, ctx->w, ctx->h, StationRect::ADD_TEST).Succeeded()) {
		_stations_nearby_list.push_back(sid);
	}

	return false; // We want to include *all* nearby stations
}

/**
 * Circulate around the to-be-built station to find stations we could join.
 * Make sure that only stations are returned where joining wouldn't exceed
 * station spread and are our own station.
 * @param ta Base tile area of the to-be-built station
 * @param distant_join Search for adjacent stations (false) or stations fully
 *                     within station spread
 * @tparam T the type of station to look for
 */
template <class T>
static const T *FindStationsNearby(TileArea ta, bool distant_join)
{
	TileArea ctx = ta;

	_stations_nearby_list.clear();
	_deleted_stations_nearby.clear();

	/* Check the inside, to return, if we sit on another station */
	TILE_AREA_LOOP(t, ta) {
		if (t < MapSize() && IsTileType(t, MP_STATION) && T::IsValidID(GetStationIndex(t))) return T::GetByTile(t);
	}

	/* Look for deleted stations */
	for (const BaseStation *st : BaseStation::Iterate()) {
		if (T::IsExpected(st) && !st->IsInUse() && st->owner == _local_company) {
			/* Include only within station spread (yes, it is strictly less than) */
			if (max(DistanceMax(ta.tile, st->xy), DistanceMax(TILE_ADDXY(ta.tile, ta.w - 1, ta.h - 1), st->xy)) < _settings_game.station.station_spread) {
				_deleted_stations_nearby.push_back({st->xy, st->index});

				/* Add the station when it's within where we're going to build */
				if (IsInsideBS(TileX(st->xy), TileX(ctx.tile), ctx.w) &&
						IsInsideBS(TileY(st->xy), TileY(ctx.tile), ctx.h)) {
					AddNearbyStation<T>(st->xy, &ctx);
				}
			}
		}
	}

	/* Only search tiles where we have a chance to stay within the station spread.
	 * The complete check needs to be done in the callback as we don't know the
	 * extent of the found station, yet. */
	if (distant_join && min(ta.w, ta.h) >= _settings_game.station.station_spread) return nullptr;
	uint max_dist = distant_join ? _settings_game.station.station_spread - min(ta.w, ta.h) : 1;

	TileIndex tile = TileAddByDir(ctx.tile, DIR_N);
	CircularTileSearch(&tile, max_dist, ta.w, ta.h, AddNearbyStation<T>, &ctx);

	return nullptr;
}

static const NWidgetPart _nested_select_station_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_JS_CAPTION), SetDataTip(STR_JOIN_STATION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_JS_PANEL), SetResize(1, 0), SetScrollbar(WID_JS_SCROLLBAR), EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WID_JS_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
		EndContainer(),
	EndContainer(),
};

/**
 * Window for selecting stations/waypoints to (distant) join to.
 * @tparam T The type of station to join with
 */
template <class T>
struct SelectStationWindow : Window {
	CommandContainer select_station_cmd; ///< Command to build new station
	TileArea area; ///< Location of new station
	Scrollbar *vscroll;

	SelectStationWindow(WindowDesc *desc, const CommandContainer &cmd, TileArea ta) :
		Window(desc),
		select_station_cmd(cmd),
		area(ta)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_JS_SCROLLBAR);
		this->GetWidget<NWidgetCore>(WID_JS_CAPTION)->widget_data = T::EXPECTED_FACIL == FACIL_WAYPOINT ? STR_JOIN_WAYPOINT_CAPTION : STR_JOIN_STATION_CAPTION;
		this->FinishInitNested(0);
		this->OnInvalidateData(0);

		_thd.freeze = true;
	}

	~SelectStationWindow()
	{
		if (_settings_client.gui.station_show_coverage) SetViewportCatchmentStation(nullptr, true);

		_thd.freeze = false;
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		if (widget != WID_JS_PANEL) return;

		/* Determine the widest string */
		Dimension d = GetStringBoundingBox(T::EXPECTED_FACIL == FACIL_WAYPOINT ? STR_JOIN_WAYPOINT_CREATE_SPLITTED_WAYPOINT : STR_JOIN_STATION_CREATE_SPLITTED_STATION);
		for (uint i = 0; i < _stations_nearby_list.size(); i++) {
			const T *st = T::Get(_stations_nearby_list[i]);
			SetDParam(0, st->index);
			SetDParam(1, st->facilities);
			d = maxdim(d, GetStringBoundingBox(T::EXPECTED_FACIL == FACIL_WAYPOINT ? STR_STATION_LIST_WAYPOINT : STR_STATION_LIST_STATION));
		}

		resize->height = d.height;
		d.height *= 5;
		d.width += WD_FRAMERECT_RIGHT + WD_FRAMERECT_LEFT;
		d.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
		*size = d;
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		if (widget != WID_JS_PANEL) return;

		uint y = r.top + WD_FRAMERECT_TOP;
		if (this->vscroll->GetPosition() == 0) {
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, T::EXPECTED_FACIL == FACIL_WAYPOINT ? STR_JOIN_WAYPOINT_CREATE_SPLITTED_WAYPOINT : STR_JOIN_STATION_CREATE_SPLITTED_STATION);
			y += this->resize.step_height;
		}

		for (uint i = max<uint>(1, this->vscroll->GetPosition()); i <= _stations_nearby_list.size(); ++i, y += this->resize.step_height) {
			/* Don't draw anything if it extends past the end of the window. */
			if (i - this->vscroll->GetPosition() >= this->vscroll->GetCapacity()) break;

			const T *st = T::Get(_stations_nearby_list[i - 1]);
			SetDParam(0, st->index);
			SetDParam(1, st->facilities);
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, T::EXPECTED_FACIL == FACIL_WAYPOINT ? STR_STATION_LIST_WAYPOINT : STR_STATION_LIST_STATION);
		}
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		if (widget != WID_JS_PANEL) return;

		uint st_index = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_JS_PANEL, WD_FRAMERECT_TOP);
		bool distant_join = (st_index > 0);
		if (distant_join) st_index--;

		if (distant_join && st_index >= _stations_nearby_list.size()) return;

		/* Insert station to be joined into stored command */
		SB(this->select_station_cmd.p2, 16, 16,
		   (distant_join ? _stations_nearby_list[st_index] : NEW_STATION));

		/* Execute stored Command */
		DoCommandP(&this->select_station_cmd);

		/* Close Window; this might cause double frees! */
		DeleteWindowById(WC_SELECT_STATION, 0);
	}

	void OnRealtimeTick(uint delta_ms) override
	{
		if (_thd.dirty & 2) {
			_thd.dirty &= ~2;
			this->SetDirty();
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_JS_PANEL, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		if (!gui_scope) return;
		FindStationsNearby<T>(this->area, true);
		this->vscroll->SetCount((uint)_stations_nearby_list.size() + 1);
		this->SetDirty();
	}

	void OnMouseOver(Point pt, int widget) override
	{
		if (widget != WID_JS_PANEL || T::EXPECTED_FACIL == FACIL_WAYPOINT) {
			SetViewportCatchmentStation(nullptr, true);
			return;
		}

		/* Show coverage area of station under cursor */
		uint st_index = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_JS_PANEL, WD_FRAMERECT_TOP);
		if (st_index == 0 || st_index > _stations_nearby_list.size()) {
			SetViewportCatchmentStation(nullptr, true);
		} else {
			st_index--;
			SetViewportCatchmentStation(Station::Get(_stations_nearby_list[st_index]), true);
		}
	}
};

static WindowDesc _select_station_desc(
	WDP_AUTO, "build_station_join", 200, 180,
	WC_SELECT_STATION, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_select_station_widgets, lengthof(_nested_select_station_widgets)
);


/**
 * Check whether we need to show the station selection window.
 * @param cmd Command to build the station.
 * @param ta Tile area of the to-be-built station
 * @tparam T the type of station
 * @return whether we need to show the station selection window.
 */
template <class T>
static bool StationJoinerNeeded(const CommandContainer &cmd, TileArea ta)
{
	/* Only show selection if distant join is enabled in the settings */
	if (!_settings_game.station.distant_join_stations) return false;

	/* If a window is already opened and we didn't ctrl-click,
	 * return true (i.e. just flash the old window) */
	Window *selection_window = FindWindowById(WC_SELECT_STATION, 0);
	if (selection_window != nullptr) {
		/* Abort current distant-join and start new one */
		delete selection_window;
		UpdateTileSelection();
	}

	/* only show the popup, if we press ctrl */
	if (!_ctrl_pressed) return false;

	/* Now check if we could build there */
	if (DoCommand(&cmd, CommandFlagsToDCFlags(GetCommandFlags(cmd.cmd))).Failed()) return false;

	/* Test for adjacent station or station below selection.
	 * If adjacent-stations is disabled and we are building next to a station, do not show the selection window.
	 * but join the other station immediately. */
	const T *st = FindStationsNearby<T>(ta, false);
	return st == nullptr && (_settings_game.station.adjacent_stations || _stations_nearby_list.size() == 0);
}

/**
 * Show the station selection window when needed. If not, build the station.
 * @param cmd Command to build the station.
 * @param ta Area to build the station in
 * @tparam the class to find stations for
 */
template <class T>
void ShowSelectBaseStationIfNeeded(const CommandContainer &cmd, TileArea ta)
{
	if (StationJoinerNeeded<T>(cmd, ta)) {
		if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
		new SelectStationWindow<T>(&_select_station_desc, cmd, ta);
	} else {
		DoCommandP(&cmd);
	}
}

/**
 * Show the station selection window when needed. If not, build the station.
 * @param cmd Command to build the station.
 * @param ta Area to build the station in
 */
void ShowSelectStationIfNeeded(const CommandContainer &cmd, TileArea ta)
{
	ShowSelectBaseStationIfNeeded<Station>(cmd, ta);
}

/**
 * Show the waypoint selection window when needed. If not, build the waypoint.
 * @param cmd Command to build the waypoint.
 * @param ta Area to build the waypoint in
 */
void ShowSelectWaypointIfNeeded(const CommandContainer &cmd, TileArea ta)
{
	ShowSelectBaseStationIfNeeded<Waypoint>(cmd, ta);
}
