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
#include "vehicle_base.h"
#include "vehicle_func.h"
#include "vehicle_gui.h"
#include "roadveh.h"
#include "station_base.h"
#include "industry.h"
#include "town.h"
#include "sound_func.h"
#include "string_func.h"
#include "statusbar_gui.h"
#include "company_manager_face.h"
#include "company_func.h"
#include "engine_base.h"
#include "engine_gui.h"
#include "core/geometry_func.hpp"
#include "command_func.h"
#include "company_base.h"
#include "settings_internal.h"
#include "group_gui.h"
#include "zoom_func.h"
#include "news_cmd.h"
#include "timer/timer.h"
#include "timer/timer_window.h"
#include "timer/timer_game_calendar.h"

#include "widgets/news_widget.h"

#include "table/strings.h"

#include "safeguards.h"

static const uint MIN_NEWS_AMOUNT = 30; ///< preferred minimum amount of news messages.
static const uint MAX_NEWS_AMOUNT = 1U << 10; ///< Do not exceed this number of news messages.

static NewsContainer _news; ///< List of news, with newest items at the start.

/**
 * Forced news item.
 * Users can force an item by accessing the history or "last message".
 * If the message being shown was forced by the user, an iterater is stored
 * in _forced_news. Otherwise, \a _forced_news variable is the end of \a _news.
 */
static NewsIterator _forced_news = std::end(_news);

/** Current news item (last item shown regularly). */
static NewsIterator _current_news = std::end(_news);

/** Current status bar news item. */
static NewsIterator _statusbar_news = std::end(_news);

/**
 * Get pointer to the current status bar news item.
 * @return Pointer to the current status bar news item, or nullptr if there is none.
 */
const NewsItem *GetStatusbarNews()
{
	return (_statusbar_news == std::end(_news)) ? nullptr : &*_statusbar_news;
}

/**
 * Get read-only reference to all news items.
 * @return Read-only reference to all news items.
 */
const NewsContainer &GetNews()
{
	return _news;
}

/**
 * Get the position a news-reference is referencing.
 * @param reftype The type of reference.
 * @param ref     The reference.
 * @return A tile for the referenced object, or INVALID_TILE if none.
 */
static TileIndex GetReferenceTile(NewsReferenceType reftype, uint32_t ref)
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
static constexpr NWidgetPart _nested_normal_news_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_WHITE, WID_N_PANEL),
		NWidget(NWID_HORIZONTAL), SetPadding(1, 1, 0, 1),
			NWidget(WWT_CLOSEBOX, COLOUR_WHITE, WID_N_CLOSEBOX), SetPadding(0, 0, 0, 1),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_LABEL, COLOUR_WHITE, WID_N_DATE), SetDataTip(STR_JUST_DATE_LONG, STR_NULL), SetTextStyle(TC_BLACK, FS_SMALL),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_EMPTY, COLOUR_WHITE, WID_N_MESSAGE), SetMinimalSize(428, 154), SetPadding(0, 5, 1, 5),
	EndContainer(),
};

static WindowDesc _normal_news_desc(
	WDP_MANUAL, nullptr, 0, 0,
	WC_NEWS_WINDOW, WC_NONE,
	0,
	std::begin(_nested_normal_news_widgets), std::end(_nested_normal_news_widgets)
);

/* New vehicles news items. */
static constexpr NWidgetPart _nested_vehicle_news_widgets[] = {
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
	WDP_MANUAL, nullptr, 0, 0,
	WC_NEWS_WINDOW, WC_NONE,
	0,
	std::begin(_nested_vehicle_news_widgets), std::end(_nested_vehicle_news_widgets)
);

/* Company news items. */
static constexpr NWidgetPart _nested_company_news_widgets[] = {
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
	WDP_MANUAL, nullptr, 0, 0,
	WC_NEWS_WINDOW, WC_NONE,
	0,
	std::begin(_nested_company_news_widgets), std::end(_nested_company_news_widgets)
);

/* Thin news items. */
static constexpr NWidgetPart _nested_thin_news_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_WHITE, WID_N_PANEL),
		NWidget(NWID_HORIZONTAL), SetPadding(1, 1, 0, 1),
			NWidget(WWT_CLOSEBOX, COLOUR_WHITE, WID_N_CLOSEBOX), SetPadding(0, 0, 0, 1),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_LABEL, COLOUR_WHITE, WID_N_DATE), SetDataTip(STR_JUST_DATE_LONG, STR_NULL), SetTextStyle(TC_BLACK, FS_SMALL),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_EMPTY, COLOUR_WHITE, WID_N_MESSAGE), SetMinimalSize(428, 48), SetFill(1, 0), SetPadding(0, 5, 0, 5),
		NWidget(NWID_VIEWPORT, INVALID_COLOUR, WID_N_VIEWPORT), SetMinimalSize(426, 70), SetPadding(1, 2, 2, 2),
	EndContainer(),
};

