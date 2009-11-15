/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_gui.cpp Implementation of the Network related GUIs. */

#ifdef ENABLE_NETWORK
#include "../stdafx.h"
#include "../openttd.h"
#include "../strings_func.h"
#include "../date_func.h"
#include "../fios.h"
#include "network_internal.h"
#include "network_client.h"
#include "network_gui.h"
#include "network_gamelist.h"
#include "../gui.h"
#include "network_udp.h"
#include "../window_func.h"
#include "../gfx_func.h"
#include "../widgets/dropdown_func.h"
#include "../querystring_gui.h"
#include "../sortlist_type.h"
#include "../company_base.h"
#include "../company_func.h"

#include "table/strings.h"
#include "../table/sprites.h"


static void ShowNetworkStartServerWindow();
static void ShowNetworkLobbyWindow(NetworkGameList *ngl);
extern void SwitchToMode(SwitchMode new_mode);

static const StringID _connection_types_dropdown[] = {
	STR_NETWORK_START_SERVER_LAN_INTERNET,
	STR_NETWORK_START_SERVER_INTERNET_ADVERTISE,
	INVALID_STRING_ID
};

static const StringID _lan_internet_types_dropdown[] = {
	STR_NETWORK_SERVER_LIST_LAN,
	STR_NETWORK_SERVER_LIST_INTERNET,
	INVALID_STRING_ID
};

static StringID _language_dropdown[NETLANG_COUNT + 1] = {STR_NULL};

void SortNetworkLanguages()
{
	/* Init the strings */
	if (_language_dropdown[0] == STR_NULL) {
		for (int i = 0; i < NETLANG_COUNT; i++) _language_dropdown[i] = STR_NETWORK_LANG_ANY + i;
		_language_dropdown[NETLANG_COUNT] = INVALID_STRING_ID;
	}

	/* Sort the strings (we don't move 'any' and the 'invalid' one) */
	QSortT(_language_dropdown, NETLANG_COUNT - 1, &StringIDSorter);
}

/** Update the network new window because a new server is
 * found on the network.
 * @param unselect unselect the currently selected item */
void UpdateNetworkGameWindow(bool unselect)
{
	InvalidateWindowData(WC_NETWORK_WINDOW, 0, unselect ? 1 : 0);
}

/** Enum for NetworkGameWindow, referring to _network_game_window_widgets */
enum NetworkGameWindowWidgets {
	NGWW_CLOSE,         ///< Close 'X' button
	NGWW_CAPTION,       ///< Caption of the window
	NGWW_MAIN,          ///< Main panel

	NGWW_CONNECTION,    ///< Label in front of connection droplist
	NGWW_CONN_BTN,      ///< 'Connection' droplist button
	NGWW_CLIENT_LABEL,  ///< Label in front of client name edit box
	NGWW_CLIENT,        ///< Panel with editbox to set client name

	NGWW_HEADER,        ///< Header container of the matrix
	NGWW_NAME,          ///< 'Name' button
	NGWW_CLIENTS,       ///< 'Clients' button
	NGWW_MAPSIZE,       ///< 'Map size' button
	NGWW_DATE,          ///< 'Date' button
	NGWW_YEARS,         ///< 'Years' button
	NGWW_INFO,          ///< Third button in the game list panel

	NGWW_MATRIX,        ///< Panel with list of games
	NGWW_SCROLLBAR,     ///< Scrollbar of matrix

	NGWW_LASTJOINED_LABEL, ///< Label "Last joined server:"
	NGWW_LASTJOINED,    ///< Info about the last joined server

	NGWW_DETAILS,       ///< Panel with game details
	NGWW_DETAILS_SPACER, ///< Spacer for game actual details
	NGWW_JOIN,          ///< 'Join game' button
	NGWW_REFRESH,       ///< 'Refresh server' button
	NGWW_NEWGRF,        ///< 'NewGRF Settings' button
	NGWW_NEWGRF_SEL,    ///< Selection 'widget' to hide the NewGRF settings

	NGWW_FIND,          ///< 'Find server' button
	NGWW_ADD,           ///< 'Add server' button
	NGWW_START,         ///< 'Start server' button
	NGWW_CANCEL,        ///< 'Cancel' button

	NGWW_RESIZE,        ///< Resize button
};

typedef GUIList<NetworkGameList*> GUIGameServerList;
typedef uint16 ServerListPosition;
static const ServerListPosition SLP_INVALID = 0xFFFF;

/** Full blown container to make it behave exactly as we want :) */
class NWidgetServerListHeader : public NWidgetContainer {
	static const uint MINIMUM_NAME_WIDTH_BEFORE_NEW_HEADER = 150; ///< Minimum width before adding a new header
	bool visible[6]; ///< The visible headers
public:
	NWidgetServerListHeader() : NWidgetContainer(NWID_HORIZONTAL)
	{
		NWidgetLeaf *leaf = new NWidgetLeaf(WWT_PUSHTXTBTN, COLOUR_WHITE, NGWW_NAME, STR_NETWORK_SERVER_LIST_GAME_NAME, STR_NETWORK_SERVER_LIST_GAME_NAME_TOOLTIP);
		leaf->SetResize(1, 0);
		leaf->SetFill(true, false);
		this->Add(leaf);

		this->Add(new NWidgetLeaf(WWT_PUSHTXTBTN, COLOUR_WHITE, NGWW_CLIENTS, STR_NETWORK_SERVER_LIST_CLIENTS_CAPTION, STR_NETWORK_SERVER_LIST_CLIENTS_CAPTION_TOOLTIP));
		this->Add(new NWidgetLeaf(WWT_PUSHTXTBTN, COLOUR_WHITE, NGWW_MAPSIZE, STR_NETWORK_SERVER_LIST_MAP_SIZE_CAPTION, STR_NETWORK_SERVER_LIST_MAP_SIZE_CAPTION_TOOLTIP));
		this->Add(new NWidgetLeaf(WWT_PUSHTXTBTN, COLOUR_WHITE, NGWW_DATE, STR_NETWORK_SERVER_LIST_DATE_CAPTION, STR_NETWORK_SERVER_LIST_DATE_CAPTION_TOOLTIP));
		this->Add(new NWidgetLeaf(WWT_PUSHTXTBTN, COLOUR_WHITE, NGWW_YEARS, STR_NETWORK_SERVER_LIST_YEARS_CAPTION, STR_NETWORK_SERVER_LIST_YEARS_CAPTION_TOOLTIP));

		leaf = new NWidgetLeaf(WWT_PUSHTXTBTN, COLOUR_WHITE, NGWW_INFO, STR_EMPTY, STR_NETWORK_SERVER_LIST_INFO_ICONS_TOOLTIP);
		leaf->SetMinimalSize(40, 12);
		leaf->SetFill(false, true);
		this->Add(leaf);

		/* First and last are always visible, the rest is implicitly zeroed */
		this->visible[0] = true;
		*lastof(this->visible) = true;
	}

	void SetupSmallestSize(Window *w, bool init_array)
	{
		/* Oh yeah, we ought to be findable! */
		w->nested_array[NGWW_HEADER] = this;

		this->smallest_x = this->head->smallest_x + this->tail->smallest_x; // First and last are always shown, rest not
		this->smallest_y = 0; // Biggest child.
		this->fill_x = true;
		this->fill_y = false;
		this->resize_x = 1; // We only resize in this direction
		this->resize_y = 0; // We never resize in this direction

		/* First initialise some variables... */
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			child_wid->SetupSmallestSize(w, init_array);
			this->smallest_y = max(this->smallest_y, child_wid->smallest_y + child_wid->padding_top + child_wid->padding_bottom);
		}

		/* ... then in a second pass make sure the 'current' sizes are set. Won't change for most widgets. */
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			child_wid->current_x = child_wid->smallest_x;
			child_wid->current_y = this->smallest_y;
		}
	}

	void AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool allow_resize_x, bool allow_resize_y, bool rtl)
	{
		assert(given_width >= this->smallest_x && given_height >= this->smallest_y);

		this->pos_x = x;
		this->pos_y = y;
		this->current_x = given_width;
		this->current_y = given_height;

		given_width -= this->tail->smallest_x;
		NWidgetBase *child_wid = this->head->next;
		/* The first and last widget are always visible, determine which other should be visible */
		for (uint i = 1; i < lengthof(this->visible) - 1; i++) {
			if (given_width - child_wid->smallest_x > MINIMUM_NAME_WIDTH_BEFORE_NEW_HEADER && this->visible[i - 1]) {
				this->visible[i] = true;
				given_width -= child_wid->smallest_x;
			} else {
				this->visible[i] = false;
			}
			child_wid = child_wid->next;
		}

		/* All remaining space goes to the first (name) widget */
		this->head->current_x = given_width;

		/* Now assign the widgets to their rightful place */
		uint position = 0; // Place to put next child relative to origin of the container.
		uint i = rtl ? lengthof(this->visible) - 1 : 0;
		child_wid = rtl ? this->tail : this->head;
		while (child_wid != NULL) {
			if (this->visible[i]) {
				child_wid->AssignSizePosition(sizing, x + position, y, child_wid->current_x, this->current_y, allow_resize_x, (this->resize_y > 0), rtl);
				position += child_wid->current_x;
			}

			child_wid = rtl ? child_wid->prev : child_wid->next;
			i += rtl ? -1 : 1;
		}
	}

	void StoreWidgets(Widget *widgets, int length, bool left_moving, bool top_moving, bool rtl)
	{
		/* We don't need to support the old version anymore! */
		NOT_REACHED();
	}

	/* virtual */ void Draw(const Window *w)
	{
		int i = 0;
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			if (!this->visible[i++]) continue;

			child_wid->Draw(w);
		}
	}

	/* virtual */ NWidgetCore *GetWidgetFromPos(int x, int y)
	{
		if (!IsInsideBS(x, this->pos_x, this->current_x) || !IsInsideBS(y, this->pos_y, this->current_y)) return NULL;

		int i = 0;
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			if (!this->visible[i++]) continue;
			NWidgetCore *nwid = child_wid->GetWidgetFromPos(x, y);
			if (nwid != NULL) return nwid;
		}
		return NULL;
	}

	/**
	 * Checks whether the given widget is actually visible.
	 * @param widget the widget to check for visibility
	 * @return true iff the widget is visible.
	 */
	bool IsWidgetVisible(NetworkGameWindowWidgets widget) const
	{
		assert((uint)(widget - NGWW_NAME) < lengthof(this->visible));
		return this->visible[widget - NGWW_NAME];
	}
};

class NetworkGameWindow : public QueryStringBaseWindow {
protected:
	/* Runtime saved values */
	static Listing last_sorting;

	/* Constants for sorting servers */
	static GUIGameServerList::SortFunction * const sorter_funcs[];

	byte field;                   ///< selected text-field
	NetworkGameList *server;      ///< selected server
	NetworkGameList *last_joined; ///< the last joined server
	GUIGameServerList servers;    ///< list with game servers.
	ServerListPosition list_pos;  ///< position of the selected server

	/**
	 * (Re)build the network game list as its amount has changed because
	 * an item has been added or deleted for example
	 */
	void BuildNetworkGameList()
	{
		if (!this->servers.NeedRebuild()) return;

		/* Create temporary array of games to use for listing */
		this->servers.Clear();

		for (NetworkGameList *ngl = _network_game_list; ngl != NULL; ngl = ngl->next) {
			*this->servers.Append() = ngl;
		}

		this->servers.Compact();
		this->servers.RebuildDone();
		this->vscroll.SetCount(this->servers.Length());
	}

	/** Sort servers by name. */
	static int CDECL NGameNameSorter(NetworkGameList * const *a, NetworkGameList * const *b)
	{
		return strcasecmp((*a)->info.server_name, (*b)->info.server_name);
	}

	/** Sort servers by the amount of clients online on a
	 * server. If the two servers have the same amount, the one with the
	 * higher maximum is preferred. */
	static int CDECL NGameClientSorter(NetworkGameList * const *a, NetworkGameList * const *b)
	{
		/* Reverse as per default we are interested in most-clients first */
		int r = (*a)->info.clients_on - (*b)->info.clients_on;

		if (r == 0) r = (*a)->info.clients_max - (*b)->info.clients_max;
		if (r == 0) r = NGameNameSorter(a, b);

		return r;
	}

