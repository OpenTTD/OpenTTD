/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "strings.h"
#include "table/strings.h"
#include "window.h"
#include "gui.h"
#include "station.h"
#include "gfx.h"
#include "player.h"
#include "economy.h"
#include "town.h"
#include "command.h"
#include "variables.h"
#include "vehicle_gui.h"
#include "date.h"
#include "vehicle.h"
#include "table/sprites.h"
#include "helpers.hpp"
#include "cargotype.h"

enum StationListWidgets {
	STATIONLIST_WIDGET_CLOSEBOX = 0,
	STATIONLIST_WIDGET_LIST = 3,
	STATIONLIST_WIDGET_TRAIN =6,
	STATIONLIST_WIDGET_TRUCK,
	STATIONLIST_WIDGET_BUS,
	STATIONLIST_WIDGET_AIRPLANE,
	STATIONLIST_WIDGET_SHIP,
	STATIONLIST_WIDGET_CARGOSTART = 12,
	STATIONLIST_WIDGET_NOCARGOWAITING = 24,
	STATIONLIST_WIDGET_FACILALL = 26,
	STATIONLIST_WIDGET_CARGOALL,
	STATIONLIST_WIDGET_SORTBY,
	STATIONLIST_WIDGET_SORTCRITERIA,
	STATIONLIST_WIDGET_SORTDROPBTN,
};

typedef int CDECL StationSortListingTypeFunction(const void*, const void*);

static StationSortListingTypeFunction StationNameSorter;
static StationSortListingTypeFunction StationTypeSorter;
static StationSortListingTypeFunction StationWaitingSorter;
static StationSortListingTypeFunction StationRatingMaxSorter;

/** Draw small boxes of cargo amount and ratings data at the given
 * coordinates. If amount exceeds 576 units, it is shown 'full', same
 * goes for the rating: at above 90% orso (224) it is also 'full'
 * Each cargo-bar is 16 pixels wide and 6 pixels high
 * Each rating 14 pixels wide and 1 pixel high and is 1 pixel below the cargo-bar
 * @param x,y X/Y coordinate to draw the box at
 * @param type Cargo type
 * @param amount Cargo amount
 * @param rating ratings data for that particular cargo */
