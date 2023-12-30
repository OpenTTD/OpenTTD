/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_gui.cpp Implementation of the Network related GUIs. */

#include "../stdafx.h"
#include "../strings_func.h"
#include "../fios.h"
#include "network_client.h"
#include "network_gui.h"
#include "network_gamelist.h"
#include "network.h"
#include "network_base.h"
#include "network_content.h"
#include "network_server.h"
#include "network_coordinator.h"
#include "network_survey.h"
#include "../gui.h"
#include "network_udp.h"
#include "../window_func.h"
#include "../gfx_func.h"
#include "../widgets/dropdown_type.h"
#include "../widgets/dropdown_func.h"
#include "../querystring_gui.h"
#include "../sortlist_type.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../core/geometry_func.hpp"
#include "../genworld.h"
#include "../map_type.h"
#include "../zoom_func.h"
#include "../sprite.h"
#include "../settings_internal.h"
#include "../company_cmd.h"
#include "../timer/timer.h"
#include "../timer/timer_window.h"
#include "../timer/timer_game_calendar.h"
#include "../textfile_gui.h"

#include "../widgets/network_widget.h"

#include "table/strings.h"
#include "../table/sprites.h"

#include "../stringfilter_type.h"

#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#endif

#include "../safeguards.h"

static void ShowNetworkStartServerWindow();

static ClientID _admin_client_id = INVALID_CLIENT_ID; ///< For what client a confirmation window is open.
static CompanyID _admin_company_id = INVALID_COMPANY; ///< For what company a confirmation window is open.

/**
 * Update the network new window because a new server is
 * found on the network.
 */
void UpdateNetworkGameWindow()
{
	InvalidateWindowData(WC_NETWORK_WINDOW, WN_NETWORK_WINDOW_GAME, 0);
}

static DropDownList BuildVisibilityDropDownList()
{
	DropDownList list;

	list.push_back(std::make_unique<DropDownListStringItem>(STR_NETWORK_SERVER_VISIBILITY_LOCAL, SERVER_GAME_TYPE_LOCAL, false));
	list.push_back(std::make_unique<DropDownListStringItem>(STR_NETWORK_SERVER_VISIBILITY_INVITE_ONLY, SERVER_GAME_TYPE_INVITE_ONLY, false));
	list.push_back(std::make_unique<DropDownListStringItem>(STR_NETWORK_SERVER_VISIBILITY_PUBLIC, SERVER_GAME_TYPE_PUBLIC, false));

	return list;
}

typedef GUIList<NetworkGameList*, std::nullptr_t, StringFilter&> GUIGameServerList;
typedef int ServerListPosition;
static const ServerListPosition SLP_INVALID = -1;

/** Full blown container to make it behave exactly as we want :) */
class NWidgetServerListHeader : public NWidgetContainer {
	static const uint MINIMUM_NAME_WIDTH_BEFORE_NEW_HEADER = 150; ///< Minimum width before adding a new header
public:
	NWidgetServerListHeader() : NWidgetContainer(NWID_HORIZONTAL)
	{
		auto leaf = std::make_unique<NWidgetLeaf>(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_NAME, STR_NETWORK_SERVER_LIST_GAME_NAME, STR_NETWORK_SERVER_LIST_GAME_NAME_TOOLTIP);
		leaf->SetResize(1, 0);
		leaf->SetFill(1, 0);
		this->Add(std::move(leaf));

		this->Add(std::make_unique<NWidgetLeaf>(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_CLIENTS, STR_NETWORK_SERVER_LIST_CLIENTS_CAPTION, STR_NETWORK_SERVER_LIST_CLIENTS_CAPTION_TOOLTIP));
		this->Add(std::make_unique<NWidgetLeaf>(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_MAPSIZE, STR_NETWORK_SERVER_LIST_MAP_SIZE_CAPTION, STR_NETWORK_SERVER_LIST_MAP_SIZE_CAPTION_TOOLTIP));
		this->Add(std::make_unique<NWidgetLeaf>(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_DATE, STR_NETWORK_SERVER_LIST_DATE_CAPTION, STR_NETWORK_SERVER_LIST_DATE_CAPTION_TOOLTIP));
		this->Add(std::make_unique<NWidgetLeaf>(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_YEARS, STR_NETWORK_SERVER_LIST_YEARS_CAPTION, STR_NETWORK_SERVER_LIST_YEARS_CAPTION_TOOLTIP));

		leaf = std::make_unique<NWidgetLeaf>(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_INFO, STR_EMPTY, STR_NETWORK_SERVER_LIST_INFO_ICONS_TOOLTIP);
		leaf->SetMinimalSize(14 + GetSpriteSize(SPR_LOCK, nullptr, ZOOM_LVL_OUT_4X).width
		                        + GetSpriteSize(SPR_BLOT, nullptr, ZOOM_LVL_OUT_4X).width, 12);
		leaf->SetFill(0, 1);
		this->Add(std::move(leaf));
	}

	void SetupSmallestSize(Window *w) override
	{
		this->smallest_y = 0; // Biggest child.
		this->fill_x = 1;
		this->fill_y = 0;
		this->resize_x = 1; // We only resize in this direction
		this->resize_y = 0; // We never resize in this direction

		/* First initialise some variables... */
		for (const auto &child_wid : this->children) {
			child_wid->SetupSmallestSize(w);
			this->smallest_y = std::max(this->smallest_y, child_wid->smallest_y + child_wid->padding.Vertical());
		}

		/* ... then in a second pass make sure the 'current' sizes are set. Won't change for most widgets. */
		for (const auto &child_wid : this->children) {
			child_wid->current_x = child_wid->smallest_x;
			child_wid->current_y = this->smallest_y;
		}

		this->smallest_x = this->children.front()->smallest_x + this->children.back()->smallest_x; // First and last are always shown, rest not
	}

	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override
	{
		assert(given_width >= this->smallest_x && given_height >= this->smallest_y);

		this->pos_x = x;
		this->pos_y = y;
		this->current_x = given_width;
		this->current_y = given_height;

		given_width -= this->children.back()->smallest_x;
		/* The first and last widget are always visible, determine which other should be visible */
		if (this->children.size() > 2) {
			auto first = std::next(std::begin(this->children));
			auto last = std::prev(std::end(this->children));
			for (auto it = first; it != last; ++it) {
				auto &child_wid = *it;
				if (given_width > ScaleGUITrad(MINIMUM_NAME_WIDTH_BEFORE_NEW_HEADER) + child_wid->smallest_x && (*std::prev(it))->current_x != 0) {
					given_width -= child_wid->smallest_x;
					child_wid->current_x = child_wid->smallest_x; /* Make visible. */
				} else {
					child_wid->current_x = 0; /* Make invisible. */
				}
			}
		}

		/* All remaining space goes to the first (name) widget */
		this->children.front()->current_x = given_width;

		/* Now assign the widgets to their rightful place */
		uint position = 0; // Place to put next child relative to origin of the container.
		auto assign_position = [&](const std::unique_ptr<NWidgetBase> &child_wid) {
			if (child_wid->current_x != 0) {
				child_wid->AssignSizePosition(sizing, x + position, y, child_wid->current_x, this->current_y, rtl);
				position += child_wid->current_x;
			}
		};

		if (rtl) {
			std::for_each(std::rbegin(this->children), std::rend(this->children), assign_position);
		} else {
			std::for_each(std::begin(this->children), std::end(this->children), assign_position);
		}
	}
};

class NetworkGameWindow : public Window {
protected:
	/* Runtime saved values */
	static Listing last_sorting;

	/* Constants for sorting servers */
	static GUIGameServerList::SortFunction * const sorter_funcs[];
	static GUIGameServerList::FilterFunction * const filter_funcs[];

	NetworkGameList *server;        ///< Selected server.
	NetworkGameList *last_joined;   ///< The last joined server.
	GUIGameServerList servers;      ///< List with game servers.
	ServerListPosition list_pos;    ///< Position of the selected server.
	Scrollbar *vscroll;             ///< Vertical scrollbar of the list of servers.
	QueryString name_editbox;       ///< Client name editbox.
	QueryString filter_editbox;     ///< Editbox for filter on servers.
	bool searched_internet = false; ///< Did we ever press "Search Internet" button?

	int lock_offset; ///< Left offset for lock icon.
	int blot_offset; ///< Left offset for green/yellow/red compatibility icon.
	int flag_offset; ///< Left offset for language flag icon.

	/**
	 * (Re)build the GUI network game list (a.k.a. this->servers) as some
	 * major change has occurred. It ensures appropriate filtering and
	 * sorting, if both or either one is enabled.
	 */
	void BuildGUINetworkGameList()
	{
		if (!this->servers.NeedRebuild()) return;

		/* Create temporary array of games to use for listing */
		this->servers.clear();

		bool found_current_server = false;
		bool found_last_joined = false;
		for (NetworkGameList *ngl = _network_game_list; ngl != nullptr; ngl = ngl->next) {
			this->servers.push_back(ngl);
			if (ngl == this->server) {
				found_current_server = true;
			}
			if (ngl == this->last_joined) {
				found_last_joined = true;
			}
		}
		/* A refresh can cause the current server to be delete; so unselect. */
		if (!found_last_joined) {
			this->last_joined = nullptr;
		}
		if (!found_current_server) {
			this->server = nullptr;
			this->list_pos = SLP_INVALID;
		}

		/* Apply the filter condition immediately, if a search string has been provided. */
		StringFilter sf;
		sf.SetFilterTerm(this->filter_editbox.text.buf);

		if (!sf.IsEmpty()) {
			this->servers.SetFilterState(true);
			this->servers.Filter(sf);
		} else {
			this->servers.SetFilterState(false);
		}

		this->servers.shrink_to_fit();
		this->servers.RebuildDone();
		this->vscroll->SetCount(this->servers.size());

		/* Sort the list of network games as requested. */
		this->servers.Sort();
		this->UpdateListPos();
	}

	/** Sort servers by name. */
	static bool NGameNameSorter(NetworkGameList * const &a, NetworkGameList * const &b)
	{
		int r = StrNaturalCompare(a->info.server_name, b->info.server_name, true); // Sort by name (natural sorting).
		if (r == 0) r = a->connection_string.compare(b->connection_string);

		return r < 0;
	}

	/**
	 * Sort servers by the amount of clients online on a
	 * server. If the two servers have the same amount, the one with the
	 * higher maximum is preferred.
	 */
	static bool NGameClientSorter(NetworkGameList * const &a, NetworkGameList * const &b)
	{
		/* Reverse as per default we are interested in most-clients first */
		int r = a->info.clients_on - b->info.clients_on;

		if (r == 0) r = a->info.clients_max - b->info.clients_max;
		if (r == 0) return NGameNameSorter(a, b);

		return r < 0;
	}

	/** Sort servers by map size */
	static bool NGameMapSizeSorter(NetworkGameList * const &a, NetworkGameList * const &b)
	{
		/* Sort by the area of the map. */
		int r = (a->info.map_height) * (a->info.map_width) - (b->info.map_height) * (b->info.map_width);

		if (r == 0) r = a->info.map_width - b->info.map_width;
		return (r != 0) ? r < 0 : NGameClientSorter(a, b);
	}

	/** Sort servers by current date */
	static bool NGameDateSorter(NetworkGameList * const &a, NetworkGameList * const &b)
	{
		auto r = a->info.game_date - b->info.game_date;
		return (r != 0) ? r < 0 : NGameClientSorter(a, b);
	}

	/** Sort servers by the number of days the game is running */
	static bool NGameYearsSorter(NetworkGameList * const &a, NetworkGameList * const &b)
	{
		auto r = a->info.game_date - a->info.start_date - b->info.game_date + b->info.start_date;
		return (r != 0) ? r < 0: NGameDateSorter(a, b);
	}

	/**
	 * Sort servers by joinability. If both servers are the
	 * same, prefer the non-passworded server first.
	 */
	static bool NGameAllowedSorter(NetworkGameList * const &a, NetworkGameList * const &b)
	{
		/* The servers we do not know anything about (the ones that did not reply) should be at the bottom) */
		int r = a->info.server_revision.empty() - b->info.server_revision.empty();

		/* Reverse default as we are interested in version-compatible clients first */
		if (r == 0) r = b->info.version_compatible - a->info.version_compatible;
		/* The version-compatible ones are then sorted with NewGRF compatible first, incompatible last */
		if (r == 0) r = b->info.compatible - a->info.compatible;
		/* Passworded servers should be below unpassworded servers */
		if (r == 0) r = a->info.use_password - b->info.use_password;

		/* Finally sort on the number of clients of the server in reverse order. */
		return (r != 0) ? r < 0 : NGameClientSorter(b, a);
	}

	/** Sort the server list */
	void SortNetworkGameList()
	{
		if (this->servers.Sort()) this->UpdateListPos();
	}

	/** Set this->list_pos to match this->server */
	void UpdateListPos()
	{
		this->list_pos = SLP_INVALID;
		for (uint i = 0; i != this->servers.size(); i++) {
			if (this->servers[i] == this->server) {
				this->list_pos = i;
				break;
			}
		}
	}

	static bool CDECL NGameSearchFilter(NetworkGameList * const *item, StringFilter &sf)
	{
		assert(item != nullptr);
		assert((*item) != nullptr);

		sf.ResetState();
		sf.AddLine((*item)->info.server_name);
		return sf.GetState();
	}

