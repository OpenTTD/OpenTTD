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

typedef int CDECL StationSortListingTypeFunction(const void*, const void*);

static StationSortListingTypeFunction StationNameSorter;
static StationSortListingTypeFunction StationTypeSorter;
static StationSortListingTypeFunction StationWaitingSorter;
static StationSortListingTypeFunction StationRatingMaxSorter;

static void StationsWndShowStationRating(int x, int y, int type, uint acceptance, int rating)
{
	static const byte _rating_colors[NUM_CARGO] = {152, 32, 15, 174, 208, 194, 191, 55, 184, 10, 191, 48};
	int color = _rating_colors[type];
	uint w;

	if (acceptance > 575) acceptance = 575;

	acceptance = (acceptance + 7) / 8;

	/* draw cargo */
	w = acceptance / 8;
	if (w != 0) {
		GfxFillRect(x, y, x + w - 1, y + 6, color);
		x += w;
	}

	w = acceptance % 8;
	if (w != 0) {
		if (w == 7) w--;
		GfxFillRect(x, y + (w - 1), x, y + 6, color);
	}

	x -= acceptance / 8;

	DrawString(x + 1, y, _cargoc.names_short[type], 0x10);

	/* draw green/red ratings bar */
	GfxFillRect(x + 1, y + 8, x + 7, y + 8, 0xB8);

	rating >>= 5;

	if (rating != 0) GfxFillRect(x + 1, y + 8, x + rating, y + 8, 0xD0);
}

const StringID _station_sort_listing[] = {
	STR_SORT_BY_DROPDOWN_NAME,
	STR_SORT_BY_FACILITY,
	STR_SORT_BY_WAITING,
	STR_SORT_BY_RATING_MAX,
	INVALID_STRING_ID
};

static char _bufcache[64];
static uint16 _last_station_idx;
static int _internal_sort_order;

static int CDECL StationNameSorter(const void *a, const void *b)
{
	char buf1[64];
	int32 argv[1];
	const SortStruct *cmp1 = (const SortStruct*)a;
	const SortStruct *cmp2 = (const SortStruct*)b;
	int r;

	argv[0] = cmp1->index;
	GetStringWithArgs(buf1, STR_STATION, argv);

	if (cmp2->index != _last_station_idx) {
		_last_station_idx = cmp2->index;
		argv[0] = cmp2->index;
		GetStringWithArgs(_bufcache, STR_STATION, argv);
	}

	r =  strcmp(buf1, _bufcache); // sort by name
	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL StationTypeSorter(const void *a, const void *b)
{
	const Station *st1 = GetStation(((const SortStruct*)a)->index);
	const Station *st2 = GetStation(((const SortStruct*)b)->index);
	return (_internal_sort_order & 1) ? st2->facilities - st1->facilities : st1->facilities - st2->facilities;
}

static int CDECL StationWaitingSorter(const void *a, const void *b)
{
	int sum1 = 0, sum2 = 0;
	int j;
	const Station *st1 = GetStation(((const SortStruct*)a)->index);
	const Station *st2 = GetStation(((const SortStruct*)b)->index);

	for (j = 0; j < NUM_CARGO; j++) {
		if (st1->goods[j].waiting_acceptance & 0xfff) sum1 += GetTransportedGoodsIncome(st1->goods[j].waiting_acceptance & 0xfff, 20, 50, j);
		if (st2->goods[j].waiting_acceptance & 0xfff) sum2 += GetTransportedGoodsIncome(st2->goods[j].waiting_acceptance & 0xfff, 20, 50, j);
	}

	return (_internal_sort_order & 1) ? sum2 - sum1 : sum1 - sum2;
}

static int CDECL StationRatingMaxSorter(const void *a, const void *b)
{
	byte maxr1 = 0;
	byte maxr2 = 0;
	int j;
	const Station *st1 = GetStation(((const SortStruct*)a)->index);
	const Station *st2 = GetStation(((const SortStruct*)b)->index);

	for (j = 0; j < NUM_CARGO; j++) {
		if (st1->goods[j].waiting_acceptance & 0xfff) maxr1 = max(maxr1, st1->goods[j].rating);
		if (st2->goods[j].waiting_acceptance & 0xfff) maxr2 = max(maxr2, st2->goods[j].rating);
	}

	return (_internal_sort_order & 1) ? maxr2 - maxr1 : maxr1 - maxr2;
}

typedef enum StationListFlags {
	SL_ORDER   = 0x01,
	SL_RESORT  = 0x02,
	SL_REBUILD = 0x04,
} StationListFlags;

typedef struct plstations_d {
	SortStruct *sort_list;
	uint16 list_length;
	byte sort_type;
	StationListFlags flags;
	uint16 resort_timer;  //was byte refresh_counter;
} plstations_d;
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(plstations_d));

