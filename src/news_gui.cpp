/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file news_gui.cpp GUI functions related to news messages. */

#include "stdafx.h"
#include "gui.h"
#include "viewport_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "date_func.h"
#include "vehicle_base.h"
#include "vehicle_func.h"
#include "vehicle_gui.h"
#include "station_base.h"
#include "industry.h"
#include "town.h"
#include "sound_func.h"
#include "string_func.h"
#include "widgets/dropdown_func.h"
#include "statusbar_gui.h"
#include "company_manager_face.h"
#include "company_func.h"
#include "engine_base.h"
#include "engine_gui.h"
#include "core/geometry_func.hpp"
#include "command_func.h"
#include "company_base.h"
#include "settings_internal.h"

#include "widgets/news_widget.h"

#include "table/strings.h"

#include "safeguards.h"

const NewsItem *_statusbar_news_item = NULL;

static uint MIN_NEWS_AMOUNT = 30;           ///< preferred minimum amount of news messages
static uint _total_news = 0;                ///< current number of news items
static NewsItem *_oldest_news = NULL;       ///< head of news items queue
static NewsItem *_latest_news = NULL;       ///< tail of news items queue

/**
 * Forced news item.
 * Users can force an item by accessing the history or "last message".
 * If the message being shown was forced by the user, a pointer is stored
 * in _forced_news. Otherwise, \a _forced_news variable is NULL.
 */
static const NewsItem *_forced_news = NULL;       ///< item the user has asked for

/** Current news item (last item shown regularly). */
static const NewsItem *_current_news = NULL;


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
		case NR_INDUSTRY: return Industry::Get((IndustryID)ref)->location.tile + TileDiffXY(1, 1);
		case NR_TOWN:     return Town::Get((TownID)ref)->xy;
		default:          return INVALID_TILE;
	}
}

/* Normal news items. */
static const NWidgetPart _nested_normal_news_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_WHITE, WID_N_PANEL),
		NWidget(NWID_HORIZONTAL), SetPadding(1, 1, 0, 1),
			NWidget(WWT_CLOSEBOX, COLOUR_WHITE, WID_N_CLOSEBOX), SetPadding(0, 0, 0, 1),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_LABEL, COLOUR_WHITE, WID_N_DATE), SetDataTip(STR_DATE_LONG_SMALL, STR_NULL),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_EMPTY, COLOUR_WHITE, WID_N_MESSAGE), SetMinimalSize(428, 154), SetPadding(0, 5, 1, 5),
	EndContainer(),
};

static WindowDesc _normal_news_desc(
	WDP_MANUAL, NULL, 0, 0,
	WC_NEWS_WINDOW, WC_NONE,
	0,
	_nested_normal_news_widgets, lengthof(_nested_normal_news_widgets)
);

/* New vehicles news items. */
static const NWidgetPart _nested_vehicle_news_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_WHITE, WID_N_PANEL),
		NWidget(NWID_HORIZONTAL), SetPadding(1, 1, 0, 1),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_CLOSEBOX, COLOUR_WHITE, WID_N_CLOSEBOX), SetPadding(0, 0, 0, 1),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
			NWidget(WWT_LABEL, COLOUR_WHITE, WID_N_VEH_TITLE), SetFill(1, 1), SetMinimalSize(419, 55), SetDataTip(STR_EMPTY, STR_NULL),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_WHITE, WID_N_VEH_BKGND), SetPadding(0, 25, 1, 25),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_N_VEH_NAME), SetMinimalSize(369, 33), SetFill(1, 0),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_N_VEH_SPR),  SetMinimalSize(369, 32), SetFill(1, 0),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_N_VEH_INFO), SetMinimalSize(369, 46), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _vehicle_news_desc(
	WDP_MANUAL, NULL, 0, 0,
	WC_NEWS_WINDOW, WC_NONE,
	0,
	_nested_vehicle_news_widgets, lengthof(_nested_vehicle_news_widgets)
);