	/**
	 * Draw a single server line.
	 * @param cur_item  the server to draw.
	 * @param y         from where to draw?
	 * @param highlight does the line need to be highlighted?
	 */
	void DrawServerLine(const NetworkGameList *cur_item, int y, bool highlight) const
	{
		Rect name = this->GetWidget<NWidgetBase>(WID_NG_NAME)->GetCurrentRect();
		Rect info = this->GetWidget<NWidgetBase>(WID_NG_INFO)->GetCurrentRect();

		/* show highlighted item with a different colour */
		if (highlight) {
			Rect r = {std::min(name.left, info.left), y, std::max(name.right, info.right), y + (int)this->resize.step_height - 1};
			GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), PC_GREY);
		}

		/* offsets to vertically centre text and icons */
		int text_y_offset = (this->resize.step_height - GetCharacterHeight(FS_NORMAL)) / 2 + 1;
		int icon_y_offset = (this->resize.step_height - GetSpriteSize(SPR_BLOT).height) / 2;
		int lock_y_offset = (this->resize.step_height - GetSpriteSize(SPR_LOCK).height) / 2;

		name = name.Shrink(WidgetDimensions::scaled.framerect);
		DrawString(name.left, name.right, y + text_y_offset, cur_item->info.server_name, TC_BLACK);

		/* only draw details if the server is online */
		if (cur_item->status == NGLS_ONLINE) {
			if (const NWidgetBase *nwid = this->GetWidget<NWidgetBase>(WID_NG_CLIENTS); nwid->current_x != 0) {
				Rect clients = nwid->GetCurrentRect();
				SetDParam(0, cur_item->info.clients_on);
				SetDParam(1, cur_item->info.clients_max);
				SetDParam(2, cur_item->info.companies_on);
				SetDParam(3, cur_item->info.companies_max);
				DrawString(clients.left, clients.right, y + text_y_offset, STR_NETWORK_SERVER_LIST_GENERAL_ONLINE, TC_FROMSTRING, SA_HOR_CENTER);
			}

			if (const NWidgetBase *nwid = this->GetWidget<NWidgetBase>(WID_NG_MAPSIZE); nwid->current_x != 0) {
				/* map size */
				Rect mapsize = nwid->GetCurrentRect();
				SetDParam(0, cur_item->info.map_width);
				SetDParam(1, cur_item->info.map_height);
				DrawString(mapsize.left, mapsize.right, y + text_y_offset, STR_NETWORK_SERVER_LIST_MAP_SIZE_SHORT, TC_FROMSTRING, SA_HOR_CENTER);
			}

			if (const NWidgetBase *nwid = this->GetWidget<NWidgetBase>(WID_NG_DATE); nwid->current_x != 0) {
				/* current date */
				Rect date = nwid->GetCurrentRect();
				TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(cur_item->info.game_date);
				SetDParam(0, ymd.year);
				DrawString(date.left, date.right, y + text_y_offset, STR_JUST_INT, TC_BLACK, SA_HOR_CENTER);
			}

			if (const NWidgetBase *nwid = this->GetWidget<NWidgetBase>(WID_NG_YEARS); nwid->current_x != 0) {
				/* number of years the game is running */
				Rect years = nwid->GetCurrentRect();
				TimerGameCalendar::YearMonthDay ymd_cur = TimerGameCalendar::ConvertDateToYMD(cur_item->info.game_date);
				TimerGameCalendar::YearMonthDay ymd_start = TimerGameCalendar::ConvertDateToYMD(cur_item->info.start_date);
				SetDParam(0, ymd_cur.year - ymd_start.year);
				DrawString(years.left, years.right, y + text_y_offset, STR_JUST_INT, TC_BLACK, SA_HOR_CENTER);
			}

			/* draw a lock if the server is password protected */
			if (cur_item->info.use_password) DrawSprite(SPR_LOCK, PAL_NONE, info.left + this->lock_offset, y + lock_y_offset);

			/* draw red or green icon, depending on compatibility with server */
			DrawSprite(SPR_BLOT, (cur_item->info.compatible ? PALETTE_TO_GREEN : (cur_item->info.version_compatible ? PALETTE_TO_YELLOW : PALETTE_TO_RED)), info.left + this->blot_offset, y + icon_y_offset + 1);
		}
	}

	/**
	 * Scroll the list up or down to the currently selected server.
	 * If the server is below the currently displayed servers, it will
	 * scroll down an amount so that the server appears at the bottom.
	 * If the server is above the currently displayed servers, it will
	 * scroll up so that the server appears at the top.
	 */
	void ScrollToSelectedServer()
	{
		if (this->list_pos == SLP_INVALID) return; // no server selected
		this->vscroll->ScrollTowards(this->list_pos);
	}

public:
	NetworkGameWindow(WindowDesc *desc) : Window(desc), name_editbox(NETWORK_CLIENT_NAME_LENGTH), filter_editbox(120)
	{
		this->list_pos = SLP_INVALID;
		this->server = nullptr;

		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_NG_SCROLLBAR);
		this->FinishInitNested(WN_NETWORK_WINDOW_GAME);

		this->querystrings[WID_NG_CLIENT] = &this->name_editbox;
		this->name_editbox.text.Assign(_settings_client.network.client_name);

		this->querystrings[WID_NG_FILTER] = &this->filter_editbox;
		this->filter_editbox.cancel_button = QueryString::ACTION_CLEAR;
		this->SetFocusedWidget(WID_NG_FILTER);

		/* As the Game Coordinator doesn't support "websocket" servers yet, we
		 * let "os/emscripten/pre.js" hardcode a list of servers people can
		 * join. This means the serverlist is curated for now, but it is the
		 * best we can offer. */
#ifdef __EMSCRIPTEN__
		EM_ASM(if (window["openttd_server_list"]) openttd_server_list());
