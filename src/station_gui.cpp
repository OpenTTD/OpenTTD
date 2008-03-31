/* $Id$ */

/** @file station_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "station_base.h"
#include "player_func.h"
#include "economy_func.h"
#include "town.h"
#include "command_func.h"
#include "variables.h"
#include "vehicle_gui.h"
#include "cargotype.h"
#include "station_gui.h"
#include "station_func.h"
#include "strings_func.h"
#include "core/alloc_func.hpp"
#include "window_func.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "widgets/dropdown_func.h"
#include "newgrf_cargo.h"

#include "table/strings.h"
#include "table/sprites.h"

typedef int CDECL StationSortListingTypeFunction(const void*, const void*);

static StationSortListingTypeFunction StationNameSorter;
static StationSortListingTypeFunction StationTypeSorter;
static StationSortListingTypeFunction StationWaitingSorter;
static StationSortListingTypeFunction StationRatingMaxSorter;

bool _station_show_coverage;

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

const StringID _station_sort_listing[] = {
	STR_SORT_BY_DROPDOWN_NAME,
	STR_SORT_BY_FACILITY,
	STR_SORT_BY_WAITING,
	STR_SORT_BY_RATING_MAX,
	INVALID_STRING_ID
};

static char _bufcache[64];
static const Station* _last_station;
static int _internal_sort_order;

static int CDECL StationNameSorter(const void *a, const void *b)
{
	const Station* st1 = *(const Station**)a;
	const Station* st2 = *(const Station**)b;
	char buf1[64];
	int r;

	SetDParam(0, st1->index);
	GetString(buf1, STR_STATION, lastof(buf1));

	if (st2 != _last_station) {
		_last_station = st2;
		SetDParam(0, st2->index);
		GetString(_bufcache, STR_STATION, lastof(_bufcache));
	}

	r = strcmp(buf1, _bufcache); // sort by name
	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL StationTypeSorter(const void *a, const void *b)
{
	const Station* st1 = *(const Station**)a;
	const Station* st2 = *(const Station**)b;
	return (_internal_sort_order & 1) ? st2->facilities - st1->facilities : st1->facilities - st2->facilities;
}

static const uint32 _cargo_filter_max = ~0;
static uint32 _cargo_filter = _cargo_filter_max;

static int CDECL StationWaitingSorter(const void *a, const void *b)
{
	const Station* st1 = *(const Station**)a;
	const Station* st2 = *(const Station**)b;
	Money sum1 = 0, sum2 = 0;

	for (CargoID j = 0; j < NUM_CARGO; j++) {
		if (!HasBit(_cargo_filter, j)) continue;
		if (!st1->goods[j].cargo.Empty()) sum1 += GetTransportedGoodsIncome(st1->goods[j].cargo.Count(), 20, 50, j);
		if (!st2->goods[j].cargo.Empty()) sum2 += GetTransportedGoodsIncome(st2->goods[j].cargo.Count(), 20, 50, j);
	}

	return (_internal_sort_order & 1) ? ClampToI32(sum2 - sum1) : ClampToI32(sum1 - sum2);
}

/**
 * qsort-compatible version of sorting two stations by maximum rating
 * @param a   First object to be sorted, must be of type (const Station *)
 * @param b   Second object to be sorted, must be of type (const Station *)
 * @return    The sort order
 * @retval >0 a should come before b in the list
 * @retval <0 b should come before a in the list
 */
static int CDECL StationRatingMaxSorter(const void *a, const void *b)
{
	const Station* st1 = *(const Station**)a;
	const Station* st2 = *(const Station**)b;
	byte maxr1 = 0;
	byte maxr2 = 0;

	for (CargoID j = 0; j < NUM_CARGO; j++) {
		if (HasBit(st1->goods[j].acceptance_pickup, GoodsEntry::PICKUP)) maxr1 = max(maxr1, st1->goods[j].rating);
		if (HasBit(st2->goods[j].acceptance_pickup, GoodsEntry::PICKUP)) maxr2 = max(maxr2, st2->goods[j].rating);
	}

	return (_internal_sort_order & 1) ? maxr2 - maxr1 : maxr1 - maxr2;
}