void RebuildStationLists(void)
{
	Window *w;

	for (w = _windows; w != _last_window; ++w) {
		if (w->window_class == WC_STATION_LIST) {
			WP(w, plstations_d).flags |= SL_REBUILD;
			SetWindowDirty(w);
		}
	}
}

void ResortStationLists(void)
{
	Window *w;

	for (w = _windows; w != _last_window; ++w) {
		if (w->window_class == WC_STATION_LIST) {
			WP(w, plstations_d).flags |= SL_RESORT;
			SetWindowDirty(w);
		}
	}
}

static void BuildStationsList(plstations_d* sl, PlayerID owner, byte facilities, uint16 cargo_filter)
{
	uint n = 0;
	uint i, j;
	const Station *st;

	if (!(sl->flags & SL_REBUILD)) return;

	/* Create array for sorting */
	_station_sort = realloc(_station_sort, GetStationPoolSize() * sizeof(_station_sort[0]));
	if (_station_sort == NULL)
		error("Could not allocate memory for the station-sorting-list");

	DEBUG(misc, 1) ("Building station list for player %d...", owner);

	FOR_ALL_STATIONS(st) {
		if (st->xy && st->owner == owner) {
			if (facilities & st->facilities) { //only stations with selected facilities
				int num_waiting_cargo = 0;
				for (j = 0; j < NUM_CARGO; j++) {
					if (st->goods[j].waiting_acceptance & 0xFFF) {
						num_waiting_cargo++; //count number of waiting cargo
						if (HASBIT(cargo_filter, j)) {
							_station_sort[n].index = st->index;
							_station_sort[n].owner = st->owner;
							n++;
							break;
						}
					}
				}
				//stations without waiting cargo
				if (num_waiting_cargo == 0 && HASBIT(cargo_filter, NUM_CARGO)) {
					_station_sort[n].index = st->index;
					_station_sort[n].owner = st->owner;
					n++;
				}
			}
		}
	}

	free(sl->sort_list);
	sl->sort_list = malloc(n * sizeof(sl->sort_list[0]));
	if (n != 0 && sl->sort_list == NULL) error("Could not allocate memory for the station-sorting-list");
	sl->list_length = n;

	for (i = 0; i < n; ++i) sl->sort_list[i] = _station_sort[i];

	sl->flags &= ~SL_REBUILD;
	sl->flags |= SL_RESORT;
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
	_last_station_idx = 0; // used for "cache" in namesorting
	qsort(sl->sort_list, sl->list_length, sizeof(sl->sort_list[0]), _station_sorter[sl->sort_type]);

	sl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
	sl->flags &= ~SL_RESORT;
}