/* Company news items. */
static const NWidgetPart _nested_company_news_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_WHITE, WID_N_PANEL),
		NWidget(NWID_HORIZONTAL), SetPadding(1, 1, 0, 1),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_CLOSEBOX, COLOUR_WHITE, WID_N_CLOSEBOX), SetPadding(0, 0, 0, 1),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
			NWidget(WWT_LABEL, COLOUR_WHITE, WID_N_TITLE), SetFill(1, 1), SetMinimalSize(410, 20), SetDataTip(STR_EMPTY, STR_NULL),
		EndContainer(),
		NWidget(NWID_HORIZONTAL), SetPadding(0, 1, 1, 1),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_EMPTY, COLOUR_WHITE, WID_N_MGR_FACE), SetMinimalSize(93, 119), SetPadding(2, 6, 2, 1),
				NWidget(WWT_EMPTY, COLOUR_WHITE, WID_N_MGR_NAME), SetMinimalSize(93, 24), SetPadding(0, 0, 0, 1),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
			NWidget(WWT_EMPTY, COLOUR_WHITE, WID_N_COMPANY_MSG), SetFill(1, 1), SetMinimalSize(328, 150),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _company_news_desc(
	WDP_MANUAL, NULL, 0, 0,
	WC_NEWS_WINDOW, WC_NONE,
	0,
	_nested_company_news_widgets, lengthof(_nested_company_news_widgets)
);

/* Thin news items. */
static const NWidgetPart _nested_thin_news_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_WHITE, WID_N_PANEL),
		NWidget(NWID_HORIZONTAL), SetPadding(1, 1, 0, 1),
			NWidget(WWT_CLOSEBOX, COLOUR_WHITE, WID_N_CLOSEBOX), SetPadding(0, 0, 0, 1),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_LABEL, COLOUR_WHITE, WID_N_DATE), SetDataTip(STR_DATE_LONG_SMALL, STR_NULL),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_EMPTY, COLOUR_WHITE, WID_N_MESSAGE), SetMinimalSize(428, 48), SetFill(1, 0), SetPadding(0, 5, 0, 5),
		NWidget(NWID_VIEWPORT, INVALID_COLOUR, WID_N_VIEWPORT), SetMinimalSize(426, 70), SetPadding(1, 2, 2, 2),
	EndContainer(),
};

static WindowDesc _thin_news_desc(
	WDP_MANUAL, NULL, 0, 0,
	WC_NEWS_WINDOW, WC_NONE,
	0,
	_nested_thin_news_widgets, lengthof(_nested_thin_news_widgets)
);

/* Small news items. */
static const NWidgetPart _nested_small_news_widgets[] = {
	/* Caption + close box. The caption is no WWT_CAPTION as the window shall not be moveable and so on. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE, WID_N_CLOSEBOX),
		NWidget(WWT_EMPTY, COLOUR_LIGHT_BLUE, WID_N_CAPTION), SetFill(1, 0),
	EndContainer(),

	/* Main part */
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, WID_N_HEADLINE),
		NWidget(WWT_INSET, COLOUR_LIGHT_BLUE, WID_N_INSET), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, INVALID_COLOUR, WID_N_VIEWPORT), SetPadding(1, 1, 1, 1), SetMinimalSize(274, 47), SetFill(1, 0),
		EndContainer(),
		NWidget(WWT_EMPTY, COLOUR_WHITE, WID_N_MESSAGE), SetMinimalSize(275, 20), SetFill(1, 0), SetPadding(0, 5, 0, 5),
	EndContainer(),
};

static WindowDesc _small_news_desc(
	WDP_MANUAL, NULL, 0, 0,
	WC_NEWS_WINDOW, WC_NONE,
	0,
	_nested_small_news_widgets, lengthof(_nested_small_news_widgets)
);

/**
 * Window layouts for news items.
 */
static WindowDesc* _news_window_layout[] = {
	&_thin_news_desc,    ///< NF_THIN
	&_small_news_desc,   ///< NF_SMALL
	&_normal_news_desc,  ///< NF_NORMAL
	&_vehicle_news_desc, ///< NF_VEHICLE
	&_company_news_desc, ///< NF_COMPANY
};

WindowDesc* GetNewsWindowLayout(NewsFlag flags)
{
	uint layout = GB(flags, NFB_WINDOW_LAYOUT, NFB_WINDOW_LAYOUT_COUNT);
	assert(layout < lengthof(_news_window_layout));
	return _news_window_layout[layout];
}

/**
 * Per-NewsType data
 */