#endif

		this->last_joined = NetworkAddServer(_settings_client.network.last_joined, false);
		this->server = this->last_joined;

		this->servers.SetListing(this->last_sorting);
		this->servers.SetSortFuncs(this->sorter_funcs);
		this->servers.SetFilterFuncs(this->filter_funcs);
		this->servers.ForceRebuild();
	}

	~NetworkGameWindow()
	{
		this->last_sorting = this->servers.GetListing();
	}

	void OnInit() override
	{
		this->lock_offset = ScaleGUITrad(5);
		this->blot_offset = this->lock_offset + ScaleGUITrad(3) + GetSpriteSize(SPR_LOCK).width;
		this->flag_offset = this->blot_offset + ScaleGUITrad(2) + GetSpriteSize(SPR_BLOT).width;
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_NG_MATRIX:
				resize->height = std::max(GetSpriteSize(SPR_BLOT).height, (uint)GetCharacterHeight(FS_NORMAL)) + padding.height;
				fill->height = resize->height;
				size->height = 12 * resize->height;
				break;

			case WID_NG_LASTJOINED:
				size->height = std::max(GetSpriteSize(SPR_BLOT).height, (uint)GetCharacterHeight(FS_NORMAL)) + WidgetDimensions::scaled.matrix.Vertical();
				break;

			case WID_NG_LASTJOINED_SPACER:
				size->width = NWidgetScrollbar::GetVerticalDimension().width;
				break;

			case WID_NG_NAME:
				size->width += 2 * Window::SortButtonWidth(); // Make space for the arrow
				break;

			case WID_NG_CLIENTS:
				size->width += 2 * Window::SortButtonWidth(); // Make space for the arrow
				SetDParamMaxValue(0, MAX_CLIENTS);
				SetDParamMaxValue(1, MAX_CLIENTS);
				SetDParamMaxValue(2, MAX_COMPANIES);
				SetDParamMaxValue(3, MAX_COMPANIES);
				*size = maxdim(*size, GetStringBoundingBox(STR_NETWORK_SERVER_LIST_GENERAL_ONLINE));
				break;

			case WID_NG_MAPSIZE:
				size->width += 2 * Window::SortButtonWidth(); // Make space for the arrow
				SetDParamMaxValue(0, MAX_MAP_SIZE);
				SetDParamMaxValue(1, MAX_MAP_SIZE);
				*size = maxdim(*size, GetStringBoundingBox(STR_NETWORK_SERVER_LIST_MAP_SIZE_SHORT));
				break;

			case WID_NG_DATE:
			case WID_NG_YEARS:
				size->width += 2 * Window::SortButtonWidth(); // Make space for the arrow
				SetDParamMaxValue(0, 5);
				*size = maxdim(*size, GetStringBoundingBox(STR_JUST_INT));
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_NG_MATRIX: {
				uint16_t y = r.top;

				const int max = std::min(this->vscroll->GetPosition() + this->vscroll->GetCapacity(), (int)this->servers.size());

				for (int i = this->vscroll->GetPosition(); i < max; ++i) {
					const NetworkGameList *ngl = this->servers[i];
					this->DrawServerLine(ngl, y, ngl == this->server);
					y += this->resize.step_height;
				}
				break;
			}

			case WID_NG_LASTJOINED:
				/* Draw the last joined server, if any */
				if (this->last_joined != nullptr) this->DrawServerLine(this->last_joined, r.top, this->last_joined == this->server);
				break;

			case WID_NG_DETAILS:
				this->DrawDetails(r);
				break;

			case WID_NG_NAME:
			case WID_NG_CLIENTS:
			case WID_NG_MAPSIZE:
			case WID_NG_DATE:
			case WID_NG_YEARS:
			case WID_NG_INFO:
				if (widget - WID_NG_NAME == this->servers.SortType()) this->DrawSortButtonState(widget, this->servers.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;
		}
	}


	void OnPaint() override
	{
		if (this->servers.NeedRebuild()) {
			this->BuildGUINetworkGameList();
		}
		if (this->servers.NeedResort()) {
			this->SortNetworkGameList();
		}

		NetworkGameList *sel = this->server;
		/* 'Refresh' button invisible if no server selected */
		this->SetWidgetDisabledState(WID_NG_REFRESH, sel == nullptr);
		/* 'Join' button disabling conditions */
		this->SetWidgetDisabledState(WID_NG_JOIN, sel == nullptr || // no Selected Server
				sel->status != NGLS_ONLINE || // Server offline
				sel->info.clients_on >= sel->info.clients_max || // Server full
				!sel->info.compatible); // Revision mismatch

		this->SetWidgetLoweredState(WID_NG_REFRESH, sel != nullptr && sel->refreshing);

		/* 'NewGRF Settings' button invisible if no NewGRF is used */
		bool changed = false;
		changed |= this->GetWidget<NWidgetStacked>(WID_NG_NEWGRF_SEL)->SetDisplayedPlane(sel == nullptr || sel->status != NGLS_ONLINE || sel->info.grfconfig == nullptr ? SZSP_NONE : 0);
		changed |= this->GetWidget<NWidgetStacked>(WID_NG_NEWGRF_MISSING_SEL)->SetDisplayedPlane(sel == nullptr || sel->status != NGLS_ONLINE || sel->info.grfconfig == nullptr || !sel->info.version_compatible || sel->info.compatible ? SZSP_NONE : 0);
		if (changed) {
			this->ReInit();
			return;
		}

#ifdef __EMSCRIPTEN__
		this->SetWidgetDisabledState(WID_NG_SEARCH_INTERNET, true);
		this->SetWidgetDisabledState(WID_NG_SEARCH_LAN, true);
		this->SetWidgetDisabledState(WID_NG_ADD, true);
		this->SetWidgetDisabledState(WID_NG_START, true);
#endif

		this->DrawWidgets();
	}

	StringID GetHeaderString() const
	{
		if (this->server == nullptr) return STR_NETWORK_SERVER_LIST_GAME_INFO;
		switch (this->server->status) {
			case NGLS_OFFLINE: return STR_NETWORK_SERVER_LIST_SERVER_OFFLINE;
			case NGLS_ONLINE: return STR_NETWORK_SERVER_LIST_GAME_INFO;
			case NGLS_FULL: return STR_NETWORK_SERVER_LIST_SERVER_FULL;
			case NGLS_BANNED: return STR_NETWORK_SERVER_LIST_SERVER_BANNED;
			case NGLS_TOO_OLD: return STR_NETWORK_SERVER_LIST_SERVER_TOO_OLD;
			default: NOT_REACHED();
		}
	}

	void DrawDetails(const Rect &r) const
	{
		NetworkGameList *sel = this->server;

		Rect tr = r.Shrink(WidgetDimensions::scaled.frametext);
		StringID header_msg = this->GetHeaderString();
		int header_height = GetStringHeight(header_msg, tr.Width()) +
				(sel == nullptr ? 0 : GetStringHeight(sel->info.server_name, tr.Width())) +
				WidgetDimensions::scaled.frametext.Vertical();

		/* Height for the title banner */
		Rect hr = r.WithHeight(header_height).Shrink(WidgetDimensions::scaled.frametext);
		tr.top += header_height;

		/* Draw the right menu */
		/* Create the nice grayish rectangle at the details top */
		GfxFillRect(r.WithHeight(header_height).Shrink(WidgetDimensions::scaled.bevel), PC_DARK_BLUE);
		hr.top = DrawStringMultiLine(hr, header_msg, TC_FROMSTRING, SA_HOR_CENTER);
		if (sel == nullptr) return;

		hr.top = DrawStringMultiLine(hr, sel->info.server_name, TC_ORANGE, SA_HOR_CENTER); // game name
		if (sel->status != NGLS_ONLINE) {
			tr.top = DrawStringMultiLine(tr, header_msg, TC_FROMSTRING, SA_HOR_CENTER);
		} else { // show game info
			SetDParam(0, sel->info.clients_on);
			SetDParam(1, sel->info.clients_max);
			SetDParam(2, sel->info.companies_on);
			SetDParam(3, sel->info.companies_max);
			tr.top = DrawStringMultiLine(tr, STR_NETWORK_SERVER_LIST_CLIENTS);

			SetDParam(0, STR_CLIMATE_TEMPERATE_LANDSCAPE + sel->info.landscape);
			tr.top = DrawStringMultiLine(tr, STR_NETWORK_SERVER_LIST_LANDSCAPE); // landscape

			SetDParam(0, sel->info.map_width);
			SetDParam(1, sel->info.map_height);
			tr.top = DrawStringMultiLine(tr, STR_NETWORK_SERVER_LIST_MAP_SIZE); // map size

			SetDParamStr(0, sel->info.server_revision);
			tr.top = DrawStringMultiLine(tr, STR_NETWORK_SERVER_LIST_SERVER_VERSION); // server version

			SetDParamStr(0, sel->connection_string);
			StringID invite_or_address = StrStartsWith(sel->connection_string, "+") ? STR_NETWORK_SERVER_LIST_INVITE_CODE : STR_NETWORK_SERVER_LIST_SERVER_ADDRESS;
			tr.top = DrawStringMultiLine(tr, invite_or_address); // server address / invite code

			SetDParam(0, sel->info.start_date);
			tr.top = DrawStringMultiLine(tr, STR_NETWORK_SERVER_LIST_START_DATE); // start date

			SetDParam(0, sel->info.game_date);
			tr.top = DrawStringMultiLine(tr, STR_NETWORK_SERVER_LIST_CURRENT_DATE); // current date

			if (sel->info.gamescript_version != -1) {
				SetDParamStr(0, sel->info.gamescript_name);
				SetDParam(1, sel->info.gamescript_version);
				tr.top = DrawStringMultiLine(tr, STR_NETWORK_SERVER_LIST_GAMESCRIPT); // gamescript name and version
			}

			tr.top += WidgetDimensions::scaled.vsep_wide;

			if (!sel->info.compatible) {
				DrawStringMultiLine(tr, sel->info.version_compatible ? STR_NETWORK_SERVER_LIST_GRF_MISMATCH : STR_NETWORK_SERVER_LIST_VERSION_MISMATCH, TC_FROMSTRING, SA_HOR_CENTER); // server mismatch
			} else if (sel->info.clients_on == sel->info.clients_max) {
				/* Show: server full, when clients_on == max_clients */
				DrawStringMultiLine(tr, STR_NETWORK_SERVER_LIST_SERVER_FULL, TC_FROMSTRING, SA_HOR_CENTER); // server full
			} else if (sel->info.use_password) {
				DrawStringMultiLine(tr, STR_NETWORK_SERVER_LIST_PASSWORD, TC_FROMSTRING, SA_HOR_CENTER); // password warning
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_NG_CANCEL: // Cancel button
				CloseWindowById(WC_NETWORK_WINDOW, WN_NETWORK_WINDOW_GAME);
				break;

			case WID_NG_NAME:    // Sort by name
			case WID_NG_CLIENTS: // Sort by connected clients
			case WID_NG_MAPSIZE: // Sort by map size
			case WID_NG_DATE:    // Sort by date
			case WID_NG_YEARS:   // Sort by years
			case WID_NG_INFO:    // Connectivity (green dot)
				if (this->servers.SortType() == widget - WID_NG_NAME) {
					this->servers.ToggleSortOrder();
					if (this->list_pos != SLP_INVALID) this->list_pos = (ServerListPosition)this->servers.size() - this->list_pos - 1;
				} else {
					this->servers.SetSortType(widget - WID_NG_NAME);
					this->servers.ForceResort();
					this->SortNetworkGameList();
				}
				this->ScrollToSelectedServer();
				this->SetDirty();
				break;

			case WID_NG_MATRIX: { // Show available network games
				auto it = this->vscroll->GetScrolledItemFromWidget(this->servers, pt.y, this, WID_NG_MATRIX);
				this->server = (it != this->servers.end()) ? *it : nullptr;
				this->list_pos = (server == nullptr) ? SLP_INVALID : it - this->servers.begin();
				this->SetDirty();

				/* FIXME the disabling should go into some InvalidateData, which is called instead of the SetDirty */
				if (click_count > 1 && !this->IsWidgetDisabled(WID_NG_JOIN)) this->OnClick(pt, WID_NG_JOIN, 1);
				break;
			}

			case WID_NG_LASTJOINED: {
				if (this->last_joined != nullptr) {
					this->server = this->last_joined;

					/* search the position of the newly selected server */
					this->UpdateListPos();
					this->ScrollToSelectedServer();
					this->SetDirty();

					/* FIXME the disabling should go into some InvalidateData, which is called instead of the SetDirty */
					if (click_count > 1 && !this->IsWidgetDisabled(WID_NG_JOIN)) this->OnClick(pt, WID_NG_JOIN, 1);
				}
				break;
			}

			case WID_NG_SEARCH_INTERNET:
				_network_coordinator_client.GetListing();
				this->searched_internet = true;
				break;

			case WID_NG_SEARCH_LAN:
				NetworkUDPSearchGame();
				break;

			case WID_NG_ADD: // Add a server
				SetDParamStr(0, _settings_client.network.connect_to_ip);
				ShowQueryString(
					STR_JUST_RAW_STRING,
					STR_NETWORK_SERVER_LIST_ENTER_SERVER_ADDRESS,
					NETWORK_HOSTNAME_PORT_LENGTH,  // maximum number of characters including '\0'
					this, CS_ALPHANUMERAL, QSF_ACCEPT_UNCHANGED);
				break;

			case WID_NG_START: // Start server
				ShowNetworkStartServerWindow();
				break;

			case WID_NG_JOIN: // Join Game
				if (this->server != nullptr) {
					NetworkClientConnectGame(this->server->connection_string, COMPANY_SPECTATOR);
				}
				break;

			case WID_NG_REFRESH: // Refresh
				if (this->server != nullptr && !this->server->refreshing) NetworkQueryServer(this->server->connection_string);
				break;

			case WID_NG_NEWGRF: // NewGRF Settings
				if (this->server != nullptr) ShowNewGRFSettings(false, false, false, &this->server->info.grfconfig);
				break;

			case WID_NG_NEWGRF_MISSING: // Find missing content online
				if (this->server != nullptr) ShowMissingContentWindow(this->server->info.grfconfig);
				break;
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		this->servers.ForceRebuild();
		this->SetDirty();
	}

	EventState OnKeyPress([[maybe_unused]] char32_t key, uint16_t keycode) override
	{
		EventState state = ES_NOT_HANDLED;

		/* handle up, down, pageup, pagedown, home and end */
		if (this->vscroll->UpdateListPositionOnKeyPress(this->list_pos, keycode) == ES_HANDLED) {
			if (this->list_pos == SLP_INVALID) return ES_HANDLED;

			this->server = this->servers[this->list_pos];

			/* Scroll to the new server if it is outside the current range. */
			this->ScrollToSelectedServer();

			/* redraw window */
			this->SetDirty();
			return ES_HANDLED;
		}

		if (this->server != nullptr) {
			if (keycode == WKC_DELETE) { // Press 'delete' to remove servers
				NetworkGameListRemoveItem(this->server);
				if (this->server == this->last_joined) this->last_joined = nullptr;
				this->server = nullptr;
				this->list_pos = SLP_INVALID;
			}
		}

		return state;
	}

	void OnEditboxChanged(WidgetID wid) override
	{
		switch (wid) {
			case WID_NG_FILTER: {
				this->servers.ForceRebuild();
				this->BuildGUINetworkGameList();
				this->ScrollToSelectedServer();
				this->SetDirty();
				break;
			}

			case WID_NG_CLIENT:
				/* Validation of the name will happen once the user tries to join or start a game, as getting
				 * error messages while typing (e.g. when you clear the name) defeats the purpose of the check. */
				_settings_client.network.client_name = this->name_editbox.text.buf;
				break;
		}
	}

	void OnQueryTextFinished(char *str) override
	{
		if (!StrEmpty(str)) {
			_settings_client.network.connect_to_ip = str;
			NetworkAddServer(str);
			NetworkRebuildHostList();
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_NG_MATRIX);
	}

	/** Refresh the online servers on a regular interval. */
	IntervalTimer<TimerWindow> refresh_interval = {std::chrono::seconds(30), [this](uint) {
		if (!this->searched_internet) return;

		_network_coordinator_client.GetListing();
	}};
};

Listing NetworkGameWindow::last_sorting = {false, 5};
GUIGameServerList::SortFunction * const NetworkGameWindow::sorter_funcs[] = {
	&NGameNameSorter,
	&NGameClientSorter,
	&NGameMapSizeSorter,
	&NGameDateSorter,
	&NGameYearsSorter,
	&NGameAllowedSorter
};

GUIGameServerList::FilterFunction * const NetworkGameWindow::filter_funcs[] = {
	&NGameSearchFilter
};

static std::unique_ptr<NWidgetBase> MakeResizableHeader()
{
	return std::make_unique<NWidgetServerListHeader>();
}

static const NWidgetPart _nested_network_game_widgets[] = {
	/* TOP */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE), SetDataTip(STR_NETWORK_SERVER_LIST_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_LIGHT_BLUE),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, WID_NG_MAIN),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse_resize),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				/* LEFT SIDE */
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0),
					NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
						NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, WID_NG_FILTER_LABEL), SetDataTip(STR_LIST_FILTER_TITLE, STR_NULL),
						NWidget(WWT_EDITBOX, COLOUR_LIGHT_BLUE, WID_NG_FILTER), SetMinimalSize(251, 12), SetFill(1, 0), SetResize(1, 0),
											SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
					EndContainer(),
					NWidget(NWID_HORIZONTAL),
						NWidget(NWID_VERTICAL),
							NWidgetFunction(MakeResizableHeader),
							NWidget(WWT_MATRIX, COLOUR_LIGHT_BLUE, WID_NG_MATRIX), SetResize(1, 1), SetFill(1, 0),
												SetMatrixDataTip(1, 0, STR_NETWORK_SERVER_LIST_CLICK_GAME_TO_SELECT), SetScrollbar(WID_NG_SCROLLBAR),
						EndContainer(),
						NWidget(NWID_VSCROLLBAR, COLOUR_LIGHT_BLUE, WID_NG_SCROLLBAR),
					EndContainer(),
					NWidget(NWID_VERTICAL),
						NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, WID_NG_LASTJOINED_LABEL), SetFill(1, 0),
											SetDataTip(STR_NETWORK_SERVER_LIST_LAST_JOINED_SERVER, STR_NULL), SetResize(1, 0),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, WID_NG_LASTJOINED), SetFill(1, 0), SetResize(1, 0),
												SetDataTip(0x0, STR_NETWORK_SERVER_LIST_CLICK_TO_SELECT_LAST),
							EndContainer(),
							NWidget(WWT_EMPTY, INVALID_COLOUR, WID_NG_LASTJOINED_SPACER), SetFill(0, 0),
						EndContainer(),
					EndContainer(),
				EndContainer(),
				/* RIGHT SIDE */
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0),
					NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
						NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, WID_NG_CLIENT_LABEL), SetDataTip(STR_NETWORK_SERVER_LIST_PLAYER_NAME, STR_NULL),
						NWidget(WWT_EDITBOX, COLOUR_LIGHT_BLUE, WID_NG_CLIENT), SetMinimalSize(151, 12), SetFill(1, 0), SetResize(1, 0),
											SetDataTip(STR_NETWORK_SERVER_LIST_PLAYER_NAME_OSKTITLE, STR_NETWORK_SERVER_LIST_ENTER_NAME_TOOLTIP),
					EndContainer(),
					NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
						NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, WID_NG_DETAILS), SetMinimalSize(140, 0), SetMinimalTextLines(15, 0), SetResize(0, 1),
						EndContainer(),
						NWidget(NWID_VERTICAL, NC_EQUALSIZE),
							NWidget(NWID_SELECTION, INVALID_COLOUR, WID_NG_NEWGRF_MISSING_SEL),
								NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_NEWGRF_MISSING), SetFill(1, 0), SetDataTip(STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_BUTTON, STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_TOOLTIP),
							EndContainer(),
							NWidget(NWID_SELECTION, INVALID_COLOUR, WID_NG_NEWGRF_SEL),
								NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_NEWGRF), SetFill(1, 0), SetDataTip(STR_INTRO_NEWGRF_SETTINGS, STR_NULL),
							EndContainer(),
							NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
								NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_JOIN), SetFill(1, 0), SetDataTip(STR_NETWORK_SERVER_LIST_JOIN_GAME, STR_NULL),
								NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_REFRESH), SetFill(1, 0), SetDataTip(STR_NETWORK_SERVER_LIST_REFRESH, STR_NETWORK_SERVER_LIST_REFRESH_TOOLTIP),
							EndContainer(),
						EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),
			/* BOTTOM */
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_SEARCH_INTERNET), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_NETWORK_SERVER_LIST_SEARCH_SERVER_INTERNET, STR_NETWORK_SERVER_LIST_SEARCH_SERVER_INTERNET_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_SEARCH_LAN), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_NETWORK_SERVER_LIST_SEARCH_SERVER_LAN, STR_NETWORK_SERVER_LIST_SEARCH_SERVER_LAN_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_ADD), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_NETWORK_SERVER_LIST_ADD_SERVER, STR_NETWORK_SERVER_LIST_ADD_SERVER_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_START), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_NETWORK_SERVER_LIST_START_SERVER, STR_NETWORK_SERVER_LIST_START_SERVER_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_CANCEL), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_BUTTON_CANCEL, STR_NULL),
			EndContainer(),
		EndContainer(),
		/* Resize button. */
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_RESIZEBOX, COLOUR_LIGHT_BLUE), SetDataTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _network_game_window_desc(__FILE__, __LINE__,
	WDP_CENTER, "list_servers", 1000, 730,
	WC_NETWORK_WINDOW, WC_NONE,
	0,
	std::begin(_nested_network_game_widgets), std::end(_nested_network_game_widgets)
);