static void PlayerStationsWndProc(Window *w, WindowEvent *e)
{
	const PlayerID owner = w->window_number;
	static byte facilities = FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK;
	static uint16 cargo_filter = 0x1FFF;
	plstations_d *sl = &WP(w, plstations_d);

	switch (e->event) {
	case WE_PAINT: {
		/* Set up cargo click-states. Toggle the all-vehicle and all-cargo types button
		* depending on if all types are clicked or not */
		SB(w->click_state, 6, 5, facilities);
		SB(w->click_state, 26, 1, facilities == (FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK));
		SB(w->click_state, 12, NUM_CARGO + 1, cargo_filter);
		SB(w->click_state, 27, 1, cargo_filter == 0x1FFF);

		BuildStationsList(sl, owner, facilities, cargo_filter);
		SortStationsList(sl);

		SetVScrollCount(w, sl->list_length);

		/* draw widgets, with player's name in the caption */
		{
			const Player* p = GetPlayer(owner);
			SetDParam(0, p->name_1);
			SetDParam(1, p->name_2);
			SetDParam(2, w->vscroll.count);
			DrawWindowWidgets(w);
		}

		{
			int max;
			int i;
			int x = 0, y = 0, xb = 2; // offset from top of widget
			static const byte _cargo_legend_colors[NUM_CARGO] = {152, 32, 15, 174, 208, 194, 191, 84, 184, 10, 202, 215};

			/* draw sorting criteria string */
			DrawString(85, 26, _station_sort_listing[sl->sort_type], 0x10);
			/* draw arrow pointing up/down for ascending/descending sorting */
			DoDrawString(sl->flags & SL_ORDER ? DOWNARROW : UPARROW, 69, 26, 0x10);


			x = 90;
			y = 14;

			for (i = 0; i < NUM_CARGO; i++) {
				GfxFillRect(x + 1, y + 2, x + 11, y + 8, _cargo_legend_colors[i]);
				DrawString(x + 3, y + 2, _cargoc.names_short[i], i == 11 ? 15 : 16);
				x += 14;
			}

			DrawString(x + 2, y + 2, STR_ABBREV_NONE, 16);
			x += 14;
			DrawString(x + 2, y + 2, STR_ABBREV_ALL, 16);
			DrawString(72, y + 2, STR_ABBREV_ALL, 16);

			if (w->vscroll.count == 0) { // player has no stations
				DrawString(xb, 40, STR_304A_NONE, 0);
				return;
			}

			max = min(w->vscroll.pos + w->vscroll.cap, sl->list_length);
			y = 40; // start of the list-widget

			for (i = w->vscroll.pos; i < max; ++i) { // do until max number of stations of owner
				Station* st = GetStation(sl->sort_list[i].index);
				uint j;
				int x;

				assert(st->xy);
				assert(st->owner == owner);

				SetDParam(0, st->index);
				SetDParam(1, st->facilities);
				x = DrawString(xb, y, STR_3049_0, 0) + 5;

				// show cargo waiting and station ratings
				for (j = 0; j != NUM_CARGO; j++) {
					uint acc = GB(st->goods[j].waiting_acceptance, 0, 12);

					if (acc != 0) {
						StationsWndShowStationRating(x, y, j, acc, st->goods[j].rating);
						x += 10;
					}
				}
				y += 10;
			}
		}
	} break;
	case WE_CLICK: {
		switch (e->click.widget) {
		case 3: {
			uint32 id_v = (e->click.pt.y - 41) / 10;

			if (id_v >= w->vscroll.cap) return; // click out of bounds

			id_v += w->vscroll.pos;

			if (id_v >= sl->list_length) return; // click out of list bound

			{
				const Station *st = GetStation(sl->sort_list[id_v].index);

				assert(st->owner == owner);
				ScrollMainWindowToTile(st->xy);
			}
		} break;
		case 6: /* train */
		case 7: /* truck */
		case 8: /* bus */
		case 9: /* airport */
		case 10: /* dock */
			if (_ctrl_pressed) {
				TOGGLEBIT(facilities, e->click.widget - 6);
			} else {
				facilities = 0;
				SETBIT(facilities, e->click.widget - 6);
			}
			sl->flags |= SL_REBUILD;
			SetWindowDirty(w);
		break;
		case 26:
			facilities = FACIL_TRAIN | FACIL_TRUCK_STOP | FACIL_BUS_STOP | FACIL_AIRPORT | FACIL_DOCK;
			sl->flags |= SL_REBUILD;
			SetWindowDirty(w);
			break;
		case 27:
			cargo_filter = 0x1FFF; /* select everything */
			sl->flags |= SL_REBUILD;
			SetWindowDirty(w);
			break;
		case 28: /*flip sorting method asc/desc*/
			TOGGLEBIT(sl->flags, 0); //DESC-flag
			sl->flags |= SL_RESORT;
			SetWindowDirty(w);
		break;
		case 29: case 30: /* select sorting criteria dropdown menu */
			ShowDropDownMenu(w, _station_sort_listing, sl->sort_type, 30, 0, 0);
		break;
		default:
			if (e->click.widget >= 12 && e->click.widget <= 24) { //change cargo_filter
				if (_ctrl_pressed) {
					TOGGLEBIT(cargo_filter, e->click.widget - 12);
				} else {
					cargo_filter = 0;
					SETBIT(cargo_filter, e->click.widget - 12);
				}
				sl->flags |= SL_REBUILD;
				SetWindowDirty(w);
			}
		}
	} break;
	case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
		if (sl->sort_type != e->dropdown.index) {
			// value has changed -> resort
			sl->sort_type = e->dropdown.index;
			sl->flags |= SL_RESORT;
		}
		SetWindowDirty(w);
		break;

	case WE_TICK:
		if (--sl->resort_timer == 0) {
			DEBUG(misc, 1) ("Periodic rebuild station list player %d", owner);
			sl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
			sl->flags |= VL_REBUILD;
			SetWindowDirty(w);
		}
		break;

	case WE_CREATE: /* set up resort timer */
		sl->sort_list = NULL;
		sl->flags = SL_REBUILD;
		sl->sort_type = 0;
		sl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
		break;

	case WE_RESIZE:
		w->vscroll.cap += e->sizing.diff.y / 10;
		break;
	}
}

