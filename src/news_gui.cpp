/* $Id$ */

/** @file news_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "strings.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "news.h"
#include "vehicle.h"
#include "sound.h"
#include "variables.h"
#include "date.h"
#include "string.h"

/* News system
 * News system is realized as a FIFO queue (in an array)
 * The positions in the queue can't be rearranged, we only access
 * the array elements through pointers to the elements. Once the
 * array is full, the oldest entry (_oldest_news) is being overwritten
 * by the newest (_latest news).
 *
 * oldest                   current   lastest
 *  |                          |         |
 * [O------------F-------------C---------L           ]
 *               |
 *            forced
 *
 * Of course by using an array we can have situations like
 *
 * [----L          O-----F---------C-----------------]
 * This is where we have wrapped around the array and have
 * (MAX_NEWS - O) + L news items
 */

#define MAX_NEWS 30
#define NB_WIDG_PER_SETTING 4

typedef byte NewsID;
#define INVALID_NEWS 255

static NewsItem _news_items[MAX_NEWS];
static NewsID _current_news = INVALID_NEWS; // points to news item that should be shown next
static NewsID _oldest_news = 0;    // points to first item in fifo queue
static NewsID _latest_news = INVALID_NEWS;  // points to last item in fifo queue
/* if the message being shown was forced by the user, its index is stored in
 * _forced_news. forced_news is INVALID_NEWS otherwise.
 * (Users can force messages through history or "last message") */
static NewsID _forced_news = INVALID_NEWS;

static byte _total_news = 0; // total news count

void DrawNewsNewVehicleAvail(Window *w);
void DrawNewsBankrupcy(Window *w);
static void MoveToNextItem();

StringID GetNewsStringNewVehicleAvail(const NewsItem *ni);
StringID GetNewsStringBankrupcy(const NewsItem *ni);

static DrawNewsCallbackProc * const _draw_news_callback[] = {
	DrawNewsNewVehicleAvail,  /* DNC_VEHICLEAVAIL */
	DrawNewsBankrupcy,        /* DNC_BANKRUPCY */
};

extern GetNewsStringCallbackProc * const _get_news_string_callback[];
GetNewsStringCallbackProc * const _get_news_string_callback[] = {
	GetNewsStringNewVehicleAvail,  /* DNC_VEHICLEAVAIL */
	GetNewsStringBankrupcy,        /* DNC_BANKRUPCY */
};

void InitNewsItemStructs()
{
	memset(_news_items, 0, sizeof(_news_items));
	_current_news = INVALID_NEWS;
	_oldest_news = 0;
	_latest_news = INVALID_NEWS;
	_forced_news = INVALID_NEWS;
	_total_news = 0;
}

void DrawNewsBorder(const Window *w)
{
	int left = 0;
	int right = w->width - 1;
	int top = 0;
	int bottom = w->height - 1;

	GfxFillRect(left, top, right, bottom, 0xF);

	GfxFillRect(left, top, left, bottom, 0xD7);
	GfxFillRect(right, top, right, bottom, 0xD7);
	GfxFillRect(left, top, right, top, 0xD7);
	GfxFillRect(left, bottom, right, bottom, 0xD7);

	DrawString(left + 2, top + 1, STR_00C6, 0);
}