static NewsTypeData _news_type_data[] = {
	/*            name,                           age, sound,          */
	NewsTypeData("news_display.arrival_player",    60, SND_1D_APPLAUSE ),  ///< NT_ARRIVAL_COMPANY
	NewsTypeData("news_display.arrival_other",     60, SND_1D_APPLAUSE ),  ///< NT_ARRIVAL_OTHER
	NewsTypeData("news_display.accident",          90, SND_BEGIN       ),  ///< NT_ACCIDENT
	NewsTypeData("news_display.company_info",      60, SND_BEGIN       ),  ///< NT_COMPANY_INFO
	NewsTypeData("news_display.open",              90, SND_BEGIN       ),  ///< NT_INDUSTRY_OPEN
	NewsTypeData("news_display.close",             90, SND_BEGIN       ),  ///< NT_INDUSTRY_CLOSE
	NewsTypeData("news_display.economy",           30, SND_BEGIN       ),  ///< NT_ECONOMY
	NewsTypeData("news_display.production_player", 30, SND_BEGIN       ),  ///< NT_INDUSTRY_COMPANY
	NewsTypeData("news_display.production_other",  30, SND_BEGIN       ),  ///< NT_INDUSTRY_OTHER
	NewsTypeData("news_display.production_nobody", 30, SND_BEGIN       ),  ///< NT_INDUSTRY_NOBODY
	NewsTypeData("news_display.advice",           150, SND_BEGIN       ),  ///< NT_ADVICE
	NewsTypeData("news_display.new_vehicles",      30, SND_1E_OOOOH    ),  ///< NT_NEW_VEHICLES
	NewsTypeData("news_display.acceptance",        90, SND_BEGIN       ),  ///< NT_ACCEPTANCE
	NewsTypeData("news_display.subsidies",        180, SND_BEGIN       ),  ///< NT_SUBSIDIES
	NewsTypeData("news_display.general",           60, SND_BEGIN       ),  ///< NT_GENERAL
};

assert_compile(lengthof(_news_type_data) == NT_END);

/**
 * Return the news display option.
 * @return display options
 */
NewsDisplay NewsTypeData::GetDisplay() const
{
	uint index;
	const SettingDesc *sd = GetSettingFromName(this->name, &index);
	assert(sd != NULL);
	void *ptr = GetVariableAddress(NULL, &sd->save);
	return (NewsDisplay)ReadValue(ptr, sd->save.conv);
}

/** Window class displaying a news item. */
struct NewsWindow : Window {
	uint16 chat_height;   ///< Height of the chat window.
	uint16 status_height; ///< Height of the status bar window
	const NewsItem *ni;   ///< News item to display.
	static uint duration; ///< Remaining time for showing current news message (may only be accessed while a news item is displayed).

	NewsWindow(WindowDesc *desc, const NewsItem *ni) : Window(desc), ni(ni)
	{
		NewsWindow::duration = 555;
		const Window *w = FindWindowByClass(WC_SEND_NETWORK_MSG);
		this->chat_height = (w != NULL) ? w->height : 0;
		this->status_height = FindWindowById(WC_STATUS_BAR, 0)->height;

		this->flags |= WF_DISABLE_VP_SCROLL;

		this->CreateNestedTree();

		/* For company news with a face we have a separate headline in param[0] */
		if (desc == &_company_news_desc) this->GetWidget<NWidgetCore>(WID_N_TITLE)->widget_data = this->ni->params[0];

		this->FinishInitNested(0);

		/* Initialize viewport if it exists. */
		NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WID_N_VIEWPORT);
		if (nvp != NULL) {
			nvp->InitializeViewport(this, ni->reftype1 == NR_VEHICLE ? 0x80000000 | ni->ref1 : GetReferenceTile(ni->reftype1, ni->ref1), ZOOM_LVL_NEWS);
			if (this->ni->flags & NF_NO_TRANSPARENT) nvp->disp_flags |= ND_NO_TRANSPARENCY;
			if ((this->ni->flags & NF_INCOLOUR) == 0) {
				nvp->disp_flags |= ND_SHADE_GREY;
			} else if (this->ni->flags & NF_SHADE) {
				nvp->disp_flags |= ND_SHADE_DIMMED;
			}
		}