	/** Sort servers by map size */
	static int CDECL NGameMapSizeSorter(NetworkGameList * const *a, NetworkGameList * const *b)
	{
		/* Sort by the area of the map. */
		int r = ((*a)->info.map_height) * ((*a)->info.map_width) - ((*b)->info.map_height) * ((*b)->info.map_width);

		if (r == 0) r = (*a)->info.map_width - (*b)->info.map_width;
		return (r != 0) ? r : NGameClientSorter(a, b);
	}

	/** Sort servers by current date */
	static int CDECL NGameDateSorter(NetworkGameList * const *a, NetworkGameList * const *b)
	{
		int r = (*a)->info.game_date - (*b)->info.game_date;
		return (r != 0) ? r : NGameClientSorter(a, b);
	}

	/** Sort servers by the number of days the game is running */
	static int CDECL NGameYearsSorter(NetworkGameList * const *a, NetworkGameList * const *b)
	{
		int r = (*a)->info.game_date - (*a)->info.start_date - (*b)->info.game_date + (*b)->info.start_date;
		return (r != 0) ? r : NGameDateSorter(a, b);
	}

	/** Sort servers by joinability. If both servers are the
	 * same, prefer the non-passworded server first. */
	static int CDECL NGameAllowedSorter(NetworkGameList * const *a, NetworkGameList * const *b)
	{
		/* The servers we do not know anything about (the ones that did not reply) should be at the bottom) */
		int r = StrEmpty((*a)->info.server_revision) - StrEmpty((*b)->info.server_revision);

		/* Reverse default as we are interested in version-compatible clients first */
		if (r == 0) r = (*b)->info.version_compatible - (*a)->info.version_compatible;
		/* The version-compatible ones are then sorted with NewGRF compatible first, incompatible last */
		if (r == 0) r = (*b)->info.compatible - (*a)->info.compatible;
		/* Passworded servers should be below unpassworded servers */
		if (r == 0) r = (*a)->info.use_password - (*b)->info.use_password;
		/* Finally sort on the name of the server */
		if (r == 0) r = NGameNameSorter(a, b);

		return r;
	}

	/** Sort the server list */
	void SortNetworkGameList()
	{
		if (!this->servers.Sort()) return;

		/* After sorting ngl->sort_list contains the sorted items. Put these back
		 * into the original list. Basically nothing has changed, we are only
		 * shuffling the ->next pointers. While iterating, look for the
		 * currently selected server and set list_pos to its position */
		this->list_pos = SLP_INVALID;
		_network_game_list = this->servers[0];
		NetworkGameList *item = _network_game_list;
		if (item == this->server) this->list_pos = 0;
		for (uint i = 1; i != this->servers.Length(); i++) {
			item->next = this->servers[i];
			item = item->next;
			if (item == this->server) this->list_pos = i;
		}
		item->next = NULL;
	}

