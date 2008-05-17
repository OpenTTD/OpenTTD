/* $Id$ */

/** @file news_gui.cpp GUI functions related to news messages. */

#include "stdafx.h"
#include "openttd.h"
#include "gui.h"
#include "window_gui.h"
#include "viewport_func.h"
#include "news_func.h"
#include "settings_type.h"
#include "transparency.h"
#include "strings_func.h"
#include "window_func.h"
#include "date_func.h"
#include "vehicle_base.h"
#include "sound_func.h"
#include "string_func.h"
#include "widgets/dropdown_func.h"
#include "map_func.h"
#include "statusbar_gui.h"
#include "player_face.h"

#include "table/sprites.h"
#include "table/strings.h"

/** @file news_gui.cpp
 *
 * News system is realized as a FIFO queue (in an array)
 * The positions in the queue can't be rearranged, we only access
 * the array elements through pointers to the elements. Once the
 * array is full, the oldest entry (\a _oldest_news) is being overwritten
 * by the newest (\a _latest_news).
 *
 * \verbatim
 * oldest                   current   lastest
 *  |                          |         |
 * [O------------F-------------C---------L           ]
 *               |
 *            forced
 * \endverbatim
 *
 * Of course by using an array we can have situations like
 *
 * \verbatim
 * [----L          O-----F---------C-----------------]
 * This is where we have wrapped around the array and have
 * (MAX_NEWS - O) + L news items
 * \endverbatim
 */

#define NB_WIDG_PER_SETTING 4

typedef byte NewsID;
#define INVALID_NEWS 255

NewsItem _statusbar_news_item;
bool _news_ticker_sound;
static NewsItem *_news_items = NULL;        ///< The news FIFO queue
static uint _max_news_items = 0;            ///< size of news FIFO queue
static NewsID _current_news = INVALID_NEWS; ///< points to news item that should be shown next
static NewsID _oldest_news = 0;             ///< points to first item in fifo queue
static NewsID _latest_news = INVALID_NEWS;  ///< points to last item in fifo queue

/** Forced news item.
 * Users can force an item by accessing the history or "last message".
 * If the message being shown was forced by the user, its index is stored in
 * _forced_news. Otherwise, \a _forced_news variable is INVALID_NEWS. */
static NewsID _forced_news = INVALID_NEWS;

static uint _total_news = 0; ///< Number of news items in FIFO queue @see _news_items
static void MoveToNextItem();


typedef void DrawNewsCallbackProc(struct Window *w, const NewsItem *ni);
void DrawNewsNewVehicleAvail(Window *w, const NewsItem *ni);

static void DrawNewsBankrupcy(Window *w, const NewsItem *ni)
{
	Player *p = GetPlayer((PlayerID)(ni->data_b));
	DrawPlayerFace(p->face, p->player_color, 2, 23);
	GfxFillRect(3, 23, 3 + 91, 23 + 118, PALETTE_TO_STRUCT_GREY | (1 << USE_COLORTABLE));

	SetDParam(0, p->index);

	DrawStringMultiCenter(49, 148, STR_7058_PRESIDENT, 94);

	switch (ni->subtype) {
		case NS_COMPANY_TROUBLE:
			DrawStringCentered(w->width >> 1, 1, STR_7056_TRANSPORT_COMPANY_IN_TROUBLE, TC_FROMSTRING);

			SetDParam(0, p->index);

			DrawStringMultiCenter(
				((w->width - 101) >> 1) + 98,
				90,
				STR_7057_WILL_BE_SOLD_OFF_OR_DECLARED,
				w->width - 101);
			break;

		case NS_COMPANY_MERGER:
			DrawStringCentered(w->width >> 1, 1, STR_7059_TRANSPORT_COMPANY_MERGER, TC_FROMSTRING);
			SetDParam(0, ni->params[2]);
			SetDParam(1, p->index);
			SetDParam(2, ni->params[4]);
			DrawStringMultiCenter(
				((w->width - 101) >> 1) + 98,
				90,
				ni->params[4] == 0 ? STR_707F_HAS_BEEN_TAKEN_OVER_BY : STR_705A_HAS_BEEN_SOLD_TO_FOR,
				w->width - 101);
			break;

		case NS_COMPANY_BANKRUPT:
			DrawStringCentered(w->width >> 1, 1, STR_705C_BANKRUPT, TC_FROMSTRING);
			SetDParam(0, p->index);
			DrawStringMultiCenter(
				((w->width - 101) >> 1) + 98,
				90,
				STR_705D_HAS_BEEN_CLOSED_DOWN_BY,
				w->width - 101);
			break;

		case NS_COMPANY_NEW:
			DrawStringCentered(w->width >> 1, 1, STR_705E_NEW_TRANSPORT_COMPANY_LAUNCHED, TC_FROMSTRING);
			SetDParam(0, p->index);
			SetDParam(1, ni->params[3]);
			DrawStringMultiCenter(
				((w->width - 101) >> 1) + 98,
				90,
				STR_705F_STARTS_CONSTRUCTION_NEAR,
				w->width - 101);
			break;

		default:
			NOT_REACHED();
	}
}


static DrawNewsCallbackProc * const _draw_news_callback[] = {
	DrawNewsNewVehicleAvail,  ///< DNC_VEHICLEAVAIL
	DrawNewsBankrupcy,        ///< DNC_BANKRUPCY
};