void ShowNetworkGameWindow()
{
	static bool first = true;
	CloseWindowById(WC_NETWORK_WINDOW, WN_NETWORK_WINDOW_START);

	/* Only show once */
	if (first) {
		first = false;
		/* Add all servers from the config file to our list. */
		for (const auto &iter : _network_host_list) {
			NetworkAddServer(iter);
		}
	}

	new NetworkGameWindow(&_network_game_window_desc);
}

struct NetworkStartServerWindow : public Window {
	WidgetID widget_id;          ///< The widget that has the pop-up input menu
	QueryString name_editbox;    ///< Server name editbox.

	NetworkStartServerWindow(WindowDesc *desc) : Window(desc), name_editbox(NETWORK_NAME_LENGTH)
	{
		this->InitNested(WN_NETWORK_WINDOW_START);

		this->querystrings[WID_NSS_GAMENAME] = &this->name_editbox;
		this->name_editbox.text.Assign(_settings_client.network.server_name);

		this->SetFocusedWidget(WID_NSS_GAMENAME);
	}

	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case WID_NSS_CONNTYPE_BTN:
				SetDParam(0, STR_NETWORK_SERVER_VISIBILITY_LOCAL + _settings_client.network.server_game_type);
				break;

			case WID_NSS_CLIENTS_TXT:
				SetDParam(0, _settings_client.network.max_clients);
				break;

			case WID_NSS_COMPANIES_TXT:
				SetDParam(0, _settings_client.network.max_companies);
				break;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_NSS_CONNTYPE_BTN:
				*size = maxdim(maxdim(GetStringBoundingBox(STR_NETWORK_SERVER_VISIBILITY_LOCAL), GetStringBoundingBox(STR_NETWORK_SERVER_VISIBILITY_PUBLIC)), GetStringBoundingBox(STR_NETWORK_SERVER_VISIBILITY_INVITE_ONLY));
				size->width += padding.width;
				size->height += padding.height;
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_NSS_SETPWD:
				/* If password is set, draw red '*' next to 'Set password' button. */
				if (!_settings_client.network.server_password.empty()) DrawString(r.right + WidgetDimensions::scaled.framerect.left, this->width - WidgetDimensions::scaled.framerect.right, r.top, "*", TC_RED);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_NSS_CANCEL: // Cancel button
				ShowNetworkGameWindow();
				break;

			case WID_NSS_SETPWD: // Set password button
				this->widget_id = WID_NSS_SETPWD;
				SetDParamStr(0, _settings_client.network.server_password);
				ShowQueryString(STR_JUST_RAW_STRING, STR_NETWORK_START_SERVER_SET_PASSWORD, NETWORK_PASSWORD_LENGTH, this, CS_ALPHANUMERAL, QSF_NONE);
				break;

			case WID_NSS_CONNTYPE_BTN: // Connection type
				ShowDropDownList(this, BuildVisibilityDropDownList(), _settings_client.network.server_game_type, WID_NSS_CONNTYPE_BTN);
				break;

			case WID_NSS_CLIENTS_BTND:    case WID_NSS_CLIENTS_BTNU:    // Click on up/down button for number of clients
			case WID_NSS_COMPANIES_BTND:  case WID_NSS_COMPANIES_BTNU:  // Click on up/down button for number of companies
				/* Don't allow too fast scrolling. */
				if (!(this->flags & WF_TIMEOUT) || this->timeout_timer <= 1) {
					this->HandleButtonClick(widget);
					this->SetDirty();
					switch (widget) {
						default: NOT_REACHED();
						case WID_NSS_CLIENTS_BTND: case WID_NSS_CLIENTS_BTNU:
							_settings_client.network.max_clients    = Clamp(_settings_client.network.max_clients    + widget - WID_NSS_CLIENTS_TXT,    2, MAX_CLIENTS);
							break;
						case WID_NSS_COMPANIES_BTND: case WID_NSS_COMPANIES_BTNU:
							_settings_client.network.max_companies  = Clamp(_settings_client.network.max_companies  + widget - WID_NSS_COMPANIES_TXT,  1, MAX_COMPANIES);
							break;
					}
				}
				_left_button_clicked = false;
				break;

			case WID_NSS_CLIENTS_TXT:    // Click on number of clients
				this->widget_id = WID_NSS_CLIENTS_TXT;
				SetDParam(0, _settings_client.network.max_clients);
				ShowQueryString(STR_JUST_INT, STR_NETWORK_START_SERVER_NUMBER_OF_CLIENTS,    4, this, CS_NUMERAL, QSF_NONE);
				break;

			case WID_NSS_COMPANIES_TXT:  // Click on number of companies
				this->widget_id = WID_NSS_COMPANIES_TXT;
				SetDParam(0, _settings_client.network.max_companies);
				ShowQueryString(STR_JUST_INT, STR_NETWORK_START_SERVER_NUMBER_OF_COMPANIES,  3, this, CS_NUMERAL, QSF_NONE);
				break;

			case WID_NSS_GENERATE_GAME: // Start game
				if (!CheckServerName()) return;
				_is_network_server = true;
				if (_ctrl_pressed) {
					StartNewGameWithoutGUI(GENERATE_NEW_SEED);
				} else {
					ShowGenerateLandscape();
				}
				break;

			case WID_NSS_LOAD_GAME:
				if (!CheckServerName()) return;
				_is_network_server = true;
				ShowSaveLoadDialog(FT_SAVEGAME, SLO_LOAD);
				break;

			case WID_NSS_PLAY_SCENARIO:
				if (!CheckServerName()) return;
				_is_network_server = true;
				ShowSaveLoadDialog(FT_SCENARIO, SLO_LOAD);
				break;

			case WID_NSS_PLAY_HEIGHTMAP:
				if (!CheckServerName()) return;
				_is_network_server = true;
				ShowSaveLoadDialog(FT_HEIGHTMAP,SLO_LOAD);
				break;
		}
	}

	void OnDropdownSelect(WidgetID widget, int index) override
	{
		switch (widget) {
			case WID_NSS_CONNTYPE_BTN:
				_settings_client.network.server_game_type = (ServerGameType)index;
				break;
			default:
				NOT_REACHED();
		}

		this->SetDirty();
	}

	bool CheckServerName()
	{
		std::string str = this->name_editbox.text.buf;
		if (!NetworkValidateServerName(str)) return false;

		SetSettingValue(GetSettingFromName("network.server_name")->AsStringSetting(), str);
		return true;
	}

	void OnTimeout() override
	{
		this->RaiseWidgetsWhenLowered(WID_NSS_CLIENTS_BTND, WID_NSS_CLIENTS_BTNU, WID_NSS_COMPANIES_BTND, WID_NSS_COMPANIES_BTNU);
	}

	void OnQueryTextFinished(char *str) override
	{
		if (str == nullptr) return;

		if (this->widget_id == WID_NSS_SETPWD) {
			_settings_client.network.server_password = str;
		} else {
			int32_t value = atoi(str);
			this->SetWidgetDirty(this->widget_id);
			switch (this->widget_id) {
				default: NOT_REACHED();
				case WID_NSS_CLIENTS_TXT:    _settings_client.network.max_clients    = Clamp(value, 2, MAX_CLIENTS); break;
				case WID_NSS_COMPANIES_TXT:  _settings_client.network.max_companies  = Clamp(value, 1, MAX_COMPANIES); break;
			}
		}

		this->SetDirty();
	}
};

static const NWidgetPart _nested_network_start_server_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE), SetDataTip(STR_NETWORK_START_SERVER_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, WID_NSS_BACKGROUND),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
					/* Game name widgets */
					NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, WID_NSS_GAMENAME_LABEL), SetFill(1, 0), SetDataTip(STR_NETWORK_START_SERVER_NEW_GAME_NAME, STR_NULL),
					NWidget(WWT_EDITBOX, COLOUR_LIGHT_BLUE, WID_NSS_GAMENAME), SetMinimalSize(10, 12), SetFill(1, 0), SetDataTip(STR_NETWORK_START_SERVER_NEW_GAME_NAME_OSKTITLE, STR_NETWORK_START_SERVER_NEW_GAME_NAME_TOOLTIP),
				EndContainer(),

				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
						NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, WID_NSS_CONNTYPE_LABEL), SetFill(1, 0), SetDataTip(STR_NETWORK_START_SERVER_VISIBILITY_LABEL, STR_NULL),
						NWidget(WWT_DROPDOWN, COLOUR_LIGHT_BLUE, WID_NSS_CONNTYPE_BTN), SetFill(1, 0), SetDataTip(STR_JUST_STRING, STR_NETWORK_START_SERVER_VISIBILITY_TOOLTIP),
					EndContainer(),
					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
						NWidget(NWID_SPACER), SetFill(1, 1),
						NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NSS_SETPWD), SetFill(1, 0), SetDataTip(STR_NETWORK_START_SERVER_SET_PASSWORD, STR_NETWORK_START_SERVER_PASSWORD_TOOLTIP),
					EndContainer(),
				EndContainer(),

				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
						NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, WID_NSS_CLIENTS_LABEL), SetFill(1, 0), SetDataTip(STR_NETWORK_START_SERVER_NUMBER_OF_CLIENTS, STR_NULL),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_IMGBTN, COLOUR_LIGHT_BLUE, WID_NSS_CLIENTS_BTND), SetMinimalSize(12, 12), SetFill(0, 1), SetDataTip(SPR_ARROW_DOWN, STR_NETWORK_START_SERVER_NUMBER_OF_CLIENTS_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_NSS_CLIENTS_TXT), SetFill(1, 0), SetDataTip(STR_NETWORK_START_SERVER_CLIENTS_SELECT, STR_NETWORK_START_SERVER_NUMBER_OF_CLIENTS_TOOLTIP),
							NWidget(WWT_IMGBTN, COLOUR_LIGHT_BLUE, WID_NSS_CLIENTS_BTNU), SetMinimalSize(12, 12), SetFill(0, 1), SetDataTip(SPR_ARROW_UP, STR_NETWORK_START_SERVER_NUMBER_OF_CLIENTS_TOOLTIP),
						EndContainer(),
					EndContainer(),

					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
						NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, WID_NSS_COMPANIES_LABEL), SetFill(1, 0), SetDataTip(STR_NETWORK_START_SERVER_NUMBER_OF_COMPANIES, STR_NULL),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_IMGBTN, COLOUR_LIGHT_BLUE, WID_NSS_COMPANIES_BTND), SetMinimalSize(12, 12), SetFill(0, 1), SetDataTip(SPR_ARROW_DOWN, STR_NETWORK_START_SERVER_NUMBER_OF_COMPANIES_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_NSS_COMPANIES_TXT), SetFill(1, 0), SetDataTip(STR_NETWORK_START_SERVER_COMPANIES_SELECT, STR_NETWORK_START_SERVER_NUMBER_OF_COMPANIES_TOOLTIP),
							NWidget(WWT_IMGBTN, COLOUR_LIGHT_BLUE, WID_NSS_COMPANIES_BTNU), SetMinimalSize(12, 12), SetFill(0, 1), SetDataTip(SPR_ARROW_UP, STR_NETWORK_START_SERVER_NUMBER_OF_COMPANIES_TOOLTIP),
						EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),

			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
				/* 'generate game' and 'load game' buttons */
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NSS_GENERATE_GAME), SetDataTip(STR_INTRO_NEW_GAME, STR_INTRO_TOOLTIP_NEW_GAME), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NSS_LOAD_GAME), SetDataTip(STR_INTRO_LOAD_GAME, STR_INTRO_TOOLTIP_LOAD_GAME), SetFill(1, 0),
				EndContainer(),

				/* 'play scenario' and 'play heightmap' buttons */
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NSS_PLAY_SCENARIO), SetDataTip(STR_INTRO_PLAY_SCENARIO, STR_INTRO_TOOLTIP_PLAY_SCENARIO), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NSS_PLAY_HEIGHTMAP), SetDataTip(STR_INTRO_PLAY_HEIGHTMAP, STR_INTRO_TOOLTIP_PLAY_HEIGHTMAP), SetFill(1, 0),
				EndContainer(),
			EndContainer(),

			NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NSS_CANCEL), SetDataTip(STR_BUTTON_CANCEL, STR_NULL), SetMinimalSize(128, 12),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _network_start_server_window_desc(__FILE__, __LINE__,
	WDP_CENTER, nullptr, 0, 0,
	WC_NETWORK_WINDOW, WC_NONE,
	0,
	std::begin(_nested_network_start_server_window_widgets), std::end(_nested_network_start_server_window_widgets)
);

static void ShowNetworkStartServerWindow()
{
	if (!NetworkValidateOurClientName()) return;

	CloseWindowById(WC_NETWORK_WINDOW, WN_NETWORK_WINDOW_GAME);

	new NetworkStartServerWindow(&_network_start_server_window_desc);
}

/* The window below gives information about the connected clients
 * and also makes able to kick them (if server) and stuff like that. */

extern void DrawCompanyIcon(CompanyID cid, int x, int y);

