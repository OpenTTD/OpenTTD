/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "strings.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "town.h"
#include "window.h"
#include "gfx.h"
#include "viewport.h"
#include "gui.h"
#include "command.h"
#include "player.h"
#include "network/network.h"
#include "variables.h"
#include "helpers.hpp"

static const Widget _town_authority_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    13,     0,    10,     0,    13, STR_00C5,                 STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    13,    11,   316,     0,    13, STR_2022_LOCAL_AUTHORITY, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    13,     0,   316,    14,   105, 0x0,                      STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    13,     0,   304,   106,   157, 0x0,                      STR_2043_LIST_OF_THINGS_TO_DO_AT},
{  WWT_SCROLLBAR,   RESIZE_NONE,    13,   305,   316,   106,   157, 0x0,                      STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,   RESIZE_NONE,    13,     0,   316,   158,   209, 0x0,                      STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,     0,   316,   210,   221, STR_2042_DO_IT,           STR_2044_CARRY_OUT_THE_HIGHLIGHTED},
{   WIDGETS_END},
};

extern const byte _town_action_costs[8];

/** Get a list of available actions to do at a town.
 * @param *nump if not NULL add put the number of available actions in it
 * @param pid the player that is querying the town
 * @param *t the town that is queried
 * @return bitmasked value of enabled actions
 */
uint GetMaskOfTownActions(int *nump, PlayerID pid, const Town *t)
{
	int32 avail, ref;
	int num = 0;
	uint avail_buttons = 0x7F; // by default all buttons except bribe are enabled.
	uint buttons = 0;

	if (pid != PLAYER_SPECTATOR) {
		uint i;

		// bribe option enabled?
		if (_patches.bribe) {
			// if unwanted, disable everything.
			if (t->unwanted[pid]) {
				avail_buttons = 0;
			} else if (t->ratings[pid] < RATING_BRIBE_MAXIMUM) {
				SETBIT(avail_buttons, 7); // Allow bribing
			}
		}

		// Things worth more than this are not shown
		avail = GetPlayer(pid)->player_money + _price.station_value * 200;
		ref = _price.build_industry >> 8;

		for (i = 0; i != lengthof(_town_action_costs); i++, avail_buttons >>= 1) {
			if (HASBIT(avail_buttons, 0) && avail >= _town_action_costs[i] * ref) {
				SETBIT(buttons, i);
				num++;
			}
		}

		/* Disable build statue if already built */
		if (HASBIT(t->statues, pid)) {
			CLRBIT(buttons, 4);
			num--;
		}

	}

	if (nump != NULL) *nump = num;
	return buttons;
}

static int GetNthSetBit(uint32 bits, int n)
{
	int i = 0;

	if (n >= 0) {
		do {
			if (bits & 1 && --n < 0) return i;
			i++;
		} while (bits >>= 1);
	}
	return -1;
}