/**
 * Data common to all news items of a given subtype (structure)
 */
struct NewsSubtypeData {
	NewsType type;         ///< News category @see NewsType
	NewsMode display_mode; ///< Display mode value @see NewsMode
	NewsFlag flags;        ///< Initial NewsFlags bits @see NewsFlag
	NewsCallback callback; ///< Call-back function
};

/**
 * Data common to all news items of a given subtype (actual data)
 */
static const struct NewsSubtypeData _news_subtype_data[NS_END] = {
	/* type,             display_mode, flags,                  callback */
	{ NT_ARRIVAL_PLAYER,  NM_THIN,     NF_VIEWPORT|NF_VEHICLE, DNC_NONE         }, ///< NS_ARRIVAL_PLAYER
	{ NT_ARRIVAL_OTHER,   NM_THIN,     NF_VIEWPORT|NF_VEHICLE, DNC_NONE         }, ///< NS_ARRIVAL_OTHER
	{ NT_ACCIDENT,        NM_THIN,     NF_VIEWPORT|NF_TILE,    DNC_NONE         }, ///< NS_ACCIDENT_TILE
	{ NT_ACCIDENT,        NM_THIN,     NF_VIEWPORT|NF_VEHICLE, DNC_NONE         }, ///< NS_ACCIDENT_VEHICLE
	{ NT_COMPANY_INFO,    NM_CALLBACK, NF_NONE,                DNC_BANKRUPCY    }, ///< NS_COMPANY_TROUBLE
	{ NT_COMPANY_INFO,    NM_CALLBACK, NF_NONE,                DNC_BANKRUPCY    }, ///< NS_COMPANY_MERGER
	{ NT_COMPANY_INFO,    NM_CALLBACK, NF_NONE,                DNC_BANKRUPCY    }, ///< NS_COMPANY_BANKRUPT
	{ NT_COMPANY_INFO,    NM_CALLBACK, NF_TILE,                DNC_BANKRUPCY    }, ///< NS_COMPANY_NEW
	{ NT_OPENCLOSE,       NM_THIN,     NF_VIEWPORT|NF_TILE,    DNC_NONE         }, ///< NS_OPENCLOSE
	{ NT_ECONOMY,         NM_NORMAL,   NF_NONE,                DNC_NONE         }, ///< NS_ECONOMY
	{ NT_INDUSTRY_PLAYER, NM_THIN,     NF_VIEWPORT|NF_TILE,    DNC_NONE         }, ///< NS_INDUSTRY_PLAYER
	{ NT_INDUSTRY_OTHER,  NM_THIN,     NF_VIEWPORT|NF_TILE,    DNC_NONE         }, ///< NS_INDUSTRY_OTHER
	{ NT_INDUSTRY_NOBODY, NM_THIN,     NF_VIEWPORT|NF_TILE,    DNC_NONE         }, ///< NS_INDUSTRY_NOBODY
	{ NT_ADVICE,          NM_SMALL,    NF_VIEWPORT|NF_VEHICLE, DNC_NONE         }, ///< NS_ADVICE
	{ NT_NEW_VEHICLES,    NM_CALLBACK, NF_NONE,                DNC_VEHICLEAVAIL }, ///< NS_NEW_VEHICLES
	{ NT_ACCEPTANCE,      NM_SMALL,    NF_VIEWPORT|NF_TILE,    DNC_NONE         }, ///< NS_ACCEPTANCE
	{ NT_SUBSIDIES,       NM_NORMAL,   NF_TILE|NF_TILE2,       DNC_NONE         }, ///< NS_SUBSIDIES
	{ NT_GENERAL,         NM_NORMAL,   NF_TILE,                DNC_NONE         }, ///< NS_GENERAL
};

/** Initialize the news-items data structures */
void InitNewsItemStructs()
{
	free(_news_items);
	_max_news_items = max(ScaleByMapSize(30), 30U);
	_news_items = CallocT<NewsItem>(_max_news_items);
	_current_news = INVALID_NEWS;
	_oldest_news = 0;
	_latest_news = INVALID_NEWS;
	_forced_news = INVALID_NEWS;
	_total_news = 0;
}

struct NewsWindow : Window {
	uint16 chat_height;
	NewsItem *ni;

	NewsWindow(const WindowDesc *desc, NewsItem *ni) : Window(desc), ni(ni)
	{
		const Window *w = FindWindowById(WC_SEND_NETWORK_MSG, 0);
		this->chat_height = (w != NULL) ? w->height : 0;

		this->ni = &_news_items[_forced_news == INVALID_NEWS ? _current_news : _forced_news];
		this->flags4 |= WF_DISABLE_VP_SCROLL;

		this->FindWindowPlacementAndResize(desc);
	}

	void DrawNewsBorder()
	{
		int left = 0;
		int right = this->width - 1;
		int top = 0;
		int bottom = this->height - 1;

		GfxFillRect(left, top, right, bottom, 0xF);

		GfxFillRect(left, top, left, bottom, 0xD7);
		GfxFillRect(right, top, right, bottom, 0xD7);
		GfxFillRect(left, top, right, top, 0xD7);
		GfxFillRect(left, bottom, right, bottom, 0xD7);

		DrawString(left + 2, top + 1, STR_00C6, TC_FROMSTRING);
	}