static WindowDesc _thin_news_desc(
	WDP_MANUAL, nullptr, 0, 0,
	WC_NEWS_WINDOW, WC_NONE,
	0,
	std::begin(_nested_thin_news_widgets), std::end(_nested_thin_news_widgets)
);

/* Small news items. */
static constexpr NWidgetPart _nested_small_news_widgets[] = {
	/* Caption + close box. The caption is no WWT_CAPTION as the window shall not be moveable and so on. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE, WID_N_CLOSEBOX),
		NWidget(WWT_EMPTY, COLOUR_LIGHT_BLUE, WID_N_CAPTION), SetFill(1, 0),
		NWidget(WWT_TEXTBTN, COLOUR_LIGHT_BLUE, WID_N_SHOW_GROUP), SetMinimalSize(14, 11), SetResize(1, 0),
				SetDataTip(STR_NULL /* filled in later */, STR_NEWS_SHOW_VEHICLE_GROUP_TOOLTIP),
	EndContainer(),

	/* Main part */
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, WID_N_HEADLINE),
		NWidget(WWT_INSET, COLOUR_LIGHT_BLUE, WID_N_INSET), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, INVALID_COLOUR, WID_N_VIEWPORT), SetMinimalSize(274, 47), SetFill(1, 0),
		EndContainer(),
		NWidget(WWT_EMPTY, COLOUR_WHITE, WID_N_MESSAGE), SetMinimalSize(275, 20), SetFill(1, 0), SetPadding(0, 5, 0, 5),
	EndContainer(),
};

static WindowDesc _small_news_desc(
	WDP_MANUAL, nullptr, 0, 0,
	WC_NEWS_WINDOW, WC_NONE,
	0,
	std::begin(_nested_small_news_widgets), std::end(_nested_small_news_widgets)
);

/**
 * Window layouts for news items.
 */
static WindowDesc *_news_window_layout[] = {
	&_thin_news_desc,    ///< NF_THIN
	&_small_news_desc,   ///< NF_SMALL
	&_normal_news_desc,  ///< NF_NORMAL
	&_vehicle_news_desc, ///< NF_VEHICLE
	&_company_news_desc, ///< NF_COMPANY
};

WindowDesc *GetNewsWindowLayout(NewsFlag flags)
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
	NewsTypeData("news_display.accident_other",    90, SND_BEGIN       ),  ///< NT_ACCIDENT_OTHER
	NewsTypeData("news_display.company_info",      60, SND_BEGIN       ),  ///< NT_COMPANY_INFO
	NewsTypeData("news_display.open",              90, SND_BEGIN       ),  ///< NT_INDUSTRY_OPEN
	NewsTypeData("news_display.close",             90, SND_BEGIN       ),  ///< NT_INDUSTRY_CLOSE
	NewsTypeData("news_display.economy",           30, SND_BEGIN       ),  ///< NT_ECONOMY
	NewsTypeData("news_display.production_player", 30, SND_BEGIN       ),  ///< NT_INDUSTRY_COMPANY
	NewsTypeData("news_display.production_other",  30, SND_BEGIN       ),  ///< NT_INDUSTRY_OTHER
	NewsTypeData("news_display.production_nobody", 30, SND_BEGIN       ),  ///< NT_INDUSTRY_NOBODY
	NewsTypeData("news_display.advice",           150, SND_BEGIN       ),  ///< NT_ADVICE
	NewsTypeData("news_display.new_vehicles",      30, SND_1E_NEW_ENGINE), ///< NT_NEW_VEHICLES
	NewsTypeData("news_display.acceptance",        90, SND_BEGIN       ),  ///< NT_ACCEPTANCE
	NewsTypeData("news_display.subsidies",        180, SND_BEGIN       ),  ///< NT_SUBSIDIES
	NewsTypeData("news_display.general",           60, SND_BEGIN       ),  ///< NT_GENERAL
};

static_assert(lengthof(_news_type_data) == NT_END);

/**
 * Return the news display option.
 * @return display options
 */
NewsDisplay NewsTypeData::GetDisplay() const
{
	const SettingDesc *sd = GetSettingFromName(this->name);
	assert(sd != nullptr && sd->IsIntSetting());
	return (NewsDisplay)sd->AsIntSetting()->Read(nullptr);
}

/** Window class displaying a news item. */
struct NewsWindow : Window {
	uint16_t chat_height;   ///< Height of the chat window.
	uint16_t status_height; ///< Height of the status bar window
	const NewsItem *ni;   ///< News item to display.
	static int duration;  ///< Remaining time for showing the current news message (may only be access while a news item is displayed).