		PositionNewsMessage(this);
	}

	void DrawNewsBorder(const Rect &r) const
	{
		GfxFillRect(r.left,  r.top,    r.right, r.bottom, PC_WHITE);

		GfxFillRect(r.left,  r.top,    r.left,  r.bottom, PC_BLACK);
		GfxFillRect(r.right, r.top,    r.right, r.bottom, PC_BLACK);
		GfxFillRect(r.left,  r.top,    r.right, r.top,    PC_BLACK);
		GfxFillRect(r.left,  r.bottom, r.right, r.bottom, PC_BLACK);
	}

	virtual Point OnInitialPosition(int16 sm_width, int16 sm_height, int window_number)
	{
		Point pt = { 0, _screen.height };
		return pt;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		StringID str = STR_NULL;
		switch (widget) {
			case WID_N_CAPTION: {
				/* Caption is not a real caption (so that the window cannot be moved)
				 * thus it doesn't get the default sizing of a caption. */
				Dimension d2 = GetStringBoundingBox(STR_NEWS_MESSAGE_CAPTION);
				d2.height += WD_CAPTIONTEXT_TOP + WD_CAPTIONTEXT_BOTTOM;
				*size = maxdim(*size, d2);
				return;
			}

			case WID_N_MGR_FACE:
				*size = maxdim(*size, GetSpriteSize(SPR_GRADIENT));
				break;

			case WID_N_MESSAGE:
				CopyInDParam(0, this->ni->params, lengthof(this->ni->params));
				str = this->ni->string_id;
				break;

			case WID_N_COMPANY_MSG:
				str = this->GetCompanyMessageString();
				break;

			case WID_N_VEH_NAME:
			case WID_N_VEH_TITLE:
				str = this->GetNewVehicleMessageString(widget);
				break;

			case WID_N_VEH_INFO: {
				assert(this->ni->reftype1 == NR_ENGINE);
				EngineID engine = this->ni->ref1;
				str = GetEngineInfoString(engine);
				break;
			}
			default:
				return; // Do nothing
		}

		/* Update minimal size with length of the multi-line string. */
		Dimension d = *size;
		d.width = (d.width >= padding.width) ? d.width - padding.width : 0;
		d.height = (d.height >= padding.height) ? d.height - padding.height : 0;
		d = GetStringMultiLineBoundingBox(str, d);
		d.width += padding.width;
		d.height += padding.height;
		*size = maxdim(*size, d);
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == WID_N_DATE) SetDParam(0, this->ni->date);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_N_CAPTION:
				DrawCaption(r, COLOUR_LIGHT_BLUE, this->owner, STR_NEWS_MESSAGE_CAPTION);
				break;

			case WID_N_PANEL:
				this->DrawNewsBorder(r);
				break;

			case WID_N_MESSAGE:
				CopyInDParam(0, this->ni->params, lengthof(this->ni->params));
				DrawStringMultiLine(r.left, r.right, r.top, r.bottom, this->ni->string_id, TC_FROMSTRING, SA_CENTER);
				break;

			case WID_N_MGR_FACE: {
				const CompanyNewsInformation *cni = (const CompanyNewsInformation*)this->ni->free_data;
				DrawCompanyManagerFace(cni->face, cni->colour, r.left, r.top);
				GfxFillRect(r.left, r.top, r.right, r.bottom, PALETTE_NEWSPAPER, FILLRECT_RECOLOUR);
				break;
			}
			case WID_N_MGR_NAME: {
				const CompanyNewsInformation *cni = (const CompanyNewsInformation*)this->ni->free_data;
				SetDParamStr(0, cni->president_name);
				DrawStringMultiLine(r.left, r.right, r.top, r.bottom, STR_JUST_RAW_STRING, TC_FROMSTRING, SA_CENTER);
				break;
			}
			case WID_N_COMPANY_MSG:
				DrawStringMultiLine(r.left, r.right, r.top, r.bottom, this->GetCompanyMessageString(), TC_FROMSTRING, SA_CENTER);
				break;

			case WID_N_VEH_BKGND:
				GfxFillRect(r.left, r.top, r.right, r.bottom, PC_GREY);
				break;

			case WID_N_VEH_NAME:
			case WID_N_VEH_TITLE:
				DrawStringMultiLine(r.left, r.right, r.top, r.bottom, this->GetNewVehicleMessageString(widget), TC_FROMSTRING, SA_CENTER);
				break;

			case WID_N_VEH_SPR: {
				assert(this->ni->reftype1 == NR_ENGINE);
				EngineID engine = this->ni->ref1;
				DrawVehicleEngine(r.left, r.right, (r.left + r.right) / 2, (r.top + r.bottom) / 2, engine, GetEnginePalette(engine, _local_company), EIT_PREVIEW);
				GfxFillRect(r.left, r.top, r.right, r.bottom, PALETTE_NEWSPAPER, FILLRECT_RECOLOUR);
				break;
			}
			case WID_N_VEH_INFO: {
				assert(this->ni->reftype1 == NR_ENGINE);
				EngineID engine = this->ni->ref1;
				DrawStringMultiLine(r.left, r.right, r.top, r.bottom, GetEngineInfoString(engine), TC_FROMSTRING, SA_CENTER);
				break;
			}
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_N_CLOSEBOX:
				NewsWindow::duration = 0;
				delete this;
				_forced_news = NULL;
				break;

			case WID_N_CAPTION:
				if (this->ni->reftype1 == NR_VEHICLE) {
					const Vehicle *v = Vehicle::Get(this->ni->ref1);
					ShowVehicleViewWindow(v);
				}
				break;

			case WID_N_VIEWPORT:
				break; // Ignore clicks

			default:
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
						if ((tile1 == INVALID_TILE || !ScrollMainWindowToTile(tile1)) && tile2 != INVALID_TILE) {
							ScrollMainWindowToTile(tile2);
						}
					}
				}
				break;
		}
	}

	virtual EventState OnKeyPress(WChar key, uint16 keycode)
	{
		if (keycode == WKC_SPACE) {
			/* Don't continue. */
			delete this;
			return ES_HANDLED;
		}
		return ES_NOT_HANDLED;
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		/* The chatbar has notified us that is was either created or closed */
		int newtop = this->top + this->chat_height - data;
		this->chat_height = data;
		this->SetWindowTop(newtop);
	}

	virtual void OnTick()
	{
		/* Scroll up newsmessages from the bottom in steps of 4 pixels */
		int newtop = max(this->top - 4, _screen.height - this->height - this->status_height - this->chat_height);
		this->SetWindowTop(newtop);
	}

