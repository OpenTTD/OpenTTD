#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "window.h"
#include "gui.h"
#include "station.h"
#include "gfx.h"
#include "player.h"
#include "town.h"
#include "command.h"

static void StationsWndShowStationRating(int x, int y, int type, uint acceptance, int rating)
{
	static const byte _rating_colors[NUM_CARGO] = {152,32,15,174,208,194,191,55,184,10,191,48};
	int color = _rating_colors[type];
	uint w;

	if (acceptance > 575)
		acceptance = 575;

	acceptance = (acceptance + 7) >> 3;

	/* draw cargo */
	if ( (w=acceptance>>3) != 0) {
		GfxFillRect(x, y, x+w-1, y+6, color);
		x += w;
	}

	if ( (w=acceptance&7) != 0) {
		if (w==7) w--;
		GfxFillRect(x, y+(w-1), x, y+6, color);
	}

	x -= (acceptance>>3);

	DrawString(x+1, y, _cargoc.names_short[type], 0x10);

	/* draw green/red ratings bar */
	GfxFillRect(x+1, y+8, x+7, y+8, 0xB8);

	rating = (rating >> 5);

	if (rating != 0) {
		GfxFillRect(x+1, y+8, x+rating, y+8, 0xD0);
	}
}

static SortStruct _station_sort[lengthof(_stations)];
static uint16 _num_station_sort[MAX_PLAYERS];

static char _bufcache[64];
static uint16 _last_station_idx;

static int CDECL StationNameSorter(const void *a, const void *b)
{
	char buf1[64];
	Station *st;
	const SortStruct *cmp1 = (const SortStruct*)a;
	const SortStruct *cmp2 = (const SortStruct*)b;

	st = DEREF_STATION(cmp1->index);
	SET_DPARAM16(0, st->town->townnametype);
	SET_DPARAM32(1, st->town->townnameparts);
	GetString(buf1, st->string_id);

	if ( cmp2->index != _last_station_idx) {
		_last_station_idx = cmp2->index;
		st = DEREF_STATION(cmp2->index);
		SET_DPARAM16(0, st->town->townnametype);
		SET_DPARAM32(1, st->town->townnameparts);
		GetString(_bufcache, st->string_id);
	}

	return strcmp(buf1, _bufcache);	// sort by name
}

static void GlobalSortStationList()
{
	const Station *st;
	uint32 n = 0;
	uint16 *i;

	// reset #-of stations to 0 because ++ is used for value-assignment
	for (i = _num_station_sort; i != endof(_num_station_sort); i++) {*i = 0;}

	FOR_ALL_STATIONS(st) {
		if(st->xy && st->owner != OWNER_NONE) {
			_station_sort[n].index = st->index;
			_station_sort[n++].owner = st->owner;
			_num_station_sort[st->owner]++;	// add number of stations of player
		}
	}

	// create cumulative station-ownership
	// stations are stored as a cummulative index, eg 25, 41, 43. This means
	// Player0: 25; Player1: (41-25) 16; Player2: (43-41) 2
	for (i = &_num_station_sort[1]; i != endof(_num_station_sort); i++) {*i += *(i-1);}

	qsort(_station_sort, n, sizeof(_station_sort[0]), GeneralOwnerSorter); // sort by owner

	// since indexes are messed up after adding/removing a station, mark all lists dirty
	memset(_station_sort_dirty, true, sizeof(_station_sort_dirty));
	_global_station_sort_dirty = false;

	DEBUG(misc, 1) ("Resorting global station list...");
}

static void MakeSortedStationList(byte owner)
{
	SortStruct *firstelement;
	uint32 n = 0;

	if (owner == 0) { // first element starts at 0th element and has n elements as described above
		firstelement =	&_station_sort[0];
		n =							_num_station_sort[0];
	}	else { // nth element starts at the end of the previous one, and has n elements as described above
		firstelement =	&_station_sort[_num_station_sort[owner-1]];
		n =							_num_station_sort[owner] - _num_station_sort[owner-1];
	}

	_last_station_idx = 0; // used for "cache" in namesorting
	qsort(firstelement, n, sizeof(_station_sort[0]), StationNameSorter); // sort by name

	_station_sort_dirty[owner] = false;

	DEBUG(misc, 1) ("Resorting Stations list player %d...", owner+1);
}