	/**
	 * Draw a single server line.
	 * @param cur_item  the server to draw.
	 * @param y         from where to draw?
	 * @param highlight does the line need to be highlighted?
	 */
	void DrawServerLine(const NetworkGameList *cur_item, uint y, bool highlight) const
	{
		const NWidgetCore *nwi_name = this->GetWidget<NWidgetCore>(NGWW_NAME);
		const NWidgetCore *nwi_info = this->GetWidget<NWidgetCore>(NGWW_INFO);

		/* show highlighted item with a different colour */
		if (highlight) GfxFillRect(nwi_name->pos_x + 1, y - 2, nwi_info->pos_x + nwi_info->current_x - 2, y + FONT_HEIGHT_NORMAL - 1, 10);

		DrawString(nwi_name->pos_x + WD_FRAMERECT_LEFT, nwi_name->pos_x + nwi_name->current_x - WD_FRAMERECT_RIGHT, y, cur_item->info.server_name, TC_BLACK);

		/* only draw details if the server is online */
		if (cur_item->online) {
			const NWidgetServerListHeader *nwi_header = this->GetWidget<NWidgetServerListHeader>(NGWW_HEADER);

			if (nwi_header->IsWidgetVisible(NGWW_CLIENTS)) {
				const NWidgetCore *nwi_clients = this->GetWidget<NWidgetCore>(NGWW_CLIENTS);
				SetDParam(0, cur_item->info.clients_on);
				SetDParam(1, cur_item->info.clients_max);
				SetDParam(2, cur_item->info.companies_on);
				SetDParam(3, cur_item->info.companies_max);
				DrawString(nwi_clients->pos_x, nwi_clients->pos_x + nwi_clients->current_x - 1, y, STR_NETWORK_SERVER_LIST_GENERAL_ONLINE, TC_FROMSTRING, SA_CENTER);
			}

			if (nwi_header->IsWidgetVisible(NGWW_MAPSIZE)) {
				/* map size */
				const NWidgetCore *nwi_mapsize = this->GetWidget<NWidgetCore>(NGWW_MAPSIZE);
				SetDParam(0, cur_item->info.map_width);
				SetDParam(1, cur_item->info.map_height);
				DrawString(nwi_mapsize->pos_x, nwi_mapsize->pos_x + nwi_mapsize->current_x - 1, y, STR_NETWORK_SERVER_LIST_MAP_SIZE_SHORT, TC_FROMSTRING, SA_CENTER);
			}

			if (nwi_header->IsWidgetVisible(NGWW_DATE)) {
				/* current date */
				const NWidgetCore *nwi_date = this->GetWidget<NWidgetCore>(NGWW_DATE);
				YearMonthDay ymd;
				ConvertDateToYMD(cur_item->info.game_date, &ymd);
				SetDParam(0, ymd.year);
				DrawString(nwi_date->pos_x, nwi_date->pos_x + nwi_date->current_x - 1, y, STR_JUST_INT, TC_BLACK, SA_CENTER);
			}

			if (nwi_header->IsWidgetVisible(NGWW_YEARS)) {
				/* number of years the game is running */
				const NWidgetCore *nwi_years = this->GetWidget<NWidgetCore>(NGWW_YEARS);
				YearMonthDay ymd_cur, ymd_start;
				ConvertDateToYMD(cur_item->info.game_date, &ymd_cur);
				ConvertDateToYMD(cur_item->info.start_date, &ymd_start);
				SetDParam(0, ymd_cur.year - ymd_start.year);
				DrawString(nwi_years->pos_x, nwi_years->pos_x + nwi_years->current_x - 1, y, STR_JUST_INT, TC_BLACK, SA_CENTER);
			}

			/* draw a lock if the server is password protected */
			if (cur_item->info.use_password) DrawSprite(SPR_LOCK, PAL_NONE, nwi_info->pos_x + 5, y - 1);

			/* draw red or green icon, depending on compatibility with server */
			DrawSprite(SPR_BLOT, (cur_item->info.compatible ? PALETTE_TO_GREEN : (cur_item->info.version_compatible ? PALETTE_TO_YELLOW : PALETTE_TO_RED)), nwi_info->pos_x + 15, y);

			/* draw flag according to server language */
			DrawSprite(SPR_FLAGS_BASE + cur_item->info.server_lang, PAL_NONE, nwi_info->pos_x + 25, y);
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
		this->vscroll.ScrollTowards(this->list_pos);
	}

public:
	NetworkGameWindow(const WindowDesc *desc) : QueryStringBaseWindow(NETWORK_CLIENT_NAME_LENGTH)
	{
		this->InitNested(desc, 0);

		ttd_strlcpy(this->edit_str_buf, _settings_client.network.client_name, this->edit_str_size);
		this->afilter = CS_ALPHANUMERAL;
		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, 120);
		this->SetFocusedWidget(NGWW_CLIENT);

		UpdateNetworkGameWindow(true);

		this->field = NGWW_CLIENT;
		this->server = NULL;
		this->list_pos = SLP_INVALID;

		this->last_joined = NetworkGameListAddItem(NetworkAddress(_settings_client.network.last_host, _settings_client.network.last_port));

		this->servers.SetListing(this->last_sorting);
		this->servers.SetSortFuncs(this->sorter_funcs);
		this->servers.ForceRebuild();
		this->SortNetworkGameList();

		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(NGWW_MATRIX);
		this->vscroll.SetCapacity(nwi->current_y / this->resize.step_height);
		nwi->widget_data = (this->vscroll.GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}

	~NetworkGameWindow()
	{
		this->last_sorting = this->servers.GetListing();
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case NGWW_CONN_BTN:
				SetDParam(0, _lan_internet_types_dropdown[_settings_client.network.lan_internet]);
				break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		switch (widget) {
			case NGWW_CONN_BTN:
				*size = maxdim(GetStringBoundingBox(_lan_internet_types_dropdown[0]), GetStringBoundingBox(_lan_internet_types_dropdown[1]));
				size->width += padding.width;
				size->height += padding.height;
				break;

			case NGWW_MATRIX:
				resize->height = WD_MATRIX_TOP + FONT_HEIGHT_NORMAL + WD_MATRIX_BOTTOM;
				size->height = 10 * resize->height;
				break;

			case NGWW_LASTJOINED:
				size->height = WD_MATRIX_TOP + FONT_HEIGHT_NORMAL + WD_MATRIX_BOTTOM;
				break;

			case NGWW_NAME:
				size->width += 2 * WD_SORTBUTTON_ARROW_WIDTH; // Make space for the arrow
				break;

			case NGWW_CLIENTS:
				size->width += 2 * WD_SORTBUTTON_ARROW_WIDTH; // Make space for the arrow
				SetDParam(0, 255);
				SetDParam(1, 255);
				SetDParam(2, 15);
				SetDParam(3, 15);
				*size = maxdim(*size, GetStringBoundingBox(STR_NETWORK_SERVER_LIST_GENERAL_ONLINE));
				break;

			case NGWW_MAPSIZE:
				size->width += 2 * WD_SORTBUTTON_ARROW_WIDTH; // Make space for the arrow
				SetDParam(0, 2048);
				SetDParam(1, 2048);
				*size = maxdim(*size, GetStringBoundingBox(STR_NETWORK_SERVER_LIST_MAP_SIZE_SHORT));
				break;

			case NGWW_DATE:
			case NGWW_YEARS:
				size->width += 2 * WD_SORTBUTTON_ARROW_WIDTH; // Make space for the arrow
				SetDParam(0, 99999);
				*size = maxdim(*size, GetStringBoundingBox(STR_JUST_INT));
				break;

			case NGWW_DETAILS_SPACER:
				size->height = 20 + 12 * FONT_HEIGHT_NORMAL;
				break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case NGWW_MATRIX: {
				uint16 y = r.top + WD_MATRIX_TOP;

				const int max = min(this->vscroll.GetPosition() + this->vscroll.GetCapacity(), (int)this->servers.Length());

				for (int i = this->vscroll.GetPosition(); i < max; ++i) {
					const NetworkGameList *ngl = this->servers[i];
					this->DrawServerLine(ngl, y, ngl == this->server);
					y += this->resize.step_height;
				}
			} break;

			case NGWW_LASTJOINED:
				/* Draw the last joined server, if any */
				if (this->last_joined != NULL) this->DrawServerLine(this->last_joined, r.top + WD_MATRIX_TOP, this->last_joined == this->server);
				break;

			case NGWW_DETAILS:
				this->DrawDetails(r);
				break;

			case NGWW_NAME:
			case NGWW_CLIENTS:
			case NGWW_MAPSIZE:
			case NGWW_DATE:
			case NGWW_YEARS:
			case NGWW_INFO:
				if (widget - NGWW_NAME == this->servers.SortType()) this->DrawSortButtonState(widget, this->servers.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;
		}
	}


	virtual void OnPaint()
	{
		if (this->servers.NeedRebuild()) {
			this->BuildNetworkGameList();
		}
		this->SortNetworkGameList();

		NetworkGameList *sel = this->server;
		/* 'Refresh' button invisible if no server selected */
		this->SetWidgetDisabledState(NGWW_REFRESH, sel == NULL);
		/* 'Join' button disabling conditions */
		this->SetWidgetDisabledState(NGWW_JOIN, sel == NULL || // no Selected Server
				!sel->online || // Server offline
				sel->info.clients_on >= sel->info.clients_max || // Server full
				!sel->info.compatible); // Revision mismatch

		/* 'NewGRF Settings' button invisible if no NewGRF is used */
		this->GetWidget<NWidgetStacked>(NGWW_NEWGRF_SEL)->SetDisplayedPlane(sel == NULL || !sel->online || sel->info.grfconfig == NULL);

		this->DrawWidgets();
		/* Edit box to set client name */
		this->DrawEditBox(NGWW_CLIENT);
	}

	void DrawDetails(const Rect &r) const
	{
		NetworkGameList *sel = this->server;

		const int detail_height = 6 + 8 + 6 + 3 * FONT_HEIGHT_NORMAL;

		/* Draw the right menu */
		GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.top + detail_height - 1, 157);
		if (sel == NULL) {
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + 6 + 4 + FONT_HEIGHT_NORMAL, STR_NETWORK_SERVER_LIST_GAME_INFO, TC_FROMSTRING, SA_CENTER);
		} else if (!sel->online) {
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + 6 + 4 + FONT_HEIGHT_NORMAL, sel->info.server_name, TC_ORANGE, SA_CENTER); // game name

			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + detail_height + 4, STR_NETWORK_SERVER_LIST_SERVER_OFFLINE, TC_FROMSTRING, SA_CENTER); // server offline
		} else { // show game info

			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + 6, STR_NETWORK_SERVER_LIST_GAME_INFO, TC_FROMSTRING, SA_CENTER);
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + 6 + 4 + FONT_HEIGHT_NORMAL, sel->info.server_name, TC_ORANGE, SA_CENTER); // game name
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + 6 + 8 + 2 * FONT_HEIGHT_NORMAL, sel->info.map_name, TC_BLACK, SA_CENTER); // map name

			uint16 y = r.top + detail_height + 4;

			SetDParam(0, sel->info.clients_on);
			SetDParam(1, sel->info.clients_max);
			SetDParam(2, sel->info.companies_on);
			SetDParam(3, sel->info.companies_max);
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_CLIENTS);
			y += FONT_HEIGHT_NORMAL;

			SetDParam(0, STR_NETWORK_LANG_ANY + sel->info.server_lang);
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_LANGUAGE); // server language
			y += FONT_HEIGHT_NORMAL;

			SetDParam(0, STR_CHEAT_SWITCH_CLIMATE_TEMPERATE_LANDSCAPE + sel->info.map_set);
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_TILESET); // tileset
			y += FONT_HEIGHT_NORMAL;

			SetDParam(0, sel->info.map_width);
			SetDParam(1, sel->info.map_height);
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_MAP_SIZE); // map size
			y += FONT_HEIGHT_NORMAL;

			SetDParamStr(0, sel->info.server_revision);
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_SERVER_VERSION); // server version
			y += FONT_HEIGHT_NORMAL;

			SetDParamStr(0, sel->address.GetAddressAsString());
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_SERVER_ADDRESS); // server address
			y += FONT_HEIGHT_NORMAL;

			SetDParam(0, sel->info.start_date);
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_START_DATE); // start date
			y += FONT_HEIGHT_NORMAL;

			SetDParam(0, sel->info.game_date);
			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_CURRENT_DATE); // current date
			y += FONT_HEIGHT_NORMAL;

			y += WD_PAR_VSEP_NORMAL;

			if (!sel->info.compatible) {
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, sel->info.version_compatible ? STR_NETWORK_SERVER_LIST_GRF_MISMATCH : STR_NETWORK_SERVER_LIST_VERSION_MISMATCH, TC_FROMSTRING, SA_CENTER); // server mismatch
			} else if (sel->info.clients_on == sel->info.clients_max) {
				/* Show: server full, when clients_on == max_clients */
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_SERVER_FULL, TC_FROMSTRING, SA_CENTER); // server full
			} else if (sel->info.use_password) {
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_PASSWORD, TC_FROMSTRING, SA_CENTER); // password warning
			}
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		this->field = widget;
		switch (widget) {
			case NGWW_CANCEL: // Cancel button
				DeleteWindowById(WC_NETWORK_WINDOW, 0);
				break;

			case NGWW_CONN_BTN: // 'Connection' droplist
				ShowDropDownMenu(this, _lan_internet_types_dropdown, _settings_client.network.lan_internet, NGWW_CONN_BTN, 0, 0); // do it for widget NSSW_CONN_BTN
				break;

			case NGWW_NAME:    // Sort by name
			case NGWW_CLIENTS: // Sort by connected clients
			case NGWW_MAPSIZE: // Sort by map size
			case NGWW_DATE:    // Sort by date
			case NGWW_YEARS:   // Sort by years
			case NGWW_INFO:    // Connectivity (green dot)
				if (this->servers.SortType() == widget - NGWW_NAME) {
					this->servers.ToggleSortOrder();
					if (this->list_pos != SLP_INVALID) this->list_pos = this->servers.Length() - this->list_pos - 1;
				} else {
					this->servers.SetSortType(widget - NGWW_NAME);
					this->servers.ForceResort();
					this->SortNetworkGameList();
				}
				this->ScrollToSelectedServer();
				this->SetDirty();
				break;

			case NGWW_MATRIX: { // Matrix to show networkgames
				uint32 id_v = (pt.y - this->GetWidget<NWidgetCore>(NGWW_MATRIX)->pos_y) / this->resize.step_height;

				if (id_v >= this->vscroll.GetCapacity()) return; // click out of bounds
				id_v += this->vscroll.GetPosition();

				this->server = (id_v < this->servers.Length()) ? this->servers[id_v] : NULL;
				this->list_pos = (server == NULL) ? SLP_INVALID : id_v;
				this->SetDirty();
			} break;

			case NGWW_LASTJOINED: {
				NetworkGameList *last_joined = NetworkGameListAddItem(NetworkAddress(_settings_client.network.last_host, _settings_client.network.last_port));
				if (last_joined != NULL) {
					this->server = last_joined;

					/* search the position of the newly selected server */
					for (uint i = 0; i < this->servers.Length(); i++) {
						if (this->servers[i] == this->server) {
							this->list_pos = i;
							break;
						}
					}
					this->ScrollToSelectedServer();
					this->SetDirty();
				}
			} break;

			case NGWW_FIND: // Find server automatically
				switch (_settings_client.network.lan_internet) {
					case 0: NetworkUDPSearchGame(); break;
					case 1: NetworkUDPQueryMasterServer(); break;
				}
				break;

			case NGWW_ADD: // Add a server
				SetDParamStr(0, _settings_client.network.connect_to_ip);
				ShowQueryString(
					STR_JUST_RAW_STRING,
					STR_NETWORK_SERVER_LIST_ENTER_IP,
					NETWORK_HOSTNAME_LENGTH,  // maximum number of characters including '\0'
					0,                        // no limit in pixels
					this, CS_ALPHANUMERAL, QSF_ACCEPT_UNCHANGED);
				break;

			case NGWW_START: // Start server
				ShowNetworkStartServerWindow();
				break;

			case NGWW_JOIN: // Join Game
				if (this->server != NULL) {
					snprintf(_settings_client.network.last_host, sizeof(_settings_client.network.last_host), "%s", this->server->address.GetHostname());
					_settings_client.network.last_port = this->server->address.GetPort();
					ShowNetworkLobbyWindow(this->server);
				}
				break;

			case NGWW_REFRESH: // Refresh
				if (this->server != NULL) NetworkUDPQueryServer(this->server->address);
				break;

			case NGWW_NEWGRF: // NewGRF Settings
				if (this->server != NULL) ShowNewGRFSettings(false, false, false, &this->server->info.grfconfig);
				break;
		}
	}

	virtual void OnDoubleClick(Point pt, int widget)
	{
		if (widget == NGWW_MATRIX || widget == NGWW_LASTJOINED) {
			/* is the Join button enabled? */
			if (!this->IsWidgetDisabled(NGWW_JOIN)) this->OnClick(pt, NGWW_JOIN);
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case NGWW_CONN_BTN:
				_settings_client.network.lan_internet = index;
				break;

			default:
				NOT_REACHED();
		}

		this->SetDirty();
	}

	virtual void OnMouseLoop()
	{
		if (this->field == NGWW_CLIENT) this->HandleEditBox(NGWW_CLIENT);
	}

	virtual void OnInvalidateData(int data)
	{
		switch (data) {
			/* Remove the selection */
			case 1:
				this->server = NULL;
				this->list_pos = SLP_INVALID;
				break;

			/* Reiterate the whole server list as we downloaded some files */
			case 2:
				for (NetworkGameList **iter = this->servers.Begin(); iter != this->servers.End(); iter++) {
					NetworkGameList *item = *iter;
					bool missing_grfs = false;
					for (GRFConfig *c = item->info.grfconfig; c != NULL; c = c->next) {
						if (c->status != GCS_NOT_FOUND) continue;

						const GRFConfig *f = FindGRFConfig(c->grfid, c->md5sum);
						if (f == NULL) {
							missing_grfs = true;
							continue;
						}

						c->filename  = f->filename;
						c->name      = f->name;
						c->info      = f->info;
						c->status    = GCS_UNKNOWN;
					}

					if (!missing_grfs) item->info.compatible = item->info.version_compatible;
				}
				break;
		}
		this->servers.ForceRebuild();
		this->SetDirty();
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		EventState state = ES_NOT_HANDLED;

		/* handle up, down, pageup, pagedown, home and end */
		if (keycode == WKC_UP || keycode == WKC_DOWN || keycode == WKC_PAGEUP || keycode == WKC_PAGEDOWN || keycode == WKC_HOME || keycode == WKC_END) {
			if (this->servers.Length() == 0) return ES_HANDLED;
			switch (keycode) {
				case WKC_UP:
					/* scroll up by one */
					if (this->server == NULL) return ES_HANDLED;
					if (this->list_pos > 0) this->list_pos--;
					break;
				case WKC_DOWN:
					/* scroll down by one */
					if (this->server == NULL) return ES_HANDLED;
					if (this->list_pos < this->servers.Length() - 1) this->list_pos++;
					break;
				case WKC_PAGEUP:
					/* scroll up a page */
					if (this->server == NULL) return ES_HANDLED;
					this->list_pos = (this->list_pos < this->vscroll.GetCapacity()) ? 0 : this->list_pos - this->vscroll.GetCapacity();
					break;
				case WKC_PAGEDOWN:
					/* scroll down a page */
					if (this->server == NULL) return ES_HANDLED;
					this->list_pos = min(this->list_pos + this->vscroll.GetCapacity(), (int)this->servers.Length() - 1);
					break;
				case WKC_HOME:
					/* jump to beginning */
					this->list_pos = 0;
					break;
				case WKC_END:
					/* jump to end */
					this->list_pos = this->servers.Length() - 1;
					break;
				default: break;
			}

			this->server = this->servers[this->list_pos];

			/* scroll to the new server if it is outside the current range */
			this->ScrollToSelectedServer();

			/* redraw window */
			this->SetDirty();
			return ES_HANDLED;
		}

		if (this->field != NGWW_CLIENT) {
			if (this->server != NULL) {
				if (keycode == WKC_DELETE) { // Press 'delete' to remove servers
					NetworkGameListRemoveItem(this->server);
					this->server = NULL;
					this->list_pos = SLP_INVALID;
				}
			}
			return state;
		}

		if (this->HandleEditBoxKey(NGWW_CLIENT, key, keycode, state) == HEBR_CONFIRM) return state;

		/* The name is only allowed when it starts with a letter! */
		if (!StrEmpty(this->edit_str_buf) && this->edit_str_buf[0] != ' ') {
			strecpy(_settings_client.network.client_name, this->edit_str_buf, lastof(_settings_client.network.client_name));
		} else {
			strecpy(_settings_client.network.client_name, "Player", lastof(_settings_client.network.client_name));
		}
		return state;
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (!StrEmpty(str)) NetworkAddServer(str);
	}

	virtual void OnResize()
	{
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(NGWW_MATRIX);
		this->vscroll.SetCapacity(nwi->current_y / this->resize.step_height);
		nwi->widget_data = (this->vscroll.GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}
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

static NWidgetBase *MakeResizableHeader(int *biggest_index)
{
	*biggest_index = max<int>(*biggest_index, NGWW_INFO);
	return new NWidgetServerListHeader();
}

/* Generates incorrect display_flags for widgets NGWW_NAME, and incorrect
 * display_flags and/or left/right side for the overlapping widgets
 * NGWW_CLIENTS through NGWW_YEARS.
 */
static const NWidgetPart _nested_network_game_widgets[] = {
	/* TOP */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE, NGWW_CLOSE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE, NGWW_CAPTION), SetMinimalSize(439, 14), SetDataTip(STR_NETWORK_SERVER_LIST_CAPTION, STR_NULL), // XXX Add default caption tooltip!
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, NGWW_MAIN),
		NWidget(NWID_VERTICAL), SetPIP(10, 7, 0),
			NWidget(NWID_HORIZONTAL), SetPIP(10, 7, 10),
				NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, NGWW_CONNECTION), SetDataTip(STR_NETWORK_SERVER_LIST_CONNECTION, STR_NULL),
				NWidget(WWT_DROPDOWN, COLOUR_LIGHT_BLUE, NGWW_CONN_BTN),
										SetDataTip(STR_BLACK_STRING, STR_NETWORK_SERVER_LIST_CONNECTION_TOOLTIP),
				NWidget(NWID_SPACER), SetFill(true, false), SetResize(1, 0),
				NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, NGWW_CLIENT_LABEL), SetDataTip(STR_NETWORK_SERVER_LIST_PLAYER_NAME, STR_NULL),
				NWidget(WWT_EDITBOX, COLOUR_LIGHT_BLUE, NGWW_CLIENT), SetMinimalSize(151, 12),
										SetDataTip(STR_NETWORK_SERVER_LIST_PLAYER_NAME_OSKTITLE, STR_NETWORK_SERVER_LIST_ENTER_NAME_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(10, 7, 10),
				/* LEFT SIDE */
				NWidget(NWID_VERTICAL),
					NWidget(NWID_HORIZONTAL),
						NWidget(NWID_VERTICAL),
							NWidgetFunction(MakeResizableHeader),
							NWidget(WWT_MATRIX, COLOUR_LIGHT_BLUE, NGWW_MATRIX), SetResize(1, 1), SetFill(true, false),
												SetDataTip(0, STR_NETWORK_SERVER_LIST_CLICK_GAME_TO_SELECT),
						EndContainer(),
						NWidget(WWT_SCROLLBAR, COLOUR_LIGHT_BLUE, NGWW_SCROLLBAR), SetFill(false, true),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(0, 7), SetResize(1, 0), SetFill(true, true),
					NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, NGWW_LASTJOINED_LABEL), SetFill(true, false),
										SetDataTip(STR_NETWORK_SERVER_LIST_LAST_JOINED_SERVER, STR_NULL), SetResize(1, 0),
					NWidget(NWID_HORIZONTAL), SetPIP(0, 0, WD_VSCROLLBAR_WIDTH),
						NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, NGWW_LASTJOINED), SetFill(true, false), SetResize(1, 0),
											SetDataTip(0x0, STR_NETWORK_SERVER_LIST_CLICK_TO_SELECT_LAST),
						EndContainer(),
					EndContainer(),
				EndContainer(),
				/* RIGHT SIDE */
				NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, NGWW_DETAILS),
					NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(5, 5, 5),
						NWidget(WWT_EMPTY, INVALID_COLOUR, NGWW_DETAILS_SPACER), SetMinimalSize(140, 155), SetResize(0, 1), SetFill(true, true), // Make sure it's at least this wide
						NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(5, 5, 5),
							NWidget(NWID_SPACER), SetFill(true, false),
							NWidget(NWID_SELECTION, INVALID_COLOUR, NGWW_NEWGRF_SEL),
								NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NGWW_NEWGRF), SetFill(true, false), SetDataTip(STR_INTRO_NEWGRF_SETTINGS, STR_NULL),
								NWidget(NWID_SPACER), SetFill(true, false),
							EndContainer(),
						EndContainer(),
						NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(5, 5, 5),
							NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NGWW_JOIN), SetFill(true, false), SetDataTip(STR_NETWORK_SERVER_LIST_JOIN_GAME, STR_NULL),
							NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NGWW_REFRESH), SetFill(true, false), SetDataTip(STR_NETWORK_SERVER_LIST_REFRESH, STR_NETWORK_SERVER_LIST_REFRESH_TOOLTIP),
						EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),
			/* BOTTOM */
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_VERTICAL),
					NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(10, 7, 4),
						NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NGWW_FIND), SetResize(1, 0), SetFill(true, false), SetDataTip(STR_NETWORK_SERVER_LIST_FIND_SERVER, STR_NETWORK_SERVER_LIST_FIND_SERVER_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NGWW_ADD), SetResize(1, 0), SetFill(true, false), SetDataTip(STR_NETWORK_SERVER_LIST_ADD_SERVER, STR_NETWORK_SERVER_LIST_ADD_SERVER_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NGWW_START), SetResize(1, 0), SetFill(true, false), SetDataTip(STR_NETWORK_SERVER_LIST_START_SERVER, STR_NETWORK_SERVER_LIST_START_SERVER_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NGWW_CANCEL), SetResize(1, 0), SetFill(true, false), SetDataTip(STR_BUTTON_CANCEL, STR_NULL),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(0, 6), SetResize(1, 0), SetFill(true, false),
				EndContainer(),
				NWidget(NWID_VERTICAL),
					NWidget(NWID_SPACER), SetFill(false, true),
					NWidget(WWT_RESIZEBOX, COLOUR_LIGHT_BLUE, NGWW_RESIZE),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _network_game_window_desc(
	WDP_CENTER, WDP_CENTER, 450, 264, 1000, 730,
	WC_NETWORK_WINDOW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_nested_network_game_widgets, lengthof(_nested_network_game_widgets)
);