/** Flags for station list */
enum StationListFlags {
	SL_ORDER   = 1 << 0, ///< Order - ascending (=0), descending (=1)
	SL_RESORT  = 1 << 1, ///< Resort the list
	SL_REBUILD = 1 << 2, ///< Rebuild the list
};

DECLARE_ENUM_AS_BIT_SET(StationListFlags);

/** Information about station list */
struct plstations_d {
	const Station** sort_list; ///< Pointer to list of stations
	uint16 list_length;        ///< Number of stations in list
	uint16 resort_timer;       ///< Tick counter to resort the list
	byte sort_type;            ///< Sort type - name, waiting, ...
	byte flags;                ///< Flags - SL_ORDER, SL_RESORT, SL_REBUILD
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(plstations_d));

/**
 * Set the 'SL_REBUILD' flag for all station lists
 */
void RebuildStationLists()
{
	Window* const *wz;

	FOR_ALL_WINDOWS(wz) {
		Window *w = *wz;
		if (w->window_class == WC_STATION_LIST) {
			WP(w, plstations_d).flags |= SL_REBUILD;
			SetWindowDirty(w);
		}
	}
}

/**
 * Set the 'SL_RESORT' flag for all station lists
 */
void ResortStationLists()
{
	Window* const *wz;

	FOR_ALL_WINDOWS(wz) {
		Window *w = *wz;
		if (w->window_class == WC_STATION_LIST) {
			WP(w, plstations_d).flags |= SL_RESORT;
			SetWindowDirty(w);
		}
	}
}

/**
 * Rebuild station list if the SL_REBUILD flag is set
 *
 * @param sl pointer to plstations_d (station list and flags)
 * @param owner player whose stations are to be in list
 * @param facilities types of stations of interest
 * @param cargo_filter bitmap of cargo types to include
 * @param include_empty whether we should include stations without waiting cargo
 */
static void BuildStationsList(plstations_d* sl, PlayerID owner, byte facilities, uint32 cargo_filter, bool include_empty)
{
	uint n = 0;
	const Station *st;

	if (!(sl->flags & SL_REBUILD)) return;

	/* Create array for sorting */
	const Station** station_sort = MallocT<const Station*>(GetMaxStationIndex() + 1);

	DEBUG(misc, 3, "Building station list for player %d", owner);

	FOR_ALL_STATIONS(st) {
		if (st->owner == owner || (st->owner == OWNER_NONE && !st->IsBuoy() && HasStationInUse(st->index, owner))) {
			if (facilities & st->facilities) { //only stations with selected facilities
				int num_waiting_cargo = 0;
				for (CargoID j = 0; j < NUM_CARGO; j++) {
					if (!st->goods[j].cargo.Empty()) {
						num_waiting_cargo++; //count number of waiting cargo
						if (HasBit(cargo_filter, j)) {
							station_sort[n++] = st;
							break;
						}
					}
				}
				/* stations without waiting cargo */
				if (num_waiting_cargo == 0 && include_empty) {
					station_sort[n++] = st;
				}
			}
		}
	}

	free((void*)sl->sort_list);
	sl->sort_list = MallocT<const Station*>(n);
	sl->list_length = n;

	for (uint i = 0; i < n; ++i) sl->sort_list[i] = station_sort[i];

	sl->flags &= ~SL_REBUILD;
	sl->flags |= SL_RESORT;
	free((void*)station_sort);
}


/**
 * Sort station list if the SL_RESORT flag is set
 *
 * @param sl pointer to plstations_d (station list and flags)
 */
static void SortStationsList(plstations_d *sl)
{
	static StationSortListingTypeFunction* const _station_sorter[] = {
		&StationNameSorter,
		&StationTypeSorter,
		&StationWaitingSorter,
		&StationRatingMaxSorter
	};

	if (!(sl->flags & SL_RESORT)) return;

	_internal_sort_order = sl->flags & SL_ORDER;
	_last_station = NULL; // used for "cache" in namesorting
	qsort((void*)sl->sort_list, sl->list_length, sizeof(sl->sort_list[0]), _station_sorter[sl->sort_type]);

	sl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
	sl->flags &= ~SL_RESORT;
}

/**
 * Fuction called when any WindowEvent occurs for PlayerStations window
 *
 * @param w pointer to the PlayerStations window
 * @param e pointer to window event
 */