	NewsWindow(WindowDesc *desc, const NewsItem *ni) : Window(desc), ni(ni)
	{
		NewsWindow::duration = 16650;
		const Window *w = FindWindowByClass(WC_SEND_NETWORK_MSG);
		this->chat_height = (w != nullptr) ? w->height : 0;
		this->status_height = FindWindowById(WC_STATUS_BAR, 0)->height;

		this->flags |= WF_DISABLE_VP_SCROLL;

		this->CreateNestedTree();

		/* For company news with a face we have a separate headline in param[0] */
		if (desc == &_company_news_desc) this->GetWidget<NWidgetCore>(WID_N_TITLE)->widget_data = this->ni->params[0].data;

		NWidgetCore *nwid = this->GetWidget<NWidgetCore>(WID_N_SHOW_GROUP);
		if (ni->reftype1 == NR_VEHICLE && nwid != nullptr) {
			const Vehicle *v = Vehicle::Get(ni->ref1);
			switch (v->type) {
				case VEH_TRAIN:
					nwid->widget_data = STR_TRAIN;
					break;
				case VEH_ROAD:
					nwid->widget_data = RoadVehicle::From(v)->IsBus() ? STR_BUS : STR_LORRY;
					break;
				case VEH_SHIP:
					nwid->widget_data = STR_SHIP;
					break;
				case VEH_AIRCRAFT:
					nwid->widget_data = STR_PLANE;
					break;
				default:
					break; // Do nothing
			}
		}

		this->FinishInitNested(0);

		/* Initialize viewport if it exists. */
		NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WID_N_VIEWPORT);
		if (nvp != nullptr) {
			if (ni->reftype1 == NR_VEHICLE) {
				nvp->InitializeViewport(this, static_cast<VehicleID>(ni->ref1), ScaleZoomGUI(ZOOM_LVL_NEWS));
			} else {
				nvp->InitializeViewport(this, GetReferenceTile(ni->reftype1, ni->ref1), ScaleZoomGUI(ZOOM_LVL_NEWS));
			}
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
		Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
		GfxFillRect(ir, PC_WHITE);

		ir = ir.Expand(1);
		GfxFillRect( r.left,   r.top,    ir.left,   r.bottom, PC_BLACK);
		GfxFillRect(ir.right,  r.top,     r.right,  r.bottom, PC_BLACK);
		GfxFillRect( r.left,   r.top,     r.right, ir.top,    PC_BLACK);
		GfxFillRect( r.left,  ir.bottom,  r.right,  r.bottom, PC_BLACK);
	}

	Point OnInitialPosition([[maybe_unused]] int16_t sm_width, [[maybe_unused]] int16_t sm_height, [[maybe_unused]] int window_number) override
	{
		Point pt = { 0, _screen.height };
		return pt;
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		StringID str = STR_NULL;
		switch (widget) {
			case WID_N_CAPTION: {
				/* Caption is not a real caption (so that the window cannot be moved)
				 * thus it doesn't get the default sizing of a caption. */
				Dimension d2 = GetStringBoundingBox(STR_NEWS_MESSAGE_CAPTION);
				d2.height += WidgetDimensions::scaled.captiontext.Vertical();
				*size = maxdim(*size, d2);
				return;
			}

			case WID_N_MGR_FACE:
				*size = maxdim(*size, GetScaledSpriteSize(SPR_GRADIENT));
				break;

			case WID_N_MGR_NAME:
				SetDParamStr(0, static_cast<const CompanyNewsInformation *>(this->ni->data.get())->president_name);
				str = STR_JUST_RAW_STRING;
				break;

			case WID_N_MESSAGE:
				CopyInDParam(this->ni->params);
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

			case WID_N_SHOW_GROUP:
				if (this->ni->reftype1 == NR_VEHICLE) {
					Dimension d2 = GetStringBoundingBox(this->GetWidget<NWidgetCore>(WID_N_SHOW_GROUP)->widget_data);
					d2.height += WidgetDimensions::scaled.captiontext.Vertical();
					d2.width += WidgetDimensions::scaled.captiontext.Horizontal();
					*size = d2;
				} else {
					/* Hide 'Show group window' button if this news is not about a vehicle. */
					size->width = 0;
					size->height = 0;
					resize->width = 0;
					resize->height = 0;
					fill->width = 0;
					fill->height = 0;
				}
				return;

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

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_N_DATE) SetDParam(0, this->ni->date);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_N_CAPTION:
				DrawCaption(r, COLOUR_LIGHT_BLUE, this->owner, TC_FROMSTRING, STR_NEWS_MESSAGE_CAPTION, SA_CENTER, FS_NORMAL);
				break;

			case WID_N_PANEL:
				this->DrawNewsBorder(r);
				break;

			case WID_N_MESSAGE:
				CopyInDParam(this->ni->params);
				DrawStringMultiLine(r.left, r.right, r.top, r.bottom, this->ni->string_id, TC_FROMSTRING, SA_CENTER);
				break;

			case WID_N_MGR_FACE: {
				const CompanyNewsInformation *cni = static_cast<const CompanyNewsInformation*>(this->ni->data.get());
				DrawCompanyManagerFace(cni->face, cni->colour, r);
				GfxFillRect(r.left, r.top, r.right, r.bottom, PALETTE_NEWSPAPER, FILLRECT_RECOLOUR);
				break;
			}
			case WID_N_MGR_NAME: {
				const CompanyNewsInformation *cni = static_cast<const CompanyNewsInformation*>(this->ni->data.get());
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
				DrawVehicleEngine(r.left, r.right, CenterBounds(r.left, r.right, 0), CenterBounds(r.top, r.bottom, 0), engine, GetEnginePalette(engine, _local_company), EIT_PREVIEW);
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

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_N_CLOSEBOX:
				NewsWindow::duration = 0;
				this->Close();
				_forced_news = std::end(_news);
				break;

			case WID_N_CAPTION:
				if (this->ni->reftype1 == NR_VEHICLE) {
					const Vehicle *v = Vehicle::Get(this->ni->ref1);
					ShowVehicleViewWindow(v);
				}
				break;

			case WID_N_VIEWPORT:
				break; // Ignore clicks

			case WID_N_SHOW_GROUP:
				if (this->ni->reftype1 == NR_VEHICLE) {
					const Vehicle *v = Vehicle::Get(this->ni->ref1);
					ShowCompanyGroupForVehicle(v);
				}
				break;
			default:
				if (this->ni->reftype1 == NR_VEHICLE) {
					const Vehicle *v = Vehicle::Get(this->ni->ref1);
					ScrollMainWindowTo(v->x_pos, v->y_pos, v->z_pos);
				} else {
					TileIndex tile1 = GetReferenceTile(this->ni->reftype1, this->ni->ref1);
					TileIndex tile2 = GetReferenceTile(this->ni->reftype2, this->ni->ref2);
					if (_ctrl_pressed) {
						if (tile1 != INVALID_TILE) ShowExtraViewportWindow(tile1);
						if (tile2 != INVALID_TILE) ShowExtraViewportWindow(tile2);
					} else {
						if ((tile1 == INVALID_TILE || !ScrollMainWindowToTile(tile1)) && tile2 != INVALID_TILE) {
							ScrollMainWindowToTile(tile2);
						}
					}
				}
				break;
		}
	}