	virtual void OnPaint()
	{
		const NewsMode display_mode = _news_subtype_data[this->ni->subtype].display_mode;

		switch (display_mode) {
			case NM_NORMAL:
			case NM_THIN: {
				this->DrawNewsBorder();

				DrawString(2, 1, STR_00C6, TC_FROMSTRING);

				SetDParam(0, this->ni->date);
				DrawStringRightAligned(428, 1, STR_01FF, TC_FROMSTRING);

				if (!(this->ni->flags & NF_VIEWPORT)) {
					CopyInDParam(0, this->ni->params, lengthof(this->ni->params));
					DrawStringMultiCenter(215, display_mode == NM_NORMAL ? 76 : 56,
						this->ni->string_id, this->width - 4);
				} else {
					/* Back up transparency options to draw news view */
					TransparencyOptionBits to_backup = _transparency_opt;
					_transparency_opt = 0;
					this->DrawViewport();
					_transparency_opt = to_backup;

					/* Shade the viewport into gray, or color*/
					ViewPort *vp = this->viewport;
					GfxFillRect(vp->left - this->left, vp->top - this->top,
						vp->left - this->left + vp->width - 1, vp->top - this->top + vp->height - 1,
						(this->ni->flags & NF_INCOLOR ? PALETTE_TO_TRANSPARENT : PALETTE_TO_STRUCT_GREY) | (1 << USE_COLORTABLE)
					);

					CopyInDParam(0, this->ni->params, lengthof(this->ni->params));
					DrawStringMultiCenter(this->width / 2, 20, this->ni->string_id, this->width - 4);
				}
				break;
			}

			case NM_CALLBACK:
				this->DrawNewsBorder();
				_draw_news_callback[_news_subtype_data[this->ni->subtype].callback](this, ni);
				break;

			default:
				this->DrawWidgets();
				if (!(this->ni->flags & NF_VIEWPORT)) {
					CopyInDParam(0, this->ni->params, lengthof(this->ni->params));
					DrawStringMultiCenter(140, 38, this->ni->string_id, 276);
				} else {
					this->DrawViewport();
					CopyInDParam(0, this->ni->params, lengthof(this->ni->params));
					DrawStringMultiCenter(this->width / 2, this->height - 16, this->ni->string_id, this->width - 4);
				}
				break;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case 1:
				this->ni->duration = 0;
				delete this;
				_forced_news = INVALID_NEWS;
				break;

			case 0:
				if (this->ni->flags & NF_VEHICLE) {
					Vehicle *v = GetVehicle(this->ni->data_a);
					ScrollMainWindowTo(v->x_pos, v->y_pos);
				} else if (this->ni->flags & NF_TILE) {
					if (_ctrl_pressed) {
						ShowExtraViewPortWindow(this->ni->data_a);
						if (this->ni->flags & NF_TILE2) {
							ShowExtraViewPortWindow(this->ni->data_b);
						}
					} else {
						if (!ScrollMainWindowToTile(this->ni->data_a) && this->ni->flags & NF_TILE2) {
							ScrollMainWindowToTile(this->ni->data_b);
						}
					}
				}
				break;
		}
	}

	virtual bool OnKeyPress(uint16 key, uint16 keycode)
	{
		if (keycode == WKC_SPACE) {
			/* Don't continue. */
			delete this;
			return false;
		}
		return true;
	}

	virtual void OnInvalidateData(int data)
	{
		/* The chatbar has notified us that is was either created or closed */
		this->chat_height = data;
	}

	virtual void OnTick()
	{
		/* Scroll up newsmessages from the bottom in steps of 4 pixels */
		int y = max(this->top - 4, _screen.height - this->height - 12 - this->chat_height);
		if (y == this->top) return;

		if (this->viewport != NULL) this->viewport->top += y - this->top;

		int diff = Delta(this->top, y);
		this->top = y;

		SetDirtyBlocks(this->left, this->top - diff, this->left + this->width, this->top + this->height);
	}
};

/**
 * Return the correct index in the pseudo-fifo
 * queue and deals with overflows when increasing the index
 */
static inline NewsID IncreaseIndex(NewsID i)
{
	assert(i != INVALID_NEWS);
	return (i + 1) % _max_news_items;
}

/**
 * Return the correct index in the pseudo-fifo
 * queue and deals with overflows when decreasing the index
 */
static inline NewsID DecreaseIndex(NewsID i)
{
	assert(i != INVALID_NEWS);
	return (i + _max_news_items - 1) % _max_news_items;
}

/**
 * Add a new newsitem to be shown.
 * @param string String to display
 * @param subtype news category, any of the NewsSubtype enums (NS_)
 * @param data_a news-specific value based on news type
 * @param data_b news-specific value based on news type
 *
 * @see NewsSubype
 */
