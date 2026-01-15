/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
#include "../dropdown_type.h"
#include "../dropdown_func.h"
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
#include "../stringfilter_type.h"
#include "../core/string_consumer.hpp"

#include "../widgets/network_widget.h"

#include "table/strings.h"
#include "../table/sprites.h"

#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#endif

#include "../safeguards.h"

static void ShowNetworkStartServerWindow();

static ClientID _admin_client_id = INVALID_CLIENT_ID; ///< For what client a confirmation window is open.
static CompanyID _admin_company_id = CompanyID::Invalid(); ///< For what company a confirmation window is open.

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

	list.push_back(MakeDropDownListStringItem(STR_NETWORK_SERVER_VISIBILITY_LOCAL, SERVER_GAME_TYPE_LOCAL));
	list.push_back(MakeDropDownListStringItem(STR_NETWORK_SERVER_VISIBILITY_INVITE_ONLY, SERVER_GAME_TYPE_INVITE_ONLY));
	list.push_back(MakeDropDownListStringItem(STR_NETWORK_SERVER_VISIBILITY_PUBLIC, SERVER_GAME_TYPE_PUBLIC));

	return list;
}

typedef GUIList<NetworkGame*, std::nullptr_t, StringFilter&> GUIGameServerList;
typedef int ServerListPosition;
static const ServerListPosition SLP_INVALID = -1;

/** Full blown container to make it behave exactly as we want :) */
class NWidgetServerListHeader : public NWidgetContainer {
	static const uint MINIMUM_NAME_WIDTH_BEFORE_NEW_HEADER = 150; ///< Minimum width before adding a new header
public:
	NWidgetServerListHeader() : NWidgetContainer(NWID_HORIZONTAL)
	{
		auto leaf = std::make_unique<NWidgetLeaf>(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_NAME, WidgetData{.string = STR_NETWORK_SERVER_LIST_GAME_NAME}, STR_NETWORK_SERVER_LIST_GAME_NAME_TOOLTIP);
		leaf->SetResize(1, 0);
		leaf->SetFill(1, 0);
		this->Add(std::move(leaf));

		this->Add(std::make_unique<NWidgetLeaf>(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_CLIENTS, WidgetData{.string = STR_NETWORK_SERVER_LIST_CLIENTS_CAPTION}, STR_NETWORK_SERVER_LIST_CLIENTS_CAPTION_TOOLTIP));
		this->Add(std::make_unique<NWidgetLeaf>(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_MAPSIZE, WidgetData{.string = STR_NETWORK_SERVER_LIST_MAP_SIZE_CAPTION}, STR_NETWORK_SERVER_LIST_MAP_SIZE_CAPTION_TOOLTIP));
		this->Add(std::make_unique<NWidgetLeaf>(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_DATE, WidgetData{.string = STR_NETWORK_SERVER_LIST_DATE_CAPTION}, STR_NETWORK_SERVER_LIST_DATE_CAPTION_TOOLTIP));
		this->Add(std::make_unique<NWidgetLeaf>(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_YEARS, WidgetData{.string = STR_NETWORK_SERVER_LIST_PLAY_TIME_CAPTION}, STR_NETWORK_SERVER_LIST_PLAY_TIME_CAPTION_TOOLTIP));

		leaf = std::make_unique<NWidgetLeaf>(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_INFO, WidgetData{.string = STR_EMPTY}, STR_NETWORK_SERVER_LIST_INFO_ICONS_TOOLTIP);
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
		this->ApplyAspectRatio();
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
	static const std::initializer_list<GUIGameServerList::SortFunction * const> sorter_funcs;
	static const std::initializer_list<GUIGameServerList::FilterFunction * const> filter_funcs;

	NetworkGame *server = nullptr; ///< Selected server.
	NetworkGame *last_joined = nullptr; ///< The last joined server.
	GUIGameServerList servers{}; ///< List with game servers.
	ServerListPosition list_pos = SLP_INVALID; ///< Position of the selected server.
	Scrollbar *vscroll = nullptr; ///< Vertical scrollbar of the list of servers.
	QueryString name_editbox; ///< Client name editbox.
	QueryString filter_editbox; ///< Editbox for filter on servers.
	bool searched_internet = false; ///< Did we ever press "Search Internet" button?

	Dimension lock{}; /// Dimension of lock icon.
	Dimension blot{}; /// Dimension of compatibility icon.

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
		for (const auto &ngl : _network_game_list) {
			this->servers.push_back(ngl.get());
			if (ngl.get() == this->server) {
				found_current_server = true;
			}
			if (ngl.get() == this->last_joined) {
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
		sf.SetFilterTerm(this->filter_editbox.text.GetText());

		if (!sf.IsEmpty()) {
			this->servers.SetFilterState(true);
			this->servers.Filter(sf);
		} else {
			this->servers.SetFilterState(false);
		}

		this->servers.RebuildDone();
		this->vscroll->SetCount(this->servers.size());

		/* Sort the list of network games as requested. */
		this->servers.Sort();
		this->UpdateListPos();
	}

	/** Sort servers by name. */
	static bool NGameNameSorter(NetworkGame * const &a, NetworkGame * const &b)
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
	static bool NGameClientSorter(NetworkGame * const &a, NetworkGame * const &b)
	{
		/* Reverse as per default we are interested in most-clients first */
		int r = a->info.clients_on - b->info.clients_on;

		if (r == 0) r = a->info.clients_max - b->info.clients_max;
		if (r == 0) return NGameNameSorter(a, b);

		return r < 0;
	}

	/** Sort servers by map size */
	static bool NGameMapSizeSorter(NetworkGame * const &a, NetworkGame * const &b)
	{
		/* Sort by the area of the map. */
		int r = (a->info.map_height) * (a->info.map_width) - (b->info.map_height) * (b->info.map_width);

		if (r == 0) r = a->info.map_width - b->info.map_width;
		return (r != 0) ? r < 0 : NGameClientSorter(a, b);
	}

	/** Sort servers by calendar date. */
	static bool NGameCalendarDateSorter(NetworkGame * const &a, NetworkGame * const &b)
	{
		auto r = a->info.calendar_date - b->info.calendar_date;
		return (r != 0) ? r < 0 : NGameClientSorter(a, b);
	}

	/** Sort servers by the number of ticks the game is running. */
	static bool NGameTicksPlayingSorter(NetworkGame * const &a, NetworkGame * const &b)
	{
		if (a->info.ticks_playing == b->info.ticks_playing) {
			return NGameClientSorter(a, b);
		}
		return a->info.ticks_playing < b->info.ticks_playing;
	}

	/**
	 * Sort servers by joinability. If both servers are the
	 * same, prefer the non-passworded server first.
	 */
	static bool NGameAllowedSorter(NetworkGame * const &a, NetworkGame * const &b)
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
		auto it = std::ranges::find(this->servers, this->server);
		if (it == std::end(this->servers)) {
			this->list_pos = SLP_INVALID;
		} else {
			this->list_pos = static_cast<ServerListPosition>(std::distance(std::begin(this->servers), it));
		}
	}

	static bool NGameSearchFilter(NetworkGame * const *item, StringFilter &sf)
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
	void DrawServerLine(const NetworkGame *cur_item, int y, bool highlight) const
	{
		Rect name = this->GetWidget<NWidgetBase>(WID_NG_NAME)->GetCurrentRect();
		Rect info = this->GetWidget<NWidgetBase>(WID_NG_INFO)->GetCurrentRect();

		/* show highlighted item with a different colour */
		if (highlight) {
			Rect r = {std::min(name.left, info.left), y, std::max(name.right, info.right), y + (int)this->resize.step_height - 1};
			GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), PC_GREY);
		}

		/* Offset to vertically position text. */
		int text_y_offset = WidgetDimensions::scaled.matrix.top + (this->resize.step_height - WidgetDimensions::scaled.matrix.Vertical() - GetCharacterHeight(FS_NORMAL)) / 2;

		info = info.Shrink(WidgetDimensions::scaled.framerect);
		name = name.Shrink(WidgetDimensions::scaled.framerect);
		DrawString(name.left, name.right, y + text_y_offset, cur_item->info.server_name, TC_BLACK);