private:
	/**
	 * Moves the window so #newtop is new 'top' coordinate. Makes screen dirty where needed.
	 * @param newtop new top coordinate
	 */
	void SetWindowTop(int newtop)
	{
		if (this->top == newtop) return;

		int mintop = min(newtop, this->top);
		int maxtop = max(newtop, this->top);
		if (this->viewport != NULL) this->viewport->top += newtop - this->top;
		this->top = newtop;

		SetDirtyBlocks(this->left, mintop, this->left + this->width, maxtop + this->height);
	}

	StringID GetCompanyMessageString() const
	{
		/* Company news with a face have a separate headline, so the normal message is shifted by two params */
		CopyInDParam(0, this->ni->params + 2, lengthof(this->ni->params) - 2);
		return this->ni->params[1];
	}

	StringID GetNewVehicleMessageString(int widget) const
	{
		assert(this->ni->reftype1 == NR_ENGINE);
		EngineID engine = this->ni->ref1;

		switch (widget) {
			case WID_N_VEH_TITLE:
				SetDParam(0, GetEngineCategoryName(engine));
				return STR_NEWS_NEW_VEHICLE_NOW_AVAILABLE;

			case WID_N_VEH_NAME:
				SetDParam(0, engine);
				return STR_NEWS_NEW_VEHICLE_TYPE;

			default:
				NOT_REACHED();
		}
	}
};

/* static */ uint NewsWindow::duration = 0; // Instance creation.


/** Open up an own newspaper window for the news item */
static void ShowNewspaper(const NewsItem *ni)
{
	SoundFx sound = _news_type_data[ni->type].sound;
	if (sound != 0 && _settings_client.sound.news_full) SndPlayFx(sound);

	new NewsWindow(GetNewsWindowLayout(ni->flags), ni);
}

/** Show news item in the ticker */
static void ShowTicker(const NewsItem *ni)
{
	if (_settings_client.sound.news_ticker) SndPlayFx(SND_16_MORSE);

	_statusbar_news_item = ni;
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
	_statusbar_news_item = NULL;
	NewsWindow::duration = 0;
}

/**
 * Are we ready to show another news item?
 * Only if nothing is in the newsticker and no newspaper is displayed
 */
