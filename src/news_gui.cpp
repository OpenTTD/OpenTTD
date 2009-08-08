/* $Id$ */

/** @file news_gui.cpp GUI functions related to news messages. */

#include "stdafx.h"
#include "openttd.h"
#include "gui.h"
#include "window_gui.h"
#include "viewport_func.h"
#include "news_type.h"
#include "settings_type.h"
#include "transparency.h"
#include "strings_func.h"
#include "window_func.h"
#include "date_func.h"
#include "vehicle_base.h"
#include "station_base.h"
#include "industry.h"
#include "town.h"
#include "sound_func.h"
#include "string_func.h"
#include "widgets/dropdown_func.h"
#include "statusbar_gui.h"
#include "company_manager_face.h"
#include "map_func.h"

#include "table/strings.h"

#define NB_WIDG_PER_SETTING 4

NewsItem _statusbar_news_item;
bool _news_ticker_sound;

static uint MIN_NEWS_AMOUNT = 30;           ///< prefered minimum amount of news messages
static uint _total_news = 0;                ///< current number of news items
static NewsItem *_oldest_news = NULL;       ///< head of news items queue
static NewsItem *_latest_news = NULL;       ///< tail of news items queue

/** Forced news item.
 * Users can force an item by accessing the history or "last message".
 * If the message being shown was forced by the user, a pointer is stored
 * in _forced_news. Otherwise, \a _forced_news variable is NULL. */
static NewsItem *_forced_news = NULL;       ///< item the user has asked for

/** Current news item (last item shown regularly). */
static NewsItem *_current_news = NULL;


typedef void DrawNewsCallbackProc(struct Window *w, const NewsItem *ni);
void DrawNewsNewVehicleAvail(Window *w, const NewsItem *ni);

static void DrawNewsBankrupcy(Window *w, const NewsItem *ni)
{
	const CompanyNewsInformation *cni = (const CompanyNewsInformation*)ni->free_data;

	DrawCompanyManagerFace(cni->face, cni->colour, 2, 23);
	GfxFillRect(3, 23, 3 + 91, 23 + 118, PALETTE_TO_STRUCT_GREY, FILLRECT_RECOLOUR);

	SetDParamStr(0, cni->president_name);
	DrawStringMultiLine(49 - MAX_LENGTH_PRESIDENT_NAME_PIXELS / 2, 49 + MAX_LENGTH_PRESIDENT_NAME_PIXELS / 2, 141, 169, STR_JUST_RAW_STRING, TC_FROMSTRING, SA_CENTER);

	switch (ni->subtype) {
		case NS_COMPANY_TROUBLE:
			DrawString(0, w->width, 1, STR_NEWS_COMPANY_IN_TROUBLE_TITLE, TC_FROMSTRING, SA_CENTER);

			SetDParam(0, ni->params[2]);

			DrawStringMultiLine(100, w->width - 2, 20, 169, STR_NEWS_COMPANY_IN_TROUBLE_DESCRIPTION, TC_FROMSTRING, SA_CENTER);
			break;

		case NS_COMPANY_MERGER:
			DrawString(0, w->width, 1, STR_NEWS_COMPANY_MERGER_TITLE, TC_FROMSTRING, SA_CENTER);
			SetDParam(0, ni->params[2]);
			SetDParam(1, ni->params[3]);
			SetDParam(2, ni->params[4]);
			DrawStringMultiLine(100, w->width - 2, 20, 169, ni->params[4] == 0 ? STR_NEWS_MERGER_TAKEOVER_TITLE : STR_NEWS_COMPANY_MERGER_DESCRIPTION, TC_FROMSTRING, SA_CENTER);
			break;

		case NS_COMPANY_BANKRUPT:
			DrawString(0, w->width, 1, STR_NEWS_COMPANY_BANKRUPT_TITLE, TC_FROMSTRING, SA_CENTER);
			SetDParam(0, ni->params[2]);
			DrawStringMultiLine(100, w->width - 2, 20, 169, STR_NEWS_COMPANY_BANKRUPT_DESCRIPTION, TC_FROMSTRING, SA_CENTER);
			break;

		case NS_COMPANY_NEW:
			DrawString(0, w->width, 1, STR_NEWS_COMPANY_LAUNCH_TITLE, TC_FROMSTRING, SA_CENTER);
			SetDParam(0, ni->params[2]);
			SetDParam(1, ni->params[3]);
			DrawStringMultiLine(100, w->width - 2, 20, 169, STR_NEWS_COMPANY_LAUNCH_DESCRIPTION, TC_FROMSTRING, SA_CENTER);
			break;

		default:
			NOT_REACHED();
	}
}

/**
 * Get the position a news-reference is referencing.
 * @param reftype The type of reference.
 * @param ref     The reference.
 * @return A tile for the referenced object, or INVALID_TILE if none.
 */
static TileIndex GetReferenceTile(NewsReferenceType reftype, uint32 ref)
{
	switch (reftype) {
		case NR_TILE:     return (TileIndex)ref;
		case NR_STATION:  return Station::Get((StationID)ref)->xy;
		case NR_INDUSTRY: return Industry::Get((IndustryID)ref)->xy + TileDiffXY(1, 1);
		case NR_TOWN:     return Town::Get((TownID)ref)->xy;
		default:          return INVALID_TILE;
	}
}

/**
 * Data common to all news items of a given subtype (structure)
 */
struct NewsSubtypeData {
	NewsType type;         ///< News category @see NewsType
	NewsMode display_mode; ///< Display mode value @see NewsMode
	NewsFlag flags;        ///< Initial NewsFlags bits @see NewsFlag
	DrawNewsCallbackProc *callback; ///< Call-back function
};

/**
 * Data common to all news items of a given subtype (actual data)
 */
