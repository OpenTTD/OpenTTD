#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "news.h"
#include "vehicle.h"
#include "sound.h"

/* News system
News system is realized as a FIFO queue (in an array)
The positions in the queue can't be rearranged, we only access
the array elements through pointers to the elements. Once the
array is full, the oldest entry (_oldest_news) is being overwritten
by the newest (_latest news).

oldest                   current   lastest
 |                          |         |
[O------------F-------------C---------L           ]
              |
           forced
*/

#define MAX_NEWS 30

#define INVALID_NEWS 255

static NewsItem _news_items[MAX_NEWS];
static byte _current_news = INVALID_NEWS; // points to news item that should be shown next
static byte _oldest_news = 0;    // points to first item in fifo queue
static byte _latest_news = INVALID_NEWS;  // points to last item in fifo queue
/* if the message being shown was forced by the user, its index is stored in
 * _forced_news. forced_news is INVALID_NEWS otherwise.
 * (Users can force messages through history or "last message") */
static byte _forced_news = INVALID_NEWS;

static byte _total_news = 0; // total news count

void DrawNewsNewTrainAvail(Window *w);
void DrawNewsNewRoadVehAvail(Window *w);
void DrawNewsNewShipAvail(Window *w);
void DrawNewsNewAircraftAvail(Window *w);
void DrawNewsBankrupcy(Window *w);
static void MoveToNexItem(void);

StringID GetNewsStringNewTrainAvail(NewsItem *ni);
StringID GetNewsStringNewRoadVehAvail(NewsItem *ni);
StringID GetNewsStringNewShipAvail(NewsItem *ni);
StringID GetNewsStringNewAircraftAvail(NewsItem *ni);
StringID GetNewsStringBankrupcy(NewsItem *ni);

static DrawNewsCallbackProc * const _draw_news_callback[] = {
	DrawNewsNewTrainAvail,    /* DNC_TRAINAVAIL */
	DrawNewsNewRoadVehAvail,  /* DNC_ROADAVAIL */
	DrawNewsNewShipAvail,     /* DNC_SHIPAVAIL */
	DrawNewsNewAircraftAvail, /* DNC_AIRCRAFTAVAIL */
	DrawNewsBankrupcy,        /* DNC_BANKRUPCY */
};

GetNewsStringCallbackProc * const _get_news_string_callback[] = {
	GetNewsStringNewTrainAvail,    /* DNC_TRAINAVAIL */
	GetNewsStringNewRoadVehAvail,  /* DNC_ROADAVAIL */
	GetNewsStringNewShipAvail,     /* DNC_SHIPAVAIL */
	GetNewsStringNewAircraftAvail, /* DNC_AIRCRAFTAVAIL */
	GetNewsStringBankrupcy,        /* DNC_BANKRUPCY */
};

void InitNewsItemStructs(void)
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
						ni->string_id, 426);
				} else {
					byte bk = _display_opt;
					_display_opt &= ~DO_TRANS_BUILDINGS;
					DrawWindowViewport(w);
					_display_opt = bk;

					/* Shade the viewport into gray, or color*/
					vp = w->viewport;
					GfxFillRect(vp->left - w->left, vp->top - w->top,
						vp->left - w->left + vp->width - 1, vp->top - w->top + vp->height - 1,
						ni->flags & NF_INCOLOR ? 0x4322 : 0x4323
					);

					COPY_IN_DPARAM(0, ni->params, lengthof(ni->params));
					DrawStringMultiCenter(w->width / 2, 20, ni->string_id, 428);
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
					DrawStringMultiCenter(w->width / 2, w->height - 16, ni->string_id, 276);
				}
				break;
			}
		}
	} break;

	case WE_CLICK: {
		switch (e->click.widget) {
		case 1: {
			NewsItem *ni = WP(w, news_d).ni;
			DeleteWindow(w);
			ni->duration = 0;
			_forced_news = INVALID_NEWS;
		} break;
		case 0: {
			NewsItem *ni = WP(w, news_d).ni;
			if (ni->flags & NF_VEHICLE) {
				Vehicle *v = &_vehicles[ni->data_a];
				ScrollMainWindowTo(v->x_pos, v->y_pos);
			} else if (ni->flags & NF_TILE) {
				if (!ScrollMainWindowToTile(ni->data_a) && ni->data_b != 0)
					ScrollMainWindowToTile(ni->data_b);
			}
		} break;
		}
	} break;

	case WE_KEYPRESS:
		if (e->keypress.keycode == WKC_SPACE) {
			// Don't continue.
			e->keypress.cont = false;
			DeleteWindow(w);
		}
		break;

	case WE_TICK: {
		int y = max(w->top - 4, _screen.height - w->height);
		if (y == w->top)
			return;

		if (w->viewport != NULL)
			w->viewport->top += y - w->top;

		w->top = y;

		SetDirtyBlocks(w->left, w->top, w->left + w->width, w->top + w->height + 4);
	} break;
	}
}