void AddNewsItem(StringID string, NewsSubtype subtype, uint data_a, uint data_b)
{
	if (_game_mode == GM_MENU) return;

	/* check the rare case that the oldest (to be overwritten) news item is open */
	if (_total_news == _max_news_items && (_oldest_news == _current_news || _oldest_news == _forced_news)) {
		MoveToNextItem();
	}

	if (_total_news < _max_news_items) _total_news++;

	/* Increase _latest_news. If we have no news yet, use _oldest news as an
	 * index. We cannot use 0 as _oldest_news can jump around due to
	 * DeleteVehicleNews */
	NewsID l_news = _latest_news;
	_latest_news = (_latest_news == INVALID_NEWS) ? _oldest_news : IncreaseIndex(_latest_news);

	/* If the fifo-buffer is full, overwrite the oldest entry */
	if (l_news != INVALID_NEWS && _latest_news == _oldest_news) {
		assert(_total_news == _max_news_items);
		_oldest_news = IncreaseIndex(_oldest_news);
	}

	/*DEBUG(misc, 0, "+cur %3d, old %2d, lat %3d, for %3d, tot %2d",
	  _current_news, _oldest_news, _latest_news, _forced_news, _total_news);*/

	/* Add news to _latest_news */
	NewsItem *ni = &_news_items[_latest_news];
	memset(ni, 0, sizeof(*ni));

	ni->string_id = string;
	ni->subtype = subtype;
	ni->flags = _news_subtype_data[subtype].flags;

	/* show this news message in color? */
	if (_cur_year >= _patches.colored_news_year) ni->flags |= NF_INCOLOR;

	ni->data_a = data_a;
	ni->data_b = data_b;
	ni->date = _date;
	CopyOutDParam(ni->params, 0, lengthof(ni->params));

	Window *w = FindWindowById(WC_MESSAGE_HISTORY, 0);
	if (w == NULL) return;
	w->SetDirty();
	w->vscroll.count = _total_news;
}


/**
 * Per-NewsType data
 */
NewsTypeData _news_type_data[NT_END] = {
	/* name,              age, sound,           display */
	{ "arrival_player",    60, SND_1D_APPLAUSE, ND_FULL },  ///< NT_ARRIVAL_PLAYER
	{ "arrival_other",     60, SND_1D_APPLAUSE, ND_FULL },  ///< NT_ARRIVAL_OTHER
	{ "accident",          90, SND_BEGIN,       ND_FULL },  ///< NT_ACCIDENT
	{ "company_info",      60, SND_BEGIN,       ND_FULL },  ///< NT_COMPANY_INFO
	{ "openclose",         90, SND_BEGIN,       ND_FULL },  ///< NT_OPENCLOSE
	{ "economy",           30, SND_BEGIN,       ND_FULL },  ///< NT_ECONOMY
	{ "production_player", 30, SND_BEGIN,       ND_FULL },  ///< NT_INDUSTRY_PLAYER
	{ "production_other",  30, SND_BEGIN,       ND_FULL },  ///< NT_INDUSTRY_OTHER
	{ "production_nobody", 30, SND_BEGIN,       ND_FULL },  ///< NT_INDUSTRY_NOBODY
	{ "advice",           150, SND_BEGIN,       ND_FULL },  ///< NT_ADVICE
	{ "new_vehicles",      30, SND_1E_OOOOH,    ND_FULL },  ///< NT_NEW_VEHICLES
	{ "acceptance",        90, SND_BEGIN,       ND_FULL },  ///< NT_ACCEPTANCE
	{ "subsidies",        180, SND_BEGIN,       ND_FULL },  ///< NT_SUBSIDIES
	{ "general",           60, SND_BEGIN,       ND_FULL },  ///< NT_GENERAL
};


static const Widget _news_type13_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,    15,     0,   429,     0,   169, 0x0, STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    15,     0,    10,     0,    11, 0x0, STR_NULL},
{   WIDGETS_END},
};

static WindowDesc _news_type13_desc = {
	WDP_CENTER, 476, 430, 170, 430, 170,
	WC_NEWS_WINDOW, WC_NONE,
	WDF_DEF_WIDGET,
	_news_type13_widgets,
	NULL
};

static const Widget _news_type2_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,    15,     0,   429,     0,   129, 0x0, STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    15,     0,    10,     0,    11, 0x0, STR_NULL},
{   WIDGETS_END},
};

static WindowDesc _news_type2_desc = {
	WDP_CENTER, 476, 430, 130, 430, 130,
	WC_NEWS_WINDOW, WC_NONE,
	WDF_DEF_WIDGET,
	_news_type2_widgets,
	NULL
};

static const Widget _news_type0_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,     5,     0,   279,    14,    86, 0x0,              STR_NULL},
{   WWT_CLOSEBOX,   RESIZE_NONE,     5,     0,    10,     0,    13, STR_00C5,         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     5,    11,   279,     0,    13, STR_012C_MESSAGE, STR_NULL},
{      WWT_INSET,   RESIZE_NONE,     5,     2,   277,    16,    64, 0x0,              STR_NULL},
{   WIDGETS_END},
};

static WindowDesc _news_type0_desc = {
	WDP_CENTER, 476, 280, 87, 280, 87,
	WC_NEWS_WINDOW, WC_NONE,
	WDF_DEF_WIDGET,
	_news_type0_widgets,
	NULL
};