static const NewsSubtypeData _news_subtype_data[] = {
	/* type,               display_mode, flags,                  callback */
	{ NT_ARRIVAL_COMPANY,  NM_THIN,     NF_VIEWPORT, NULL                    }, ///< NS_ARRIVAL_COMPANY
	{ NT_ARRIVAL_OTHER,    NM_THIN,     NF_VIEWPORT, NULL                    }, ///< NS_ARRIVAL_OTHER
	{ NT_ACCIDENT,         NM_THIN,     NF_VIEWPORT, NULL                    }, ///< NS_ACCIDENT
	{ NT_COMPANY_INFO,     NM_NORMAL,   NF_NONE,     DrawNewsBankrupcy       }, ///< NS_COMPANY_TROUBLE
	{ NT_COMPANY_INFO,     NM_NORMAL,   NF_NONE,     DrawNewsBankrupcy       }, ///< NS_COMPANY_MERGER
	{ NT_COMPANY_INFO,     NM_NORMAL,   NF_NONE,     DrawNewsBankrupcy       }, ///< NS_COMPANY_BANKRUPT
	{ NT_COMPANY_INFO,     NM_NORMAL,   NF_NONE,     DrawNewsBankrupcy       }, ///< NS_COMPANY_NEW
	{ NT_INDUSTRY_OPEN,    NM_THIN,     NF_VIEWPORT, NULL                    }, ///< NS_INDUSTRY_OPEN
	{ NT_INDUSTRY_CLOSE,   NM_THIN,     NF_VIEWPORT, NULL                    }, ///< NS_INDUSTRY_CLOSE
	{ NT_ECONOMY,          NM_NORMAL,   NF_NONE,     NULL                    }, ///< NS_ECONOMY
	{ NT_INDUSTRY_COMPANY, NM_THIN,     NF_VIEWPORT, NULL                    }, ///< NS_INDUSTRY_COMPANY
	{ NT_INDUSTRY_OTHER,   NM_THIN,     NF_VIEWPORT, NULL                    }, ///< NS_INDUSTRY_OTHER
	{ NT_INDUSTRY_NOBODY,  NM_THIN,     NF_VIEWPORT, NULL                    }, ///< NS_INDUSTRY_NOBODY
	{ NT_ADVICE,           NM_SMALL,    NF_VIEWPORT, NULL                    }, ///< NS_ADVICE
	{ NT_NEW_VEHICLES,     NM_NORMAL,   NF_NONE,     DrawNewsNewVehicleAvail }, ///< NS_NEW_VEHICLES
	{ NT_ACCEPTANCE,       NM_SMALL,    NF_VIEWPORT, NULL                    }, ///< NS_ACCEPTANCE
	{ NT_SUBSIDIES,        NM_NORMAL,   NF_NONE,     NULL                    }, ///< NS_SUBSIDIES
	{ NT_GENERAL,          NM_NORMAL,   NF_NONE,     NULL                    }, ///< NS_GENERAL
};

assert_compile(lengthof(_news_subtype_data) == NS_END);

/**
 * Per-NewsType data
 */
NewsTypeData _news_type_data[] = {
	/*            name,              age, sound,           description */
	NewsTypeData("arrival_player",    60, SND_1D_APPLAUSE, STR_NEWS_MESSAGE_TYPE_ARRIVAL_OF_FIRST_VEHICLE_OWN       ),  ///< NT_ARRIVAL_COMPANY
	NewsTypeData("arrival_other",     60, SND_1D_APPLAUSE, STR_NEWS_MESSAGE_TYPE_ARRIVAL_OF_FIRST_VEHICLE_OTHER     ),  ///< NT_ARRIVAL_OTHER
	NewsTypeData("accident",          90, SND_BEGIN,       STR_NEWS_MESSAGE_TYPE_ACCIDENTS_DISASTERS                ),  ///< NT_ACCIDENT
	NewsTypeData("company_info",      60, SND_BEGIN,       STR_NEWS_MESSAGE_TYPE_COMPANY_INFORMATION                ),  ///< NT_COMPANY_INFO
	NewsTypeData("open",              90, SND_BEGIN,       STR_NEWS_MESSAGE_TYPE_INDUSTRY_OPEN                      ),  ///< NT_INDUSTRY_OPEN
	NewsTypeData("close",             90, SND_BEGIN,       STR_NEWS_MESSAGE_TYPE_INDUSTRY_CLOSE                     ),  ///< NT_INDUSTRY_CLOSE
	NewsTypeData("economy",           30, SND_BEGIN,       STR_NEWS_MESSAGE_TYPE_ECONOMY_CHANGES                    ),  ///< NT_ECONOMY
	NewsTypeData("production_player", 30, SND_BEGIN,       STR_NEWS_MESSAGE_TYPE_INDUSTRY_CHANGES_SERVED_BY_COMPANY ),  ///< NT_INDUSTRY_COMPANY
	NewsTypeData("production_other",  30, SND_BEGIN,       STR_NEWS_MESSAGE_TYPE_INDUSTRY_CHANGES_SERVED_BY_OTHER   ),  ///< NT_INDUSTRY_OTHER
	NewsTypeData("production_nobody", 30, SND_BEGIN,       STR_NEWS_MESSAGE_TYPE_INDUSTRY_CHANGES_UNSERVED          ),  ///< NT_INDUSTRY_NOBODY
	NewsTypeData("advice",           150, SND_BEGIN,       STR_NEWS_MESSAGE_TYPE_ADVICE_INFORMATION_ON_COMPANY      ),  ///< NT_ADVICE
	NewsTypeData("new_vehicles",      30, SND_1E_OOOOH,    STR_NEWS_MESSAGE_TYPE_NEW_VEHICLES                       ),  ///< NT_NEW_VEHICLES
	NewsTypeData("acceptance",        90, SND_BEGIN,       STR_NEWS_MESSAGE_TYPE_CHANGES_OF_CARGO_ACCEPTANCE        ),  ///< NT_ACCEPTANCE
	NewsTypeData("subsidies",        180, SND_BEGIN,       STR_NEWS_MESSAGE_TYPE_SUBSIDIES                          ),  ///< NT_SUBSIDIES
	NewsTypeData("general",           60, SND_BEGIN,       STR_NEWS_MESSAGE_TYPE_GENERAL_INFORMATION                ),  ///< NT_GENERAL
};

assert_compile(lengthof(_news_type_data) == NT_END);