static void NewsWindowProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE: { /* If chatbar is open at creation time, we need to go above it */
		const Window *w1 = FindWindowById(WC_SEND_NETWORK_MSG, 0);
		w->message.msg = (w1 != NULL) ? w1->height : 0;
	} break;

	case WE_PAINT: {
		const NewsItem *ni = WP(w, news_d).ni;
		ViewPort *vp;

		switch (ni->display_mode) {
			case NM_NORMAL:
			case NM_THIN: {
				DrawNewsBorder(w);

				DrawString(2, 1, STR_00C6, 0);

				SetDParam(0, ni->date);
				DrawStringRightAligned(428, 1, STR_01FF, 0);

				if (!(ni->flags & NF_VIEWPORT)) {
					COPY_IN_DPARAM(0, ni->params, lengthof(ni->params));
					DrawStringMultiCenter(215, ni->display_mode == NM_NORMAL ? 76 : 56,
						ni->string_id, w->width - 4);
				} else {
					byte bk = _display_opt;
					_display_opt &= ~DO_TRANS_BUILDINGS;
					DrawWindowViewport(w);
					_display_opt = bk;

					/* Shade the viewport into gray, or color*/
					vp = w->viewport;
					GfxFillRect(vp->left - w->left, vp->top - w->top,
						vp->left - w->left + vp->width - 1, vp->top - w->top + vp->height - 1,
						(ni->flags & NF_INCOLOR ? PALETTE_TO_TRANSPARENT : PALETTE_TO_STRUCT_GREY) | (1 << USE_COLORTABLE)
					);

					COPY_IN_DPARAM(0, ni->params, lengthof(ni->params));
					DrawStringMultiCenter(w->width / 2, 20, ni->string_id, w->width - 4);
				}
				break;
			}

			case NM_CALLBACK: {
				_draw_news_callback[ni->callback](w);
				break;
			}

			default: {
				DrawWindowWidgets(w);
				if (!(ni->flags & NF_VIEWPORT)) {
					COPY_IN_DPARAM(0, ni->params, lengthof(ni->params));
					DrawStringMultiCenter(140, 38, ni->string_id, 276);
				} else {
					DrawWindowViewport(w);
					COPY_IN_DPARAM(0, ni->params, lengthof(ni->params));
					DrawStringMultiCenter(w->width / 2, w->height - 16, ni->string_id, w->width - 4);
				}
				break;
			}
		}
	} break;

	case WE_CLICK: {
		switch (e->we.click.widget) {
		case 1: {
			NewsItem *ni = WP(w, news_d).ni;
			DeleteWindow(w);
			ni->duration = 0;
			_forced_news = INVALID_NEWS;
		} break;
		case 0: {
			NewsItem *ni = WP(w, news_d).ni;
			if (ni->flags & NF_VEHICLE) {
				Vehicle *v = GetVehicle(ni->data_a);
				ScrollMainWindowTo(v->x_pos, v->y_pos);
			} else if (ni->flags & NF_TILE) {
				if (!ScrollMainWindowToTile(ni->data_a) && ni->data_b != 0)
					ScrollMainWindowToTile(ni->data_b);
			}
		} break;
		}
	} break;

	case WE_KEYPRESS:
		if (e->we.keypress.keycode == WKC_SPACE) {
			// Don't continue.
			e->we.keypress.cont = false;
			DeleteWindow(w);
		}
		break;

	case WE_MESSAGE: /* The chatbar has notified us that is was either created or closed */
		switch (e->we.message.msg) {
			case WE_CREATE: w->message.msg = e->we.message.wparam; break;
			case WE_DESTROY: w->message.msg = 0; break;
		}
		break;

	case WE_TICK: { /* Scroll up newsmessages from the bottom in steps of 4 pixels */
		int diff;
		int y = max(w->top - 4, _screen.height - w->height - 12 - w->message.msg);
		if (y == w->top) return;

		if (w->viewport != NULL)
			w->viewport->top += y - w->top;

		diff = delta(w->top, y);
		w->top = y;

		SetDirtyBlocks(w->left, w->top - diff, w->left + w->width, w->top + w->height);
	} break;
	}
}

/** Return the correct index in the pseudo-fifo
 * queue and deals with overflows when increasing the index */
static inline NewsID increaseIndex(NewsID i)
{
	assert(i != INVALID_NEWS);
	return (i + 1) % MAX_NEWS;
}

/** Return the correct index in the pseudo-fifo
 * queue and deals with overflows when decreasing the index */
static inline NewsID decreaseIndex(NewsID i)
{
	assert(i != INVALID_NEWS);
	return (i + MAX_NEWS - 1) % MAX_NEWS;
}

/** Add a new newsitem to be shown.
 * @param string String to display, can have special values based on parameter 'flags'
 * @param flags various control bits that will show various news-types. See macro NEWS_FLAGS()
 * @param data_a news-specific value based on news type
 * @param data_b news-specific value based on news type
 * @note flags exists of 4 byte-sized extra parameters.<br/>
 * 1.  0 -  7 display_mode, any of the NewsMode enums (NM_)<br/>
 * 2.  8 - 15 news flags, any of the NewsFlags enums (NF_) NF_INCOLOR are set automatically if needed<br/>
 * 3. 16 - 23 news category, any of the NewsType enums (NT_)<br/>
 * 4. 24 - 31 news callback function, any of the NewsCallback enums (DNC_)<br/>
 * If the display mode is NM_CALLBACK special news is shown and parameter
 * stringid has a special meaning.<br/>
 * DNC_TRAINAVAIL, DNC_ROADAVAIL, DNC_SHIPAVAIL, DNC_AIRCRAFTAVAIL: StringID is
 * the index of the engine that is shown<br/>
 * DNC_BANKRUPCY: bytes 0-3 of StringID contains the player that is in trouble,
 * and 4-7 contains what kind of bankrupcy message is shown, NewsBankrupcy enum (NB_)<br/>
 * @see NewsMode
 * @see NewsFlags
 * @see NewsType
 * @see NewsCallback */