static void PlayerStationsWndProc(Window *w, WindowEvent *e)
{
	const PlayerID owner = (PlayerID)w->window_number;
	static byte facilities = FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK;
	static Listing station_sort = {0, 0};
	static bool include_empty = true;

	plstations_d *sl = &WP(w, plstations_d);

	switch (e->event) {
		case WE_CREATE:
			if (_cargo_filter == _cargo_filter_max) _cargo_filter = _cargo_mask;

			for (uint i = 0; i < 5; i++) {
				if (HasBit(facilities, i)) w->LowerWidget(i + SLW_TRAIN);
			}
			w->SetWidgetLoweredState(SLW_FACILALL, facilities == (FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK));
			w->SetWidgetLoweredState(SLW_CARGOALL, _cargo_filter == _cargo_mask && include_empty);
			w->SetWidgetLoweredState(SLW_NOCARGOWAITING, include_empty);

			sl->sort_list = NULL;
			sl->flags = SL_REBUILD;
			sl->sort_type = station_sort.criteria;
			if (station_sort.order) sl->flags |= SL_ORDER;

			/* set up resort timer */
			sl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
			break;

		case WE_PAINT: {
			BuildStationsList(sl, owner, facilities, _cargo_filter, include_empty);
			SortStationsList(sl);

			SetVScrollCount(w, sl->list_length);

			/* draw widgets, with player's name in the caption */
			SetDParam(0, owner);
			SetDParam(1, w->vscroll.count);

			/* Set text of sort by dropdown */
			w->widget[SLW_SORTDROPBTN].data = _station_sort_listing[sl->sort_type];

			DrawWindowWidgets(w);

			/* draw arrow pointing up/down for ascending/descending sorting */
			DrawSortButtonState(w, SLW_SORTBY, sl->flags & SL_ORDER ? SBS_DOWN : SBS_UP);

			int cg_ofst;
			int x = 89;
			int y = 14;
			int xb = 2; ///< offset from left of widget

			uint i = 0;
			for (CargoID c = 0; c < NUM_CARGO; c++) {
				const CargoSpec *cs = GetCargo(c);
				if (!cs->IsValid()) continue;

				cg_ofst = HasBit(_cargo_filter, c) ? 2 : 1;
				GfxFillRect(x + cg_ofst, y + cg_ofst, x + cg_ofst + 10 , y + cg_ofst + 7, cs->rating_colour);
				DrawStringCentered(x + 6 + cg_ofst, y + cg_ofst, cs->abbrev, TC_BLACK);
				x += 14;
				i++;
			}

			x += 6;
			cg_ofst = w->IsWidgetLowered(SLW_NOCARGOWAITING) ? 2 : 1;
			DrawStringCentered(x + cg_ofst, y + cg_ofst, STR_ABBREV_NONE, TC_BLACK);
			x += 14;
			cg_ofst = w->IsWidgetLowered(SLW_CARGOALL) ? 2 : 1;
			DrawStringCentered(x + cg_ofst, y + cg_ofst, STR_ABBREV_ALL, TC_BLACK);

			cg_ofst = w->IsWidgetLowered(SLW_FACILALL) ? 2 : 1;
			DrawString(71 + cg_ofst, y + cg_ofst, STR_ABBREV_ALL, TC_BLACK);

			if (w->vscroll.count == 0) { // player has no stations
				DrawString(xb, 40, STR_304A_NONE, TC_FROMSTRING);
				return;
			}

			int max = min(w->vscroll.pos + w->vscroll.cap, sl->list_length);
			y = 40; // start of the list-widget

			for (int i = w->vscroll.pos; i < max; ++i) { // do until max number of stations of owner
				const Station *st = sl->sort_list[i];
				int x;

				assert(st->xy != 0);

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
			break;
		}

		case WE_CLICK:
			switch (e->we.click.widget) {
				case SLW_LIST: {
					uint32 id_v = (e->we.click.pt.y - 41) / 10;

					if (id_v >= w->vscroll.cap) return; // click out of bounds

					id_v += w->vscroll.pos;

					if (id_v >= sl->list_length) return; // click out of list bound

					const Station *st = sl->sort_list[id_v];
					/* do not check HasStationInUse - it is slow and may be invalid */
					assert(st->owner == owner || (st->owner == OWNER_NONE && !st->IsBuoy()));
					ScrollMainWindowToTile(st->xy);
					break;
				}

				case SLW_TRAIN:
				case SLW_TRUCK:
				case SLW_BUS:
				case SLW_AIRPLANE:
				case SLW_SHIP:
					if (_ctrl_pressed) {
						ToggleBit(facilities, e->we.click.widget - SLW_TRAIN);
						w->ToggleWidgetLoweredState(e->we.click.widget);
					} else {
						uint i;
						FOR_EACH_SET_BIT(i, facilities) {
							w->RaiseWidget(i + SLW_TRAIN);
						}
						SetBit(facilities, e->we.click.widget - SLW_TRAIN);
						w->LowerWidget(e->we.click.widget);
					}
					w->SetWidgetLoweredState(SLW_FACILALL, facilities == (FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK));
					sl->flags |= SL_REBUILD;
					SetWindowDirty(w);
					break;

				case SLW_FACILALL:
					for (uint i = 0; i < 5; i++) {
						w->LowerWidget(i + SLW_TRAIN);
					}
					w->LowerWidget(SLW_FACILALL);

					facilities = FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK;
					sl->flags |= SL_REBUILD;
					SetWindowDirty(w);
					break;

				case SLW_CARGOALL: {
					uint i = 0;
					for (CargoID c = 0; c < NUM_CARGO; c++) {
						if (!GetCargo(c)->IsValid()) continue;
						w->LowerWidget(i + SLW_CARGOSTART);
						i++;
					}
					w->LowerWidget(SLW_NOCARGOWAITING);
					w->LowerWidget(SLW_CARGOALL);

					_cargo_filter = _cargo_mask;
					include_empty = true;
					sl->flags |= SL_REBUILD;
					SetWindowDirty(w);
					break;
				}

				case SLW_SORTBY: // flip sorting method asc/desc
					sl->flags ^= SL_ORDER; //DESC-flag
					station_sort.order = HasBit(sl->flags, 0);
					sl->flags |= SL_RESORT;
					w->flags4 |= 5 << WF_TIMEOUT_SHL;
					w->LowerWidget(SLW_SORTBY);
					SetWindowDirty(w);
					break;

				case SLW_SORTDROPBTN: // select sorting criteria dropdown menu
					ShowDropDownMenu(w, _station_sort_listing, sl->sort_type, SLW_SORTDROPBTN, 0, 0);
					break;

				case SLW_NOCARGOWAITING:
					if (_ctrl_pressed) {
						include_empty = !include_empty;
						w->ToggleWidgetLoweredState(SLW_NOCARGOWAITING);
					} else {
						for (uint i = SLW_CARGOSTART; i < w->widget_count; i++) {
							w->RaiseWidget(i);
						}

						_cargo_filter = 0;
						include_empty = true;

						w->LowerWidget(SLW_NOCARGOWAITING);
					}
					sl->flags |= SL_REBUILD;
					w->SetWidgetLoweredState(SLW_CARGOALL, _cargo_filter == _cargo_mask && include_empty);
					SetWindowDirty(w);
					break;

				default:
					if (e->we.click.widget >= SLW_CARGOSTART) { // change cargo_filter
						/* Determine the selected cargo type */
						CargoID c;
						int i = 0;
						for (c = 0; c < NUM_CARGO; c++) {
							if (!GetCargo(c)->IsValid()) continue;
							if (e->we.click.widget - SLW_CARGOSTART == i) break;
							i++;
						}

						if (_ctrl_pressed) {
							ToggleBit(_cargo_filter, c);
							w->ToggleWidgetLoweredState(e->we.click.widget);
						} else {
							for (uint i = SLW_CARGOSTART; i < w->widget_count; i++) {
								w->RaiseWidget(i);
							}
							w->RaiseWidget(SLW_NOCARGOWAITING);

							_cargo_filter = 0;
							include_empty = false;

							SetBit(_cargo_filter, c);
							w->LowerWidget(e->we.click.widget);
						}
						sl->flags |= SL_REBUILD;
						w->SetWidgetLoweredState(SLW_CARGOALL, _cargo_filter == _cargo_mask && include_empty);
						SetWindowDirty(w);
					}
					break;
			}
			break;

		case WE_DROPDOWN_SELECT: // we have selected a dropdown item in the list
			if (sl->sort_type != e->we.dropdown.index) {
				/* value has changed -> resort */
				sl->sort_type = e->we.dropdown.index;
				station_sort.criteria = sl->sort_type;
				sl->flags |= SL_RESORT;
			}
			SetWindowDirty(w);
			break;

		case WE_TICK:
			if (_pause_game != 0) break;
			if (--sl->resort_timer == 0) {
				DEBUG(misc, 3, "Periodic rebuild station list player %d", owner);
				sl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
				sl->flags |= SL_REBUILD;
				SetWindowDirty(w);
			}
			break;

		case WE_TIMEOUT:
			w->RaiseWidget(SLW_SORTBY);
			SetWindowDirty(w);
			break;

		case WE_RESIZE:
			w->vscroll.cap += e->we.sizing.diff.y / 10;
			break;
	}
}

static const Widget _player_stations_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,          STR_018B_CLOSE_WINDOW},            // SLW_CLOSEBOX
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   345,     0,    13, STR_3048_STATIONS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   346,   357,     0,    13, 0x0,               STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_RB,    14,     0,   345,    37,   161, 0x0,               STR_3057_STATION_NAMES_CLICK_ON},  // SLW_LIST
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   346,   357,    37,   149, 0x0,               STR_0190_SCROLL_BAR_SCROLLS_LIST},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   346,   357,   150,   161, 0x0,               STR_RESIZE_BUTTON},