/** Widget numbers of the news display windows. */
enum NewsTypeWidgets {
	NTW_HEADLINE, ///< The news headline.
	NTW_CLOSEBOX, ///< Close the window.
	NTW_CAPTION,  ///< Title bar of the window. Only used in type0-news.
	NTW_VIEWPORT, ///< Viewport in window. Only used in type0-news.
};

struct NewsWindow : Window {
	uint16 chat_height;
	NewsItem *ni;
	static uint duration;

	NewsWindow(const WindowDesc *desc, NewsItem *ni) : Window(desc), ni(ni)
	{
		NewsWindow::duration = 555;
		const Window *w = FindWindowById(WC_SEND_NETWORK_MSG, 0);
		this->chat_height = (w != NULL) ? w->height : 0;

		this->ni = _forced_news == NULL ? _current_news : _forced_news;
		this->flags4 |= WF_DISABLE_VP_SCROLL;

		this->FindWindowPlacementAndResize(desc);
	}

	void DrawNewsBorder()
	{
		int left = 0;
		int right = this->width - 1;
		int top = 0;
		int bottom = this->height - 1;

		GfxFillRect(left,  top,    right, bottom, 0xF);

		GfxFillRect(left,  top,    left,  bottom, 0xD7);
		GfxFillRect(right, top,    right, bottom, 0xD7);
		GfxFillRect(left,  top,    right, top,    0xD7);
		GfxFillRect(left,  bottom, right, bottom, 0xD7);

		DrawString(left + 2, right - 2, top + 1, STR_SILVER_CROSS);
	}

	virtual void OnPaint()
	{
		const NewsMode display_mode = _news_subtype_data[this->ni->subtype].display_mode;

		switch (display_mode) {
			case NM_NORMAL:
			case NM_THIN: {
				this->DrawNewsBorder();

				if (_news_subtype_data[this->ni->subtype].callback != NULL) {
					(_news_subtype_data[this->ni->subtype].callback)(this, ni);
					break;
				}

				DrawString(2, this->width - 1, 1, STR_SILVER_CROSS);

				SetDParam(0, this->ni->date);
				DrawString(2, this->width - 2, 1, STR_DATE_LONG_SMALL, TC_FROMSTRING, SA_RIGHT);

				if (!(this->ni->flags & NF_VIEWPORT)) {
					CopyInDParam(0, this->ni->params, lengthof(this->ni->params));
					DrawStringMultiLine(2, this->width - 2, 20, this->height, this->ni->string_id, TC_FROMSTRING, SA_CENTER);
				} else {
					/* Back up transparency options to draw news view */
					TransparencyOptionBits to_backup = _transparency_opt;
					_transparency_opt = 0;
					this->DrawViewport();
					_transparency_opt = to_backup;

					/* Shade the viewport into gray, or colour*/
					ViewPort *vp = this->viewport;
					GfxFillRect(vp->left - this->left, vp->top - this->top,
						vp->left - this->left + vp->width - 1, vp->top - this->top + vp->height - 1,
						(this->ni->flags & NF_INCOLOUR ? PALETTE_TO_TRANSPARENT : PALETTE_TO_STRUCT_GREY), FILLRECT_RECOLOUR
					);

					CopyInDParam(0, this->ni->params, lengthof(this->ni->params));
					DrawStringMultiLine(2, this->width - 2, 0, 58, this->ni->string_id, TC_FROMSTRING, SA_CENTER);
				}
				break;
			}

			default:
				this->DrawWidgets();
				if (!(this->ni->flags & NF_VIEWPORT)) {
					CopyInDParam(0, this->ni->params, lengthof(this->ni->params));
					DrawStringMultiLine(2, 278, 38, this->ni->string_id, SA_CENTER);
				} else {
					this->DrawViewport();
					CopyInDParam(0, this->ni->params, lengthof(this->ni->params));
					DrawStringMultiLine(2, this->width - 2, 64, this->height, this->ni->string_id, TC_FROMSTRING, SA_CENTER);
				}
				break;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case NTW_CLOSEBOX:
				NewsWindow::duration = 0;
				delete this;
				_forced_news = NULL;
				break;

			case NTW_HEADLINE:
				if (this->ni->reftype1 == NR_VEHICLE) {
					const Vehicle *v = Vehicle::Get(this->ni->ref1);
					ScrollMainWindowTo(v->x_pos, v->y_pos, v->z_pos);
				} else {
					TileIndex tile1 = GetReferenceTile(this->ni->reftype1, this->ni->ref1);
					TileIndex tile2 = GetReferenceTile(this->ni->reftype2, this->ni->ref2);
					if (_ctrl_pressed) {
						if (tile1 != INVALID_TILE) ShowExtraViewPortWindow(tile1);
						if (tile2 != INVALID_TILE) ShowExtraViewPortWindow(tile2);
					} else {
						if (((tile1 == INVALID_TILE) || !ScrollMainWindowToTile(tile1)) && (tile2 != INVALID_TILE)) {
							ScrollMainWindowToTile(tile2);
						}
					}
				}
				break;
		}
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		if (keycode == WKC_SPACE) {
			/* Don't continue. */
			delete this;
			return ES_HANDLED;
		}
		return ES_NOT_HANDLED;
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

/* static */ uint NewsWindow::duration; ///< Remaining time for showing current news message


static const Widget _news_type13_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_WHITE,     0,   429,     0,   169, 0x0, STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_WHITE,     0,    10,     0,    11, 0x0, STR_NULL},
{   WIDGETS_END},
};

static const NWidgetPart _nested_news_type13_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_WHITE, NTW_HEADLINE),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_WHITE, NTW_CLOSEBOX), SetMinimalSize(11, 12), EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(419, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 158),
	EndContainer(),
};

static WindowDesc _news_type13_desc(
	WDP_CENTER, 476, 430, 170, 430, 170,
	WC_NEWS_WINDOW, WC_NONE,
	WDF_DEF_WIDGET,
	_news_type13_widgets, _nested_news_type13_widgets, lengthof(_nested_news_type13_widgets)
);

static const Widget _news_type2_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_WHITE,     0,   429,     0,   129, 0x0, STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_WHITE,     0,    10,     0,    11, 0x0, STR_NULL},
{   WIDGETS_END},
};