void AddNewsItem(StringID string, uint32 flags, uint data_a, uint data_b)
{
	NewsID l_news;

	if (_game_mode == GM_MENU) return;

	// check the rare case that the oldest (to be overwritten) news item is open
	if (_total_news == MAX_NEWS && (_oldest_news == _current_news || _oldest_news == _forced_news))
		MoveToNextItem();

	if (_total_news < MAX_NEWS) _total_news++;

	/* Increase _latest_news. If we have no news yet, use _oldest news as an
	 * index. We cannot use 0 as _oldest_news can jump around due to
	 * DeleteVehicleNews */
	l_news = _latest_news;
	_latest_news = (_latest_news == INVALID_NEWS) ? _oldest_news : increaseIndex(_latest_news);

	/* If the fifo-buffer is full, overwrite the oldest entry */
	if (l_news != INVALID_NEWS && _latest_news == _oldest_news) {
		assert(_total_news == MAX_NEWS);
		_oldest_news = increaseIndex(_oldest_news);
	}

	/*DEBUG(misc, 0, "+cur %3d, old %2d, lat %3d, for %3d, tot %2d",
	  _current_news, _oldest_news, _latest_news, _forced_news, _total_news);*/

	{ /* Add news to _latest_news */
		Window *w;
		NewsItem *ni = &_news_items[_latest_news];
		memset(ni, 0, sizeof(*ni));

		ni->string_id = string;
		ni->display_mode = (byte)flags;
		ni->flags = (byte)(flags >> 8);

		// show this news message in color?
		if (_cur_year >= _patches.colored_news_year) ni->flags |= NF_INCOLOR;

		ni->type = (byte)(flags >> 16);
		ni->callback = (byte)(flags >> 24);
		ni->data_a = data_a;
		ni->data_b = data_b;
		ni->date = _date;
		COPY_OUT_DPARAM(ni->params, 0, lengthof(ni->params));

		w = FindWindowById(WC_MESSAGE_HISTORY, 0);
		if (w == NULL) return;
		SetWindowDirty(w);
		w->vscroll.count = _total_news;
	}
}


/* Don't show item if it's older than x days, corresponds with NewsType in news.h */
static const byte _news_items_age[] = {60, 60, 90, 60, 90, 30, 150, 30, 90, 180};

static const Widget _news_type13_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,    15,     0,   429,     0,   169, 0x0, STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    15,     0,    10,     0,    11, 0x0, STR_NULL},
{   WIDGETS_END},
};

static WindowDesc _news_type13_desc = {
	WDP_CENTER, 476, 430, 170,
	WC_NEWS_WINDOW, WC_NONE,
	WDF_DEF_WIDGET,
	_news_type13_widgets,
	NewsWindowProc
};

static const Widget _news_type2_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,    15,     0,   429,     0,   129, 0x0, STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    15,     0,    10,     0,    11, 0x0, STR_NULL},
{   WIDGETS_END},
};

static WindowDesc _news_type2_desc = {
	WDP_CENTER, 476, 430, 130,
	WC_NEWS_WINDOW, WC_NONE,
	WDF_DEF_WIDGET,
	_news_type2_widgets,
	NewsWindowProc
};

static const Widget _news_type0_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,     5,     0,   279,    14,    86, 0x0,              STR_NULL},
{   WWT_CLOSEBOX,   RESIZE_NONE,     5,     0,    10,     0,    13, STR_00C5,         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     5,    11,   279,     0,    13, STR_012C_MESSAGE, STR_NULL},
{      WWT_INSET,   RESIZE_NONE,     5,     2,   277,    16,    64, 0x0,              STR_NULL},
{   WIDGETS_END},
};

static WindowDesc _news_type0_desc = {
	WDP_CENTER, 476, 280, 87,
	WC_NEWS_WINDOW, WC_NONE,
	WDF_DEF_WIDGET,
	_news_type0_widgets,
	NewsWindowProc
};

static const SoundFx _news_sounds[NT_END] = {
	SND_1D_APPLAUSE,
	SND_1D_APPLAUSE,
	SND_BEGIN,
	SND_BEGIN,
	SND_BEGIN,
	SND_BEGIN,
	SND_BEGIN,
	SND_1E_OOOOH,
	SND_BEGIN,
	SND_BEGIN,
	SND_BEGIN,
};

const char *_news_display_name[NT_END] = {
	"arrival_player",
	"arrival_other",
	"accident",
	"company_info",
	"openclose",
	"economy",
	"advice",
	"new_vehicles",
	"acceptance",
	"subsidies",
	"general",
};

/** Get the value of an item of the news-display settings. This is
 * a little tricky since on/off/summary must use 2 bits to store the value
 * @param item the item whose value is requested
 * @return return the found value which is between 0-2
 */
static inline byte GetNewsDisplayValue(byte item)
{
	assert(item < NT_END && GB(_news_display_opt, item * 2, 2) <= 2);
	return GB(_news_display_opt, item * 2, 2);
}

/** Set the value of an item in the news-display settings. This is
 * a little tricky since on/off/summary must use 2 bits to store the value
 * @param item the item whose value is being set
 * @param val new value
 */