	void OnResize() override
	{
		if (this->viewport != nullptr) {
			NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WID_N_VIEWPORT);
			nvp->UpdateViewportCoordinates(this);

			if (ni->reftype1 != NR_VEHICLE) {
				ScrollWindowToTile(GetReferenceTile(ni->reftype1, ni->ref1), this, true); // Re-center viewport.
			}
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		/* The chatbar has notified us that is was either created or closed */
		int newtop = this->top + this->chat_height - data;
		this->chat_height = data;
		this->SetWindowTop(newtop);
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		/* Decrement the news timer. We don't need to action an elapsed event here,
		 * so no need to use TimerElapsed(). */
		if (NewsWindow::duration > 0) NewsWindow::duration -= delta_ms;
	}

	/**
	 * Scroll the news message slowly up from the bottom.
	 *
	 * The interval of 210ms is chosen to maintain 15ms at normal zoom: 210 / GetCharacterHeight(FS_NORMAL) = 15ms.
	 */
	IntervalTimer<TimerWindow> scroll_interval = {std::chrono::milliseconds(210) / GetCharacterHeight(FS_NORMAL), [this](uint count) {
		int newtop = std::max(this->top - 2 * static_cast<int>(count), _screen.height - this->height - this->status_height - this->chat_height);
		this->SetWindowTop(newtop);
	}};

private:
	/**
	 * Moves the window to a new #top coordinate. Makes screen dirty where needed.
	 * @param newtop new top coordinate
	 */
	void SetWindowTop(int newtop)
	{
		if (this->top == newtop) return;

		int mintop = std::min(newtop, this->top);
		int maxtop = std::max(newtop, this->top);
		if (this->viewport != nullptr) this->viewport->top += newtop - this->top;
		this->top = newtop;

		AddDirtyBlock(this->left, mintop, this->left + this->width, maxtop + this->height);
	}

	StringID GetCompanyMessageString() const
	{
		/* Company news with a face have a separate headline, so the normal message is shifted by two params */
		CopyInDParam(std::span(this->ni->params.data() + 2, this->ni->params.size() - 2));
		return this->ni->params[1].data;
	}

	StringID GetNewVehicleMessageString(WidgetID widget) const
	{
		assert(this->ni->reftype1 == NR_ENGINE);
		EngineID engine = this->ni->ref1;

		switch (widget) {
			case WID_N_VEH_TITLE:
				SetDParam(0, GetEngineCategoryName(engine));
				return STR_NEWS_NEW_VEHICLE_NOW_AVAILABLE;

			case WID_N_VEH_NAME:
				SetDParam(0, PackEngineNameDParam(engine, EngineNameContext::PreviewNews));
				return STR_NEWS_NEW_VEHICLE_TYPE;

			default:
				NOT_REACHED();
		}
	}
};