static void TownAuthorityWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const Town *t = GetTown(w->window_number);
		int numact;
		uint buttons = GetMaskOfTownActions(&numact, _local_player, t);

		SetVScrollCount(w, numact + 1);

		if (WP(w,def_d).data_1 != -1 && !HASBIT(buttons, WP(w,def_d).data_1))
			WP(w,def_d).data_1 = -1;

		SetWindowWidgetDisabledState(w, 6, WP(w, def_d).data_1 == -1);

		{
			int y;
			const Player *p;
			int r;
			StringID str;

			SetDParam(0, w->window_number);
			DrawWindowWidgets(w);

			DrawString(2, 15, STR_2023_TRANSPORT_COMPANY_RATINGS, 0);

			// Draw list of players
			y = 25;
			FOR_ALL_PLAYERS(p) {
				if (p->is_active && (HASBIT(t->have_ratings, p->index) || t->exclusivity == p->index)) {
					DrawPlayerIcon(p->index, 2, y);

					SetDParam(0, p->name_1);
					SetDParam(1, p->name_2);
					SetDParam(2, GetPlayerNameString(p->index, 3));

					r = t->ratings[p->index];
					(str = STR_3035_APPALLING, r <= RATING_APPALLING) || // Apalling
					(str++,                    r <= RATING_VERYPOOR)  || // Very Poor
					(str++,                    r <= RATING_POOR)      || // Poor
					(str++,                    r <= RATING_MEDIOCRE)  || // Mediocore
					(str++,                    r <= RATING_GOOD)      || // Good
					(str++,                    r <= RATING_VERYGOOD)  || // Very Good
					(str++,                    r <= RATING_EXCELLENT) || // Excellent
					(str++,                    true);                    // Outstanding

					SetDParam(4, str);
					if (t->exclusivity == p->index) { // red icon for player with exclusive rights
						DrawSprite(SPR_BLOT, PALETTE_TO_RED, 18, y);
					}

					DrawString(28, y, STR_2024, 0);
					y += 10;
				}
			}
		}

		// Draw actions list
		{
			int y = 107, i;
			int pos = w->vscroll.pos;

			if (--pos < 0) {
				DrawString(2, y, STR_2045_ACTIONS_AVAILABLE, 0);
				y += 10;
			}
			for (i = 0; buttons; i++, buttons >>= 1) {
				if (pos <= -5) break;

				if (buttons&1 && --pos < 0) {
					DrawString(3, y, STR_2046_SMALL_ADVERTISING_CAMPAIGN + i, 6);
					y += 10;
				}
			}
		}

		{
			int i = WP(w,def_d).data_1;

			if (i != -1) {
				SetDParam(1, (_price.build_industry >> 8) * _town_action_costs[i]);
				SetDParam(0, STR_2046_SMALL_ADVERTISING_CAMPAIGN + i);
				DrawStringMultiLine(2, 159, STR_204D_INITIATE_A_SMALL_LOCAL + i, 313);
			}
		}

	} break;

	case WE_CLICK:
		switch (e->we.click.widget) {
		case 3: { /* listbox */
			const Town *t = GetTown(w->window_number);
			int y = (e->we.click.pt.y - 0x6B) / 10;

			if (!IS_INT_INSIDE(y, 0, 5)) return;

			y = GetNthSetBit(GetMaskOfTownActions(NULL, _local_player, t), y + w->vscroll.pos - 1);
			if (y >= 0) {
				WP(w,def_d).data_1 = y;
				SetWindowDirty(w);
			}
			break;
		}

		case 6: { /* carry out the action */
			DoCommandP(GetTown(w->window_number)->xy, w->window_number, WP(w,def_d).data_1, NULL, CMD_DO_TOWN_ACTION | CMD_MSG(STR_00B4_CAN_T_DO_THIS));
			break;
		}
		}
		break;

	case WE_4:
		SetWindowDirty(w);
		break;
	}
}

static const WindowDesc _town_authority_desc = {
	WDP_AUTO, WDP_AUTO, 317, 222,
	WC_TOWN_AUTHORITY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_town_authority_widgets,
	TownAuthorityWndProc
};

static void ShowTownAuthorityWindow(uint town)
{
	Window *w = AllocateWindowDescFront(&_town_authority_desc, town);

	if (w != NULL) {
		w->vscroll.cap = 5;
		WP(w,def_d).data_1 = -1;
	}
}

static void TownViewWndProc(Window *w, WindowEvent *e)
{
	Town *t = GetTown(w->window_number);

	switch (e->event) {
	case WE_PAINT:
		// disable renaming town in network games if you are not the server
		SetWindowWidgetDisabledState(w, 8, _networking && !_network_server);

		SetDParam(0, t->index);
		DrawWindowWidgets(w);

		SetDParam(0, t->population);
		SetDParam(1, t->num_houses);
		DrawString(2, 107, STR_2006_POPULATION, 0);

		SetDParam(0, t->act_pass);
		SetDParam(1, t->max_pass);
		DrawString(2, 117, STR_200D_PASSENGERS_LAST_MONTH_MAX, 0);

		SetDParam(0, t->act_mail);
		SetDParam(1, t->max_mail);
		DrawString(2, 127, STR_200E_MAIL_LAST_MONTH_MAX, 0);

		DrawWindowViewport(w);
		break;

	case WE_CLICK:
		switch (e->we.click.widget) {
			case 6: /* scroll to location */
				ScrollMainWindowToTile(t->xy);
				break;

			case 7: /* town authority */
				ShowTownAuthorityWindow(w->window_number);
				break;

			case 8: /* rename */
				SetDParam(0, w->window_number);
				ShowQueryString(STR_TOWN, STR_2007_RENAME_TOWN, 31, 130, w, CS_ALPHANUMERAL);
				break;

			case 9: /* expand town */
				ExpandTown(t);
				break;

			case 10: /* delete town */
				DeleteTown(t);
				break;
		}
		break;

	case WE_ON_EDIT_TEXT:
		if (e->we.edittext.str[0] != '\0') {
			_cmd_text = e->we.edittext.str;
			DoCommandP(0, w->window_number, 0, NULL,
				CMD_RENAME_TOWN | CMD_MSG(STR_2008_CAN_T_RENAME_TOWN));
		}
		break;
	}
}