static const NWidgetPart _nested_client_list_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_NETWORK_CLIENT_LIST_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(WWT_FRAME, COLOUR_GREY), SetDataTip(STR_NETWORK_CLIENT_LIST_SERVER, STR_NULL), SetPadding(4, 4, 0, 4), SetPIP(0, 2, 0),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
				NWidget(WWT_TEXT, COLOUR_GREY), SetDataTip(STR_NETWORK_CLIENT_LIST_SERVER_NAME, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(10, 0),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_CL_SERVER_NAME), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_JUST_RAW_STRING, STR_NETWORK_CLIENT_LIST_SERVER_NAME_TOOLTIP), SetAlignment(SA_VERT_CENTER | SA_RIGHT),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_CL_SERVER_NAME_EDIT), SetMinimalSize(12, 14), SetDataTip(SPR_RENAME, STR_NETWORK_CLIENT_LIST_SERVER_NAME_EDIT_TOOLTIP),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_CL_SERVER_SELECTOR),
				NWidget(NWID_VERTICAL),
					NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
						NWidget(WWT_TEXT, COLOUR_GREY), SetDataTip(STR_NETWORK_CLIENT_LIST_SERVER_VISIBILITY, STR_NULL),
						NWidget(NWID_SPACER), SetMinimalSize(10, 0), SetFill(1, 0), SetResize(1, 0),
						NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_CL_SERVER_VISIBILITY), SetDataTip(STR_JUST_STRING, STR_NETWORK_CLIENT_LIST_SERVER_VISIBILITY_TOOLTIP),
					EndContainer(),
					NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
						NWidget(WWT_TEXT, COLOUR_GREY), SetDataTip(STR_NETWORK_CLIENT_LIST_SERVER_INVITE_CODE, STR_NULL),
						NWidget(NWID_SPACER), SetMinimalSize(10, 0),
						NWidget(WWT_TEXT, COLOUR_GREY, WID_CL_SERVER_INVITE_CODE), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_JUST_RAW_STRING, STR_NETWORK_CLIENT_LIST_SERVER_INVITE_CODE_TOOLTIP), SetAlignment(SA_VERT_CENTER | SA_RIGHT),
					EndContainer(),
					NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
						NWidget(WWT_TEXT, COLOUR_GREY), SetDataTip(STR_NETWORK_CLIENT_LIST_SERVER_CONNECTION_TYPE, STR_NULL),
						NWidget(NWID_SPACER), SetMinimalSize(10, 0),
						NWidget(WWT_TEXT, COLOUR_GREY, WID_CL_SERVER_CONNECTION_TYPE), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_JUST_STRING, STR_NETWORK_CLIENT_LIST_SERVER_CONNECTION_TYPE_TOOLTIP), SetAlignment(SA_VERT_CENTER | SA_RIGHT),
					EndContainer(),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_FRAME, COLOUR_GREY), SetDataTip(STR_NETWORK_CLIENT_LIST_PLAYER, STR_NULL), SetPadding(4, 4, 4, 4), SetPIP(0, 2, 0),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
				NWidget(WWT_TEXT, COLOUR_GREY), SetDataTip(STR_NETWORK_CLIENT_LIST_PLAYER_NAME, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(10, 0),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_CL_CLIENT_NAME), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_JUST_RAW_STRING, STR_NETWORK_CLIENT_LIST_PLAYER_NAME_TOOLTIP), SetAlignment(SA_VERT_CENTER | SA_RIGHT),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_CL_CLIENT_NAME_EDIT), SetMinimalSize(12, 14), SetDataTip(SPR_RENAME, STR_NETWORK_CLIENT_LIST_PLAYER_NAME_EDIT_TOOLTIP),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_MATRIX, COLOUR_GREY, WID_CL_MATRIX), SetMinimalSize(180, 0), SetResize(1, 1), SetFill(1, 1), SetMatrixDataTip(1, 0, STR_NULL), SetScrollbar(WID_CL_SCROLLBAR),
			NWidget(WWT_PANEL, COLOUR_GREY),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_CL_CLIENT_COMPANY_COUNT), SetFill(1, 0), SetResize(1, 0), SetPadding(2, 1, 2, 1), SetAlignment(SA_CENTER), SetDataTip(STR_NETWORK_CLIENT_LIST_CLIENT_COMPANY_COUNT, STR_NETWORK_CLIENT_LIST_CLIENT_COMPANY_COUNT_TOOLTIP),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_CL_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _client_list_desc(__FILE__, __LINE__,
	WDP_AUTO, "list_clients", 220, 300,
	WC_CLIENT_LIST, WC_NONE,
	0,
	std::begin(_nested_client_list_widgets), std::end(_nested_client_list_widgets)
);

/**
 * The possibly entries in a DropDown for an admin.
 * Client and companies are mixed; they just have to be unique.
 */
enum DropDownAdmin {
	DD_CLIENT_ADMIN_KICK,
	DD_CLIENT_ADMIN_BAN,
	DD_COMPANY_ADMIN_RESET,
	DD_COMPANY_ADMIN_UNLOCK,
};

/**
 * Callback function for admin command to kick client.
 * @param confirmed Iff the user pressed Yes.
 */
static void AdminClientKickCallback(Window *, bool confirmed)
{
	if (confirmed) NetworkServerKickClient(_admin_client_id, {});
}

/**
 * Callback function for admin command to ban client.
 * @param confirmed Iff the user pressed Yes.
 */
static void AdminClientBanCallback(Window *, bool confirmed)
{
	if (confirmed) NetworkServerKickOrBanIP(_admin_client_id, true, {});
}

/**
 * Callback function for admin command to reset company.
 * @param confirmed Iff the user pressed Yes.
 */
static void AdminCompanyResetCallback(Window *, bool confirmed)
{
	if (confirmed) {
		if (NetworkCompanyHasClients(_admin_company_id)) return;
		Command<CMD_COMPANY_CTRL>::Post(CCA_DELETE, _admin_company_id, CRR_MANUAL, INVALID_CLIENT_ID);
	}
}

/**
 * Callback function for admin command to unlock company.
 * @param confirmed Iff the user pressed Yes.
 */
static void AdminCompanyUnlockCallback(Window *, bool confirmed)
{
	if (confirmed) NetworkServerSetCompanyPassword(_admin_company_id, "", false);
}

/**
 * Button shown for either a company or client in the client-list.
 *
 * These buttons are dynamic and strongly depends on which company/client
 * what buttons are available. This class allows dynamically creating them
 * as the current Widget system does not.
 */
class ButtonCommon {
public:
	SpriteID sprite;   ///< The sprite to use on the button.
	StringID tooltip;  ///< The tooltip of the button.
	Colours colour;    ///< The colour of the button.
	bool disabled;     ///< Is the button disabled?
	uint height;       ///< Calculated height of the button.
	uint width;        ///< Calculated width of the button.

	ButtonCommon(SpriteID sprite, StringID tooltip, Colours colour, bool disabled = false) :
		sprite(sprite),
		tooltip(tooltip),
		colour(colour),
		disabled(disabled)
	{
		Dimension d = GetSpriteSize(sprite);
		this->height = d.height + WidgetDimensions::scaled.framerect.Vertical();
		this->width = d.width + WidgetDimensions::scaled.framerect.Horizontal();
	}
	virtual ~ButtonCommon() = default;

	/**
	 * OnClick handler for when the button is pressed.
	 */
	virtual void OnClick(struct NetworkClientListWindow *w, Point pt) = 0;
};

/**
 * Template version of Button, with callback support.
 */
template<typename T>
class Button : public ButtonCommon {
private:
	typedef void (*ButtonCallback)(struct NetworkClientListWindow *w, Point pt, T id); ///< Callback function to call on click.
	T id;                 ///< ID this button belongs to.
	ButtonCallback proc;  ///< Callback proc to call when button is pressed.

public:
	Button(SpriteID sprite, StringID tooltip, Colours colour, T id, ButtonCallback proc, bool disabled = false) :
		ButtonCommon(sprite, tooltip, colour, disabled),
		id(id),
		proc(proc)
	{
		assert(proc != nullptr);
	}

	void OnClick(struct NetworkClientListWindow *w, Point pt) override
	{
		if (this->disabled) return;

		this->proc(w, pt, this->id);
	}
};

using CompanyButton = Button<CompanyID>;
using ClientButton = Button<ClientID>;

/**
 * Main handle for clientlist
 */
struct NetworkClientListWindow : Window {
private:
	ClientListWidgets query_widget; ///< During a query this tracks what widget caused the query.
	CompanyID join_company; ///< During query for company password, this stores what company we wanted to join.

	ClientID dd_client_id; ///< During admin dropdown, track which client this was for.
	CompanyID dd_company_id; ///< During admin dropdown, track which company this was for.

	Scrollbar *vscroll; ///< Vertical scrollbar of this window.
	uint line_height; ///< Current lineheight of each entry in the matrix.
	uint line_count; ///< Amount of lines in the matrix.
	int hover_index; ///< Index of the current line we are hovering over, or -1 if none.
	int player_self_index; ///< The line the current player is on.
	int player_host_index; ///< The line the host is on.

	std::map<uint, std::vector<std::unique_ptr<ButtonCommon>>> buttons; ///< Per line which buttons are available.

	/**
	 * Chat button on a Company is clicked.
	 * @param w The instance of this window.
	 * @param pt The point where this button was clicked.
	 * @param company_id The company this button was assigned to.
	 */
	static void OnClickCompanyChat([[maybe_unused]] NetworkClientListWindow *w, [[maybe_unused]] Point pt, CompanyID company_id)
	{
		ShowNetworkChatQueryWindow(DESTTYPE_TEAM, company_id);
	}

	/**
	 * Join button on a Company is clicked.
	 * @param w The instance of this window.
	 * @param pt The point where this button was clicked.
	 * @param company_id The company this button was assigned to.
	 */
	static void OnClickCompanyJoin([[maybe_unused]] NetworkClientListWindow *w, [[maybe_unused]] Point pt, CompanyID company_id)
	{
		if (_network_server) {
			NetworkServerDoMove(CLIENT_ID_SERVER, company_id);
			MarkWholeScreenDirty();
		} else if (NetworkCompanyIsPassworded(company_id)) {
			w->query_widget = WID_CL_COMPANY_JOIN;
			w->join_company = company_id;
			ShowQueryString(STR_EMPTY, STR_NETWORK_NEED_COMPANY_PASSWORD_CAPTION, NETWORK_PASSWORD_LENGTH, w, CS_ALPHANUMERAL, QSF_PASSWORD);
		} else {
			NetworkClientRequestMove(company_id);
		}
	}

	/**
	 * Crete new company button is clicked.
	 * @param w The instance of this window.
	 * @param pt The point where this button was clicked.
	 */
	static void OnClickCompanyNew([[maybe_unused]] NetworkClientListWindow *w, [[maybe_unused]] Point pt, CompanyID)
	{
		if (_network_server) {
			Command<CMD_COMPANY_CTRL>::Post(CCA_NEW, INVALID_COMPANY, CRR_NONE, _network_own_client_id);
		} else {
			Command<CMD_COMPANY_CTRL>::SendNet(STR_NULL, _local_company, CCA_NEW, INVALID_COMPANY, CRR_NONE, INVALID_CLIENT_ID);
		}
	}

	/**
	 * Admin button on a Client is clicked.
	 * @param w The instance of this window.
	 * @param pt The point where this button was clicked.
	 * @param client_id The client this button was assigned to.
	 */
	static void OnClickClientAdmin([[maybe_unused]] NetworkClientListWindow *w, [[maybe_unused]] Point pt, ClientID client_id)
	{
		DropDownList list;
		list.push_back(std::make_unique<DropDownListStringItem>(STR_NETWORK_CLIENT_LIST_ADMIN_CLIENT_KICK, DD_CLIENT_ADMIN_KICK, false));
		list.push_back(std::make_unique<DropDownListStringItem>(STR_NETWORK_CLIENT_LIST_ADMIN_CLIENT_BAN, DD_CLIENT_ADMIN_BAN, false));

		Rect wi_rect;
		wi_rect.left   = pt.x;
		wi_rect.right  = pt.x;
		wi_rect.top    = pt.y;
		wi_rect.bottom = pt.y;

		w->dd_client_id = client_id;
		ShowDropDownListAt(w, std::move(list), -1, WID_CL_MATRIX, wi_rect, COLOUR_GREY, true);
	}

	/**
	 * Admin button on a Company is clicked.
	 * @param w The instance of this window.
	 * @param pt The point where this button was clicked.
	 * @param company_id The company this button was assigned to.
	 */
	static void OnClickCompanyAdmin([[maybe_unused]] NetworkClientListWindow *w, [[maybe_unused]] Point pt, CompanyID company_id)
	{
		DropDownList list;
		list.push_back(std::make_unique<DropDownListStringItem>(STR_NETWORK_CLIENT_LIST_ADMIN_COMPANY_RESET, DD_COMPANY_ADMIN_RESET, NetworkCompanyHasClients(company_id)));
		list.push_back(std::make_unique<DropDownListStringItem>(STR_NETWORK_CLIENT_LIST_ADMIN_COMPANY_UNLOCK, DD_COMPANY_ADMIN_UNLOCK, !NetworkCompanyIsPassworded(company_id)));

		Rect wi_rect;
		wi_rect.left   = pt.x;
		wi_rect.right  = pt.x;
		wi_rect.top    = pt.y;
		wi_rect.bottom = pt.y;

		w->dd_company_id = company_id;
		ShowDropDownListAt(w, std::move(list), -1, WID_CL_MATRIX, wi_rect, COLOUR_GREY, true);
	}
	/**
	 * Chat button on a Client is clicked.
	 * @param w The instance of this window.
	 * @param pt The point where this button was clicked.
	 * @param client_id The client this button was assigned to.
	 */
	static void OnClickClientChat([[maybe_unused]] NetworkClientListWindow *w, [[maybe_unused]] Point pt, ClientID client_id)
	{
		ShowNetworkChatQueryWindow(DESTTYPE_CLIENT, client_id);
	}