static inline void SetNewsDisplayValue(byte item, byte val)
{
	assert(item < NT_END && val <= 2);
	SB(_news_display_opt, item * 2, 2, val);
}

// open up an own newspaper window for the news item
static void ShowNewspaper(NewsItem *ni)
{
	Window *w;
	SoundFx sound;
	int top;
	ni->flags &= ~NF_FORCE_BIG;
	ni->duration = 555;

	sound = _news_sounds[ni->type];
	if (sound != 0) SndPlayFx(sound);

	top = _screen.height;
	switch (ni->display_mode) {
		case NM_NORMAL:
		case NM_CALLBACK: {
			_news_type13_desc.top = top;
			w = AllocateWindowDesc(&_news_type13_desc);
			if (ni->flags & NF_VIEWPORT)
				AssignWindowViewport(w, 2, 58, 0x1AA, 0x6E,
					ni->data_a | (ni->flags & NF_VEHICLE ? 0x80000000 : 0), 0);
			break;
		}

		case NM_THIN: {
			_news_type2_desc.top = top;
			w = AllocateWindowDesc(&_news_type2_desc);
			if (ni->flags & NF_VIEWPORT)
				AssignWindowViewport(w, 2, 58, 0x1AA, 0x46,
					ni->data_a | (ni->flags & NF_VEHICLE ? 0x80000000 : 0), 0);
			break;
		}

		default: {
			_news_type0_desc.top = top;
			w = AllocateWindowDesc(&_news_type0_desc);
			if (ni->flags & NF_VIEWPORT)
				AssignWindowViewport(w, 3, 17, 0x112, 0x2F,
					ni->data_a | (ni->flags & NF_VEHICLE ? 0x80000000 : 0), 0);
			break;
		}
	}

	/*DEBUG(misc, 0, " cur %3d, old %2d, lat %3d, for %3d, tot %2d",
	  _current_news, _oldest_news, _latest_news, _forced_news, _total_news);*/

	WP(w, news_d).ni = &_news_items[_forced_news == INVALID_NEWS ? _current_news : _forced_news];
	w->flags4 |= WF_DISABLE_VP_SCROLL;
}

// show news item in the ticker
static void ShowTicker(const NewsItem *ni)
{
	Window *w;

	if (_news_ticker_sound) SndPlayFx(SND_16_MORSE);

	_statusbar_news_item = *ni;
	w = FindWindowById(WC_STATUS_BAR, 0);
	if (w != NULL) WP(w, def_d).data_1 = 360;
}


// Are we ready to show another news item?
// Only if nothing is in the newsticker and no newspaper is displayed
static bool ReadyForNextItem()
{
	const Window *w;
	NewsID item = (_forced_news == INVALID_NEWS) ? _current_news : _forced_news;
	NewsItem *ni;

	if (item >= MAX_NEWS) return true;
	ni = &_news_items[item];

	// Ticker message
	// Check if the status bar message is still being displayed?
	w = FindWindowById(WC_STATUS_BAR, 0);
	if (w != NULL && WP(w, const def_d).data_1 > -1280) return false;

	// Newspaper message, decrement duration counter
	if (ni->duration != 0) ni->duration--;

	// neither newsticker nor newspaper are running
	return (ni->duration == 0 || FindWindowById(WC_NEWS_WINDOW, 0) == NULL);
}

static void MoveToNextItem()
{
	DeleteWindowById(WC_NEWS_WINDOW, 0);
	_forced_news = INVALID_NEWS;

	// if we're not at the last item, then move on
	if (_current_news != _latest_news) {
		NewsItem *ni;

		_current_news = (_current_news == INVALID_NEWS) ? _oldest_news : increaseIndex(_current_news);
		ni = &_news_items[_current_news];

		// check the date, don't show too old items
		if (_date - _news_items_age[ni->type] > ni->date) return;

		switch (GetNewsDisplayValue(ni->type)) {
		case 0: { /* Off - show nothing only a small reminder in the status bar */
			Window *w = FindWindowById(WC_STATUS_BAR, 0);

			if (w != NULL) {
				WP(w, def_d).data_2 = 91;
				SetWindowDirty(w);
			}
			break;
		}

		case 1: /* Summary - show ticker, but if forced big, cascade to full */
			if (!(ni->flags & NF_FORCE_BIG)) {
				ShowTicker(ni);
				break;
			}
			/* Fallthrough */

		case 2: /* Full - show newspaper*/
			ShowNewspaper(ni);
			break;
		}
	}
}

void NewsLoop()
{
	// no news item yet
	if (_total_news == 0) return;

	if (ReadyForNextItem()) MoveToNextItem();
}