static void StationsWndShowStationRating(int x, int y, CargoID type, uint amount, byte rating)
{
	const CargoSpec *cs = GetCargo(type);
	if (!cs->IsValid()) return;

	int colour = cs->rating_colour;
	uint w = (minu(amount, 576) + 5) / 36;

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

	DrawString(x + 1, y, cs->abbrev, 0x10);

	/* Draw green/red ratings bar (fits into 14 pixels) */
	y += 8;
	GfxFillRect(x + 1, y, x + 14, y, 0xB8);
	rating = minu(rating,  224) / 16;
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

static int CDECL StationWaitingSorter(const void *a, const void *b)
{
	const Station* st1 = *(const Station**)a;
	const Station* st2 = *(const Station**)b;
	int sum1 = 0, sum2 = 0;

	for (CargoID j = 0; j < NUM_CARGO; j++) {
		if (st1->goods[j].waiting_acceptance & 0xfff) sum1 += GetTransportedGoodsIncome(st1->goods[j].waiting_acceptance & 0xfff, 20, 50, j);
		if (st2->goods[j].waiting_acceptance & 0xfff) sum2 += GetTransportedGoodsIncome(st2->goods[j].waiting_acceptance & 0xfff, 20, 50, j);
	}

	return (_internal_sort_order & 1) ? sum2 - sum1 : sum1 - sum2;
}

static int CDECL StationRatingMaxSorter(const void *a, const void *b)
{
	const Station* st1 = *(const Station**)a;
	const Station* st2 = *(const Station**)b;
	byte maxr1 = 0;
	byte maxr2 = 0;

	for (CargoID j = 0; j < NUM_CARGO; j++) {
		if (st1->goods[j].waiting_acceptance & 0xfff) maxr1 = max(maxr1, st1->goods[j].rating);
		if (st2->goods[j].waiting_acceptance & 0xfff) maxr2 = max(maxr2, st2->goods[j].rating);
	}

	return (_internal_sort_order & 1) ? maxr2 - maxr1 : maxr1 - maxr2;
}

enum StationListFlags {
	SL_ORDER   = 0x01,
	SL_RESORT  = 0x02,
	SL_REBUILD = 0x04,
};

DECLARE_ENUM_AS_BIT_SET(StationListFlags);

struct plstations_d {
	const Station** sort_list;
	uint16 list_length;
	byte sort_type;
	StationListFlags flags;
	uint16 resort_timer;  //was byte refresh_counter;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(plstations_d));

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

static void BuildStationsList(plstations_d* sl, PlayerID owner, byte facilities, uint16 cargo_filter)
{
	uint n = 0;
	const Station *st;

	if (!(sl->flags & SL_REBUILD)) return;

	/* Create array for sorting */
	const Station** station_sort = MallocT<const Station*>(GetMaxStationIndex() + 1);
	if (station_sort == NULL) error("Could not allocate memory for the station-sorting-list");

	DEBUG(misc, 3, "Building station list for player %d", owner);

	FOR_ALL_STATIONS(st) {
		if (st->owner == owner) {
			if (facilities & st->facilities) { //only stations with selected facilities
				int num_waiting_cargo = 0;
				for (CargoID j = 0; j < NUM_CARGO; j++) {
					if (st->goods[j].waiting_acceptance & 0xFFF) {
						num_waiting_cargo++; //count number of waiting cargo
						if (HASBIT(cargo_filter, j)) {
							station_sort[n++] = st;
							break;
						}
					}
				}
				//stations without waiting cargo
				if (num_waiting_cargo == 0 && HASBIT(cargo_filter, NUM_CARGO)) {
					station_sort[n++] = st;
				}
			}
		}
	}

	free((void*)sl->sort_list);
	sl->sort_list = MallocT<const Station*>(n);
	if (n != 0 && sl->sort_list == NULL) error("Could not allocate memory for the station-sorting-list");
	sl->list_length = n;

	for (uint i = 0; i < n; ++i) sl->sort_list[i] = station_sort[i];

	sl->flags &= ~SL_REBUILD;
	sl->flags |= SL_RESORT;
	free((void*)station_sort);
}

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

static void PlayerStationsWndProc(Window *w, WindowEvent *e)
{
	static const uint16 CARGO_ALL_SELECTED = 0x1FFF;

	const PlayerID owner = (PlayerID)w->window_number;
	static byte facilities = FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK;
	static uint16 cargo_filter = CARGO_ALL_SELECTED;
	static Listing station_sort = {0, 0};

	plstations_d *sl = &WP(w, plstations_d);

	switch (e->event) {
		case WE_CREATE: /* set up resort timer */
			for (uint i = 0; i < 5; i++) {
				if (HASBIT(facilities, i)) LowerWindowWidget(w, i + STATIONLIST_WIDGET_TRAIN);
			}
			for (CargoID i = 0; i < NUM_CARGO; i++) {
				if (HASBIT(cargo_filter, i)) LowerWindowWidget(w, i + STATIONLIST_WIDGET_CARGOSTART);
			}
			SetWindowWidgetLoweredState(w, STATIONLIST_WIDGET_FACILALL, facilities == (FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK));
			SetWindowWidgetLoweredState(w, STATIONLIST_WIDGET_CARGOALL, cargo_filter == CARGO_ALL_SELECTED);
			SetWindowWidgetLoweredState(w, STATIONLIST_WIDGET_NOCARGOWAITING, HASBIT(cargo_filter, STATIONLIST_WIDGET_NOCARGOWAITING - NUM_CARGO));

			sl->sort_list = NULL;
			sl->flags = SL_REBUILD;
			sl->sort_type = station_sort.criteria;
			if (station_sort.order) sl->flags |= SL_ORDER;
			sl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
			break;

		case WE_PAINT: {
			BuildStationsList(sl, owner, facilities, cargo_filter);
			SortStationsList(sl);

			SetVScrollCount(w, sl->list_length);

			/* draw widgets, with player's name in the caption */
			const Player* p = GetPlayer(owner);
			SetDParam(0, p->name_1);
			SetDParam(1, p->name_2);
			SetDParam(2, w->vscroll.count);
			DrawWindowWidgets(w);

			/* draw sorting criteria string */
			DrawString(85, 26, _station_sort_listing[sl->sort_type], 0x10);
			/* draw arrow pointing up/down for ascending/descending sorting */
			DoDrawString(sl->flags & SL_ORDER ? DOWNARROW : UPARROW, 69, 26, 0x10);

			int cg_ofst;
			int x = 89;
			int y = 14;
			int xb = 2; // offset from left of widget

			for (CargoID i = 0; i < NUM_CARGO; i++) {
				const CargoSpec *cs = GetCargo(i);
				if (cs->IsValid()) {
					cg_ofst = IsWindowWidgetLowered(w, i + STATIONLIST_WIDGET_CARGOSTART) ? 2 : 1;
					GfxFillRect(x + cg_ofst, y + cg_ofst, x + cg_ofst + 10 , y + cg_ofst + 7, cs->rating_colour);
					DrawStringCentered(x + 6 + cg_ofst, y + cg_ofst, cs->abbrev, 0x10);
				}
				x += 14;
			}

			x += 6;
			cg_ofst = IsWindowWidgetLowered(w, STATIONLIST_WIDGET_NOCARGOWAITING) ? 2 : 1;
			DrawStringCentered(x + cg_ofst, y + cg_ofst, STR_ABBREV_NONE, 16);
			x += 14;
			cg_ofst = IsWindowWidgetLowered(w, STATIONLIST_WIDGET_CARGOALL) ? 2 : 1;
			DrawStringCentered(x + cg_ofst, y + cg_ofst, STR_ABBREV_ALL, 16);

			cg_ofst = IsWindowWidgetLowered(w, STATIONLIST_WIDGET_FACILALL) ? 2 : 1;
			DrawString(71 + cg_ofst, y + cg_ofst, STR_ABBREV_ALL, 16);

			if (w->vscroll.count == 0) { // player has no stations
				DrawString(xb, 40, STR_304A_NONE, 0);
				return;
			}

			int max = min(w->vscroll.pos + w->vscroll.cap, sl->list_length);
			y = 40; // start of the list-widget

			for (int i = w->vscroll.pos; i < max; ++i) { // do until max number of stations of owner
				const Station *st = sl->sort_list[i];
				int x;

				assert(st->xy != 0);
				assert(st->owner == owner);

				SetDParam(0, st->index);
				SetDParam(1, st->facilities);
				x = DrawString(xb, y, STR_3049_0, 0) + 5;

				// show cargo waiting and station ratings
				for (CargoID j = 0; j != NUM_CARGO; j++) {
					uint amount = GB(st->goods[j].waiting_acceptance, 0, 12);
					if (amount != 0) {
						StationsWndShowStationRating(x, y, j, amount, st->goods[j].rating);
						x += 20;
					}
				}
				y += 10;
			}
			break;
		}

		case WE_CLICK:
			switch (e->we.click.widget) {
				case STATIONLIST_WIDGET_LIST: {
					uint32 id_v = (e->we.click.pt.y - 41) / 10;

					if (id_v >= w->vscroll.cap) return; // click out of bounds

					id_v += w->vscroll.pos;

					if (id_v >= sl->list_length) return; // click out of list bound

					const Station *st = sl->sort_list[id_v];
					assert(st->owner == owner);
					ScrollMainWindowToTile(st->xy);
					break;
				}

				case STATIONLIST_WIDGET_TRAIN:
				case STATIONLIST_WIDGET_TRUCK:
				case STATIONLIST_WIDGET_BUS:
				case STATIONLIST_WIDGET_AIRPLANE:
				case STATIONLIST_WIDGET_SHIP:
					if (_ctrl_pressed) {
						TOGGLEBIT(facilities, e->we.click.widget - STATIONLIST_WIDGET_TRAIN);
						ToggleWidgetLoweredState(w, e->we.click.widget);
					} else {
						for (uint i = 0; facilities != 0; i++, facilities >>= 1) {
							if (HASBIT(facilities, 0)) RaiseWindowWidget(w, i + STATIONLIST_WIDGET_TRAIN);
						}
						SETBIT(facilities, e->we.click.widget - STATIONLIST_WIDGET_TRAIN);
						LowerWindowWidget(w, e->we.click.widget);
					}
					SetWindowWidgetLoweredState(w, STATIONLIST_WIDGET_FACILALL, facilities == (FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK));
					sl->flags |= SL_REBUILD;
					SetWindowDirty(w);
					break;

				case STATIONLIST_WIDGET_FACILALL:
					for (uint i = 0; i < 5; i++) {
						LowerWindowWidget(w, i + STATIONLIST_WIDGET_TRAIN);
					}
					LowerWindowWidget(w, STATIONLIST_WIDGET_FACILALL);

					facilities = FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK;
					sl->flags |= SL_REBUILD;
					SetWindowDirty(w);
					break;

				case STATIONLIST_WIDGET_CARGOALL:
					for (CargoID i = 0; i < NUM_CARGO; i++) {
						LowerWindowWidget(w, i + STATIONLIST_WIDGET_CARGOSTART);
					}
					LowerWindowWidget(w, STATIONLIST_WIDGET_NOCARGOWAITING);
					LowerWindowWidget(w, STATIONLIST_WIDGET_CARGOALL);

					cargo_filter = CARGO_ALL_SELECTED;
					sl->flags |= SL_REBUILD;
					SetWindowDirty(w);
					break;

				case STATIONLIST_WIDGET_SORTBY: /*flip sorting method asc/desc*/
					sl->flags ^= SL_ORDER; //DESC-flag
					station_sort.order = GB(sl->flags, 0, 1);
					sl->flags |= SL_RESORT;
					w->flags4 |= 5 << WF_TIMEOUT_SHL;
					LowerWindowWidget(w, STATIONLIST_WIDGET_SORTBY);
					SetWindowDirty(w);
					break;

				case STATIONLIST_WIDGET_SORTCRITERIA:
				case STATIONLIST_WIDGET_SORTDROPBTN: /* select sorting criteria dropdown menu */
					ShowDropDownMenu(w, _station_sort_listing, sl->sort_type, STATIONLIST_WIDGET_SORTDROPBTN, 0, 0);
					break;

				default:
					if (e->we.click.widget >= STATIONLIST_WIDGET_CARGOSTART && e->we.click.widget <= STATIONLIST_WIDGET_NOCARGOWAITING) { //change cargo_filter
						if (_ctrl_pressed) {
							TOGGLEBIT(cargo_filter, e->we.click.widget - STATIONLIST_WIDGET_CARGOSTART);
							ToggleWidgetLoweredState(w, e->we.click.widget);
						} else {
							for (uint i = 0; cargo_filter != 0; i++, cargo_filter >>= 1) {
								if (HASBIT(cargo_filter, 0)) RaiseWindowWidget(w, i + STATIONLIST_WIDGET_CARGOSTART);
							}
							SETBIT(cargo_filter, e->we.click.widget - STATIONLIST_WIDGET_CARGOSTART);
							LowerWindowWidget(w, e->we.click.widget);
						}
						sl->flags |= SL_REBUILD;
						SetWindowWidgetLoweredState(w, STATIONLIST_WIDGET_CARGOALL, cargo_filter == CARGO_ALL_SELECTED);
						SetWindowDirty(w);
					}
					break;
			}
			break;

		case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
			if (sl->sort_type != e->we.dropdown.index) {
				// value has changed -> resort
				sl->sort_type = e->we.dropdown.index;
				station_sort.criteria = sl->sort_type;
				sl->flags |= SL_RESORT;
			}
			SetWindowDirty(w);
			break;

		case WE_TICK:
			if (--sl->resort_timer == 0) {
				DEBUG(misc, 3, "Periodic rebuild station list player %d", owner);
				sl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
				sl->flags |= SL_REBUILD;
				SetWindowDirty(w);
			}
			break;

		case WE_TIMEOUT:
			RaiseWindowWidget(w, STATIONLIST_WIDGET_SORTBY);
			SetWindowDirty(w);
			break;

		case WE_RESIZE:
			w->vscroll.cap += e->we.sizing.diff.y / 10;
			break;
	}
}

static const Widget _player_stations_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   345,     0,    13, STR_3048_STATIONS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   346,   357,     0,    13, 0x0,               STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_RB,    14,     0,   345,    37,   161, 0x0,               STR_3057_STATION_NAMES_CLICK_ON},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   346,   357,    37,   149, 0x0,               STR_0190_SCROLL_BAR_SCROLLS_LIST},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   346,   357,   150,   161, 0x0,               STR_RESIZE_BUTTON},
//Index 6
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    13,    14,    24, STR_TRAIN,         STR_USE_CTRL_TO_SELECT_MORE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    14,    27,    14,    24, STR_LORRY,         STR_USE_CTRL_TO_SELECT_MORE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    28,    41,    14,    24, STR_BUS,           STR_USE_CTRL_TO_SELECT_MORE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    42,    55,    14,    24, STR_PLANE,         STR_USE_CTRL_TO_SELECT_MORE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    56,    69,    14,    24, STR_SHIP,          STR_USE_CTRL_TO_SELECT_MORE},
//Index 11
{      WWT_PANEL,   RESIZE_NONE,    14,    83,    88,    14,    24, 0x0,               STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,    89,   102,    14,    24, 0x0,               STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   103,   116,    14,    24, 0x0,               STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   117,   130,    14,    24, 0x0,               STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   131,   144,    14,    24, 0x0,               STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   145,   158,    14,    24, 0x0,               STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   159,   172,    14,    24, 0x0,               STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   173,   186,    14,    24, 0x0,               STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   187,   200,    14,    24, 0x0,               STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   201,   214,    14,    24, 0x0,               STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   215,   228,    14,    24, 0x0,               STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   229,   242,    14,    24, 0x0,               STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   243,   256,    14,    24, 0x0,               STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   257,   270,    14,    24, 0x0,               STR_NO_WAITING_CARGO},
{      WWT_PANEL,  RESIZE_RIGHT,    14,   285,   357,    14,    24, 0x0,               STR_NULL},

