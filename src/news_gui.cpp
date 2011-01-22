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
#include "news_type.h"
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
#include "engine_gui.h"
#include "core/geometry_func.hpp"

#include "table/strings.h"

const NewsItem *_statusbar_news_item = NULL;
bool _news_ticker_sound; ///< Make a ticker sound when a news item is published.

static uint MIN_NEWS_AMOUNT = 30;           ///< prefered minimum amount of news messages
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

/** Widget numbers of the news display windows. */
enum NewsTypeWidgets {
	NTW_PANEL,       ///< The news item background panel.
	NTW_TITLE,       ///< Title of the company news.
	NTW_HEADLINE,    ///< The news headline.
	NTW_CLOSEBOX,    ///< Close the window.
	NTW_DATE,        ///< Date of the news item.
	NTW_CAPTION,     ///< Title bar of the window. Only used in small news items.
	NTW_INSET,       ///< Inset around the viewport in the window. Only used in small news items.
	NTW_VIEWPORT,    ///< Viewport in the window.
	NTW_COMPANY_MSG, ///< Message in company news items.
	NTW_MESSAGE,     ///< Space for displaying the message. Only used in small news items.
	NTW_MGR_FACE,    ///< Face of the manager.
	NTW_MGR_NAME,    ///< Name of the manager.
	NTW_VEH_TITLE,   ///< Vehicle new title.
	NTW_VEH_BKGND,   ///< Dark background of new vehicle news.
	NTW_VEH_NAME,    ///< Name of the new vehicle.
	NTW_VEH_SPR,     ///< Graphical display of the new vehicle.
	NTW_VEH_INFO,    ///< Some technical data of the new vehicle.
};

/* Normal news items. */
static const NWidgetPart _nested_normal_news_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_WHITE, NTW_PANEL),
		NWidget(NWID_HORIZONTAL), SetPadding(1, 1, 0, 1),
			NWidget(WWT_TEXT, COLOUR_WHITE, NTW_CLOSEBOX), SetDataTip(STR_SILVER_CROSS, STR_NULL), SetPadding(0, 0, 0, 1),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_LABEL, COLOUR_WHITE, NTW_DATE), SetDataTip(STR_DATE_LONG_SMALL, STR_NULL),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_EMPTY, COLOUR_WHITE, NTW_MESSAGE), SetMinimalSize(428, 154), SetPadding(0, 1, 1, 1),
	EndContainer(),
};

static const WindowDesc _normal_news_desc(
	WDP_MANUAL, 0, 0,
	WC_NEWS_WINDOW, WC_NONE,
	0,
	_nested_normal_news_widgets, lengthof(_nested_normal_news_widgets)
);

/* New vehicles news items. */
static const NWidgetPart _nested_vehicle_news_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_WHITE, NTW_PANEL),
		NWidget(NWID_HORIZONTAL), SetPadding(1, 1, 0, 1),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_TEXT, COLOUR_WHITE, NTW_CLOSEBOX), SetDataTip(STR_SILVER_CROSS, STR_NULL), SetPadding(0, 0, 0, 1),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
			NWidget(WWT_LABEL, COLOUR_WHITE, NTW_VEH_TITLE), SetFill(1, 1), SetMinimalSize(419, 55), SetDataTip(STR_EMPTY, STR_NULL),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_WHITE, NTW_VEH_BKGND), SetPadding(0, 25, 1, 25),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_EMPTY, INVALID_COLOUR, NTW_VEH_NAME), SetMinimalSize(369, 33), SetFill(1, 0),
				NWidget(WWT_EMPTY, INVALID_COLOUR, NTW_VEH_SPR),  SetMinimalSize(369, 32), SetFill(1, 0),
				NWidget(WWT_EMPTY, INVALID_COLOUR, NTW_VEH_INFO), SetMinimalSize(369, 46), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _vehicle_news_desc(
	WDP_MANUAL, 0, 0,
	WC_NEWS_WINDOW, WC_NONE,
	0,
	_nested_vehicle_news_widgets, lengthof(_nested_vehicle_news_widgets)
);