static const Widget _player_stations_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   345,     0,    13, STR_3048_STATIONS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   346,   357,     0,    13, 0x0,               STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_RB,    14,     0,   345,    37,   161, 0x0,               STR_3057_STATION_NAMES_CLICK_ON},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   346,   357,    25,   149, 0x0,               STR_0190_SCROLL_BAR_SCROLLS_LIST},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   346,   357,   150,   161, 0x0,               STR_RESIZE_BUTTON},
//Index 6
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    13,    14,    24, STR_TRAIN, STR_USE_CTRL_TO_SELECT_MORE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    14,    27,    14,    24, STR_LORRY, STR_USE_CTRL_TO_SELECT_MORE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    28,    41,    14,    24, STR_BUS,   STR_USE_CTRL_TO_SELECT_MORE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    42,    55,    14,    24, STR_PLANE, STR_USE_CTRL_TO_SELECT_MORE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    56,    69,    14,    24, STR_SHIP,  STR_USE_CTRL_TO_SELECT_MORE},
//Index 11
{      WWT_PANEL,   RESIZE_NONE,    14,    83,    88,    14,    24, 0x0, STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,    89,   102,    14,    24, 0x0, STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   103,   116,    14,    24, 0x0, STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   117,   130,    14,    24, 0x0, STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   131,   144,    14,    24, 0x0, STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   145,   158,    14,    24, 0x0, STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   159,   172,    14,    24, 0x0, STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   173,   186,    14,    24, 0x0, STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   187,   200,    14,    24, 0x0, STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   201,   214,    14,    24, 0x0, STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   215,   228,    14,    24, 0x0, STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   229,   242,    14,    24, 0x0, STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   243,   256,    14,    24, 0x0, STR_USE_CTRL_TO_SELECT_MORE},
{      WWT_PANEL,   RESIZE_NONE,    14,   257,   270,    14,    24, 0x0, STR_NO_WAITING_CARGO},
{      WWT_PANEL,  RESIZE_RIGHT,    14,   285,   357,    14,    24, 0x0, STR_NULL},

//26
{      WWT_PANEL,   RESIZE_NONE,    14,    70,    83,    14,    24, 0x0, STR_SELECT_ALL_FACILITIES},
{      WWT_PANEL,   RESIZE_NONE,    14,   271,   284,    14,    24, 0x0, STR_SELECT_ALL_TYPES},