/* static */ int NewsWindow::duration = 0; // Instance creation.

/** Open up an own newspaper window for the news item */
static void ShowNewspaper(const NewsItem *ni)
{
	SoundFx sound = _news_type_data[ni->type].sound;
	if (sound != 0 && _settings_client.sound.news_full) SndPlayFx(sound);

	new NewsWindow(GetNewsWindowLayout(ni->flags), ni);
}

/** Show news item in the ticker */
static void ShowTicker(NewsIterator ni)
{
	if (_settings_client.sound.news_ticker) SndPlayFx(SND_16_NEWS_TICKER);

	_statusbar_news = ni;
	InvalidateWindowData(WC_STATUS_BAR, 0, SBI_SHOW_TICKER);
}

/** Initialize the news-items data structures */
void InitNewsItemStructs()
{
	_news.clear();
	_forced_news = std::end(_news);
	_current_news = std::end(_news);
	_statusbar_news = std::end(_news);
	NewsWindow::duration = 0;
}

/**
 * Are we ready to show another ticker item?
 * Only if nothing is in the newsticker is displayed
 */
static bool ReadyForNextTickerItem()
{
	const NewsItem *ni = GetStatusbarNews();
	if (ni == nullptr) return true;

	/* Ticker message
	 * Check if the status bar message is still being displayed? */
	return !IsNewsTickerShown();
}

/**
 * Are we ready to show another news item?
 * Only if no newspaper is displayed
 */
static bool ReadyForNextNewsItem()
{
	if (_forced_news == std::end(_news) && _current_news == std::end(_news)) return true;

	/* neither newsticker nor newspaper are running */
	return (NewsWindow::duration <= 0 || FindWindowById(WC_NEWS_WINDOW, 0) == nullptr);
}

/** Move to the next ticker item */
static void MoveToNextTickerItem()
{
	assert(!std::empty(_news));

	/* There is no status bar, so no reason to show news;
	 * especially important with the end game screen when
	 * there is no status bar but possible news. */
	if (FindWindowById(WC_STATUS_BAR, 0) == nullptr) return;

	/* if we're not at the latest item, then move on */
	while (_statusbar_news != std::begin(_news)) {
		--_statusbar_news;
		const NewsType type = _statusbar_news->type;

		/* check the date, don't show too old items */
		if (TimerGameEconomy::date - _news_type_data[type].age > _statusbar_news->economy_date) continue;

		switch (_news_type_data[type].GetDisplay()) {
			default: NOT_REACHED();
			case ND_OFF: // Off - show nothing only a small reminder in the status bar
				InvalidateWindowData(WC_STATUS_BAR, 0, SBI_SHOW_REMINDER);
				return;

			case ND_SUMMARY: // Summary - show ticker
				ShowTicker(_statusbar_news);
				return;

			case ND_FULL: // Full - show newspaper, skipped here
				break;;
		}
	}
}

/** Move to the next news item */
static void MoveToNextNewsItem()
{
	assert(!std::empty(_news));

	/* There is no status bar, so no reason to show news;
	 * especially important with the end game screen when
	 * there is no status bar but possible news. */
	if (FindWindowById(WC_STATUS_BAR, 0) == nullptr) return;

	CloseWindowById(WC_NEWS_WINDOW, 0); // close the newspapers window if shown
	_forced_news = std::end(_news);

	/* if we're not at the latest item, then move on */
	while (_current_news != std::begin(_news)) {
		--_current_news;
		const NewsType type = _current_news->type;

		/* check the date, don't show too old items */
		if (TimerGameEconomy::date - _news_type_data[type].age > _current_news->economy_date) continue;

		switch (_news_type_data[type].GetDisplay()) {
			default: NOT_REACHED();
			case ND_OFF: // Off - show nothing only a small reminder in the status bar, skipped here
				break;

			case ND_SUMMARY: // Summary - show ticker, skipped here
				break;;

			case ND_FULL: // Full - show newspaper
				ShowNewspaper(&*_current_news);
				return;
		}
	}
}

