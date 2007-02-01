/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "functions.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "player.h"
#include "signs.h"
#include "strings.h"
#include "debug.h"
#include "variables.h"
#include "helpers.hpp"

static const Sign **_sign_sort;
static uint _num_sign_sort;

static char _bufcache[64];
static const Sign *_last_sign;

static int CDECL SignNameSorter(const void *a, const void *b)
{
	const Sign *sign0 = *(const Sign**)a;
	const Sign *sign1 = *(const Sign**)b;
	char buf1[64];

	GetString(buf1, sign0->str, lastof(buf1));

	if (sign1 != _last_sign) {
		_last_sign = sign1;
		GetString(_bufcache, sign1->str, lastof(_bufcache));
	}

	return strcmp(buf1, _bufcache); // sort by name
}

static void GlobalSortSignList(void)
{
	const Sign *si;
	uint n = 0;

	/* Create array for sorting */
	_sign_sort = ReallocT(_sign_sort, GetMaxSignIndex() + 1);
	if (_sign_sort == NULL) error("Could not allocate memory for the sign-sorting-list");

	FOR_ALL_SIGNS(si) _sign_sort[n++] = si;
	_num_sign_sort = n;

	qsort((void*)_sign_sort, n, sizeof(_sign_sort[0]), SignNameSorter);

	_sign_sort_dirty = false;

	DEBUG(misc, 3, "Resorting global signs list");
}

static void SignListWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		int y = 16; // offset from top of widget

		if (_sign_sort_dirty)
			GlobalSortSignList();

		SetVScrollCount(w, _num_sign_sort);

		SetDParam(0, w->vscroll.count);
		DrawWindowWidgets(w);

		/* No signs? */
		if (w->vscroll.count == 0) {
			DrawString(2, y, STR_304A_NONE, 0);
			return;
		}

		{
			uint16 i;

			/* Start drawing the signs */
			for (i = w->vscroll.pos; i < w->vscroll.cap + w->vscroll.pos && i < w->vscroll.count; i++) {
				const Sign *si = _sign_sort[i];

				if (si->owner != OWNER_NONE)
					DrawPlayerIcon(si->owner, 4, y + 1);

				DrawString(22, y, si->str, 8);
				y += 10;
			}
		}
	} break;

	case WE_CLICK: {
		switch (e->we.click.widget) {
		case 3: {
			uint32 id_v = (e->we.click.pt.y - 15) / 10;
			const Sign *si;

			if (id_v >= w->vscroll.cap)
				return;

			id_v += w->vscroll.pos;

			if (id_v >= w->vscroll.count)
				return;

			si = _sign_sort[id_v];
			ScrollMainWindowToTile(TileVirtXY(si->x, si->y));
		} break;
		}
	} break;

	case WE_RESIZE:
		w->vscroll.cap += e->we.sizing.diff.y / 10;
		break;
	}
}

static const Widget _sign_list_widget[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,              STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   345,     0,    13, STR_SIGN_LIST_CAPTION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   346,   357,     0,    13, 0x0,                   STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_RB,    14,     0,   345,    14,   137, 0x0,                   STR_NULL},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   346,   357,    14,   125, 0x0,                   STR_0190_SCROLL_BAR_SCROLLS_LIST},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   346,   357,   126,   137, 0x0,                   STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _sign_list_desc = {
	WDP_AUTO, WDP_AUTO, 358, 138,
	WC_SIGN_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_sign_list_widget,
	SignListWndProc
};


void ShowSignList(void)
{
	Window *w;

	w = AllocateWindowDescFront(&_sign_list_desc, 0);
	if (w != NULL) {
		w->vscroll.cap = 12;
		w->resize.step_height = 10;
		w->resize.height = w->height - 10 * 7; // minimum if 5 in the list
	}
}