{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    13,    14,    24, STR_TRAIN,         STR_USE_CTRL_TO_SELECT_MORE},      // SLW_TRAIN
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    14,    27,    14,    24, STR_LORRY,         STR_USE_CTRL_TO_SELECT_MORE},      // SLW_TRUCK
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    28,    41,    14,    24, STR_BUS,           STR_USE_CTRL_TO_SELECT_MORE},      // SLW_BUS
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    42,    55,    14,    24, STR_PLANE,         STR_USE_CTRL_TO_SELECT_MORE},      // SLW_AIRPLANE
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    56,    69,    14,    24, STR_SHIP,          STR_USE_CTRL_TO_SELECT_MORE},      // SLW_SHIP
{      WWT_PANEL,   RESIZE_NONE,    14,    70,    83,    14,    24, 0x0,               STR_SELECT_ALL_FACILITIES},        // SLW_FACILALL

{      WWT_PANEL,   RESIZE_NONE,    14,    83,    88,    14,    24, 0x0,               STR_NULL},                         // SLW_PAN_BETWEEN
{      WWT_PANEL,   RESIZE_NONE,    14,    89,   102,    14,    24, 0x0,               STR_NO_WAITING_CARGO},             // SLW_NOCARGOWAITING
{      WWT_PANEL,   RESIZE_NONE,    14,   103,   116,    14,    24, 0x0,               STR_SELECT_ALL_TYPES},             // SLW_CARGOALL
{      WWT_PANEL,  RESIZE_RIGHT,    14,   117,   357,    14,    24, 0x0,               STR_NULL},                         // SLW_PAN_RIGHT