/** Delete a news item from the queue */
static std::list<NewsItem>::iterator DeleteNewsItem(std::list<NewsItem>::iterator ni)
{
	if (_forced_news == ni || _current_news == ni) {
		/* When we're the current news, go to the previous item first;
		 * we just possibly made that the last news item. */
		if (_current_news == ni) _current_news = (_current_news == std::begin(_news)) ? std::end(_news) : std::prev(_current_news);

		/* About to remove the currently forced item (shown as newspapers) ||
		 * about to remove the currently displayed item (newspapers) */
		MoveToNextNewsItem();
	}

	if (_statusbar_news == ni) {
		/* When we're the current news, go to the previous item first;
		 * we just possibly made that the last news item. */
		 if (_statusbar_news == ni) _statusbar_news = (_statusbar_news == std::begin(_news)) ? std::end(_news) : std::prev(_statusbar_news);

		/* About to remove the currently displayed item (ticker, or just a reminder) */
		InvalidateWindowData(WC_STATUS_BAR, 0, SBI_NEWS_DELETED); // invalidate the statusbar
		MoveToNextTickerItem();
	}

	/* Delete the news from the news queue. */
	return _news.erase(ni);
}

/**
 * Create a new newsitem to be shown.
 * @param string_id String to display.
 * @param type      The type of news.
 * @param flags     Flags related to how to display the news.
 * @param reftype1  Type of ref1.
 * @param ref1      Reference 1 to some object: Used for a possible viewport, scrolling after clicking on the news, and for deleting the news when the object is deleted.
 * @param reftype2  Type of ref2.
 * @param ref2      Reference 2 to some object: Used for scrolling after clicking on the news, and for deleting the news when the object is deleted.
 * @param data      Pointer to data that must be released once the news message is cleared.
 *
 * @see NewsSubtype
 */
NewsItem::NewsItem(StringID string_id, NewsType type, NewsFlag flags, NewsReferenceType reftype1, uint32_t ref1, NewsReferenceType reftype2, uint32_t ref2, const NewsAllocatedData *data) :
	string_id(string_id), date(TimerGameCalendar::date), economy_date(TimerGameEconomy::date), type(type), flags(flags), reftype1(reftype1), reftype2(reftype2), ref1(ref1), ref2(ref2), data(data)
{
	/* show this news message in colour? */
	if (TimerGameCalendar::year >= _settings_client.gui.coloured_news_year) this->flags |= NF_INCOLOUR;
	CopyOutDParam(this->params, 10);
}

/**
 * Add a new newsitem to be shown.
 * @param string String to display
 * @param type news category
 * @param flags display flags for the news
 * @param reftype1 Type of ref1
 * @param ref1     Reference 1 to some object: Used for a possible viewport, scrolling after clicking on the news, and for deleting the news when the object is deleted.
 * @param reftype2 Type of ref2
 * @param ref2     Reference 2 to some object: Used for scrolling after clicking on the news, and for deleting the news when the object is deleted.
 * @param data     Pointer to data that must be released once the news message is cleared.
 *
 * @see NewsSubtype
 */
void AddNewsItem(StringID string, NewsType type, NewsFlag flags, NewsReferenceType reftype1, uint32_t ref1, NewsReferenceType reftype2, uint32_t ref2, const NewsAllocatedData *data)
{
	if (_game_mode == GM_MENU) return;

	/* Create new news item node */
	_news.emplace_front(string, type, flags, reftype1, ref1, reftype2, ref2, data);

	/* Keep the number of stored news items to a managable number */
	if (std::size(_news) > MAX_NEWS_AMOUNT) {
		DeleteNewsItem(std::prev(std::end(_news)));
	}

	InvalidateWindowData(WC_MESSAGE_HISTORY, 0);
}

/**
 * Create a new custom news item.
 * @param flags type of operation
 * @aram type NewsType of the message.
 * @param reftype1 NewsReferenceType of first reference.
 * @param company Company this news message is for.
 * @param reference First reference of the news message.
 * @param text The text of the news message.
 * @return the cost of this operation or an error
 */
CommandCost CmdCustomNewsItem(DoCommandFlag flags, NewsType type, NewsReferenceType reftype1, CompanyID company, uint32_t reference, const std::string &text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;

	if (company != INVALID_OWNER && !Company::IsValidID(company)) return CMD_ERROR;
	if (type >= NT_END) return CMD_ERROR;
	if (text.empty()) return CMD_ERROR;

	switch (reftype1) {
		case NR_NONE: break;
		case NR_TILE:
			if (!IsValidTile(reference)) return CMD_ERROR;
			break;

		case NR_VEHICLE:
			if (!Vehicle::IsValidID(reference)) return CMD_ERROR;
			break;

		case NR_STATION:
			if (!Station::IsValidID(reference)) return CMD_ERROR;
			break;

		case NR_INDUSTRY:
			if (!Industry::IsValidID(reference)) return CMD_ERROR;
			break;

		case NR_TOWN:
			if (!Town::IsValidID(reference)) return CMD_ERROR;
			break;

		case NR_ENGINE:
			if (!Engine::IsValidID(reference)) return CMD_ERROR;
			break;

		default: return CMD_ERROR;
	}

	if (company != INVALID_OWNER && company != _local_company) return CommandCost();

	if (flags & DC_EXEC) {
		NewsStringData *news = new NewsStringData(text);
		SetDParamStr(0, news->string);
		AddNewsItem(STR_NEWS_CUSTOM_ITEM, type, NF_NORMAL, reftype1, reference, NR_NONE, UINT32_MAX, news);
	}

	return CommandCost();
}

