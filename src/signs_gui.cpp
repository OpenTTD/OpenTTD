/* $Id$ */

/** @file signs_gui.cpp The GUI for signs. */

#include "stdafx.h"
#include "openttd.h"
#include "gui.h"
#include "textbuf_gui.h"
#include "window_gui.h"
#include "player_gui.h"
#include "signs_base.h"
#include "signs_func.h"
#include "debug.h"
#include "variables.h"
#include "command_func.h"
#include "strings_func.h"
#include "core/alloc_func.hpp"
#include "window_func.h"
#include "map_func.h"
#include "gfx_func.h"
#include "viewport_func.h"

#include "table/strings.h"
#include "table/sprites.h"

static const Sign **_sign_sort;
static uint _num_sign_sort;

static char _bufcache[64];
static const Sign *_last_sign;

static int CDECL SignNameSorter(const void *a, const void *b)
{
	const Sign *sign0 = *(const Sign**)a;
	const Sign *sign1 = *(const Sign**)b;
	char buf1[64];

	SetDParam(0, sign0->index);
	GetString(buf1, STR_SIGN_NAME, lastof(buf1));

	if (sign1 != _last_sign) {
		_last_sign = sign1;
		SetDParam(0, sign1->index);
		GetString(_bufcache, STR_SIGN_NAME, lastof(_bufcache));
	}

	return strcmp(buf1, _bufcache); // sort by name
}

static void GlobalSortSignList()
{
	const Sign *si;
	uint n = 0;

	/* Create array for sorting */
	_sign_sort = ReallocT(_sign_sort, GetMaxSignIndex() + 1);

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
			if (_sign_sort_dirty) GlobalSortSignList();

			SetVScrollCount(w, _num_sign_sort);

			SetDParam(0, w->vscroll.count);
			DrawWindowWidgets(w);

			/* No signs? */
			int y = 16; // offset from top of widget
			if (w->vscroll.count == 0) {
				DrawString(2, y, STR_304A_NONE, TC_FROMSTRING);
				return;
			}

			/* Start drawing the signs */
			for (uint16 i = w->vscroll.pos; i < w->vscroll.cap + w->vscroll.pos && i < w->vscroll.count; i++) {
				const Sign *si = _sign_sort[i];

				if (si->owner != OWNER_NONE) DrawPlayerIcon(si->owner, 4, y + 1);

				SetDParam(0, si->index);
				DrawString(22, y, STR_SIGN_NAME, TC_YELLOW);
				y += 10;
			}
		} break;

		case WE_CLICK:
			if (e->we.click.widget == 3) {
				uint32 id_v = (e->we.click.pt.y - 15) / 10;

				if (id_v >= w->vscroll.cap) return;
				id_v += w->vscroll.pos;
				if (id_v >= w->vscroll.count) return;

				const Sign *si = _sign_sort[id_v];
				ScrollMainWindowToTile(TileVirtXY(si->x, si->y));
			}
			break;

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
	WDP_AUTO, WDP_AUTO, 358, 138, 358, 138,
	WC_SIGN_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_sign_list_widget,
	SignListWndProc
};


void ShowSignList()
{
	Window *w = AllocateWindowDescFront(&_sign_list_desc, 0);
	if (w != NULL) {
		w->vscroll.cap = 12;
		w->resize.step_height = 10;
		w->resize.height = w->height - 10 * 7; // minimum if 5 in the list
	}
}

/** Edit sign window stuff */
struct editsign_d : querystr_d {
	SignID cur_sign;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(editsign_d));

enum QueryEditSignWidgets {
	QUERY_EDIT_SIGN_WIDGET_TEXT = 3,
	QUERY_EDIT_SIGN_WIDGET_OK,
	QUERY_EDIT_SIGN_WIDGET_CANCEL,
	QUERY_EDIT_SIGN_WIDGET_DELETE,
	QUERY_EDIT_SIGN_WIDGET_PREVIOUS = QUERY_EDIT_SIGN_WIDGET_DELETE + 2,
	QUERY_EDIT_SIGN_WIDGET_NEXT,
};

static void UpdateSignEditWindow(Window *w, const Sign *si)
{
	/* Display an empty string when the sign hasnt been edited yet */
	if (si->name != NULL) {
		SetDParam(0, si->index);
		GetString(_edit_str_buf, STR_SIGN_NAME, lastof(_edit_str_buf));
	} else {
		GetString(_edit_str_buf, STR_EMPTY, lastof(_edit_str_buf));
	}
	_edit_str_buf[lengthof(_edit_str_buf) - 1] = '\0';

	WP(w, editsign_d).cur_sign = si->index;
	InitializeTextBuffer(&WP(w, querystr_d).text, _edit_str_buf, 31, 255); // Allow 31 characters (including \0)

	w->InvalidateWidget(QUERY_EDIT_SIGN_WIDGET_TEXT);
}

static void RenameSign(SignID index, const char *text)
{
	_cmd_text = text;
	DoCommandP(0, index, 0, NULL, CMD_RENAME_SIGN | CMD_MSG(STR_280C_CAN_T_CHANGE_SIGN_NAME));
}