static const NWidgetPart _nested_news_type2_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_WHITE, NTW_HEADLINE),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_WHITE, NTW_CLOSEBOX), SetMinimalSize(11, 12), EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(419, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 118),
	EndContainer(),
};

static WindowDesc _news_type2_desc(
	WDP_CENTER, 476, 430, 130, 430, 130,
	WC_NEWS_WINDOW, WC_NONE,
	WDF_DEF_WIDGET,
	_news_type2_widgets, _nested_news_type2_widgets, lengthof(_nested_news_type2_widgets)
);

static const Widget _news_type0_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_LIGHT_BLUE,     0,   279,    14,    86, 0x0,                      STR_NULL},
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_LIGHT_BLUE,     0,    10,     0,    13, STR_BLACK_CROSS,          STR_TOOLTIP_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_LIGHT_BLUE,    11,   279,     0,    13, STR_NEWS_MESSAGE_CAPTION, STR_NULL},
{      WWT_INSET,   RESIZE_NONE,  COLOUR_LIGHT_BLUE,     2,   277,    16,    64, 0x0,                      STR_NULL},
{   WIDGETS_END},
};

static NWidgetPart _nested_news_type0_widgets[] = {
	/* Caption + close box */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE, NTW_CLOSEBOX), SetMinimalSize(11, 14), SetDataTip(STR_BLACK_CROSS, STR_TOOLTIP_CLOSE_WINDOW),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE, NTW_CAPTION), SetMinimalSize(269, 14), SetDataTip(STR_NEWS_MESSAGE_CAPTION, STR_NULL),
	EndContainer(),

	/* Main part */
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, NTW_HEADLINE),
		NWidget(NWID_SPACER), SetMinimalSize(0, 2),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),

			NWidget(WWT_INSET, COLOUR_LIGHT_BLUE, NTW_VIEWPORT), SetMinimalSize(276, 49),
			EndContainer(),

			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 22),
	EndContainer(),
};

static WindowDesc _news_type0_desc(
	WDP_CENTER, 476, 280, 87, 280, 87,
	WC_NEWS_WINDOW, WC_NONE,
	WDF_DEF_WIDGET,
	_news_type0_widgets,
	_nested_news_type0_widgets, lengthof(_nested_news_type0_widgets)
);


/** Open up an own newspaper window for the news item */
static void ShowNewspaper(NewsItem *ni)
{
	SoundFx sound = _news_type_data[_news_subtype_data[ni->subtype].type].sound;
	if (sound != 0) SndPlayFx(sound);

	int top = _screen.height;
	Window *w;
	switch (_news_subtype_data[ni->subtype].display_mode) {
		case NM_NORMAL:
			_news_type13_desc.top = top;
			w = new NewsWindow(&_news_type13_desc, ni);
			if (ni->flags & NF_VIEWPORT) {
				InitializeWindowViewport(w, 2, 58, 426, 110,
					ni->reftype1 == NR_VEHICLE ? 0x80000000 | ni->ref1 : GetReferenceTile(ni->reftype1, ni->ref1), ZOOM_LVL_NEWS);
			}
			break;

		case NM_THIN:
			_news_type2_desc.top = top;
			w = new NewsWindow(&_news_type2_desc, ni);
			if (ni->flags & NF_VIEWPORT) {
				InitializeWindowViewport(w, 2, 58, 426, 70,
					ni->reftype1 == NR_VEHICLE ? 0x80000000 | ni->ref1 : GetReferenceTile(ni->reftype1, ni->ref1), ZOOM_LVL_NEWS);
			}
			break;

		default:
			_news_type0_desc.top = top;
			w = new NewsWindow(&_news_type0_desc, ni);
			if (ni->flags & NF_VIEWPORT) {
				InitializeWindowViewport(w, 3, 17, 274, 47,
					ni->reftype1 == NR_VEHICLE ? 0x80000000 | ni->ref1 : GetReferenceTile(ni->reftype1, ni->ref1), ZOOM_LVL_NEWS);
			}
			break;
	}
}

/** Show news item in the ticker */
static void ShowTicker(const NewsItem *ni)
{
	if (_news_ticker_sound) SndPlayFx(SND_16_MORSE);

	_statusbar_news_item = *ni;
	InvalidateWindowData(WC_STATUS_BAR, 0, SBI_SHOW_TICKER);
}

/** Initialize the news-items data structures */
void InitNewsItemStructs()
{
	for (NewsItem *ni = _oldest_news; ni != NULL; ) {
		NewsItem *next = ni->next;
		delete ni;
		ni = next;
	}

	_total_news = 0;
	_oldest_news = NULL;
	_latest_news = NULL;
	_forced_news = NULL;
	_current_news = NULL;
}

/**
 * Are we ready to show another news item?
 * Only if nothing is in the newsticker and no newspaper is displayed
 */
static bool ReadyForNextItem()
{
	NewsItem *ni = _forced_news == NULL ? _current_news : _forced_news;
	if (ni == NULL) return true;

	/* Ticker message
	 * Check if the status bar message is still being displayed? */
	if (IsNewsTickerShown()) return false;

	/* Newspaper message, decrement duration counter */
	if (NewsWindow::duration != 0) NewsWindow::duration--;

	/* neither newsticker nor newspaper are running */
	return (NewsWindow::duration == 0 || FindWindowById(WC_NEWS_WINDOW, 0) == NULL);
}

/** Move to the next news item */
static void MoveToNextItem()
{
	InvalidateWindowData(WC_STATUS_BAR, 0, SBI_NEWS_DELETED); // invalidate the statusbar
	DeleteWindowById(WC_NEWS_WINDOW, 0); // close the newspapers window if shown
	_forced_news = NULL;

	/* if we're not at the last item, then move on */
	if (_current_news != _latest_news) {
		_current_news = (_current_news == NULL) ? _oldest_news : _current_news->next;
		NewsItem *ni = _current_news;
		const NewsType type = _news_subtype_data[ni->subtype].type;

		/* check the date, don't show too old items */
		if (_date - _news_type_data[type].age > ni->date) return;

		switch (_news_type_data[type].display) {
			default: NOT_REACHED();
			case ND_OFF: // Off - show nothing only a small reminder in the status bar
				InvalidateWindowData(WC_STATUS_BAR, 0, SBI_SHOW_REMINDER);
				break;

			case ND_SUMMARY: // Summary - show ticker
				ShowTicker(ni);
				break;

			case ND_FULL: // Full - show newspaper
				ShowNewspaper(ni);
				break;
		}
	}
}