/** Open up an own newspaper window for the news item */
static void ShowNewspaper(NewsItem *ni)
{
	ni->flags &= ~NF_FORCE_BIG;
	ni->duration = 555;

	SoundFx sound = _news_type_data[_news_subtype_data[ni->subtype].type].sound;
	if (sound != 0) SndPlayFx(sound);

	int top = _screen.height;
	Window *w;
	switch (_news_subtype_data[ni->subtype].display_mode) {
		case NM_NORMAL:
		case NM_CALLBACK:
			_news_type13_desc.top = top;
			w = new NewsWindow(&_news_type13_desc, ni);
			if (ni->flags & NF_VIEWPORT) {
				InitializeWindowViewport(w, 2, 58, 426, 110,
					ni->data_a | (ni->flags & NF_VEHICLE ? 0x80000000 : 0), ZOOM_LVL_NEWS);
			}
			break;

		case NM_THIN:
			_news_type2_desc.top = top;
			w = new NewsWindow(&_news_type2_desc, ni);
			if (ni->flags & NF_VIEWPORT) {
				InitializeWindowViewport(w, 2, 58, 426, 70,
					ni->data_a | (ni->flags & NF_VEHICLE ? 0x80000000 : 0), ZOOM_LVL_NEWS);
			}
			break;

		default:
			_news_type0_desc.top = top;
			w = new NewsWindow(&_news_type0_desc, ni);
			if (ni->flags & NF_VIEWPORT) {
				InitializeWindowViewport(w, 3, 17, 274, 47,
					ni->data_a | (ni->flags & NF_VEHICLE ? 0x80000000 : 0), ZOOM_LVL_NEWS);
			}
			break;
	}

	/*DEBUG(misc, 0, " cur %3d, old %2d, lat %3d, for %3d, tot %2d",
	  _current_news, _oldest_news, _latest_news, _forced_news, _total_news);*/
}

/** Show news item in the ticker */
static void ShowTicker(const NewsItem *ni)
{
	if (_news_ticker_sound) SndPlayFx(SND_16_MORSE);

	_statusbar_news_item = *ni;
	InvalidateWindowData(WC_STATUS_BAR, 0, SBI_SHOW_TICKER);
}


/**
 * Are we ready to show another news item?
 * Only if nothing is in the newsticker and no newspaper is displayed
 */
static bool ReadyForNextItem()
{
	NewsID item = (_forced_news == INVALID_NEWS) ? _current_news : _forced_news;

	if (item >= _max_news_items) return true;
	NewsItem *ni = &_news_items[item];

	/* Ticker message
	 * Check if the status bar message is still being displayed? */
	if (IsNewsTickerShown()) return false;

	/* Newspaper message, decrement duration counter */
	if (ni->duration != 0) ni->duration--;

	/* neither newsticker nor newspaper are running */
	return (ni->duration == 0 || FindWindowById(WC_NEWS_WINDOW, 0) == NULL);
}

/** Move to the next news item */
static void MoveToNextItem()
{
	DeleteWindowById(WC_NEWS_WINDOW, 0);
	_forced_news = INVALID_NEWS;

	/* if we're not at the last item, then move on */
	if (_current_news != _latest_news) {
		_current_news = (_current_news == INVALID_NEWS) ? _oldest_news : IncreaseIndex(_current_news);
		NewsItem *ni = &_news_items[_current_news];
		const NewsType type = _news_subtype_data[ni->subtype].type;

		/* check the date, don't show too old items */
		if (_date - _news_type_data[type].age > ni->date) return;

		switch (_news_type_data[type].display) {
			default: NOT_REACHED();
			case ND_OFF: // Off - show nothing only a small reminder in the status bar
				InvalidateWindowData(WC_STATUS_BAR, 0, SBI_SHOW_REMINDER);
				break;

			case ND_SUMMARY: // Summary - show ticker, but if forced big, cascade to full
				if (!(ni->flags & NF_FORCE_BIG)) {
					ShowTicker(ni);
					break;
				}
				/* Fallthrough */

			case ND_FULL: // Full - show newspaper
				ShowNewspaper(ni);
				break;
		}
	}
}

void NewsLoop()
{
	/* no news item yet */
	if (_total_news == 0) return;

	if (ReadyForNextItem()) MoveToNextItem();
}

/** Do a forced show of a specific message */
static void ShowNewsMessage(NewsID i)
{
	if (_total_news == 0) return;

	/* Delete the news window */
	DeleteWindowById(WC_NEWS_WINDOW, 0);

	/* setup forced news item */
	_forced_news = i;

	if (_forced_news != INVALID_NEWS) {
		NewsItem *ni = &_news_items[_forced_news];
		ni->duration = 555;
		ni->flags |= NF_FORCE_BIG;
		DeleteWindowById(WC_NEWS_WINDOW, 0);
		ShowNewspaper(ni);
	}
}

/** Show previous news item */
void ShowLastNewsMessage()
{
	if (_forced_news == INVALID_NEWS) {
		/* Not forced any news yet, show the current one, unless a news window is
		 * open (which can only be the current one), then show the previous item */
		const Window *w = FindWindowById(WC_NEWS_WINDOW, 0);
		ShowNewsMessage((w == NULL || (_current_news == _oldest_news)) ? _current_news : DecreaseIndex(_current_news));
	} else if (_forced_news == _oldest_news) {
		/* We have reached the oldest news, start anew with the latest */
		ShowNewsMessage(_latest_news);
	} else {
		/* 'Scrolling' through news history show each one in turn */
		ShowNewsMessage(DecreaseIndex(_forced_news));
	}
}


/* return news by number, with 0 being the most
 * recent news. Returns INVALID_NEWS if end of queue reached. */
static NewsID getNews(NewsID i)
{
	if (i >= _total_news) return INVALID_NEWS;

	if (_latest_news < i) {
		i = _latest_news + _max_news_items - i;
	} else {
		i = _latest_news - i;
	}

	i %= _max_news_items;
	return i;
}