static void PlayerStationsWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		uint32 i;
		const byte window_number = (byte)w->window_number;

		// resort station window if stations have been added/removed
		if (_global_station_sort_dirty)
			GlobalSortStationList();

		if (_station_sort_dirty[window_number]) { // resort in case of a station rename.
			MakeSortedStationList(window_number);
		}

		// stations are stored as a cummulative index, eg 25, 41, 43. This means
		// Player0: 25; Player1: (41-25) 16; Player2: (43-41) 2 stations
		i = (window_number == 0) ? 0 : _num_station_sort[window_number-1];
		SetVScrollCount(w, _num_station_sort[window_number] - i);

		/* draw widgets, with player's name in the caption */
		{
			Player *p = DEREF_PLAYER(window_number);
			SET_DPARAM16(0, p->name_1);
			SET_DPARAM32(1, p->name_2);
			SET_DPARAM16(2, w->vscroll.count);
			DrawWindowWidgets(w);
		}

		{
			byte p = 0;
			Station *st;
			int x,xb = 2;
			int y = 16;	// offset from top of widget
			int j;

			if (w->vscroll.count == 0) {	// player has no stations
				DrawString(xb, y, STR_304A_NONE, 0);
				return;
			}

			i += w->vscroll.pos;	// offset from sorted station list of current player
			assert(i < _num_station_sort[window_number]); // at least one station must exist

			while (i < _num_station_sort[window_number]) {	// do until max number of stations of owner
				st = DEREF_STATION(_station_sort[i].index);

				assert(st->xy && st->owner == window_number);

				SET_DPARAM16(0, st->index);
				SET_DPARAM8(1, st->facilities);
				x = DrawString(xb, y, STR_3049_0, 0) + 5;

				// show cargo waiting and station ratings
				for(j=0; j!=NUM_CARGO; j++) {
					int acc = (st->goods[j].waiting_acceptance & 0xFFF);
					if (acc != 0) {
						StationsWndShowStationRating(x, y, j, acc, st->goods[j].rating);
						x += 10;
					}
				}
				y += 10;
				i++;	// next station
				if (++p == w->vscroll.cap) { break;} // max number of stations in 1 window
			}
		}
	} break;
	case WE_CLICK: {
		switch(e->click.widget) {
		case 2: {
			uint32 id_v = (e->click.pt.y - 15) / 10;

			if (id_v >= w->vscroll.cap) { return;} // click out of bounds

			id_v += w->vscroll.pos;

			{
				const byte owner = (byte)w->window_number;
				Station *st;
				id_v	+= (owner == 0) ? 0 : _num_station_sort[owner - 1]; // first element in list

				if (id_v >= _num_station_sort[owner]) { return;} // click out of station bound

				st = DEREF_STATION(_station_sort[id_v].index);

				assert(st->xy && st->owner == owner);

				ScrollMainWindowToTile(st->xy);
			}
		} break;
		}
	} break;

	case WE_4:
		WP(w,plstations_d).refresh_counter++;
		if (WP(w,plstations_d).refresh_counter==5) {
			WP(w,plstations_d).refresh_counter = 0;
			SetWindowDirty(w);
		}
		break;
	}
}

static const Widget _player_stations_widgets[] = {
{   WWT_CLOSEBOX,    14,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   357,     0,    13, STR_3048_STATIONS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,    14,     0,   346,    14,   137, 0x0, STR_3057_STATION_NAMES_CLICK_ON},
{  WWT_SCROLLBAR,    14,   347,   357,    14,   137, 0x0, STR_0190_SCROLL_BAR_SCROLLS_LIST},
{   WIDGETS_END},
};

static const WindowDesc _player_stations_desc = {
	-1, -1, 358, 138,
	WC_STATION_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_player_stations_widgets,
	PlayerStationsWndProc
};


