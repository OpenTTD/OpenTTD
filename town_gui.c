#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "town.h"
#include "window.h"
#include "gfx.h"
#include "viewport.h"
#include "gui.h"
#include "command.h"
#include "player.h"

static const Widget _town_authority_widgets[] = {
{    WWT_TEXTBTN,    13,     0,    10,     0,    13, STR_00C5,				STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    13,    11,   316,     0,    13, STR_2022_LOCAL_AUTHORITY, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,    13,     0,   316,    14,   105, 0x0,							STR_NULL},
{     WWT_IMGBTN,    13,     0,   305,   106,   157, 0x0,							STR_2043_LIST_OF_THINGS_TO_DO_AT},
{  WWT_SCROLLBAR,    13,   306,   316,   106,   157, 0x0,							STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_IMGBTN,    13,     0,   316,   158,   209, 0x0,							STR_NULL},
{ WWT_PUSHTXTBTN,    13,     0,   316,   210,   221, STR_2042_DO_IT,	STR_2044_CARRY_OUT_THE_HIGHLIGHTED},
{   WIDGETS_END},
};

extern const byte _town_action_costs[8];
extern void DrawPlayerIcon(int p, int x, int y);

static uint GetMaskOfTownActions(int *nump, Town *t)
{
	int32 avail, ref;
	int i, num;
	uint avail_buttons = 0x7F; // by default all buttons except bribe are enabled.
	uint buttons;

	if (_local_player != OWNER_SPECTATOR) {
		// bribe option enabled?
		if (_patches.bribe) {
			// if unwanted, disable everything.
			if (t->unwanted[_local_player]) {
				avail_buttons = 0;
			} else if (t->ratings[_local_player] < 600)
				avail_buttons |= (1 << 7); // only bribe if less than excellent
		}

		// Things worth more than this are not shown
		avail = DEREF_PLAYER(_local_player)->player_money + _price.station_value * 200;
		ref = _price.build_industry >> 8;

		for(i=0,buttons=0,num=0; i != lengthof(_town_action_costs); i++,avail_buttons>>=1) {
			if (avail_buttons&1 && avail >= _town_action_costs[i] * ref) {
				SETBIT(buttons, i);
				num++;
			}
		}

		// Disable build statue if already built
		if(HASBIT(t->statues, _local_player))
		{
			CLRBIT(buttons, 4);
			num--;
		}

	} else {
		// no actions available for spectator
		buttons = 0;
		num = 0;
	}

	if (nump) *nump = num;
	return buttons;
}

static int GetNthSetBit(uint32 bits, int n)
{
	int i = 0;
	if (n >= 0) {
		do {
			if (bits&1 && --n < 0) return i;
			i++;
		} while (bits>>=1);
	}
	return -1;
}