/**
 * Add a new newsitem to be shown.
 * @param string String to display
 * @param subtype news category, any of the NewsSubtype enums (NS_)
 * @param reftype1 Type of ref1
 * @param ref1     Reference 1 to some object: Used for a possible viewport, scrolling after clicking on the news, and for deleteing the news when the object is deleted.
 * @param reftype2 Type of ref2
 * @param ref2     Reference 2 to some object: Used for scrolling after clicking on the news, and for deleteing the news when the object is deleted.
 *
 * @see NewsSubype
 */
void AddNewsItem(StringID string, NewsSubtype subtype, NewsReferenceType reftype1, uint32 ref1, NewsReferenceType reftype2, uint32 ref2, void *free_data)
{
	if (_game_mode == GM_MENU) return;

	/* Create new news item node */
	NewsItem *ni = new NewsItem;

	ni->string_id = string;
	ni->subtype = subtype;
	ni->flags = _news_subtype_data[subtype].flags;

	/* show this news message in colour? */
	if (_cur_year >= _settings_client.gui.coloured_news_year) ni->flags |= NF_INCOLOUR;

	ni->reftype1 = reftype1;
	ni->reftype2 = reftype2;
	ni->ref1 = ref1;
	ni->ref2 = ref2;
	ni->free_data = free_data;
	ni->date = _date;
	CopyOutDParam(ni->params, 0, lengthof(ni->params));

	if (_total_news++ == 0) {
		assert(_oldest_news == NULL);
		_oldest_news = ni;
		ni->prev = NULL;
	} else {
		assert(_latest_news->next == NULL);
		_latest_news->next = ni;
		ni->prev = _latest_news;
	}

	ni->next = NULL;
	_latest_news = ni;

	InvalidateWindow(WC_MESSAGE_HISTORY, 0);
}

/** Delete a news item from the queue */
static void DeleteNewsItem(NewsItem *ni)
{
	if (_forced_news == ni || _current_news == ni) {
		/* about to remove the currently forced item (shown as newspapers) ||
		 * about to remove the currently displayed item (newspapers, ticker, or just a reminder) */
		MoveToNextItem();
	}

	/* delete item */

	if (ni->prev != NULL) {
		ni->prev->next = ni->next;
	} else {
		assert(_oldest_news == ni);
		_oldest_news = ni->next;
	}

	if (ni->next != NULL) {
		ni->next->prev = ni->prev;
	} else {
		assert(_latest_news == ni);
		_latest_news = ni->prev;
	}

	free(ni->free_data);

	if (_current_news == ni) _current_news = ni->prev;
	_total_news--;
	delete ni;

	InvalidateWindow(WC_MESSAGE_HISTORY, 0);
}

void DeleteVehicleNews(VehicleID vid, StringID news)
{
	NewsItem *ni = _oldest_news;

	while (ni != NULL) {
		NewsItem *next = ni->next;
		if (((ni->reftype1 == NR_VEHICLE && ni->ref1 == vid) || (ni->reftype2 == NR_VEHICLE && ni->ref2 == vid)) &&
				(news == INVALID_STRING_ID || ni->string_id == news)) {
			DeleteNewsItem(ni);
		}
		ni = next;
	}
}

/** Remove news regarding given station so there are no 'unknown station now accepts Mail'
 * or 'First train arrived at unknown station' news items.
 * @param sid station to remove news about
 */
void DeleteStationNews(StationID sid)
{
	NewsItem *ni = _oldest_news;

	while (ni != NULL) {
		NewsItem *next = ni->next;
		if ((ni->reftype1 == NR_STATION && ni->ref1 == sid) || (ni->reftype2 == NR_STATION && ni->ref2 == sid)) {
			DeleteNewsItem(ni);
		}
		ni = next;
	}
}

/** Remove news regarding given industry
 * @param iid industry to remove news about
 */
void DeleteIndustryNews(IndustryID iid)
{
	NewsItem *ni = _oldest_news;

	while (ni != NULL) {
		NewsItem *next = ni->next;
		if ((ni->reftype1 == NR_INDUSTRY && ni->ref1 == iid) || (ni->reftype2 == NR_INDUSTRY && ni->ref2 == iid)) {
			DeleteNewsItem(ni);
		}
		ni = next;
	}
}

void RemoveOldNewsItems()
{
	NewsItem *next;
	for (NewsItem *cur = _oldest_news; _total_news > MIN_NEWS_AMOUNT && cur != NULL; cur = next) {
		next = cur->next;
		if (_date - _news_type_data[_news_subtype_data[cur->subtype].type].age * _settings_client.gui.news_message_timeout > cur->date) DeleteNewsItem(cur);
	}
}

void NewsLoop()
{
	/* no news item yet */
	if (_total_news == 0) return;

	static byte _last_clean_month = 0;

	if (_last_clean_month != _cur_month) {
		RemoveOldNewsItems();
		_last_clean_month = _cur_month;
	}

	if (ReadyForNextItem()) MoveToNextItem();
}

/** Do a forced show of a specific message */
static void ShowNewsMessage(NewsItem *ni)
{
	assert(_total_news != 0);

	/* Delete the news window */
	DeleteWindowById(WC_NEWS_WINDOW, 0);

	/* setup forced news item */
	_forced_news = ni;

	if (_forced_news != NULL) {
		DeleteWindowById(WC_NEWS_WINDOW, 0);
		ShowNewspaper(ni);
	}
}