void ShowNetworkGameWindow()
{
	static bool first = true;
	DeleteWindowById(WC_NETWORK_WINDOW, 0);

	/* Only show once */
	if (first) {
		first = false;
		/* add all servers from the config file to our list */
		for (char **iter = _network_host_list.Begin(); iter != _network_host_list.End(); iter++) {
			NetworkAddServer(*iter);
		}
	}

	new NetworkGameWindow(&_network_game_window_desc);
}

/** Enum for NetworkStartServerWindow, referring to _network_start_server_window_widgets */
enum NetworkStartServerWidgets {
	NSSW_CLOSE,             ///< Close 'X' button
	NSSW_CAPTION,
	NSSW_BACKGROUND,
	NSSW_GAMENAME_LABEL,
	NSSW_GAMENAME,          ///< Background for editbox to set game name
	NSSW_SETPWD,            ///< 'Set password' button
	NSSW_SELECT_MAP_LABEL,
	NSSW_SELMAP,            ///< 'Select map' list
	NSSW_SCROLLBAR,
	NSSW_CONNTYPE_LABEL,
	NSSW_CONNTYPE_BTN,      ///< 'Connection type' droplist button
	NSSW_CLIENTS_LABEL,
	NSSW_CLIENTS_BTND,      ///< 'Max clients' downarrow
	NSSW_CLIENTS_TXT,       ///< 'Max clients' text
	NSSW_CLIENTS_BTNU,      ///< 'Max clients' uparrow
	NSSW_COMPANIES_LABEL,
	NSSW_COMPANIES_BTND,    ///< 'Max companies' downarrow
	NSSW_COMPANIES_TXT,     ///< 'Max companies' text
	NSSW_COMPANIES_BTNU,    ///< 'Max companies' uparrow
	NSSW_SPECTATORS_LABEL,
	NSSW_SPECTATORS_BTND,   ///< 'Max spectators' downarrow
	NSSW_SPECTATORS_TXT,    ///< 'Max spectators' text
	NSSW_SPECTATORS_BTNU,   ///< 'Max spectators' uparrow

	NSSW_LANGUAGE_LABEL,
	NSSW_LANGUAGE_BTN,      ///< 'Language spoken' droplist button
	NSSW_START,             ///< 'Start' button
	NSSW_LOAD,              ///< 'Load' button
	NSSW_CANCEL,            ///< 'Cancel' button
};

struct NetworkStartServerWindow : public QueryStringBaseWindow {
	byte field;                  ///< Selected text-field
	FiosItem *map;               ///< Selected map
	byte widget_id;              ///< The widget that has the pop-up input menu

	NetworkStartServerWindow(const WindowDesc *desc) : QueryStringBaseWindow(NETWORK_NAME_LENGTH)
	{
		this->InitNested(desc, 0);

		ttd_strlcpy(this->edit_str_buf, _settings_client.network.server_name, this->edit_str_size);

		_saveload_mode = SLD_NEW_GAME;
		BuildFileList();
		this->vscroll.SetCapacity(14);
		this->vscroll.SetCount(_fios_items.Length() + 1);

		this->afilter = CS_ALPHANUMERAL;
		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, 160);
		this->SetFocusedWidget(NSSW_GAMENAME);

		this->field = NSSW_GAMENAME;
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case NSSW_CONNTYPE_BTN:
				SetDParam(0, _connection_types_dropdown[_settings_client.network.server_advertise]);
				break;

			case NSSW_CLIENTS_TXT:
				SetDParam(0, _settings_client.network.max_clients);
				break;

			case NSSW_COMPANIES_TXT:
				SetDParam(0, _settings_client.network.max_companies);
				break;

			case NSSW_SPECTATORS_TXT:
				SetDParam(0, _settings_client.network.max_spectators);
				break;

