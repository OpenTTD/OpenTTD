/* $Id$ */

/** @file station_gui.cpp The GUI for stations. */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "company_func.h"
#include "command_func.h"
#include "vehicle_gui.h"
#include "cargotype.h"
#include "station_gui.h"
#include "strings_func.h"
#include "window_func.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "widgets/dropdown_func.h"
#include "newgrf_cargo.h"
#include "station_map.h"
#include "tilehighlight_func.h"
#include "core/smallmap_type.hpp"
#include "company_base.h"
#include "sortlist_type.h"
#include "settings_type.h"

#include "table/strings.h"
#include "table/sprites.h"

/**
 * Draw small boxes of cargo amount and ratings data at the given
 * coordinates. If amount exceeds 576 units, it is shown 'full', same
 * goes for the rating: at above 90% orso (224) it is also 'full'
 *
 * @param x coordinate to draw the box at
 * @param y coordinate to draw the box at
 * @param type Cargo type
 * @param amount Cargo amount
 * @param rating ratings data for that particular cargo
 *
 * @note Each cargo-bar is 16 pixels wide and 6 pixels high
 * @note Each rating 14 pixels wide and 1 pixel high and is 1 pixel below the cargo-bar
 */
static void StationsWndShowStationRating(int x, int y, CargoID type, uint amount, byte rating)
{
	static const uint units_full  = 576; ///< number of units to show station as 'full'
	static const uint rating_full = 224; ///< rating needed so it is shown as 'full'

	const CargoSpec *cs = GetCargo(type);
	if (!cs->IsValid()) return;

	int colour = cs->rating_colour;
	uint w = (minu(amount, units_full) + 5) / 36;

	/* Draw total cargo (limited) on station (fits into 16 pixels) */
	if (w != 0) GfxFillRect(x, y, x + w - 1, y + 6, colour);

	/* Draw a one pixel-wide bar of additional cargo meter, useful
	 * for stations with only a small amount (<=30) */
	if (w == 0) {
		uint rest = amount / 5;
		if (rest != 0) {
			w += x;
			GfxFillRect(w, y + 6 - rest, w, y + 6, colour);
		}
	}

	DrawString(x + 1, y, cs->abbrev, TC_BLACK);

	/* Draw green/red ratings bar (fits into 14 pixels) */
	y += 8;
	GfxFillRect(x + 1, y, x + 14, y, 0xB8);
	rating = minu(rating, rating_full) / 16;
	if (rating != 0) GfxFillRect(x + 1, y, x + rating, y, 0xD0);
}

typedef GUIList<const Station*> GUIStationList;

/**
 * The list of stations per company.
 */
class CompanyStationsWindow : public Window
{
	/** Enum for CompanyStations, referring to _company_stations_widgets */
	enum StationListWidgets {
		SLW_CLOSEBOX =  0,  ///< Close window button
		SLW_CAPTION,        ///< Window caption
		SLW_STICKY,         ///< Sticky button
		SLW_LIST,           ///< The main panel, list of stations
		SLW_SCROLLBAR,      ///< Scrollbar next to the main panel
		SLW_RESIZE,         ///< Resize button

		SLW_TRAIN,          ///< 'TRAIN' button - list only facilities where is a railroad station
		SLW_TRUCK,          ///< 'TRUCK' button - list only facilities where is a truck stop
		SLW_BUS,            ///< 'BUS' button - list only facilities where is a bus stop
		SLW_AIRPLANE,       ///< 'AIRPLANE' button - list only facilities where is an airport
		SLW_SHIP,           ///< 'SHIP' button - list only facilities where is a dock
		SLW_FACILALL,       ///< 'ALL' button - list all facilities

		SLW_PAN_BETWEEN,    ///< Small panel between list of types of ficilities and list of cargo types
		SLW_NOCARGOWAITING, ///< 'NO' button - list stations where no cargo is waiting
		SLW_CARGOALL,       ///< 'ALL' button - list all stations
		SLW_PAN_RIGHT,      ///< Panel right of list of cargo types

		SLW_SORTBY,         ///< 'Sort by' button - reverse sort direction
		SLW_SORTDROPBTN,    ///< Dropdown button
		SLW_PAN_SORT_RIGHT, ///< Panel right of sorting options

		SLW_CARGOSTART,     ///< Widget numbers used for list of cargo types (not present in _company_stations_widgets)
	};

protected:
	/* Runtime saved values */
	static Listing last_sorting;
	static byte facilities;               // types of stations of interest
	static bool include_empty;            // whether we should include stations without waiting cargo
	static const uint32 cargo_filter_max;
	static uint32 cargo_filter;           // bitmap of cargo types to include
	static const Station *last_station;

	/* Constants for sorting stations */
	static const StringID sorter_names[];
	static GUIStationList::SortFunction * const sorter_funcs[];

	GUIStationList stations;


	/**
	 * (Re)Build station list
	 *
	 * @param owner company whose stations are to be in list
	 */
	void BuildStationsList(const Owner owner)
	{
		if (!this->stations.NeedRebuild()) return;

		DEBUG(misc, 3, "Building station list for company %d", owner);

		this->stations.Clear();

		const Station *st;
		FOR_ALL_STATIONS(st) {
			if (st->owner == owner || (st->owner == OWNER_NONE && !st->IsBuoy() && HasStationInUse(st->index, owner))) {
				if (this->facilities & st->facilities) { // only stations with selected facilities
					int num_waiting_cargo = 0;
					for (CargoID j = 0; j < NUM_CARGO; j++) {
						if (!st->goods[j].cargo.Empty()) {
							num_waiting_cargo++; // count number of waiting cargo
							if (HasBit(this->cargo_filter, j)) {
								*this->stations.Append() = st;
								break;
							}
						}
					}
					/* stations without waiting cargo */
					if (num_waiting_cargo == 0 && this->include_empty) {
						*this->stations.Append() = st;
					}
				}
			}
		}

		this->stations.Compact();
		this->stations.RebuildDone();
	}