{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    80,    25,    36, STR_SORT_BY,       STR_SORT_ORDER_TIP},               // SLW_SORTBY
{   WWT_DROPDOWN,   RESIZE_NONE,    14,    81,   243,    25,    36, 0x0,               STR_SORT_CRITERIA_TIP},            // SLW_SORTDROPBTN
{      WWT_PANEL,  RESIZE_RIGHT,    14,   244,   357,    25,    36, 0x0,               STR_NULL},                         // SLW_PAN_SORT_RIGHT
{   WIDGETS_END},
};

static const WindowDesc _player_stations_desc = {
	WDP_AUTO, WDP_AUTO, 358, 162, 358, 162,
	WC_STATION_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_player_stations_widgets,
	PlayerStationsWndProc
};

/**
 * Opens window with list of player's stations
 *
 * @param player player whose stations' list show
 */
void ShowPlayerStations(PlayerID player)
{
	if (!IsValidPlayer(player)) return;

	Window *w = AllocateWindowDescFront(&_player_stations_desc, player);
	if (w == NULL) return;

	w->caption_color = (byte)w->window_number;
	w->vscroll.cap = 12;
	w->resize.step_height = 10;
	w->resize.height = w->height - 10 * 7; // minimum if 5 in the list

	/* Add cargo filter buttons */
	uint num_active = 0;
	for (CargoID c = 0; c < NUM_CARGO; c++) {
		if (GetCargo(c)->IsValid()) num_active++;
	}

	w->widget_count += num_active;
	w->widget = ReallocT(w->widget, w->widget_count + 1);
	w->widget[w->widget_count].type = WWT_LAST;

	uint i = 0;
	for (CargoID c = 0; c < NUM_CARGO; c++) {
		if (!GetCargo(c)->IsValid()) continue;

		Widget *wi = &w->widget[SLW_CARGOSTART + i];
		wi->type     = WWT_PANEL;
		wi->display_flags = RESIZE_NONE;
		wi->color    = 14;
		wi->left     = 89 + i * 14;
		wi->right    = wi->left + 13;
		wi->top      = 14;
		wi->bottom   = 24;
		wi->data     = 0;
		wi->tooltips = STR_USE_CTRL_TO_SELECT_MORE;

		if (HasBit(_cargo_filter, c)) w->LowerWidget(SLW_CARGOSTART + i);
		i++;
	}

	w->widget[SLW_NOCARGOWAITING].left += num_active * 14;
	w->widget[SLW_NOCARGOWAITING].right += num_active * 14;
	w->widget[SLW_CARGOALL].left += num_active * 14;
	w->widget[SLW_CARGOALL].right += num_active * 14;
	w->widget[SLW_PAN_RIGHT].left += num_active * 14;

	if (num_active > 15) {
		/* Resize and fix the minimum width, if necessary */
		ResizeWindow(w, (num_active - 15) * 14, 0);
		w->resize.width = w->width;
	}
}