/** Show previous news item */
void ShowLastNewsMessage()
{
	if (_total_news == 0) {
		return;
	} else if (_forced_news == NULL) {
		/* Not forced any news yet, show the current one, unless a news window is
		 * open (which can only be the current one), then show the previous item */
		const Window *w = FindWindowById(WC_NEWS_WINDOW, 0);
		ShowNewsMessage((w == NULL || (_current_news == _oldest_news)) ? _current_news : _current_news->prev);
	} else if (_forced_news == _oldest_news) {
		/* We have reached the oldest news, start anew with the latest */
		ShowNewsMessage(_latest_news);
	} else {
		/* 'Scrolling' through news history show each one in turn */
		ShowNewsMessage(_forced_news->prev);
	}
}


/**
 * Draw an unformatted news message truncated to a maximum length. If
 * length exceeds maximum length it will be postfixed by '...'
 * @param x,y position of the string
 * @param colour the colour the string will be shown in
 * @param *ni NewsItem being printed
 * @param maxw maximum width of string in pixels
 */
static void DrawNewsString(int x, int y, TextColour colour, const NewsItem *ni, uint maxw)
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
	DrawString(x, x + maxw, y, buffer2, colour);
}

/** Widget numbers of the message history window. */
enum MessageHistoryWidgets {
	MHW_CLOSEBOX,
	MHW_CAPTION,
	MHW_STICKYBOX,
	MHW_BACKGROUND,
	MHW_SCROLLBAR,
	MHW_RESIZEBOX,
};

struct MessageHistoryWindow : Window {
	static const int top_spacing;    ///< Additional spacing at the top of the #MHW_BACKGROUND widget.
	static const int bottom_spacing; ///< Additional spacing at the bottom of the #MHW_BACKGROUND widget.

	int line_height; /// < Height of a single line in the news histoy window including spacing.
	int date_width;  /// < Width needed for the date part.

	MessageHistoryWindow(const WindowDesc *desc) : Window()
	{
		this->InitNested(desc); // Initializes 'this->line_height' and 'this->date_width'.
		this->vscroll.cap = (this->nested_array[MHW_BACKGROUND]->current_y - this->top_spacing - this->bottom_spacing) / this->line_height;
		this->OnInvalidateData(0);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		if (widget == MHW_BACKGROUND) {
			this->line_height = FONT_HEIGHT_NORMAL + 2;
			resize->height = this->line_height;

			SetDParam(0, ConvertYMDToDate(2024, 7, 28));
			this->date_width = GetStringBoundingBox(STR_SHORT_DATE).width;

			size->height = 4 * resize->height + this->top_spacing + this->bottom_spacing; // At least 4 lines are visible.
			size->width = max(200u, size->width); // At least 200 pixels wide.
		}
	}

	virtual void OnPaint()
	{
		this->OnInvalidateData(0);
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != MHW_BACKGROUND || _total_news == 0) return;

		/* Find the first news item to display. */
		NewsItem *ni = _latest_news;
		for (int n = this->vscroll.pos; n > 0; n--) {
			ni = ni->prev;
			if (ni == NULL) return;
		}

		/* Fill the widget with news items. */
		int y = r.top + this->top_spacing;
		const int date_left = r.left + WD_FRAMETEXT_LEFT;       // Left edge of dates
		const int news_left = date_left + this->date_width + 5; // Left edge of news items
		for (int n = this->vscroll.cap; n > 0; n--) {
			SetDParam(0, ni->date);
			DrawString(date_left, news_left, y, STR_SHORT_DATE);

			DrawNewsString(news_left, y, TC_WHITE, ni, r.right - WD_FRAMETEXT_RIGHT - news_left);
			y += this->line_height;

			ni = ni->prev;
			if (ni == NULL) return;
		}
	}

	virtual void OnInvalidateData(int data)
	{
		SetVScrollCount(this, _total_news);
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget == MHW_BACKGROUND) {
			NewsItem *ni = _latest_news;
			if (ni == NULL) return;

			for (int n = (pt.y - this->nested_array[MHW_BACKGROUND]->pos_y - WD_FRAMERECT_TOP) / this->line_height + this->vscroll.pos; n > 0; n--) {
				ni = ni->prev;
				if (ni == NULL) return;
			}

			ShowNewsMessage(ni);
		}
	}

	virtual void OnResize(Point delta)
	{
		this->vscroll.cap += delta.y / this->line_height;
		this->OnInvalidateData(0);
	}
};

const int MessageHistoryWindow::top_spacing = WD_FRAMERECT_TOP + 4;
const int MessageHistoryWindow::bottom_spacing = WD_FRAMERECT_BOTTOM;

static const NWidgetPart _nested_message_history[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN, MHW_CLOSEBOX), SetMinimalSize(11, 14), SetDataTip(STR_BLACK_CROSS, STR_TOOLTIP_CLOSE_WINDOW),
		NWidget(WWT_CAPTION, COLOUR_BROWN, MHW_CAPTION), SetDataTip(STR_MESSAGE_HISTORY, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN, MHW_STICKYBOX), SetMinimalSize(12, 14), SetDataTip(0x0, STR_TOOLTIP_STICKY),
	EndContainer(),

	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN, MHW_BACKGROUND), SetMinimalSize(200, 125), SetDataTip(0x0, STR_MESSAGE_HISTORY_TOOLTIP), SetResize(1, 12),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_SCROLLBAR, COLOUR_BROWN, MHW_SCROLLBAR), SetFill(0, 1), SetDataTip(0x0, STR_TOOLTIP_VSCROLL_BAR_SCROLLS_LIST), SetResize(0, 1),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN, MHW_RESIZEBOX), SetMinimalSize(12, 12), SetDataTip(0x0, STR_TOOLTIP_RESIZE),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _message_history_desc(
	240, 22, 400, 140, 400, 140,
	WC_MESSAGE_HISTORY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	NULL, _nested_message_history, lengthof(_nested_message_history)
);

/** Display window with news messages history */
void ShowMessageHistory()
{
	DeleteWindowById(WC_MESSAGE_HISTORY, 0);
	new MessageHistoryWindow(&_message_history_desc);
}