			case NSSW_LANGUAGE_BTN:
				SetDParam(0, STR_NETWORK_LANG_ANY + _settings_client.network.server_lang);
				break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		switch (widget) {
			case NSSW_CONNTYPE_BTN:
				*size = maxdim(GetStringBoundingBox(_connection_types_dropdown[0]), GetStringBoundingBox(_connection_types_dropdown[1]));
				size->width += padding.width;
				size->height += padding.height;
				break;

			case NSSW_SELMAP:
				resize->height = FONT_HEIGHT_NORMAL;
				size->height = 14 * resize->height + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case NSSW_SELMAP:
				this->DrawMapSelection(r);
				break;

			case NSSW_SETPWD:
				/* if password is set, draw red '*' next to 'Set password' button */
				if (!StrEmpty(_settings_client.network.server_password)) DrawString(r.right + WD_FRAMERECT_LEFT, this->width - WD_FRAMERECT_RIGHT, r.top, "*", TC_RED);
		}
	}

	virtual void OnPaint()
	{
		/* draw basic widgets */
		this->DrawWidgets();

		/* editbox to set game name */
		this->DrawEditBox(NSSW_GAMENAME);
	}

	void DrawMapSelection(const Rect &r) const
	{
		int y = r.top + WD_FRAMERECT_TOP;
		/* draw list of maps */
		GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, 0xD7);  // black background of maps list

		for (uint pos = this->vscroll.GetPosition(); pos < _fios_items.Length() + 1; pos++) {
			const FiosItem *item = _fios_items.Get(pos - 1);
			if (item == this->map || (pos == 0 && this->map == NULL)) {
				GfxFillRect(r.left + 1, y, r.right - 1, y + FONT_HEIGHT_NORMAL - 1, 155); // show highlighted item with a different colour
			}

			if (pos == 0) {
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_START_SERVER_SERVER_RANDOM_GAME, TC_DARK_GREEN);
			} else {
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, item->title, _fios_colours[item->type] );
			}
			y += FONT_HEIGHT_NORMAL;

			if (y >= this->vscroll.GetCapacity() * FONT_HEIGHT_NORMAL + r.top) break;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		this->field = widget;
		switch (widget) {
			case NSSW_CLOSE:  // Close 'X'
			case NSSW_CANCEL: // Cancel button
				ShowNetworkGameWindow();
				break;

			case NSSW_SETPWD: // Set password button
				this->widget_id = NSSW_SETPWD;
				SetDParamStr(0, _settings_client.network.server_password);
				ShowQueryString(STR_JUST_RAW_STRING, STR_NETWORK_START_SERVER_SET_PASSWORD, 20, 250, this, CS_ALPHANUMERAL, QSF_NONE);
				break;

			case NSSW_SELMAP: { // Select map
				int y = (pt.y - this->GetWidget<NWidgetCore>(NSSW_SELMAP)->pos_y - WD_FRAMERECT_TOP) / FONT_HEIGHT_NORMAL;

				y += this->vscroll.GetPosition();
				if (y >= this->vscroll.GetCount()) return;

				this->map = (y == 0) ? NULL : _fios_items.Get(y - 1);
				this->SetDirty();
			} break;

			case NSSW_CONNTYPE_BTN: // Connection type
				ShowDropDownMenu(this, _connection_types_dropdown, _settings_client.network.server_advertise, NSSW_CONNTYPE_BTN, 0, 0); // do it for widget NSSW_CONNTYPE_BTN
				break;

			case NSSW_CLIENTS_BTND:    case NSSW_CLIENTS_BTNU:    // Click on up/down button for number of clients
			case NSSW_COMPANIES_BTND:  case NSSW_COMPANIES_BTNU:  // Click on up/down button for number of companies
			case NSSW_SPECTATORS_BTND: case NSSW_SPECTATORS_BTNU: // Click on up/down button for number of spectators
				/* Don't allow too fast scrolling */
				if ((this->flags4 & WF_TIMEOUT_MASK) <= WF_TIMEOUT_TRIGGER) {
					this->HandleButtonClick(widget);
					this->SetDirty();
					switch (widget) {
						default: NOT_REACHED();
						case NSSW_CLIENTS_BTND: case NSSW_CLIENTS_BTNU:
							_settings_client.network.max_clients    = Clamp(_settings_client.network.max_clients    + widget - NSSW_CLIENTS_TXT,    2, MAX_CLIENTS);
							break;
						case NSSW_COMPANIES_BTND: case NSSW_COMPANIES_BTNU:
							_settings_client.network.max_companies  = Clamp(_settings_client.network.max_companies  + widget - NSSW_COMPANIES_TXT,  1, MAX_COMPANIES);
							break;
						case NSSW_SPECTATORS_BTND: case NSSW_SPECTATORS_BTNU:
							_settings_client.network.max_spectators = Clamp(_settings_client.network.max_spectators + widget - NSSW_SPECTATORS_TXT, 0, MAX_CLIENTS);
							break;
					}
				}
				_left_button_clicked = false;
				break;

			case NSSW_CLIENTS_TXT:    // Click on number of clients
				this->widget_id = NSSW_CLIENTS_TXT;
				SetDParam(0, _settings_client.network.max_clients);
				ShowQueryString(STR_JUST_INT, STR_NETWORK_START_SERVER_NUMBER_OF_CLIENTS,    4, 50, this, CS_NUMERAL, QSF_NONE);
				break;

			case NSSW_COMPANIES_TXT:  // Click on number of companies
				this->widget_id = NSSW_COMPANIES_TXT;
				SetDParam(0, _settings_client.network.max_companies);
				ShowQueryString(STR_JUST_INT, STR_NETWORK_START_SERVER_NUMBER_OF_COMPANIES,  3, 50, this, CS_NUMERAL, QSF_NONE);
				break;

			case NSSW_SPECTATORS_TXT: // Click on number of spectators
				this->widget_id = NSSW_SPECTATORS_TXT;
				SetDParam(0, _settings_client.network.max_spectators);
				ShowQueryString(STR_JUST_INT, STR_NETWORK_START_SERVER_NUMBER_OF_SPECTATORS, 4, 50, this, CS_NUMERAL, QSF_NONE);
				break;

			case NSSW_LANGUAGE_BTN: { // Language
				uint sel = 0;
				for (uint i = 0; i < lengthof(_language_dropdown) - 1; i++) {
					if (_language_dropdown[i] == STR_NETWORK_LANG_ANY + _settings_client.network.server_lang) {
						sel = i;
						break;
					}
				}
				ShowDropDownMenu(this, _language_dropdown, sel, NSSW_LANGUAGE_BTN, 0, 0);
			} break;

			case NSSW_START: // Start game
				_is_network_server = true;

				if (this->map == NULL) { // start random new game
					ShowGenerateLandscape();
				} else { // load a scenario
					const char *name = FiosBrowseTo(this->map);
					if (name != NULL) {
						SetFiosType(this->map->type);
						_file_to_saveload.filetype = FT_SCENARIO;
						strecpy(_file_to_saveload.name, name, lastof(_file_to_saveload.name));
						strecpy(_file_to_saveload.title, this->map->title, lastof(_file_to_saveload.title));

						delete this;
						SwitchToMode(SM_START_SCENARIO);
					}
				}
				break;

			case NSSW_LOAD: // Load game
				_is_network_server = true;
				/* XXX - WC_NETWORK_WINDOW (this window) should stay, but if it stays, it gets
				 * copied all the elements of 'load game' and upon closing that, it segfaults */
				delete this;
				ShowSaveLoadDialog(SLD_LOAD_GAME);
				break;
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case NSSW_CONNTYPE_BTN:
				_settings_client.network.server_advertise = (index != 0);
				break;
			case NSSW_LANGUAGE_BTN:
				_settings_client.network.server_lang = _language_dropdown[index] - STR_NETWORK_LANG_ANY;
				break;
			default:
				NOT_REACHED();
		}

		this->SetDirty();
	}

	virtual void OnMouseLoop()
	{
		if (this->field == NSSW_GAMENAME) this->HandleEditBox(NSSW_GAMENAME);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		EventState state = ES_NOT_HANDLED;
		if (this->field == NSSW_GAMENAME) {
			if (this->HandleEditBoxKey(NSSW_GAMENAME, key, keycode, state) == HEBR_CONFIRM) return state;

			strecpy(_settings_client.network.server_name, this->text.buf, lastof(_settings_client.network.server_name));
		}

		return state;
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		if (this->widget_id == NSSW_SETPWD) {
			strecpy(_settings_client.network.server_password, str, lastof(_settings_client.network.server_password));
		} else {
			int32 value = atoi(str);
			this->SetWidgetDirty(this->widget_id);
			switch (this->widget_id) {
				default: NOT_REACHED();
				case NSSW_CLIENTS_TXT:    _settings_client.network.max_clients    = Clamp(value, 2, MAX_CLIENTS); break;
				case NSSW_COMPANIES_TXT:  _settings_client.network.max_companies  = Clamp(value, 1, MAX_COMPANIES); break;
				case NSSW_SPECTATORS_TXT: _settings_client.network.max_spectators = Clamp(value, 0, MAX_CLIENTS); break;
			}
		}

		this->SetDirty();
	}
};

static const NWidgetPart _nested_network_start_server_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE, NSSW_CLOSE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE, NSSW_CAPTION), SetDataTip(STR_NETWORK_START_SERVER_CAPTION, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, NSSW_BACKGROUND),
		NWidget(NWID_HORIZONTAL), SetPIP(10, 8, 10),
			NWidget(NWID_VERTICAL), SetPIP(10, 0, 10),
				/* Game name widgets */
				NWidget(NWID_HORIZONTAL), SetPIP(0, 2, 0),
					NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, NSSW_GAMENAME_LABEL), SetDataTip(STR_NETWORK_START_SERVER_NEW_GAME_NAME, STR_NULL),
					NWidget(WWT_EDITBOX, COLOUR_LIGHT_BLUE, NSSW_GAMENAME), SetMinimalSize(10, 12), SetFill(true, false),
														SetDataTip(STR_NETWORK_START_SERVER_NEW_GAME_NAME_OSKTITLE, STR_NETWORK_START_SERVER_NEW_GAME_NAME_TOOLTIP),
				EndContainer(),
				/* List of playable scenarios. */
				NWidget(NWID_SPACER), SetMinimalSize(0, 8),
				NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, NSSW_SELECT_MAP_LABEL), SetFill(true, false), SetDataTip(STR_NETWORK_START_SERVER_SELECT_MAP, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 6),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, NSSW_SELMAP), SetMinimalSize(250, 0), SetFill(true, true), SetDataTip(STR_NULL, STR_NETWORK_START_SERVER_SELECT_MAP_TOOLTIP), EndContainer(),
					NWidget(WWT_SCROLLBAR, COLOUR_LIGHT_BLUE, NSSW_SCROLLBAR),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_VERTICAL), SetPIP(10, 0, 10),
				/* Password widgets. */
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NSSW_SETPWD), SetFill(true, false),
														SetDataTip(STR_NETWORK_START_SERVER_SET_PASSWORD, STR_NETWORK_START_SERVER_PASSWORD_TOOLTIP),
				/* Combo/selection boxes to control Connection Type / Max Clients / Max Companies / Max Observers / Language */
				NWidget(NWID_SPACER), SetFill(true, true),
				NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, NSSW_CONNTYPE_LABEL), SetFill(true, false), SetDataTip(STR_NETWORK_SERVER_LIST_CONNECTION, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
				NWidget(WWT_DROPDOWN, COLOUR_LIGHT_BLUE, NSSW_CONNTYPE_BTN), SetFill(true, false),
												SetDataTip(STR_BLACK_STRING, STR_NETWORK_SERVER_LIST_CONNECTION_TOOLTIP),

				NWidget(NWID_SPACER), SetMinimalSize(0, 6),
				NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, NSSW_CLIENTS_LABEL), SetFill(true, false), SetDataTip(STR_NETWORK_START_SERVER_NUMBER_OF_CLIENTS, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_PUSHIMGBTN, COLOUR_LIGHT_BLUE, NSSW_CLIENTS_BTND), SetMinimalSize(12, 12), SetFill(false, true),
												SetDataTip(SPR_ARROW_DOWN, STR_NETWORK_START_SERVER_NUMBER_OF_CLIENTS_TOOLTIP),
					NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, NSSW_CLIENTS_TXT), SetFill(true, false),
												SetDataTip(STR_NETWORK_START_SERVER_CLIENTS_SELECT, STR_NETWORK_START_SERVER_NUMBER_OF_CLIENTS_TOOLTIP),
					NWidget(WWT_PUSHIMGBTN, COLOUR_LIGHT_BLUE, NSSW_CLIENTS_BTNU), SetMinimalSize(12, 12), SetFill(false, true),
												SetDataTip(SPR_ARROW_UP, STR_NETWORK_START_SERVER_NUMBER_OF_CLIENTS_TOOLTIP),
				EndContainer(),

				NWidget(NWID_SPACER), SetMinimalSize(0, 6),
				NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, NSSW_COMPANIES_LABEL), SetFill(true, false), SetDataTip(STR_NETWORK_START_SERVER_NUMBER_OF_COMPANIES, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_PUSHIMGBTN, COLOUR_LIGHT_BLUE, NSSW_COMPANIES_BTND), SetMinimalSize(12, 12), SetFill(false, true),
												SetDataTip(SPR_ARROW_DOWN, STR_NETWORK_START_SERVER_NUMBER_OF_COMPANIES_TOOLTIP),
					NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, NSSW_COMPANIES_TXT), SetFill(true, false),
												SetDataTip(STR_NETWORK_START_SERVER_COMPANIES_SELECT, STR_NETWORK_START_SERVER_NUMBER_OF_COMPANIES_TOOLTIP),
					NWidget(WWT_PUSHIMGBTN, COLOUR_LIGHT_BLUE, NSSW_COMPANIES_BTNU), SetMinimalSize(12, 12), SetFill(false, true),
												SetDataTip(SPR_ARROW_UP, STR_NETWORK_START_SERVER_NUMBER_OF_COMPANIES_TOOLTIP),
				EndContainer(),

				NWidget(NWID_SPACER), SetMinimalSize(0, 6),
				NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, NSSW_SPECTATORS_LABEL), SetFill(true, false), SetDataTip(STR_NETWORK_START_SERVER_NUMBER_OF_SPECTATORS, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_PUSHIMGBTN, COLOUR_LIGHT_BLUE, NSSW_SPECTATORS_BTND), SetMinimalSize(12, 12), SetFill(false, true),
												SetDataTip(SPR_ARROW_DOWN, STR_NETWORK_START_SERVER_NUMBER_OF_SPECTATORS_TOOLTIP),
					NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, NSSW_SPECTATORS_TXT), SetFill(true, false),
												SetDataTip(STR_NETWORK_START_SERVER_SPECTATORS_SELECT, STR_NETWORK_START_SERVER_NUMBER_OF_SPECTATORS_TOOLTIP),
					NWidget(WWT_PUSHIMGBTN, COLOUR_LIGHT_BLUE, NSSW_SPECTATORS_BTNU), SetMinimalSize(12, 12), SetFill(false, true),
												SetDataTip(SPR_ARROW_UP, STR_NETWORK_START_SERVER_NUMBER_OF_SPECTATORS_TOOLTIP),
				EndContainer(),

				NWidget(NWID_SPACER), SetMinimalSize(0, 6),
				NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, NSSW_LANGUAGE_LABEL), SetFill(true, false), SetDataTip(STR_NETWORK_START_SERVER_LANGUAGE_SPOKEN, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
				NWidget(WWT_DROPDOWN, COLOUR_LIGHT_BLUE, NSSW_LANGUAGE_BTN), SetFill(true, false),
												SetDataTip(STR_BLACK_STRING, STR_NETWORK_START_SERVER_LANGUAGE_TOOLTIP),
			EndContainer(),
		EndContainer(),
		/* Buttons Start / Load / Cancel. */
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(10, 5, 10),
			NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NSSW_START), SetFill(true, false), SetDataTip(STR_NETWORK_START_SERVER_START_GAME, STR_NETWORK_START_SERVER_START_GAME_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NSSW_LOAD), SetFill(true, false), SetDataTip(STR_NETWORK_START_SERVER_LOAD_GAME, STR_NETWORK_START_SERVER_LOAD_GAME_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NSSW_CANCEL), SetFill(true, false), SetDataTip(STR_BUTTON_CANCEL, STR_NULL),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 10),
	EndContainer(),
};