/* Company news items. */
static const NWidgetPart _nested_company_news_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_WHITE, NTW_PANEL),
		NWidget(NWID_HORIZONTAL), SetPadding(1, 1, 0, 1),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_TEXT, COLOUR_WHITE, NTW_CLOSEBOX), SetDataTip(STR_SILVER_CROSS, STR_NULL), SetPadding(0, 0, 0, 1),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
			NWidget(WWT_LABEL, COLOUR_WHITE, NTW_TITLE), SetFill(1, 1), SetMinimalSize(410, 20), SetDataTip(STR_EMPTY, STR_NULL),
		EndContainer(),
		NWidget(NWID_HORIZONTAL), SetPadding(0, 1, 1, 1),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_EMPTY, COLOUR_WHITE, NTW_MGR_FACE), SetMinimalSize(93, 119), SetPadding(2, 6, 2, 1),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_EMPTY, COLOUR_WHITE, NTW_MGR_NAME), SetMinimalSize(93, 24), SetPadding(0, 0, 0, 1),
					NWidget(NWID_SPACER), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
			NWidget(WWT_EMPTY, COLOUR_WHITE, NTW_COMPANY_MSG), SetFill(1, 1), SetMinimalSize(328, 150),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _company_news_desc(
	WDP_MANUAL, 0, 0,
	WC_NEWS_WINDOW, WC_NONE,
	0,
	_nested_company_news_widgets, lengthof(_nested_company_news_widgets)
);

/* Thin news items. */
static const NWidgetPart _nested_thin_news_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_WHITE, NTW_PANEL),
		NWidget(NWID_HORIZONTAL), SetPadding(1, 1, 0, 1),
			NWidget(WWT_TEXT, COLOUR_WHITE, NTW_CLOSEBOX), SetDataTip(STR_SILVER_CROSS, STR_NULL), SetPadding(0, 0, 0, 1),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_LABEL, COLOUR_WHITE, NTW_DATE), SetDataTip(STR_DATE_LONG_SMALL, STR_NULL),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_EMPTY, COLOUR_WHITE, NTW_MESSAGE), SetMinimalSize(428, 48), SetFill(1, 0), SetPadding(0, 1, 0, 1),
		NWidget(NWID_VIEWPORT, INVALID_COLOUR, NTW_VIEWPORT), SetMinimalSize(426, 70), SetPadding(1, 2, 2, 2),
	EndContainer(),
};

static const WindowDesc _thin_news_desc(
	WDP_MANUAL, 0, 0,
	WC_NEWS_WINDOW, WC_NONE,
	0,
	_nested_thin_news_widgets, lengthof(_nested_thin_news_widgets)
);

/* Small news items. */
static const NWidgetPart _nested_small_news_widgets[] = {
	/* Caption + close box. The caption is no WWT_CAPTION as the window shall not be moveable and so on. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE, NTW_CLOSEBOX),
		NWidget(WWT_EMPTY, COLOUR_LIGHT_BLUE, NTW_CAPTION), SetFill(1, 0),
	EndContainer(),

	/* Main part */
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, NTW_HEADLINE),
		NWidget(WWT_INSET, COLOUR_LIGHT_BLUE, NTW_INSET), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, INVALID_COLOUR, NTW_VIEWPORT), SetPadding(1, 1, 1, 1), SetMinimalSize(274, 47), SetFill(1, 0),
		EndContainer(),
		NWidget(WWT_EMPTY, COLOUR_WHITE, NTW_MESSAGE), SetMinimalSize(275, 20), SetFill(1, 0),
	EndContainer(),
};

static const WindowDesc _small_news_desc(
	WDP_MANUAL, 0, 0,
	WC_NEWS_WINDOW, WC_NONE,
	0,
	_nested_small_news_widgets, lengthof(_nested_small_news_widgets)
);

/**
 * Data common to all news items of a given subtype (structure)
 */
struct NewsSubtypeData {
	NewsType type;          ///< News category @see NewsType
	NewsFlag flags;         ///< Initial NewsFlags bits @see NewsFlag
	const WindowDesc *desc; ///< Window description for displaying this news.
};

/**
 * Data common to all news items of a given subtype (actual data)
 */