static void QuerySignEditWndProc(Window *w, WindowEvent *e)
{
	editsign_d *qs = &WP(w, editsign_d);
	Sign       *si;
	uint       sign_index = 0;

	switch (e->event) {
		case WE_CREATE:
			SetBit(_no_scroll, SCROLL_EDIT);
			break;

		case WE_PAINT:
			SetDParam(0, qs->caption);
			DrawWindowWidgets(w);
			DrawEditBox(w, qs, QUERY_EDIT_SIGN_WIDGET_TEXT);
			break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case QUERY_EDIT_SIGN_WIDGET_PREVIOUS:
					if (_sign_sort_dirty) GlobalSortSignList();
					sign_index = _sign_sort[_num_sign_sort - 1]->index;
					for (uint i = 1; i < _num_sign_sort; i++) {
						if (qs->cur_sign == _sign_sort[i]->index) {
							sign_index = _sign_sort[i - 1]->index;
							break;
						}
					}
					si = GetSign(sign_index);

					/* Scroll to sign and reopen window */
					ScrollMainWindowToTile(TileVirtXY(si->x, si->y));
					UpdateSignEditWindow(w, si);
					break;

				case QUERY_EDIT_SIGN_WIDGET_NEXT:
					if (_sign_sort_dirty) GlobalSortSignList();
					sign_index = _sign_sort[0]->index;
					for (uint i = 0; i < _num_sign_sort - 1; i++) {
						if (qs->cur_sign == _sign_sort[i]->index) {
							sign_index = _sign_sort[i + 1]->index;
							break;
						}
					}
					si = GetSign(sign_index);

					/* Scroll to sign and reopen window */
					ScrollMainWindowToTile(TileVirtXY(si->x, si->y));
					UpdateSignEditWindow(w, si);
					break;

				case QUERY_EDIT_SIGN_WIDGET_TEXT:
					ShowOnScreenKeyboard(w, qs, e->we.click.widget, QUERY_EDIT_SIGN_WIDGET_CANCEL, QUERY_EDIT_SIGN_WIDGET_OK);
					break;

				case QUERY_EDIT_SIGN_WIDGET_DELETE:
					/* Only need to set the buffer to null, the rest is handled as the OK button */
					DeleteTextBufferAll(&qs->text);
					/* FALL THROUGH */

				case QUERY_EDIT_SIGN_WIDGET_OK:
					RenameSign(qs->cur_sign, qs->text.buf);
					/* FALL THROUGH */

				case QUERY_EDIT_SIGN_WIDGET_CANCEL:
					delete w;
					break;
			}
			break;

		case WE_KEYPRESS:
			switch (HandleEditBoxKey(w, qs, QUERY_EDIT_SIGN_WIDGET_TEXT, e)) {
				case 1: // Enter pressed, confirms change
					RenameSign(qs->cur_sign, qs->text.buf);
					/* FALL THROUGH */

				case 2: // ESC pressed, closes window, abandons changes
					delete w;
					break;
			}
			break;

		case WE_MOUSELOOP:
			HandleEditBox(w, qs, QUERY_EDIT_SIGN_WIDGET_TEXT);
			break;

		case WE_DESTROY:
			ClrBit(_no_scroll, SCROLL_EDIT);
			break;
	}
}

static const Widget _query_sign_edit_widgets[] = {
{ WWT_CLOSEBOX, RESIZE_NONE,  14,   0,  10,   0,  13, STR_00C5,          STR_018B_CLOSE_WINDOW},
{  WWT_CAPTION, RESIZE_NONE,  14,  11, 259,   0,  13, STR_012D,          STR_NULL },
{    WWT_PANEL, RESIZE_NONE,  14,   0, 259,  14,  29, STR_NULL,          STR_NULL },
{  WWT_EDITBOX, RESIZE_NONE,  14,   2, 257,  16,  27, STR_SIGN_OSKTITLE, STR_NULL },  // Text field
{  WWT_TEXTBTN, RESIZE_NONE,  14,   0,  60,  30,  41, STR_012F_OK,       STR_NULL },
{  WWT_TEXTBTN, RESIZE_NONE,  14,  61, 120,  30,  41, STR_012E_CANCEL,   STR_NULL },
{  WWT_TEXTBTN, RESIZE_NONE,  14, 121, 180,  30,  41, STR_0290_DELETE,   STR_NULL },
{    WWT_PANEL, RESIZE_NONE,  14, 181, 237,  30,  41, STR_NULL,          STR_NULL },
{  WWT_TEXTBTN, RESIZE_NONE,  14, 238, 248,  30,  41, STR_6819,          STR_PREVIOUS_SIGN_TOOLTIP },
{  WWT_TEXTBTN, RESIZE_NONE,  14, 249, 259,  30,  41, STR_681A,          STR_NEXT_SIGN_TOOLTIP },
{ WIDGETS_END },
};

static const WindowDesc _query_sign_edit_desc = {
	190, 170, 260, 42, 260, 42,
	WC_QUERY_STRING, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_query_sign_edit_widgets,
	QuerySignEditWndProc
};

void ShowRenameSignWindow(const Sign *si)
{
	/* Delete all other edit windows and the save window */
	DeleteWindowById(WC_QUERY_STRING, 0);
	DeleteWindowById(WC_SAVELOAD, 0);

	Window *w = AllocateWindowDesc(&_query_sign_edit_desc);

	WP(w, editsign_d).caption = STR_280B_EDIT_SIGN_TEXT;
	WP(w, editsign_d).afilter = CS_ALPHANUMERAL;
	w->LowerWidget(QUERY_EDIT_SIGN_WIDGET_TEXT);

	UpdateSignEditWindow(w, si);
}