static const WindowDesc _network_start_server_window_desc(
	WDP_CENTER, WDP_CENTER, 420, 244, 420, 244,
	WC_NETWORK_WINDOW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_nested_network_start_server_window_widgets, lengthof(_nested_network_start_server_window_widgets)
);

static void ShowNetworkStartServerWindow()
{
	DeleteWindowById(WC_NETWORK_WINDOW, 0);

	new NetworkStartServerWindow(&_network_start_server_window_desc);
}

/** Enum for NetworkLobbyWindow, referring to _network_lobby_window_widgets */
enum NetworkLobbyWindowWidgets {
	NLWW_CLOSE,      ///< Close 'X' button
	NLWW_CAPTION,    ///< Titlebar
	NLWW_BACKGROUND, ///< Background panel
	NLWW_TEXT,       ///< Heading text
	NLWW_HEADER,     ///< Header above list of companies
	NLWW_MATRIX,     ///< List of companies
	NLWW_SCROLLBAR,  ///< Scroll bar
	NLWW_DETAILS,    ///< Company details
	NLWW_JOIN,       ///< 'Join company' button
	NLWW_NEW,        ///< 'New company' button
	NLWW_SPECTATE,   ///< 'Spectate game' button
	NLWW_REFRESH,    ///< 'Refresh server' button
	NLWW_CANCEL,     ///< 'Cancel' button
};

struct NetworkLobbyWindow : public Window {
	CompanyID company;       ///< Select company
	NetworkGameList *server; ///< Selected server
	NetworkCompanyInfo company_info[MAX_COMPANIES];

	NetworkLobbyWindow(const WindowDesc *desc, NetworkGameList *ngl) :
			Window(), company(INVALID_COMPANY), server(ngl)
	{
		this->InitNested(desc, 0);
		this->OnResize();
	}

	CompanyID NetworkLobbyFindCompanyIndex(byte pos) const
	{
		/* Scroll through all this->company_info and get the 'pos' item that is not empty */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			if (!StrEmpty(this->company_info[i].company_name)) {
				if (pos-- == 0) return i;
			}
		}

		return COMPANY_FIRST;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		switch (widget) {
			case NLWW_HEADER:
				size->height = WD_MATRIX_TOP + FONT_HEIGHT_NORMAL + WD_MATRIX_BOTTOM;;
				break;

			case NLWW_MATRIX:
				resize->height = WD_MATRIX_TOP + FONT_HEIGHT_NORMAL + WD_MATRIX_BOTTOM;
				size->height = 10 * resize->height;
				break;

			case NLWW_DETAILS:
				size->height = 30 + 11 * FONT_HEIGHT_NORMAL;
				break;
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case NLWW_TEXT:
				SetDParamStr(0, this->server->info.server_name);
				break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case NLWW_DETAILS:
				this->DrawDetails(r);
				break;

			case NLWW_MATRIX:
				this->DrawMatrix(r);
				break;
		}
	}

	virtual void OnPaint()
	{
		const NetworkGameInfo *gi = &this->server->info;

		/* Join button is disabled when no company is selected and for AI companies*/
		this->SetWidgetDisabledState(NLWW_JOIN, this->company == INVALID_COMPANY || GetLobbyCompanyInfo(this->company)->ai);
		/* Cannot start new company if there are too many */
		this->SetWidgetDisabledState(NLWW_NEW, gi->companies_on >= gi->companies_max);
		/* Cannot spectate if there are too many spectators */
		this->SetWidgetDisabledState(NLWW_SPECTATE, gi->spectators_on >= gi->spectators_max);

		this->vscroll.SetCount(gi->companies_on);

		/* Draw window widgets */
		this->DrawWidgets();
	}

	void DrawMatrix(const Rect &r) const
	{
		int y = r.top + WD_MATRIX_TOP;
		/* Draw company list */
		int pos = this->vscroll.GetPosition();
		while (pos < this->server->info.companies_on) {
			byte company = NetworkLobbyFindCompanyIndex(pos);
			bool income = false;
			if (this->company == company) {
				GfxFillRect(r.left + 1, y - 2, r.right - 1, y + FONT_HEIGHT_NORMAL, 10); // show highlighted item with a different colour
			}

			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT - 20, y, this->company_info[company].company_name, TC_BLACK);
			if (this->company_info[company].use_password != 0) DrawSprite(SPR_LOCK, PAL_NONE, r.right - WD_FRAMERECT_RIGHT - 20, y);

			/* If the company's income was positive puts a green dot else a red dot */
			if (this->company_info[company].income >= 0) income = true;
			DrawSprite(SPR_BLOT, income ? PALETTE_TO_GREEN : PALETTE_TO_RED, r.right - WD_FRAMERECT_RIGHT - 10, y);

			pos++;
			y += this->resize.step_height;
			if (pos >= this->vscroll.GetPosition() + this->vscroll.GetCapacity()) break;
		}
	}

	void DrawDetails(const Rect &r) const
	{
		const int detail_height = 12 + FONT_HEIGHT_NORMAL + 12;
		/* Draw info about selected company when it is selected in the left window */
		GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.top + detail_height - 1, 157);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + 12, STR_NETWORK_GAME_LOBBY_COMPANY_INFO, TC_FROMSTRING, SA_CENTER);

		if (this->company == INVALID_COMPANY || StrEmpty(this->company_info[this->company].company_name)) return;

		int y = r.top + detail_height + 4;
		const NetworkGameInfo *gi = &this->server->info;

		SetDParam(0, gi->clients_on);
		SetDParam(1, gi->clients_max);
		SetDParam(2, gi->companies_on);
		SetDParam(3, gi->companies_max);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_CLIENTS);
		y += FONT_HEIGHT_NORMAL;

		SetDParamStr(0, this->company_info[this->company].company_name);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_GAME_LOBBY_COMPANY_NAME);
		y += FONT_HEIGHT_NORMAL;

		SetDParam(0, this->company_info[this->company].inaugurated_year);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_GAME_LOBBY_INAUGURATION_YEAR); // inauguration year
		y += FONT_HEIGHT_NORMAL;

		SetDParam(0, this->company_info[this->company].company_value);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_GAME_LOBBY_VALUE); // company value
		y += FONT_HEIGHT_NORMAL;

		SetDParam(0, this->company_info[this->company].money);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_GAME_LOBBY_CURRENT_BALANCE); // current balance
		y += FONT_HEIGHT_NORMAL;

		SetDParam(0, this->company_info[this->company].income);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_GAME_LOBBY_LAST_YEARS_INCOME); // last year's income
		y += FONT_HEIGHT_NORMAL;

		SetDParam(0, this->company_info[this->company].performance);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_GAME_LOBBY_PERFORMANCE); // performance
		y += FONT_HEIGHT_NORMAL;

		for (uint i = 0; i < lengthof(this->company_info[this->company].num_vehicle); i++) {
			SetDParam(i, this->company_info[this->company].num_vehicle[i]);
		}
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_GAME_LOBBY_VEHICLES); // vehicles
		y += FONT_HEIGHT_NORMAL;

		for (uint i = 0; i < lengthof(this->company_info[this->company].num_station); i++) {
			SetDParam(i, this->company_info[this->company].num_station[i]);
		}
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_GAME_LOBBY_STATIONS); // stations
		y += FONT_HEIGHT_NORMAL;

		SetDParamStr(0, this->company_info[this->company].clients);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_GAME_LOBBY_PLAYERS); // players
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case NLWW_CLOSE:    // Close 'X'
			case NLWW_CANCEL:   // Cancel button
				ShowNetworkGameWindow();
				break;

			case NLWW_MATRIX: { // Company list
				uint32 id_v = (pt.y - this->GetWidget<NWidgetCore>(NLWW_MATRIX)->pos_y) / this->resize.step_height;

				if (id_v >= this->vscroll.GetCapacity()) break;

				id_v += this->vscroll.GetPosition();
				this->company = (id_v >= this->server->info.companies_on) ? INVALID_COMPANY : NetworkLobbyFindCompanyIndex(id_v);
				this->SetDirty();
			} break;

			case NLWW_JOIN:     // Join company
				/* Button can be clicked only when it is enabled */
				NetworkClientConnectGame(NetworkAddress(_settings_client.network.last_host, _settings_client.network.last_port), this->company);
				break;

			case NLWW_NEW:      // New company
				NetworkClientConnectGame(NetworkAddress(_settings_client.network.last_host, _settings_client.network.last_port), COMPANY_NEW_COMPANY);
				break;

			case NLWW_SPECTATE: // Spectate game
				NetworkClientConnectGame(NetworkAddress(_settings_client.network.last_host, _settings_client.network.last_port), COMPANY_SPECTATOR);
				break;

			case NLWW_REFRESH:  // Refresh
				NetworkTCPQueryServer(NetworkAddress(_settings_client.network.last_host, _settings_client.network.last_port)); // company info
				NetworkUDPQueryServer(NetworkAddress(_settings_client.network.last_host, _settings_client.network.last_port)); // general data
				/* Clear the information so removed companies don't remain */
				memset(this->company_info, 0, sizeof(this->company_info));
				break;
		}
	}

	virtual void OnDoubleClick(Point pt, int widget)
	{
		if (widget == NLWW_MATRIX) {
			/* is the Join button enabled? */
			if (!this->IsWidgetDisabled(NLWW_JOIN)) this->OnClick(pt, NLWW_JOIN);
		}
	}

	virtual void OnResize()
	{
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(NLWW_MATRIX);
		this->vscroll.SetCapacity(nwi->current_y / this->resize.step_height);
		nwi->widget_data = (this->vscroll.GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}
};