static const Widget _station_view_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,          STR_018B_CLOSE_WINDOW},                // SVW_CLOSEBOX
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   236,     0,    13, STR_300A_0,        STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   237,   248,     0,    13, 0x0,               STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_RB,    14,     0,   236,    14,    65, 0x0,               STR_NULL},                             // SVW_WAITING
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   237,   248,    14,    65, 0x0,               STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,    RESIZE_RTB,    14,     0,   248,    66,    97, 0x0,               STR_NULL},                             // SVW_ACCEPTLIST / SVW_RATINGLIST
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,    59,    98,   109, STR_00E4_LOCATION, STR_3053_CENTER_MAIN_VIEW_ON_STATION}, // SVW_LOCATION
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,    60,   120,    98,   109, STR_3032_RATINGS,  STR_3054_SHOW_STATION_RATINGS},        // SVW_RATINGS / SVW_ACCEPTS
{ WWT_PUSHTXTBTN,    RESIZE_RTB,    14,   121,   180,    98,   109, STR_0130_RENAME,   STR_3055_CHANGE_NAME_OF_STATION},      // SVW_RENAME
{ WWT_PUSHTXTBTN,   RESIZE_LRTB,    14,   181,   194,    98,   109, STR_TRAIN,         STR_SCHEDULED_TRAINS_TIP },            // SVW_TRAINS
{ WWT_PUSHTXTBTN,   RESIZE_LRTB,    14,   195,   208,    98,   109, STR_LORRY,         STR_SCHEDULED_ROAD_VEHICLES_TIP },     // SVW_ROADVEHS
{ WWT_PUSHTXTBTN,   RESIZE_LRTB,    14,   209,   222,    98,   109, STR_PLANE,         STR_SCHEDULED_AIRCRAFT_TIP },          // SVW_PLANES
{ WWT_PUSHTXTBTN,   RESIZE_LRTB,    14,   223,   236,    98,   109, STR_SHIP,          STR_SCHEDULED_SHIPS_TIP },             // SVW_SHIPS
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   237,   248,    98,   109, 0x0,               STR_RESIZE_BUTTON},
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

struct stationview_d {
	uint32 cargo;                 ///< Bitmask of cargo types to expand
	uint16 cargo_rows[NUM_CARGO]; ///< Header row for each cargo type
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(stationview_d));

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
 * Redraws whole StationView window
 *
 * @param w pointer to window
 */