static const Widget _town_view_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    13,     0,    10,     0,    13, STR_00C5,                 STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    13,    11,   247,     0,    13, STR_2005,                 STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    13,   248,   259,     0,    13, 0x0,                      STR_STICKY_BUTTON},
{      WWT_PANEL,   RESIZE_NONE,    13,     0,   259,    14,   105, 0x0,                      STR_NULL},
{      WWT_INSET,   RESIZE_NONE,    13,     2,   257,    16,   103, 0x0,                      STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    13,     0,   259,   106,   137, 0x0,                      STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,     0,    85,   138,   149, STR_00E4_LOCATION,        STR_200B_CENTER_THE_MAIN_VIEW_ON},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,    86,   171,   138,   149, STR_2020_LOCAL_AUTHORITY, STR_2021_SHOW_INFORMATION_ON_LOCAL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,   172,   259,   138,   149, STR_0130_RENAME,          STR_200C_CHANGE_TOWN_NAME},
{   WIDGETS_END},
};

static const WindowDesc _town_view_desc = {
	WDP_AUTO, WDP_AUTO, 260, 150,
	WC_TOWN_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_town_view_widgets,
	TownViewWndProc
};

static const Widget _town_view_scen_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    13,     0,    10,     0,    13, STR_00C5,          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    13,    11,   172,     0,    13, STR_2005,          STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    13,   248,   259,     0,    13, 0x0,               STR_STICKY_BUTTON},
{      WWT_PANEL,   RESIZE_NONE,    13,     0,   259,    14,   105, 0x0,               STR_NULL},
{      WWT_INSET,   RESIZE_NONE,    13,     2,   257,    16,   103, 0x0,               STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    13,     0,   259,   106,   137, 0x0,               STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,     0,    85,   138,   149, STR_00E4_LOCATION, STR_200B_CENTER_THE_MAIN_VIEW_ON},
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,               STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,   173,   247,     0,    13, STR_0130_RENAME,   STR_200C_CHANGE_TOWN_NAME},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,    86,   171,   138,   149, STR_023C_EXPAND,   STR_023B_INCREASE_SIZE_OF_TOWN},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,   172,   259,   138,   149, STR_0290_DELETE,   STR_0291_DELETE_THIS_TOWN_COMPLETELY},
{   WIDGETS_END},
};

static const WindowDesc _town_view_scen_desc = {
	WDP_AUTO, WDP_AUTO, 260, 150,
	WC_TOWN_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_town_view_scen_widgets,
	TownViewWndProc
};

void ShowTownViewWindow(TownID town)
{
	Window *w;

	if (_game_mode != GM_EDITOR) {
		w = AllocateWindowDescFront(&_town_view_desc, town);
	} else {
		w = AllocateWindowDescFront(&_town_view_scen_desc, town);
	}

	if (w != NULL) {
		w->flags4 |= WF_DISABLE_VP_SCROLL;
		AssignWindowViewport(w, 3, 17, 0xFE, 0x56, GetTown(town)->xy, 1);
	}
}

static const Widget _town_directory_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    13,     0,    10,     0,    13, STR_00C5,               STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    13,    11,   195,     0,    13, STR_2000_TOWNS,         STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    13,   196,   207,     0,    13, 0x0,                    STR_STICKY_BUTTON},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,     0,    98,    14,    25, STR_SORT_BY_NAME,       STR_SORT_ORDER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,    99,   195,    14,    25, STR_SORT_BY_POPULATION, STR_SORT_ORDER_TIP},
{      WWT_PANEL, RESIZE_BOTTOM,    13,     0,   195,    26,   189, 0x0,                    STR_200A_TOWN_NAMES_CLICK_ON_NAME},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    13,   196,   207,    14,   189, 0x0,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    13,     0,   195,   190,   201, 0x0,                    STR_NULL},
{  WWT_RESIZEBOX,     RESIZE_TB,    13,   196,   207,   190,   201, 0x0,                    STR_RESIZE_BUTTON},
{   WIDGETS_END},
};


// used to get a sorted list of the towns
static uint _num_town_sort;