static void TownAuthorityWndProc(Window *w, WindowEvent *e)
{
	uint buttons;
	int numact;
	Town *t = DEREF_TOWN(w->window_number);

	switch(e->event) {
	case WE_PAINT:
		buttons = GetMaskOfTownActions(&numact, t);
		SetVScrollCount(w, numact + 1);

		if (WP(w,def_d).data_1 != -1 && !HASBIT(buttons, WP(w,def_d).data_1))
			WP(w,def_d).data_1 = -1;

		w->disabled_state = (WP(w,def_d).data_1 == -1) ? (1 << 6) : 0;

		{
			int y;
			Player *p;
			int r;
			StringID str;

			SetDParam(0, w->window_number);
			DrawWindowWidgets(w);

			DrawString(2, 15, STR_2023_TRANSPORT_COMPANY_RATINGS, 0);

			// Draw list of players
			y = 25;
			FOR_ALL_PLAYERS(p) {
				if (p->is_active && (HASBIT(t->have_ratings, p->index) || t->exclusivity==p->index)) {
					DrawPlayerIcon(p->index, 2, y);

					SetDParam(0, p->name_1);
					SetDParam(1, p->name_2);
					SetDParam(2, GetPlayerNameString(p->index, 3));

					r = t->ratings[p->index];
					(str = STR_3035_APPALLING, r <= -400) ||	// Apalling
					(str++, r <= -200) ||											// Very Poor
					(str++, r <= 0) ||												// Poor
					(str++, r <= 200) ||											// Mediocore
					(str++, r <= 400) ||											// Good
					(str++, r <= 600) ||											// Very Good
					(str++, r <= 800) ||											// Excellent
					(str++, true);														// Outstanding

					/*	WARNING ugly hack!
							GetPlayerNameString sets up (Player #) if the player is human in an extra DPARAM16
							It seems that if player is non-human, nothing is set up, so param is 0. GetString doesn't like
							that because there is another param after it.
							So we'll just shift the rating one back if player is AI and all is fine
						*/
					SetDParam((IS_HUMAN_PLAYER(p->index) ? 4 : 3), str);
					if (t->exclusivity == p->index) // red icon for player with exclusive rights
						DrawSprite((SPR_BLOT) | 0x30b8000, 18, y);

					DrawString(28, y, STR_2024, 0);
					y+=10;
				}
			}
		}

		// Draw actions list
		{
			int y = 107, i;
			int pos = w->vscroll.pos;

			if (--pos < 0) {
				DrawString(2, y, STR_2045_ACTIONS_AVAILABLE, 0);
				y+=10;
			}
			for(i=0; buttons; i++,buttons>>=1) {
				if (pos <= -5)
					break;

				if (buttons&1 && --pos < 0) {
					DrawString(3, y, STR_2046_SMALL_ADVERTISING_CAMPAIGN + i, 6);
					y += 10;
				}
			}
		}

		{
			int i;
			if ((i=WP(w,def_d).data_1) != -1) {
				SetDParam(1, (_price.build_industry >> 8) * _town_action_costs[i]);
				SetDParam(0, STR_2046_SMALL_ADVERTISING_CAMPAIGN + i);
				DrawStringMultiLine(2, 159, STR_204D_INITIATE_A_SMALL_LOCAL + i, 313);
			}
		}

		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 3: { /* listbox */
			int y = (e->click.pt.y - 0x6B) / 10;
			if (!IS_INT_INSIDE(y, 0, 5))
				return;

			y = GetNthSetBit(GetMaskOfTownActions(NULL, t), y + w->vscroll.pos - 1);
			if (y >= 0) {
				WP(w,def_d).data_1 = y;
				SetWindowDirty(w);
			}
			break;
		}

		case 6: { /* carry out the action */
			DoCommandP(t->xy, w->window_number, WP(w,def_d).data_1, NULL, CMD_DO_TOWN_ACTION | CMD_MSG(STR_2054_CAN_T_DO_THIS));
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
	-1, -1, 317, 222,
	WC_TOWN_AUTHORITY,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_town_authority_widgets,
	TownAuthorityWndProc
};

static void ShowTownAuthorityWindow(uint town)
{
	Window *w;

	w = AllocateWindowDescFront(&_town_authority_desc, town);
	if (w) {
		w->vscroll.cap = 5;
		WP(w,def_d).data_1 = -1;
	}
}

static void TownViewWndProc(Window *w, WindowEvent *e)
{
	Town *t = DEREF_TOWN(w->window_number);

	switch(e->event) {
	case WE_PAINT:
		SetDParam(0, t->index);
		DrawWindowWidgets(w);

		SetDParam(0, t->population);
		SetDParam(1, t->num_houses);
		DrawString(2,107,STR_2006_POPULATION,0);

		SetDParam(0, t->act_pass);
		SetDParam(1, t->max_pass);
		DrawString(2,117,STR_200D_PASSENGERS_LAST_MONTH_MAX,0);

		SetDParam(0, t->act_mail);
		SetDParam(1, t->max_mail);
		DrawString(2,127,STR_200E_MAIL_LAST_MONTH_MAX,0);

		DrawWindowViewport(w);
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 5: /* scroll to location */
			ScrollMainWindowToTile(t->xy);
			break;
		case 6: /* town authority */
			ShowTownAuthorityWindow(w->window_number);
			break;
		case 7: /* rename */
			SetDParam(0, t->townnameparts);
			ShowQueryString(t->townnametype, STR_2007_RENAME_TOWN, 31, 130, w->window_class, w->window_number);
			break;
		case 8: /* expand town */
			ExpandTown(t);
			break;
		case 9: /* delete town */
			DeleteTown(t);
			break;
		}
		break;

	case WE_ON_EDIT_TEXT: {
		byte *b = e->edittext.str;
		if (*b == 0)
			return;
		memcpy(_decode_parameters, b, 32);
		DoCommandP(0, w->window_number, 0, NULL, CMD_RENAME_TOWN | CMD_MSG(STR_2008_CAN_T_RENAME_TOWN));
	} break;
	}
}


static const Widget _town_view_widgets[] = {
{    WWT_TEXTBTN,    13,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    13,    11,   259,     0,    13, STR_2005, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,    13,     0,   259,    14,   105, 0x0,      STR_NULL},
{          WWT_6,    13,     2,   257,    16,   103, 0x0,      STR_NULL},
{     WWT_IMGBTN,    13,     0,   259,   106,   137, 0x0,      STR_NULL},
{ WWT_PUSHTXTBTN,    13,     0,    85,   138,   149, STR_00E4_LOCATION,				STR_200B_CENTER_THE_MAIN_VIEW_ON},
{ WWT_PUSHTXTBTN,    13,    86,   171,   138,   149, STR_2020_LOCAL_AUTHORITY,STR_2021_SHOW_INFORMATION_ON_LOCAL},
{ WWT_PUSHTXTBTN,    13,   172,   259,   138,   149, STR_0130_RENAME,					STR_200C_CHANGE_TOWN_NAME},
{   WIDGETS_END},
};

static const WindowDesc _town_view_desc = {
	-1, -1, 260, 150,
	WC_TOWN_VIEW,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_town_view_widgets,
	TownViewWndProc
};

static const Widget _town_view_scen_widgets[] = {
{    WWT_TEXTBTN,    13,     0,    10,     0,    13, STR_00C5,					STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    13,    11,   184,     0,    13, STR_2005,					STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,    13,     0,   259,    14,   105, 0x0,								STR_NULL},
{          WWT_6,    13,     2,   257,    16,   103, 0x0,								STR_NULL},
{     WWT_IMGBTN,    13,     0,   259,   106,   137, 0x0,								STR_NULL},
{ WWT_PUSHTXTBTN,    13,     0,    85,   138,   149, STR_00E4_LOCATION,	STR_200B_CENTER_THE_MAIN_VIEW_ON},
{      WWT_EMPTY,     0,     0,     0,     0,     0, 0x0,								STR_NULL},
{ WWT_PUSHTXTBTN,    13,   185,   259,     0,    13, STR_0130_RENAME,		STR_200C_CHANGE_TOWN_NAME},
{ WWT_PUSHTXTBTN,    13,    86,   171,   138,   149, STR_023C_EXPAND,		STR_023B_INCREASE_SIZE_OF_TOWN},
{ WWT_PUSHTXTBTN,    13,   172,   259,   138,   149, STR_0290_DELETE,		STR_0291_DELETE_THIS_TOWN_COMPLETELY},
{   WIDGETS_END},
};

static const WindowDesc _town_view_scen_desc = {
	-1, -1, 260, 150,
	WC_TOWN_VIEW,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_town_view_scen_widgets,
	TownViewWndProc
};

void ShowTownViewWindow(uint town)
{
	Window *w;
	Town *t;

	if (_game_mode != GM_EDITOR) {
		w = AllocateWindowDescFront(&_town_view_desc, town);
	} else {
		w = AllocateWindowDescFront(&_town_view_scen_desc, town);
	}

	if (w) {
		w->flags4 |= WF_DISABLE_VP_SCROLL;
		t = DEREF_TOWN(w->window_number);
		AssignWindowViewport(w, 3, 17, 0xFE, 0x56, t->xy, 1);
	}
}

static const Widget _town_directory_widgets[] = {
{    WWT_TEXTBTN,    13,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    13,    11,   207,     0,    13, STR_2000_TOWNS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,    13,     0,    98,    14,    25, STR_SORT_BY_NAME, STR_SORT_TIP},
{ WWT_PUSHTXTBTN,    13,    99,   196,    14,    25, STR_SORT_BY_POPULATION,STR_SORT_TIP},
{     WWT_IMGBTN,    13,     0,   196,    26,   189, 0x0, STR_200A_TOWN_NAMES_CLICK_ON_NAME},
{  WWT_SCROLLBAR,    13,   197,   207,    14,   189, 0x0, STR_0190_SCROLL_BAR_SCROLLS_LIST},
{   WIDGETS_END},
};


// used to get a sorted list of the towns
static byte _town_sort[lengthof(_towns)];
static uint _num_town_sort;

static char _bufcache[64];
static byte _last_town_idx;

static int CDECL TownNameSorter(const void *a, const void *b)
{
	char buf1[64];
	const Town *t;
	byte val;
	int r;

	t = DEREF_TOWN(*(const byte*)a);
	SetDParam(0, t->townnameparts);
	GetString(buf1, t->townnametype);

	if ( (val=*(const byte*)b) != _last_town_idx) {
		_last_town_idx = val;
		t = DEREF_TOWN(val);
		SetDParam(0, t->townnameparts);
		GetString(_bufcache, t->townnametype);
	}

	r = strcmp(buf1, _bufcache);
	if (_town_sort_order & 1) r = -r;
	return r;
}

static int CDECL TownPopSorter(const void *a, const void *b)
{
	const Town *ta = DEREF_TOWN(*(const byte*)a);
	const Town *tb = DEREF_TOWN(*(const byte*)b);
	int r = ta->population - tb->population;
	if (_town_sort_order & 1) r = -r;
	return r;
}

static void MakeSortedTownList()
{
	Town *t;
	int n = 0;
	FOR_ALL_TOWNS(t) if(t->xy) _town_sort[n++] = t->index;
	_num_town_sort = n;

	_last_town_idx = 0; // used for "cache"
	qsort(_town_sort, n, sizeof(_town_sort[0]), _town_sort_order & 2 ? TownPopSorter : TownNameSorter);

	DEBUG(misc, 1) ("Resorting Towns list...");
}


static void TownDirectoryWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {

		if (_town_sort_dirty) {
			_town_sort_dirty = false;
			MakeSortedTownList();
		}

		SetVScrollCount(w, _num_town_sort);

		DrawWindowWidgets(w);
		DoDrawString(_town_sort_order & 1 ? "\xAA" : "\xA0", (_town_sort_order <= 1) ? 88 : 187, 15, 0x10);

		{
			Town *t;
			int n = 0;
			uint16 i = w->vscroll.pos;
			int y = 28;

			while (i < _num_town_sort) {
				t = DEREF_TOWN(_town_sort[i]);

				assert(t->xy);

				SetDParam(0, t->index);
				SetDParam(1, t->population);
				DrawString(2, y, STR_2057, 0);

				y += 10;
				i++;
				if (++n == w->vscroll.cap) { break;} // max number of towns in 1 window
			}
		}
	} break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 2: { /* Sort by Name ascending/descending */
			_town_sort_order = (_town_sort_order == 0) ? 1 : 0;
			_town_sort_dirty = true;
			SetWindowDirty(w);
		} break;

		case 3: { /* Sort by Population ascending/descending */
			_town_sort_order = (_town_sort_order == 2) ? 3 : 2;
			_town_sort_dirty = true;
			SetWindowDirty(w);
		} break;

		case 4: { /* Click on Town Matrix */
			uint16 id_v = (e->click.pt.y - 28) / 10;

			if (id_v >= w->vscroll.cap) { return;} // click out of bounds

			id_v += w->vscroll.pos;

			if (id_v >= _num_town_sort) { return;} // click out of town bounds

			{
				Town *t = DEREF_TOWN(_town_sort[id_v]);
				assert(t->xy);

				ScrollMainWindowToTile(t->xy);
			}
		}	break;
		}
		break;

	case WE_4:
		SetWindowDirty(w);
		break;
	}
}

static const WindowDesc _town_directory_desc = {
	-1, -1, 208, 190,
	WC_TOWN_DIRECTORY,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_town_directory_widgets,
	TownDirectoryWndProc
};


void ShowTownDirectory()
{
	Window *w;

	w = AllocateWindowDescFront(&_town_directory_desc, 0);
	if (w) {
		w->vscroll.cap = 16;
	}
}