	/**
	 * Part of RebuildList() to create the information for a single company.
	 * @param company_id The company to build the list for.
	 * @param client_playas The company the client is joined as.
	 */
	void RebuildListCompany(CompanyID company_id, CompanyID client_playas)
	{
		ButtonCommon *chat_button = new CompanyButton(SPR_CHAT, company_id == COMPANY_SPECTATOR ? STR_NETWORK_CLIENT_LIST_CHAT_SPECTATOR_TOOLTIP : STR_NETWORK_CLIENT_LIST_CHAT_COMPANY_TOOLTIP, COLOUR_ORANGE, company_id, &NetworkClientListWindow::OnClickCompanyChat);

		if (_network_server) this->buttons[line_count].push_back(std::make_unique<CompanyButton>(SPR_ADMIN, STR_NETWORK_CLIENT_LIST_ADMIN_COMPANY_TOOLTIP, COLOUR_RED, company_id, &NetworkClientListWindow::OnClickCompanyAdmin, company_id == COMPANY_SPECTATOR));
		this->buttons[line_count].emplace_back(chat_button);
		if (client_playas != company_id) this->buttons[line_count].push_back(std::make_unique<CompanyButton>(SPR_JOIN, STR_NETWORK_CLIENT_LIST_JOIN_TOOLTIP, COLOUR_ORANGE, company_id, &NetworkClientListWindow::OnClickCompanyJoin, company_id != COMPANY_SPECTATOR && Company::Get(company_id)->is_ai));

		this->line_count += 1;

		bool has_players = false;
		for (const NetworkClientInfo *ci : NetworkClientInfo::Iterate()) {
			if (ci->client_playas != company_id) continue;
			has_players = true;

			if (_network_server) this->buttons[line_count].push_back(std::make_unique<ClientButton>(SPR_ADMIN, STR_NETWORK_CLIENT_LIST_ADMIN_CLIENT_TOOLTIP, COLOUR_RED, ci->client_id, &NetworkClientListWindow::OnClickClientAdmin, _network_own_client_id == ci->client_id));
			if (_network_own_client_id != ci->client_id) this->buttons[line_count].push_back(std::make_unique<ClientButton>(SPR_CHAT, STR_NETWORK_CLIENT_LIST_CHAT_CLIENT_TOOLTIP, COLOUR_ORANGE, ci->client_id, &NetworkClientListWindow::OnClickClientChat));

			if (ci->client_id == _network_own_client_id) {
				this->player_self_index = this->line_count;
			} else if (ci->client_id == CLIENT_ID_SERVER) {
				this->player_host_index = this->line_count;
			}

			this->line_count += 1;
		}

		/* Disable the chat button when there are players in this company. */
		chat_button->disabled = !has_players;
	}

	/**
	 * Rebuild the list, meaning: calculate the lines needed and what buttons go on which line.
	 */
	void RebuildList()
	{
		const NetworkClientInfo *own_ci = NetworkClientInfo::GetByClientID(_network_own_client_id);
		CompanyID client_playas = own_ci == nullptr ? COMPANY_SPECTATOR : own_ci->client_playas;

		this->buttons.clear();
		this->line_count = 0;
		this->player_host_index = -1;
		this->player_self_index = -1;

		/* As spectator, show a line to create a new company. */
		if (client_playas == COMPANY_SPECTATOR && !NetworkMaxCompaniesReached()) {
			this->buttons[line_count].push_back(std::make_unique<CompanyButton>(SPR_JOIN, STR_NETWORK_CLIENT_LIST_NEW_COMPANY_TOOLTIP, COLOUR_ORANGE, COMPANY_SPECTATOR, &NetworkClientListWindow::OnClickCompanyNew));
			this->line_count += 1;
		}

		if (client_playas != COMPANY_SPECTATOR) {
			this->RebuildListCompany(client_playas, client_playas);
		}

		/* Companies */
		for (const Company *c : Company::Iterate()) {
			if (c->index == client_playas) continue;

			this->RebuildListCompany(c->index, client_playas);
		}

		/* Spectators */
		this->RebuildListCompany(COMPANY_SPECTATOR, client_playas);

		this->vscroll->SetCount(this->line_count);
	}

	/**
	 * Get the button at a specific point on the WID_CL_MATRIX.
	 * @param pt The point to look for a button.
	 * @return The button or a nullptr if there was none.
	 */
	ButtonCommon *GetButtonAtPoint(Point pt)
	{
		uint index = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_CL_MATRIX);
		Rect matrix = this->GetWidget<NWidgetBase>(WID_CL_MATRIX)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);

		bool rtl = _current_text_dir == TD_RTL;
		uint x = rtl ? matrix.left : matrix.right;

		/* Find the buttons for this row. */
		auto button_find = this->buttons.find(index);
		if (button_find == this->buttons.end()) return nullptr;

		/* Check if we want to display a tooltip for any of the buttons. */
		for (auto &button : button_find->second) {
			uint left = rtl ? x : x - button->width;
			uint right = rtl ? x + button->width : x;

			if (IsInsideMM(pt.x, left, right)) {
				return button.get();
			}

			int width = button->width + WidgetDimensions::scaled.framerect.Horizontal();
			x += rtl ? width : -width;
		}

		return nullptr;
	}