/**
 * Delete news items by predicate, and invalidate the message history if necessary.
 * @tparam Tmin Stop if the number of news items remaining reaches \a min items.
 * @tparam Tpredicate Condition for a news item to be deleted.
 */
template <size_t Tmin = 0, class Tpredicate>
void DeleteNews(Tpredicate predicate)
{
	bool dirty = false;
	for (auto it = std::rbegin(_news); it != std::rend(_news); /* nothing */) {
		if constexpr (Tmin > 0) {
			if (std::size(_news) <= Tmin) break;
		}
		if (predicate(*it)) {
			it = std::make_reverse_iterator(DeleteNewsItem(std::prev(it.base())));
			dirty = true;
		} else {
			++it;
		}
	}
	if (dirty) InvalidateWindowData(WC_MESSAGE_HISTORY, 0);
}

/**
 * Delete a news item type about a vehicle.
 * When the news item type is INVALID_STRING_ID all news about the vehicle gets deleted.
 * @param vid  The vehicle to remove the news for.
 * @param news The news type to remove.
 */
void DeleteVehicleNews(VehicleID vid, StringID news)
{
	DeleteNews([&](const auto &ni) {
		return ((ni.reftype1 == NR_VEHICLE && ni.ref1 == vid) || (ni.reftype2 == NR_VEHICLE && ni.ref2 == vid)) && (news == INVALID_STRING_ID || ni.string_id == news);
	});
}

/**
 * Remove news regarding given station so there are no 'unknown station now accepts Mail'
 * or 'First train arrived at unknown station' news items.
 * @param sid station to remove news about
 */
void DeleteStationNews(StationID sid)
{
	DeleteNews([&](const auto &ni) {
		return (ni.reftype1 == NR_STATION && ni.ref1 == sid) || (ni.reftype2 == NR_STATION && ni.ref2 == sid);
	});
}

/**
 * Remove news regarding given industry
 * @param iid industry to remove news about
 */
void DeleteIndustryNews(IndustryID iid)
{
	DeleteNews([&](const auto &ni) {
		return (ni.reftype1 == NR_INDUSTRY && ni.ref1 == iid) || (ni.reftype2 == NR_INDUSTRY && ni.ref2 == iid);
	});
}

/**
 * Remove engine announcements for invalid engines.
 */
void DeleteInvalidEngineNews()
{
	DeleteNews([](const auto &ni) {
		return (ni.reftype1 == NR_ENGINE && (!Engine::IsValidID(ni.ref1) || !Engine::Get(ni.ref1)->IsEnabled())) ||
				(ni.reftype2 == NR_ENGINE && (!Engine::IsValidID(ni.ref2) || !Engine::Get(ni.ref2)->IsEnabled()));
	});
}

static void RemoveOldNewsItems()
{
	DeleteNews<MIN_NEWS_AMOUNT>([](const auto &ni) {
		return TimerGameEconomy::date - _news_type_data[ni.type].age * _settings_client.gui.news_message_timeout > ni.economy_date;
	});
}

/**
 * Report a change in vehicle IDs (due to autoreplace) to affected vehicle news.
 * @note Viewports of currently displayed news is changed via #ChangeVehicleViewports
 * @param from_index the old vehicle ID
 * @param to_index the new vehicle ID
 */
void ChangeVehicleNews(VehicleID from_index, VehicleID to_index)
{
	for (auto &ni : _news) {
		if (ni.reftype1 == NR_VEHICLE && ni.ref1 == from_index) ni.ref1 = to_index;
		if (ni.reftype2 == NR_VEHICLE && ni.ref2 == from_index) ni.ref2 = to_index;
		if (ni.flags & NF_VEHICLE_PARAM0 && ni.params[0].data == from_index) ni.params[0] = to_index;
	}
}

void NewsLoop()
{
	/* no news item yet */
	if (std::empty(_news)) return;

	static TimerGameEconomy::Month _last_clean_month = 0;

	if (_last_clean_month != TimerGameEconomy::month) {
		RemoveOldNewsItems();
		_last_clean_month = TimerGameEconomy::month;
	}

	if (ReadyForNextTickerItem()) MoveToNextTickerItem();
	if (ReadyForNextNewsItem()) MoveToNextNewsItem();
}

/** Do a forced show of a specific message */
static void ShowNewsMessage(NewsIterator ni)
{
	assert(!std::empty(_news));

	/* Delete the news window */
	CloseWindowById(WC_NEWS_WINDOW, 0);

	/* setup forced news item */
	_forced_news = ni;

	if (_forced_news != std::end(_news)) {
		CloseWindowById(WC_NEWS_WINDOW, 0);
		ShowNewspaper(&*ni);
	}
}