		/* only draw details if the server is online */
		if (cur_item->status == NGLS_ONLINE) {
			if (const NWidgetBase *nwid = this->GetWidget<NWidgetBase>(WID_NG_CLIENTS); nwid->current_x != 0) {
				Rect clients = nwid->GetCurrentRect();
				DrawString(clients.left, clients.right, y + text_y_offset,
					GetString(STR_NETWORK_SERVER_LIST_GENERAL_ONLINE, cur_item->info.clients_on, cur_item->info.clients_max, cur_item->info.companies_on, cur_item->info.companies_max),
					TC_FROMSTRING, SA_HOR_CENTER);
			}

			if (const NWidgetBase *nwid = this->GetWidget<NWidgetBase>(WID_NG_MAPSIZE); nwid->current_x != 0) {
				/* map size */
				Rect mapsize = nwid->GetCurrentRect();
				DrawString(mapsize.left, mapsize.right, y + text_y_offset,
					GetString(STR_NETWORK_SERVER_LIST_MAP_SIZE_SHORT, cur_item->info.map_width, cur_item->info.map_height),
					TC_FROMSTRING, SA_HOR_CENTER);
			}

			if (const NWidgetBase *nwid = this->GetWidget<NWidgetBase>(WID_NG_DATE); nwid->current_x != 0) {
				/* current date */
				Rect date = nwid->GetCurrentRect();
				TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(cur_item->info.calendar_date);
				DrawString(date.left, date.right, y + text_y_offset,
					GetString(STR_JUST_INT, ymd.year),
					TC_BLACK, SA_HOR_CENTER);
			}

			if (const NWidgetBase *nwid = this->GetWidget<NWidgetBase>(WID_NG_YEARS); nwid->current_x != 0) {
				/* play time */
				Rect years = nwid->GetCurrentRect();
				const auto play_time = cur_item->info.ticks_playing / Ticks::TICKS_PER_SECOND;
				DrawString(years.left, years.right, y + text_y_offset,
					GetString(STR_NETWORK_SERVER_LIST_PLAY_TIME_SHORT, play_time / 60 / 60, (play_time / 60) % 60),
					TC_BLACK, SA_HOR_CENTER);
			}

			/* Set top and bottom of info rect to current row. */
			info.top = y;
			info.bottom = y + this->resize.step_height - 1;

			bool rtl = _current_text_dir == TD_RTL;

			/* draw a lock if the server is password protected */
			if (cur_item->info.use_password) DrawSpriteIgnorePadding(SPR_LOCK, PAL_NONE, info.WithWidth(this->lock.width, rtl), SA_CENTER);

			/* draw red or green icon, depending on compatibility with server */
			PaletteID pal = cur_item->info.compatible ? PALETTE_TO_GREEN : (cur_item->info.version_compatible ? PALETTE_TO_YELLOW : PALETTE_TO_RED);
			DrawSpriteIgnorePadding(SPR_BLOT, pal, info.WithWidth(this->blot.width, !rtl), SA_CENTER);
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
	NetworkGameWindow(WindowDesc &desc) : Window(desc), name_editbox(NETWORK_CLIENT_NAME_LENGTH), filter_editbox(120)
	{
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
		this->servers.SetSortFuncs(NetworkGameWindow::sorter_funcs);
		this->servers.SetFilterFuncs(NetworkGameWindow::filter_funcs);
		this->servers.ForceRebuild();
	}

	~NetworkGameWindow() override
	{
		this->last_sorting = this->servers.GetListing();
	}

	void OnInit() override
	{
		this->lock = GetScaledSpriteSize(SPR_LOCK);
		this->blot = GetScaledSpriteSize(SPR_BLOT);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_NG_MATRIX:
				fill.height = resize.height = std::max<uint>(this->blot.height, GetCharacterHeight(FS_NORMAL)) + padding.height;
				size.height = 12 * resize.height;
				break;

			case WID_NG_LASTJOINED:
				size.height = std::max<uint>(this->blot.height, GetCharacterHeight(FS_NORMAL)) + WidgetDimensions::scaled.matrix.Vertical();
				break;

			case WID_NG_LASTJOINED_SPACER:
				size.width = NWidgetScrollbar::GetVerticalDimension().width;
				break;

			case WID_NG_NAME:
				size.width += 2 * Window::SortButtonWidth(); // Make space for the arrow
				break;

			case WID_NG_CLIENTS: {
				size.width += 2 * Window::SortButtonWidth(); // Make space for the arrow
				auto max_clients = GetParamMaxValue(MAX_CLIENTS);
				auto max_companies = GetParamMaxValue(MAX_COMPANIES);
				size = maxdim(size, GetStringBoundingBox(GetString(STR_NETWORK_SERVER_LIST_GENERAL_ONLINE, max_clients, max_clients, max_companies, max_companies)));
				break;
			}

			case WID_NG_MAPSIZE: {
				size.width += 2 * Window::SortButtonWidth(); // Make space for the arrow
				auto max_map_size = GetParamMaxValue(MAX_MAP_SIZE);
				size = maxdim(size, GetStringBoundingBox(GetString(STR_NETWORK_SERVER_LIST_MAP_SIZE_SHORT, max_map_size, max_map_size)));
				break;
			}

			case WID_NG_DATE:
			case WID_NG_YEARS:
				size.width += 2 * Window::SortButtonWidth(); // Make space for the arrow
				size = maxdim(size, GetStringBoundingBox(GetString(STR_JUST_INT, GetParamMaxValue(5))));
				break;

			case WID_NG_INFO:
				size.width = this->lock.width + WidgetDimensions::scaled.hsep_normal + this->blot.width + padding.width;
				size.height = std::max(this->lock.height, this->blot.height) + padding.height;
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_NG_MATRIX: {
				uint16_t y = r.top;

				auto [first, last] = this->vscroll->GetVisibleRangeIterators(this->servers);
				for (auto it = first; it != last; ++it) {
					const NetworkGame *ngl = *it;
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

		NetworkGame *sel = this->server;
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
		changed |= this->GetWidget<NWidgetStacked>(WID_NG_NEWGRF_SEL)->SetDisplayedPlane(sel == nullptr || sel->status != NGLS_ONLINE || sel->info.grfconfig.empty() ? SZSP_NONE : 0);
		changed |= this->GetWidget<NWidgetStacked>(WID_NG_NEWGRF_MISSING_SEL)->SetDisplayedPlane(sel == nullptr || sel->status != NGLS_ONLINE || sel->info.grfconfig.empty() || !sel->info.version_compatible || sel->info.compatible ? SZSP_NONE : 0);
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
		NetworkGame *sel = this->server;

		Rect tr = r.Shrink(WidgetDimensions::scaled.frametext);
		StringID header_msg = this->GetHeaderString();
		int header_height = GetStringHeight(header_msg, tr.Width()) +
				(sel == nullptr ? 0 : GetStringHeight(sel->info.server_name, tr.Width())) +
				WidgetDimensions::scaled.frametext.Vertical();

		/* Height for the title banner */
		Rect hr = r.WithHeight(header_height).Shrink(WidgetDimensions::scaled.frametext);
		tr.top += header_height;

		/* Draw the right menu */
		/* Create the nice darker rectangle at the details top */
		GfxFillRect(r.WithHeight(header_height).Shrink(WidgetDimensions::scaled.bevel), GetColourGradient(COLOUR_LIGHT_BLUE, SHADE_NORMAL));
		hr.top = DrawStringMultiLine(hr, header_msg, TC_FROMSTRING, SA_HOR_CENTER);
		if (sel == nullptr) return;

		hr.top = DrawStringMultiLine(hr, sel->info.server_name, TC_ORANGE, SA_HOR_CENTER); // game name
		if (sel->status != NGLS_ONLINE) {
			tr.top = DrawStringMultiLine(tr, header_msg, TC_FROMSTRING, SA_HOR_CENTER);
		} else { // show game info
			tr.top = DrawStringMultiLine(tr, GetString(STR_NETWORK_SERVER_LIST_CLIENTS, sel->info.clients_on, sel->info.clients_max, sel->info.companies_on, sel->info.companies_max));

			tr.top = DrawStringMultiLine(tr, GetString(STR_NETWORK_SERVER_LIST_LANDSCAPE, STR_CLIMATE_TEMPERATE_LANDSCAPE + to_underlying(sel->info.landscape))); // landscape

			tr.top = DrawStringMultiLine(tr, GetString(STR_NETWORK_SERVER_LIST_MAP_SIZE, sel->info.map_width, sel->info.map_height)); // map size

			tr.top = DrawStringMultiLine(tr, GetString(STR_NETWORK_SERVER_LIST_SERVER_VERSION, sel->info.server_revision)); // server version

			StringID invite_or_address = sel->connection_string.starts_with("+") ? STR_NETWORK_SERVER_LIST_INVITE_CODE : STR_NETWORK_SERVER_LIST_SERVER_ADDRESS;
			tr.top = DrawStringMultiLine(tr, GetString(invite_or_address, sel->connection_string)); // server address / invite code

			tr.top = DrawStringMultiLine(tr, GetString(STR_NETWORK_SERVER_LIST_START_DATE, sel->info.calendar_start)); // start date

			tr.top = DrawStringMultiLine(tr, GetString(STR_NETWORK_SERVER_LIST_CURRENT_DATE, sel->info.calendar_date)); // current date

			const auto play_time = sel->info.ticks_playing / Ticks::TICKS_PER_SECOND;
			tr.top = DrawStringMultiLine(tr, GetString(STR_NETWORK_SERVER_LIST_PLAY_TIME, play_time / 60 / 60, (play_time / 60) % 60)); // play time

			if (sel->info.gamescript_version != -1) {
				tr.top = DrawStringMultiLine(tr, GetString(STR_NETWORK_SERVER_LIST_GAMESCRIPT, sel->info.gamescript_name, sel->info.gamescript_version)); // gamescript name and version
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
				ShowQueryString(
					_settings_client.network.connect_to_ip,
					STR_NETWORK_SERVER_LIST_ENTER_SERVER_ADDRESS,
					NETWORK_HOSTNAME_PORT_LENGTH,  // maximum number of characters including '\0'
					this, CS_ALPHANUMERAL, QueryStringFlag::AcceptUnchanged);
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
				if (this->server != nullptr) ShowNewGRFSettings(false, false, false, this->server->info.grfconfig);
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
				_settings_client.network.client_name = this->name_editbox.text.GetText();
				break;
		}
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value() || str->empty()) return;

		_settings_client.network.connect_to_ip = std::move(*str);
		NetworkAddServer(_settings_client.network.connect_to_ip);
		NetworkRebuildHostList();
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_NG_MATRIX);
	}

	/** Refresh the online servers on a regular interval. */
	const IntervalTimer<TimerWindow> refresh_interval = {std::chrono::seconds(30), [this](uint) {
		if (!this->searched_internet) return;

		_network_coordinator_client.GetListing();
	}};
};

Listing NetworkGameWindow::last_sorting = {false, 5};
const std::initializer_list<GUIGameServerList::SortFunction * const> NetworkGameWindow::sorter_funcs = {
	&NGameNameSorter,
	&NGameClientSorter,
	&NGameMapSizeSorter,
	&NGameCalendarDateSorter,
	&NGameTicksPlayingSorter,
	&NGameAllowedSorter
};

const std::initializer_list<GUIGameServerList::FilterFunction * const> NetworkGameWindow::filter_funcs = {
	&NGameSearchFilter
};

static std::unique_ptr<NWidgetBase> MakeResizableHeader()
{
	return std::make_unique<NWidgetServerListHeader>();
}

static constexpr std::initializer_list<NWidgetPart> _nested_network_game_widgets = {
	/* TOP */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE), SetStringTip(STR_NETWORK_SERVER_LIST_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_LIGHT_BLUE),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, WID_NG_MAIN),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse_resize),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				/* LEFT SIDE */
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0),
					NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
						NWidget(WWT_TEXT, INVALID_COLOUR, WID_NG_FILTER_LABEL), SetStringTip(STR_LIST_FILTER_TITLE),
						NWidget(WWT_EDITBOX, COLOUR_LIGHT_BLUE, WID_NG_FILTER), SetMinimalSize(251, 0), SetFill(1, 0), SetResize(1, 0),
											SetStringTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
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
						NWidget(WWT_TEXT, INVALID_COLOUR, WID_NG_LASTJOINED_LABEL), SetFill(1, 0),
											SetStringTip(STR_NETWORK_SERVER_LIST_LAST_JOINED_SERVER), SetResize(1, 0),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, WID_NG_LASTJOINED), SetFill(1, 0), SetResize(1, 0),
												SetToolTip(STR_NETWORK_SERVER_LIST_CLICK_TO_SELECT_LAST_TOOLTIP),
							EndContainer(),
							NWidget(WWT_EMPTY, INVALID_COLOUR, WID_NG_LASTJOINED_SPACER), SetFill(0, 0),
						EndContainer(),
					EndContainer(),
				EndContainer(),
				/* RIGHT SIDE */
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0),
					NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
						NWidget(WWT_TEXT, INVALID_COLOUR, WID_NG_CLIENT_LABEL), SetStringTip(STR_NETWORK_SERVER_LIST_PLAYER_NAME),
						NWidget(WWT_EDITBOX, COLOUR_LIGHT_BLUE, WID_NG_CLIENT), SetMinimalSize(151, 0), SetFill(1, 0), SetResize(1, 0),
											SetStringTip(STR_NETWORK_SERVER_LIST_PLAYER_NAME_OSKTITLE, STR_NETWORK_SERVER_LIST_ENTER_NAME_TOOLTIP),
					EndContainer(),
					NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
						NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, WID_NG_DETAILS), SetMinimalSize(140, 0), SetMinimalTextLines(15, 0), SetResize(0, 1),
						EndContainer(),
						NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize),
							NWidget(NWID_SELECTION, INVALID_COLOUR, WID_NG_NEWGRF_MISSING_SEL),
								NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_NEWGRF_MISSING), SetFill(1, 0), SetStringTip(STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_BUTTON, STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_TOOLTIP),
							EndContainer(),
							NWidget(NWID_SELECTION, INVALID_COLOUR, WID_NG_NEWGRF_SEL),
								NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_NEWGRF), SetFill(1, 0), SetStringTip(STR_MAPGEN_NEWGRF_SETTINGS, STR_MAPGEN_NEWGRF_SETTINGS_TOOLTIP),
							EndContainer(),
							NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
								NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_JOIN), SetFill(1, 0), SetStringTip(STR_NETWORK_SERVER_LIST_JOIN_GAME),
								NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_REFRESH), SetFill(1, 0), SetStringTip(STR_NETWORK_SERVER_LIST_REFRESH, STR_NETWORK_SERVER_LIST_REFRESH_TOOLTIP),
							EndContainer(),
						EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),
			/* BOTTOM */
			NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_SEARCH_INTERNET), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_NETWORK_SERVER_LIST_SEARCH_SERVER_INTERNET, STR_NETWORK_SERVER_LIST_SEARCH_SERVER_INTERNET_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_SEARCH_LAN), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_NETWORK_SERVER_LIST_SEARCH_SERVER_LAN, STR_NETWORK_SERVER_LIST_SEARCH_SERVER_LAN_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_ADD), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_NETWORK_SERVER_LIST_ADD_SERVER, STR_NETWORK_SERVER_LIST_ADD_SERVER_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NG_START), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_NETWORK_SERVER_LIST_START_SERVER, STR_NETWORK_SERVER_LIST_START_SERVER_TOOLTIP),
			EndContainer(),
		EndContainer(),
		/* Resize button. */
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_RESIZEBOX, COLOUR_LIGHT_BLUE), SetResizeWidgetTypeTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _network_game_window_desc(
	WDP_CENTER, "list_servers", 1000, 730,
	WC_NETWORK_WINDOW, WC_NONE,
	{},
	_nested_network_game_widgets
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

	new NetworkGameWindow(_network_game_window_desc);
}