//28
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    80,    25,    36, STR_SORT_BY, STR_SORT_ORDER_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,    81,   232,    25,    36, 0x0,         STR_SORT_CRITERIA_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   233,   243,    25,    36, STR_0225,    STR_SORT_CRITERIA_TIP},
{      WWT_PANEL,  RESIZE_RIGHT,    14,   244,   345,    25,    36, 0x0,         STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _player_stations_desc = {
	-1, -1, 358, 162,
	WC_STATION_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_player_stations_widgets,
	PlayerStationsWndProc
};


void ShowPlayerStations(PlayerID player)
{
	Window *w;

	w = AllocateWindowDescFront(&_player_stations_desc, player);
	if (w != NULL) {
		w->caption_color = (byte)w->window_number;
		w->vscroll.cap = 12;
		w->resize.step_height = 10;
		w->resize.height = w->height - 10 * 7; // minimum if 5 in the list
	}
}

static const Widget _station_view_expanded_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   236,     0,    13, STR_300A_0,        STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   237,   248,     0,    13, 0x0,               STR_STICKY_BUTTON},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   236,    14,    65, 0x0,               STR_NULL},
{  WWT_SCROLLBAR,   RESIZE_NONE,    14,   237,   248,    14,    65, 0x0,               STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,               STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   248,    66,   197, 0x0,               STR_NULL},
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
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   236,    14,    65, 0x0,               STR_NULL},
{  WWT_SCROLLBAR,   RESIZE_NONE,    14,   237,   248,    14,    65, 0x0,               STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   248,    66,    97, 0x0,               STR_NULL},
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
	uint i;
	uint num;
	int x,y;
	int pos;
	StringID str;

	num = 1;
	for (i = 0; i != NUM_CARGO; i++) {
		if (GB(st->goods[i].waiting_acceptance, 0, 12) != 0) {
			num++;
			if (st->goods[i].enroute_from != station_id) num++;
		}
	}
	SetVScrollCount(w, num);

	w->disabled_state = st->owner == _local_player ? 0 : (1 << 9);

	if (!(st->facilities & FACIL_TRAIN)) SETBIT(w->disabled_state,  10);
	if (!(st->facilities & FACIL_TRUCK_STOP) &&
			!(st->facilities & FACIL_BUS_STOP)) SETBIT(w->disabled_state, 11);
	if (!(st->facilities & FACIL_AIRPORT)) SETBIT(w->disabled_state, 12);
	if (!(st->facilities & FACIL_DOCK)) SETBIT(w->disabled_state, 13);

	SetDParam(0, st->index);
	SetDParam(1, st->facilities);
	DrawWindowWidgets(w);

	x = 2;
	y = 15;
	pos = w->vscroll.pos;

	if (--pos < 0) {
		str = STR_00D0_NOTHING;
		for (i = 0; i != NUM_CARGO; i++) {
			if (GB(st->goods[i].waiting_acceptance, 0, 12) != 0) str = STR_EMPTY;
		}
		SetDParam(0, str);
		DrawString(x, y, STR_0008_WAITING, 0);
		y += 10;
	}

	i = 0;
	do {
		uint waiting = GB(st->goods[i].waiting_acceptance, 0, 12);
		if (waiting == 0) continue;

		num = (waiting + 5) / 10;
		if (num != 0) {
			int cur_x = x;
			num = min(num, 23);
			do {
				DrawSprite(_cargoc.sprites[i], cur_x, y);
				cur_x += 10;
			} while (--num);
		}

		if ( st->goods[i].enroute_from == station_id) {
			if (--pos < 0) {
				SetDParam(1, waiting);
				SetDParam(0, _cargoc.names_long[i]);
				DrawStringRightAligned(x + 234, y, STR_0009, 0);
				y += 10;
			}
		} else {
			/* enroute */
			if (--pos < 0) {
				SetDParam(1, waiting);
				SetDParam(0, _cargoc.names_long[i]);
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

		b = InlineString(b, STR_000C_ACCEPTS);

		for (i = 0; i != NUM_CARGO; i++) {
			if (b >= endof(_userstring) - 5 - 1) break;
			if (st->goods[i].waiting_acceptance & 0x8000) {
				b = InlineString(b, _cargoc.names_s[i]);
				*b++ = ',';
				*b++ = ' ';
			}
		}

		if (b == &_userstring[3]) {
			b = InlineString(b, STR_00D0_NOTHING);
			*b++ = '\0';
		} else {
			b[-2] = '\0';
		}

		DrawStringMultiLine(2, 67, STR_SPEC_USERSTRING, 245);
	} else {
		DrawString(2, 67, STR_3034_LOCAL_RATING_OF_TRANSPORT, 0);

		y = 77;
		for (i = 0; i != NUM_CARGO; i++) {
			if (st->goods[i].enroute_from != INVALID_STATION) {
				SetDParam(0, _cargoc.names_s[i]);
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
		switch (e->click.widget) {
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

		case 9: {
			SetDParam(0, w->window_number);
			ShowQueryString(STR_STATION, STR_3030_RENAME_STATION_LOADING, 31, 180, w->window_class, w->window_number);
		} break;

		case 10: { /* Show a list of scheduled trains to this station */
			const Station *st = GetStation(w->window_number);
			ShowPlayerTrains(st->owner, w->window_number);
			break;
		}

		case 11: { /* Show a list of scheduled road-vehicles to this station */
			const Station *st = GetStation(w->window_number);
			ShowPlayerRoadVehicles(st->owner, w->window_number);
			break;
		}

		case 12: { /* Show a list of scheduled aircraft to this station */
			const Station *st = GetStation(w->window_number);
			/* Since oilrigs have no owners, show the scheduled aircraft of current player */
			PlayerID owner = (st->owner == OWNER_NONE) ? _current_player : st->owner;
			ShowPlayerAircraft(owner, w->window_number);
			break;
		}

		case 13: { /* Show a list of scheduled ships to this station */
			const Station *st = GetStation(w->window_number);
			/* Since oilrigs/bouys have no owners, show the scheduled ships of current player */
			PlayerID owner = (st->owner == OWNER_NONE) ? _current_player : st->owner;
			ShowPlayerShips(owner, w->window_number);
			break;
		}
		}
		break;

	case WE_ON_EDIT_TEXT:
		if (e->edittext.str[0] != '\0') {
			_cmd_text = e->edittext.str;
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
	-1, -1, 249, 110,
	WC_STATION_VIEW,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_station_view_widgets,
	StationViewWndProc
};

void ShowStationViewWindow(StationID station)
{
	Window *w;

	w = AllocateWindowDescFront(&_station_view_desc, station);
	if (w != NULL) {
		PlayerID owner = GetStation(w->window_number)->owner;
		if (owner != OWNER_NONE) w->caption_color = owner;
		w->vscroll.cap = 5;
	}
}