/**
 * Close active news message window
 * @return true if a window was closed.
 */
bool HideActiveNewsMessage()
{
	NewsWindow *w = dynamic_cast<NewsWindow *>(FindWindowById(WC_NEWS_WINDOW, 0));
	if (w == nullptr) return false;
	w->Close();
	return true;
}

/** Show previous news item */
void ShowLastNewsMessage()
{
	if (std::empty(_news)) return;

	NewsIterator ni;
	if (_forced_news == std::end(_news)) {
		/* Not forced any news yet, show the current one, unless a news window is
		 * open (which can only be the current one), then show the previous item */
		if (_current_news == std::end(_news)) {
			/* No news were shown yet resp. the last shown one was already deleted.
			 * Treat this as if _forced_news reached the oldest news; so, wrap around and start anew with the latest. */
			ni = std::begin(_news);
		} else {
			const Window *w = FindWindowById(WC_NEWS_WINDOW, 0);
			ni = (w == nullptr || (std::next(_current_news) == std::end(_news))) ? _current_news : std::next(_current_news);
		}
	} else if (std::next(_forced_news) == std::end(_news)) {
		/* We have reached the oldest news, start anew with the latest */
		ni = std::begin(_news);
	} else {
		/* 'Scrolling' through news history show each one in turn */
		ni = std::next(_forced_news);
	}
	bool wrap = false;
	for (;;) {
		if (_news_type_data[ni->type].GetDisplay() != ND_OFF) {
			ShowNewsMessage(ni);
			break;
		}

		++ni;
		if (ni == std::end(_news)) {
			if (wrap) break;
			/* We have reached the oldest news, start anew with the latest */
			ni = std::begin(_news);
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
 */
static void DrawNewsString(uint left, uint right, int y, TextColour colour, const NewsItem *ni)
{
	CopyInDParam(ni->params);

	/* Get the string, replaces newlines with spaces and remove control codes from the string. */
	std::string message = StrMakeValid(GetString(ni->string_id), SVS_REPLACE_TAB_CR_NL_WITH_SPACE);

	/* Truncate and show string; postfixed by '...' if necessary */
	DrawString(left, right, y, message, colour);
}

struct MessageHistoryWindow : Window {
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

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget == WID_MH_BACKGROUND) {
			this->line_height = GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;
			resize->height = this->line_height;

			/* Months are off-by-one, so it's actually 8. Not using
			 * month 12 because the 1 is usually less wide. */
			SetDParam(0, TimerGameCalendar::ConvertYMDToDate(CalendarTime::ORIGINAL_MAX_YEAR, 7, 30));
			this->date_width = GetStringBoundingBox(STR_JUST_DATE_TINY).width + WidgetDimensions::scaled.hsep_wide;

			size->height = 4 * resize->height + WidgetDimensions::scaled.framerect.Vertical(); // At least 4 lines are visible.
			size->width = std::max(200u, size->width); // At least 200 pixels wide.
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_MH_BACKGROUND || std::empty(_news)) return;

		/* Fill the widget with news items. */
		bool rtl = _current_text_dir == TD_RTL;
		Rect news = r.Shrink(WidgetDimensions::scaled.framerect).Indent(this->date_width + WidgetDimensions::scaled.hsep_wide, rtl);
		Rect date = r.Shrink(WidgetDimensions::scaled.framerect).WithWidth(this->date_width, rtl);
		int y = news.top;

		auto [first, last] = this->vscroll->GetVisibleRangeIterators(_news);
		for (auto ni = first; ni != last; ++ni) {
			SetDParam(0, ni->date);
			DrawString(date.left, date.right, y, STR_JUST_DATE_TINY, TC_WHITE);

			DrawNewsString(news.left, news.right, y, TC_WHITE, &*ni);
			y += this->line_height;
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->vscroll->SetCount(std::size(_news));
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget == WID_MH_BACKGROUND) {
			/* Scheduled window invalidations currently occur after the input loop, which means the scrollbar count
			 * could be invalid, so ensure it's correct now. Potentially this means that item clicked on might be
			 * different as well. */
			this->vscroll->SetCount(std::size(_news));
			auto ni = this->vscroll->GetScrolledItemFromWidget(_news, pt.y, this, widget);
			if (ni == std::end(_news)) return;

			ShowNewsMessage(ni);
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_MH_BACKGROUND);
	}
};

static constexpr NWidgetPart _nested_message_history[] = {
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
	std::begin(_nested_message_history), std::end(_nested_message_history)
);

/** Display window with news messages history */
void ShowMessageHistory()
{
	CloseWindowById(WC_MESSAGE_HISTORY, 0);
	new MessageHistoryWindow(&_message_history_desc);
}