	/** Sort stations by their name */
	static int CDECL StationNameSorter(const Station * const *a, const Station * const *b)
	{
		static char buf_cache[64];
		char buf[64];

		SetDParam(0, (*a)->index);
		GetString(buf, STR_STATION, lastof(buf));

		if (*b != last_station) {
			last_station = *b;
			SetDParam(0, (*b)->index);
			GetString(buf_cache, STR_STATION, lastof(buf_cache));
		}

		return strcmp(buf, buf_cache);
	}

	/** Sort stations by their type */
	static int CDECL StationTypeSorter(const Station * const *a, const Station * const *b)
	{
		return (*a)->facilities - (*b)->facilities;
	}

	/** Sort stations by their waiting cargo */
	static int CDECL StationWaitingSorter(const Station * const *a, const Station * const *b)
	{
		Money diff = 0;

		for (CargoID j = 0; j < NUM_CARGO; j++) {
			if (!HasBit(cargo_filter, j)) continue;
			if (!(*a)->goods[j].cargo.Empty()) diff += GetTransportedGoodsIncome((*a)->goods[j].cargo.Count(), 20, 50, j);
			if (!(*b)->goods[j].cargo.Empty()) diff -= GetTransportedGoodsIncome((*b)->goods[j].cargo.Count(), 20, 50, j);
		}

		return ClampToI32(diff);
	}

	/** Sort stations by their rating */
	static int CDECL StationRatingMaxSorter(const Station * const *a, const Station * const *b)
	{
		byte maxr1 = 0;
		byte maxr2 = 0;

		for (CargoID j = 0; j < NUM_CARGO; j++) {
			if (HasBit((*a)->goods[j].acceptance_pickup, GoodsEntry::PICKUP)) maxr1 = max(maxr1, (*a)->goods[j].rating);
			if (HasBit((*b)->goods[j].acceptance_pickup, GoodsEntry::PICKUP)) maxr2 = max(maxr2, (*b)->goods[j].rating);
		}

		return maxr1 - maxr2;
	}

	/** Sort the stations list */
	void SortStationsList()
	{
		if (!this->stations.Sort()) return;

		/* Reset name sorter sort cache */
		this->last_station = NULL;

		/* Set the modified widget dirty */
		this->InvalidateWidget(SLW_LIST);
	}

public:
	CompanyStationsWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->owner = (Owner)this->window_number;
		this->vscroll.cap = 12;
		this->resize.step_height = 10;
		this->resize.height = this->height - 10 * 7; // minimum if 5 in the list

		/* Add cargo filter buttons */
		uint num_active = 0;
		for (CargoID c = 0; c < NUM_CARGO; c++) {
			if (GetCargo(c)->IsValid()) num_active++;
		}

		this->widget_count += num_active;
		this->widget = ReallocT(this->widget, this->widget_count + 1);
		this->widget[this->widget_count].type = WWT_LAST;

		uint i = 0;
		for (CargoID c = 0; c < NUM_CARGO; c++) {
			if (!GetCargo(c)->IsValid()) continue;

			Widget *wi = &this->widget[SLW_CARGOSTART + i];
			wi->type     = WWT_PANEL;
			wi->display_flags = RESIZE_NONE;
			wi->colour   = COLOUR_GREY;
			wi->left     = 89 + i * 14;
			wi->right    = wi->left + 13;
			wi->top      = 14;
			wi->bottom   = 24;
			wi->data     = 0;
			wi->tooltips = STR_USE_CTRL_TO_SELECT_MORE;

			if (HasBit(this->cargo_filter, c)) this->LowerWidget(SLW_CARGOSTART + i);
			i++;
		}

		this->widget[SLW_NOCARGOWAITING].left += num_active * 14;
		this->widget[SLW_NOCARGOWAITING].right += num_active * 14;
		this->widget[SLW_CARGOALL].left += num_active * 14;
		this->widget[SLW_CARGOALL].right += num_active * 14;
		this->widget[SLW_PAN_RIGHT].left += num_active * 14;

		if (num_active > 15) {
			/* Resize and fix the minimum width, if necessary */
			ResizeWindow(this, (num_active - 15) * 14, 0);
			this->resize.width = this->width;
		}

		if (this->cargo_filter == this->cargo_filter_max) this->cargo_filter = _cargo_mask;

		for (uint i = 0; i < 5; i++) {
			if (HasBit(this->facilities, i)) this->LowerWidget(i + SLW_TRAIN);
		}
		this->SetWidgetLoweredState(SLW_FACILALL, this->facilities == (FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK));
		this->SetWidgetLoweredState(SLW_CARGOALL, this->cargo_filter == _cargo_mask && this->include_empty);
		this->SetWidgetLoweredState(SLW_NOCARGOWAITING, this->include_empty);

		this->stations.SetListing(this->last_sorting);
		this->stations.SetSortFuncs(this->sorter_funcs);
		this->stations.ForceRebuild();
		this->stations.NeedResort();
		this->SortStationsList();

		this->widget[SLW_SORTDROPBTN].data = this->sorter_names[this->stations.SortType()];