void ShowPlayerStations(int player)
{
	Window *w;

	w = AllocateWindowDescFront(&_player_stations_desc, player);
	if (w) {
		w->caption_color = (byte)w->window_number;
		w->vscroll.cap = 12;
	}
}

static const Widget _station_view_expanded_widgets[] = {
{    WWT_TEXTBTN,    14,     0,    10,     0,    13, STR_00C5,		STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   248,     0,    13, STR_300A_0,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,    14,     0,   237,    14,    65, 0x0,					STR_NULL},
{  WWT_SCROLLBAR,    14,   238,   248,    14,    65, 0x0,					STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_EMPTY,     0,     0,     0,     0,     0, 0x0,					STR_NULL},
{     WWT_IMGBTN,    14,     0,   248,    66,   197, 0x0,					STR_NULL},
{ WWT_PUSHTXTBTN,    14,     0,    82,   198,   209, STR_00E4_LOCATION,	STR_3053_CENTER_MAIN_VIEW_ON_STATION},
{ WWT_PUSHTXTBTN,    14,    83,   165,   198,   209, STR_3033_ACCEPTS,	STR_3056_SHOW_LIST_OF_ACCEPTED_CARGO},
{ WWT_PUSHTXTBTN,    14,   166,   248,   198,   209, STR_0130_RENAME,		STR_3055_CHANGE_NAME_OF_STATION},
{   WIDGETS_END},
};

static const Widget _station_view_widgets[] = {
{    WWT_TEXTBTN,    14,     0,    10,     0,    13, STR_00C5,		STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   248,     0,    13, STR_300A_0,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,    14,     0,   237,    14,    65, 0x0,					STR_NULL},
{  WWT_SCROLLBAR,    14,   238,   248,    14,    65, 0x0,					STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_IMGBTN,    14,     0,   248,    66,    97, 0x0,					STR_NULL},
{      WWT_EMPTY,     0,     0,     0,     0,     0, 0x0,					STR_NULL},
{ WWT_PUSHTXTBTN,    14,     0,    82,    98,   109, STR_00E4_LOCATION,	STR_3053_CENTER_MAIN_VIEW_ON_STATION},
{ WWT_PUSHTXTBTN,    14,    83,   165,    98,   109, STR_3032_RATINGS,	STR_3054_SHOW_STATION_RATINGS},
{ WWT_PUSHTXTBTN,    14,   166,   248,    98,   109, STR_0130_RENAME,		STR_3055_CHANGE_NAME_OF_STATION},
{   WIDGETS_END},
};