/** News settings window widget offset constants */
enum NewsSettingsWidgets {
	WIDGET_NEWSOPT_CLOSEBOX,          ///< Close box.
	WIDGET_NEWSOPT_CAPTION,           ///< Caption.
	WIDGET_NEWSOPT_BACKGROUND,        ///< Background widget.
	WIDGET_NEWSOPT_LABEL,             ///< Top label.
	WIDGET_NEWSOPT_DROP_SUMMARY,      ///< Dropdown that adjusts at once the level for all settings.
	WIDGET_NEWSOPT_LABEL_SUMMARY,     ///< Label of the summary drop down.
	WIDGET_NEWSOPT_SOUNDTICKER,       ///< Button for (de)activating sound on events.
	WIDGET_NEWSOPT_SOUNDTICKER_LABEL, ///< Label of the soundticker button,

	WIDGET_NEWSOPT_START_OPTION,      ///< First widget that is part of a group [<] .. [.]
};

static const StringID _message_opt[] = {STR_NEWS_MESSAGES_OFF, STR_NEWS_MESSAGES_SUMMARY, STR_NEWS_MESSAGES_FULL, INVALID_STRING_ID};

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

		this->FindWindowPlacementAndResize(desc);
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
		for (int i = 0; i < NT_END; i++) {
			DrawString(this->widget[WIDGET_NEWSOPT_START_OPTION + 1].left, this->widget[WIDGET_NEWSOPT_START_OPTION + 1].right,
					this->widget[WIDGET_NEWSOPT_START_OPTION + NB_WIDG_PER_SETTING * i + 1].top + 1, _message_opt[_news_type_data[i].display], TC_BLACK, SA_CENTER);
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
 * Main part of the window is a list of news setting lines, one for each news category.
 * Each line is constructed by an expansion of the \c NEWS_SETTINGS_LINE macro
 */

/**
 * Macro to construct one news setting line in the news - settings window.
 * One line consists of four widgets, namely
 * - A [<] button
 * - A [...] label
 * - A [>] button
 * - A text label describing the news category
 * Horizontal positions of the widgets are hard coded, vertical start position is (\a basey + \a linenum * \c NEWS_SETTING_BASELINE_SKIP).
 * Height of one line is 12, with the text label shifted 1 pixel down.
 *
 * First line should be widget number WIDGET_NEWSOPT_START_OPTION
 *
 * @param basey: Base Y coordinate
 * @param linenum: Count, news - setting is the \a linenum - th line
 */
#define NEWS_SETTINGS_LINE(basey, linenum) \
	{ WWT_PUSHIMGBTN, RESIZE_NONE, COLOUR_YELLOW, \
	    4,  12,  basey     + linenum * NEWS_SETTING_BASELINE_SKIP,  basey + 11 + linenum * NEWS_SETTING_BASELINE_SKIP, \
	  SPR_ARROW_LEFT, STR_TOOLTIP_HSCROLL_BAR_SCROLLS_LIST}, \
	{ WWT_PUSHTXTBTN, RESIZE_NONE, COLOUR_YELLOW, \
	   13,  89,  basey     + linenum * NEWS_SETTING_BASELINE_SKIP,  basey + 11 + linenum * NEWS_SETTING_BASELINE_SKIP, \
	  STR_EMPTY, STR_NULL}, \
	{ WWT_PUSHIMGBTN, RESIZE_NONE, COLOUR_YELLOW, \
	   90,  98,  basey     + linenum * NEWS_SETTING_BASELINE_SKIP,  basey + 11 + linenum * NEWS_SETTING_BASELINE_SKIP, \
	  SPR_ARROW_RIGHT, STR_TOOLTIP_HSCROLL_BAR_SCROLLS_LIST}, \
        { WWT_TEXT, RESIZE_NONE, COLOUR_YELLOW, \
	  103, 409,  basey + linenum * NEWS_SETTING_BASELINE_SKIP,  basey + 11 + linenum * NEWS_SETTING_BASELINE_SKIP, \
	  _news_type_data[linenum].description, STR_NULL}

static const int NEWS_SETTING_BASELINE_SKIP = 12; ///< Distance between two news-setting lines, should be at least 12


static const Widget _message_options_widgets[] = {
{ WWT_CLOSEBOX, RESIZE_NONE, COLOUR_BROWN,   0,  10,  0, 13,
	STR_BLACK_CROSS,                 STR_TOOLTIP_CLOSE_WINDOW},
{  WWT_CAPTION, RESIZE_NONE, COLOUR_BROWN,  11, 409,  0, 13,
	STR_NEWS_MESSAGE_OPTIONS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},
{    WWT_PANEL, RESIZE_NONE, COLOUR_BROWN,   0, 409, 14, 64 + NT_END * NEWS_SETTING_BASELINE_SKIP,
	0x0,                      STR_NULL},

/* Text at the top of the main panel, in black */
{    WWT_LABEL, RESIZE_NONE, COLOUR_BROWN,
	  0, 409, 14, 27,
	STR_NEWS_MESSAGE_TYPES,   STR_NULL},

/* General drop down and sound button, widgets WIDGET_NEWSOPT_BTN_SUMMARY and WIDGET_NEWSOPT_DROP_SUMMARY */
{  WWT_DROPDOWN, RESIZE_NONE, COLOUR_YELLOW,
	  4,  98,  34 + NT_END * NEWS_SETTING_BASELINE_SKIP,  45 + NT_END * NEWS_SETTING_BASELINE_SKIP,
	0x0, STR_NULL},

{      WWT_TEXT, RESIZE_NONE, COLOUR_YELLOW,
	103, 409,  34 + NT_END * NEWS_SETTING_BASELINE_SKIP,  45 + NT_END * NEWS_SETTING_BASELINE_SKIP,
	STR_NEWS_MESSAGES_ALL, STR_NULL},

/* Below is widget WIDGET_NEWSOPT_SOUNDTICKER */
{ WWT_TEXTBTN_2, RESIZE_NONE, COLOUR_YELLOW,
	  4,  98,  46 + NT_END * NEWS_SETTING_BASELINE_SKIP,  57 + NT_END * NEWS_SETTING_BASELINE_SKIP,
	STR_STATION_BUILD_COVERAGE_OFF,  STR_NULL},

{      WWT_TEXT, RESIZE_NONE, COLOUR_YELLOW,
	103, 409,  46 + NT_END * NEWS_SETTING_BASELINE_SKIP,  57 + NT_END * NEWS_SETTING_BASELINE_SKIP,
	STR_NEWS_MESSAGES_SOUND, STR_NULL},

/* List of news-setting lines (4 widgets for each line).
 * First widget must be number WIDGET_NEWSOPT_START_OPTION
 */
NEWS_SETTINGS_LINE(28, NT_ARRIVAL_COMPANY),
NEWS_SETTINGS_LINE(28, NT_ARRIVAL_OTHER),
NEWS_SETTINGS_LINE(28, NT_ACCIDENT),
NEWS_SETTINGS_LINE(28, NT_COMPANY_INFO),
NEWS_SETTINGS_LINE(28, NT_INDUSTRY_OPEN),
NEWS_SETTINGS_LINE(28, NT_INDUSTRY_CLOSE),
NEWS_SETTINGS_LINE(28, NT_ECONOMY),
NEWS_SETTINGS_LINE(28, NT_INDUSTRY_COMPANY),
NEWS_SETTINGS_LINE(28, NT_INDUSTRY_OTHER),
NEWS_SETTINGS_LINE(28, NT_INDUSTRY_NOBODY),
NEWS_SETTINGS_LINE(28, NT_ADVICE),
NEWS_SETTINGS_LINE(28, NT_NEW_VEHICLES),
NEWS_SETTINGS_LINE(28, NT_ACCEPTANCE),
NEWS_SETTINGS_LINE(28, NT_SUBSIDIES),
NEWS_SETTINGS_LINE(28, NT_GENERAL),

{   WIDGETS_END},
};

/**
 * Make nested widget tree for the news settings.
 * @param biggest_index Storage for collecting the biggest index used in the returned tree.
 * @return Panel with rows of news settings.
 * @postcond \c *biggest_index contains the largest used index in the tree.
 */
static NWidgetBase *MakeNewsSettingLines(int *biggest_index)
{
	const int NEWS_SETTING_HEIGHT = 12; // Height of one line.
	NWidgetVertical *vert = new NWidgetVertical;

	int widnum = WIDGET_NEWSOPT_START_OPTION;
	for (int i = 0; i < NT_END; i++) {
		NWidgetHorizontal *hor = new NWidgetHorizontal;
		hor->SetPIP(4, 0, 0);
		/* [<] button. */
		NWidgetLeaf *leaf = new NWidgetLeaf(WWT_PUSHIMGBTN, COLOUR_YELLOW, widnum++, SPR_ARROW_LEFT, STR_TOOLTIP_HSCROLL_BAR_SCROLLS_LIST);
		leaf->SetMinimalSize(9, NEWS_SETTING_HEIGHT);
		hor->Add(leaf);
		/* Label. */
		leaf = new NWidgetLeaf(WWT_PUSHTXTBTN, COLOUR_YELLOW, widnum++, STR_EMPTY, STR_NULL);
		leaf->SetMinimalSize(77, NEWS_SETTING_HEIGHT);
		hor->Add(leaf);
		/* [>] button. */
		leaf = new NWidgetLeaf(WWT_PUSHIMGBTN, COLOUR_YELLOW, widnum++, SPR_ARROW_RIGHT, STR_TOOLTIP_HSCROLL_BAR_SCROLLS_LIST);
		leaf->SetMinimalSize(9, NEWS_SETTING_HEIGHT);
		hor->Add(leaf);
		/* Descriptive text. */
		leaf = new NWidgetLeaf(WWT_TEXT, COLOUR_YELLOW, widnum++, _news_type_data[i].description, STR_NULL);
		leaf->SetMinimalSize(307, NEWS_SETTING_HEIGHT);
		leaf->SetPadding(0, 0, 0, 4);
		hor->Add(leaf);

		vert->Add(hor);
	}
	*biggest_index = widnum - 1;
	return vert;
}

static const NWidgetPart _nested_message_options_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN, WIDGET_NEWSOPT_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WIDGET_NEWSOPT_CAPTION), SetDataTip(STR_NEWS_MESSAGE_OPTIONS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WIDGET_NEWSOPT_BACKGROUND),
		NWidget(WWT_LABEL, COLOUR_BROWN, WIDGET_NEWSOPT_LABEL), SetMinimalSize(410, 14), SetDataTip(STR_NEWS_MESSAGE_TYPES, STR_NULL),
		NWidgetFunction(MakeNewsSettingLines),
		NWidget(NWID_SPACER), SetMinimalSize(0, 6),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL), SetPadding(0, 0, 0, 4),
				NWidget(WWT_DROPDOWN, COLOUR_YELLOW, WIDGET_NEWSOPT_DROP_SUMMARY), SetMinimalSize(95, 12), SetDataTip(0x0, STR_NULL),
				NWidget(WWT_TEXT, COLOUR_YELLOW, WIDGET_NEWSOPT_LABEL_SUMMARY), SetMinimalSize(307, 12), SetDataTip(STR_NEWS_MESSAGES_ALL, STR_NULL), SetPadding(0, 0, 0, 4),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPadding(0, 0, 0, 4),
				NWidget(WWT_TEXTBTN_2, COLOUR_YELLOW, WIDGET_NEWSOPT_SOUNDTICKER), SetMinimalSize(95, 12), SetDataTip(STR_STATION_BUILD_COVERAGE_OFF, STR_NULL),
				NWidget(WWT_TEXT, COLOUR_YELLOW, WIDGET_NEWSOPT_SOUNDTICKER_LABEL), SetMinimalSize(307, 12), SetDataTip(STR_NEWS_MESSAGES_SOUND, STR_NULL), SetPadding(0, 0, 0, 4),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 7),
	EndContainer(),
};

static const WindowDesc _message_options_desc(
	270,  22,  410,  65 + NT_END * NEWS_SETTING_BASELINE_SKIP,
	           410,  65 + NT_END * NEWS_SETTING_BASELINE_SKIP,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_message_options_widgets, _nested_message_options_widgets, lengthof(_nested_message_options_widgets)
);

void ShowMessageOptions()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	new MessageOptionsWindow(&_message_options_desc);
}