// returns the correct index in the array
// (to deal with overflows)
byte increaseIndex(byte i)
{
	if (i == INVALID_NEWS)
		return 0;
	i++;
	if (i >= MAX_NEWS)
		i = i % MAX_NEWS;
	return i;
}

void AddNewsItem(StringID string, uint32 flags, uint data_a, uint data_b)
{
	NewsItem *ni;
	Window *w;

	if (_game_mode == GM_MENU)
		return;

	// check the rare case that the oldest (to be overwritten) news item is open
	if (_oldest_news == _current_news || _oldest_news == _forced_news)
		MoveToNexItem();

	_forced_news = INVALID_NEWS;
	if (_total_news < MAX_NEWS) _total_news++;

	// make sure our pointer isn't overflowing
	_latest_news = increaseIndex(_latest_news);

	// overwrite oldest news entry
	if (_oldest_news == _latest_news && _news_items[_oldest_news].string_id != 0)
		_oldest_news = increaseIndex(_oldest_news); // but make sure we're not overflowing here

	// add news to _latest_news
	ni = &_news_items[_latest_news];

	ni->string_id = string;
	ni->display_mode = (byte)flags;
	ni->flags = (byte)(flags >> 8) | NF_NOEXPIRE;

	// show this news message in color?
	if (_date >= ConvertIntDate(_patches.colored_news_date))
		ni->flags |= NF_INCOLOR;

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

/* To add a news item with an attached validation function. This validation function
 * makes sure that the news item is not outdated when the newspaper pops up. */
void AddValidatedNewsItem(StringID string, uint32 flags, uint data_a, uint data_b, ValidationProc validation)
{
	AddNewsItem(string, flags, data_a, data_b);
	_news_items[_latest_news].isValid = validation;
}

// don't show item if it's older than x days
static const byte _news_items_age[] = {60, 60, 90, 60, 90, 30, 150, 30, 90, 180};

static const Widget _news_type13_widgets[] = {
{      WWT_PANEL,    15,     0,   429,     0,   169, 0x0, STR_NULL},
{      WWT_PANEL,    15,     0,    10,     0,    11, 0x0, STR_NULL},
{   WIDGETS_END},
};

static WindowDesc _news_type13_desc = {
	WDP_CENTER, 476, 430, 170,
	WC_NEWS_WINDOW, 0,
	WDF_DEF_WIDGET,
	_news_type13_widgets,
	NewsWindowProc
};

static const Widget _news_type2_widgets[] = {
{      WWT_PANEL,    15,     0,   429,     0,   129, 0x0, STR_NULL},
{      WWT_PANEL,    15,     0,    10,     0,    11, 0x0, STR_NULL},
{   WIDGETS_END},
};

static WindowDesc _news_type2_desc = {
	WDP_CENTER, 476, 430, 130,
	WC_NEWS_WINDOW, 0,
	WDF_DEF_WIDGET,
	_news_type2_widgets,
	NewsWindowProc
};

static const Widget _news_type0_widgets[] = {
{      WWT_PANEL,     5,     0,   279,    14,    86, 0x0,								STR_NULL},
{   WWT_CLOSEBOX,     5,     0,    10,     0,    13, STR_00C5,					STR_NULL},
{    WWT_CAPTION,     5,    11,   279,     0,    13, STR_012C_MESSAGE,	STR_NULL},
{          WWT_6,     5,     2,   277,    16,    64, 0x0,								STR_NULL},
{   WIDGETS_END},
};

static WindowDesc _news_type0_desc = {
	WDP_CENTER, 476, 280, 87,
	WC_NEWS_WINDOW, 0,
	WDF_DEF_WIDGET,
	_news_type0_widgets,
	NewsWindowProc
};

static const SoundFx _news_sounds[] = {
	SND_1D_APPLAUSE,
	SND_1D_APPLAUSE,
	0,
	0,
	0,
	0,
	SND_1E_OOOOH,
	0,
	0,
	0
};

// open up an own newspaper window for the news item
static void ShowNewspaper(NewsItem *ni)
{
	Window *w;
	int sound;
	int top;
	ni->flags &= ~(NF_NOEXPIRE | NF_FORCE_BIG);
	ni->duration = 555;

	sound = _news_sounds[ni->type];
	if (sound != 0)
		SndPlayFx(sound);

	top = _screen.height - 4;
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
	WP(w, news_d).ni = &_news_items[_forced_news == INVALID_NEWS ? _current_news : _forced_news];
	w->flags4 |= WF_DISABLE_VP_SCROLL;
}

// show news item in the ticker
static void ShowTicker(const NewsItem *ni)
{
	Window *w;

	SndPlayFx(SND_16_MORSE);
	_statusbar_news_item = *ni;
	w = FindWindowById(WC_STATUS_BAR, 0);
	if (w != NULL)
		WP(w, def_d).data_1 = 360;
}


// Are we ready to show another news item?
// Only if nothing is in the newsticker and no newspaper is displayed
static bool ReadyForNextItem(void)
{
	const Window *w;
	byte item = _forced_news == INVALID_NEWS ? _current_news : _forced_news;
	NewsItem *ni;

	if (item >= MAX_NEWS) return true;
	ni = &_news_items[item];

	// Ticker message
	// Check if the status bar message is still being displayed?
	w = FindWindowById(WC_STATUS_BAR, 0);
	if (w != NULL && WP(w, def_d).data_1 > -1280)
		return false;

	// Newspaper message
	// Wait until duration reaches 0
	if (ni->duration != 0) {
		ni->duration--;
		return false;
	}

	// neither newsticker nor newspaper are running
	return true;
}

static void MoveToNexItem(void)
{
	DeleteWindowById(WC_NEWS_WINDOW, 0);
	_forced_news = INVALID_NEWS;

	// if we're not at the last item, than move on
	if (_current_news != _latest_news) {
		NewsItem *ni;

		_current_news = increaseIndex(_current_news);
		ni = &_news_items[_current_news];

		// check the date, don't show too old items
		if (_date - _news_items_age[ni->type] > ni->date)
			return;

		// execute the validation function to see if this item is still valid
		if ( ni->isValid != NULL && !ni->isValid(ni->data_a, ni->data_b) )
			return;

		// show newspaper or send to ticker?
		if (!HASBIT(_news_display_opt, ni->type) && !(ni->flags & NF_FORCE_BIG))
			ShowTicker(ni);
		else
			ShowNewspaper(ni);
	}
}

void NewsLoop(void)
{
	// no news item yet
	if (_total_news == 0) return;

	if (ReadyForNextItem())
		MoveToNexItem();
}

/* Do a forced show of a specific message */
void ShowNewsMessage(byte i)
{
	if (_total_news == 0) return;

	// Delete the news window
	DeleteWindowById(WC_NEWS_WINDOW, 0);

	// setup forced news item
	_forced_news = i;

	if (_forced_news != INVALID_NEWS) {
		NewsItem *ni = &_news_items[_forced_news];
		ni->duration = 555;
		ni->flags |= NF_NOEXPIRE | NF_FORCE_BIG;
		DeleteWindowById(WC_NEWS_WINDOW, 0);
		ShowNewspaper(ni);
	}
}

void ShowLastNewsMessage(void)
{
	if (_forced_news == INVALID_NEWS)
		ShowNewsMessage(_current_news);
	else if (_forced_news != 0)
		ShowNewsMessage(_forced_news - 1);
	else {
		if (_total_news != MAX_NEWS)
			ShowNewsMessage(_latest_news);
		else
			ShowNewsMessage(MAX_NEWS - 1);
	}
}


/* return news by number, with 0 being the most
recent news. Returns INVALID_NEWS if end of queue reached. */
static byte getNews(byte i)
{
	if (i >= _total_news)
		return INVALID_NEWS;

	if (_latest_news < i)
		i = _latest_news + MAX_NEWS - i;
	else
		i = _latest_news - i;

	i %= MAX_NEWS;
	return i;
}

static void GetNewsString(NewsItem *ni, byte *buffer)
{
	StringID str;
	byte *s, *d;

	if (ni->display_mode == 3) {
		str = _get_news_string_callback[ni->callback](ni);
	} else {
		COPY_IN_DPARAM(0, ni->params, lengthof(ni->params));
		str = ni->string_id;
	}

	GetString(str_buffr, str);
	assert(strlen(str_buffr) < sizeof(str_buffr) - 1);

	s = str_buffr;
	d = buffer;

	for (;; s++) {
		// cut strings that are too long
		if (s >= str_buffr + 55) {
			d[0] = d[1] = d[2] = '.';
			d += 3;
			*d = '\0';
			break;
		}

		if (*s == '\0') {
			*d = '\0';
			break;
		} else if (*s == '\r') {
			d[0] = d[1] = d[2] = d[3] = ' ';
			d += 4;
		} else if (*s >= ' ' && (*s < 0x88 || *s >= 0x99)) {
			*d++ = *s;
		}
	}
}


static void MessageHistoryWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		byte buffer[256];
		int y = 19;
		byte p, show;
		NewsItem *ni;

		DrawWindowWidgets(w);

		if (_total_news == 0) break;
		show = min(_total_news, 10);

		for (p = w->vscroll.pos; p < w->vscroll.pos + show; p++) {
			// get news in correct order
			ni = &_news_items[getNews(p)];

			SetDParam(0, ni->date);
			DrawString(4, y, STR_00AF, 16);

			GetNewsString(ni, buffer);
			DoDrawString(buffer, 85, y, 16);
			y += 12;
		}

		break;
	}

	case WE_CLICK:
		switch (e->click.widget) {
		case 2: {
			int y = (e->click.pt.y - 19) / 12;
			byte p, q;

			#if 0 // === DEBUG code only
			for (p = 0; p < _total_news; p++)
			{
				NewsItem *ni;
				byte buffer[256];
				ni = &_news_items[p];
				GetNewsString(ni, buffer);
				printf("%i\t%i\t%s\n", p, ni->date, buffer);
			}
			printf("=========================\n");
			#endif

			p = y + w->vscroll.pos;
			if (p > _total_news - 1) break;

			if (_latest_news >= p)
				q = _latest_news - p;
			else
				q = _latest_news + MAX_NEWS - p;
			ShowNewsMessage(q);

			break;
		}
		}
		break;
	}
}