/* Do a forced show of a specific message */
static void ShowNewsMessage(NewsID i)
{
	if (_total_news == 0) return;

	// Delete the news window
	DeleteWindowById(WC_NEWS_WINDOW, 0);

	// setup forced news item
	_forced_news = i;

	if (_forced_news != INVALID_NEWS) {
		NewsItem *ni = &_news_items[_forced_news];
		ni->duration = 555;
		ni->flags |= NF_FORCE_BIG;
		DeleteWindowById(WC_NEWS_WINDOW, 0);
		ShowNewspaper(ni);
	}
}

void ShowLastNewsMessage()
{
	if (_forced_news == INVALID_NEWS) {
		/* Not forced any news yet, show the current one, unless a news window is
		 * open (which can only be the current one), then show the previous item */
		const Window *w = FindWindowById(WC_NEWS_WINDOW, 0);
		ShowNewsMessage((w == NULL || (_current_news == _oldest_news)) ? _current_news : decreaseIndex(_current_news));
	} else if (_forced_news == _oldest_news) {
		/* We have reached the oldest news, start anew with the latest */
		ShowNewsMessage(_latest_news);
	} else {
		/* 'Scrolling' through news history show each one in turn */
		ShowNewsMessage(decreaseIndex(_forced_news));
	}
}


/* return news by number, with 0 being the most
 * recent news. Returns INVALID_NEWS if end of queue reached. */
static NewsID getNews(NewsID i)
{
	if (i >= _total_news) return INVALID_NEWS;

	if (_latest_news < i) {
		i = _latest_news + MAX_NEWS - i;
	} else {
		i = _latest_news - i;
	}

	i %= MAX_NEWS;
	return i;
}

/** Draw an unformatted news message truncated to a maximum length. If
 * length exceeds maximum length it will be postfixed by '...'
 * @param x,y position of the string
 * @param color the color the string will be shown in
 * @param *ni NewsItem being printed
 * @param maxw maximum width of string in pixels
 */
static void DrawNewsString(int x, int y, uint16 color, const NewsItem *ni, uint maxw)
{
	char buffer[512], buffer2[512];
	const char *ptr;
	char *dest;
	StringID str;

	if (ni->display_mode == 3) {
		str = _get_news_string_callback[ni->callback](ni);
	} else {
		COPY_IN_DPARAM(0, ni->params, lengthof(ni->params));
		str = ni->string_id;
	}

	GetString(buffer, str, lastof(buffer));
	/* Copy the just gotten string to another buffer to remove any formatting
	 * from it such as big fonts, etc. */
	ptr  = buffer;
	dest = buffer2;
	for (;;) {
		WChar c = Utf8Consume(&ptr);
		if (c == 0) break;
		if (c == '\r') {
			dest[0] = dest[1] = dest[2] = dest[3] = ' ';
			dest += 4;
		} else if (IsPrintable(c)) {
			dest += Utf8Encode(dest, c);
		}
	}

	*dest = '\0';
	/* Truncate and show string; postfixed by '...' if neccessary */
	DoDrawStringTruncated(buffer2, x, y, color, maxw);
}


static void MessageHistoryWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		int y = 19;
		NewsID p, show;

		SetVScrollCount(w, _total_news);
		DrawWindowWidgets(w);

		if (_total_news == 0) break;
		show = min(_total_news, w->vscroll.cap);

		for (p = w->vscroll.pos; p < w->vscroll.pos + show; p++) {
			// get news in correct order
			const NewsItem *ni = &_news_items[getNews(p)];

			SetDParam(0, ni->date);
			DrawString(4, y, STR_SHORT_DATE, 12);

			DrawNewsString(82, y, 12, ni, w->width - 95);
			y += 12;
		}
		break;
	}

	case WE_CLICK:
		switch (e->we.click.widget) {
		case 3: {
			int y = (e->we.click.pt.y - 19) / 12;
			NewsID p = getNews(y + w->vscroll.pos);

			if (p == INVALID_NEWS) break;

			ShowNewsMessage(p);
			break;
		}
		}
		break;

	case WE_RESIZE:
		w->vscroll.cap += e->we.sizing.diff.y / 12;
		break;
	}
}

static const Widget _message_history_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    13,     0,    10,     0,    13, STR_00C5,            STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    13,    11,   387,     0,    13, STR_MESSAGE_HISTORY, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    13,   388,   399,     0,    13, 0x0,                 STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_RB,    13,     0,   387,    14,   139, 0x0,                 STR_MESSAGE_HISTORY_TIP},
{  WWT_SCROLLBAR,    RESIZE_LRB,    13,   388,   399,    14,   127, 0x0,                 STR_0190_SCROLL_BAR_SCROLLS_LIST},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    13,   388,   399,   128,   139, 0x0,                 STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _message_history_desc = {
	240, 22, 400, 140,
	WC_MESSAGE_HISTORY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_message_history_widgets,
	MessageHistoryWndProc
};