		this->FindWindowPlacementAndResize(desc);
	}

	~CompanyStationsWindow()
	{
		this->last_sorting = this->stations.GetListing();
	}

	virtual void OnPaint()
	{
		const Owner owner = (Owner)this->window_number;

		this->BuildStationsList(owner);
		this->SortStationsList();

		SetVScrollCount(this, this->stations.Length());

		/* draw widgets, with company's name in the caption */
		SetDParam(0, owner);
		SetDParam(1, this->vscroll.count);

		this->DrawWidgets();

		/* draw arrow pointing up/down for ascending/descending sorting */
		this->DrawSortButtonState(SLW_SORTBY, this->stations.IsDescSortOrder() ? SBS_DOWN : SBS_UP);

		int cg_ofst;
		int x = 89;
		int y = 14;
		int xb = 2; ///< offset from left of widget

		uint i = 0;
		for (CargoID c = 0; c < NUM_CARGO; c++) {
			const CargoSpec *cs = GetCargo(c);
			if (!cs->IsValid()) continue;

			cg_ofst = HasBit(this->cargo_filter, c) ? 2 : 1;
			GfxFillRect(x + cg_ofst, y + cg_ofst, x + cg_ofst + 10 , y + cg_ofst + 7, cs->rating_colour);
			DrawStringCentered(x + 6 + cg_ofst, y + cg_ofst, cs->abbrev, TC_BLACK);
			x += 14;
			i++;
		}

		x += 6;
		cg_ofst = this->IsWidgetLowered(SLW_NOCARGOWAITING) ? 2 : 1;
		DrawStringCentered(x + cg_ofst, y + cg_ofst, STR_ABBREV_NONE, TC_BLACK);
		x += 14;
		cg_ofst = this->IsWidgetLowered(SLW_CARGOALL) ? 2 : 1;
		DrawStringCentered(x + cg_ofst, y + cg_ofst, STR_ABBREV_ALL, TC_BLACK);

		cg_ofst = this->IsWidgetLowered(SLW_FACILALL) ? 2 : 1;
		DrawString(71 + cg_ofst, y + cg_ofst, STR_ABBREV_ALL, TC_BLACK);

		if (this->vscroll.count == 0) { // company has no stations
			DrawString(xb, 40, STR_304A_NONE, TC_FROMSTRING);
			return;
		}

		int max = min(this->vscroll.pos + this->vscroll.cap, this->stations.Length());
		y = 40; // start of the list-widget

		for (int i = this->vscroll.pos; i < max; ++i) { // do until max number of stations of owner
			const Station *st = this->stations[i];
			int x;

			assert(st->xy != INVALID_TILE);

			/* Do not do the complex check HasStationInUse here, it may be even false
			 * when the order had been removed and the station list hasn't been removed yet */
			assert(st->owner == owner || (st->owner == OWNER_NONE && !st->IsBuoy()));

			SetDParam(0, st->index);
			SetDParam(1, st->facilities);
			x = DrawString(xb, y, STR_3049_0, TC_FROMSTRING) + 5;

			/* show cargo waiting and station ratings */
			for (CargoID j = 0; j < NUM_CARGO; j++) {
				if (!st->goods[j].cargo.Empty()) {
					StationsWndShowStationRating(x, y, j, st->goods[j].cargo.Count(), st->goods[j].rating);
					x += 20;
				}
			}
			y += 10;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case SLW_LIST: {
				uint32 id_v = (pt.y - 41) / 10;

				if (id_v >= this->vscroll.cap) return; // click out of bounds

				id_v += this->vscroll.pos;

				if (id_v >= this->stations.Length()) return; // click out of list bound

				const Station *st = this->stations[id_v];
				/* do not check HasStationInUse - it is slow and may be invalid */
				assert(st->owner == (Owner)this->window_number || (st->owner == OWNER_NONE && !st->IsBuoy()));

				if (_ctrl_pressed) {
					ShowExtraViewPortWindow(st->xy);
				} else {
					ScrollMainWindowToTile(st->xy);
				}
				break;
			}

			case SLW_TRAIN:
			case SLW_TRUCK:
			case SLW_BUS:
			case SLW_AIRPLANE:
			case SLW_SHIP:
				if (_ctrl_pressed) {
					ToggleBit(this->facilities, widget - SLW_TRAIN);
					this->ToggleWidgetLoweredState(widget);
				} else {
					uint i;
					FOR_EACH_SET_BIT(i, this->facilities) {
						this->RaiseWidget(i + SLW_TRAIN);
					}
					SetBit(this->facilities, widget - SLW_TRAIN);
					this->LowerWidget(widget);
				}
				this->SetWidgetLoweredState(SLW_FACILALL, this->facilities == (FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK));
				this->stations.ForceRebuild();
				this->SetDirty();
				break;

			case SLW_FACILALL:
				for (uint i = 0; i < 5; i++) {
					this->LowerWidget(i + SLW_TRAIN);
				}
				this->LowerWidget(SLW_FACILALL);

				this->facilities = FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK;
				this->stations.ForceRebuild();
				this->SetDirty();
				break;

			case SLW_CARGOALL: {
				uint i = 0;
				for (CargoID c = 0; c < NUM_CARGO; c++) {
					if (!GetCargo(c)->IsValid()) continue;
					this->LowerWidget(i + SLW_CARGOSTART);
					i++;
				}
				this->LowerWidget(SLW_NOCARGOWAITING);
				this->LowerWidget(SLW_CARGOALL);

				this->cargo_filter = _cargo_mask;
				this->include_empty = true;
				this->stations.ForceRebuild();
				this->SetDirty();
				break;
			}

			case SLW_SORTBY: // flip sorting method asc/desc
				this->stations.ToggleSortOrder();
				this->flags4 |= WF_TIMEOUT_BEGIN;
				this->LowerWidget(SLW_SORTBY);
				this->SetDirty();
				break;

			case SLW_SORTDROPBTN: // select sorting criteria dropdown menu
				ShowDropDownMenu(this, this->sorter_names, this->stations.SortType(), SLW_SORTDROPBTN, 0, 0);
				break;

			case SLW_NOCARGOWAITING:
				if (_ctrl_pressed) {
					this->include_empty = !this->include_empty;
					this->ToggleWidgetLoweredState(SLW_NOCARGOWAITING);
				} else {
					for (uint i = SLW_CARGOSTART; i < this->widget_count; i++) {
						this->RaiseWidget(i);
					}

					this->cargo_filter = 0;
					this->include_empty = true;

					this->LowerWidget(SLW_NOCARGOWAITING);
				}
				this->SetWidgetLoweredState(SLW_CARGOALL, this->cargo_filter == _cargo_mask && this->include_empty);
				this->stations.ForceRebuild();
				this->SetDirty();
				break;

			default:
				if (widget >= SLW_CARGOSTART) { // change cargo_filter
					/* Determine the selected cargo type */
					CargoID c;
					int i = 0;
					for (c = 0; c < NUM_CARGO; c++) {
						if (!GetCargo(c)->IsValid()) continue;
						if (widget - SLW_CARGOSTART == i) break;
						i++;
					}

					if (_ctrl_pressed) {
						ToggleBit(this->cargo_filter, c);
						this->ToggleWidgetLoweredState(widget);
					} else {
						for (uint i = SLW_CARGOSTART; i < this->widget_count; i++) {
							this->RaiseWidget(i);
						}
						this->RaiseWidget(SLW_NOCARGOWAITING);

						this->cargo_filter = 0;
						this->include_empty = false;

						SetBit(this->cargo_filter, c);
						this->LowerWidget(widget);
					}
					this->SetWidgetLoweredState(SLW_CARGOALL, this->cargo_filter == _cargo_mask && this->include_empty);
					this->stations.ForceRebuild();
					this->SetDirty();
				}
				break;
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		if (this->stations.SortType() != index) {
			this->stations.SetSortType(index);

			/* Display the current sort variant */
			this->widget[SLW_SORTDROPBTN].data = this->sorter_names[this->stations.SortType()];

			this->SetDirty();
		}
	}

	virtual void OnTick()
	{
		if (_pause_game != 0) return;
		if (this->stations.NeedResort()) {
			DEBUG(misc, 3, "Periodic rebuild station list company %d", this->window_number);
			this->SetDirty();
		}
	}

	virtual void OnTimeout()
	{
		this->RaiseWidget(SLW_SORTBY);
		this->SetDirty();
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		this->vscroll.cap += delta.y / 10;
	}

	virtual void OnInvalidateData(int data)
	{
		if (data == 0) {
			this->stations.ForceRebuild();
		} else {
			this->stations.ForceResort();
		}
	}
};