static const Widget _message_history_widgets[] = {
{   WWT_CLOSEBOX,    13,     0,    10,     0,    13, STR_00C5,			STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    13,    11,   399,     0,    13, STR_MESSAGE_HISTORY,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,    13,     0,   388,    14,   139, 0x0, STR_MESSAGE_HISTORY_TIP},
{  WWT_SCROLLBAR,    13,   389,   399,    14,   139, 0x0, STR_0190_SCROLL_BAR_SCROLLS_LIST},
{   WIDGETS_END},
};

static const WindowDesc _message_history_desc = {
	240, 22, 400, 140,
	WC_MESSAGE_HISTORY, 0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_message_history_widgets,
	MessageHistoryWndProc
};

void ShowMessageHistory(void)
{
	Window *w;

	DeleteWindowById(WC_MESSAGE_HISTORY, 0);
	w = AllocateWindowDesc(&_message_history_desc);

	if (w != NULL) {
		w->vscroll.cap = 10;
		w->vscroll.count = _total_news;
		SetWindowDirty(w);
	}
}


static void MessageOptionsWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		uint16 x = _news_display_opt;
		uint32 cs = 0;
		int i, y;

		for (i = 3; i != 23; i += 2) {
			cs |= 1 << (i + (x & 1));
			x >>= 1;
		}
		cs |= (w->click_state >> 23) << 23;

		w->click_state = cs;
		DrawWindowWidgets(w);

		DrawStringCentered(185, 15, STR_0205_MESSAGE_TYPES, 0);

		y = 27;
		for (i = STR_0206_ARRIVAL_OF_FIRST_VEHICLE; i <= STR_020F_GENERAL_INFORMATION; i++) {
			DrawString(124, y, i, 0);
			y += 12;
		}

		break;
	}

	case WE_CLICK: {
		int wid;
		if ((uint)(wid = e->click.widget - 3) < 20) {
			if (!(wid & 1))
				_news_display_opt &= ~(1 << (wid / 2));
			else
				_news_display_opt |= (1 << (wid / 2));
			SetWindowDirty(w);
			// XXX: write settings
		}
		if (e->click.widget == 23) {
			_news_display_opt = 0;
			HandleButtonClick(w, 23);
			SetWindowDirty(w);
		}
		if (e->click.widget == 24) {
			_news_display_opt = ~0;
			HandleButtonClick(w, 24);
			SetWindowDirty(w);
		}
	} break;
	}
}