void ShowMessageHistory()
{
	Window *w;

	DeleteWindowById(WC_MESSAGE_HISTORY, 0);
	w = AllocateWindowDesc(&_message_history_desc);

	if (w != NULL) {
		w->vscroll.cap = 10;
		w->vscroll.count = _total_news;
		w->resize.step_height = 12;
		w->resize.height = w->height - 12 * 6; // minimum of 4 items in the list, each item 12 high
		w->resize.step_width = 1;
		w->resize.width = 200; // can't make window any smaller than 200 pixel
		SetWindowDirty(w);
	}
}


enum {
	WIDGET_NEWSOPT_BTN_SUMMARY  = 4,  ///< Button that adjusts at once the level for all settings
	WIDGET_NEWSOPT_DROP_SUMMARY,      ///< Drop down button for same upper button
	WIDGET_NEWSOPT_SOUNDTICKER  = 7,  ///< Button activating sound on events
	WIDGET_NEWSOPT_START_OPTION = 9,  ///< First widget that is part of a group [<] .. [.]
};

/** Setup the disabled/enabled buttons in the message window
 * If the value is 'off' disable the [<] widget, and enable the [>] one
 * Same-wise for all the others. Starting value of 4 is the first widget
 * group. These are grouped as [<][>] .. [<][>], etc.
 * @param w Window been used
 * @param value to set in the widget
 * @param element index of the group of widget to set
 */
static void SetMessageButtonStates(Window *w, byte value, int element)
{
	element *= NB_WIDG_PER_SETTING;

	SetWindowWidgetDisabledState(w, element + WIDGET_NEWSOPT_START_OPTION, value == 0);
	SetWindowWidgetDisabledState(w, element + WIDGET_NEWSOPT_START_OPTION + 2, value == 2);
}

/**
 * Event handler of the Message Options window
 * @param w window pointer
 * @param e event been triggered
 */
static void MessageOptionsWndProc(Window *w, WindowEvent *e)
{
	static const StringID message_opt[] = {STR_OFF, STR_SUMMARY, STR_FULL, INVALID_STRING_ID};

	/* WP(w, def_d).data_1 stores state of the ALL on/off/summary button */
	switch (e->event) {
		case WE_CREATE: {
			uint32 val = _news_display_opt;
			int i;
			WP(w, def_d).data_1 = 0;

			/* Set up the initial disabled buttons in the case of 'off' or 'full' */
			for (i = 0; i < NT_END; i++, val >>= 2) SetMessageButtonStates(w, val & 0x3, i);
		} break;

		case WE_PAINT: {
			uint32 val = _news_display_opt;
			int i, y;

			if (_news_ticker_sound) LowerWindowWidget(w, WIDGET_NEWSOPT_SOUNDTICKER);
			DrawWindowWidgets(w);

			/* Draw the string of each setting on each button. */
			for (i = 0, y = 26; i < NT_END; i++, y += 12, val >>= 2) {
				/* 51 comes from 13 + 89 (left and right of the button)+1, shiefted by one as to get division,
				 * which will give centered position */
				DrawStringCentered(51, y + 1, message_opt[val & 0x3], 0x10);
			}

			/* Draw the general bottom button string as well */
			DrawStringCentered(51, y + 10, message_opt[WP(w, def_d).data_1], 0x10);
		} break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case WIDGET_NEWSOPT_BTN_SUMMARY:
				case WIDGET_NEWSOPT_DROP_SUMMARY: // Dropdown menu for all settings
					ShowDropDownMenu(w, message_opt, WP(w, def_d).data_1, WIDGET_NEWSOPT_DROP_SUMMARY, 0, 0);
					break;

				case WIDGET_NEWSOPT_SOUNDTICKER: // Change ticker sound on/off
					_news_ticker_sound ^= 1;
					ToggleWidgetLoweredState(w, e->we.click.widget);
					InvalidateWidget(w, e->we.click.widget);
					break;

				default: { // Clicked on the [<] .. [>] widgets
					int wid = e->we.click.widget - WIDGET_NEWSOPT_START_OPTION;
					if (wid >= 0 && wid < (NB_WIDG_PER_SETTING * NT_END)) {
						int element = wid / NB_WIDG_PER_SETTING;
						byte val = (GetNewsDisplayValue(element) + ((wid % NB_WIDG_PER_SETTING) ? 1 : -1)) % 3;

						SetMessageButtonStates(w, val, element);
						SetNewsDisplayValue(element, val);
						SetWindowDirty(w);
					}
				} break;
			} break;

		case WE_DROPDOWN_SELECT: { // Select all settings for newsmessages
			int i;

			WP(w, def_d).data_1 = e->we.dropdown.index;

			for (i = 0; i < NT_END; i++) {
				SetMessageButtonStates(w, e->we.dropdown.index, i);
				SetNewsDisplayValue(i, e->we.dropdown.index);
			}
			SetWindowDirty(w);
		} break;
	}
}