struct NetworkStartServerWindow : public Window {
	WidgetID widget_id{}; ///< The widget that has the pop-up input menu
	QueryString name_editbox; ///< Server name editbox.

	NetworkStartServerWindow(WindowDesc &desc) : Window(desc), name_editbox(NETWORK_NAME_LENGTH)
	{
		this->InitNested(WN_NETWORK_WINDOW_START);

		this->querystrings[WID_NSS_GAMENAME] = &this->name_editbox;
		this->name_editbox.text.Assign(_settings_client.network.server_name);

		this->SetFocusedWidget(WID_NSS_GAMENAME);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_NSS_CONNTYPE_BTN:
				return GetString(STR_NETWORK_SERVER_VISIBILITY_LOCAL + _settings_client.network.server_game_type);

			case WID_NSS_CLIENTS_TXT:
				return GetString(STR_NETWORK_START_SERVER_CLIENTS_SELECT, _settings_client.network.max_clients);

			case WID_NSS_COMPANIES_TXT:
				return GetString(STR_NETWORK_START_SERVER_COMPANIES_SELECT, _settings_client.network.max_companies);

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_NSS_CONNTYPE_BTN:
				size = maxdim(maxdim(GetStringBoundingBox(STR_NETWORK_SERVER_VISIBILITY_LOCAL), GetStringBoundingBox(STR_NETWORK_SERVER_VISIBILITY_PUBLIC)), GetStringBoundingBox(STR_NETWORK_SERVER_VISIBILITY_INVITE_ONLY));
				size.width += padding.width;
				size.height += padding.height;
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
				ShowQueryString(_settings_client.network.server_password, STR_NETWORK_START_SERVER_SET_PASSWORD, NETWORK_PASSWORD_LENGTH, this, CS_ALPHANUMERAL, {});
				break;

			case WID_NSS_CONNTYPE_BTN: // Connection type
				ShowDropDownList(this, BuildVisibilityDropDownList(), _settings_client.network.server_game_type, WID_NSS_CONNTYPE_BTN);
				break;

			case WID_NSS_CLIENTS_BTND:    case WID_NSS_CLIENTS_BTNU:    // Click on up/down button for number of clients
			case WID_NSS_COMPANIES_BTND:  case WID_NSS_COMPANIES_BTNU:  // Click on up/down button for number of companies
				/* Don't allow too fast scrolling. */
				if (!this->flags.Test(WindowFlag::Timeout) || this->timeout_timer <= 1) {
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
				ShowQueryString(GetString(STR_JUST_INT, _settings_client.network.max_clients), STR_NETWORK_START_SERVER_NUMBER_OF_CLIENTS,    4, this, CS_NUMERAL, {});
				break;

			case WID_NSS_COMPANIES_TXT:  // Click on number of companies
				this->widget_id = WID_NSS_COMPANIES_TXT;
				ShowQueryString(GetString(STR_JUST_INT, _settings_client.network.max_companies), STR_NETWORK_START_SERVER_NUMBER_OF_COMPANIES,  3, this, CS_NUMERAL, {});
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
				ShowSaveLoadDialog(FT_HEIGHTMAP, SLO_LOAD);
				break;
		}
	}

	void OnDropdownSelect(WidgetID widget, int index, int) override
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
		std::string str{this->name_editbox.text.GetText()};
		if (!NetworkValidateServerName(str)) return false;