public:
	NetworkClientListWindow(WindowDesc *desc, WindowNumber window_number) :
			Window(desc),
			hover_index(-1),
			player_self_index(-1),
			player_host_index(-1)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_CL_SCROLLBAR);
		this->OnInvalidateData();
		this->FinishInitNested(window_number);
	}

	void OnInit() override
	{
		RebuildList();
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		this->RebuildList();

		/* Currently server information is not sync'd to clients, so we cannot show it on clients. */
		this->GetWidget<NWidgetStacked>(WID_CL_SERVER_SELECTOR)->SetDisplayedPlane(_network_server ? 0 : SZSP_HORIZONTAL);
		this->SetWidgetDisabledState(WID_CL_SERVER_NAME_EDIT, !_network_server);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_CL_SERVER_VISIBILITY:
				*size = maxdim(maxdim(GetStringBoundingBox(STR_NETWORK_SERVER_VISIBILITY_LOCAL), GetStringBoundingBox(STR_NETWORK_SERVER_VISIBILITY_PUBLIC)), GetStringBoundingBox(STR_NETWORK_SERVER_VISIBILITY_INVITE_ONLY));
				size->width += padding.width;
				size->height += padding.height;
				break;

			case WID_CL_MATRIX: {
				uint height = std::max({GetSpriteSize(SPR_COMPANY_ICON).height, GetSpriteSize(SPR_JOIN).height, GetSpriteSize(SPR_ADMIN).height, GetSpriteSize(SPR_CHAT).height});
				height += WidgetDimensions::scaled.framerect.Vertical();
				this->line_height = std::max(height, (uint)GetCharacterHeight(FS_NORMAL)) + padding.height;

				resize->width = 1;
				resize->height = this->line_height;
				fill->height = this->line_height;
				size->height = std::max(size->height, 5 * this->line_height);
				break;
			}
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_CL_MATRIX);
	}

	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case WID_CL_SERVER_NAME:
				SetDParamStr(0, _network_server ? _settings_client.network.server_name : _network_server_name);
				break;

			case WID_CL_SERVER_VISIBILITY:
				SetDParam(0, STR_NETWORK_SERVER_VISIBILITY_LOCAL + _settings_client.network.server_game_type);
				break;

			case WID_CL_SERVER_INVITE_CODE: {
				static std::string empty = {};
				SetDParamStr(0, _network_server_connection_type == CONNECTION_TYPE_UNKNOWN ? empty : _network_server_invite_code);
				break;
			}

			case WID_CL_SERVER_CONNECTION_TYPE:
				SetDParam(0, STR_NETWORK_CLIENT_LIST_SERVER_CONNECTION_TYPE_UNKNOWN + _network_server_connection_type);
				break;

			case WID_CL_CLIENT_NAME: {
				const NetworkClientInfo *own_ci = NetworkClientInfo::GetByClientID(_network_own_client_id);
				SetDParamStr(0, own_ci != nullptr ? own_ci->client_name : _settings_client.network.client_name);
				break;
			}

			case WID_CL_CLIENT_COMPANY_COUNT:
				SetDParam(0, NetworkClientInfo::GetNumItems());
				SetDParam(1, Company::GetNumItems());
				SetDParam(2, NetworkMaxCompaniesAllowed());
				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_CL_SERVER_NAME_EDIT:
				if (!_network_server) break;

				this->query_widget = WID_CL_SERVER_NAME_EDIT;
				SetDParamStr(0, _settings_client.network.server_name);
				ShowQueryString(STR_JUST_RAW_STRING, STR_NETWORK_CLIENT_LIST_SERVER_NAME_QUERY_CAPTION, NETWORK_NAME_LENGTH, this, CS_ALPHANUMERAL, QSF_LEN_IN_CHARS);
				break;

			case WID_CL_CLIENT_NAME_EDIT: {
				const NetworkClientInfo *own_ci = NetworkClientInfo::GetByClientID(_network_own_client_id);
				this->query_widget = WID_CL_CLIENT_NAME_EDIT;
				SetDParamStr(0, own_ci != nullptr ? own_ci->client_name : _settings_client.network.client_name);
				ShowQueryString(STR_JUST_RAW_STRING, STR_NETWORK_CLIENT_LIST_PLAYER_NAME_QUERY_CAPTION, NETWORK_CLIENT_NAME_LENGTH, this, CS_ALPHANUMERAL, QSF_LEN_IN_CHARS);
				break;
			}
			case WID_CL_SERVER_VISIBILITY:
				if (!_network_server) break;

				ShowDropDownList(this, BuildVisibilityDropDownList(), _settings_client.network.server_game_type, WID_CL_SERVER_VISIBILITY);
				break;

			case WID_CL_MATRIX: {
				ButtonCommon *button = this->GetButtonAtPoint(pt);
				if (button == nullptr) break;

				button->OnClick(this, pt);
				break;
			}
		}
	}

	bool OnTooltip([[maybe_unused]] Point pt, WidgetID widget, TooltipCloseCondition close_cond) override
	{
		switch (widget) {
			case WID_CL_MATRIX: {
				int index = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_CL_MATRIX);

				bool rtl = _current_text_dir == TD_RTL;
				Rect matrix = this->GetWidget<NWidgetBase>(WID_CL_MATRIX)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);

				Dimension d = GetSpriteSize(SPR_COMPANY_ICON);
				uint text_left  = matrix.left  + (rtl ? 0 : d.width + WidgetDimensions::scaled.hsep_wide);
				uint text_right = matrix.right - (rtl ? d.width + WidgetDimensions::scaled.hsep_wide : 0);

				Dimension d2 = GetSpriteSize(SPR_PLAYER_SELF);
				uint offset_x = WidgetDimensions::scaled.hsep_indent - d2.width - ScaleGUITrad(3);

				uint player_icon_x = rtl ? text_right - offset_x - d2.width : text_left + offset_x;

				if (IsInsideMM(pt.x, player_icon_x, player_icon_x + d2.width)) {
					if (index == this->player_self_index) {
						GuiShowTooltips(this, STR_NETWORK_CLIENT_LIST_PLAYER_ICON_SELF_TOOLTIP, close_cond);
						return true;
					} else if (index == this->player_host_index) {
						GuiShowTooltips(this, STR_NETWORK_CLIENT_LIST_PLAYER_ICON_HOST_TOOLTIP, close_cond);
						return true;
					}
				}

				ButtonCommon *button = this->GetButtonAtPoint(pt);
				if (button == nullptr) return false;

				GuiShowTooltips(this, button->tooltip, close_cond);
				return true;
			};
		}

		return false;
	}

	void OnDropdownClose(Point pt, WidgetID widget, int index, bool instant_close) override
	{
		/* If you close the dropdown outside the list, don't take any action. */
		if (widget == WID_CL_MATRIX) return;

		Window::OnDropdownClose(pt, widget, index, instant_close);
	}

	void OnDropdownSelect(WidgetID widget, int index) override
	{
		switch (widget) {
			case WID_CL_SERVER_VISIBILITY:
				if (!_network_server) break;

				_settings_client.network.server_game_type = (ServerGameType)index;
				NetworkUpdateServerGameType();
				break;

			case WID_CL_MATRIX: {
				StringID text = STR_NULL;
				QueryCallbackProc *callback = nullptr;

				switch (index) {
					case DD_CLIENT_ADMIN_KICK:
						_admin_client_id = this->dd_client_id;
						text = STR_NETWORK_CLIENT_LIST_ASK_CLIENT_KICK;
						callback = AdminClientKickCallback;
						SetDParamStr(0, NetworkClientInfo::GetByClientID(_admin_client_id)->client_name);
						break;

					case DD_CLIENT_ADMIN_BAN:
						_admin_client_id = this->dd_client_id;
						text = STR_NETWORK_CLIENT_LIST_ASK_CLIENT_BAN;
						callback = AdminClientBanCallback;
						SetDParamStr(0, NetworkClientInfo::GetByClientID(_admin_client_id)->client_name);
						break;

					case DD_COMPANY_ADMIN_RESET:
						_admin_company_id = this->dd_company_id;
						text = STR_NETWORK_CLIENT_LIST_ASK_COMPANY_RESET;
						callback = AdminCompanyResetCallback;
						SetDParam(0, _admin_company_id);
						break;

					case DD_COMPANY_ADMIN_UNLOCK:
						_admin_company_id = this->dd_company_id;
						text = STR_NETWORK_CLIENT_LIST_ASK_COMPANY_UNLOCK;
						callback = AdminCompanyUnlockCallback;
						SetDParam(0, _admin_company_id);
						break;

					default:
						NOT_REACHED();
				}

				assert(text != STR_NULL);
				assert(callback != nullptr);

				/* Always ask confirmation for all admin actions. */
				ShowQuery(STR_NETWORK_CLIENT_LIST_ASK_CAPTION, text, this, callback);

				break;
			}

			default:
				NOT_REACHED();
		}

		this->SetDirty();
	}

	void OnQueryTextFinished(char *str) override
	{
		if (str == nullptr) return;

		switch (this->query_widget) {
			default: NOT_REACHED();

			case WID_CL_SERVER_NAME_EDIT: {
				if (!_network_server) break;

				SetSettingValue(GetSettingFromName("network.server_name")->AsStringSetting(), str);
				this->InvalidateData();
				break;
			}

			case WID_CL_CLIENT_NAME_EDIT: {
				SetSettingValue(GetSettingFromName("network.client_name")->AsStringSetting(), str);
				this->InvalidateData();
				break;
			}

			case WID_CL_COMPANY_JOIN:
				NetworkClientRequestMove(this->join_company, str);
				break;
		}
	}

	/**
	 * Draw the buttons for a single line in the matrix.
	 *
	 * The x-position in RTL is the most left or otherwise the most right pixel
	 * we can draw the buttons from.
	 *
	 * @param x The x-position to start with the buttons. Updated during this function.
	 * @param y The y-position to start with the buttons.
	 * @param buttons The buttons to draw.
	 */
	void DrawButtons(int &x, uint y, const std::vector<std::unique_ptr<ButtonCommon>> &buttons) const
	{
		Rect r;

		for (auto &button : buttons) {
			bool rtl = _current_text_dir == TD_RTL;

			int offset = (this->line_height - button->height) / 2;
			r.left = rtl ? x : x - button->width + 1;
			r.right = rtl ? x + button->width - 1 : x;
			r.top = y + offset;
			r.bottom = r.top + button->height - 1;

			DrawFrameRect(r, button->colour, FR_NONE);
			DrawSprite(button->sprite, PAL_NONE, r.left + WidgetDimensions::scaled.framerect.left, r.top + WidgetDimensions::scaled.framerect.top);
			if (button->disabled) {
				GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), _colour_gradient[button->colour & 0xF][2], FILLRECT_CHECKER);
			}

			int width = button->width + WidgetDimensions::scaled.hsep_normal;
			x += rtl ? width : -width;
		}
	}

	/**
	 * Draw a company and its clients on the matrix.
	 * @param company_id The company to draw.
	 * @param r The rect to draw within.
	 * @param line The Nth line we are drawing. Updated during this function.
	 */
	void DrawCompany(CompanyID company_id, const Rect &r, uint &line) const
	{
		bool rtl = _current_text_dir == TD_RTL;
		int text_y_offset = CenterBounds(0, this->line_height, GetCharacterHeight(FS_NORMAL));

		Dimension d = GetSpriteSize(SPR_COMPANY_ICON);
		int offset = CenterBounds(0, this->line_height, d.height);

		uint line_start = this->vscroll->GetPosition();
		uint line_end = line_start + this->vscroll->GetCapacity();

		uint y = r.top + (this->line_height * (line - line_start));

		/* Draw the company line (if in range of scrollbar). */
		if (IsInsideMM(line, line_start, line_end)) {
			int icon_left = r.WithWidth(d.width, rtl).left;
			Rect tr = r.Indent(d.width + WidgetDimensions::scaled.hsep_normal, rtl);
			int &x = rtl ? tr.left : tr.right;

			/* If there are buttons for this company, draw them. */
			auto button_find = this->buttons.find(line);
			if (button_find != this->buttons.end()) {
				this->DrawButtons(x, y, button_find->second);
			}

			if (company_id == COMPANY_SPECTATOR) {
				DrawSprite(SPR_COMPANY_ICON, PALETTE_TO_GREY, icon_left, y + offset);
				DrawString(tr.left, tr.right, y + text_y_offset, STR_NETWORK_CLIENT_LIST_SPECTATORS, TC_SILVER);
			} else if (company_id == COMPANY_NEW_COMPANY) {
				DrawSprite(SPR_COMPANY_ICON, PALETTE_TO_GREY, icon_left, y + offset);
				DrawString(tr.left, tr.right, y + text_y_offset, STR_NETWORK_CLIENT_LIST_NEW_COMPANY, TC_WHITE);
			} else {
				DrawCompanyIcon(company_id, icon_left, y + offset);

				SetDParam(0, company_id);
				SetDParam(1, company_id);
				DrawString(tr.left, tr.right, y + text_y_offset, STR_COMPANY_NAME, TC_SILVER);
			}
		}

		y += this->line_height;
		line++;

		for (const NetworkClientInfo *ci : NetworkClientInfo::Iterate()) {
			if (ci->client_playas != company_id) continue;

			/* Draw the player line (if in range of scrollbar). */
			if (IsInsideMM(line, line_start, line_end)) {
				Rect tr = r.Indent(WidgetDimensions::scaled.hsep_indent, rtl);

				/* If there are buttons for this client, draw them. */
				auto button_find = this->buttons.find(line);
				if (button_find != this->buttons.end()) {
					int &x = rtl ? tr.left : tr.right;
					this->DrawButtons(x, y, button_find->second);
				}

				SpriteID player_icon = 0;
				if (ci->client_id == _network_own_client_id) {
					player_icon = SPR_PLAYER_SELF;
				} else if (ci->client_id == CLIENT_ID_SERVER) {
					player_icon = SPR_PLAYER_HOST;
				}

				if (player_icon != 0) {
					Dimension d2 = GetSpriteSize(player_icon);
					int offset_y = CenterBounds(0, this->line_height, d2.height);
					DrawSprite(player_icon, PALETTE_TO_GREY, rtl ? tr.right - d2.width : tr.left, y + offset_y);
					tr = tr.Indent(d2.width + WidgetDimensions::scaled.hsep_normal, rtl);
				}

				SetDParamStr(0, ci->client_name);
				DrawString(tr.left, tr.right, y + text_y_offset, STR_JUST_RAW_STRING, TC_BLACK);
			}

			y += this->line_height;
			line++;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_CL_MATRIX: {
				Rect ir = r.Shrink(WidgetDimensions::scaled.framerect, RectPadding::zero);
				uint line = 0;

				if (this->hover_index >= 0) {
					Rect br = r.WithHeight(this->line_height).Translate(0, this->hover_index * this->line_height);
					GfxFillRect(br.Shrink(WidgetDimensions::scaled.bevel), GREY_SCALE(9));
				}

				NetworkClientInfo *own_ci = NetworkClientInfo::GetByClientID(_network_own_client_id);
				CompanyID client_playas = own_ci == nullptr ? COMPANY_SPECTATOR : own_ci->client_playas;

				if (client_playas == COMPANY_SPECTATOR && !NetworkMaxCompaniesReached()) {
					this->DrawCompany(COMPANY_NEW_COMPANY, ir, line);
				}

				if (client_playas != COMPANY_SPECTATOR) {
					this->DrawCompany(client_playas, ir, line);
				}

				for (const Company *c : Company::Iterate()) {
					if (client_playas == c->index) continue;
					this->DrawCompany(c->index, ir, line);
				}

				/* Spectators */
				this->DrawCompany(COMPANY_SPECTATOR, ir, line);

				break;
			}
		}
	}

	void OnMouseOver([[maybe_unused]] Point pt, WidgetID widget) override
	{
		if (widget != WID_CL_MATRIX) {
			if (this->hover_index != -1) {
				this->hover_index = -1;
				this->SetWidgetDirty(WID_CL_MATRIX);
			}
		} else {
			int index = this->GetRowFromWidget(pt.y, widget, 0, -1);
			if (index != this->hover_index) {
				this->hover_index = index;
				this->SetWidgetDirty(WID_CL_MATRIX);
			}
		}
	}
};

void ShowClientList()
{
	AllocateWindowDescFront<NetworkClientListWindow>(&_client_list_desc, 0);
}

NetworkJoinStatus _network_join_status; ///< The status of joining.
uint8_t _network_join_waiting;            ///< The number of clients waiting in front of us.
uint32_t _network_join_bytes;             ///< The number of bytes we already downloaded.
uint32_t _network_join_bytes_total;       ///< The total number of bytes to download.

struct NetworkJoinStatusWindow : Window {
	NetworkPasswordType password_type;

	NetworkJoinStatusWindow(WindowDesc *desc) : Window(desc)
	{
		this->parent = FindWindowById(WC_NETWORK_WINDOW, WN_NETWORK_WINDOW_GAME);
		this->InitNested(WN_NETWORK_STATUS_WINDOW_JOIN);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_NJS_PROGRESS_BAR: {
				/* Draw the % complete with a bar and a text */
				DrawFrameRect(r, COLOUR_GREY, FR_BORDERONLY | FR_LOWERED);
				Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
				uint8_t progress; // used for progress bar
				switch (_network_join_status) {
					case NETWORK_JOIN_STATUS_CONNECTING:
					case NETWORK_JOIN_STATUS_AUTHORIZING:
					case NETWORK_JOIN_STATUS_GETTING_COMPANY_INFO:
						progress = 10; // first two stages 10%
						break;
					case NETWORK_JOIN_STATUS_WAITING:
						progress = 15; // third stage is 15%
						break;
					case NETWORK_JOIN_STATUS_DOWNLOADING:
						if (_network_join_bytes_total == 0) {
							progress = 15; // We don't have the final size yet; the server is still compressing!
							break;
						}
						FALLTHROUGH;

					default: // Waiting is 15%, so the resting receivement of map is maximum 70%
						progress = 15 + _network_join_bytes * (100 - 15) / _network_join_bytes_total;
						break;
				}
				DrawFrameRect(ir.WithWidth(ir.Width() * progress / 100, false), COLOUR_MAUVE, FR_NONE);
				DrawString(ir.left, ir.right, CenterBounds(ir.top, ir.bottom, GetCharacterHeight(FS_NORMAL)), STR_NETWORK_CONNECTING_1 + _network_join_status, TC_FROMSTRING, SA_HOR_CENTER);
				break;
			}

			case WID_NJS_PROGRESS_TEXT:
				switch (_network_join_status) {
					case NETWORK_JOIN_STATUS_WAITING:
						SetDParam(0, _network_join_waiting);
						DrawStringMultiLine(r, STR_NETWORK_CONNECTING_WAITING, TC_FROMSTRING, SA_CENTER);
						break;
					case NETWORK_JOIN_STATUS_DOWNLOADING:
						SetDParam(0, _network_join_bytes);
						SetDParam(1, _network_join_bytes_total);
						DrawStringMultiLine(r, _network_join_bytes_total == 0 ? STR_NETWORK_CONNECTING_DOWNLOADING_1 : STR_NETWORK_CONNECTING_DOWNLOADING_2, TC_FROMSTRING, SA_CENTER);
						break;
					default:
						break;
				}
				break;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_NJS_PROGRESS_BAR:
				/* Account for the statuses */
				for (uint i = 0; i < NETWORK_JOIN_STATUS_END; i++) {
					*size = maxdim(*size, GetStringBoundingBox(STR_NETWORK_CONNECTING_1 + i));
				}
				/* For the number of waiting (other) players */
				SetDParamMaxValue(0, MAX_CLIENTS);
				*size = maxdim(*size, GetStringBoundingBox(STR_NETWORK_CONNECTING_WAITING));
				/* We need some spacing for the 'border' */
				size->height += WidgetDimensions::scaled.frametext.Horizontal();
				size->width  += WidgetDimensions::scaled.frametext.Vertical();
				break;

			case WID_NJS_PROGRESS_TEXT:
				/* Account for downloading ~ 10 MiB */
				SetDParamMaxDigits(0, 8);
				SetDParamMaxDigits(1, 8);
				*size = maxdim(*size, GetStringBoundingBox(STR_NETWORK_CONNECTING_DOWNLOADING_1));
				*size = maxdim(*size, GetStringBoundingBox(STR_NETWORK_CONNECTING_DOWNLOADING_1));
				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget == WID_NJS_CANCELOK) { // Disconnect button
			NetworkDisconnect();
			SwitchToMode(SM_MENU);
			ShowNetworkGameWindow();
		}
	}

	void OnQueryTextFinished(char *str) override
	{
		if (StrEmpty(str)) {
			NetworkDisconnect();
			return;
		}

		switch (this->password_type) {
			case NETWORK_GAME_PASSWORD:    MyClient::SendGamePassword   (str); break;
			case NETWORK_COMPANY_PASSWORD: MyClient::SendCompanyPassword(str); break;
			default: NOT_REACHED();
		}
	}
};