static const NewsSubtypeData _news_subtype_data[] = {
	/* type,               display_mode, flags,                         window description,            callback */
	{ NT_ARRIVAL_COMPANY,  (NF_NO_TRANSPARENT | NF_SHADE), &_thin_news_desc    }, ///< NS_ARRIVAL_COMPANY
	{ NT_ARRIVAL_OTHER,    (NF_NO_TRANSPARENT | NF_SHADE), &_thin_news_desc    }, ///< NS_ARRIVAL_OTHER
	{ NT_ACCIDENT,         (NF_NO_TRANSPARENT | NF_SHADE), &_thin_news_desc    }, ///< NS_ACCIDENT
	{ NT_COMPANY_INFO,     NF_NONE,                        &_company_news_desc }, ///< NS_COMPANY_TROUBLE
	{ NT_COMPANY_INFO,     NF_NONE,                        &_company_news_desc }, ///< NS_COMPANY_MERGER
	{ NT_COMPANY_INFO,     NF_NONE,                        &_company_news_desc }, ///< NS_COMPANY_BANKRUPT
	{ NT_COMPANY_INFO,     NF_NONE,                        &_company_news_desc }, ///< NS_COMPANY_NEW
	{ NT_INDUSTRY_OPEN,    (NF_NO_TRANSPARENT | NF_SHADE), &_thin_news_desc    }, ///< NS_INDUSTRY_OPEN
	{ NT_INDUSTRY_CLOSE,   (NF_NO_TRANSPARENT | NF_SHADE), &_thin_news_desc    }, ///< NS_INDUSTRY_CLOSE
	{ NT_ECONOMY,          NF_NONE,                        &_normal_news_desc  }, ///< NS_ECONOMY
	{ NT_INDUSTRY_COMPANY, (NF_NO_TRANSPARENT | NF_SHADE), &_thin_news_desc    }, ///< NS_INDUSTRY_COMPANY
	{ NT_INDUSTRY_OTHER,   (NF_NO_TRANSPARENT | NF_SHADE), &_thin_news_desc    }, ///< NS_INDUSTRY_OTHER
	{ NT_INDUSTRY_NOBODY,  (NF_NO_TRANSPARENT | NF_SHADE), &_thin_news_desc    }, ///< NS_INDUSTRY_NOBODY
	{ NT_ADVICE,           NF_INCOLOUR,                    &_small_news_desc   }, ///< NS_ADVICE
	{ NT_NEW_VEHICLES,     NF_NONE,                        &_vehicle_news_desc }, ///< NS_NEW_VEHICLES
	{ NT_ACCEPTANCE,       NF_INCOLOUR,                    &_small_news_desc   }, ///< NS_ACCEPTANCE
	{ NT_SUBSIDIES,        NF_NONE,                        &_normal_news_desc  }, ///< NS_SUBSIDIES
	{ NT_GENERAL,          NF_NONE,                        &_normal_news_desc  }, ///< NS_GENERAL
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

/** Window class displaying a news item. */
struct NewsWindow : Window {
	uint16 chat_height;   ///< Height of the chat window.
	uint16 status_height; ///< Height of the status bar window
	const NewsItem *ni;   ///< News item to display.
	static uint duration; ///< Remaining time for showing current news message (may only be accessed while a news item is displayed).

	NewsWindow(const WindowDesc *desc, const NewsItem *ni) : Window(), ni(ni)
	{
		NewsWindow::duration = 555;
		const Window *w = FindWindowByClass(WC_SEND_NETWORK_MSG);
		this->chat_height = (w != NULL) ? w->height : 0;
		this->status_height = FindWindowById(WC_STATUS_BAR, 0)->height;

		this->flags4 |= WF_DISABLE_VP_SCROLL;

		this->CreateNestedTree(desc);
		switch (this->ni->subtype) {
			case NS_COMPANY_TROUBLE:
				this->GetWidget<NWidgetCore>(NTW_TITLE)->widget_data = STR_NEWS_COMPANY_IN_TROUBLE_TITLE;
				break;

			case NS_COMPANY_MERGER:
				this->GetWidget<NWidgetCore>(NTW_TITLE)->widget_data = STR_NEWS_COMPANY_MERGER_TITLE;
				break;

			case NS_COMPANY_BANKRUPT:
				this->GetWidget<NWidgetCore>(NTW_TITLE)->widget_data = STR_NEWS_COMPANY_BANKRUPT_TITLE;
				break;

			case NS_COMPANY_NEW:
				this->GetWidget<NWidgetCore>(NTW_TITLE)->widget_data = STR_NEWS_COMPANY_LAUNCH_TITLE;
				break;

			default:
				break;
		}
		this->FinishInitNested(desc, 0);

		/* Initialize viewport if it exists. */
		NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(NTW_VIEWPORT);
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
		GfxFillRect(r.left,  r.top,    r.right, r.bottom, 0xF);

		GfxFillRect(r.left,  r.top,    r.left,  r.bottom, 0xD7);
		GfxFillRect(r.right, r.top,    r.right, r.bottom, 0xD7);
		GfxFillRect(r.left,  r.top,    r.right, r.top,    0xD7);
		GfxFillRect(r.left,  r.bottom, r.right, r.bottom, 0xD7);
	}

	virtual Point OnInitialPosition(const WindowDesc *desc, int16 sm_width, int16 sm_height, int window_number)
	{
		Point pt = { 0, _screen.height };
		return pt;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		StringID str = STR_NULL;
		switch (widget) {
			case NTW_MESSAGE:
				CopyInDParam(0, this->ni->params, lengthof(this->ni->params));
				str = this->ni->string_id;
				break;

			case NTW_COMPANY_MSG:
				str = this->GetCompanyMessageString();
				break;

			case NTW_VEH_NAME:
			case NTW_VEH_TITLE:
				str = this->GetNewVehicleMessageString(widget);
				break;

			case NTW_VEH_INFO: {
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
		if (widget == NTW_DATE) SetDParam(0, this->ni->date);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case NTW_CAPTION:
				DrawCaption(r, COLOUR_LIGHT_BLUE, this->owner, STR_NEWS_MESSAGE_CAPTION);
				break;

			case NTW_PANEL:
				this->DrawNewsBorder(r);
				break;

			case NTW_MESSAGE:
				CopyInDParam(0, this->ni->params, lengthof(this->ni->params));
				DrawStringMultiLine(r.left + 2, r.right - 2, r.top, r.bottom, this->ni->string_id, TC_FROMSTRING, SA_CENTER);
				break;

			case NTW_MGR_FACE: {
				const CompanyNewsInformation *cni = (const CompanyNewsInformation*)this->ni->free_data;
				DrawCompanyManagerFace(cni->face, cni->colour, r.left, r.top);
				GfxFillRect(r.left + 1, r.top, r.left + 1 + 91, r.top + 118, PALETTE_TO_STRUCT_GREY, FILLRECT_RECOLOUR);
				break;
			}
			case NTW_MGR_NAME: {
				const CompanyNewsInformation *cni = (const CompanyNewsInformation*)this->ni->free_data;
				SetDParamStr(0, cni->president_name);
				DrawStringMultiLine(r.left, r.right, r.top, r.bottom, STR_JUST_RAW_STRING, TC_FROMSTRING, SA_CENTER);
				break;
			}
			case NTW_COMPANY_MSG:
				DrawStringMultiLine(r.left, r.right, r.top, r.bottom, this->GetCompanyMessageString(), TC_FROMSTRING, SA_CENTER);
				break;

			case NTW_VEH_BKGND:
				GfxFillRect(r.left, r.top, r.right, r.bottom, 10);
				break;

			case NTW_VEH_NAME:
			case NTW_VEH_TITLE:
				DrawStringMultiLine(r.left, r.right, r.top, r.bottom, this->GetNewVehicleMessageString(widget), TC_FROMSTRING, SA_CENTER);
				break;

			case NTW_VEH_SPR: {
				assert(this->ni->reftype1 == NR_ENGINE);
				EngineID engine = this->ni->ref1;
				DrawVehicleEngine(r.left, r.right, (r.left + r.right) / 2, (r.top + r.bottom) / 2, engine, GetEnginePalette(engine, _local_company));
				GfxFillRect(r.left, r.top, r.right, r.bottom, PALETTE_TO_STRUCT_GREY, FILLRECT_RECOLOUR);
				break;
			}
			case NTW_VEH_INFO: {
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
			case NTW_CLOSEBOX:
				NewsWindow::duration = 0;
				delete this;
				_forced_news = NULL;
				break;

			case NTW_CAPTION:
				if (this->ni->reftype1 == NR_VEHICLE) {
					const Vehicle *v = Vehicle::Get(this->ni->ref1);
					ShowVehicleViewWindow(v);
				}
				break;

			case NTW_VIEWPORT:
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
		switch (this->ni->subtype) {
			case NS_COMPANY_TROUBLE:
				SetDParam(0, this->ni->params[2]);
				return STR_NEWS_COMPANY_IN_TROUBLE_DESCRIPTION;

			case NS_COMPANY_MERGER:
				SetDParam(0, this->ni->params[2]);
				SetDParam(1, this->ni->params[3]);
				SetDParam(2, this->ni->params[4]);
				return this->ni->params[4] == 0 ? STR_NEWS_MERGER_TAKEOVER_TITLE : STR_NEWS_COMPANY_MERGER_DESCRIPTION;

			case NS_COMPANY_BANKRUPT:
				SetDParam(0, this->ni->params[2]);
				return STR_NEWS_COMPANY_BANKRUPT_DESCRIPTION;

			case NS_COMPANY_NEW:
				SetDParam(0, this->ni->params[2]);
				SetDParam(1, this->ni->params[3]);
				return STR_NEWS_COMPANY_LAUNCH_DESCRIPTION;

			default:
				NOT_REACHED();
		}
	}

	StringID GetNewVehicleMessageString(int widget) const
	{
		assert(this->ni->reftype1 == NR_ENGINE);
		EngineID engine = this->ni->ref1;

		switch (widget) {
			case NTW_VEH_TITLE:
				SetDParam(0, GetEngineCategoryName(engine));
				return STR_NEWS_NEW_VEHICLE_NOW_AVAILABLE;

			case NTW_VEH_NAME:
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
	SoundFx sound = _news_type_data[_news_subtype_data[ni->subtype].type].sound;
	if (sound != 0) SndPlayFx(sound);

	new NewsWindow(_news_subtype_data[ni->subtype].desc, ni);
}

/** Show news item in the ticker */
static void ShowTicker(const NewsItem *ni)
{
	if (_news_ticker_sound) SndPlayFx(SND_16_MORSE);

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
 * @param free_data Pointer to data that must be freed once the news message is cleared
 *
 * @see NewsSubtype
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

	SetWindowDirty(WC_MESSAGE_HISTORY, 0);
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

static void RemoveOldNewsItems()
{
	NewsItem *next;
	for (NewsItem *cur = _oldest_news; _total_news > MIN_NEWS_AMOUNT && cur != NULL; cur = next) {
		next = cur->next;
		if (_date - _news_type_data[_news_subtype_data[cur->subtype].type].age * _settings_client.gui.news_message_timeout > cur->date) DeleteNewsItem(cur);
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

		/* Oh noes :(
		 * Autoreplace is breaking the whole news-reference concept here, as we want to keep the news,
		 * but do not know which DParams to change.
		 *
		 * Currently only NS_ADVICE news have vehicle IDs in their DParams.
		 * And all NS_ADVICE news have the ID in param 0.
		 */
		if (ni->subtype == NS_ADVICE && ni->params[0] == from_index) ni->params[0] = to_index;
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
	/* Truncate and show string; postfixed by '...' if neccessary */
	DrawString(left, right, y, buffer2, colour);
}

/** Widget numbers of the message history window. */
enum MessageHistoryWidgets {
	MHW_STICKYBOX,
	MHW_BACKGROUND,
	MHW_SCROLLBAR,
};

struct MessageHistoryWindow : Window {
	static const int top_spacing;    ///< Additional spacing at the top of the #MHW_BACKGROUND widget.
	static const int bottom_spacing; ///< Additional spacing at the bottom of the #MHW_BACKGROUND widget.

	int line_height; /// < Height of a single line in the news histoy window including spacing.
	int date_width;  /// < Width needed for the date part.

	Scrollbar *vscroll;

	MessageHistoryWindow(const WindowDesc *desc) : Window()
	{
		this->CreateNestedTree(desc);
		this->vscroll = this->GetScrollbar(MHW_SCROLLBAR);
		this->FinishInitNested(desc); // Initializes 'this->line_height' and 'this->date_width'.
		this->OnInvalidateData(0);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget == MHW_BACKGROUND) {
			this->line_height = FONT_HEIGHT_NORMAL + 2;
			resize->height = this->line_height;

			SetDParam(0, ConvertYMDToDate(ORIGINAL_MAX_YEAR, 12, 30));
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

	virtual void OnInvalidateData(int data)
	{
		this->vscroll->SetCount(_total_news);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		if (widget == MHW_BACKGROUND) {
			NewsItem *ni = _latest_news;
			if (ni == NULL) return;

			for (int n = this->vscroll->GetScrolledRowFromWidget(pt.y, this, MHW_BACKGROUND, WD_FRAMERECT_TOP, this->line_height); n > 0; n--) {
				ni = ni->prev;
				if (ni == NULL) return;
			}

			ShowNewsMessage(ni);
		}
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacity(this->GetWidget<NWidgetBase>(MHW_BACKGROUND)->current_y / this->line_height);
	}
};

const int MessageHistoryWindow::top_spacing = WD_FRAMERECT_TOP + 4;
const int MessageHistoryWindow::bottom_spacing = WD_FRAMERECT_BOTTOM;

static const NWidgetPart _nested_message_history[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_MESSAGE_HISTORY, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),

	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN, MHW_BACKGROUND), SetMinimalSize(200, 125), SetDataTip(0x0, STR_MESSAGE_HISTORY_TOOLTIP), SetResize(1, 12), SetScrollbar(MHW_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, MHW_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _message_history_desc(
	WDP_AUTO, 400, 140,
	WC_MESSAGE_HISTORY, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_message_history, lengthof(_nested_message_history)
);

/** Display window with news messages history */
void ShowMessageHistory()
{
	DeleteWindowById(WC_MESSAGE_HISTORY, 0);
	new MessageHistoryWindow(&_message_history_desc);
}

/** Constants in the message options window. */
enum MessageOptionsSpace {
	MOS_WIDG_PER_SETTING      = 4,  ///< Number of widgets needed for each news category, starting at widget #WIDGET_NEWSOPT_START_OPTION.

	MOS_LEFT_EDGE             = 6,  ///< Number of pixels between left edge of the window and the options buttons column.
	MOS_COLUMN_SPACING        = 4,  ///< Number of pixels between the buttons and the description columns.
	MOS_RIGHT_EDGE            = 6,  ///< Number of pixels between right edge of the window and the options descriptions column.
	MOS_BUTTON_SPACE          = 10, ///< Additional space in the button with the option value (for better looks).

	MOS_ABOVE_GLOBAL_SETTINGS = 6,  ///< Number of vertical pixels between the categories and the global options.
	MOS_BOTTOM_EDGE           = 6,  ///< Number of pixels between bottom edge of the window and bottom of the global options.
};

/** Message options widget numbers. */
enum MessageOptionWidgets {
	WIDGET_NEWSOPT_BACKGROUND,        ///< Background widget.
	WIDGET_NEWSOPT_LABEL,             ///< Top label.
	WIDGET_NEWSOPT_DROP_SUMMARY,      ///< Dropdown that adjusts at once the level for all settings.
	WIDGET_NEWSOPT_LABEL_SUMMARY,     ///< Label of the summary drop down.
	WIDGET_NEWSOPT_SOUNDTICKER,       ///< Button for (de)activating sound on events.
	WIDGET_NEWSOPT_SOUNDTICKER_LABEL, ///< Label of the soundticker button,

	WIDGET_NEWSOPT_START_OPTION,      ///< First widget that is part of a group [<][label][>] [description]
	WIDGET_NEWSOPT_END_OPTION = WIDGET_NEWSOPT_START_OPTION + NT_END * MOS_WIDG_PER_SETTING, ///< First widget after the groups.
};

struct MessageOptionsWindow : Window {
	static const StringID message_opt[]; ///< Message report options, 'off', 'summary', or 'full'.
	int state;                           ///< Option value for setting all categories at once.
	Dimension dim_message_opt;           ///< Amount of space needed for a label such that all labels will fit.

	MessageOptionsWindow(const WindowDesc *desc) : Window()
	{
		this->InitNested(desc);
		/* Set up the initial disabled buttons in the case of 'off' or 'full' */
		NewsDisplay all_val = _news_type_data[0].display;
		for (int i = 0; i < NT_END; i++) {
			this->SetMessageButtonStates(_news_type_data[i].display, i);
			/* If the value doesn't match the ALL-button value, set the ALL-button value to 'off' */
			if (_news_type_data[i].display != all_val) all_val = ND_OFF;
		}
		/* If all values are the same value, the ALL-button will take over this value */
		this->state = all_val;
		this->OnInvalidateData(0);
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
		element *= MOS_WIDG_PER_SETTING;

		this->SetWidgetDisabledState(element + WIDGET_NEWSOPT_START_OPTION, value == 0);
		this->SetWidgetDisabledState(element + WIDGET_NEWSOPT_START_OPTION + 2, value == 2);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget >= WIDGET_NEWSOPT_START_OPTION && widget < WIDGET_NEWSOPT_END_OPTION && (widget -  WIDGET_NEWSOPT_START_OPTION) % MOS_WIDG_PER_SETTING == 1) {
			/* Draw the string of each setting on each button. */
			int i = (widget -  WIDGET_NEWSOPT_START_OPTION) / MOS_WIDG_PER_SETTING;
			DrawString(r.left, r.right, r.top + 2, this->message_opt[_news_type_data[i].display], TC_BLACK, SA_HOR_CENTER);
		}
	}

	virtual void OnInit()
	{
		this->dim_message_opt.width  = 0;
		this->dim_message_opt.height = 0;
		for (const StringID *str = message_opt; *str != INVALID_STRING_ID; str++) this->dim_message_opt = maxdim(this->dim_message_opt, GetStringBoundingBox(*str));
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget >= WIDGET_NEWSOPT_START_OPTION && widget < WIDGET_NEWSOPT_END_OPTION) {
			/* Height is the biggest widget height in a row. */
			size->height = FONT_HEIGHT_NORMAL + max(WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM, WD_IMGBTN_TOP + WD_IMGBTN_BOTTOM);

			/* Compute width for the label widget only. */
			if ((widget - WIDGET_NEWSOPT_START_OPTION) % MOS_WIDG_PER_SETTING == 1) {
				size->width = this->dim_message_opt.width + padding.width + MOS_BUTTON_SPACE; // A bit extra for better looks.
			}
			return;
		}

		/* Size computations for global message options. */
		if (widget == WIDGET_NEWSOPT_DROP_SUMMARY || widget == WIDGET_NEWSOPT_LABEL_SUMMARY || widget == WIDGET_NEWSOPT_SOUNDTICKER || widget == WIDGET_NEWSOPT_SOUNDTICKER_LABEL) {
			/* Height is the biggest widget height in a row. */
			size->height = FONT_HEIGHT_NORMAL + max(WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM, WD_DROPDOWNTEXT_TOP + WD_DROPDOWNTEXT_BOTTOM);

			if (widget == WIDGET_NEWSOPT_DROP_SUMMARY) {
				size->width = this->dim_message_opt.width + padding.width + MOS_BUTTON_SPACE; // A bit extra for better looks.
			} else if (widget == WIDGET_NEWSOPT_SOUNDTICKER) {
				size->width += MOS_BUTTON_SPACE; // A bit extra for better looks.
			}
			return;
		}
	}

	virtual void OnInvalidateData(int data)
	{
		/* Update the dropdown value for 'set all categories'. */
		this->GetWidget<NWidgetCore>(WIDGET_NEWSOPT_DROP_SUMMARY)->widget_data = this->message_opt[this->state];

		/* Update widget to reflect the value of the #_news_ticker_sound variable. */
		this->SetWidgetLoweredState(WIDGET_NEWSOPT_SOUNDTICKER, _news_ticker_sound);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WIDGET_NEWSOPT_DROP_SUMMARY: // Dropdown menu for all settings
				ShowDropDownMenu(this, this->message_opt, this->state, WIDGET_NEWSOPT_DROP_SUMMARY, 0, 0);
				break;

			case WIDGET_NEWSOPT_SOUNDTICKER: // Change ticker sound on/off
				_news_ticker_sound ^= 1;
				this->InvalidateData();
				break;

			default: { // Clicked on the [<] .. [>] widgets
				if (widget >= WIDGET_NEWSOPT_START_OPTION && widget < WIDGET_NEWSOPT_END_OPTION) {
					int wid = widget - WIDGET_NEWSOPT_START_OPTION;
					int element = wid / MOS_WIDG_PER_SETTING;
					byte val = (_news_type_data[element].display + ((wid % MOS_WIDG_PER_SETTING) ? 1 : -1)) % 3;

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
		this->InvalidateData();
	}
};

const StringID MessageOptionsWindow::message_opt[] = {STR_NEWS_MESSAGES_OFF, STR_NEWS_MESSAGES_SUMMARY, STR_NEWS_MESSAGES_FULL, INVALID_STRING_ID};

/** Make a column with the buttons for changing each news category setting, and the global settings. */
static NWidgetBase *MakeButtonsColumn(int *biggest_index)
{
	NWidgetVertical *vert_buttons = new NWidgetVertical;

	/* Top-part of the column, one row for each new category. */
	int widnum = WIDGET_NEWSOPT_START_OPTION;
	for (int i = 0; i < NT_END; i++) {
		NWidgetHorizontal *hor = new NWidgetHorizontal;
		/* [<] button. */
		NWidgetLeaf *leaf = new NWidgetLeaf(WWT_PUSHARROWBTN, COLOUR_YELLOW, widnum, AWV_DECREASE, STR_TOOLTIP_HSCROLL_BAR_SCROLLS_LIST);
		leaf->SetFill(1, 1);
		hor->Add(leaf);
		/* Label. */
		leaf = new NWidgetLeaf(WWT_PUSHTXTBTN, COLOUR_YELLOW, widnum + 1, STR_EMPTY, STR_NULL);
		leaf->SetFill(1, 1);
		hor->Add(leaf);
		/* [>] button. */
		leaf = new NWidgetLeaf(WWT_PUSHARROWBTN, COLOUR_YELLOW, widnum + 2, AWV_INCREASE, STR_TOOLTIP_HSCROLL_BAR_SCROLLS_LIST);
		leaf->SetFill(1, 1);
		hor->Add(leaf);
		vert_buttons->Add(hor);

		widnum += MOS_WIDG_PER_SETTING;
	}
	*biggest_index = widnum - MOS_WIDG_PER_SETTING + 2;

	/* Space between the category buttons and the global settings buttons. */
	NWidgetSpacer *spacer = new NWidgetSpacer(0, MOS_ABOVE_GLOBAL_SETTINGS);
	vert_buttons->Add(spacer);

	/* Bottom part of the column with buttons for global changes. */
	NWidgetLeaf *leaf = new NWidgetLeaf(WWT_DROPDOWN, COLOUR_YELLOW, WIDGET_NEWSOPT_DROP_SUMMARY, STR_EMPTY, STR_NULL);
	leaf->SetFill(1, 1);
	vert_buttons->Add(leaf);

	leaf = new NWidgetLeaf(WWT_TEXTBTN_2, COLOUR_YELLOW, WIDGET_NEWSOPT_SOUNDTICKER, STR_STATION_BUILD_COVERAGE_OFF, STR_NULL);
	leaf->SetFill(1, 1);
	vert_buttons->Add(leaf);

	*biggest_index = max(*biggest_index, max<int>(WIDGET_NEWSOPT_DROP_SUMMARY, WIDGET_NEWSOPT_SOUNDTICKER));
	return vert_buttons;
}

/** Make a column with descriptions for each news category and the global settings. */
static NWidgetBase *MakeDescriptionColumn(int *biggest_index)
{
	NWidgetVertical *vert_desc = new NWidgetVertical;

	/* Top-part of the column, one row for each new category. */
	int widnum = WIDGET_NEWSOPT_START_OPTION;
	for (int i = 0; i < NT_END; i++) {
		NWidgetHorizontal *hor = new NWidgetHorizontal;

		/* Descriptive text. */
		NWidgetLeaf *leaf = new NWidgetLeaf(WWT_TEXT, COLOUR_YELLOW, widnum + 3, _news_type_data[i].description, STR_NULL);
		hor->Add(leaf);
		/* Filling empty space to push text to the left. */
		NWidgetSpacer *spacer = new NWidgetSpacer(0, 0);
		spacer->SetFill(1, 0);
		hor->Add(spacer);
		vert_desc->Add(hor);

		widnum += MOS_WIDG_PER_SETTING;
	}
	*biggest_index = widnum - MOS_WIDG_PER_SETTING + 3;

	/* Space between the category descriptions and the global settings descriptions. */
	NWidgetSpacer *spacer = new NWidgetSpacer(0, MOS_ABOVE_GLOBAL_SETTINGS);
	vert_desc->Add(spacer);

	/* Bottom part of the column with descriptions of global changes. */
	NWidgetHorizontal *hor = new NWidgetHorizontal;
	NWidgetLeaf *leaf = new NWidgetLeaf(WWT_TEXT, COLOUR_YELLOW, WIDGET_NEWSOPT_LABEL_SUMMARY, STR_NEWS_MESSAGES_ALL, STR_NULL);
	hor->Add(leaf);
	/* Filling empty space to push text to the left. */
	spacer = new NWidgetSpacer(0, 0);
	spacer->SetFill(1, 0);
	hor->Add(spacer);
	vert_desc->Add(hor);

	hor = new NWidgetHorizontal;
	leaf = new NWidgetLeaf(WWT_TEXT, COLOUR_YELLOW, WIDGET_NEWSOPT_SOUNDTICKER_LABEL, STR_NEWS_MESSAGES_SOUND, STR_NULL);
	hor->Add(leaf);
	/* Filling empty space to push text to the left. */
	spacer = new NWidgetSpacer(0, 0);
	leaf->SetFill(1, 0);
	hor->Add(spacer);
	vert_desc->Add(hor);

	*biggest_index = max(*biggest_index, max<int>(WIDGET_NEWSOPT_LABEL_SUMMARY, WIDGET_NEWSOPT_SOUNDTICKER_LABEL));
	return vert_desc;
}

static const NWidgetPart _nested_message_options_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_NEWS_MESSAGE_OPTIONS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WIDGET_NEWSOPT_BACKGROUND),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_LABEL, COLOUR_BROWN, WIDGET_NEWSOPT_LABEL), SetMinimalSize(0, 14), SetDataTip(STR_NEWS_MESSAGE_TYPES, STR_NULL),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(MOS_LEFT_EDGE, 0),
			NWidgetFunction(MakeButtonsColumn),
			NWidget(NWID_SPACER), SetMinimalSize(MOS_COLUMN_SPACING, 0),
			NWidgetFunction(MakeDescriptionColumn),
			NWidget(NWID_SPACER), SetMinimalSize(MOS_RIGHT_EDGE, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, MOS_BOTTOM_EDGE),
	EndContainer(),
};

static const WindowDesc _message_options_desc(
	WDP_AUTO, 0, 0,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_message_options_widgets, lengthof(_nested_message_options_widgets)
);

void ShowMessageOptions()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	new MessageOptionsWindow(&_message_options_desc);
}