		SetSettingValue(GetSettingFromName("network.server_name")->AsStringSetting(), std::move(str));
		return true;
	}

	void OnTimeout() override
	{
		this->RaiseWidgetsWhenLowered(WID_NSS_CLIENTS_BTND, WID_NSS_CLIENTS_BTNU, WID_NSS_COMPANIES_BTND, WID_NSS_COMPANIES_BTNU);
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value()) return;

		if (this->widget_id == WID_NSS_SETPWD) {
			_settings_client.network.server_password = std::move(*str);
		} else {
			auto value = ParseInteger<int32_t>(*str, 10, true);
			if (!value.has_value()) return;
			this->SetWidgetDirty(this->widget_id);
			switch (this->widget_id) {
				default: NOT_REACHED();
				case WID_NSS_CLIENTS_TXT:    _settings_client.network.max_clients    = Clamp(*value, 2, MAX_CLIENTS); break;
				case WID_NSS_COMPANIES_TXT:  _settings_client.network.max_companies  = Clamp(*value, 1, MAX_COMPANIES); break;
			}
		}

		this->SetDirty();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_network_start_server_window_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE), SetStringTip(STR_NETWORK_START_SERVER_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, WID_NSS_BACKGROUND),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
					/* Game name widgets */
					NWidget(WWT_TEXT, INVALID_COLOUR, WID_NSS_GAMENAME_LABEL), SetFill(1, 0), SetStringTip(STR_NETWORK_START_SERVER_NEW_GAME_NAME),
					NWidget(WWT_EDITBOX, COLOUR_LIGHT_BLUE, WID_NSS_GAMENAME), SetFill(1, 0), SetStringTip(STR_NETWORK_START_SERVER_NEW_GAME_NAME_OSKTITLE, STR_NETWORK_START_SERVER_NEW_GAME_NAME_TOOLTIP),
				EndContainer(),

				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
						NWidget(WWT_TEXT, INVALID_COLOUR, WID_NSS_CONNTYPE_LABEL), SetFill(1, 0), SetStringTip(STR_NETWORK_START_SERVER_VISIBILITY_LABEL),
						NWidget(WWT_DROPDOWN, COLOUR_LIGHT_BLUE, WID_NSS_CONNTYPE_BTN), SetFill(1, 0), SetToolTip(STR_NETWORK_START_SERVER_VISIBILITY_TOOLTIP),
					EndContainer(),
					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
						NWidget(NWID_SPACER), SetFill(1, 1),
						NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NSS_SETPWD), SetFill(1, 0), SetStringTip(STR_NETWORK_START_SERVER_SET_PASSWORD, STR_NETWORK_START_SERVER_PASSWORD_TOOLTIP),
					EndContainer(),
				EndContainer(),

				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
						NWidget(WWT_TEXT, INVALID_COLOUR, WID_NSS_CLIENTS_LABEL), SetFill(1, 0), SetStringTip(STR_NETWORK_START_SERVER_NUMBER_OF_CLIENTS),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_IMGBTN, COLOUR_LIGHT_BLUE, WID_NSS_CLIENTS_BTND), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON), SetFill(0, 1), SetSpriteTip(SPR_ARROW_DOWN, STR_NETWORK_START_SERVER_NUMBER_OF_CLIENTS_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_NSS_CLIENTS_TXT), SetFill(1, 0), SetToolTip(STR_NETWORK_START_SERVER_NUMBER_OF_CLIENTS_TOOLTIP),
							NWidget(WWT_IMGBTN, COLOUR_LIGHT_BLUE, WID_NSS_CLIENTS_BTNU), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON), SetFill(0, 1), SetSpriteTip(SPR_ARROW_UP, STR_NETWORK_START_SERVER_NUMBER_OF_CLIENTS_TOOLTIP),
						EndContainer(),
					EndContainer(),

					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
						NWidget(WWT_TEXT, INVALID_COLOUR, WID_NSS_COMPANIES_LABEL), SetFill(1, 0), SetStringTip(STR_NETWORK_START_SERVER_NUMBER_OF_COMPANIES),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_IMGBTN, COLOUR_LIGHT_BLUE, WID_NSS_COMPANIES_BTND), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON), SetFill(0, 1), SetSpriteTip(SPR_ARROW_DOWN, STR_NETWORK_START_SERVER_NUMBER_OF_COMPANIES_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_NSS_COMPANIES_TXT), SetFill(1, 0), SetToolTip(STR_NETWORK_START_SERVER_NUMBER_OF_COMPANIES_TOOLTIP),
							NWidget(WWT_IMGBTN, COLOUR_LIGHT_BLUE, WID_NSS_COMPANIES_BTNU), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON), SetFill(0, 1), SetSpriteTip(SPR_ARROW_UP, STR_NETWORK_START_SERVER_NUMBER_OF_COMPANIES_TOOLTIP),
						EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),

			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
				/* 'generate game' and 'load game' buttons */
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NSS_GENERATE_GAME), SetStringTip(STR_INTRO_NEW_GAME, STR_INTRO_TOOLTIP_NEW_GAME), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NSS_LOAD_GAME), SetStringTip(STR_INTRO_LOAD_GAME, STR_INTRO_TOOLTIP_LOAD_GAME), SetFill(1, 0),
				EndContainer(),

				/* 'play scenario' and 'play heightmap' buttons */
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NSS_PLAY_SCENARIO), SetStringTip(STR_INTRO_PLAY_SCENARIO, STR_INTRO_TOOLTIP_PLAY_SCENARIO), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NSS_PLAY_HEIGHTMAP), SetStringTip(STR_INTRO_PLAY_HEIGHTMAP, STR_INTRO_TOOLTIP_PLAY_HEIGHTMAP), SetFill(1, 0),
				EndContainer(),
			EndContainer(),

			NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NSS_CANCEL), SetStringTip(STR_BUTTON_CANCEL), SetMinimalSize(128, 12),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _network_start_server_window_desc(
	WDP_CENTER, {}, 0, 0,
	WC_NETWORK_WINDOW, WC_NONE,
	{},
	_nested_network_start_server_window_widgets
);

static void ShowNetworkStartServerWindow()
{
	if (!NetworkValidateOurClientName()) return;

	CloseWindowById(WC_NETWORK_WINDOW, WN_NETWORK_WINDOW_GAME);

	new NetworkStartServerWindow(_network_start_server_window_desc);
}

/* The window below gives information about the connected clients
 * and also makes able to kick them (if server) and stuff like that. */

extern void DrawCompanyIcon(CompanyID cid, int x, int y);

static constexpr std::initializer_list<NWidgetPart> _nested_client_list_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetStringTip(STR_NETWORK_CLIENT_LIST_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0), SetPadding(4),
			NWidget(WWT_FRAME, COLOUR_GREY), SetStringTip(STR_NETWORK_CLIENT_LIST_SERVER), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
					NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_NETWORK_CLIENT_LIST_SERVER_NAME),
					NWidget(WWT_TEXT, INVALID_COLOUR, WID_CL_SERVER_NAME), SetFill(1, 0), SetResize(1, 0), SetToolTip(STR_NETWORK_CLIENT_LIST_SERVER_NAME_TOOLTIP), SetAlignment(SA_VERT_CENTER | SA_RIGHT),
					NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_CL_SERVER_NAME_EDIT), SetAspect(WidgetDimensions::ASPECT_RENAME), SetSpriteTip(SPR_RENAME, STR_NETWORK_CLIENT_LIST_SERVER_NAME_EDIT_TOOLTIP),
				EndContainer(),
				NWidget(NWID_SELECTION, INVALID_COLOUR, WID_CL_SERVER_SELECTOR),
					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_NETWORK_CLIENT_LIST_SERVER_VISIBILITY),
							NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_CL_SERVER_VISIBILITY), SetToolTip(STR_NETWORK_CLIENT_LIST_SERVER_VISIBILITY_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_NETWORK_CLIENT_LIST_SERVER_INVITE_CODE),
							NWidget(WWT_TEXT, INVALID_COLOUR, WID_CL_SERVER_INVITE_CODE), SetFill(1, 0), SetResize(1, 0), SetToolTip(STR_NETWORK_CLIENT_LIST_SERVER_INVITE_CODE_TOOLTIP), SetAlignment(SA_VERT_CENTER | SA_RIGHT),
						EndContainer(),
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_NETWORK_CLIENT_LIST_SERVER_CONNECTION_TYPE),
							NWidget(WWT_TEXT, INVALID_COLOUR, WID_CL_SERVER_CONNECTION_TYPE), SetFill(1, 0), SetResize(1, 0), SetToolTip(STR_NETWORK_CLIENT_LIST_SERVER_CONNECTION_TYPE_TOOLTIP), SetAlignment(SA_VERT_CENTER | SA_RIGHT),
						EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_FRAME, COLOUR_GREY), SetStringTip(STR_NETWORK_CLIENT_LIST_PLAYER),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
					NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_NETWORK_CLIENT_LIST_PLAYER_NAME),
					NWidget(WWT_TEXT, INVALID_COLOUR, WID_CL_CLIENT_NAME), SetFill(1, 0), SetResize(1, 0), SetToolTip(STR_NETWORK_CLIENT_LIST_PLAYER_NAME_TOOLTIP), SetAlignment(SA_VERT_CENTER | SA_RIGHT),
					NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_CL_CLIENT_NAME_EDIT), SetAspect(WidgetDimensions::ASPECT_RENAME), SetSpriteTip(SPR_RENAME, STR_NETWORK_CLIENT_LIST_PLAYER_NAME_EDIT_TOOLTIP),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_CL_MATRIX), SetMinimalSize(180, 0), SetResize(1, 1), SetFill(1, 1), SetMatrixDataTip(1, 0), SetScrollbar(WID_CL_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_CL_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_TEXT, INVALID_COLOUR, WID_CL_CLIENT_COMPANY_COUNT), SetFill(1, 0), SetResize(1, 0), SetPadding(WidgetDimensions::unscaled.framerect), SetAlignment(SA_CENTER), SetToolTip(STR_NETWORK_CLIENT_LIST_CLIENT_COMPANY_COUNT_TOOLTIP),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static WindowDesc _client_list_desc(
	WDP_AUTO, "list_clients", 220, 300,
	WC_CLIENT_LIST, WC_NONE,
	{},
	_nested_client_list_widgets
);