Listing CompanyStationsWindow::last_sorting = {false, 0};
byte CompanyStationsWindow::facilities = FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK;
bool CompanyStationsWindow::include_empty = true;
const uint32 CompanyStationsWindow::cargo_filter_max = UINT32_MAX;
uint32 CompanyStationsWindow::cargo_filter = UINT32_MAX;
const Station *CompanyStationsWindow::last_station = NULL;

/* Availible station sorting functions */
GUIStationList::SortFunction * const CompanyStationsWindow::sorter_funcs[] = {
	&StationNameSorter,
	&StationTypeSorter,
	&StationWaitingSorter,
	&StationRatingMaxSorter
};

/* Names of the sorting functions */
const StringID CompanyStationsWindow::sorter_names[] = {
	STR_SORT_BY_DROPDOWN_NAME,
	STR_SORT_BY_FACILITY,
	STR_SORT_BY_WAITING,
	STR_SORT_BY_RATING_MAX,
	INVALID_STRING_ID
};


static const Widget _company_stations_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,          STR_018B_CLOSE_WINDOW},            // SLW_CLOSEBOX
{    WWT_CAPTION,  RESIZE_RIGHT,  COLOUR_GREY,    11,   345,     0,    13, STR_3048_STATIONS, STR_018C_WINDOW_TITLE_DRAG_THIS},  // SLW_CAPTION
{  WWT_STICKYBOX,     RESIZE_LR,  COLOUR_GREY,   346,   357,     0,    13, 0x0,               STR_STICKY_BUTTON},                // SLW_STICKY
{      WWT_PANEL,     RESIZE_RB,  COLOUR_GREY,     0,   345,    37,   161, 0x0,               STR_3057_STATION_NAMES_CLICK_ON},  // SLW_LIST
{  WWT_SCROLLBAR,    RESIZE_LRB,  COLOUR_GREY,   346,   357,    37,   149, 0x0,               STR_0190_SCROLL_BAR_SCROLLS_LIST}, // SLW_SCROLLBAR
{  WWT_RESIZEBOX,   RESIZE_LRTB,  COLOUR_GREY,   346,   357,   150,   161, 0x0,               STR_RESIZE_BUTTON},                // SLW_RESIZE

{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,    13,    14,    24, STR_TRAIN,         STR_USE_CTRL_TO_SELECT_MORE},      // SLW_TRAIN
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    14,    27,    14,    24, STR_LORRY,         STR_USE_CTRL_TO_SELECT_MORE},      // SLW_TRUCK
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    28,    41,    14,    24, STR_BUS,           STR_USE_CTRL_TO_SELECT_MORE},      // SLW_BUS
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    42,    55,    14,    24, STR_PLANE,         STR_USE_CTRL_TO_SELECT_MORE},      // SLW_AIRPLANE
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    56,    69,    14,    24, STR_SHIP,          STR_USE_CTRL_TO_SELECT_MORE},      // SLW_SHIP
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    70,    83,    14,    24, 0x0,               STR_SELECT_ALL_FACILITIES},        // SLW_FACILALL

{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    83,    88,    14,    24, 0x0,               STR_NULL},                         // SLW_PAN_BETWEEN
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    89,   102,    14,    24, 0x0,               STR_NO_WAITING_CARGO},             // SLW_NOCARGOWAITING
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   103,   116,    14,    24, 0x0,               STR_SELECT_ALL_TYPES},             // SLW_CARGOALL
{      WWT_PANEL,  RESIZE_RIGHT,  COLOUR_GREY,   117,   357,    14,    24, 0x0,               STR_NULL},                         // SLW_PAN_RIGHT

{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,    80,    25,    36, STR_SORT_BY,       STR_SORT_ORDER_TIP},               // SLW_SORTBY
{   WWT_DROPDOWN,   RESIZE_NONE,  COLOUR_GREY,    81,   243,    25,    36, 0x0,               STR_SORT_CRITERIA_TIP},            // SLW_SORTDROPBTN
{      WWT_PANEL,  RESIZE_RIGHT,  COLOUR_GREY,   244,   357,    25,    36, 0x0,               STR_NULL},                         // SLW_PAN_SORT_RIGHT
{   WIDGETS_END},
};

static const WindowDesc _company_stations_desc(
	WDP_AUTO, WDP_AUTO, 358, 162, 358, 162,
	WC_STATION_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_company_stations_widgets
);

/**
 * Opens window with list of company's stations
 *
 * @param company whose stations' list show
 */
void ShowCompanyStations(CompanyID company)
{
	if (!IsValidCompanyID(company)) return;

	AllocateWindowDescFront<CompanyStationsWindow>(&_company_stations_desc, company);
}