//26
{      WWT_PANEL,   RESIZE_NONE,    14,    70,    83,    14,    24, 0x0,               STR_SELECT_ALL_FACILITIES},
{      WWT_PANEL,   RESIZE_NONE,    14,   271,   284,    14,    24, 0x0,               STR_SELECT_ALL_TYPES},

//28
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    80,    25,    36, STR_SORT_BY,       STR_SORT_ORDER_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,    81,   232,    25,    36, 0x0,               STR_SORT_CRITERIA_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   233,   243,    25,    36, STR_0225,          STR_SORT_CRITERIA_TIP},
{      WWT_PANEL,  RESIZE_RIGHT,    14,   244,   357,    25,    36, 0x0,               STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _player_stations_desc = {
	WDP_AUTO, WDP_AUTO, 358, 162,
	WC_STATION_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_player_stations_widgets,
	PlayerStationsWndProc
};


void ShowPlayerStations(PlayerID player)
{
	if (!IsValidPlayer(player)) return;

	Window *w = AllocateWindowDescFront(&_player_stations_desc, player);
	if (w == NULL) return;

	w->caption_color = (byte)w->window_number;
	w->vscroll.cap = 12;
	w->resize.step_height = 10;
	w->resize.height = w->height - 10 * 7; // minimum if 5 in the list
}

static const Widget _station_view_expanded_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   236,     0,    13, STR_300A_0,        STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   237,   248,     0,    13, 0x0,               STR_STICKY_BUTTON},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   236,    14,    65, 0x0,               STR_NULL},
{  WWT_SCROLLBAR,   RESIZE_NONE,    14,   237,   248,    14,    65, 0x0,               STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,               STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   248,    66,   197, 0x0,               STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    63,   198,   209, STR_00E4_LOCATION, STR_3053_CENTER_MAIN_VIEW_ON_STATION},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    64,   128,   198,   209, STR_3033_ACCEPTS,  STR_3056_SHOW_LIST_OF_ACCEPTED_CARGO},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   129,   192,   198,   209, STR_0130_RENAME,   STR_3055_CHANGE_NAME_OF_STATION},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   193,   206,   198,   209, STR_TRAIN,         STR_SCHEDULED_TRAINS_TIP },
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   207,   220,   198,   209, STR_LORRY,         STR_SCHEDULED_ROAD_VEHICLES_TIP },
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   221,   234,   198,   209, STR_PLANE,         STR_SCHEDULED_AIRCRAFT_TIP },
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   235,   248,   198,   209, STR_SHIP,          STR_SCHEDULED_SHIPS_TIP },
{   WIDGETS_END},
};