/**
 * The possibly entries in a DropDown for an admin.
 * Client and companies are mixed; they just have to be unique.
 */
enum DropDownAdmin : uint8_t {
	DD_CLIENT_ADMIN_KICK,
	DD_CLIENT_ADMIN_BAN,
	DD_COMPANY_ADMIN_RESET,
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
		Command<Commands::CompanyControl>::Post(CCA_DELETE, _admin_company_id, CRR_MANUAL, INVALID_CLIENT_ID);
	}
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
		Dimension d = GetScaledSpriteSize(sprite);
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
template <typename T>
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
 * Base interface for a network client list line.
 */
class ButtonLine {
public:
	std::vector<std::unique_ptr<ButtonCommon>> buttons{}; ///< Buttons for this line.

	virtual ~ButtonLine() = default;

	/**
	 * Draw the button line.
	 * @param r Rect to draw within.
	 */
	virtual void Draw(Rect r) const = 0;

	template <typename T, typename...TArgs>
	T &AddButton(TArgs &&... args)
	{
		auto &button = this->buttons.emplace_back(std::make_unique<T>(std::forward<TArgs &&>(args)...));
		return static_cast<T &>(*button);
	}

	/**
	 * Get the button at a given point on the line.
	 * @param r Rect of line.
	 * @param pt Point of interest.
	 * @return Button at point, or \c nullptr if button isn't pressed.
	 */
	ButtonCommon *GetButton(Rect r, const Point &pt) const
	{
		bool rtl = _current_text_dir == TD_RTL;
		for (auto &button : this->buttons) {
			if (r.WithWidth(button->width, !rtl).Contains(pt)) return button.get();
			r = r.Indent(button->width + WidgetDimensions::scaled.hsep_normal, !rtl);
		}
		return nullptr;
	}

	/**
	 * Get tooptip for a given point on the line.
	 * @param r Rect of line.
	 * @param pt Point of interest.
	 * @return EncodedString of tooltip, or \c std::nullopt if none.
	 */
	virtual std::optional<EncodedString> GetTooltip(Rect r, const Point &pt) const
	{
		ButtonCommon *button = this->GetButton(r, pt);
		if (button == nullptr) return {};
		return GetEncodedString(button->tooltip);
	}

protected:
	/**
	 * Draw the buttons for this line.
	 * @param r Rect to draw within.
	 * @return Rect of remaining space.
	 */
	Rect DrawButtons(Rect r) const
	{
		bool rtl = _current_text_dir == TD_RTL;
		for (auto &button : buttons) {
			Rect br = r.CentreToHeight(button->height).WithWidth(button->width, !rtl);
			DrawFrameRect(br, button->colour, {});
			DrawSpriteIgnorePadding(button->sprite, PAL_NONE, br, SA_CENTER);
			if (button->disabled) {
				GfxFillRect(br.Shrink(WidgetDimensions::scaled.bevel), GetColourGradient(button->colour, SHADE_DARKER), FILLRECT_CHECKER);
			}
			r = r.Indent(button->width + WidgetDimensions::scaled.hsep_normal, !rtl);
		}
		return r;
	}
};

class CompanyButtonLine : public ButtonLine {
public:
	CompanyButtonLine(CompanyID company_id) : company_id(company_id) {}

	void Draw(Rect r) const override
	{
		bool rtl = _current_text_dir == TD_RTL;
		r = this->DrawButtons(r);

		Dimension d = GetScaledSpriteSize(SPR_COMPANY_ICON);
		PaletteID pal = Company::IsValidID(this->company_id) ? GetCompanyPalette(this->company_id) : PALETTE_TO_GREY;
		DrawSpriteIgnorePadding(SPR_COMPANY_ICON, pal, r.WithWidth(d.width, rtl), SA_CENTER);

		Rect tr = r.CentreToHeight(GetCharacterHeight(FS_NORMAL)).Indent(d.width + WidgetDimensions::scaled.hsep_normal, rtl);
		if (this->company_id == COMPANY_SPECTATOR) {
			DrawString(tr, STR_NETWORK_CLIENT_LIST_SPECTATORS, TC_SILVER);
		} else if (this->company_id == COMPANY_NEW_COMPANY) {
			DrawString(tr, STR_NETWORK_CLIENT_LIST_NEW_COMPANY, TC_WHITE);
		} else {
			DrawString(tr, GetString(STR_COMPANY_NAME, this->company_id, this->company_id), TC_SILVER);
		}
	};

private:
	CompanyID company_id;
};

class ClientButtonLine : public ButtonLine {
public:
	ClientButtonLine(ClientPoolID client_pool_id) : client_pool_id(client_pool_id) {}

	void Draw(Rect r) const override
	{
		const NetworkClientInfo *ci = NetworkClientInfo::GetIfValid(this->client_pool_id);
		if (ci == nullptr) return;

		bool rtl = _current_text_dir == TD_RTL;
		r = this->DrawButtons(r);

		Rect tr = r.CentreToHeight(GetCharacterHeight(FS_NORMAL));

		SpriteID player_icon = 0;
		if (ci->client_id == _network_own_client_id) {
			player_icon = SPR_PLAYER_SELF;
		} else if (ci->client_id == CLIENT_ID_SERVER) {
			player_icon = SPR_PLAYER_HOST;
		}

		if (player_icon != 0) {
			Dimension d = GetScaledSpriteSize(player_icon);
			DrawSpriteIgnorePadding(player_icon, PALETTE_TO_GREY, r.WithWidth(d.width, rtl), SA_CENTER);
			tr = tr.Indent(d.width + WidgetDimensions::scaled.hsep_normal, rtl);
		}

		DrawString(tr, GetString(STR_JUST_RAW_STRING, ci->client_name), TC_BLACK);
	}

	std::optional<EncodedString> GetTooltip(Rect r, const Point &pt) const override
	{
		bool rtl = _current_text_dir == TD_RTL;
		Dimension d = GetScaledSpriteSize(SPR_PLAYER_SELF);

		if (r.WithWidth(d.width, rtl).Contains(pt)) {
			const NetworkClientInfo *ci = NetworkClientInfo::GetIfValid(this->client_pool_id);
			if (ci != nullptr) {
				if (ci->client_id == _network_own_client_id) {
					return GetEncodedString(STR_NETWORK_CLIENT_LIST_PLAYER_ICON_SELF_TOOLTIP);
				} else if (ci->client_id == CLIENT_ID_SERVER) {
					return GetEncodedString(STR_NETWORK_CLIENT_LIST_PLAYER_ICON_HOST_TOOLTIP);
				}
			}
		}

		return this->ButtonLine::GetTooltip(r, pt);
	}

private:
	ClientPoolID client_pool_id;
};

/**
 * Main handle for clientlist
 */
struct NetworkClientListWindow : Window {
private:
	ClientListWidgets query_widget{}; ///< During a query this tracks what widget caused the query.

	ClientID dd_client_id{}; ///< During admin dropdown, track which client this was for.
	CompanyID dd_company_id = CompanyID::Invalid(); ///< During admin dropdown, track which company this was for.

	Scrollbar *vscroll = nullptr; ///< Vertical scrollbar of this window.
	uint line_height = 0; ///< Current lineheight of each entry in the matrix.
	int hover_index = -1; ///< Index of the current line we are hovering over, or -1 if none.

	std::vector<std::unique_ptr<ButtonLine>> buttons{}; ///< Per line which buttons are available.