static const NWidgetPart _nested_network_lobby_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE, NLWW_CLOSE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE, NLWW_CAPTION), SetDataTip(STR_NETWORK_GAME_LOBBY_CAPTION, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, NLWW_BACKGROUND),
		NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, NLWW_TEXT), SetDataTip(STR_NETWORK_GAME_LOBBY_PREPARE_TO_JOIN, STR_NULL), SetResize(1, 0), SetPadding(10, 10, 0, 10),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
		NWidget(NWID_HORIZONTAL), SetPIP(10, 0, 10),
			/* Company list. */
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PANEL, COLOUR_WHITE, NLWW_HEADER), SetMinimalSize(146, 0), SetResize(1, 0), SetFill(true, false), EndContainer(),
				NWidget(WWT_MATRIX, COLOUR_LIGHT_BLUE, NLWW_MATRIX), SetMinimalSize(146, 0), SetResize(1, 1), SetFill(true, true), SetDataTip(0, STR_NETWORK_GAME_LOBBY_COMPANY_LIST_TOOLTIP),
			EndContainer(),
			NWidget(WWT_SCROLLBAR, COLOUR_LIGHT_BLUE, NLWW_SCROLLBAR),
			NWidget(NWID_SPACER), SetMinimalSize(5, 0), SetResize(0, 1),
			/* Company info. */
			NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, NLWW_DETAILS), SetMinimalSize(232, 0), SetResize(1, 1), SetFill(true, true), EndContainer(),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 9),
		/* Buttons. */
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(10, 3, 10),
			NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(0, 3, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NLWW_JOIN), SetResize(1, 0), SetFill(true, false), SetDataTip(STR_NETWORK_GAME_LOBBY_JOIN_COMPANY, STR_NETWORK_GAME_LOBBY_JOIN_COMPANY_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NLWW_NEW), SetResize(1, 0), SetFill(true, false), SetDataTip(STR_NETWORK_GAME_LOBBY_NEW_COMPANY, STR_NETWORK_GAME_LOBBY_NEW_COMPANY_TOOLTIP),
			EndContainer(),
			NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(0, 3, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NLWW_SPECTATE), SetResize(1, 0), SetFill(true, false), SetDataTip(STR_NETWORK_GAME_LOBBY_SPECTATE_GAME, STR_NETWORK_GAME_LOBBY_SPECTATE_GAME_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NLWW_REFRESH), SetResize(1, 0), SetFill(true, false), SetDataTip(STR_NETWORK_SERVER_LIST_REFRESH, STR_NETWORK_SERVER_LIST_REFRESH_TOOLTIP),
			EndContainer(),
			NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(0, 3, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NLWW_CANCEL), SetResize(1, 0), SetFill(true, false), SetDataTip(STR_BUTTON_CANCEL, STR_NULL),
				NWidget(NWID_SPACER), SetFill(true, true),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 8),
	EndContainer(),
};

static const WindowDesc _network_lobby_window_desc(
	WDP_CENTER, WDP_CENTER, 0, 0, 0, 0,
	WC_NETWORK_WINDOW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_nested_network_lobby_window_widgets, lengthof(_nested_network_lobby_window_widgets)
);

/* Show the networklobbywindow with the selected server
 * @param ngl Selected game pointer which is passed to the new window */
static void ShowNetworkLobbyWindow(NetworkGameList *ngl)
{
	DeleteWindowById(WC_NETWORK_WINDOW, 0);

	NetworkTCPQueryServer(NetworkAddress(_settings_client.network.last_host, _settings_client.network.last_port)); // company info
	NetworkUDPQueryServer(NetworkAddress(_settings_client.network.last_host, _settings_client.network.last_port)); // general data

	new NetworkLobbyWindow(&_network_lobby_window_desc, ngl);
}

/**
 * Get the company information of a given company to fill for the lobby.
 * @param company the company to get the company info struct from.
 * @return the company info struct to write the (downloaded) data to.
 */
NetworkCompanyInfo *GetLobbyCompanyInfo(CompanyID company)
{
	NetworkLobbyWindow *lobby = dynamic_cast<NetworkLobbyWindow*>(FindWindowById(WC_NETWORK_WINDOW, 0));
	return (lobby != NULL && company < MAX_COMPANIES) ? &lobby->company_info[company] : NULL;
}

/* The window below gives information about the connected clients
 *  and also makes able to give money to them, kick them (if server)
 *  and stuff like that. */

extern void DrawCompanyIcon(CompanyID cid, int x, int y);

/* Every action must be of this form */
typedef void ClientList_Action_Proc(byte client_no);

static const NWidgetPart _nested_client_list_popup_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_GREY, 0), EndContainer(),
};

static const WindowDesc _client_list_popup_desc(
	WDP_AUTO, WDP_AUTO, 150, 1, 150, 1,
	WC_TOOLBAR_MENU, WC_CLIENT_LIST,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET,
	_nested_client_list_popup_widgets, lengthof(_nested_client_list_popup_widgets)
);

/* Finds the Xth client-info that is active */
static NetworkClientInfo *NetworkFindClientInfo(byte client_no)
{
	NetworkClientInfo *ci;

	FOR_ALL_CLIENT_INFOS(ci) {
		if (client_no == 0) return ci;
		client_no--;
	}

	return NULL;
}

/* Here we start to define the options out of the menu */
static void ClientList_Kick(byte client_no)
{
	const NetworkClientInfo *ci = NetworkFindClientInfo(client_no);

	if (ci == NULL) return;

	NetworkServerKickClient(ci->client_id);
}

static void ClientList_Ban(byte client_no)
{
	NetworkClientInfo *ci = NetworkFindClientInfo(client_no);

	if (ci == NULL) return;

	NetworkServerBanIP(GetClientIP(ci));
}

static void ClientList_GiveMoney(byte client_no)
{
	if (NetworkFindClientInfo(client_no) != NULL) {
		ShowNetworkGiveMoneyWindow(NetworkFindClientInfo(client_no)->client_playas);
	}
}

static void ClientList_SpeakToClient(byte client_no)
{
	if (NetworkFindClientInfo(client_no) != NULL) {
		ShowNetworkChatQueryWindow(DESTTYPE_CLIENT, NetworkFindClientInfo(client_no)->client_id);
	}
}

static void ClientList_SpeakToCompany(byte client_no)
{
	if (NetworkFindClientInfo(client_no) != NULL) {
		ShowNetworkChatQueryWindow(DESTTYPE_TEAM, NetworkFindClientInfo(client_no)->client_playas);
	}
}

static void ClientList_SpeakToAll(byte client_no)
{
	ShowNetworkChatQueryWindow(DESTTYPE_BROADCAST, 0);
}

/** Popup selection window to chose an action to perform */
struct NetworkClientListPopupWindow : Window {
	/** Container for actions that can be executed. */
	struct ClientListAction {
		StringID name;                ///< Name of the action to execute
		ClientList_Action_Proc *proc; ///< Action to execute
	};

	uint sel_index;
	int client_no;
	Point desired_location;
	SmallVector<ClientListAction, 2> actions; ///< Actions to execute

	/**
	 * Add an action to the list of actions to execute.
	 * @param name the name of the action
	 * @param proc the procedure to execute for the action
	 */
	inline void AddAction(StringID name, ClientList_Action_Proc *proc)
	{
		ClientListAction *action = this->actions.Append();
		action->name = name;
		action->proc = proc;
	}

	NetworkClientListPopupWindow(const WindowDesc *desc, int x, int y, int client_no) :
			Window(),
			sel_index(0), client_no(client_no)
	{
		this->desired_location.x = x;
		this->desired_location.y = y;

		const NetworkClientInfo *ci = NetworkFindClientInfo(client_no);

		if (_network_own_client_id != ci->client_id) {
			this->AddAction(STR_NETWORK_CLIENTLIST_SPEAK_TO_CLIENT, &ClientList_SpeakToClient);
		}

		if (Company::IsValidID(ci->client_playas) || ci->client_playas == COMPANY_SPECTATOR) {
			this->AddAction(STR_NETWORK_CLIENTLIST_SPEAK_TO_COMPANY, &ClientList_SpeakToCompany);
		}
		this->AddAction(STR_NETWORK_CLIENTLIST_SPEAK_TO_ALL, &ClientList_SpeakToAll);

		if (_network_own_client_id != ci->client_id) {
			/* We are no spectator and the company we want to give money to is no spectator and money gifts are allowed */
			if (Company::IsValidID(_local_company) && Company::IsValidID(ci->client_playas) && _settings_game.economy.give_money) {
				this->AddAction(STR_NETWORK_CLIENTLIST_GIVE_MONEY, &ClientList_GiveMoney);
			}
		}

		/* A server can kick clients (but not himself) */
		if (_network_server && _network_own_client_id != ci->client_id) {
			this->AddAction(STR_NETWORK_CLIENTLIST_KICK, &ClientList_Kick);
			this->AddAction(STR_NETWORK_CLIENTLIST_BAN, &ClientList_Ban);
		}

		this->flags4 &= ~WF_WHITE_BORDER_MASK;
		this->InitNested(desc, 0);
	}

	virtual Point OnInitialPosition(const WindowDesc *desc, int16 sm_width, int16 sm_height, int window_number)
	{
		return this->desired_location;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		Dimension d = *size;
		for (const ClientListAction *action = this->actions.Begin(); action != this->actions.End(); action++) {
			d = maxdim(GetStringBoundingBox(action->name), d);
		}

		d.height *= this->actions.Length();
		d.width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
		d.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
		*size = d;
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		/* Draw the actions */
		int sel = this->sel_index;
		int y = r.top + WD_FRAMERECT_TOP;
		for (const ClientListAction *action = this->actions.Begin(); action != this->actions.End(); action++, y += FONT_HEIGHT_NORMAL) {
			TextColour colour;
			if (sel-- == 0) { // Selected item, highlight it
				GfxFillRect(r.left + 1, y, r.right - 1, y + FONT_HEIGHT_NORMAL - 1, 0);
				colour = TC_WHITE;
			} else {
				colour = TC_BLACK;
			}

			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, action->name, colour);
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void OnMouseLoop()
	{
		/* We selected an action */
		uint index = (_cursor.pos.y - this->top - WD_FRAMERECT_TOP) / FONT_HEIGHT_NORMAL;

		if (_left_button_down) {
			if (index == this->sel_index || index >= this->actions.Length()) return;

			this->sel_index = index;
			this->SetDirty();
		} else {
			if (index < this->actions.Length() && _cursor.pos.y >= this->top) {
				this->actions[index].proc(this->client_no);
			}

			DeleteWindowById(WC_TOOLBAR_MENU, 0);
		}
	}
};

/**
 * Show the popup (action list)
 */
static void PopupClientList(int client_no, int x, int y)
{
	DeleteWindowById(WC_TOOLBAR_MENU, 0);

	if (NetworkFindClientInfo(client_no) == NULL) return;

	new NetworkClientListPopupWindow(&_client_list_popup_desc, x, y, client_no);
}


/** Widget numbers of the client list window. */
enum ClientListWidgets {
	CLW_CLOSE,
	CLW_CAPTION,
	CLW_STICKY,
	CLW_PANEL,
};

static const NWidgetPart _nested_client_list_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, CLW_CLOSE),
		NWidget(WWT_CAPTION, COLOUR_GREY, CLW_CAPTION), SetDataTip(STR_NETWORK_COMPANY_LIST_CLIENT_LIST, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_GREY, CLW_STICKY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, CLW_PANEL), SetMinimalSize(250, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM), SetResize(1, 1), EndContainer(),
};

static const WindowDesc _client_list_desc(
	WDP_AUTO, WDP_AUTO, 250, 16, 250, 16,
	WC_CLIENT_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_nested_client_list_widgets, lengthof(_nested_client_list_widgets)
);

/**
 * Main handle for clientlist
 */
struct NetworkClientListWindow : Window {
	int selected_item;

	uint server_client_width;
	uint company_icon_width;

	NetworkClientListWindow(const WindowDesc *desc, WindowNumber window_number) :
			Window(),
			selected_item(-1)
	{
		this->InitNested(desc, window_number);
	}

	/**
	 * Finds the amount of clients and set the height correct
	 */
	bool CheckClientListHeight()
	{
		int num = 0;
		const NetworkClientInfo *ci;

		/* Should be replaced with a loop through all clients */
		FOR_ALL_CLIENT_INFOS(ci) {
			if (ci->client_playas != COMPANY_INACTIVE_CLIENT) num++;
		}

		num *= FONT_HEIGHT_NORMAL;

		int diff = (num + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM) - (this->GetWidget<NWidgetBase>(CLW_PANEL)->current_y);
		/* If height is changed */
		if (diff != 0) {
			ResizeWindow(this, 0, diff);
			return false;
		}
		return true;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		if (widget != CLW_PANEL) return;

		this->server_client_width = max(GetStringBoundingBox(STR_NETWORK_SERVER).width, GetStringBoundingBox(STR_NETWORK_CLIENT).width) + WD_FRAMERECT_RIGHT;
		this->company_icon_width = GetSpriteSize(SPR_COMPANY_ICON).width + WD_FRAMERECT_LEFT;

		uint width = 200; // Default width
		const NetworkClientInfo *ci;
		FOR_ALL_CLIENT_INFOS(ci) {
			width = max(width, GetStringBoundingBox(ci->client_name).width);
		}

		size->width = WD_FRAMERECT_LEFT + this->server_client_width + this->company_icon_width + WD_FRAMERECT_RIGHT;
	}