static void DrawStationViewWindow(Window *w)
{
	StationID station_id = w->window_number;
	const Station* st = GetStation(station_id);
	int x, y;     ///< coordinates used for printing waiting/accepted/rating of cargo
	int pos;      ///< = w->vscroll.pos
	StringID str;
	CargoDataList cargolist;
	uint32 transfers = 0;

	/* count types of cargos waiting in station */
	for (CargoID i = 0; i < NUM_CARGO; i++) {
		if (st->goods[i].cargo.Empty()) {
			WP(w, stationview_d).cargo_rows[i] = 0;
		} else {
			/* Add an entry for total amount of cargo of this type waiting. */
			cargolist.push_back(CargoData(i, INVALID_STATION, st->goods[i].cargo.Count()));

			/* Set the row for this cargo entry for the expand/hide button */
			WP(w, stationview_d).cargo_rows[i] = cargolist.size();

			/* Add an entry for each distinct cargo source. */
			const CargoList::List *packets = st->goods[i].cargo.Packets();
			for (CargoList::List::const_iterator it = packets->begin(); it != packets->end(); it++) {
				const CargoPacket *cp = *it;
				if (cp->source != station_id) {
					bool added = false;

					/* Enable the expand/hide button for this cargo type */
					SetBit(transfers, i);

					/* Don't add cargo lines if not expanded */
					if (!HasBit(WP(w, stationview_d).cargo, i)) break;

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
	SetVScrollCount(w, cargolist.size() + 1); // update scrollbar

	/* disable some buttons */
	w->SetWidgetDisabledState(SVW_RENAME,   st->owner != _local_player);
	w->SetWidgetDisabledState(SVW_TRAINS,   !(st->facilities & FACIL_TRAIN));
	w->SetWidgetDisabledState(SVW_ROADVEHS, !(st->facilities & FACIL_TRUCK_STOP) && !(st->facilities & FACIL_BUS_STOP));
	w->SetWidgetDisabledState(SVW_PLANES,   !(st->facilities & FACIL_AIRPORT));
	w->SetWidgetDisabledState(SVW_SHIPS,    !(st->facilities & FACIL_DOCK));

	SetDParam(0, st->index);
	SetDParam(1, st->facilities);
	DrawWindowWidgets(w);

	x = 2;
	y = 15;
	pos = w->vscroll.pos;

	uint width = w->widget[SVW_WAITING].right - w->widget[SVW_WAITING].left - 4;
	int maxrows = w->vscroll.cap;

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
					const char *sym = HasBit(WP(w, stationview_d).cargo, cd->cargo) ? "-" : "+";
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

	if (w->widget[SVW_ACCEPTS].data == STR_3032_RATINGS) { // small window with list of accepted cargo
		char *b = _userstring;
		bool first = true;

		b = InlineString(b, STR_000C_ACCEPTS);

		for (CargoID i = 0; i < NUM_CARGO; i++) {
			if (b >= lastof(_userstring) - (1 + 2 * 4)) break; // ',' or ' ' and two calls to Utf8Encode()
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
		assert(b < endof(_userstring));

		DrawStringMultiLine(2, w->widget[SVW_ACCEPTLIST].top + 1, STR_SPEC_USERSTRING, w->widget[SVW_ACCEPTLIST].right - w->widget[SVW_ACCEPTLIST].left);
	} else { // extended window with list of cargo ratings
		y = w->widget[SVW_RATINGLIST].top + 1;

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

static void HandleCargoWaitingClick(Window *w, int row)
{
	if (row == 0) return;

	for (CargoID c = 0; c < NUM_CARGO; c++) {
		if (WP(w, stationview_d).cargo_rows[c] == row) {
			ToggleBit(WP(w, stationview_d).cargo, c);
			w->InvalidateWidget(SVW_WAITING);
			break;
		}
	}
}


/**
 * Fuction called when any WindowEvent occurs for any StationView window
 *
 * @param w pointer to the StationView window
 * @param e pointer to window event
 */
static void StationViewWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT:
			DrawStationViewWindow(w);
			break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case SVW_WAITING:
					HandleCargoWaitingClick(w, (e->we.click.pt.y - w->widget[SVW_WAITING].top) / 10 + w->vscroll.pos);
					break;

				case SVW_LOCATION:
					ScrollMainWindowToTile(GetStation(w->window_number)->xy);
					break;

				case SVW_RATINGS:
					SetWindowDirty(w);

					if (w->widget[SVW_RATINGS].data == STR_3032_RATINGS) {
						/* Switch to ratings view */
						w->widget[SVW_RATINGS].data = STR_3033_ACCEPTS;
						w->widget[SVW_RATINGS].tooltips = STR_3056_SHOW_LIST_OF_ACCEPTED_CARGO;
						ResizeWindowForWidget(w, SVW_ACCEPTLIST, 0, 100);
					} else {
						/* Switch to accepts view */
						w->widget[SVW_RATINGS].data = STR_3032_RATINGS;
						w->widget[SVW_RATINGS].tooltips = STR_3054_SHOW_STATION_RATINGS;
						ResizeWindowForWidget(w, SVW_ACCEPTLIST, 0, -100);
					}

					SetWindowDirty(w);
					break;

				case SVW_RENAME:
					SetDParam(0, w->window_number);
					ShowQueryString(STR_STATION, STR_3030_RENAME_STATION_LOADING, 31, 180, w, CS_ALPHANUMERAL);
					break;

				case SVW_TRAINS: { // Show a list of scheduled trains to this station
					const Station *st = GetStation(w->window_number);
					ShowVehicleListWindow(st->owner, VEH_TRAIN, (StationID)w->window_number);
					break;
				}

				case SVW_ROADVEHS: { // Show a list of scheduled road-vehicles to this station
					const Station *st = GetStation(w->window_number);
					ShowVehicleListWindow(st->owner, VEH_ROAD, (StationID)w->window_number);
					break;
				}

				case SVW_PLANES: { // Show a list of scheduled aircraft to this station
					const Station *st = GetStation(w->window_number);
					/* Since oilrigs have no owners, show the scheduled aircraft of current player */
					PlayerID owner = (st->owner == OWNER_NONE) ? _current_player : st->owner;
					ShowVehicleListWindow(owner, VEH_AIRCRAFT, (StationID)w->window_number);
					break;
				}

				case SVW_SHIPS: { // Show a list of scheduled ships to this station
					const Station *st = GetStation(w->window_number);
					/* Since oilrigs/bouys have no owners, show the scheduled ships of current player */
					PlayerID owner = (st->owner == OWNER_NONE) ? _current_player : st->owner;
					ShowVehicleListWindow(owner, VEH_SHIP, (StationID)w->window_number);
					break;
				}
			}
			break;

		case WE_ON_EDIT_TEXT:
			if (e->we.edittext.str[0] != '\0') {
				_cmd_text = e->we.edittext.str;
				DoCommandP(0, w->window_number, 0, NULL,
					CMD_RENAME_STATION | CMD_MSG(STR_3031_CAN_T_RENAME_STATION));
			}
			break;

		case WE_DESTROY: {
			WindowNumber wno =
				(w->window_number << 16) | VLW_STATION_LIST | GetStation(w->window_number)->owner;

			DeleteWindowById(WC_TRAINS_LIST, wno);
			DeleteWindowById(WC_ROADVEH_LIST, wno);
			DeleteWindowById(WC_SHIPS_LIST, wno);
			DeleteWindowById(WC_AIRCRAFT_LIST, wno);
			break;
		}

		case WE_RESIZE:
			if (e->we.sizing.diff.x != 0) ResizeButtons(w, SVW_LOCATION, SVW_RENAME);
			w->vscroll.cap += e->we.sizing.diff.y / (int)w->resize.step_height;
			break;
	}
}


static const WindowDesc _station_view_desc = {
	WDP_AUTO, WDP_AUTO, 249, 110, 249, 110,
	WC_STATION_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_station_view_widgets,
	StationViewWndProc
};

/**
 * Opens StationViewWindow for given station
 *
 * @param station station which window should be opened
 */
void ShowStationViewWindow(StationID station)
{
	Window *w = AllocateWindowDescFront(&_station_view_desc, station);
	if (w == NULL) return;

	PlayerID owner = GetStation(w->window_number)->owner;
	if (owner != OWNER_NONE) w->caption_color = owner;
	w->vscroll.cap = 5;
	w->resize.step_height = 10;
}