static const NWidgetPart _nested_network_join_status_window_widgets[] = {
	NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_NETWORK_CONNECTING_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_NJS_PROGRESS_BAR), SetFill(1, 0),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_NJS_PROGRESS_TEXT), SetFill(1, 0), SetMinimalSize(350, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NJS_CANCELOK), SetMinimalSize(101, 12), SetDataTip(STR_NETWORK_CONNECTION_DISCONNECT, STR_NULL), SetFill(1, 0),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _network_join_status_window_desc(__FILE__, __LINE__,
	WDP_CENTER, nullptr, 0, 0,
	WC_NETWORK_STATUS_WINDOW, WC_NONE,
	WDF_MODAL,
	std::begin(_nested_network_join_status_window_widgets), std::end(_nested_network_join_status_window_widgets)
);

void ShowJoinStatusWindow()
{
	CloseWindowById(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_JOIN);
	new NetworkJoinStatusWindow(&_network_join_status_window_desc);
}

void ShowNetworkNeedPassword(NetworkPasswordType npt)
{
	NetworkJoinStatusWindow *w = (NetworkJoinStatusWindow *)FindWindowById(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_JOIN);
	if (w == nullptr) return;
	w->password_type = npt;

	StringID caption;
	switch (npt) {
		default: NOT_REACHED();
		case NETWORK_GAME_PASSWORD:    caption = STR_NETWORK_NEED_GAME_PASSWORD_CAPTION; break;
		case NETWORK_COMPANY_PASSWORD: caption = STR_NETWORK_NEED_COMPANY_PASSWORD_CAPTION; break;
	}
	ShowQueryString(STR_EMPTY, caption, NETWORK_PASSWORD_LENGTH, w, CS_ALPHANUMERAL, QSF_PASSWORD);
}

struct NetworkCompanyPasswordWindow : public Window {
	QueryString password_editbox; ///< Password editbox.
	Dimension warning_size;       ///< How much space to use for the warning text

	NetworkCompanyPasswordWindow(WindowDesc *desc, Window *parent) : Window(desc), password_editbox(lengthof(_settings_client.network.default_company_pass))
	{
		this->InitNested(0);
		this->UpdateWarningStringSize();

		this->parent = parent;
		this->querystrings[WID_NCP_PASSWORD] = &this->password_editbox;
		this->password_editbox.cancel_button = WID_NCP_CANCEL;
		this->password_editbox.ok_button = WID_NCP_OK;
		this->SetFocusedWidget(WID_NCP_PASSWORD);
	}

	void UpdateWarningStringSize()
	{
		assert(this->nested_root->smallest_x > 0);
		this->warning_size.width = this->nested_root->current_x - (WidgetDimensions::scaled.framerect.Horizontal()) * 2;
		this->warning_size.height = GetStringHeight(STR_WARNING_PASSWORD_SECURITY, this->warning_size.width);
		this->warning_size.height += (WidgetDimensions::scaled.framerect.Vertical()) * 2;

		this->ReInit();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget == WID_NCP_WARNING) {
			*size = this->warning_size;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_NCP_WARNING) return;

		DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.framerect),
			STR_WARNING_PASSWORD_SECURITY, TC_FROMSTRING, SA_CENTER);
	}

	void OnOk()
	{
		if (this->IsWidgetLowered(WID_NCP_SAVE_AS_DEFAULT_PASSWORD)) {
			_settings_client.network.default_company_pass = this->password_editbox.text.buf;
		}

		NetworkChangeCompanyPassword(_local_company, this->password_editbox.text.buf);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_NCP_OK:
				this->OnOk();
				FALLTHROUGH;

			case WID_NCP_CANCEL:
				this->Close();
				break;

			case WID_NCP_SAVE_AS_DEFAULT_PASSWORD:
				this->ToggleWidgetLoweredState(WID_NCP_SAVE_AS_DEFAULT_PASSWORD);
				this->SetDirty();
				break;
		}
	}
};

static const NWidgetPart _nested_network_company_password_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_COMPANY_PASSWORD_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_NCP_BACKGROUND),
		NWidget(NWID_VERTICAL), SetPIP(5, 5, 5),
			NWidget(NWID_HORIZONTAL), SetPIP(5, 5, 5),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_NCP_LABEL), SetDataTip(STR_COMPANY_VIEW_PASSWORD, STR_NULL),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WID_NCP_PASSWORD), SetFill(1, 0), SetMinimalSize(194, 12), SetDataTip(STR_COMPANY_VIEW_SET_PASSWORD, STR_NULL),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(5, 0, 5),
				NWidget(NWID_SPACER), SetFill(1, 0),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_NCP_SAVE_AS_DEFAULT_PASSWORD), SetMinimalSize(194, 12),
											SetDataTip(STR_COMPANY_PASSWORD_MAKE_DEFAULT, STR_COMPANY_PASSWORD_MAKE_DEFAULT_TOOLTIP),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_NCP_WARNING), EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_NCP_CANCEL), SetFill(1, 0), SetDataTip(STR_BUTTON_CANCEL, STR_COMPANY_PASSWORD_CANCEL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_NCP_OK), SetFill(1, 0), SetDataTip(STR_BUTTON_OK, STR_COMPANY_PASSWORD_OK),
	EndContainer(),
};

static WindowDesc _network_company_password_window_desc(__FILE__, __LINE__,
	WDP_AUTO, nullptr, 0, 0,
	WC_COMPANY_PASSWORD_WINDOW, WC_NONE,
	0,
	std::begin(_nested_network_company_password_window_widgets), std::end(_nested_network_company_password_window_widgets)
);

void ShowNetworkCompanyPasswordWindow(Window *parent)
{
	CloseWindowById(WC_COMPANY_PASSWORD_WINDOW, 0);

	new NetworkCompanyPasswordWindow(&_network_company_password_window_desc, parent);
}

/**
 * Window used for asking the user if he is okay using a relay server.
 */
struct NetworkAskRelayWindow : public Window {
	std::string server_connection_string; ///< The game server we want to connect to.
	std::string relay_connection_string;  ///< The relay server we want to connect to.
	std::string token;                    ///< The token for this connection.

	NetworkAskRelayWindow(WindowDesc *desc, Window *parent, const std::string &server_connection_string, const std::string &relay_connection_string, const std::string &token) :
		Window(desc),
		server_connection_string(server_connection_string),
		relay_connection_string(relay_connection_string),
		token(token)
	{
		this->parent = parent;
		this->InitNested(0);
	}

	void Close(int data = 0) override
	{
		if (data == NRWCD_UNHANDLED) _network_coordinator_client.ConnectFailure(this->token, 0);
		this->Window::Close();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget == WID_NAR_TEXT) {
			*size = GetStringBoundingBox(STR_NETWORK_ASK_RELAY_TEXT);
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget == WID_NAR_TEXT) {
			DrawStringMultiLine(r, STR_NETWORK_ASK_RELAY_TEXT, TC_FROMSTRING, SA_CENTER);
		}
	}

	void FindWindowPlacementAndResize([[maybe_unused]] int def_width, [[maybe_unused]] int def_height) override
	{
		/* Position query window over the calling window, ensuring it's within screen bounds. */
		this->left = Clamp(parent->left + (parent->width / 2) - (this->width / 2), 0, _screen.width - this->width);
		this->top = Clamp(parent->top + (parent->height / 2) - (this->height / 2), 0, _screen.height - this->height);
		this->SetDirty();
	}

	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case WID_NAR_TEXT:
				SetDParamStr(0, this->server_connection_string);
				SetDParamStr(1, this->relay_connection_string);
				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_NAR_NO:
				_network_coordinator_client.ConnectFailure(this->token, 0);
				this->Close(NRWCD_HANDLED);
				break;

			case WID_NAR_YES_ONCE:
				_network_coordinator_client.StartTurnConnection(this->token);
				this->Close(NRWCD_HANDLED);
				break;

			case WID_NAR_YES_ALWAYS:
				_settings_client.network.use_relay_service = URS_ALLOW;
				_network_coordinator_client.StartTurnConnection(this->token);
				this->Close(NRWCD_HANDLED);
				break;
		}
	}
};

static const NWidgetPart _nested_network_ask_relay_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_RED),
		NWidget(WWT_CAPTION, COLOUR_RED, WID_NAR_CAPTION), SetDataTip(STR_NETWORK_ASK_RELAY_CAPTION, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_RED),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_TEXT, COLOUR_RED, WID_NAR_TEXT), SetAlignment(SA_HOR_CENTER), SetFill(1, 1),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NAR_NO), SetMinimalSize(71, 12), SetFill(1, 1), SetDataTip(STR_NETWORK_ASK_RELAY_NO, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NAR_YES_ONCE), SetMinimalSize(71, 12), SetFill(1, 1), SetDataTip(STR_NETWORK_ASK_RELAY_YES_ONCE, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NAR_YES_ALWAYS), SetMinimalSize(71, 12), SetFill(1, 1), SetDataTip(STR_NETWORK_ASK_RELAY_YES_ALWAYS, STR_NULL),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _network_ask_relay_desc(__FILE__, __LINE__,
	WDP_CENTER, nullptr, 0, 0,
	WC_NETWORK_ASK_RELAY, WC_NONE,
	WDF_MODAL,
	std::begin(_nested_network_ask_relay_widgets), std::end(_nested_network_ask_relay_widgets)
);

/**
 * Show a modal confirmation window with "no" / "yes, once" / "yes, always" buttons.
 * @param server_connection_string The game server we want to connect to.
 * @param relay_connection_string The relay server we want to connect to.
 * @param token The token for this connection.
 */
void ShowNetworkAskRelay(const std::string &server_connection_string, const std::string &relay_connection_string, const std::string &token)
{
	CloseWindowByClass(WC_NETWORK_ASK_RELAY, NRWCD_HANDLED);

	Window *parent = GetMainWindow();
	new NetworkAskRelayWindow(&_network_ask_relay_desc, parent, server_connection_string, relay_connection_string, token);
}

/**
 * Window used for asking if the user wants to participate in the automated survey.
 */
struct NetworkAskSurveyWindow : public Window {
	NetworkAskSurveyWindow(WindowDesc *desc, Window *parent) :
		Window(desc)
	{
		this->parent = parent;
		this->InitNested(0);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget == WID_NAS_TEXT) {
			*size = GetStringBoundingBox(STR_NETWORK_ASK_SURVEY_TEXT);
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget == WID_NAS_TEXT) {
			DrawStringMultiLine(r, STR_NETWORK_ASK_SURVEY_TEXT, TC_BLACK, SA_CENTER);
		}
	}

	void FindWindowPlacementAndResize([[maybe_unused]] int def_width, [[maybe_unused]] int def_height) override
	{
		/* Position query window over the calling window, ensuring it's within screen bounds. */
		this->left = Clamp(parent->left + (parent->width / 2) - (this->width / 2), 0, _screen.width - this->width);
		this->top = Clamp(parent->top + (parent->height / 2) - (this->height / 2), 0, _screen.height - this->height);
		this->SetDirty();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_NAS_PREVIEW:
				ShowSurveyResultTextfileWindow();
				break;

			case WID_NAS_LINK:
				OpenBrowser(NETWORK_SURVEY_DETAILS_LINK);
				break;

			case WID_NAS_NO:
				_settings_client.network.participate_survey = PS_NO;
				this->Close();
				break;

			case WID_NAS_YES:
				_settings_client.network.participate_survey = PS_YES;
				this->Close();
				break;
		}
	}
};

static const NWidgetPart _nested_network_ask_survey_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_NAS_CAPTION), SetDataTip(STR_NETWORK_ASK_SURVEY_CAPTION, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_NAS_TEXT), SetAlignment(SA_HOR_CENTER), SetFill(1, 1),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NAS_PREVIEW), SetMinimalSize(71, 12), SetFill(1, 1), SetDataTip(STR_NETWORK_ASK_SURVEY_PREVIEW, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NAS_LINK), SetMinimalSize(71, 12), SetFill(1, 1), SetDataTip(STR_NETWORK_ASK_SURVEY_LINK, STR_NULL),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_NAS_NO), SetMinimalSize(71, 12), SetFill(1, 1), SetDataTip(STR_NETWORK_ASK_SURVEY_NO, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_NAS_YES), SetMinimalSize(71, 12), SetFill(1, 1), SetDataTip(STR_NETWORK_ASK_SURVEY_YES, STR_NULL),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _network_ask_survey_desc(__FILE__, __LINE__,
	WDP_CENTER, nullptr, 0, 0,
	WC_NETWORK_ASK_SURVEY, WC_NONE,
	WDF_MODAL,
	std::begin(_nested_network_ask_survey_widgets), std::end(_nested_network_ask_survey_widgets)
);

/**
 * Show a modal confirmation window with "no" / "preview" / "yes" buttons.
 */
void ShowNetworkAskSurvey()
{
	/* If we can't send a survey, don't ask the question. */
	if constexpr (!NetworkSurveyHandler::IsSurveyPossible()) return;

	CloseWindowByClass(WC_NETWORK_ASK_SURVEY);

	Window *parent = GetMainWindow();
	new NetworkAskSurveyWindow(&_network_ask_survey_desc, parent);
}

/** Window for displaying the textfile of a survey result. */
struct SurveyResultTextfileWindow : public TextfileWindow {
	const GRFConfig *grf_config; ///< View the textfile of this GRFConfig.

	SurveyResultTextfileWindow(TextfileType file_type) : TextfileWindow(file_type)
	{
		auto result = _survey.CreatePayload(NetworkSurveyHandler::Reason::PREVIEW, true);
		this->LoadText(result);
		this->InvalidateData();
	}
};

void ShowSurveyResultTextfileWindow()
{
	CloseWindowById(WC_TEXTFILE, TFT_SURVEY_RESULT);
	new SurveyResultTextfileWindow(TFT_SURVEY_RESULT);
}