/**
 * Draw an unformatted news message truncated to a maximum length. If
 * length exceeds maximum length it will be postfixed by '...'
 * @param x,y position of the string
 * @param color the color the string will be shown in
 * @param *ni NewsItem being printed
 * @param maxw maximum width of string in pixels
 */
static void DrawNewsString(int x, int y, uint16 color, const NewsItem *ni, uint maxw)
{
	char buffer[512], buffer2[512];
	StringID str;

	CopyInDParam(0, ni->params, lengthof(ni->params));
	str = ni->string_id;

	GetString(buffer, str, lastof(buffer));
	/* Copy the just gotten string to another buffer to remove any formatting
	 * from it such as big fonts, etc. */
	const char *ptr = buffer;
	char *dest = buffer2;
	WChar c_last = '\0';
	for (;;) {
		WChar c = Utf8Consume(&ptr);
		if (c == 0) break;
		/* Make a space from a newline, but ignore multiple newlines */
		if (c == '\n' && c_last != '\n') {
			dest[0] = ' ';
			dest++;
		} else if (c == '\r') {
			dest[0] = dest[1] = dest[2] = dest[3] = ' ';
			dest += 4;
		} else if (IsPrintable(c)) {
			dest += Utf8Encode(dest, c);
		}
		c_last = c;
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

			SetVScrollCount(w, _total_news);
			w->DrawWidgets();

			if (_total_news == 0) break;
			NewsID show = min(_total_news, w->vscroll.cap);

			for (NewsID p = w->vscroll.pos; p < w->vscroll.pos + show; p++) {
				/* get news in correct order */
				const NewsItem *ni = &_news_items[getNews(p)];

				SetDParam(0, ni->date);
				DrawString(4, y, STR_SHORT_DATE, TC_WHITE);

				DrawNewsString(82, y, TC_WHITE, ni, w->width - 95);
				y += 12;
			}
			break;
		}

		case WE_CLICK:
			if (e->we.click.widget == 3) {
				int y = (e->we.click.pt.y - 19) / 12;
				NewsID p = getNews(y + w->vscroll.pos);

				if (p != INVALID_NEWS) ShowNewsMessage(p);
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
	240, 22, 400, 140, 400, 140,
	WC_MESSAGE_HISTORY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_message_history_widgets,
	MessageHistoryWndProc
};

/** Display window with news messages history */
void ShowMessageHistory()
{
	DeleteWindowById(WC_MESSAGE_HISTORY, 0);
	Window *w = new Window(&_message_history_desc);

	if (w == NULL) return;

	w->vscroll.cap = 10;
	w->vscroll.count = _total_news;
	w->resize.step_height = 12;
	w->resize.height = w->height - 12 * 6; // minimum of 4 items in the list, each item 12 high
	w->resize.step_width = 1;
	w->resize.width = 200; // can't make window any smaller than 200 pixel
	w->SetDirty();
}


/** News settings window widget offset constants */
enum {
	WIDGET_NEWSOPT_DROP_SUMMARY = 4,  ///< Dropdown that adjusts at once the level for all settings
	WIDGET_NEWSOPT_SOUNDTICKER  = 6,  ///< Button activating sound on events
	WIDGET_NEWSOPT_START_OPTION = 8,  ///< First widget that is part of a group [<] .. [.]
};

static const StringID _message_opt[] = {STR_OFF, STR_SUMMARY, STR_FULL, INVALID_STRING_ID};

struct MessageOptionsWindow : Window {
	int state;

	MessageOptionsWindow(const WindowDesc *desc) : Window(desc)
	{
		NewsDisplay all_val;

		/* Set up the initial disabled buttons in the case of 'off' or 'full' */
		all_val = _news_type_data[0].display;
		for (int i = 0; i < NT_END; i++) {
			this->SetMessageButtonStates(_news_type_data[i].display, i);
			/* If the value doesn't match the ALL-button value, set the ALL-button value to 'off' */
			if (_news_type_data[i].display != all_val) all_val = ND_OFF;
		}
		/* If all values are the same value, the ALL-button will take over this value */
		this->state = all_val;
	}

	/**
	 * Setup the disabled/enabled buttons in the message window
	 * If the value is 'off' disable the [<] widget, and enable the [>] one
	 * Same-wise for all the others. Starting value of 4 is the first widget
	 * group. These are grouped as [<][>] .. [<][>], etc.
	 * @param value to set in the widget
	 * @param element index of the group of widget to set
	 */
	void SetMessageButtonStates(byte value, int element)
	{
		element *= NB_WIDG_PER_SETTING;

		this->SetWidgetDisabledState(element + WIDGET_NEWSOPT_START_OPTION, value == 0);
		this->SetWidgetDisabledState(element + WIDGET_NEWSOPT_START_OPTION + 2, value == 2);
	}

	virtual void OnPaint()
	{
		if (_news_ticker_sound) this->LowerWidget(WIDGET_NEWSOPT_SOUNDTICKER);

		this->widget[WIDGET_NEWSOPT_DROP_SUMMARY].data = _message_opt[this->state];
		this->DrawWidgets();

		/* Draw the string of each setting on each button. */
		for (int i = 0, y = 26; i < NT_END; i++, y += 12) {
			/* 51 comes from 13 + 89 (left and right of the button)+1, shiefted by one as to get division,
				* which will give centered position */
			DrawStringCentered(51, y + 1, _message_opt[_news_type_data[i].display], TC_BLACK);
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case WIDGET_NEWSOPT_DROP_SUMMARY: // Dropdown menu for all settings
				ShowDropDownMenu(this, _message_opt, this->state, WIDGET_NEWSOPT_DROP_SUMMARY, 0, 0);
				break;

			case WIDGET_NEWSOPT_SOUNDTICKER: // Change ticker sound on/off
				_news_ticker_sound ^= 1;
				this->ToggleWidgetLoweredState(widget);
				this->InvalidateWidget(widget);
				break;

			default: { // Clicked on the [<] .. [>] widgets
				int wid = widget - WIDGET_NEWSOPT_START_OPTION;
				if (wid >= 0 && wid < (NB_WIDG_PER_SETTING * NT_END)) {
					int element = wid / NB_WIDG_PER_SETTING;
					byte val = (_news_type_data[element].display + ((wid % NB_WIDG_PER_SETTING) ? 1 : -1)) % 3;

					this->SetMessageButtonStates(val, element);
					_news_type_data[element].display = (NewsDisplay)val;
					this->SetDirty();
				}
				break;
			}
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		this->state = index;

		for (int i = 0; i < NT_END; i++) {
			this->SetMessageButtonStates(index, i);
			_news_type_data[i].display = (NewsDisplay)index;
		}
		this->SetDirty();
	}
};


/*
* The news settings window widgets
*
* Main part of the window is a list of news-setting lines, one for each news category.
* Each line is constructed by an expansion of the \c NEWS_SETTINGS_LINE macro
*/

/**
* Macro to construct one news-setting line in the news-settings window.
* One line consists of four widgets, namely
* - A [<] button
* - A [...] label
* - A [>] button
* - A text label describing the news category
* Horizontal positions of the widgets are hard-coded, vertical start position is (\a basey + \a linenum * \c NEWS_SETTING_BASELINE_SKIP).
* Height of one line is 12, with the text label shifted 1 pixel down.
*
* First line should be widget number WIDGET_NEWSOPT_START_OPTION
*
* @param basey: Base Y coordinate
* @param linenum: Count, news-setting is the \a linenum-th line
* @param text: StringID for the text label to display
*/
#define NEWS_SETTINGS_LINE(basey, linenum, text) \
	{ WWT_PUSHIMGBTN, RESIZE_NONE, COLOUR_YELLOW, \
	    4,  12,  basey     + linenum * NEWS_SETTING_BASELINE_SKIP,  basey + 11 + linenum * NEWS_SETTING_BASELINE_SKIP, \
	  SPR_ARROW_LEFT, STR_HSCROLL_BAR_SCROLLS_LIST}, \
	{ WWT_PUSHTXTBTN, RESIZE_NONE, COLOUR_YELLOW, \
	   13,  89,  basey     + linenum * NEWS_SETTING_BASELINE_SKIP,  basey + 11 + linenum * NEWS_SETTING_BASELINE_SKIP, \
	  STR_EMPTY, STR_NULL}, \
	{ WWT_PUSHIMGBTN, RESIZE_NONE, COLOUR_YELLOW, \
	   90,  98,  basey     + linenum * NEWS_SETTING_BASELINE_SKIP,  basey + 11 + linenum * NEWS_SETTING_BASELINE_SKIP, \
	  SPR_ARROW_RIGHT, STR_HSCROLL_BAR_SCROLLS_LIST}, \
        { WWT_TEXT, RESIZE_NONE, COLOUR_YELLOW, \
	  103, 409,  basey + 1 + linenum * NEWS_SETTING_BASELINE_SKIP,  basey + 13 + linenum * NEWS_SETTING_BASELINE_SKIP, \
	  text, STR_NULL}

static const int NEWS_SETTING_BASELINE_SKIP = 12; ///< Distance between two news-setting lines, should be at least 12


static const Widget _message_options_widgets[] = {
{ WWT_CLOSEBOX, RESIZE_NONE, COLOUR_BROWN,   0,  10,  0, 13,
	STR_00C5,                 STR_018B_CLOSE_WINDOW},
{  WWT_CAPTION, RESIZE_NONE, COLOUR_BROWN,  11, 409,  0, 13,
	STR_0204_MESSAGE_OPTIONS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{    WWT_PANEL, RESIZE_NONE, COLOUR_BROWN,   0, 409, 14, 64 + NT_END * NEWS_SETTING_BASELINE_SKIP,
	0x0,                      STR_NULL},

/* Text at the top of the main panel, in black */
{    WWT_LABEL, RESIZE_NONE, COLOUR_BROWN,
	  0, 409, 13, 26,
	STR_0205_MESSAGE_TYPES,   STR_NULL},

/* General drop down and sound button, widgets WIDGET_NEWSOPT_BTN_SUMMARY and WIDGET_NEWSOPT_DROP_SUMMARY */
{  WWT_DROPDOWN, RESIZE_NONE, COLOUR_YELLOW,
	  4,  98,  34 + NT_END * NEWS_SETTING_BASELINE_SKIP,  45 + NT_END * NEWS_SETTING_BASELINE_SKIP,
	0x0, STR_NULL},

{      WWT_TEXT, RESIZE_NONE, COLOUR_YELLOW,
	103, 409,  35 + NT_END * NEWS_SETTING_BASELINE_SKIP,  47 + NT_END * NEWS_SETTING_BASELINE_SKIP,
	STR_MESSAGES_ALL, STR_NULL},

/* Below is widget WIDGET_NEWSOPT_SOUNDTICKER */
{ WWT_TEXTBTN_2, RESIZE_NONE, COLOUR_YELLOW,
	  4,  98,  46 + NT_END * NEWS_SETTING_BASELINE_SKIP,  57 + NT_END * NEWS_SETTING_BASELINE_SKIP,
	STR_02DB_OFF,  STR_NULL},

{      WWT_TEXT, RESIZE_NONE, COLOUR_YELLOW,
	103, 409,  47 + NT_END * NEWS_SETTING_BASELINE_SKIP,  59 + NT_END * NEWS_SETTING_BASELINE_SKIP,
	STR_MESSAGE_SOUND, STR_NULL},

/* List of news-setting lines (4 widgets for each line).
 * First widget must be number WIDGET_NEWSOPT_START_OPTION
 */
NEWS_SETTINGS_LINE(26, NT_ARRIVAL_PLAYER, STR_0206_ARRIVAL_OF_FIRST_VEHICLE),
NEWS_SETTINGS_LINE(26, NT_ARRIVAL_OTHER,  STR_0207_ARRIVAL_OF_FIRST_VEHICLE),
NEWS_SETTINGS_LINE(26, NT_ACCIDENT, STR_0208_ACCIDENTS_DISASTERS),
NEWS_SETTINGS_LINE(26, NT_COMPANY_INFO, STR_0209_COMPANY_INFORMATION),
NEWS_SETTINGS_LINE(26, NT_OPENCLOSE, STR_NEWS_OPEN_CLOSE),
NEWS_SETTINGS_LINE(26, NT_ECONOMY, STR_020A_ECONOMY_CHANGES),
NEWS_SETTINGS_LINE(26, NT_INDUSTRY_PLAYER, STR_INDUSTRY_CHANGES_SERVED_BY_PLAYER),
NEWS_SETTINGS_LINE(26, NT_INDUSTRY_OTHER, STR_INDUSTRY_CHANGES_SERVED_BY_OTHER),
NEWS_SETTINGS_LINE(26, NT_INDUSTRY_NOBODY, STR_OTHER_INDUSTRY_PRODUCTION_CHANGES),
NEWS_SETTINGS_LINE(26, NT_ADVICE, STR_020B_ADVICE_INFORMATION_ON_PLAYER),
NEWS_SETTINGS_LINE(26, NT_NEW_VEHICLES, STR_020C_NEW_VEHICLES),
NEWS_SETTINGS_LINE(26, NT_ACCEPTANCE, STR_020D_CHANGES_OF_CARGO_ACCEPTANCE),
NEWS_SETTINGS_LINE(26, NT_SUBSIDIES, STR_020E_SUBSIDIES),
NEWS_SETTINGS_LINE(26, NT_GENERAL, STR_020F_GENERAL_INFORMATION),

{   WIDGETS_END},
};

static const WindowDesc _message_options_desc = {
	270,  22,  410,  65 + NT_END * NEWS_SETTING_BASELINE_SKIP,
	           410,  65 + NT_END * NEWS_SETTING_BASELINE_SKIP,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_message_options_widgets,
	NULL
};

void ShowMessageOptions()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	new MessageOptionsWindow(&_message_options_desc);
}


void DeleteVehicleNews(VehicleID vid, StringID news)
{
	for (NewsID n = _oldest_news; _latest_news != INVALID_NEWS; n = IncreaseIndex(n)) {
		const NewsItem *ni = &_news_items[n];

		if (ni->flags & NF_VEHICLE &&
				ni->data_a == vid &&
				(news == INVALID_STRING_ID || ni->string_id == news)) {
			/* If we delete a forced news and it is just before the current news
			 * then we need to advance to the next news (if any) */
			if (_forced_news == n) MoveToNextItem();
			if (_forced_news == INVALID_NEWS && _current_news == n) MoveToNextItem();
			_total_news--;

			/* If this is the last news item, invalidate _latest_news */
			if (_total_news == 0) {
				assert(_latest_news == _oldest_news);
				_latest_news = INVALID_NEWS;
				_current_news = INVALID_NEWS;
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
				NewsWindow *w = dynamic_cast<NewsWindow*>(FindWindowById(WC_NEWS_WINDOW, 0));
				NewsID visible_news = (w != NULL) ? (NewsID)(w->ni - _news_items) : INVALID_NEWS;

				for (NewsID i = n;; i = DecreaseIndex(i)) {
					_news_items[i] = _news_items[DecreaseIndex(i)];

					if (i != _latest_news) {
						if (i == _current_news) _current_news = IncreaseIndex(_current_news);
						if (i == _forced_news) _forced_news = IncreaseIndex(_forced_news);
						if (i == visible_news) w->ni = &_news_items[IncreaseIndex(visible_news)];
					}

					if (i == _oldest_news) break;
				}
				_oldest_news = IncreaseIndex(_oldest_news);
			}

			/*DEBUG(misc, 0, "-cur %3d, old %2d, lat %3d, for %3d, tot %2d",
			  _current_news, _oldest_news, _latest_news, _forced_news, _total_news);*/

			Window *w = FindWindowById(WC_MESSAGE_HISTORY, 0);
			if (w != NULL) {
				w->SetDirty();
				w->vscroll.count = _total_news;
			}
		}

		if (n == _latest_news) break;
	}
}