static char _bufcache[64];
static const Town* _last_town;

static int CDECL TownNameSorter(const void *a, const void *b)
{
	const Town* ta = *(const Town**)a;
	const Town* tb = *(const Town**)b;
	char buf1[64];
	int r;

	SetDParam(0, ta->index);
	GetString(buf1, STR_TOWN, lastof(buf1));

	/* If 'b' is the same town as in the last round, use the cached value
	 *  We do this to speed stuff up ('b' is called with the same value a lot of
	 *  times after eachother) */
	if (tb != _last_town) {
		_last_town = tb;
		SetDParam(0, tb->index);
		GetString(_bufcache, STR_TOWN, lastof(_bufcache));
	}

	r = strcmp(buf1, _bufcache);
	if (_town_sort_order & 1) r = -r;
	return r;
}

static int CDECL TownPopSorter(const void *a, const void *b)
{
	const Town* ta = *(const Town**)a;
	const Town* tb = *(const Town**)b;
	int r = ta->population - tb->population;
	if (_town_sort_order & 1) r = -r;
	return r;
}

static void MakeSortedTownList()
{
	const Town* t;
	uint n = 0;

	/* Create array for sorting */
	_town_sort = ReallocT(_town_sort, GetMaxTownIndex() + 1);
	if (_town_sort == NULL) error("Could not allocate memory for the town-sorting-list");

	FOR_ALL_TOWNS(t) _town_sort[n++] = t;

	_num_town_sort = n;

	_last_town = NULL; // used for "cache"
	qsort((void*)_town_sort, n, sizeof(_town_sort[0]), _town_sort_order & 2 ? TownPopSorter : TownNameSorter);

	DEBUG(misc, 3, "Resorting towns list");
}


static void TownDirectoryWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		if (_town_sort_dirty) {
			_town_sort_dirty = false;
			MakeSortedTownList();
		}

		SetVScrollCount(w, _num_town_sort);

		DrawWindowWidgets(w);
		DoDrawString(_town_sort_order & 1 ? DOWNARROW : UPARROW, (_town_sort_order <= 1) ? 88 : 187, 15, 0x10);

		{
			int n = 0;
			uint16 i = w->vscroll.pos;
			int y = 28;

			while (i < _num_town_sort) {
				const Town* t = _town_sort[i];

				assert(t->xy);

				SetDParam(0, t->index);
				SetDParam(1, t->population);
				DrawString(2, y, STR_2057, 0);

				y += 10;
				i++;
				if (++n == w->vscroll.cap) break; // max number of towns in 1 window
			}
			SetDParam(0, GetWorldPopulation());
			DrawString(3, w->height - 12 + 2, STR_TOWN_POPULATION, 0);
		}
	} break;

	case WE_CLICK:
		switch (e->we.click.widget) {
		case 3: { /* Sort by Name ascending/descending */
			_town_sort_order = (_town_sort_order == 0) ? 1 : 0;
			_town_sort_dirty = true;
			SetWindowDirty(w);
		} break;

		case 4: { /* Sort by Population ascending/descending */
			_town_sort_order = (_town_sort_order == 2) ? 3 : 2;
			_town_sort_dirty = true;
			SetWindowDirty(w);
		} break;

		case 5: { /* Click on Town Matrix */
			const Town* t;

			uint16 id_v = (e->we.click.pt.y - 28) / 10;

			if (id_v >= w->vscroll.cap) return; // click out of bounds

			id_v += w->vscroll.pos;

			if (id_v >= _num_town_sort) return; // click out of town bounds

			t = _town_sort[id_v];
			assert(t->xy);
			ScrollMainWindowToTile(t->xy);
			break;
		}
		}
		break;

	case WE_4:
		SetWindowDirty(w);
		break;

	case WE_RESIZE:
		w->vscroll.cap += e->we.sizing.diff.y / 10;
		break;
	}
}

static const WindowDesc _town_directory_desc = {
	WDP_AUTO, WDP_AUTO, 208, 202,
	WC_TOWN_DIRECTORY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_town_directory_widgets,
	TownDirectoryWndProc
};


void ShowTownDirectory()
{
	Window *w = AllocateWindowDescFront(&_town_directory_desc, 0);

	if (w != NULL) {
		w->vscroll.cap = 16;
		w->resize.step_height = 10;
		w->resize.height = w->height - 10 * 6; // minimum of 10 items in the list, each item 10 high
	}
}