	/**
	 * Chat button on a Company is clicked.
	 * @param w The instance of this window.
	 * @param pt The point where this button was clicked.
	 * @param company_id The company this button was assigned to.
	 */
	static void OnClickCompanyChat([[maybe_unused]] NetworkClientListWindow *w, [[maybe_unused]] Point pt, CompanyID company_id)
	{
		ShowNetworkChatQueryWindow(DESTTYPE_TEAM, company_id.base());
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
		} else {
			NetworkClientRequestMove(company_id);
		}
	}

	/**
	 * Create new company button is clicked.
	 * @param w The instance of this window.
	 * @param pt The point where this button was clicked.
	 */
	static void OnClickCompanyNew([[maybe_unused]] NetworkClientListWindow *w, [[maybe_unused]] Point pt, CompanyID)
	{
		Command<Commands::CompanyControl>::Post(CCA_NEW, CompanyID::Invalid(), CRR_NONE, _network_own_client_id);
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
		list.push_back(MakeDropDownListStringItem(STR_NETWORK_CLIENT_LIST_ADMIN_CLIENT_KICK, DD_CLIENT_ADMIN_KICK));
		list.push_back(MakeDropDownListStringItem(STR_NETWORK_CLIENT_LIST_ADMIN_CLIENT_BAN, DD_CLIENT_ADMIN_BAN));

		Rect wi_rect;
		wi_rect.left   = pt.x;
		wi_rect.right  = pt.x;
		wi_rect.top    = pt.y;
		wi_rect.bottom = pt.y;

		w->dd_client_id = client_id;
		ShowDropDownListAt(w, std::move(list), -1, WID_CL_MATRIX, wi_rect, COLOUR_GREY, DropDownOption::InstantClose);
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
		list.push_back(MakeDropDownListStringItem(STR_NETWORK_CLIENT_LIST_ADMIN_COMPANY_RESET, DD_COMPANY_ADMIN_RESET, NetworkCompanyHasClients(company_id)));

		Rect wi_rect;
		wi_rect.left   = pt.x;
		wi_rect.right  = pt.x;
		wi_rect.top    = pt.y;
		wi_rect.bottom = pt.y;

		w->dd_company_id = company_id;
		ShowDropDownListAt(w, std::move(list), -1, WID_CL_MATRIX, wi_rect, COLOUR_GREY, DropDownOption::InstantClose);
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

	static void OnClickClientAuthorize([[maybe_unused]] NetworkClientListWindow *w, [[maybe_unused]] Point pt, ClientID client_id)
	{
		AutoRestoreBackup<CompanyID> cur_company(_current_company, NetworkClientInfo::GetByClientID(_network_own_client_id)->client_playas);
		Command<Commands::CompanyAllowListControl>::Post(CALCA_ADD, NetworkClientInfo::GetByClientID(client_id)->public_key);
	}

	/**
	 * Part of RebuildList() to create the information for a single company.
	 * @param company_id The company to build the list for.
	 * @param client_playas The company the client is joined as.
	 * @param can_join_company Whether this company can be joined by us.
	 */
	void RebuildListCompany(CompanyID company_id, CompanyID client_playas, bool can_join_company)
	{
		ButtonLine &company_line = *this->buttons.emplace_back(std::make_unique<CompanyButtonLine>(company_id));
		if (_network_server) company_line.AddButton<CompanyButton>(SPR_ADMIN, STR_NETWORK_CLIENT_LIST_ADMIN_COMPANY_TOOLTIP, COLOUR_RED, company_id, &NetworkClientListWindow::OnClickCompanyAdmin, company_id == COMPANY_SPECTATOR);
		ButtonCommon &chat_button = company_line.AddButton<CompanyButton>(SPR_CHAT, company_id == COMPANY_SPECTATOR ? STR_NETWORK_CLIENT_LIST_CHAT_SPECTATOR_TOOLTIP : STR_NETWORK_CLIENT_LIST_CHAT_COMPANY_TOOLTIP, COLOUR_ORANGE, company_id, &NetworkClientListWindow::OnClickCompanyChat);
		if (can_join_company) company_line.AddButton<CompanyButton>(SPR_JOIN, STR_NETWORK_CLIENT_LIST_JOIN_TOOLTIP, COLOUR_ORANGE, company_id, &NetworkClientListWindow::OnClickCompanyJoin, company_id != COMPANY_SPECTATOR && Company::Get(company_id)->is_ai);

		bool has_players = false;
		for (const NetworkClientInfo *ci : NetworkClientInfo::Iterate()) {
			if (ci->client_playas != company_id) continue;
			has_players = true;

			ButtonLine &line = *this->buttons.emplace_back(std::make_unique<ClientButtonLine>(ci->index));
			if (_network_server) line.AddButton<ClientButton>(SPR_ADMIN, STR_NETWORK_CLIENT_LIST_ADMIN_CLIENT_TOOLTIP, COLOUR_RED, ci->client_id, &NetworkClientListWindow::OnClickClientAdmin, _network_own_client_id == ci->client_id);
			if (_network_own_client_id != ci->client_id) line.AddButton<ClientButton>(SPR_CHAT, STR_NETWORK_CLIENT_LIST_CHAT_CLIENT_TOOLTIP, COLOUR_ORANGE, ci->client_id, &NetworkClientListWindow::OnClickClientChat);
			if (_network_own_client_id != ci->client_id && client_playas != COMPANY_SPECTATOR && !ci->CanJoinCompany(client_playas)) line.AddButton<ClientButton>(SPR_JOIN, STR_NETWORK_CLIENT_LIST_COMPANY_AUTHORIZE_TOOLTIP, COLOUR_GREEN, ci->client_id, &NetworkClientListWindow::OnClickClientAuthorize);
		}

		/* Disable the chat button when there are players in this company. */
		chat_button.disabled = !has_players;
	}

	/**
	 * Rebuild the list, meaning: calculate the lines needed and what buttons go on which line.
	 */
	void RebuildList()
	{
		const NetworkClientInfo *own_ci = NetworkClientInfo::GetByClientID(_network_own_client_id);
		CompanyID client_playas = own_ci == nullptr ? COMPANY_SPECTATOR : own_ci->client_playas;

		this->buttons.clear();

		/* As spectator, show a line to create a new company. */
		if (client_playas == COMPANY_SPECTATOR && !NetworkMaxCompaniesReached()) {
			ButtonLine &line = *this->buttons.emplace_back(std::make_unique<CompanyButtonLine>(COMPANY_NEW_COMPANY));
			line.AddButton<CompanyButton>(SPR_JOIN, STR_NETWORK_CLIENT_LIST_NEW_COMPANY_TOOLTIP, COLOUR_ORANGE, COMPANY_SPECTATOR, &NetworkClientListWindow::OnClickCompanyNew);
		}

		if (client_playas != COMPANY_SPECTATOR) {
			this->RebuildListCompany(client_playas, client_playas, false);
		}

		/* Companies */
		for (const Company *c : Company::Iterate()) {
			if (c->index == client_playas) continue;

			this->RebuildListCompany(c->index, client_playas, (own_ci != nullptr && c->allow_list.Contains(own_ci->public_key)) || _network_server);
		}

		/* Spectators */
		this->RebuildListCompany(COMPANY_SPECTATOR, client_playas, client_playas != COMPANY_SPECTATOR);

		this->vscroll->SetCount(this->buttons.size());
	}

public:
	NetworkClientListWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_CL_SCROLLBAR);
		this->OnInvalidateData();
		this->FinishInitNested(window_number);
	}

	void OnInit() override
	{
		this->RebuildList();
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		this->RebuildList();

		/* Currently server information is not synced to clients, so we cannot show it on clients. */
		this->GetWidget<NWidgetStacked>(WID_CL_SERVER_SELECTOR)->SetDisplayedPlane(_network_server ? 0 : SZSP_HORIZONTAL);
		this->SetWidgetDisabledState(WID_CL_SERVER_NAME_EDIT, !_network_server);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_CL_SERVER_NAME:
			case WID_CL_CLIENT_NAME: {
				std::string str;
				if (widget == WID_CL_SERVER_NAME) {
					str = GetString(STR_JUST_RAW_STRING, _network_server ? _settings_client.network.server_name : _network_server_name);
				} else {
					const NetworkClientInfo *own_ci = NetworkClientInfo::GetByClientID(_network_own_client_id);
					str = GetString(STR_JUST_RAW_STRING, own_ci != nullptr ? own_ci->client_name : _settings_client.network.client_name);
				}
				size = GetStringBoundingBox(str);
				size.width = std::min(size.width, static_cast<uint>(ScaleGUITrad(200))); // By default, don't open the window too wide.
				break;
			}

			case WID_CL_SERVER_VISIBILITY:
				size = maxdim(maxdim(GetStringBoundingBox(STR_NETWORK_SERVER_VISIBILITY_LOCAL), GetStringBoundingBox(STR_NETWORK_SERVER_VISIBILITY_PUBLIC)), GetStringBoundingBox(STR_NETWORK_SERVER_VISIBILITY_INVITE_ONLY));
				size.width += padding.width;
				size.height += padding.height;
				break;

			case WID_CL_MATRIX: {
				uint height = std::max({GetScaledSpriteSize(SPR_COMPANY_ICON).height, GetScaledSpriteSize(SPR_JOIN).height, GetScaledSpriteSize(SPR_ADMIN).height, GetScaledSpriteSize(SPR_CHAT).height});
				height += WidgetDimensions::scaled.framerect.Vertical();
				this->line_height = std::max(height, (uint)GetCharacterHeight(FS_NORMAL)) + padding.height;

				resize.width = 1;
				fill.height = resize.height = this->line_height;
				size.height = std::max(size.height, 5 * this->line_height);
				break;
			}
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_CL_MATRIX);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_CL_SERVER_NAME:
				return _network_server ? _settings_client.network.server_name : _network_server_name;

			case WID_CL_SERVER_VISIBILITY:
				return GetString(STR_NETWORK_SERVER_VISIBILITY_LOCAL + _settings_client.network.server_game_type);

			case WID_CL_SERVER_INVITE_CODE:
				return _network_server_connection_type == CONNECTION_TYPE_UNKNOWN ? std::string{} : _network_server_invite_code;

			case WID_CL_SERVER_CONNECTION_TYPE:
				return GetString(STR_NETWORK_CLIENT_LIST_SERVER_CONNECTION_TYPE_UNKNOWN + _network_server_connection_type);

			case WID_CL_CLIENT_NAME: {
				const NetworkClientInfo *own_ci = NetworkClientInfo::GetByClientID(_network_own_client_id);
				return own_ci != nullptr ? own_ci->client_name : _settings_client.network.client_name;
			}

			case WID_CL_CLIENT_COMPANY_COUNT:
				return GetString(STR_NETWORK_CLIENT_LIST_CLIENT_COMPANY_COUNT, NetworkClientInfo::GetNumItems(), Company::GetNumItems(), NetworkMaxCompaniesAllowed());

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_CL_SERVER_NAME_EDIT:
				if (!_network_server) break;

				this->query_widget = WID_CL_SERVER_NAME_EDIT;
				ShowQueryString(_settings_client.network.server_name, STR_NETWORK_CLIENT_LIST_SERVER_NAME_QUERY_CAPTION, NETWORK_NAME_LENGTH, this, CS_ALPHANUMERAL, QueryStringFlag::LengthIsInChars);
				break;

			case WID_CL_CLIENT_NAME_EDIT: {
				const NetworkClientInfo *own_ci = NetworkClientInfo::GetByClientID(_network_own_client_id);
				this->query_widget = WID_CL_CLIENT_NAME_EDIT;
				ShowQueryString(own_ci != nullptr ? own_ci->client_name : _settings_client.network.client_name, STR_NETWORK_CLIENT_LIST_PLAYER_NAME_QUERY_CAPTION, NETWORK_CLIENT_NAME_LENGTH, this, CS_ALPHANUMERAL, QueryStringFlag::LengthIsInChars);
				break;
			}
			case WID_CL_SERVER_VISIBILITY:
				if (!_network_server) break;

				ShowDropDownList(this, BuildVisibilityDropDownList(), _settings_client.network.server_game_type, WID_CL_SERVER_VISIBILITY);
				break;

			case WID_CL_MATRIX: {
				auto it = this->vscroll->GetScrolledItemFromWidget(this->buttons, pt.y, this, WID_CL_MATRIX);
				if (it == std::end(this->buttons)) break;

				Rect r = this->GetWidget<NWidgetBase>(WID_CL_MATRIX)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
				ButtonCommon *button = (*it)->GetButton(r, pt);
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
				auto it = this->vscroll->GetScrolledItemFromWidget(this->buttons, pt.y, this, WID_CL_MATRIX);
				if (it == std::end(this->buttons)) break;

				Rect r = this->GetWidget<NWidgetBase>(WID_CL_MATRIX)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
				auto tooltip = (*it)->GetTooltip(r, pt);
				if (!tooltip.has_value()) break;

				GuiShowTooltips(this, std::move(*tooltip), close_cond);
				return true;
			};
		}

		return false;
	}

	void OnDropdownClose(Point pt, WidgetID widget, int index, int click_result, bool instant_close) override
	{
		/* If you close the dropdown outside the list, don't take any action. */
		if (widget == WID_CL_MATRIX) return;

		Window::OnDropdownClose(pt, widget, index, click_result, instant_close);
	}

	void OnDropdownSelect(WidgetID widget, int index, int) override
	{
		switch (widget) {
			case WID_CL_SERVER_VISIBILITY:
				if (!_network_server) break;

				_settings_client.network.server_game_type = (ServerGameType)index;
				NetworkUpdateServerGameType();
				break;

			case WID_CL_MATRIX: {
				QueryCallbackProc *callback = nullptr;

				EncodedString text;
				switch (index) {
					case DD_CLIENT_ADMIN_KICK:
						_admin_client_id = this->dd_client_id;
						callback = AdminClientKickCallback;
						text = GetEncodedString(STR_NETWORK_CLIENT_LIST_ASK_CLIENT_KICK, NetworkClientInfo::GetByClientID(_admin_client_id)->client_name);
						break;

					case DD_CLIENT_ADMIN_BAN:
						_admin_client_id = this->dd_client_id;
						callback = AdminClientBanCallback;
						text = GetEncodedString(STR_NETWORK_CLIENT_LIST_ASK_CLIENT_BAN, NetworkClientInfo::GetByClientID(_admin_client_id)->client_name);
						break;

					case DD_COMPANY_ADMIN_RESET:
						_admin_company_id = this->dd_company_id;
						callback = AdminCompanyResetCallback;
						text = GetEncodedString(STR_NETWORK_CLIENT_LIST_ASK_COMPANY_RESET, _admin_company_id);
						break;

					default:
						NOT_REACHED();
				}

				assert(callback != nullptr);

				/* Always ask confirmation for all admin actions. */
				ShowQuery(
					GetEncodedString(STR_NETWORK_CLIENT_LIST_ASK_CAPTION),
					std::move(text),
					this, callback);

				break;
			}

			default:
				NOT_REACHED();
		}

		this->SetDirty();
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value()) return;

		switch (this->query_widget) {
			default: NOT_REACHED();

			case WID_CL_SERVER_NAME_EDIT: {
				if (!_network_server) break;

				SetSettingValue(GetSettingFromName("network.server_name")->AsStringSetting(), *str);
				this->InvalidateData();
				break;
			}

			case WID_CL_CLIENT_NAME_EDIT: {
				SetSettingValue(GetSettingFromName("network.client_name")->AsStringSetting(), *str);
				this->InvalidateData();
				break;
			}
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_CL_MATRIX: {
				Rect ir = r.WithHeight(this->line_height).Shrink(WidgetDimensions::scaled.framerect, RectPadding::zero);

				if (this->hover_index >= 0) {
					Rect br = r.WithHeight(this->line_height).Translate(0, this->hover_index * this->line_height);
					GfxFillRect(br.Shrink(WidgetDimensions::scaled.bevel), GREY_SCALE(9));
				}

				auto [first, last] = this->vscroll->GetVisibleRangeIterators(this->buttons);
				for (auto it = first; it != last; ++it) {
					(*it)->Draw(ir);
					ir = ir.Translate(0, this->line_height);
				}

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
	AllocateWindowDescFront<NetworkClientListWindow>(_client_list_desc, 0);
}

NetworkJoinStatus _network_join_status; ///< The status of joining.
uint8_t _network_join_waiting;            ///< The number of clients waiting in front of us.
uint32_t _network_join_bytes;             ///< The number of bytes we already downloaded.
uint32_t _network_join_bytes_total;       ///< The total number of bytes to download.

struct NetworkJoinStatusWindow : Window {
	std::shared_ptr<NetworkAuthenticationPasswordRequest> request{};

	NetworkJoinStatusWindow(WindowDesc &desc) : Window(desc)
	{
		this->parent = FindWindowById(WC_NETWORK_WINDOW, WN_NETWORK_WINDOW_GAME);
		this->InitNested(WN_NETWORK_STATUS_WINDOW_JOIN);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_NJS_PROGRESS_BAR: {
				/* Draw the % complete with a bar and a text */
				DrawFrameRect(r, COLOUR_GREY, {FrameFlag::BorderOnly, FrameFlag::Lowered});
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
						[[fallthrough]];

					default: // Waiting is 15%, so the remaining downloading of the map is maximum 70%
						progress = 15 + _network_join_bytes * (100 - 15) / _network_join_bytes_total;
						break;
				}
				DrawFrameRect(ir.WithWidth(ir.Width() * progress / 100, _current_text_dir == TD_RTL), COLOUR_MAUVE, {});
				DrawString(ir.left, ir.right, CentreBounds(ir.top, ir.bottom, GetCharacterHeight(FS_NORMAL)), STR_NETWORK_CONNECTING_1 + _network_join_status, TC_FROMSTRING, SA_HOR_CENTER);
				break;
			}

			case WID_NJS_PROGRESS_TEXT:
				switch (_network_join_status) {
					case NETWORK_JOIN_STATUS_WAITING:
						DrawStringMultiLine(r, GetString(STR_NETWORK_CONNECTING_WAITING, _network_join_waiting), TC_FROMSTRING, SA_CENTER);
						break;

					case NETWORK_JOIN_STATUS_DOWNLOADING:
						if (_network_join_bytes_total == 0) {
							DrawStringMultiLine(r, GetString(STR_NETWORK_CONNECTING_DOWNLOADING_1, _network_join_bytes), TC_FROMSTRING, SA_CENTER);
						} else {
							DrawStringMultiLine(r, GetString(STR_NETWORK_CONNECTING_DOWNLOADING_2, _network_join_bytes, _network_join_bytes_total), TC_FROMSTRING, SA_CENTER);
						}
						break;

					default:
						break;
				}
				break;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_NJS_PROGRESS_BAR:
				/* Account for the statuses */
				for (uint i = 0; i < NETWORK_JOIN_STATUS_END; i++) {
					size = maxdim(size, GetStringBoundingBox(STR_NETWORK_CONNECTING_1 + i));
				}
				/* For the number of waiting (other) players */
				size = maxdim(size, GetStringBoundingBox(GetString(STR_NETWORK_CONNECTING_WAITING, GetParamMaxValue(MAX_CLIENTS))));
				/* We need some spacing for the 'border' */
				size.height += WidgetDimensions::scaled.frametext.Horizontal();
				size.width  += WidgetDimensions::scaled.frametext.Vertical();
				break;

			case WID_NJS_PROGRESS_TEXT: {
				/* Account for downloading ~ 10 MiB */
				uint64_t max_digits = GetParamMaxDigits(8);
				size = maxdim(size, GetStringBoundingBox(GetString(STR_NETWORK_CONNECTING_DOWNLOADING_1, max_digits, max_digits)));
				size = maxdim(size, GetStringBoundingBox(GetString(STR_NETWORK_CONNECTING_DOWNLOADING_1, max_digits, max_digits)));
				break;
			}
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

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value() || str->empty() || this->request == nullptr) {
			NetworkDisconnect();
			return;
		}

		this->request->Reply(*str);
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_network_join_status_window_widgets = {
	NWidget(WWT_CAPTION, COLOUR_GREY), SetStringTip(STR_NETWORK_CONNECTING_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_NJS_PROGRESS_BAR), SetFill(1, 0),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_NJS_PROGRESS_TEXT), SetFill(1, 0), SetMinimalSize(350, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NJS_CANCELOK), SetMinimalSize(101, 12), SetStringTip(STR_NETWORK_CONNECTION_DISCONNECT), SetFill(1, 0),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _network_join_status_window_desc(
	WDP_CENTER, {}, 0, 0,
	WC_NETWORK_STATUS_WINDOW, WC_NONE,
	WindowDefaultFlag::Modal,
	_nested_network_join_status_window_widgets
);

void ShowJoinStatusWindow()
{
	CloseWindowById(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_JOIN);
	new NetworkJoinStatusWindow(_network_join_status_window_desc);
}

void ShowNetworkNeedPassword(std::shared_ptr<NetworkAuthenticationPasswordRequest> request)
{
	NetworkJoinStatusWindow *w = dynamic_cast<NetworkJoinStatusWindow *>(FindWindowById(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_JOIN));
	if (w == nullptr) return;
	w->request = std::move(request);

	ShowQueryString({}, STR_NETWORK_NEED_GAME_PASSWORD_CAPTION, NETWORK_PASSWORD_LENGTH, w, CS_ALPHANUMERAL, {});
}

/**
 * Window used for asking the user if he is okay using a relay server.
 */
struct NetworkAskRelayWindow : public Window {
	std::string server_connection_string{}; ///< The game server we want to connect to.
	std::string relay_connection_string{}; ///< The relay server we want to connect to.
	std::string token{}; ///< The token for this connection.

	NetworkAskRelayWindow(WindowDesc &desc, Window *parent, std::string_view server_connection_string, std::string &&relay_connection_string, std::string &&token) :
		Window(desc),
		server_connection_string(server_connection_string),
		relay_connection_string(std::move(relay_connection_string)),
		token(std::move(token))
	{
		this->parent = parent;
		this->InitNested(0);
	}

	void Close(int data = 0) override
	{
		if (data == NRWCD_UNHANDLED) _network_coordinator_client.ConnectFailure(this->token, 0);
		this->Window::Close();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget == WID_NAR_TEXT) {
			size = GetStringBoundingBox(GetString(STR_NETWORK_ASK_RELAY_TEXT, this->server_connection_string, this->relay_connection_string));
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget == WID_NAR_TEXT) {
			DrawStringMultiLine(r, GetString(STR_NETWORK_ASK_RELAY_TEXT, this->server_connection_string, this->relay_connection_string), TC_FROMSTRING, SA_CENTER);
		}
	}

	void FindWindowPlacementAndResize(int, int, bool) override
	{
		/* Position query window over the calling window, ensuring it's within screen bounds. */
		this->left = Clamp(parent->left + (parent->width / 2) - (this->width / 2), 0, _screen.width - this->width);
		this->top = Clamp(parent->top + (parent->height / 2) - (this->height / 2), 0, _screen.height - this->height);
		this->SetDirty();
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
				_settings_client.network.use_relay_service = UseRelayService::Allow;
				_network_coordinator_client.StartTurnConnection(this->token);
				this->Close(NRWCD_HANDLED);
				break;
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_network_ask_relay_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_RED),
		NWidget(WWT_CAPTION, COLOUR_RED, WID_NAR_CAPTION), SetStringTip(STR_NETWORK_ASK_RELAY_CAPTION),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_RED),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_TEXT, INVALID_COLOUR, WID_NAR_TEXT), SetAlignment(SA_HOR_CENTER), SetFill(1, 1),
			NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NAR_NO), SetMinimalSize(71, 12), SetFill(1, 1), SetStringTip(STR_NETWORK_ASK_RELAY_NO),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NAR_YES_ONCE), SetMinimalSize(71, 12), SetFill(1, 1), SetStringTip(STR_NETWORK_ASK_RELAY_YES_ONCE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NAR_YES_ALWAYS), SetMinimalSize(71, 12), SetFill(1, 1), SetStringTip(STR_NETWORK_ASK_RELAY_YES_ALWAYS),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _network_ask_relay_desc(
	WDP_CENTER, {}, 0, 0,
	WC_NETWORK_ASK_RELAY, WC_NONE,
	WindowDefaultFlag::Modal,
	_nested_network_ask_relay_widgets
);