static const Widget _message_options_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    13,     0,   10,     0,    13, STR_00C5,                              STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    13,    11,  409,     0,    13, STR_0204_MESSAGE_OPTIONS,              STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    13,     0,  409,    14,   196, 0x0,                                   STR_NULL},

/* Text at the top of the main panel, in black */
{      WWT_LABEL,   RESIZE_NONE,    13,     0,  409,    13,    26, STR_0205_MESSAGE_TYPES,                STR_NULL},

/* General drop down and sound button */
{      WWT_PANEL,   RESIZE_NONE,     3,     4,   86,   166,   177, 0x0,                                   STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,     3,    87,   98,   166,   177, STR_0225,                              STR_NULL},
{       WWT_TEXT,   RESIZE_NONE,     3,    103, 409,   167,   179, STR_MESSAGES_ALL,                      STR_NULL},

{  WWT_TEXTBTN_2,   RESIZE_NONE,     3,     4,   98,   178,   189, STR_02DB_OFF,                          STR_NULL},
{       WWT_TEXT,   RESIZE_NONE,     3,    103, 409,   179,   191, STR_MESSAGE_SOUND,                     STR_NULL},

/* Each four group is composed of the buttons [<] [..] [>] and the descriptor of the setting */
{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,     4,   12,    26,    37, SPR_ARROW_LEFT,                        STR_HSCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,    13,   89,    26,    37, STR_EMPTY,                             STR_NULL},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,    90,   98,    26,    37, SPR_ARROW_RIGHT,                       STR_HSCROLL_BAR_SCROLLS_LIST},
{       WWT_TEXT,   RESIZE_NONE,     3,    103, 409,    27,    39, STR_0206_ARRIVAL_OF_FIRST_VEHICLE,     STR_NULL},

{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,     4,   12,    38,    49, SPR_ARROW_LEFT,                        STR_HSCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,    13,   89,    38,    49, STR_EMPTY,                             STR_NULL},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,    90,   98,    38,    49, SPR_ARROW_RIGHT,                       STR_HSCROLL_BAR_SCROLLS_LIST},
{       WWT_TEXT,   RESIZE_NONE,     3,    103, 409,    39,    51, STR_0207_ARRIVAL_OF_FIRST_VEHICLE,     STR_NULL},

{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,     4,   12,    50,    61, SPR_ARROW_LEFT,                        STR_HSCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,    13,   89,    50,    61, STR_EMPTY,                             STR_NULL},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,    90,   98,    50,    61, SPR_ARROW_RIGHT,                       STR_HSCROLL_BAR_SCROLLS_LIST},
{       WWT_TEXT,   RESIZE_NONE,     3,    103, 409,    51,    63, STR_0208_ACCIDENTS_DISASTERS,          STR_NULL},

{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,     4,   12,    62,    73, SPR_ARROW_LEFT,                        STR_HSCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,    13,   89,    62,    73, STR_EMPTY,                             STR_NULL},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,    90,   98,    62,    73, SPR_ARROW_RIGHT,                       STR_HSCROLL_BAR_SCROLLS_LIST},
{       WWT_TEXT,   RESIZE_NONE,     3,    103, 409,    63,    75, STR_0209_COMPANY_INFORMATION,          STR_NULL},

{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,     4,   12,    74,    85, SPR_ARROW_LEFT,                        STR_HSCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,    13,   89,    74,    85, STR_EMPTY,                             STR_NULL},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,    90,   98,    74,    85, SPR_ARROW_RIGHT,                       STR_HSCROLL_BAR_SCROLLS_LIST},
{       WWT_TEXT,   RESIZE_NONE,     3,    103, 409,    75,    87, STR_NEWS_OPEN_CLOSE,                   STR_NULL},

{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,     4,   12,    86,    97, SPR_ARROW_LEFT,                        STR_HSCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,    13,   89,    86,    97, STR_EMPTY,                             STR_NULL},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,    90,   98,    86,    97, SPR_ARROW_RIGHT,                       STR_HSCROLL_BAR_SCROLLS_LIST},
{       WWT_TEXT,   RESIZE_NONE,     3,    103, 409,    87,    99, STR_020A_ECONOMY_CHANGES,              STR_NULL},

{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,     4,   12,    98,   109, SPR_ARROW_LEFT,                        STR_HSCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,    13,   89,    98,   109, STR_EMPTY,                             STR_NULL},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,    90,   98,    98,   109, SPR_ARROW_RIGHT,                       STR_HSCROLL_BAR_SCROLLS_LIST},
{       WWT_TEXT,   RESIZE_NONE,     3,    103, 409,    99,   111, STR_020B_ADVICE_INFORMATION_ON_PLAYER, STR_NULL},