static const Widget _station_view_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   236,     0,    13, STR_300A_0,        STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   237,   248,     0,    13, 0x0,               STR_STICKY_BUTTON},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   236,    14,    65, 0x0,               STR_NULL},
{  WWT_SCROLLBAR,   RESIZE_NONE,    14,   237,   248,    14,    65, 0x0,               STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   248,    66,    97, 0x0,               STR_NULL},
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,               STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    63,    98,   109, STR_00E4_LOCATION, STR_3053_CENTER_MAIN_VIEW_ON_STATION},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    64,   128,    98,   109, STR_3032_RATINGS,  STR_3054_SHOW_STATION_RATINGS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   129,   192,    98,   109, STR_0130_RENAME,   STR_3055_CHANGE_NAME_OF_STATION},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   193,   206,    98,   109, STR_TRAIN,         STR_SCHEDULED_TRAINS_TIP },
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   207,   220,    98,   109, STR_LORRY,         STR_SCHEDULED_ROAD_VEHICLES_TIP },
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   221,   234,    98,   109, STR_PLANE,         STR_SCHEDULED_AIRCRAFT_TIP },
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   235,   248,    98,   109, STR_SHIP,          STR_SCHEDULED_SHIPS_TIP },
{   WIDGETS_END},
};

static void DrawStationViewWindow(Window *w)
{
	StationID station_id = w->window_number;
	const Station* st = GetStation(station_id);
	uint num;
	int x,y;
	int pos;
	StringID str;

	num = 1;
	for (CargoID i = 0; i != NUM_CARGO; i++) {
		if (GB(st->goods[i].waiting_acceptance, 0, 12) != 0) {
			num++;
			if (st->goods[i].enroute_from != station_id) num++;
		}
	}
	SetVScrollCount(w, num);

	SetWindowWidgetDisabledState(w,  9, st->owner != _local_player);
	SetWindowWidgetDisabledState(w, 10, !(st->facilities & FACIL_TRAIN));
	SetWindowWidgetDisabledState(w, 11, !(st->facilities & FACIL_TRUCK_STOP) && !(st->facilities & FACIL_BUS_STOP));
	SetWindowWidgetDisabledState(w, 12, !(st->facilities & FACIL_AIRPORT));
	SetWindowWidgetDisabledState(w, 13, !(st->facilities & FACIL_DOCK));

	SetDParam(0, st->index);
	SetDParam(1, st->facilities);
	DrawWindowWidgets(w);

	x = 2;
	y = 15;
	pos = w->vscroll.pos;

	if (--pos < 0) {
		str = STR_00D0_NOTHING;
		for (CargoID i = 0; i != NUM_CARGO; i++) {
			if (GB(st->goods[i].waiting_acceptance, 0, 12) != 0) str = STR_EMPTY;
		}
		SetDParam(0, str);
		DrawString(x, y, STR_0008_WAITING, 0);
		y += 10;
	}

	CargoID i = 0;
	do {
		uint waiting = GB(st->goods[i].waiting_acceptance, 0, 12);
		if (waiting == 0) continue;

		num = (waiting + 5) / 10;
		if (num != 0) {
			int cur_x = x;
			num = min(num, 23);
			do {
				DrawSprite(GetCargo(i)->sprite, PAL_NONE, cur_x, y);
				cur_x += 10;
			} while (--num);
		}

		if (st->goods[i].enroute_from == station_id) {
			if (--pos < 0) {
				SetDParam(1, waiting);
				SetDParam(0, i);
				DrawStringRightAligned(x + 234, y, STR_0009, 0);
				y += 10;
			}
		} else {
			/* enroute */
			if (--pos < 0) {
				SetDParam(1, waiting);
				SetDParam(0, i);
				DrawStringRightAligned(x + 234, y, STR_000A_EN_ROUTE_FROM, 0);
				y += 10;
			}

			if (pos > -5 && --pos < 0) {
				SetDParam(0, st->goods[i].enroute_from);
				DrawStringRightAligned(x + 234, y, STR_000B, 0);
				y += 10;
			}
		}
	} while (pos > -5 && ++i != NUM_CARGO);

	if (IsWindowOfPrototype(w, _station_view_widgets)) {
		char *b = _userstring;
		bool first = true;

		b = InlineString(b, STR_000C_ACCEPTS);

		for (CargoID i = 0; i != NUM_CARGO; i++) {
			if (b >= endof(_userstring) - 5 - 1) break;
			if (st->goods[i].waiting_acceptance & 0x8000) {
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
		DrawStringMultiLine(2, 67, STR_SPEC_USERSTRING, 245);
	} else {
		DrawString(2, 67, STR_3034_LOCAL_RATING_OF_TRANSPORT, 0);

		y = 77;
		for (CargoID i = 0; i != NUM_CARGO; i++) {
			if (st->goods[i].enroute_from != INVALID_STATION) {
				SetDParam(0, GetCargo(i)->name);
				SetDParam(2, st->goods[i].rating * 101 >> 8);
				SetDParam(1, STR_3035_APPALLING + (st->goods[i].rating >> 5));
				DrawString(8, y, STR_303D, 0);
				y += 10;
			}
		}
	}
}


static void StationViewWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT:
			DrawStationViewWindow(w);
			break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case 7:
					ScrollMainWindowToTile(GetStation(w->window_number)->xy);
					break;

				case 8:
					SetWindowDirty(w);

					/* toggle height/widget set */
					if (IsWindowOfPrototype(w, _station_view_expanded_widgets)) {
						AssignWidgetToWindow(w, _station_view_widgets);
						w->height = 110;
					} else {
						AssignWidgetToWindow(w, _station_view_expanded_widgets);
						w->height = 210;
					}

					SetWindowDirty(w);
					break;

				case 9:
					SetDParam(0, w->window_number);
					ShowQueryString(STR_STATION, STR_3030_RENAME_STATION_LOADING, 31, 180, w, CS_ALPHANUMERAL);
					break;

				case 10: { /* Show a list of scheduled trains to this station */
					const Station *st = GetStation(w->window_number);
					ShowVehicleListWindow(st->owner, VEH_Train, (StationID)w->window_number);
					break;
				}

				case 11: { /* Show a list of scheduled road-vehicles to this station */
					const Station *st = GetStation(w->window_number);
					ShowVehicleListWindow(st->owner, VEH_Road, (StationID)w->window_number);
					break;
				}

				case 12: { /* Show a list of scheduled aircraft to this station */
					const Station *st = GetStation(w->window_number);
					/* Since oilrigs have no owners, show the scheduled aircraft of current player */
					PlayerID owner = (st->owner == OWNER_NONE) ? _current_player : st->owner;
					ShowVehicleListWindow(owner, VEH_Aircraft, (StationID)w->window_number);
					break;
				}

				case 13: { /* Show a list of scheduled ships to this station */
					const Station *st = GetStation(w->window_number);
					/* Since oilrigs/bouys have no owners, show the scheduled ships of current player */
					PlayerID owner = (st->owner == OWNER_NONE) ? _current_player : st->owner;
					ShowVehicleListWindow(owner, VEH_Ship, (StationID)w->window_number);
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
				(w->window_number << 16) | GetStation(w->window_number)->owner;

			DeleteWindowById(WC_TRAINS_LIST, wno);
			DeleteWindowById(WC_ROADVEH_LIST, wno);
			DeleteWindowById(WC_SHIPS_LIST, wno);
			DeleteWindowById(WC_AIRCRAFT_LIST, wno);
			break;
		}
	}
}


static const WindowDesc _station_view_desc = {
	WDP_AUTO, WDP_AUTO, 249, 110,
	WC_STATION_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_station_view_widgets,
	StationViewWndProc
};

void ShowStationViewWindow(StationID station)
{
	Window *w = AllocateWindowDescFront(&_station_view_desc, station);
	if (w == NULL) return;

	PlayerID owner = GetStation(w->window_number)->owner;
	if (owner != OWNER_NONE) w->caption_color = owner;
	w->vscroll.cap = 5;
}