	virtual void OnPaint()
	{
		/* Check if we need to reset the height */
		if (!this->CheckClientListHeight()) return;

		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != CLW_PANEL) return;

		int y = r.top + WD_FRAMERECT_TOP;
		int left = r.left + WD_FRAMERECT_LEFT;
		int i = 0;
		const NetworkClientInfo *ci;
		FOR_ALL_CLIENT_INFOS(ci) {
			TextColour colour;
			if (this->selected_item == i++) { // Selected item, highlight it
				GfxFillRect(r.left + 1, y, r.right - 1, y + FONT_HEIGHT_NORMAL - 1, 0);
				colour = TC_WHITE;
			} else {
				colour = TC_BLACK;
			}

			if (ci->client_id == CLIENT_ID_SERVER) {
				DrawString(left, left + this->server_client_width, y, STR_NETWORK_SERVER, colour);
			} else {
				DrawString(left, left + this->server_client_width, y, STR_NETWORK_CLIENT, colour);
			}

			/* Filter out spectators */
			if (Company::IsValidID(ci->client_playas)) DrawCompanyIcon(ci->client_playas, left + this->server_client_width, y + 1);

			DrawString(left + this->server_client_width + this->company_icon_width, r.right - WD_FRAMERECT_RIGHT, y, ci->client_name, colour);

			y += FONT_HEIGHT_NORMAL;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		/* Show the popup with option */
		if (this->selected_item != -1) {
			PopupClientList(this->selected_item, pt.x + this->left, pt.y + this->top);
		}
	}

	virtual void OnMouseOver(Point pt, int widget)
	{
		/* -1 means we left the current window */
		if (pt.y == -1) {
			this->selected_item = -1;
			this->SetDirty();
			return;
		}

		/* Find the new selected item (if any) */
		pt.y -= this->GetWidget<NWidgetBase>(CLW_PANEL)->pos_y;
		int item = -1;
		if (IsInsideMM(pt.y, WD_FRAMERECT_TOP, this->GetWidget<NWidgetBase>(CLW_PANEL)->current_y - WD_FRAMERECT_BOTTOM)) {
			item = (pt.y - WD_FRAMERECT_TOP) / FONT_HEIGHT_NORMAL;
		}

		/* It did not change.. no update! */
		if (item == this->selected_item) return;
		this->selected_item = item;

		/* Repaint */
		this->SetDirty();
	}
};

void ShowClientList()
{
	AllocateWindowDescFront<NetworkClientListWindow>(&_client_list_desc, 0);
}


static NetworkPasswordType pw_type;


void ShowNetworkNeedPassword(NetworkPasswordType npt)
{
	StringID caption;

	pw_type = npt;
	switch (npt) {
		default: NOT_REACHED();
		case NETWORK_GAME_PASSWORD:    caption = STR_NETWORK_NEED_GAME_PASSWORD_CAPTION; break;
		case NETWORK_COMPANY_PASSWORD: caption = STR_NETWORK_NEED_COMPANY_PASSWORD_CAPTION; break;
	}
	ShowQueryString(STR_EMPTY, caption, 20, 180, FindWindowById(WC_NETWORK_STATUS_WINDOW, 0), CS_ALPHANUMERAL, QSF_NONE);
}

/* Vars needed for the join-GUI */
NetworkJoinStatus _network_join_status;
uint8 _network_join_waiting;
uint32 _network_join_bytes;
uint32 _network_join_bytes_total;

/** Widgets used for the join status window. */
enum NetworkJoinStatusWidgets {
	NJSW_CAPTION,    ///< Caption of the window
	NJSW_BACKGROUND, ///< Background
	NJSW_CANCELOK,   ///< Cancel/OK button
};

struct NetworkJoinStatusWindow : Window {
	NetworkJoinStatusWindow(const WindowDesc *desc) : Window()
	{
		this->parent = FindWindowById(WC_NETWORK_WINDOW, 0);
		this->InitNested(desc, 0);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != NJSW_BACKGROUND) return;

		uint8 progress; // used for progress bar
		DrawString(r.left + 2, r.right - 2, r.top + 20, STR_NETWORK_CONNECTING_1 + _network_join_status, TC_FROMSTRING, SA_CENTER);
		switch (_network_join_status) {
			case NETWORK_JOIN_STATUS_CONNECTING: case NETWORK_JOIN_STATUS_AUTHORIZING:
			case NETWORK_JOIN_STATUS_GETTING_COMPANY_INFO:
				progress = 10; // first two stages 10%
				break;
			case NETWORK_JOIN_STATUS_WAITING:
				SetDParam(0, _network_join_waiting);
				DrawString(r.left + 2, r.right - 2, r.top + 20 + FONT_HEIGHT_NORMAL, STR_NETWORK_CONNECTING_WAITING, TC_FROMSTRING, SA_CENTER);
				progress = 15; // third stage is 15%
				break;
			case NETWORK_JOIN_STATUS_DOWNLOADING:
				SetDParam(0, _network_join_bytes);
				SetDParam(1, _network_join_bytes_total);
				DrawString(r.left + 2, r.right - 2, r.top + 20 + FONT_HEIGHT_NORMAL, STR_NETWORK_CONNECTING_DOWNLOADING, TC_FROMSTRING, SA_CENTER);
				/* Fallthrough */
			default: // Waiting is 15%, so the resting receivement of map is maximum 70%
				progress = 15 + _network_join_bytes * (100 - 15) / _network_join_bytes_total;
		}

		/* Draw nice progress bar :) */
		DrawFrameRect(r.left + 20, r.top + 5, (int)((this->width - 20) * progress / 100), r.top + 15, COLOUR_MAUVE, FR_NONE);
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget == NJSW_CANCELOK) { // Disconnect button
			NetworkDisconnect();
			SwitchToMode(SM_MENU);
			ShowNetworkGameWindow();
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (StrEmpty(str)) {
			NetworkDisconnect();
			ShowNetworkGameWindow();
		} else {
			SEND_COMMAND(PACKET_CLIENT_PASSWORD)(pw_type, str);
		}
	}
};

static const NWidgetPart _nested_network_join_status_window_widgets[] = {
	NWidget(WWT_CAPTION, COLOUR_GREY, NJSW_CAPTION), SetDataTip(STR_NETWORK_CONNECTING_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_PANEL, COLOUR_GREY, NJSW_BACKGROUND),
		NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NJSW_CANCELOK), SetMinimalSize(101, 12), SetPadding(55, 74, 4, 75), SetDataTip(STR_NETWORK_CONNECTION_DISCONNECT, STR_NULL),
	EndContainer(),
};

static const WindowDesc _network_join_status_window_desc(
	WDP_CENTER, WDP_CENTER, 250, 85, 250, 85,
	WC_NETWORK_STATUS_WINDOW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_MODAL,
	_nested_network_join_status_window_widgets, lengthof(_nested_network_join_status_window_widgets)
);

void ShowJoinStatusWindow()
{
	DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
	new NetworkJoinStatusWindow(&_network_join_status_window_desc);
}


/** Enum for NetworkGameWindow, referring to _network_game_window_widgets */
enum NetworkCompanyPasswordWindowWidgets {
	NCPWW_CLOSE,                    ///< Close 'X' button
	NCPWW_CAPTION,                  ///< Caption of the whole window
	NCPWW_BACKGROUND,               ///< The background of the interface
	NCPWW_LABEL,                    ///< Label in front of the password field
	NCPWW_PASSWORD,                 ///< Input field for the password
	NCPWW_SAVE_AS_DEFAULT_PASSWORD, ///< Toggle 'button' for saving the current password as default password
	NCPWW_CANCEL,                   ///< Close the window without changing anything
	NCPWW_OK,                       ///< Safe the password etc.
};

struct NetworkCompanyPasswordWindow : public QueryStringBaseWindow {
	NetworkCompanyPasswordWindow(const WindowDesc *desc, Window *parent) : QueryStringBaseWindow(lengthof(_settings_client.network.default_company_pass))
	{
		this->InitNested(desc, 0);

		this->parent = parent;
		this->afilter = CS_ALPHANUMERAL;
		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, 0);
		this->SetFocusedWidget(NCPWW_PASSWORD);
	}

	void OnOk()
	{
		if (this->IsWidgetLowered(NCPWW_SAVE_AS_DEFAULT_PASSWORD)) {
			snprintf(_settings_client.network.default_company_pass, lengthof(_settings_client.network.default_company_pass), "%s", this->edit_str_buf);
		}

		/* empty password is a '*' because of console argument */
		if (StrEmpty(this->edit_str_buf)) snprintf(this->edit_str_buf, this->edit_str_size, "*");
		char *password = this->edit_str_buf;
		NetworkChangeCompanyPassword(1, &password);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
		this->DrawEditBox(NCPWW_PASSWORD);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case NCPWW_OK:
				this->OnOk();

			/* FALL THROUGH */
			case NCPWW_CANCEL:
				delete this;
				break;

			case NCPWW_SAVE_AS_DEFAULT_PASSWORD:
				this->ToggleWidgetLoweredState(NCPWW_SAVE_AS_DEFAULT_PASSWORD);
				this->SetDirty();
				break;
		}
	}

	virtual void OnMouseLoop()
	{
		this->HandleEditBox(4);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		EventState state = ES_NOT_HANDLED;
		switch (this->HandleEditBoxKey(4, key, keycode, state)) {
			default: break;

			case HEBR_CONFIRM:
				this->OnOk();
				/* FALL THROUGH */

			case HEBR_CANCEL:
				delete this;
				break;
		}
		return state;
	}

	virtual void OnOpenOSKWindow(int wid)
	{
		ShowOnScreenKeyboard(this, wid, NCPWW_CANCEL, NCPWW_OK);
	}
};

static const NWidgetPart _nested_network_company_password_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, NCPWW_CLOSE),
		NWidget(WWT_CAPTION, COLOUR_GREY, NCPWW_CAPTION), SetDataTip(STR_COMPANY_PASSWORD_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, NCPWW_BACKGROUND),
		NWidget(NWID_VERTICAL), SetPIP(5, 5, 5),
			NWidget(NWID_HORIZONTAL), SetPIP(5, 5, 5),
				NWidget(WWT_TEXT, COLOUR_GREY, NCPWW_LABEL), SetDataTip(STR_COMPANY_VIEW_PASSWORD, STR_NULL),
				NWidget(WWT_EDITBOX, COLOUR_GREY, NCPWW_PASSWORD), SetMinimalSize(194, 12), SetDataTip(STR_COMPANY_VIEW_SET_PASSWORD, STR_NULL),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(5, 0, 5),
				NWidget(NWID_SPACER), SetFill(true, false),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, NCPWW_SAVE_AS_DEFAULT_PASSWORD), SetMinimalSize(194, 12),
											SetDataTip(STR_COMPANY_PASSWORD_MAKE_DEFAULT, STR_COMPANY_PASSWORD_MAKE_DEFAULT_TOOLTIP),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, NCPWW_CANCEL), SetFill(true, false), SetDataTip(STR_BUTTON_CANCEL, STR_COMPANY_PASSWORD_CANCEL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, NCPWW_OK), SetFill(true, false), SetDataTip(STR_BUTTON_OK, STR_COMPANY_PASSWORD_OK),
	EndContainer(),
};

static const WindowDesc _network_company_password_window_desc(
	WDP_AUTO, WDP_AUTO, 300, 63, 300, 63,
	WC_COMPANY_PASSWORD_WINDOW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_nested_network_company_password_window_widgets, lengthof(_nested_network_company_password_window_widgets)
);

void ShowNetworkCompanyPasswordWindow(Window *parent)
{
	DeleteWindowById(WC_COMPANY_PASSWORD_WINDOW, 0);

	new NetworkCompanyPasswordWindow(&_network_company_password_window_desc, parent);
}

#endif /* ENABLE_NETWORK */