/**
 * Show a modal confirmation window with "no" / "yes, once" / "yes, always" buttons.
 * @param server_connection_string The game server we want to connect to.
 * @param relay_connection_string The relay server we want to connect to.
 * @param token The token for this connection.
 */
void ShowNetworkAskRelay(std::string_view server_connection_string, std::string &&relay_connection_string, std::string &&token)
{
	CloseWindowByClass(WC_NETWORK_ASK_RELAY, NRWCD_HANDLED);

	Window *parent = GetMainWindow();
	new NetworkAskRelayWindow(_network_ask_relay_desc, parent, server_connection_string, std::move(relay_connection_string), std::move(token));
}

/**
 * Window used for asking if the user wants to participate in the automated survey.
 */
struct NetworkAskSurveyWindow : public Window {
	NetworkAskSurveyWindow(WindowDesc &desc, Window *parent) :
		Window(desc)
	{
		this->parent = parent;
		this->InitNested(0);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget == WID_NAS_TEXT) {
			size = GetStringBoundingBox(STR_NETWORK_ASK_SURVEY_TEXT);
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget == WID_NAS_TEXT) {
			DrawStringMultiLine(r, STR_NETWORK_ASK_SURVEY_TEXT, TC_BLACK, SA_CENTER);
		}
	}

	void FindWindowPlacementAndResize(int, int, bool) override
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
				ShowSurveyResultTextfileWindow(this);
				break;

			case WID_NAS_LINK:
				OpenBrowser(NETWORK_SURVEY_DETAILS_LINK);
				break;

			case WID_NAS_NO:
				_settings_client.network.participate_survey = ParticipateSurvey::No;
				this->Close();
				break;

			case WID_NAS_YES:
				_settings_client.network.participate_survey = ParticipateSurvey::Yes;
				this->Close();
				break;
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_network_ask_survey_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_NAS_CAPTION), SetStringTip(STR_NETWORK_ASK_SURVEY_CAPTION),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_TEXT, INVALID_COLOUR, WID_NAS_TEXT), SetAlignment(SA_HOR_CENTER), SetFill(1, 1),
			NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NAS_PREVIEW), SetMinimalSize(71, 12), SetFill(1, 1), SetStringTip(STR_NETWORK_ASK_SURVEY_PREVIEW),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NAS_LINK), SetMinimalSize(71, 12), SetFill(1, 1), SetStringTip(STR_NETWORK_ASK_SURVEY_LINK),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_NAS_NO), SetMinimalSize(71, 12), SetFill(1, 1), SetStringTip(STR_NETWORK_ASK_SURVEY_NO),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_NAS_YES), SetMinimalSize(71, 12), SetFill(1, 1), SetStringTip(STR_NETWORK_ASK_SURVEY_YES),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _network_ask_survey_desc(
	WDP_CENTER, {}, 0, 0,
	WC_NETWORK_ASK_SURVEY, WC_NONE,
	WindowDefaultFlag::Modal,
	_nested_network_ask_survey_widgets
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
	new NetworkAskSurveyWindow(_network_ask_survey_desc, parent);
}

/** Window for displaying the textfile of a survey result. */
struct SurveyResultTextfileWindow : public TextfileWindow {
	const GRFConfig *grf_config; ///< View the textfile of this GRFConfig.

	SurveyResultTextfileWindow(Window *parent, TextfileType file_type) : TextfileWindow(parent, file_type)
	{
		this->ConstructWindow();

		auto result = _survey.CreatePayload(NetworkSurveyHandler::Reason::Preview, true);
		this->LoadText(result);
		this->InvalidateData();
	}
};

void ShowSurveyResultTextfileWindow(Window *parent)
{
	parent->CloseChildWindowById(WC_TEXTFILE, TFT_SURVEY_RESULT);
	new SurveyResultTextfileWindow(parent, TFT_SURVEY_RESULT);
}