static const Widget _message_options_widgets[] = {
{   WWT_CLOSEBOX,    13,     0,    10,     0,    13, STR_00C5,								STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    13,    11,   369,     0,    13, STR_0204_MESSAGE_OPTIONS,STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,    13,     0,   369,    14,   172, 0x0,											STR_NULL},
{   WWT_CLOSEBOX,     3,     2,    61,    26,    37, STR_02B8_SUMMARY,				STR_NULL},
{   WWT_CLOSEBOX,     3,    62,   121,    26,    37, STR_02B9_FULL,						STR_NULL},
{   WWT_CLOSEBOX,     3,     2,    61,    38,    49, STR_02B8_SUMMARY,				STR_NULL},
{   WWT_CLOSEBOX,     3,    62,   121,    38,    49, STR_02B9_FULL,						STR_NULL},
{   WWT_CLOSEBOX,     3,     2,    61,    50,    61, STR_02B8_SUMMARY,				STR_NULL},
{   WWT_CLOSEBOX,     3,    62,   121,    50,    61, STR_02B9_FULL,						STR_NULL},
{   WWT_CLOSEBOX,     3,     2,    61,    62,    73, STR_02B8_SUMMARY,				STR_NULL},
{   WWT_CLOSEBOX,     3,    62,   121,    62,    73, STR_02B9_FULL,						STR_NULL},
{   WWT_CLOSEBOX,     3,     2,    61,    74,    85, STR_02B8_SUMMARY,				STR_NULL},
{   WWT_CLOSEBOX,     3,    62,   121,    74,    85, STR_02B9_FULL,						STR_NULL},
{   WWT_CLOSEBOX,     3,     2,    61,    86,    97, STR_02B8_SUMMARY,				STR_NULL},
{   WWT_CLOSEBOX,     3,    62,   121,    86,    97, STR_02B9_FULL,						STR_NULL},
{   WWT_CLOSEBOX,     3,     2,    61,    98,   109, STR_02B8_SUMMARY,				STR_NULL},
{   WWT_CLOSEBOX,     3,    62,   121,    98,   109, STR_02B9_FULL,						STR_NULL},
{   WWT_CLOSEBOX,     3,     2,    61,   110,   121, STR_02B8_SUMMARY,				STR_NULL},
{   WWT_CLOSEBOX,     3,    62,   121,   110,   121, STR_02B9_FULL,						STR_NULL},
{   WWT_CLOSEBOX,     3,     2,    61,   122,   133, STR_02B8_SUMMARY,				STR_NULL},
{   WWT_CLOSEBOX,     3,    62,   121,   122,   133, STR_02B9_FULL,						STR_NULL},
{   WWT_CLOSEBOX,     3,     2,    61,   134,   145, STR_02B8_SUMMARY,				STR_NULL},
{   WWT_CLOSEBOX,     3,    62,   121,   134,   145, STR_02B9_FULL,						STR_NULL},

{ WWT_PUSHTXTBTN,     3,    15,   170,   154,   165, STR_MESSAGES_DISABLE_ALL,STR_NULL },
{ WWT_PUSHTXTBTN,     3,   200,   355,   154,   165, STR_MESSAGES_ENABLE_ALL,	STR_NULL },

{   WIDGETS_END},
};

static const WindowDesc _message_options_desc = {
	270, 22, 370, 173,
	WC_GAME_OPTIONS, 0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_message_options_widgets,
	MessageOptionsWndProc
};

void ShowMessageOptions(void)
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	AllocateWindowDesc(&_message_options_desc);
}