{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,     4,   12,   110,   121, SPR_ARROW_LEFT,                        STR_HSCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,    13,   89,   110,   121, STR_EMPTY,                             STR_NULL},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,    90,   98,   110,   121, SPR_ARROW_RIGHT,                       STR_HSCROLL_BAR_SCROLLS_LIST},
{       WWT_TEXT,   RESIZE_NONE,     3,    103, 409,   111,   123, STR_020C_NEW_VEHICLES,                 STR_NULL},

{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,     4,   12,   122,   133, SPR_ARROW_LEFT,                        STR_HSCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,    13,   89,   122,   133, STR_EMPTY,                             STR_NULL},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,    90,   98,   122,   133, SPR_ARROW_RIGHT,                       STR_HSCROLL_BAR_SCROLLS_LIST},
{       WWT_TEXT,   RESIZE_NONE,     3,    103, 409,   123,   135, STR_020D_CHANGES_OF_CARGO_ACCEPTANCE,  STR_NULL},

{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,     4,   12,   134,   145, SPR_ARROW_LEFT,                        STR_HSCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,    13,   89,   134,   145, STR_EMPTY,                             STR_NULL},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,    90,   98,   134,   145, SPR_ARROW_RIGHT,                       STR_HSCROLL_BAR_SCROLLS_LIST},
{       WWT_TEXT,   RESIZE_NONE,     3,    103, 409,   135,   147, STR_020E_SUBSIDIES,                    STR_NULL},

{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,     4,   12,   146,   157, SPR_ARROW_LEFT,                        STR_HSCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     3,    13,   89,   146,   157, STR_EMPTY,                             STR_NULL},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,     3,    90,   98,   146,   157, SPR_ARROW_RIGHT,                       STR_HSCROLL_BAR_SCROLLS_LIST},
{       WWT_TEXT,   RESIZE_NONE,     3,    103, 409,   147,   159, STR_020F_GENERAL_INFORMATION,          STR_NULL},

{   WIDGETS_END},
};

static const WindowDesc _message_options_desc = {
	270, 22, 410, 197,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_message_options_widgets,
	MessageOptionsWndProc
};

void ShowMessageOptions()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	AllocateWindowDesc(&_message_options_desc);
}


void DeleteVehicleNews(VehicleID vid, StringID news)
{
	NewsID n;

	for (n = _oldest_news; _latest_news != INVALID_NEWS; n = increaseIndex(n)) {
		const NewsItem *ni = &_news_items[n];

		if (ni->flags & NF_VEHICLE &&
				ni->data_a == vid &&
				(news == INVALID_STRING_ID || ni->string_id == news)) {
			Window *w;

			/* If we delete a forced news and it is just before the current news
			 * then we need to advance to the next news (if any) */
			if (_forced_news == n) MoveToNextItem();
			if (_forced_news == INVALID_NEWS && _current_news == n) MoveToNextItem();
			_total_news--;

			/* If this is the last news item, invalidate _latest_news */
			if (_total_news == 0) {
				assert(_latest_news == _oldest_news);
				_latest_news = INVALID_NEWS;
			}

			/* Since we only imitate a FIFO removing an arbitrary element does need
			 * some magic. Remove the item by shifting head towards the tail. eg
			 *    oldest    remove  last
			 *        |        |     |
			 * [------O--------n-----L--]
			 * will become (change dramatized to make clear)
			 * [---------O-----------L--]
			 * We also need an update of the current, forced and visible (open window)
			 * news's as this shifting could change the items they were pointing to */
			if (_total_news != 0) {
				w = FindWindowById(WC_NEWS_WINDOW, 0);
				NewsID visible_news = (w != NULL) ? (NewsID)(WP(w, news_d).ni - _news_items) : INVALID_NEWS;

				for (NewsID i = n;; i = decreaseIndex(i)) {
					_news_items[i] = _news_items[decreaseIndex(i)];

					if (i != _latest_news) {
						if (i == _current_news) _current_news = increaseIndex(_current_news);
						if (i == _forced_news) _forced_news = increaseIndex(_forced_news);
						if (i == visible_news) WP(w, news_d).ni = &_news_items[increaseIndex(visible_news)];
					}

					if (i == _oldest_news) break;
				}
				_oldest_news = increaseIndex(_oldest_news);
			}

			/*DEBUG(misc, 0, "-cur %3d, old %2d, lat %3d, for %3d, tot %2d",
			  _current_news, _oldest_news, _latest_news, _forced_news, _total_news);*/

			w = FindWindowById(WC_MESSAGE_HISTORY, 0);
			if (w != NULL) {
				SetWindowDirty(w);
				w->vscroll.count = _total_news;
			}
		}

		if (n == _latest_news) break;
	}
}