static bool ReadyForNextItem()
{
	const NewsItem *ni = _forced_news == NULL ? _current_news : _forced_news;
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
	_statusbar_news_item = NULL;

	/* if we're not at the last item, then move on */
	if (_current_news != _latest_news) {
		_current_news = (_current_news == NULL) ? _oldest_news : _current_news->next;
		const NewsItem *ni = _current_news;
		const NewsType type = ni->type;

		/* check the date, don't show too old items */
		if (_date - _news_type_data[type].age > ni->date) return;

		switch (_news_type_data[type].GetDisplay()) {
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
 * @param type news category
 * @param flags display flags for the news
 * @param reftype1 Type of ref1
 * @param ref1     Reference 1 to some object: Used for a possible viewport, scrolling after clicking on the news, and for deleteing the news when the object is deleted.
 * @param reftype2 Type of ref2
 * @param ref2     Reference 2 to some object: Used for scrolling after clicking on the news, and for deleteing the news when the object is deleted.
 * @param free_data Pointer to data that must be freed once the news message is cleared
 *
 * @see NewsSubtype
 */
void AddNewsItem(StringID string, NewsType type, NewsFlag flags, NewsReferenceType reftype1, uint32 ref1, NewsReferenceType reftype2, uint32 ref2, void *free_data)
{
	if (_game_mode == GM_MENU) return;

	/* Create new news item node */
	NewsItem *ni = new NewsItem;

	ni->string_id = string;
	ni->type = type;
	ni->flags = flags;

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

	SetWindowDirty(WC_MESSAGE_HISTORY, 0);
}

/**
 * Create a new custom news item.
 * @param tile unused
 * @param flags type of operation
 * @param p1 various bitstuffed elements
 * - p1 = (bit  0 -  7) - NewsType of the message.
 * - p1 = (bit  8 - 15) - NewsReferenceType of first reference.
 * - p1 = (bit 16 - 23) - Company this news message is for.
 * @param p2 First reference of the news message.
 * @param text The text of the news message.
 * @return the cost of this operation or an error
 */
CommandCost CmdCustomNewsItem(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;

	NewsType type = (NewsType)GB(p1, 0, 8);
	NewsReferenceType reftype1 = (NewsReferenceType)GB(p1, 8, 8);
	CompanyID company = (CompanyID)GB(p1, 16, 8);

	if (company != INVALID_OWNER && !Company::IsValidID(company)) return CMD_ERROR;
	if (type >= NT_END) return CMD_ERROR;
	if (StrEmpty(text)) return CMD_ERROR;

	switch (reftype1) {
		case NR_NONE: break;
		case NR_TILE:
			if (!IsValidTile(p2)) return CMD_ERROR;
			break;

		case NR_VEHICLE:
			if (!Vehicle::IsValidID(p2)) return CMD_ERROR;
			break;

		case NR_STATION:
			if (!Station::IsValidID(p2)) return CMD_ERROR;
			break;

		case NR_INDUSTRY:
			if (!Industry::IsValidID(p2)) return CMD_ERROR;
			break;

		case NR_TOWN:
			if (!Town::IsValidID(p2)) return CMD_ERROR;
			break;

		case NR_ENGINE:
			if (!Engine::IsValidID(p2)) return CMD_ERROR;
			break;

		default: return CMD_ERROR;
	}

	if (company != INVALID_OWNER && company != _local_company) return CommandCost();

	if (flags & DC_EXEC) {
		char *news = stredup(text);
		SetDParamStr(0, news);
		AddNewsItem(STR_NEWS_CUSTOM_ITEM, type, NF_NORMAL, reftype1, p2, NR_NONE, UINT32_MAX, news);
	}

	return CommandCost();
}

/** Delete a news item from the queue */
static void DeleteNewsItem(NewsItem *ni)
{
	/* Delete the news from the news queue. */
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

	_total_news--;

	if (_forced_news == ni || _current_news == ni || _statusbar_news_item == ni) {
		/* When we're the current news, go to the previous item first;
		 * we just possibly made that the last news item. */
		if (_current_news == ni) _current_news = ni->prev;

		/* About to remove the currently forced item (shown as newspapers) ||
		 * about to remove the currently displayed item (newspapers, ticker, or just a reminder) */
		MoveToNextItem();
	}

	delete ni;

	SetWindowDirty(WC_MESSAGE_HISTORY, 0);
}

/**
 * Delete a news item type about a vehicle.
 * When the news item type is INVALID_STRING_ID all news about the vehicle gets deleted.
 * @param vid  The vehicle to remove the news for.
 * @param news The news type to remove.
 */
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

/**
 * Remove news regarding given station so there are no 'unknown station now accepts Mail'
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

/**
 * Remove news regarding given industry
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

/**
 * Remove engine announcements for invalid engines.
 */
void DeleteInvalidEngineNews()
{
	NewsItem *ni = _oldest_news;

	while (ni != NULL) {
		NewsItem *next = ni->next;
		if ((ni->reftype1 == NR_ENGINE && (!Engine::IsValidID(ni->ref1) || !Engine::Get(ni->ref1)->IsEnabled())) ||
				(ni->reftype2 == NR_ENGINE && (!Engine::IsValidID(ni->ref2) || !Engine::Get(ni->ref2)->IsEnabled()))) {
			DeleteNewsItem(ni);
		}
		ni = next;
	}
}

static void RemoveOldNewsItems()
{
	NewsItem *next;
	for (NewsItem *cur = _oldest_news; _total_news > MIN_NEWS_AMOUNT && cur != NULL; cur = next) {
		next = cur->next;
		if (_date - _news_type_data[cur->type].age * _settings_client.gui.news_message_timeout > cur->date) DeleteNewsItem(cur);
	}
}

/**
 * Report a change in vehicle IDs (due to autoreplace) to affected vehicle news.
 * @note Viewports of currently displayed news is changed via #ChangeVehicleViewports
 * @param from_index the old vehicle ID
 * @param to_index the new vehicle ID
 */
void ChangeVehicleNews(VehicleID from_index, VehicleID to_index)
{
	for (NewsItem *ni = _oldest_news; ni != NULL; ni = ni->next) {
		if (ni->reftype1 == NR_VEHICLE && ni->ref1 == from_index) ni->ref1 = to_index;
		if (ni->reftype2 == NR_VEHICLE && ni->ref2 == from_index) ni->ref2 = to_index;
		if (ni->flags & NF_VEHICLE_PARAM0 && ni->params[0] == from_index) ni->params[0] = to_index;
	}
}

void NewsLoop()
{
	/* no news item yet */
	if (_total_news == 0) return;

	/* There is no status bar, so no reason to show news;
	 * especially important with the end game screen when
	 * there is no status bar but possible news. */
	if (FindWindowById(WC_STATUS_BAR, 0) == NULL) return;

	static byte _last_clean_month = 0;

	if (_last_clean_month != _cur_month) {
		RemoveOldNewsItems();
		_last_clean_month = _cur_month;
	}

	if (ReadyForNextItem()) MoveToNextItem();
}

/** Do a forced show of a specific message */
static void ShowNewsMessage(const NewsItem *ni)
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
	const NewsItem *ni = NULL;
	if (_total_news == 0) {
		return;
	} else if (_forced_news == NULL) {
		/* Not forced any news yet, show the current one, unless a news window is
		 * open (which can only be the current one), then show the previous item */
		if (_current_news == NULL) {
			/* No news were shown yet resp. the last shown one was already deleted.
			 * Threat this as if _forced_news reached _oldest_news; so, wrap around and start anew with the latest. */
			ni = _latest_news;
		} else {
			const Window *w = FindWindowById(WC_NEWS_WINDOW, 0);
			ni = (w == NULL || (_current_news == _oldest_news)) ? _current_news : _current_news->prev;
		}
	} else if (_forced_news == _oldest_news) {
		/* We have reached the oldest news, start anew with the latest */
		ni = _latest_news;
	} else {
		/* 'Scrolling' through news history show each one in turn */
		ni = _forced_news->prev;
	}
	bool wrap = false;
	for (;;) {
		if (_news_type_data[ni->type].GetDisplay() != ND_OFF) {
			ShowNewsMessage(ni);
			break;
		}

		ni = ni->prev;
		if (ni == NULL) {
			if (wrap) break;
			/* We have reached the oldest news, start anew with the latest */
			ni = _latest_news;
			wrap = true;
		}
	}
}


/**
 * Draw an unformatted news message truncated to a maximum length. If
 * length exceeds maximum length it will be postfixed by '...'
 * @param left  the left most location for the string
 * @param right the right most location for the string
 * @param y position of the string
 * @param colour the colour the string will be shown in
 * @param *ni NewsItem being printed
 * @param maxw maximum width of string in pixels
 */
static void DrawNewsString(uint left, uint right, int y, TextColour colour, const NewsItem *ni)
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
	/* Truncate and show string; postfixed by '...' if necessary */
	DrawString(left, right, y, buffer2, colour);
}

struct MessageHistoryWindow : Window {
	static const int top_spacing;    ///< Additional spacing at the top of the #WID_MH_BACKGROUND widget.
	static const int bottom_spacing; ///< Additional spacing at the bottom of the #WID_MH_BACKGROUND widget.

	int line_height; /// < Height of a single line in the news history window including spacing.
	int date_width;  /// < Width needed for the date part.

	Scrollbar *vscroll;

	MessageHistoryWindow(WindowDesc *desc) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_MH_SCROLLBAR);
		this->FinishInitNested(); // Initializes 'this->line_height' and 'this->date_width'.
		this->OnInvalidateData(0);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget == WID_MH_BACKGROUND) {
			this->line_height = FONT_HEIGHT_NORMAL + 2;
			resize->height = this->line_height;

			/* Months are off-by-one, so it's actually 8. Not using
			 * month 12 because the 1 is usually less wide. */
			SetDParam(0, ConvertYMDToDate(ORIGINAL_MAX_YEAR, 7, 30));
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
		if (widget != WID_MH_BACKGROUND || _total_news == 0) return;

		/* Find the first news item to display. */
		NewsItem *ni = _latest_news;
		for (int n = this->vscroll->GetPosition(); n > 0; n--) {
			ni = ni->prev;
			if (ni == NULL) return;
		}

		/* Fill the widget with news items. */
		int y = r.top + this->top_spacing;
		bool rtl = _current_text_dir == TD_RTL;
		uint date_left  = rtl ? r.right - WD_FRAMERECT_RIGHT - this->date_width : r.left + WD_FRAMERECT_LEFT;
		uint date_right = rtl ? r.right - WD_FRAMERECT_RIGHT : r.left + WD_FRAMERECT_LEFT + this->date_width;
		uint news_left  = rtl ? r.left + WD_FRAMERECT_LEFT : r.left + WD_FRAMERECT_LEFT + this->date_width + WD_FRAMERECT_RIGHT;
		uint news_right = rtl ? r.right - WD_FRAMERECT_RIGHT - this->date_width - WD_FRAMERECT_RIGHT : r.right - WD_FRAMERECT_RIGHT;
		for (int n = this->vscroll->GetCapacity(); n > 0; n--) {
			SetDParam(0, ni->date);
			DrawString(date_left, date_right, y, STR_SHORT_DATE);

			DrawNewsString(news_left, news_right, y, TC_WHITE, ni);
			y += this->line_height;

			ni = ni->prev;
			if (ni == NULL) return;
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		this->vscroll->SetCount(_total_news);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		if (widget == WID_MH_BACKGROUND) {
			NewsItem *ni = _latest_news;
			if (ni == NULL) return;

			for (int n = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_MH_BACKGROUND, WD_FRAMERECT_TOP, this->line_height); n > 0; n--) {
				ni = ni->prev;
				if (ni == NULL) return;
			}

			ShowNewsMessage(ni);
		}
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, WID_MH_BACKGROUND);
	}
};

const int MessageHistoryWindow::top_spacing = WD_FRAMERECT_TOP + 4;
const int MessageHistoryWindow::bottom_spacing = WD_FRAMERECT_BOTTOM;

static const NWidgetPart _nested_message_history[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_MESSAGE_HISTORY, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),

	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN, WID_MH_BACKGROUND), SetMinimalSize(200, 125), SetDataTip(0x0, STR_MESSAGE_HISTORY_TOOLTIP), SetResize(1, 12), SetScrollbar(WID_MH_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_MH_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _message_history_desc(
	WDP_AUTO, "list_news", 400, 140,
	WC_MESSAGE_HISTORY, WC_NONE,
	0,
	_nested_message_history, lengthof(_nested_message_history)
);

/** Display window with news messages history */
void ShowMessageHistory()
{
	DeleteWindowById(WC_MESSAGE_HISTORY, 0);
	new MessageHistoryWindow(&_message_history_desc);
}