static const Widget _station_view_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,          STR_018B_CLOSE_WINDOW},                // SVW_CLOSEBOX
{    WWT_CAPTION,  RESIZE_RIGHT,  COLOUR_GREY,    11,   236,     0,    13, STR_300A_0,        STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,  COLOUR_GREY,   237,   248,     0,    13, 0x0,               STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_RB,  COLOUR_GREY,     0,   236,    14,    65, 0x0,               STR_NULL},                             // SVW_WAITING
{  WWT_SCROLLBAR,    RESIZE_LRB,  COLOUR_GREY,   237,   248,    14,    65, 0x0,               STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,    RESIZE_RTB,  COLOUR_GREY,     0,   248,    66,    97, 0x0,               STR_NULL},                             // SVW_ACCEPTLIST / SVW_RATINGLIST
{ WWT_PUSHTXTBTN,     RESIZE_TB,  COLOUR_GREY,     0,    59,    98,   109, STR_00E4_LOCATION, STR_3053_CENTER_MAIN_VIEW_ON_STATION}, // SVW_LOCATION
{ WWT_PUSHTXTBTN,     RESIZE_TB,  COLOUR_GREY,    60,   120,    98,   109, STR_3032_RATINGS,  STR_3054_SHOW_STATION_RATINGS},        // SVW_RATINGS / SVW_ACCEPTS
{ WWT_PUSHTXTBTN,    RESIZE_RTB,  COLOUR_GREY,   121,   180,    98,   109, STR_0130_RENAME,   STR_3055_CHANGE_NAME_OF_STATION},      // SVW_RENAME
{ WWT_PUSHTXTBTN,   RESIZE_LRTB,  COLOUR_GREY,   181,   194,    98,   109, STR_TRAIN,         STR_SCHEDULED_TRAINS_TIP },            // SVW_TRAINS
{ WWT_PUSHTXTBTN,   RESIZE_LRTB,  COLOUR_GREY,   195,   208,    98,   109, STR_LORRY,         STR_SCHEDULED_ROAD_VEHICLES_TIP },     // SVW_ROADVEHS
{ WWT_PUSHTXTBTN,   RESIZE_LRTB,  COLOUR_GREY,   209,   222,    98,   109, STR_PLANE,         STR_SCHEDULED_AIRCRAFT_TIP },          // SVW_PLANES
{ WWT_PUSHTXTBTN,   RESIZE_LRTB,  COLOUR_GREY,   223,   236,    98,   109, STR_SHIP,          STR_SCHEDULED_SHIPS_TIP },             // SVW_SHIPS
{  WWT_RESIZEBOX,   RESIZE_LRTB,  COLOUR_GREY,   237,   248,    98,   109, 0x0,               STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

SpriteID GetCargoSprite(CargoID i)
{
	const CargoSpec *cs = GetCargo(i);
	SpriteID sprite;

	if (cs->sprite == 0xFFFF) {
		/* A value of 0xFFFF indicates we should draw a custom icon */
		sprite = GetCustomCargoSprite(cs);
	} else {
		sprite = cs->sprite;
	}

	if (sprite == 0) sprite = SPR_CARGO_GOODS;

	return sprite;
}

/**
 * Draws icons of waiting cargo in the StationView window
 *
 * @param i type of cargo
 * @param waiting number of waiting units
 * @param x x on-screen coordinate where to start with drawing icons
 * @param y y coordinate
 */
static void DrawCargoIcons(CargoID i, uint waiting, int x, int y, uint width)
{
	uint num = min((waiting + 5) / 10, width / 10); // maximum is width / 10 icons so it won't overflow
	if (num == 0) return;

	SpriteID sprite = GetCargoSprite(i);

	do {
		DrawSprite(sprite, PAL_NONE, x, y);
		x += 10;
	} while (--num);
}

struct CargoData {
	CargoID cargo;
	StationID source;
	uint count;

	CargoData(CargoID cargo, StationID source, uint count) :
		cargo(cargo),
		source(source),
		count(count)
	{ }
};

typedef std::list<CargoData> CargoDataList;

/**
 * The StationView window
 */
struct StationViewWindow : public Window {
	uint32 cargo;                 ///< Bitmask of cargo types to expand
	uint16 cargo_rows[NUM_CARGO]; ///< Header row for each cargo type

	StationViewWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		Owner owner = GetStation(window_number)->owner;
		if (owner != OWNER_NONE) this->owner = owner;
		this->vscroll.cap = 5;
		this->resize.step_height = 10;

		this->FindWindowPlacementAndResize(desc);
	}

	~StationViewWindow()
	{
		WindowNumber wno =
			(this->window_number << 16) | VLW_STATION_LIST | GetStation(this->window_number)->owner;

		DeleteWindowById(WC_TRAINS_LIST, wno | (VEH_TRAIN << 11), false);
		DeleteWindowById(WC_ROADVEH_LIST, wno | (VEH_ROAD << 11), false);
		DeleteWindowById(WC_SHIPS_LIST, wno | (VEH_SHIP << 11), false);
		DeleteWindowById(WC_AIRCRAFT_LIST, wno | (VEH_AIRCRAFT << 11), false);
	}

	virtual void OnPaint()
	{
		StationID station_id = this->window_number;
		const Station *st = GetStation(station_id);
		CargoDataList cargolist;
		uint32 transfers = 0;

		/* count types of cargos waiting in station */
		for (CargoID i = 0; i < NUM_CARGO; i++) {
			if (st->goods[i].cargo.Empty()) {
				this->cargo_rows[i] = 0;
			} else {
				/* Add an entry for total amount of cargo of this type waiting. */
				cargolist.push_back(CargoData(i, INVALID_STATION, st->goods[i].cargo.Count()));

				/* Set the row for this cargo entry for the expand/hide button */
				this->cargo_rows[i] = (uint16)cargolist.size();

				/* Add an entry for each distinct cargo source. */
				const CargoList::List *packets = st->goods[i].cargo.Packets();
				for (CargoList::List::const_iterator it = packets->begin(); it != packets->end(); it++) {
					const CargoPacket *cp = *it;
					if (cp->source != station_id) {
						bool added = false;

						/* Enable the expand/hide button for this cargo type */
						SetBit(transfers, i);

						/* Don't add cargo lines if not expanded */
						if (!HasBit(this->cargo, i)) break;

						/* Check if we already have this source in the list */
						for (CargoDataList::iterator jt = cargolist.begin(); jt != cargolist.end(); jt++) {
							CargoData *cd = &(*jt);
							if (cd->cargo == i && cd->source == cp->source) {
								cd->count += cp->count;
								added = true;
								break;
							}
						}

						if (!added) cargolist.push_back(CargoData(i, cp->source, cp->count));
					}
				}
			}
		}
		SetVScrollCount(this, (int)cargolist.size() + 1); // update scrollbar

		/* disable some buttons */
		this->SetWidgetDisabledState(SVW_RENAME,   st->owner != _local_company);
		this->SetWidgetDisabledState(SVW_TRAINS,   !(st->facilities & FACIL_TRAIN));
		this->SetWidgetDisabledState(SVW_ROADVEHS, !(st->facilities & FACIL_TRUCK_STOP) && !(st->facilities & FACIL_BUS_STOP));
		this->SetWidgetDisabledState(SVW_PLANES,   !(st->facilities & FACIL_AIRPORT));
		this->SetWidgetDisabledState(SVW_SHIPS,    !(st->facilities & FACIL_DOCK));

		SetDParam(0, st->index);
		SetDParam(1, st->facilities);
		this->DrawWidgets();

		int x = 2;  ///< coordinates used for printing waiting/accepted/rating of cargo
		int y = 15;
		int pos = this->vscroll.pos; ///< = this->vscroll.pos

		uint width = this->widget[SVW_WAITING].right - this->widget[SVW_WAITING].left - 4;
		int maxrows = this->vscroll.cap;

		StringID str;

		if (--pos < 0) {
			str = STR_00D0_NOTHING;
			for (CargoID i = 0; i < NUM_CARGO; i++) {
				if (!st->goods[i].cargo.Empty()) str = STR_EMPTY;
			}
			SetDParam(0, str);
			DrawString(x, y, STR_0008_WAITING, TC_FROMSTRING);
			y += 10;
		}

		for (CargoDataList::const_iterator it = cargolist.begin(); it != cargolist.end() && pos > -maxrows; ++it) {
			if (--pos < 0) {
				const CargoData *cd = &(*it);
				if (cd->source == INVALID_STATION) {
					/* Heading */
					DrawCargoIcons(cd->cargo, cd->count, x, y, width);
					SetDParam(0, cd->cargo);
					SetDParam(1, cd->count);
					if (HasBit(transfers, cd->cargo)) {
						/* This cargo has transfers waiting so show the expand or shrink 'button' */
						const char *sym = HasBit(this->cargo, cd->cargo) ? "-" : "+";
						DrawStringRightAligned(x + width - 8, y, STR_0009, TC_FROMSTRING);
						DoDrawString(sym, x + width - 6, y, TC_YELLOW);
					} else {
						DrawStringRightAligned(x + width, y, STR_0009, TC_FROMSTRING);
					}
				} else {
					SetDParam(0, cd->cargo);
					SetDParam(1, cd->count);
					SetDParam(2, cd->source);
					DrawStringRightAlignedTruncated(x + width, y, STR_EN_ROUTE_FROM, TC_FROMSTRING, width);
				}

				y += 10;
			}
		}

		if (this->widget[SVW_ACCEPTS].data == STR_3032_RATINGS) { // small window with list of accepted cargo
			char string[512];
			char *b = string;
			bool first = true;

			b = InlineString(b, STR_000C_ACCEPTS);

			for (CargoID i = 0; i < NUM_CARGO; i++) {
				if (b >= lastof(string) - (1 + 2 * 4)) break; // ',' or ' ' and two calls to Utf8Encode()
				if (HasBit(st->goods[i].acceptance_pickup, GoodsEntry::ACCEPTANCE)) {
					if (first) {
						first = false;
					} else {
						/* Add a comma if this is not the first item */
						*b++ = ',';
						*b++ = ' ';
					}
					b = InlineString(b, GetCargo(i)->name);
				}
			}

			/* If first is still true then no cargo is accepted */
			if (first) b = InlineString(b, STR_00D0_NOTHING);

			*b = '\0';

			/* Make sure we detect any buffer overflow */
			assert(b < endof(string));

			SetDParamStr(0, string);
			DrawStringMultiLine(2, this->widget[SVW_ACCEPTLIST].top + 1, STR_JUST_RAW_STRING, this->widget[SVW_ACCEPTLIST].right - this->widget[SVW_ACCEPTLIST].left);
		} else { // extended window with list of cargo ratings
			y = this->widget[SVW_RATINGLIST].top + 1;

			DrawString(2, y, STR_3034_LOCAL_RATING_OF_TRANSPORT, TC_FROMSTRING);
			y += 10;

			for (CargoID i = 0; i < NUM_CARGO; i++) {
				const CargoSpec *cs = GetCargo(i);
				if (!cs->IsValid()) continue;

				const GoodsEntry *ge = &st->goods[i];
				if (!HasBit(ge->acceptance_pickup, GoodsEntry::PICKUP)) continue;

				SetDParam(0, cs->name);
				SetDParam(2, ge->rating * 101 >> 8);
				SetDParam(1, STR_3035_APPALLING + (ge->rating >> 5));
				DrawString(8, y, STR_303D, TC_FROMSTRING);
				y += 10;
			}
		}
	}

	void HandleCargoWaitingClick(int row)
	{
		if (row == 0) return;

		for (CargoID c = 0; c < NUM_CARGO; c++) {
			if (this->cargo_rows[c] == row) {
				ToggleBit(this->cargo, c);
				this->InvalidateWidget(SVW_WAITING);
				break;
			}
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case SVW_WAITING:
				this->HandleCargoWaitingClick((pt.y - this->widget[SVW_WAITING].top) / 10 + this->vscroll.pos);
				break;

			case SVW_LOCATION:
				if (_ctrl_pressed) {
					ShowExtraViewPortWindow(GetStation(this->window_number)->xy);
				} else {
					ScrollMainWindowToTile(GetStation(this->window_number)->xy);
				}
				break;

			case SVW_RATINGS:
				this->SetDirty();

				if (this->widget[SVW_RATINGS].data == STR_3032_RATINGS) {
					/* Switch to ratings view */
					this->widget[SVW_RATINGS].data = STR_3033_ACCEPTS;
					this->widget[SVW_RATINGS].tooltips = STR_3056_SHOW_LIST_OF_ACCEPTED_CARGO;
					ResizeWindowForWidget(this, SVW_ACCEPTLIST, 0, 100);
				} else {
					/* Switch to accepts view */
					this->widget[SVW_RATINGS].data = STR_3032_RATINGS;
					this->widget[SVW_RATINGS].tooltips = STR_3054_SHOW_STATION_RATINGS;
					ResizeWindowForWidget(this, SVW_ACCEPTLIST, 0, -100);
				}

				this->SetDirty();
				break;

			case SVW_RENAME:
				SetDParam(0, this->window_number);
				ShowQueryString(STR_STATION, STR_3030_RENAME_STATION_LOADING, MAX_LENGTH_STATION_NAME_BYTES, MAX_LENGTH_STATION_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT);
				break;

			case SVW_TRAINS: { // Show a list of scheduled trains to this station
				const Station *st = GetStation(this->window_number);
				ShowVehicleListWindow(st->owner, VEH_TRAIN, (StationID)this->window_number);
				break;
			}

			case SVW_ROADVEHS: { // Show a list of scheduled road-vehicles to this station
				const Station *st = GetStation(this->window_number);
				ShowVehicleListWindow(st->owner, VEH_ROAD, (StationID)this->window_number);
				break;
			}

			case SVW_PLANES: { // Show a list of scheduled aircraft to this station
				const Station *st = GetStation(this->window_number);
				/* Since oilrigs have no owners, show the scheduled aircraft of local company */
				Owner owner = (st->owner == OWNER_NONE) ? _local_company : st->owner;
				ShowVehicleListWindow(owner, VEH_AIRCRAFT, (StationID)this->window_number);
				break;
			}

			case SVW_SHIPS: { // Show a list of scheduled ships to this station
				const Station *st = GetStation(this->window_number);
				/* Since oilrigs/bouys have no owners, show the scheduled ships of local company */
				Owner owner = (st->owner == OWNER_NONE) ? _local_company : st->owner;
				ShowVehicleListWindow(owner, VEH_SHIP, (StationID)this->window_number);
				break;
			}
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		DoCommandP(0, this->window_number, 0, CMD_RENAME_STATION | CMD_MSG(STR_3031_CAN_T_RENAME_STATION), NULL, str);
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		if (delta.x != 0) ResizeButtons(this, SVW_LOCATION, SVW_RENAME);
		this->vscroll.cap += delta.y / (int)this->resize.step_height;
	}
};


static const WindowDesc _station_view_desc(
	WDP_AUTO, WDP_AUTO, 249, 110, 249, 110,
	WC_STATION_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_station_view_widgets
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

static SmallVector<StationID, 8> _stations_nearby_list;
static SmallMap<TileIndex, StationID, 8> _deleted_stations_nearby;

/** Context for FindStationsNearby */
struct FindNearbyStationContext {
	TileIndex tile; ///< Base tile of station to be built
	uint      w;    ///< Width of station to be built
	uint      h;    ///< Height of station to be built
};

/**
 * Add station on this tile to _stations_nearby_list if it's fully within the
 * station spread.
 * @param tile Tile just being checked
 * @param user_data Pointer to FindNearbyStationContext context
 */
static bool AddNearbyStation(TileIndex tile, void *user_data)
{
	FindNearbyStationContext *ctx = (FindNearbyStationContext *)user_data;

	/* First check if there was a deleted station here */
	SmallPair<TileIndex, StationID> *dst = _deleted_stations_nearby.Find(tile);
	if (dst != _deleted_stations_nearby.End()) {
		_stations_nearby_list.Include(dst->second);
		return false;
	}

	/* Check if own station and if we stay within station spread */
	if (!IsTileType(tile, MP_STATION)) return false;

	StationID sid = GetStationIndex(tile);
	Station *st = GetStation(sid);
	if (st->owner != _local_company || _stations_nearby_list.Contains(sid)) return false;

	if (st->rect.BeforeAddRect(ctx->tile, ctx->w, ctx->h, StationRect::ADD_TEST)) {
		*_stations_nearby_list.Append() = sid;
	}

	return false; // We want to include *all* nearby stations
}

/**
 * Circulate around the to-be-built station to find stations we could join.
 * Make sure that only stations are returned where joining wouldn't exceed
 * station spread and are our own station.
 * @param tile Base tile of the to-be-built station
 * @param w Width of the to-be-built station
 * @param h Height of the to-be-built station
 * @param distant_join Search for adjacent stations (false) or stations fully
 *                     within station spread
 **/
static const Station *FindStationsNearby(TileIndex tile, int w, int h, bool distant_join)
{
	FindNearbyStationContext ctx;
	ctx.tile = tile;
	ctx.w = w;
	ctx.h = h;

	_stations_nearby_list.Clear();
	_deleted_stations_nearby.Clear();

	/* Check the inside, to return, if we sit on another station */
	BEGIN_TILE_LOOP(t, w, h, tile)
		if (t < MapSize() && IsTileType(t, MP_STATION)) return GetStationByTile(t);
	END_TILE_LOOP(t, w, h, tile)

	/* Look for deleted stations */
	const Station *st;
	FOR_ALL_STATIONS(st) {
		if (st->facilities == 0 && st->owner == _local_company) {
			/* Include only within station spread (yes, it is strictly less than) */
			if (max(DistanceMax(tile, st->xy), DistanceMax(TILE_ADDXY(tile, w - 1, h - 1), st->xy)) < _settings_game.station.station_spread) {
				_deleted_stations_nearby.Insert(st->xy, st->index);

				/* Add the station when it's within where we're going to build */
				if (IsInsideBS(TileX(st->xy), TileX(ctx.tile), ctx.w) &&
						IsInsideBS(TileY(st->xy), TileY(ctx.tile), ctx.h)) {
					AddNearbyStation(st->xy, &ctx);
				}
			}
		}
	}

	/* Only search tiles where we have a chance to stay within the station spread.
	 * The complete check needs to be done in the callback as we don't know the
	 * extent of the found station, yet. */
	if (distant_join && min(w, h) >= _settings_game.station.station_spread) return NULL;
	uint max_dist = distant_join ? _settings_game.station.station_spread - min(w, h) : 1;

	tile = TILE_ADD(ctx.tile, TileOffsByDir(DIR_N));
	CircularTileSearch(&tile, max_dist, w, h, AddNearbyStation, &ctx);

	return NULL;
}

enum JoinStationWidgets {
	JSW_WIDGET_CLOSEBOX = 0,
	JSW_WIDGET_CAPTION,
	JSW_PANEL,
	JSW_SCROLLBAR,
	JSW_EMPTY,
	JSW_RESIZEBOX,
};

static const Widget _select_station_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE, COLOUR_DARK_GREEN,     0,    10,     0,    13, STR_00C5,                        STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT, COLOUR_DARK_GREEN,    11,   199,     0,    13, STR_SELECT_STATION_TO_JOIN,      STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,     RESIZE_RB, COLOUR_DARK_GREEN,     0,   187,    14,    79, 0x0,                             STR_NULL},
{  WWT_SCROLLBAR,    RESIZE_LRB, COLOUR_DARK_GREEN,   188,   199,    14,    79, 0x0,                             STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,    RESIZE_RTB, COLOUR_DARK_GREEN,     0,   187,    80,    91, 0x0,                             STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB, COLOUR_DARK_GREEN,   188,   199,    80,    91, 0x0,                             STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

struct SelectStationWindow : Window {
	CommandContainer select_station_cmd; ///< Command to build new station
	TileIndex tile; ///< Base tile of new station
	int size_x;     ///< Size in x direction of new station
	int size_y;     ///< Size in y direction of new station

	SelectStationWindow(const WindowDesc *desc, CommandContainer cmd, int w, int h) :
		Window(desc, 0),
		select_station_cmd(cmd),
		tile(cmd.tile),
		size_x(w),
		size_y(h)
	{
		this->vscroll.cap = 6;
		this->resize.step_height = 10;

		FindStationsNearby(this->tile, this->size_x, this->size_y, true);

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		SetVScrollCount(this, _stations_nearby_list.Length() + 1);

		this->DrawWidgets();

		uint y = 17;
		if (this->vscroll.pos == 0) {
			DrawStringTruncated(3, y, STR_CREATE_SPLITTED_STATION, TC_FROMSTRING, this->widget[JSW_PANEL].right - 5);
			y += 10;
		}

		for (uint i = max<uint>(1, this->vscroll.pos); i <= _stations_nearby_list.Length(); ++i, y += 10) {
			/* Don't draw anything if it extends past the end of the window. */
			if (i - this->vscroll.pos >= this->vscroll.cap) break;

			const Station *st = GetStation(_stations_nearby_list[i - 1]);
			SetDParam(0, st->index);
			SetDParam(1, st->facilities);
			DrawStringTruncated(3, y, STR_3049_0, TC_FROMSTRING, this->widget[JSW_PANEL].right - 5);
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget != JSW_PANEL) return;

		uint32 st_index = (pt.y - 16) / 10 + this->vscroll.pos;
		bool distant_join = (st_index > 0);
		if (distant_join) st_index--;

		if (distant_join && st_index >= _stations_nearby_list.Length()) return;

		/* Insert station to be joined into stored command */
		SB(this->select_station_cmd.p2, 16, 16,
		   (distant_join ? _stations_nearby_list[st_index] : INVALID_STATION));

		/* Execute stored Command */
		DoCommandP(&this->select_station_cmd);

		/* Close Window; this might cause double frees! */
		DeleteWindowById(WC_SELECT_STATION, 0);
	}

	virtual void OnTick()
	{
		if (_thd.dirty & 2) {
			_thd.dirty &= ~2;
			this->SetDirty();
		}
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		this->vscroll.cap = (this->widget[JSW_PANEL].bottom - this->widget[JSW_PANEL].top) / 10;
	}

	virtual void OnInvalidateData(int data)
	{
		FindStationsNearby(this->tile, this->size_x, this->size_y, true);
		this->SetDirty();
	}
};

static const WindowDesc _select_station_desc(
	WDP_AUTO, WDP_AUTO, 200, 92, 200, 182,
	WC_SELECT_STATION, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_RESIZABLE | WDF_CONSTRUCTION,
	_select_station_widgets
);


/**
 * Check whether we need to show the station selection window.
 * @param cmd Command to build the station.
 * @param w Width of the to-be-built station
 * @param h Height of the to-be-built station
 * @return whether we need to show the station selection window.
 */
static bool StationJoinerNeeded(CommandContainer cmd, int w, int h)
{
	/* Only show selection if distant join is enabled in the settings */
	if (!_settings_game.station.distant_join_stations) return false;

	/* If a window is already opened and we didn't ctrl-click,
	 * return true (i.e. just flash the old window) */
	Window *selection_window = FindWindowById(WC_SELECT_STATION, 0);
	if (selection_window != NULL) {
		if (!_ctrl_pressed) return true;

		/* Abort current distant-join and start new one */
		delete selection_window;
		UpdateTileSelection();
	}

	/* only show the popup, if we press ctrl */
	if (!_ctrl_pressed) return false;

	/* Now check if we could build there */
	if (CmdFailed(DoCommand(&cmd, CommandFlagsToDCFlags(GetCommandFlags(cmd.cmd))))) return false;

	/* Test for adjacent station or station below selection.
	 * If adjacent-stations is disabled and we are building next to a station, do not show the selection window.
	 * but join the other station immediatelly. */
	const Station *st = FindStationsNearby(cmd.tile, w, h, false);
	return st == NULL && (_settings_game.station.adjacent_stations || _stations_nearby_list.Length() == 0);
}

/**
 * Show the station selection window when needed. If not, build the station.
 * @param cmd Command to build the station.
 * @param w Width of the to-be-built station
 * @param h Height of the to-be-built station
 */
void ShowSelectStationIfNeeded(CommandContainer cmd, int w, int h)
{
	if (StationJoinerNeeded(cmd, w, h)) {
		if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
		if (BringWindowToFrontById(WC_SELECT_STATION, 0)) return;
		new SelectStationWindow(&_select_station_desc, cmd, w, h);
	} else {
		DoCommandP(&cmd);
	}
}