static void DrawStationViewWindow(Window *w)
{
	Station *st;
	int i;
	int num;
	int x,y;
	int pos;
	StringID str;
	byte station_id;
	byte *b;


	station_id = (byte)w->window_number;

	st = DEREF_STATION(w->window_number);

	num = 1;
	for(i=0; i!=NUM_CARGO; i++) {
		if ((st->goods[i].waiting_acceptance & 0xFFF) != 0) {
			num++;
			if (st->goods[i].enroute_from != station_id)
				num++;
		}
	}
	SetVScrollCount(w, num);

	w->disabled_state = st->owner == _local_player ? 0 : (1 << 8);
	SET_DPARAM16(0, st->index);
	SET_DPARAM8(1, st->facilities);
	DrawWindowWidgets(w);

	x = 2;
	y = 15;
	pos = w->vscroll.pos;

	if (--pos < 0) {
		str = STR_00D0_NOTHING;
		for(i=0; i!=NUM_CARGO; i++)
			if (st->goods[i].waiting_acceptance & 0xFFF)
				str = STR_EMPTY;
		SET_DPARAM16(0, str);
		DrawString(x, y, STR_0008_WAITING, 0);
		y += 10;
	}

	i = 0;
	do {
		uint waiting = (st->goods[i].waiting_acceptance & 0xFFF);
		if (waiting == 0)
			continue;

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
				SET_DPARAM16(1, waiting);
				SET_DPARAM16(0, _cargoc.names_long_s[i] + (waiting==1 ? 0 : 32));
				DrawStringRightAligned(x + 234, y, STR_0009, 0);
				y += 10;
			}
		} else {
			/* enroute */
			if (--pos < 0) {
				SET_DPARAM16(1, waiting);
				SET_DPARAM16(0, _cargoc.names_long_s[i] + (waiting==1 ? 0 : 32));
				DrawStringRightAligned(x + 234, y, STR_000A_EN_ROUTE_FROM, 0);
				y += 10;
			}

			if (pos > -5 && --pos < 0) {
				SET_DPARAM16(0, st->goods[i].enroute_from);
				DrawStringRightAligned(x + 234, y, STR_000B, 0);
				y += 10;
			}
		}
	} while (pos > -5 && ++i != 12);

	if (w->widget == _station_view_widgets) {
		b = _userstring;
		b[0] = 0x81;
		b[1] = STR_000C_ACCEPTS;
		b[2] = STR_000C_ACCEPTS >> 8;
		b += 3;

		for(i=0; i!=NUM_CARGO; i++) {
			if ((b - (byte *) &_userstring) + 5 > USERSTRING_LEN - 1)
				break;
			if (st->goods[i].waiting_acceptance & 0x8000) {
				b[0] = 0x81;
				WRITE_LE_UINT16(b+1, _cargoc.names_s[i]);
				WRITE_LE_UINT16(b+3, 0x202C);
				b += 5;
			}
		}

		if (b == (byte*)&_userstring[3]) {
			b[0] = 0x81;
			b[1] = STR_00D0_NOTHING;
			b[2] = STR_00D0_NOTHING >> 8;
			b[3] = 0;
		} else {
			b[-2] = 0;
		}

		DrawStringMultiLine(2, 67, STR_SPEC_USERSTRING, 245);
	} else {

		DrawString(2, 67, STR_3034_LOCAL_RATING_OF_TRANSPORT, 0);

		y = 77;
		for(i=0; i!=NUM_CARGO; i++) {
			if (st->goods[i].enroute_from != 0xFF) {
				SET_DPARAM16(0, _cargoc.names_s[i]);
				SET_DPARAM8(2, st->goods[i].rating * 101 >> 8);
				SET_DPARAM16(1, STR_3035_APPALLING + (st->goods[i].rating >> 5));
				DrawString(8, y, STR_303D, 0);
				y += 10;
			}
		}
	}
}


static void StationViewWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT:
		DrawStationViewWindow(w);
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 6:
			ScrollMainWindowToTile(DEREF_STATION(w->window_number)->xy);
			break;

		case 7:
			SetWindowDirty(w);

			/* toggle height/widget set */
			w->height ^= (210 ^ 110);
			*(uint32*)&w->widget ^= (uint32)_station_view_expanded_widgets ^ (uint32)_station_view_widgets;

			SetWindowDirty(w);
			break;

		case 8: {
			Station *st = DEREF_STATION(w->window_number);
			SET_DPARAM16(0, st->town->townnametype);
			SET_DPARAM32(1, st->town->townnameparts);
			ShowQueryString(st->string_id, STR_3030_RENAME_STATION_LOADING, 31, 180, w->window_class, w->window_number);
		}	break;
		}
		break;

	case WE_ON_EDIT_TEXT: {
		Station *st;
		byte *b = e->edittext.str;
		if (*b == 0)
			return;
		memcpy(_decode_parameters, b, 32);

		st = DEREF_STATION(w->window_number);
		DoCommandP(st->xy, w->window_number, 0, NULL, CMD_RENAME_STATION | CMD_MSG(STR_3031_CAN_T_RENAME_STATION));
	} break;
	}
}


static const WindowDesc _station_view_desc = {
	-1, -1, 249, 110,
	WC_STATION_VIEW,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_station_view_widgets,
	StationViewWndProc
};

void ShowStationViewWindow(int station)
{
	Window *w;
	byte color;

	w = AllocateWindowDescFront(&_station_view_desc, station);
	if (w) {
		color = DEREF_STATION(w->window_number)->owner;
		if (color != 0x10)
			w->caption_color = color;
		w->vscroll.cap = 5;
	}
}
