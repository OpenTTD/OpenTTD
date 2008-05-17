/* $Id$ */

/** @file network_gui.cpp Implementation of the Network related GUIs. */

#ifdef ENABLE_NETWORK
#include "../stdafx.h"
#include "../openttd.h"
#include "../strings_func.h"
#include "network.h"
#include "../date_func.h"
#include "../fios.h"
#include "network_data.h"
#include "network_client.h"
#include "network_gui.h"
#include "network_gamelist.h"
#include "../gui.h"
#include "../window_gui.h"
#include "../textbuf_gui.h"
#include "../variables.h"
#include "network_server.h"
#include "network_udp.h"
#include "../town.h"
#include "../newgrf.h"
#include "../functions.h"
#include "../window_func.h"
#include "../core/alloc_func.hpp"
#include "../string_func.h"
#include "../gfx_func.h"
#include "../player_func.h"
#include "../settings_type.h"
#include "../widgets/dropdown_func.h"
#include "../querystring_gui.h"

#include "table/strings.h"
#include "../table/sprites.h"

#define BGC 5
#define BTC 15

/* Global to remember sorting after window has been closed */
static Listing _ng_sorting;

static bool _chat_tab_completion_active;

static void ShowNetworkStartServerWindow();
static void ShowNetworkLobbyWindow(NetworkGameList *ngl);
extern void SwitchMode(int new_mode);

static const StringID _connection_types_dropdown[] = {
	STR_NETWORK_LAN_INTERNET,
	STR_NETWORK_INTERNET_ADVERTISE,
	INVALID_STRING_ID
};

static const StringID _lan_internet_types_dropdown[] = {
	STR_NETWORK_LAN,
	STR_NETWORK_INTERNET,
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
	qsort(&_language_dropdown[1], NETLANG_COUNT - 1, sizeof(StringID), &StringIDSorter);
}

enum {
	NET_PRC__OFFSET_TOP_WIDGET          = 54,
	NET_PRC__OFFSET_TOP_WIDGET_COMPANY  = 52,
	NET_PRC__SIZE_OF_ROW                = 14,
};

/** Update the network new window because a new server is
 * found on the network.
 * @param unselect unselect the currently selected item */
void UpdateNetworkGameWindow(bool unselect)
{
	InvalidateWindowData(WC_NETWORK_WINDOW, 0, unselect);
}

static bool _internal_sort_order; // Used for Qsort order-flipping
typedef int CDECL NGameNameSortFunction(const void*, const void*);

/** Qsort function to sort by name. */
static int CDECL NGameNameSorter(const void *a, const void *b)
{
	const NetworkGameList *cmp1 = *(const NetworkGameList**)a;
	const NetworkGameList *cmp2 = *(const NetworkGameList**)b;
	int r = strcasecmp(cmp1->info.server_name, cmp2->info.server_name);

	return _internal_sort_order ? -r : r;
}

/** Qsort function to sort by the amount of clients online on a
 * server. If the two servers have the same amount, the one with the
 * higher maximum is preferred. */
static int CDECL NGameClientSorter(const void *a, const void *b)
{
	const NetworkGameList *cmp1 = *(const NetworkGameList**)a;
	const NetworkGameList *cmp2 = *(const NetworkGameList**)b;
	/* Reverse as per default we are interested in most-clients first */
	int r = cmp1->info.clients_on - cmp2->info.clients_on;

	if (r == 0) r = cmp1->info.clients_max - cmp2->info.clients_max;
	if (r == 0) r = strcasecmp(cmp1->info.server_name, cmp2->info.server_name);

	return _internal_sort_order ? -r : r;
}

/** Qsort function to sort by joinability. If both servers are the
 * same, prefer the non-passworded server first. */
static int CDECL NGameAllowedSorter(const void *a, const void *b)
{
	const NetworkGameList *cmp1 = *(const NetworkGameList**)a;
	const NetworkGameList *cmp2 = *(const NetworkGameList**)b;

	/* The servers we do not know anything about (the ones that did not reply) should be at the bottom) */
	int r = StrEmpty(cmp1->info.server_revision) - StrEmpty(cmp2->info.server_revision);

	/* Reverse default as we are interested in version-compatible clients first */
	if (r == 0) r = cmp2->info.version_compatible - cmp1->info.version_compatible;
	/* The version-compatible ones are then sorted with NewGRF compatible first, incompatible last */
	if (r == 0) r = cmp2->info.compatible - cmp1->info.compatible;
	/* Passworded servers should be below unpassworded servers */
	if (r == 0) r = cmp1->info.use_password - cmp2->info.use_password;
	/* Finally sort on the name of the server */
	if (r == 0) r = strcasecmp(cmp1->info.server_name, cmp2->info.server_name);

	return _internal_sort_order ? -r : r;
}

/** Enum for NetworkGameWindow, referring to _network_game_window_widgets */
enum NetworkGameWindowWidgets {
	NGWW_CLOSE,         ///< Close 'X' button
	NGWW_CAPTION,       ///< Caption of the window
	NGWW_RESIZE,        ///< Resize button

	NGWW_CONNECTION,    ///< Label in from of connection droplist
	NGWW_CONN_BTN,      ///< 'Connection' droplist button
	NGWW_PLAYER,        ///< Panel with editbox to set player name

	NGWW_NAME,          ///< 'Name' button
	NGWW_CLIENTS,       ///< 'Clients' button
	NGWW_INFO,          ///< Third button in the game list panel

	NGWW_MATRIX,        ///< Panel with list of games
	NGWW_SCROLLBAR,     ///< Scrollbar of matrix

	NGWW_LASTJOINED_LABEL, ///< Label "Last joined server:"
	NGWW_LASTJOINED,    ///< Info about the last joined server

	NGWW_DETAILS,       ///< Panel with game details
	NGWW_JOIN,          ///< 'Join game' button
	NGWW_REFRESH,       ///< 'Refresh server' button
	NGWW_NEWGRF,        ///< 'NewGRF Settings' button

	NGWW_FIND,          ///< 'Find server' button
	NGWW_ADD,           ///< 'Add server' button
	NGWW_START,         ///< 'Start server' button
	NGWW_CANCEL,        ///< 'Cancel' button
};

typedef GUIList<NetworkGameList*> GUIGameServerList;

struct NetworkGameWindow : public QueryStringBaseWindow {
	byte field;                  ///< selected text-field
	NetworkGameList *server;     ///< selected server
	GUIGameServerList servers;   ///< list with game servers.

	NetworkGameWindow(const WindowDesc *desc) : QueryStringBaseWindow(desc)
	{
		ttd_strlcpy(this->edit_str_buf, _network_player_name, lengthof(this->edit_str_buf));
		this->afilter = CS_ALPHANUMERAL;
		InitializeTextBuffer(&this->text, this->edit_str_buf, lengthof(this->edit_str_buf), 120);

		UpdateNetworkGameWindow(true);

		this->vscroll.cap = 11;
		this->resize.step_height = NET_PRC__SIZE_OF_ROW;

		this->field = NGWW_PLAYER;
		this->server = NULL;

		this->servers.sort_list = NULL;
		this->servers.flags = VL_REBUILD | (_ng_sorting.order ? VL_DESC : VL_NONE);
		this->servers.sort_type = _ng_sorting.criteria;

		this->FindWindowPlacementAndResize(desc);
	}

	~NetworkGameWindow()
	{
		free(this->servers.sort_list);
	}

	/**
	 * (Re)build the network game list as its amount has changed because
	 * an item has been added or deleted for example
	 */
	void BuildNetworkGameList()
	{
		NetworkGameList *ngl_temp;
		uint n = 0;

		if (!(this->servers.flags & VL_REBUILD)) return;

		/* Count the number of games in the list */
		for (ngl_temp = _network_game_list; ngl_temp != NULL; ngl_temp = ngl_temp->next) n++;
		if (n == 0) return;

		/* Create temporary array of games to use for listing */
		this->servers.sort_list = ReallocT(this->servers.sort_list, n);
		this->servers.list_length = n;

		for (n = 0, ngl_temp = _network_game_list; ngl_temp != NULL; ngl_temp = ngl_temp->next) {
			this->servers.sort_list[n++] = ngl_temp;
		}

		/* Force resort */
		this->servers.flags &= ~VL_REBUILD;
		this->servers.flags |= VL_RESORT;
	}

	void SortNetworkGameList()
	{
		static NGameNameSortFunction * const ngame_sorter[] = {
			&NGameNameSorter,
			&NGameClientSorter,
			&NGameAllowedSorter
		};

		NetworkGameList *item;
		uint i;

		if (!(this->servers.flags & VL_RESORT)) return;
		if (this->servers.list_length == 0) return;

		_internal_sort_order = !!(this->servers.flags & VL_DESC);
		qsort(this->servers.sort_list, this->servers.list_length, sizeof(this->servers.sort_list[0]), ngame_sorter[this->servers.sort_type]);

		/* After sorting ngl->sort_list contains the sorted items. Put these back
		 * into the original list. Basically nothing has changed, we are only
		 * shuffling the ->next pointers */
		_network_game_list = this->servers.sort_list[0];
		for (item = _network_game_list, i = 1; i != this->servers.list_length; i++) {
			item->next = this->servers.sort_list[i];
			item = item->next;
		}
		item->next = NULL;

		this->servers.flags &= ~VL_RESORT;
	}

	/**
	 * Draw a single server line.
	 * @param cur_item  the server to draw.
	 * @param y         from where to draw?
	 * @param highlight does the line need to be highlighted?
	 */
	void DrawServerLine(const NetworkGameList *cur_item, uint y, bool highlight)
	{
		/* show highlighted item with a different colour */
		if (highlight) GfxFillRect(this->widget[NGWW_NAME].left + 1, y - 2, this->widget[NGWW_INFO].right - 1, y + 9, 10);

		SetDParamStr(0, cur_item->info.server_name);
		DrawStringTruncated(this->widget[NGWW_NAME].left + 5, y, STR_02BD, TC_BLACK, this->widget[NGWW_NAME].right - this->widget[NGWW_NAME].left - 5);

		SetDParam(0, cur_item->info.clients_on);
		SetDParam(1, cur_item->info.clients_max);
		SetDParam(2, cur_item->info.companies_on);
		SetDParam(3, cur_item->info.companies_max);
		DrawStringCentered(this->widget[NGWW_CLIENTS].left + 39, y, STR_NETWORK_GENERAL_ONLINE, TC_GOLD);

		/* only draw icons if the server is online */
		if (cur_item->online) {
			/* draw a lock if the server is password protected */
			if (cur_item->info.use_password) DrawSprite(SPR_LOCK, PAL_NONE, this->widget[NGWW_INFO].left + 5, y - 1);

			/* draw red or green icon, depending on compatibility with server */
			DrawSprite(SPR_BLOT, (cur_item->info.compatible ? PALETTE_TO_GREEN : (cur_item->info.version_compatible ? PALETTE_TO_YELLOW : PALETTE_TO_RED)), this->widget[NGWW_INFO].left + 15, y);

			/* draw flag according to server language */
			DrawSprite(SPR_FLAGS_BASE + cur_item->info.server_lang, PAL_NONE, this->widget[NGWW_INFO].left + 25, y);
		}
	}

	virtual void OnPaint()
	{
		const NetworkGameList *sel = this->server;
		const SortButtonState arrow = (this->servers.flags & VL_DESC) ? SBS_DOWN : SBS_UP;

		if (this->servers.flags & VL_REBUILD) {
			this->BuildNetworkGameList();
			SetVScrollCount(this, this->servers.list_length);
		}
		if (this->servers.flags & VL_RESORT) this->SortNetworkGameList();

		/* 'Refresh' button invisible if no server selected */
		this->SetWidgetDisabledState(NGWW_REFRESH, sel == NULL);
		/* 'Join' button disabling conditions */
		this->SetWidgetDisabledState(NGWW_JOIN, sel == NULL || // no Selected Server
				!sel->online || // Server offline
				sel->info.clients_on >= sel->info.clients_max || // Server full
				!sel->info.compatible); // Revision mismatch

		/* 'NewGRF Settings' button invisible if no NewGRF is used */
		this->SetWidgetHiddenState(NGWW_NEWGRF, sel == NULL ||
				!sel->online ||
				sel->info.grfconfig == NULL);

		SetDParam(0, 0x00);
		SetDParam(1, _lan_internet_types_dropdown[_network_lan_internet]);
		DrawWindowWidgets(this);

		/* Edit box to set player name */
		this->DrawEditBox(NGWW_PLAYER);

		DrawString(this->widget[NGWW_PLAYER].left - 100, 23, STR_NETWORK_PLAYER_NAME, TC_GOLD);

		/* Sort based on widgets: name, clients, compatibility */
		switch (this->servers.sort_type) {
			case NGWW_NAME    - NGWW_NAME: DrawSortButtonState(this, NGWW_NAME,    arrow); break;
			case NGWW_CLIENTS - NGWW_NAME: DrawSortButtonState(this, NGWW_CLIENTS, arrow); break;
			case NGWW_INFO    - NGWW_NAME: DrawSortButtonState(this, NGWW_INFO,    arrow); break;
		}

		uint16 y = NET_PRC__OFFSET_TOP_WIDGET + 3;
		int32 n = 0;
		int32 pos = this->vscroll.pos;
		const NetworkGameList *cur_item = _network_game_list;

		while (pos > 0 && cur_item != NULL) {
			pos--;
			cur_item = cur_item->next;
		}

		while (cur_item != NULL) {
			this->DrawServerLine(cur_item, y, cur_item == sel);

			cur_item = cur_item->next;
			y += NET_PRC__SIZE_OF_ROW;
			if (++n == this->vscroll.cap) break; // max number of games in the window
		}

		const NetworkGameList *last_joined = NetworkGameListAddItem(inet_addr(_network_last_host), _network_last_port);
		/* Draw the last joined server, if any */
		if (last_joined != NULL) this->DrawServerLine(last_joined, y = this->widget[NGWW_LASTJOINED].top + 3, last_joined == sel);

		/* Draw the right menu */
		GfxFillRect(this->widget[NGWW_DETAILS].left + 1, 43, this->widget[NGWW_DETAILS].right - 1, 92, 157);
		if (sel == NULL) {
			DrawStringCentered(this->widget[NGWW_DETAILS].left + 115, 58, STR_NETWORK_GAME_INFO, TC_FROMSTRING);
		} else if (!sel->online) {
			SetDParamStr(0, sel->info.server_name);
			DrawStringCentered(this->widget[NGWW_DETAILS].left + 115, 68, STR_ORANGE, TC_FROMSTRING); // game name

			DrawStringCentered(this->widget[NGWW_DETAILS].left + 115, 132, STR_NETWORK_SERVER_OFFLINE, TC_FROMSTRING); // server offline
		} else { // show game info
			uint16 y = 100;
			const uint16 x = this->widget[NGWW_DETAILS].left + 5;

			DrawStringCentered(this->widget[NGWW_DETAILS].left + 115, 48, STR_NETWORK_GAME_INFO, TC_FROMSTRING);


			SetDParamStr(0, sel->info.server_name);
			DrawStringCenteredTruncated(this->widget[NGWW_DETAILS].left, this->widget[NGWW_DETAILS].right, 62, STR_ORANGE, TC_BLACK); // game name

			SetDParamStr(0, sel->info.map_name);
			DrawStringCenteredTruncated(this->widget[NGWW_DETAILS].left, this->widget[NGWW_DETAILS].right, 74, STR_02BD, TC_BLACK); // map name

			SetDParam(0, sel->info.clients_on);
			SetDParam(1, sel->info.clients_max);
			SetDParam(2, sel->info.companies_on);
			SetDParam(3, sel->info.companies_max);
			DrawString(x, y, STR_NETWORK_CLIENTS, TC_GOLD);
			y += 10;

			SetDParam(0, STR_NETWORK_LANG_ANY + sel->info.server_lang);
			DrawString(x, y, STR_NETWORK_LANGUAGE, TC_GOLD); // server language
			y += 10;

			SetDParam(0, STR_TEMPERATE_LANDSCAPE + sel->info.map_set);
			DrawString(x, y, STR_NETWORK_TILESET, TC_GOLD); // tileset
			y += 10;

			SetDParam(0, sel->info.map_width);
			SetDParam(1, sel->info.map_height);
			DrawString(x, y, STR_NETWORK_MAP_SIZE, TC_GOLD); // map size
			y += 10;

			SetDParamStr(0, sel->info.server_revision);
			DrawString(x, y, STR_NETWORK_SERVER_VERSION, TC_GOLD); // server version
			y += 10;

			SetDParamStr(0, sel->info.hostname);
			SetDParam(1, sel->port);
			DrawString(x, y, STR_NETWORK_SERVER_ADDRESS, TC_GOLD); // server address
			y += 10;

			SetDParam(0, sel->info.start_date);
			DrawString(x, y, STR_NETWORK_START_DATE, TC_GOLD); // start date
			y += 10;

			SetDParam(0, sel->info.game_date);
			DrawString(x, y, STR_NETWORK_CURRENT_DATE, TC_GOLD); // current date
			y += 10;

			y += 2;

			if (!sel->info.compatible) {
				DrawStringCentered(this->widget[NGWW_DETAILS].left + 115, y, sel->info.version_compatible ? STR_NETWORK_GRF_MISMATCH : STR_NETWORK_VERSION_MISMATCH, TC_FROMSTRING); // server mismatch
			} else if (sel->info.clients_on == sel->info.clients_max) {
				/* Show: server full, when clients_on == clients_max */
				DrawStringCentered(this->widget[NGWW_DETAILS].left + 115, y, STR_NETWORK_SERVER_FULL, TC_FROMSTRING); // server full
			} else if (sel->info.use_password) {
				DrawStringCentered(this->widget[NGWW_DETAILS].left + 115, y, STR_NETWORK_PASSWORD, TC_FROMSTRING); // password warning
			}

			y += 10;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		this->field = widget;
		switch (widget) {
			case NGWW_PLAYER:
				ShowOnScreenKeyboard(this, NGWW_PLAYER, 0, 0);
				break;

			case NGWW_CANCEL: // Cancel button
				DeleteWindowById(WC_NETWORK_WINDOW, 0);
				break;

			case NGWW_CONN_BTN: // 'Connection' droplist
				ShowDropDownMenu(this, _lan_internet_types_dropdown, _network_lan_internet, NGWW_CONN_BTN, 0, 0); // do it for widget NSSW_CONN_BTN
				break;

			case NGWW_NAME: // Sort by name
			case NGWW_CLIENTS: // Sort by connected clients
			case NGWW_INFO: // Connectivity (green dot)
				if (this->servers.sort_type == widget - NGWW_NAME) this->servers.flags ^= VL_DESC;
				this->servers.flags |= VL_RESORT;
				this->servers.sort_type = widget - NGWW_NAME;

				_ng_sorting.order = !!(this->servers.flags & VL_DESC);
				_ng_sorting.criteria = this->servers.sort_type;
				this->SetDirty();
				break;

			case NGWW_MATRIX: { // Matrix to show networkgames
				NetworkGameList *cur_item;
				uint32 id_v = (pt.y - NET_PRC__OFFSET_TOP_WIDGET) / NET_PRC__SIZE_OF_ROW;

				if (id_v >= this->vscroll.cap) return; // click out of bounds
				id_v += this->vscroll.pos;

				cur_item = _network_game_list;
				for (; id_v > 0 && cur_item != NULL; id_v--) cur_item = cur_item->next;

				this->server = cur_item;
				this->SetDirty();
			} break;

			case NGWW_LASTJOINED: {
				NetworkGameList *last_joined = NetworkGameListAddItem(inet_addr(_network_last_host), _network_last_port);
				if (last_joined != NULL) {
					this->server = last_joined;
					this->SetDirty();
				}
			} break;

			case NGWW_FIND: // Find server automatically
				switch (_network_lan_internet) {
					case 0: NetworkUDPSearchGame(); break;
					case 1: NetworkUDPQueryMasterServer(); break;
				}
				break;

			case NGWW_ADD: // Add a server
				ShowQueryString(
					BindCString(_network_default_ip),
					STR_NETWORK_ENTER_IP,
					31 | 0x1000,  // maximum number of characters OR
					250, // characters up to this width pixels, whichever is satisfied first
					this, CS_ALPHANUMERAL);
				break;

			case NGWW_START: // Start server
				ShowNetworkStartServerWindow();
				break;

			case NGWW_JOIN: // Join Game
				if (this->server != NULL) {
					snprintf(_network_last_host, sizeof(_network_last_host), "%s", inet_ntoa(*(struct in_addr *)&this->server->ip));
					_network_last_port = this->server->port;
					ShowNetworkLobbyWindow(this->server);
				}
				break;

			case NGWW_REFRESH: // Refresh
				if (this->server != NULL) NetworkUDPQueryServer(this->server->info.hostname, this->server->port);
				break;

			case NGWW_NEWGRF: // NewGRF Settings
				if (this->server != NULL) ShowNewGRFSettings(false, false, false, &this->server->info.grfconfig);
				break;
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case NGWW_CONN_BTN:
				_network_lan_internet = index;
				break;

			default:
				NOT_REACHED();
		}

		this->SetDirty();
	}

	virtual void OnMouseLoop()
	{
		if (this->field == NGWW_PLAYER) this->HandleEditBox(NGWW_PLAYER);
	}

	virtual void OnInvalidateData(int data)
	{
		if (data != 0) this->server = NULL;
		this->servers.flags |= VL_REBUILD;
		this->SetDirty();
	}

	virtual bool OnKeyPress(uint16 key, uint16 keycode)
	{
		bool cont = true;
		if (this->field != NGWW_PLAYER) {
			if (this->server != NULL) {
				if (keycode == WKC_DELETE) { // Press 'delete' to remove servers
					NetworkGameListRemoveItem(this->server);
					NetworkRebuildHostList();
					this->server = NULL;
				}
			}
			return cont;
		}

		if (this->HandleEditBoxKey(NGWW_PLAYER, keycode, key, cont) == 1) return cont; // enter pressed

		/* The name is only allowed when it starts with a letter! */
		if (StrEmpty(this->edit_str_buf) && this->edit_str_buf[0] != ' ') {
			ttd_strlcpy(_network_player_name, this->edit_str_buf, lengthof(_network_player_name));
		} else {
			ttd_strlcpy(_network_player_name, "Player", lengthof(_network_player_name));
		}
		return cont;
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (!StrEmpty(str)) {
			NetworkAddServer(str);
			NetworkRebuildHostList();
		}
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		this->vscroll.cap += delta.y / (int)this->resize.step_height;

		this->widget[NGWW_MATRIX].data = (this->vscroll.cap << 8) + 1;

		SetVScrollCount(this, this->servers.list_length);

		int widget_width = this->widget[NGWW_FIND].right - this->widget[NGWW_FIND].left;
		int space = (this->width - 4 * widget_width - 25) / 3;

		int offset = 10;
		for (uint i = 0; i < 4; i++) {
			this->widget[NGWW_FIND + i].left  = offset;
			offset += widget_width;
			this->widget[NGWW_FIND + i].right = offset;
			offset += space;
		}
	}
};

static const Widget _network_game_window_widgets[] = {
/* TOP */
{   WWT_CLOSEBOX,   RESIZE_NONE,   BGC,     0,    10,     0,    13, STR_00C5,                         STR_018B_CLOSE_WINDOW},            // NGWW_CLOSE
{    WWT_CAPTION,   RESIZE_RIGHT,  BGC,    11,   449,     0,    13, STR_NETWORK_MULTIPLAYER,          STR_NULL},                         // NGWW_CAPTION
{      WWT_PANEL,   RESIZE_RB,     BGC,     0,   449,    14,   263, 0x0,                              STR_NULL},                         // NGWW_RESIZE

{       WWT_TEXT,   RESIZE_NONE,   BGC,     9,    85,    23,    35, STR_NETWORK_CONNECTION,           STR_NULL},                         // NGWW_CONNECTION
{ WWT_DROPDOWNIN,   RESIZE_NONE,   BGC,    90,   181,    22,    33, STR_NETWORK_LAN_INTERNET_COMBO,   STR_NETWORK_CONNECTION_TIP},       // NGWW_CONN_BTN

{    WWT_EDITBOX,   RESIZE_LR,     BGC,   290,   440,    22,    33, STR_NETWORK_PLAYER_NAME_OSKTITLE, STR_NETWORK_ENTER_NAME_TIP},       // NGWW_PLAYER

/* LEFT SIDE */
{ WWT_PUSHTXTBTN,   RESIZE_RIGHT,  BTC,    10,    70,    42,    53, STR_NETWORK_GAME_NAME,            STR_NETWORK_GAME_NAME_TIP},        // NGWW_NAME
{ WWT_PUSHTXTBTN,   RESIZE_LR,     BTC,    71,   150,    42,    53, STR_NETWORK_CLIENTS_CAPTION,      STR_NETWORK_CLIENTS_CAPTION_TIP},  // NGWW_CLIENTS
{ WWT_PUSHTXTBTN,   RESIZE_LR,     BTC,   151,   190,    42,    53, STR_EMPTY,                        STR_NETWORK_INFO_ICONS_TIP},       // NGWW_INFO

{     WWT_MATRIX,   RESIZE_RB,     BGC,    10,   190,    54,   208, (11 << 8) + 1,                    STR_NETWORK_CLICK_GAME_TO_SELECT}, // NGWW_MATRIX
{  WWT_SCROLLBAR,   RESIZE_LRB,    BGC,   191,   202,    42,   208, 0x0,                              STR_0190_SCROLL_BAR_SCROLLS_LIST}, // NGWW_SCROLLBAR
{       WWT_TEXT,   RESIZE_RTB,    BGC,    10,   190,   211,   222, STR_NETWORK_LAST_JOINED_SERVER,   STR_NULL},                         // NGWW_LASTJOINED_LABEL
{      WWT_PANEL,   RESIZE_RTB,    BGC,    10,   190,   223,   236, 0x0,                              STR_NETWORK_CLICK_TO_SELECT_LAST}, // NGWW_LASTJOINED

/* RIGHT SIDE */
{      WWT_PANEL,   RESIZE_LRB,    BGC,   210,   440,    42,   236, 0x0,                              STR_NULL},                         // NGWW_DETAILS

{ WWT_PUSHTXTBTN,   RESIZE_LRTB,   BTC,   215,   315,   215,   226, STR_NETWORK_JOIN_GAME,            STR_NULL},                         // NGWW_JOIN
{ WWT_PUSHTXTBTN,   RESIZE_LRTB,   BTC,   330,   435,   215,   226, STR_NETWORK_REFRESH,              STR_NETWORK_REFRESH_TIP},          // NGWW_REFRESH

{ WWT_PUSHTXTBTN,   RESIZE_LRTB,   BTC,   330,   435,   197,   208, STR_NEWGRF_SETTINGS_BUTTON,       STR_NULL},                         // NGWW_NEWGRF

/* BOTTOM */
{ WWT_PUSHTXTBTN,   RESIZE_TB,     BTC,    10,   110,   246,   257, STR_NETWORK_FIND_SERVER,          STR_NETWORK_FIND_SERVER_TIP},      // NGWW_FIND
{ WWT_PUSHTXTBTN,   RESIZE_TB,     BTC,   118,   218,   246,   257, STR_NETWORK_ADD_SERVER,           STR_NETWORK_ADD_SERVER_TIP},       // NGWW_ADD
{ WWT_PUSHTXTBTN,   RESIZE_TB,     BTC,   226,   326,   246,   257, STR_NETWORK_START_SERVER,         STR_NETWORK_START_SERVER_TIP},     // NGWW_START
{ WWT_PUSHTXTBTN,   RESIZE_TB,     BTC,   334,   434,   246,   257, STR_012E_CANCEL,                  STR_NULL},                         // NGWW_CANCEL

{  WWT_RESIZEBOX,   RESIZE_LRTB,   BGC,   438,   449,   252,   263, 0x0,                              STR_RESIZE_BUTTON },

{   WIDGETS_END},
};

static const WindowDesc _network_game_window_desc = {
	WDP_CENTER, WDP_CENTER, 450, 264, 550, 264,
	WC_NETWORK_WINDOW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_network_game_window_widgets,
	NULL,
};

void ShowNetworkGameWindow()
{
	static bool first = true;
	DeleteWindowById(WC_NETWORK_WINDOW, 0);

	/* Only show once */
	if (first) {
		char * const *srv;

		first = false;
		// add all servers from the config file to our list
		for (srv = &_network_host_list[0]; srv != endof(_network_host_list) && *srv != NULL; srv++) {
			NetworkAddServer(*srv);
		}

		_ng_sorting.criteria = 2; // sort default by collectivity (green-dots on top)
		_ng_sorting.order = 0;    // sort ascending by default
	}

	new NetworkGameWindow(&_network_game_window_desc);
}

enum {
	NSSWND_START = 64,
	NSSWND_ROWSIZE = 12
};

/** Enum for NetworkStartServerWindow, referring to _network_start_server_window_widgets */
enum NetworkStartServerWidgets {
	NSSW_CLOSE           =  0,   ///< Close 'X' button
	NSSW_GAMENAME        =  4,   ///< Background for editbox to set game name
	NSSW_SETPWD          =  5,   ///< 'Set password' button
	NSSW_SELMAP          =  7,   ///< 'Select map' list
	NSSW_CONNTYPE_BTN    = 10,   ///< 'Connection type' droplist button
	NSSW_CLIENTS_BTND    = 12,   ///< 'Max clients' downarrow
	NSSW_CLIENTS_TXT     = 13,   ///< 'Max clients' text
	NSSW_CLIENTS_BTNU    = 14,   ///< 'Max clients' uparrow
	NSSW_COMPANIES_BTND  = 16,   ///< 'Max companies' downarrow
	NSSW_COMPANIES_TXT   = 17,   ///< 'Max companies' text
	NSSW_COMPANIES_BTNU  = 18,   ///< 'Max companies' uparrow
	NSSW_SPECTATORS_BTND = 20,   ///< 'Max spectators' downarrow
	NSSW_SPECTATORS_TXT  = 21,   ///< 'Max spectators' text
	NSSW_SPECTATORS_BTNU = 22,   ///< 'Max spectators' uparrow
	NSSW_LANGUAGE_BTN    = 24,   ///< 'Language spoken' droplist button
	NSSW_START           = 25,   ///< 'Start' button
	NSSW_LOAD            = 26,   ///< 'Load' button
	NSSW_CANCEL          = 27,   ///< 'Cancel' button
};

struct NetworkStartServerWindow : public QueryStringBaseWindow {
	byte field;                  ///< Selected text-field
	FiosItem *map;               ///< Selected map
	byte widget_id;              ///< The widget that has the pop-up input menu

	NetworkStartServerWindow(const WindowDesc *desc) : QueryStringBaseWindow(desc)
	{
		ttd_strlcpy(this->edit_str_buf, _network_server_name, lengthof(this->edit_str_buf));

		_saveload_mode = SLD_NEW_GAME;
		BuildFileList();
		this->vscroll.cap = 12;
		this->vscroll.count = _fios_num + 1;

		this->afilter = CS_ALPHANUMERAL;
		InitializeTextBuffer(&this->text, this->edit_str_buf, lengthof(this->edit_str_buf), 160);

		this->field = NSSW_GAMENAME;
		_network_game_info.use_password = !StrEmpty(_network_server_password);

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		int y = NSSWND_START, pos;
		const FiosItem *item;

		/* draw basic widgets */
		SetDParam(1, _connection_types_dropdown[_network_advertise]);
		SetDParam(2, _network_game_info.clients_max);
		SetDParam(3, _network_game_info.companies_max);
		SetDParam(4, _network_game_info.spectators_max);
		SetDParam(5, STR_NETWORK_LANG_ANY + _network_game_info.server_lang);
		DrawWindowWidgets(this);

		/* editbox to set game name */
		this->DrawEditBox(NSSW_GAMENAME);

		/* if password is set, draw red '*' next to 'Set password' button */
		if (_network_game_info.use_password) DoDrawString("*", 408, 23, TC_RED);

		/* draw list of maps */
		GfxFillRect(11, 63, 258, 215, 0xD7);  // black background of maps list

		pos = this->vscroll.pos;
		while (pos < _fios_num + 1) {
			item = _fios_list + pos - 1;
			if (item == this->map || (pos == 0 && this->map == NULL))
				GfxFillRect(11, y - 1, 258, y + 10, 155); // show highlighted item with a different colour

			if (pos == 0) {
				DrawString(14, y, STR_4010_GENERATE_RANDOM_NEW_GAME, TC_DARK_GREEN);
			} else {
				DoDrawString(item->title, 14, y, _fios_colors[item->type] );
			}
			pos++;
			y += NSSWND_ROWSIZE;

			if (y >= this->vscroll.cap * NSSWND_ROWSIZE + NSSWND_START) break;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget != NSSW_CONNTYPE_BTN && widget != NSSW_LANGUAGE_BTN) HideDropDownMenu(this);
		this->field = widget;
		switch (widget) {
			case NSSW_CLOSE:  // Close 'X'
			case NSSW_CANCEL: // Cancel button
				ShowNetworkGameWindow();
				break;

			case NSSW_GAMENAME:
				ShowOnScreenKeyboard(this, NSSW_GAMENAME, 0, 0);
				break;

			case NSSW_SETPWD: // Set password button
				this->widget_id = NSSW_SETPWD;
				ShowQueryString(BindCString(_network_server_password), STR_NETWORK_SET_PASSWORD, 20, 250, this, CS_ALPHANUMERAL);
				break;

			case NSSW_SELMAP: { // Select map
				int y = (pt.y - NSSWND_START) / NSSWND_ROWSIZE;

				y += this->vscroll.pos;
				if (y >= this->vscroll.count) return;

				this->map = (y == 0) ? NULL : _fios_list + y - 1;
				this->SetDirty();
			} break;

			case NSSW_CONNTYPE_BTN: // Connection type
				ShowDropDownMenu(this, _connection_types_dropdown, _network_advertise, NSSW_CONNTYPE_BTN, 0, 0); // do it for widget NSSW_CONNTYPE_BTN
				break;

			case NSSW_CLIENTS_BTND:    case NSSW_CLIENTS_BTNU:    // Click on up/down button for number of clients
			case NSSW_COMPANIES_BTND:  case NSSW_COMPANIES_BTNU:  // Click on up/down button for number of companies
			case NSSW_SPECTATORS_BTND: case NSSW_SPECTATORS_BTNU: // Click on up/down button for number of spectators
				/* Don't allow too fast scrolling */
				if ((this->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
					this->HandleButtonClick(widget);
					this->SetDirty();
					switch (widget) {
						default: NOT_REACHED();
						case NSSW_CLIENTS_BTND: case NSSW_CLIENTS_BTNU:
							_network_game_info.clients_max    = Clamp(_network_game_info.clients_max    + widget - NSSW_CLIENTS_TXT,    2, MAX_CLIENTS);
							break;
						case NSSW_COMPANIES_BTND: case NSSW_COMPANIES_BTNU:
							_network_game_info.companies_max  = Clamp(_network_game_info.companies_max  + widget - NSSW_COMPANIES_TXT,  1, MAX_PLAYERS);
							break;
						case NSSW_SPECTATORS_BTND: case NSSW_SPECTATORS_BTNU:
							_network_game_info.spectators_max = Clamp(_network_game_info.spectators_max + widget - NSSW_SPECTATORS_TXT, 0, MAX_CLIENTS);
							break;
					}
				}
				_left_button_clicked = false;
				break;

			case NSSW_CLIENTS_TXT:    // Click on number of players
				this->widget_id = NSSW_CLIENTS_TXT;
				SetDParam(0, _network_game_info.clients_max);
				ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_NETWORK_NUMBER_OF_CLIENTS,    3, 50, this, CS_NUMERAL);
				break;

			case NSSW_COMPANIES_TXT:  // Click on number of companies
				this->widget_id = NSSW_COMPANIES_TXT;
				SetDParam(0, _network_game_info.companies_max);
				ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_NETWORK_NUMBER_OF_COMPANIES,  3, 50, this, CS_NUMERAL);
				break;

			case NSSW_SPECTATORS_TXT: // Click on number of spectators
				this->widget_id = NSSW_SPECTATORS_TXT;
				SetDParam(0, _network_game_info.spectators_max);
				ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_NETWORK_NUMBER_OF_SPECTATORS, 3, 50, this, CS_NUMERAL);
				break;

			case NSSW_LANGUAGE_BTN: { // Language
				uint sel = 0;
				for (uint i = 0; i < lengthof(_language_dropdown) - 1; i++) {
					if (_language_dropdown[i] == STR_NETWORK_LANG_ANY + _network_game_info.server_lang) {
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
					char *name = FiosBrowseTo(this->map);
					if (name != NULL) {
						SetFiosType(this->map->type);
						_file_to_saveload.filetype = FT_SCENARIO;
						ttd_strlcpy(_file_to_saveload.name, name, sizeof(_file_to_saveload.name));
						ttd_strlcpy(_file_to_saveload.title, this->map->title, sizeof(_file_to_saveload.title));

						delete this;
						SwitchMode(SM_START_SCENARIO);
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
				_network_advertise = (index != 0);
				break;
			case NSSW_LANGUAGE_BTN:
				_network_game_info.server_lang = _language_dropdown[index] - STR_NETWORK_LANG_ANY;
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

	virtual bool OnKeyPress(uint16 key, uint16 keycode)
	{
		bool cont = true;
		if (this->field == NSSW_GAMENAME) {
			if (this->HandleEditBoxKey(NSSW_GAMENAME, key, keycode, cont) == 1) return cont; // enter pressed

			ttd_strlcpy(_network_server_name, this->text.buf, sizeof(_network_server_name));
		}

		return cont;
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		if (this->widget_id == NSSW_SETPWD) {
			ttd_strlcpy(_network_server_password, str, lengthof(_network_server_password));
			_network_game_info.use_password = !StrEmpty(_network_server_password);
		} else {
			int32 value = atoi(str);
			this->InvalidateWidget(this->widget_id);
			switch (this->widget_id) {
				default: NOT_REACHED();
				case NSSW_CLIENTS_TXT:    _network_game_info.clients_max    = Clamp(value, 2, MAX_CLIENTS); break;
				case NSSW_COMPANIES_TXT:  _network_game_info.companies_max  = Clamp(value, 1, MAX_PLAYERS); break;
				case NSSW_SPECTATORS_TXT: _network_game_info.spectators_max = Clamp(value, 0, MAX_CLIENTS); break;
			}
		}

		this->SetDirty();
	}
};

static const Widget _network_start_server_window_widgets[] = {
/* Window decoration and background panel */
{   WWT_CLOSEBOX,   RESIZE_NONE,   BGC,     0,    10,     0,    13, STR_00C5,                           STR_018B_CLOSE_WINDOW },               // NSSW_CLOSE
{    WWT_CAPTION,   RESIZE_NONE,   BGC,    11,   419,     0,    13, STR_NETWORK_START_GAME_WINDOW,      STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,   BGC,     0,   419,    14,   243, 0x0,                                STR_NULL},

/* Set game name and password widgets */
{       WWT_TEXT,   RESIZE_NONE,   BGC,    10,    90,    22,    34, STR_NETWORK_NEW_GAME_NAME,          STR_NULL},
{    WWT_EDITBOX,   RESIZE_NONE,   BGC,   100,   272,    22,    33, STR_NETWORK_NEW_GAME_NAME_OSKTITLE, STR_NETWORK_NEW_GAME_NAME_TIP},        // NSSW_GAMENAME
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   285,   405,    22,    33, STR_NETWORK_SET_PASSWORD,           STR_NETWORK_PASSWORD_TIP},             // NSSW_SETPWD

/* List of playable scenarios */
{       WWT_TEXT,   RESIZE_NONE,   BGC,    10,   110,    43,    55, STR_NETWORK_SELECT_MAP,             STR_NULL},
{      WWT_INSET,   RESIZE_NONE,   BGC,    10,   271,    62,   216, STR_NULL,                           STR_NETWORK_SELECT_MAP_TIP},           // NSSW_SELMAP
{  WWT_SCROLLBAR,   RESIZE_NONE,   BGC,   259,   270,    63,   215, 0x0,                                STR_0190_SCROLL_BAR_SCROLLS_LIST},

/* Combo/selection boxes to control Connection Type / Max Clients / Max Companies / Max Observers / Language */
{       WWT_TEXT,   RESIZE_NONE,   BGC,   280,   419,    63,    75, STR_NETWORK_CONNECTION,             STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,   BGC,   280,   410,    77,    88, STR_NETWORK_LAN_INTERNET_COMBO,     STR_NETWORK_CONNECTION_TIP},           // NSSW_CONNTYPE_BTN

{       WWT_TEXT,   RESIZE_NONE,   BGC,   280,   419,    95,   107, STR_NETWORK_NUMBER_OF_CLIENTS,      STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,   BGC,   280,   291,   109,   120, SPR_ARROW_DOWN,                     STR_NETWORK_NUMBER_OF_CLIENTS_TIP},    // NSSW_CLIENTS_BTND
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BGC,   292,   397,   109,   120, STR_NETWORK_CLIENTS_SELECT,         STR_NETWORK_NUMBER_OF_CLIENTS_TIP},    // NSSW_CLIENTS_TXT
{     WWT_IMGBTN,   RESIZE_NONE,   BGC,   398,   410,   109,   120, SPR_ARROW_UP,                       STR_NETWORK_NUMBER_OF_CLIENTS_TIP},    // NSSW_CLIENTS_BTNU

{       WWT_TEXT,   RESIZE_NONE,   BGC,   280,   419,   127,   139, STR_NETWORK_NUMBER_OF_COMPANIES,    STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,   BGC,   280,   291,   141,   152, SPR_ARROW_DOWN,                     STR_NETWORK_NUMBER_OF_COMPANIES_TIP},  // NSSW_COMPANIES_BTND
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BGC,   292,   397,   141,   152, STR_NETWORK_COMPANIES_SELECT,       STR_NETWORK_NUMBER_OF_COMPANIES_TIP},  // NSSW_COMPANIES_TXT
{     WWT_IMGBTN,   RESIZE_NONE,   BGC,   398,   410,   141,   152, SPR_ARROW_UP,                       STR_NETWORK_NUMBER_OF_COMPANIES_TIP},  // NSSW_COMPANIES_BTNU

{       WWT_TEXT,   RESIZE_NONE,   BGC,   280,   419,   159,   171, STR_NETWORK_NUMBER_OF_SPECTATORS,   STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,   BGC,   280,   291,   173,   184, SPR_ARROW_DOWN,                     STR_NETWORK_NUMBER_OF_SPECTATORS_TIP}, // NSSW_SPECTATORS_BTND
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BGC,   292,   397,   173,   184, STR_NETWORK_SPECTATORS_SELECT,      STR_NETWORK_NUMBER_OF_SPECTATORS_TIP}, // NSSW_SPECTATORS_TXT
{     WWT_IMGBTN,   RESIZE_NONE,   BGC,   398,   410,   173,   184, SPR_ARROW_UP,                       STR_NETWORK_NUMBER_OF_SPECTATORS_TIP}, // NSSW_SPECTATORS_BTNU

{       WWT_TEXT,   RESIZE_NONE,   BGC,   280,   419,   191,   203, STR_NETWORK_LANGUAGE_SPOKEN,        STR_NULL},
{ WWT_DROPDOWNIN,   RESIZE_NONE,   BGC,   280,   410,   205,   216, STR_NETWORK_LANGUAGE_COMBO,         STR_NETWORK_LANGUAGE_TIP},             // NSSW_LANGUAGE_BTN

/* Buttons Start / Load / Cancel */
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,    40,   140,   224,   235, STR_NETWORK_START_GAME,             STR_NETWORK_START_GAME_TIP},           // NSSW_START
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   150,   250,   224,   235, STR_NETWORK_LOAD_GAME,              STR_NETWORK_LOAD_GAME_TIP},            // NSSW_LOAD
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   260,   360,   224,   235, STR_012E_CANCEL,                    STR_NULL},                             // NSSW_CANCEL

{   WIDGETS_END},
};

static const WindowDesc _network_start_server_window_desc = {
	WDP_CENTER, WDP_CENTER, 420, 244, 420, 244,
	WC_NETWORK_WINDOW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_network_start_server_window_widgets,
	NULL,
};

static void ShowNetworkStartServerWindow()
{
	DeleteWindowById(WC_NETWORK_WINDOW, 0);

	new NetworkStartServerWindow(&_network_start_server_window_desc);
}

static PlayerID NetworkLobbyFindCompanyIndex(byte pos)
{
	/* Scroll through all _network_player_info and get the 'pos' item that is not empty */
	for (PlayerID i = PLAYER_FIRST; i < MAX_PLAYERS; i++) {
		if (_network_player_info[i].company_name[0] != '\0') {
			if (pos-- == 0) return i;
		}
	}

	return PLAYER_FIRST;
}

/** Enum for NetworkLobbyWindow, referring to _network_lobby_window_widgets */
enum NetworkLobbyWindowWidgets {
	NLWW_CLOSE    =  0, ///< Close 'X' button
	NLWW_MATRIX   =  5, ///< List of companies
	NLWW_DETAILS  =  7, ///< Company details
	NLWW_JOIN     =  8, ///< 'Join company' button
	NLWW_NEW      =  9, ///< 'New company' button
	NLWW_SPECTATE = 10, ///< 'Spectate game' button
	NLWW_REFRESH  = 11, ///< 'Refresh server' button
	NLWW_CANCEL   = 12, ///< 'Cancel' button
};

struct NetworkLobbyWindow : public Window {
	PlayerID company;        ///< Select company
	NetworkGameList *server; ///< Selected server

	NetworkLobbyWindow(const WindowDesc *desc, NetworkGameList *ngl) :
			Window(desc), company(INVALID_PLAYER), server(ngl)
	{
		this->vscroll.cap = 10;

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		const NetworkGameInfo *gi = &this->server->info;
		int y = NET_PRC__OFFSET_TOP_WIDGET_COMPANY, pos;

		/* Join button is disabled when no company is selected */
		this->SetWidgetDisabledState(NLWW_JOIN, this->company == INVALID_PLAYER);
		/* Cannot start new company if there are too many */
		this->SetWidgetDisabledState(NLWW_NEW, gi->companies_on >= gi->companies_max);
		/* Cannot spectate if there are too many spectators */
		this->SetWidgetDisabledState(NLWW_SPECTATE, gi->spectators_on >= gi->spectators_max);

		/* Draw window widgets */
		SetDParamStr(0, gi->server_name);
		DrawWindowWidgets(this);

		/* Draw company list */
		pos = this->vscroll.pos;
		while (pos < gi->companies_on) {
			byte company = NetworkLobbyFindCompanyIndex(pos);
			bool income = false;
			if (this->company == company) {
				GfxFillRect(11, y - 1, 154, y + 10, 10); // show highlighted item with a different colour
			}

			DoDrawStringTruncated(_network_player_info[company].company_name, 13, y, TC_BLACK, 135 - 13);
			if (_network_player_info[company].use_password != 0) DrawSprite(SPR_LOCK, PAL_NONE, 135, y);

			/* If the company's income was positive puts a green dot else a red dot */
			if (_network_player_info[company].income >= 0) income = true;
			DrawSprite(SPR_BLOT, income ? PALETTE_TO_GREEN : PALETTE_TO_RED, 145, y);

			pos++;
			y += NET_PRC__SIZE_OF_ROW;
			if (pos >= this->vscroll.cap) break;
		}

		/* Draw info about selected company when it is selected in the left window */
		GfxFillRect(174, 39, 403, 75, 157);
		DrawStringCentered(290, 50, STR_NETWORK_COMPANY_INFO, TC_FROMSTRING);
		if (this->company != INVALID_PLAYER) {
			const uint x = 183;
			const uint trunc_width = this->widget[NLWW_DETAILS].right - x;
			y = 80;

			SetDParam(0, gi->clients_on);
			SetDParam(1, gi->clients_max);
			SetDParam(2, gi->companies_on);
			SetDParam(3, gi->companies_max);
			DrawString(x, y, STR_NETWORK_CLIENTS, TC_GOLD);
			y += 10;

			SetDParamStr(0, _network_player_info[this->company].company_name);
			DrawStringTruncated(x, y, STR_NETWORK_COMPANY_NAME, TC_GOLD, trunc_width);
			y += 10;

			SetDParam(0, _network_player_info[this->company].inaugurated_year);
			DrawString(x, y, STR_NETWORK_INAUGURATION_YEAR, TC_GOLD); // inauguration year
			y += 10;

			SetDParam(0, _network_player_info[this->company].company_value);
			DrawString(x, y, STR_NETWORK_VALUE, TC_GOLD); // company value
			y += 10;

			SetDParam(0, _network_player_info[this->company].money);
			DrawString(x, y, STR_NETWORK_CURRENT_BALANCE, TC_GOLD); // current balance
			y += 10;

			SetDParam(0, _network_player_info[this->company].income);
			DrawString(x, y, STR_NETWORK_LAST_YEARS_INCOME, TC_GOLD); // last year's income
			y += 10;

			SetDParam(0, _network_player_info[this->company].performance);
			DrawString(x, y, STR_NETWORK_PERFORMANCE, TC_GOLD); // performance
			y += 10;

			SetDParam(0, _network_player_info[this->company].num_vehicle[0]);
			SetDParam(1, _network_player_info[this->company].num_vehicle[1]);
			SetDParam(2, _network_player_info[this->company].num_vehicle[2]);
			SetDParam(3, _network_player_info[this->company].num_vehicle[3]);
			SetDParam(4, _network_player_info[this->company].num_vehicle[4]);
			DrawString(x, y, STR_NETWORK_VEHICLES, TC_GOLD); // vehicles
			y += 10;

			SetDParam(0, _network_player_info[this->company].num_station[0]);
			SetDParam(1, _network_player_info[this->company].num_station[1]);
			SetDParam(2, _network_player_info[this->company].num_station[2]);
			SetDParam(3, _network_player_info[this->company].num_station[3]);
			SetDParam(4, _network_player_info[this->company].num_station[4]);
			DrawString(x, y, STR_NETWORK_STATIONS, TC_GOLD); // stations
			y += 10;

			SetDParamStr(0, _network_player_info[this->company].players);
			DrawStringTruncated(x, y, STR_NETWORK_PLAYERS, TC_GOLD, trunc_width); // players
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case NLWW_CLOSE:    // Close 'X'
			case NLWW_CANCEL:   // Cancel button
				ShowNetworkGameWindow();
				break;

			case NLWW_MATRIX: { // Company list
				uint32 id_v = (pt.y - NET_PRC__OFFSET_TOP_WIDGET_COMPANY) / NET_PRC__SIZE_OF_ROW;

				if (id_v >= this->vscroll.cap) break;

				id_v += this->vscroll.pos;
				this->company = (id_v >= this->server->info.companies_on) ? INVALID_PLAYER : NetworkLobbyFindCompanyIndex(id_v);
				this->SetDirty();
			} break;

			case NLWW_JOIN:     // Join company
				/* Button can be clicked only when it is enabled */
				_network_playas = this->company;
				NetworkClientConnectGame(_network_last_host, _network_last_port);
				break;

			case NLWW_NEW:      // New company
				_network_playas = PLAYER_NEW_COMPANY;
				NetworkClientConnectGame(_network_last_host, _network_last_port);
				break;

			case NLWW_SPECTATE: // Spectate game
				_network_playas = PLAYER_SPECTATOR;
				NetworkClientConnectGame(_network_last_host, _network_last_port);
				break;

			case NLWW_REFRESH:  // Refresh
				NetworkTCPQueryServer(_network_last_host, _network_last_port); // company info
				NetworkUDPQueryServer(_network_last_host, _network_last_port); // general data
				break;
		}
	}
};

static const Widget _network_lobby_window_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,   BGC,     0,    10,     0,    13, STR_00C5,                    STR_018B_CLOSE_WINDOW },           // NLWW_CLOSE
{    WWT_CAPTION,   RESIZE_NONE,   BGC,    11,   419,     0,    13, STR_NETWORK_GAME_LOBBY,      STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,   BGC,     0,   419,    14,   234, 0x0,                         STR_NULL},
{       WWT_TEXT,   RESIZE_NONE,   BGC,    10,   419,    22,    34, STR_NETWORK_PREPARE_TO_JOIN, STR_NULL},

/* company list */
{      WWT_PANEL,   RESIZE_NONE,   BTC,    10,   155,    38,    49, 0x0,                         STR_NULL},
{     WWT_MATRIX,   RESIZE_NONE,   BGC,    10,   155,    50,   190, (10 << 8) + 1,               STR_NETWORK_COMPANY_LIST_TIP},     // NLWW_MATRIX
{  WWT_SCROLLBAR,   RESIZE_NONE,   BGC,   156,   167,    38,   190, 0x0,                         STR_0190_SCROLL_BAR_SCROLLS_LIST},

/* company/player info */
{      WWT_PANEL,   RESIZE_NONE,   BGC,   173,   404,    38,   190, 0x0,                         STR_NULL},                         // NLWW_DETAILS

/* buttons */
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,    10,   151,   200,   211, STR_NETWORK_JOIN_COMPANY,    STR_NETWORK_JOIN_COMPANY_TIP},     // NLWW_JOIN
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,    10,   151,   215,   226, STR_NETWORK_NEW_COMPANY,     STR_NETWORK_NEW_COMPANY_TIP},      // NLWW_NEW
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   158,   268,   200,   211, STR_NETWORK_SPECTATE_GAME,   STR_NETWORK_SPECTATE_GAME_TIP},    // NLWW_SPECTATE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   158,   268,   215,   226, STR_NETWORK_REFRESH,         STR_NETWORK_REFRESH_TIP},          // NLWW_REFRESH
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   278,   388,   200,   211, STR_012E_CANCEL,             STR_NULL},                         // NLWW_CANCEL

{   WIDGETS_END},
};

static const WindowDesc _network_lobby_window_desc = {
	WDP_CENTER, WDP_CENTER, 420, 235, 420, 235,
	WC_NETWORK_WINDOW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_network_lobby_window_widgets,
	NULL,
};

/* Show the networklobbywindow with the selected server
 * @param ngl Selected game pointer which is passed to the new window */
static void ShowNetworkLobbyWindow(NetworkGameList *ngl)
{
	DeleteWindowById(WC_NETWORK_WINDOW, 0);

	NetworkTCPQueryServer(_network_last_host, _network_last_port); // company info
	NetworkUDPQueryServer(_network_last_host, _network_last_port); // general data

	new NetworkLobbyWindow(&_network_lobby_window_desc, ngl);
}

// The window below gives information about the connected clients
//  and also makes able to give money to them, kick them (if server)
//  and stuff like that.

extern void DrawPlayerIcon(PlayerID pid, int x, int y);

// Every action must be of this form
typedef void ClientList_Action_Proc(byte client_no);

// Max 10 actions per client
#define MAX_CLIENTLIST_ACTION 10

enum {
	CLNWND_OFFSET = 16,
	CLNWND_ROWSIZE = 10
};

static const Widget _client_list_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                 STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   237,     0,    13, STR_NETWORK_CLIENT_LIST,  STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   238,   249,     0,    13, STR_NULL,                 STR_STICKY_BUTTON},

{      WWT_PANEL,   RESIZE_NONE,    14,     0,   249,    14,    14 + CLNWND_ROWSIZE + 1, 0x0, STR_NULL},
{   WIDGETS_END},
};

static const Widget _client_list_popup_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   99,     0,     0,     0, STR_NULL},
{   WIDGETS_END},
};

static WindowDesc _client_list_desc = {
	WDP_AUTO, WDP_AUTO, 250, 1, 250, 1,
	WC_CLIENT_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_client_list_widgets,
	NULL
};

// Finds the Xth client-info that is active
static const NetworkClientInfo *NetworkFindClientInfo(byte client_no)
{
	const NetworkClientInfo *ci;

	FOR_ALL_ACTIVE_CLIENT_INFOS(ci) {
		if (client_no == 0) return ci;
		client_no--;
	}

	return NULL;
}

// Here we start to define the options out of the menu
static void ClientList_Kick(byte client_no)
{
	if (client_no < MAX_PLAYERS)
		SEND_COMMAND(PACKET_SERVER_ERROR)(DEREF_CLIENT(client_no), NETWORK_ERROR_KICKED);
}

static void ClientList_Ban(byte client_no)
{
	uint32 ip = NetworkFindClientInfo(client_no)->client_ip;

	for (uint i = 0; i < lengthof(_network_ban_list); i++) {
		if (_network_ban_list[i] == NULL) {
			_network_ban_list[i] = strdup(inet_ntoa(*(struct in_addr *)&ip));
			break;
		}
	}

	if (client_no < MAX_PLAYERS) {
		SEND_COMMAND(PACKET_SERVER_ERROR)(DEREF_CLIENT(client_no), NETWORK_ERROR_KICKED);
	}
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
		ShowNetworkChatQueryWindow(DESTTYPE_CLIENT, NetworkFindClientInfo(client_no)->client_index);
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

static void ClientList_None(byte client_no)
{
	/* No action ;) */
}



struct NetworkClientListPopupWindow : Window {
	int sel_index;
	int client_no;
	char action[MAX_CLIENTLIST_ACTION][50];
	ClientList_Action_Proc *proc[MAX_CLIENTLIST_ACTION];

	NetworkClientListPopupWindow(int x, int y, const Widget *widgets, int client_no) :
			Window(x, y, 150, 100, NULL, WC_TOOLBAR_MENU, widgets),
			sel_index(0), client_no(client_no)
	{
		/*
		 * Fill the actions this client has.
		 * Watch is, max 50 chars long!
		 */

		const NetworkClientInfo *ci = NetworkFindClientInfo(client_no);

		int i = 0;
		if (_network_own_client_index != ci->client_index) {
			GetString(this->action[i], STR_NETWORK_CLIENTLIST_SPEAK_TO_CLIENT, lastof(this->action[i]));
			this->proc[i++] = &ClientList_SpeakToClient;
		}

		if (IsValidPlayer(ci->client_playas) || ci->client_playas == PLAYER_SPECTATOR) {
			GetString(this->action[i], STR_NETWORK_CLIENTLIST_SPEAK_TO_COMPANY, lastof(this->action[i]));
			this->proc[i++] = &ClientList_SpeakToCompany;
		}
		GetString(this->action[i], STR_NETWORK_CLIENTLIST_SPEAK_TO_ALL, lastof(this->action[i]));
		this->proc[i++] = &ClientList_SpeakToAll;

		if (_network_own_client_index != ci->client_index) {
			/* We are no spectator and the player we want to give money to is no spectator and money gifts are allowed */
			if (IsValidPlayer(_network_playas) && IsValidPlayer(ci->client_playas) && _patches.give_money) {
				GetString(this->action[i], STR_NETWORK_CLIENTLIST_GIVE_MONEY, lastof(this->action[i]));
				this->proc[i++] = &ClientList_GiveMoney;
			}
		}

		/* A server can kick clients (but not himself) */
		if (_network_server && _network_own_client_index != ci->client_index) {
			GetString(this->action[i], STR_NETWORK_CLIENTLIST_KICK, lastof(this->action[i]));
			this->proc[i++] = &ClientList_Kick;

			sprintf(this->action[i],"Ban"); // XXX GetString?
			this->proc[i++] = &ClientList_Ban;
		}

		if (i == 0) {
			GetString(this->action[i], STR_NETWORK_CLIENTLIST_NONE, lastof(this->action[i]));
			this->proc[i++] = &ClientList_None;
		}

		/* Calculate the height */
		int h = ClientListPopupHeight();

		/* Allocate the popup */
		this->widget[0].bottom = this->widget[0].top + h;
		this->widget[0].right = this->widget[0].left + 150;

		this->flags4 &= ~WF_WHITE_BORDER_MASK;

		this->FindWindowPlacementAndResize(150, h + 1);
	}

	/**
	 * An action is clicked! What do we do?
	 */
	void HandleClientListPopupClick(byte index)
	{
		/* A click on the Popup of the ClientList.. handle the command */
		if (index < MAX_CLIENTLIST_ACTION && this->proc[index] != NULL) {
			this->proc[index](this->client_no);
		}
	}

	/**
	 * Finds the amount of actions in the popup and set the height correct
	 */
	uint ClientListPopupHeight()
	{
		int num = 0;

		// Find the amount of actions
		for (int i = 0; i < MAX_CLIENTLIST_ACTION; i++) {
			if (this->action[i][0] == '\0') continue;
			if (this->proc[i] == NULL) continue;
			num++;
		}

		num *= CLNWND_ROWSIZE;

		return num + 1;
	}


	virtual void OnPaint()
	{
		DrawWindowWidgets(this);

		/* Draw the actions */
		int sel = this->sel_index;
		int y = 1;
		for (int i = 0; i < MAX_CLIENTLIST_ACTION; i++, y += CLNWND_ROWSIZE) {
			if (this->action[i][0] == '\0') continue;
			if (this->proc[i] == NULL) continue;

			TextColour colour;
			if (sel-- == 0) { // Selected item, highlight it
				GfxFillRect(1, y, 150 - 2, y + CLNWND_ROWSIZE - 1, 0);
				colour = TC_WHITE;
			} else {
				colour = TC_BLACK;
			}

			DoDrawString(this->action[i], 4, y, colour);
		}
	}

	virtual void OnMouseLoop()
	{
		/* We selected an action */
		int index = (_cursor.pos.y - this->top) / CLNWND_ROWSIZE;

		if (_left_button_down) {
			if (index == -1 || index == this->sel_index) return;

			this->sel_index = index;
			this->SetDirty();
		} else {
			if (index >= 0 && _cursor.pos.y >= this->top) {
				HandleClientListPopupClick(index);
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

	new NetworkClientListPopupWindow(x, y, _client_list_popup_widgets, client_no);
}

/**
 * Main handle for clientlist
 */
struct NetworkClientListWindow : Window
{
	byte selected_item;
	byte selected_y;

	NetworkClientListWindow(const WindowDesc *desc, WindowNumber window_number) :
			Window(desc, window_number),
			selected_item(0),
			selected_y(255)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	/**
	 * Finds the amount of clients and set the height correct
	 */
	bool CheckClientListHeight()
	{
		int num = 0;
		const NetworkClientInfo *ci;

		/* Should be replaced with a loop through all clients */
		FOR_ALL_ACTIVE_CLIENT_INFOS(ci) {
			num++;
		}

		num *= CLNWND_ROWSIZE;

		/* If height is changed */
		if (this->height != CLNWND_OFFSET + num + 1) {
			// XXX - magic unfortunately; (num + 2) has to be one bigger than heigh (num + 1)
			this->SetDirty();
			this->widget[3].bottom = this->widget[3].top + num + 2;
			this->height = CLNWND_OFFSET + num + 1;
			this->SetDirty();
			return false;
		}
		return true;
	}

	virtual void OnPaint()
	{
		NetworkClientInfo *ci;
		int i = 0;

		/* Check if we need to reset the height */
		if (!this->CheckClientListHeight()) return;

		DrawWindowWidgets(this);

		int y = CLNWND_OFFSET;

		FOR_ALL_ACTIVE_CLIENT_INFOS(ci) {
			TextColour colour;
			if (this->selected_item == i++) { // Selected item, highlight it
				GfxFillRect(1, y, 248, y + CLNWND_ROWSIZE - 1, 0);
				colour = TC_WHITE;
			} else {
				colour = TC_BLACK;
			}

			if (ci->client_index == NETWORK_SERVER_INDEX) {
				DrawString(4, y, STR_NETWORK_SERVER, colour);
			} else {
				DrawString(4, y, STR_NETWORK_CLIENT, colour);
			}

			/* Filter out spectators */
			if (IsValidPlayer(ci->client_playas)) DrawPlayerIcon(ci->client_playas, 64, y + 1);

			DoDrawString(ci->client_name, 81, y, colour);

			y += CLNWND_ROWSIZE;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		/* Show the popup with option */
		if (this->selected_item != 255) {
			PopupClientList(this->selected_item, pt.x + this->left, pt.y + this->top);
		}
	}

	virtual void OnMouseOver(Point pt, int widget)
	{
		/* -1 means we left the current window */
		if (pt.y == -1) {
			this->selected_y = 0;
			this->selected_item = 255;
			this->SetDirty();
			return;
		}
		/* It did not change.. no update! */
		if (pt.y == this->selected_y) return;

		/* Find the new selected item (if any) */
		this->selected_y = pt.y;
		if (pt.y > CLNWND_OFFSET) {
			this->selected_item = (pt.y - CLNWND_OFFSET) / CLNWND_ROWSIZE;
		} else {
			this->selected_item = 255;
		}

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
	ShowQueryString(STR_EMPTY, caption, 20, 180, FindWindowById(WC_NETWORK_STATUS_WINDOW, 0), CS_ALPHANUMERAL);
}

struct NetworkJoinStatusWindow : Window {
	NetworkJoinStatusWindow(const WindowDesc *desc) : Window(desc)
	{
		this->parent = FindWindowById(WC_NETWORK_WINDOW, 0);
	}

	virtual void OnPaint()
	{
		uint8 progress; // used for progress bar
		DrawWindowWidgets(this);

		DrawStringCentered(125, 35, STR_NETWORK_CONNECTING_1 + _network_join_status, TC_GREY);
		switch (_network_join_status) {
			case NETWORK_JOIN_STATUS_CONNECTING: case NETWORK_JOIN_STATUS_AUTHORIZING:
			case NETWORK_JOIN_STATUS_GETTING_COMPANY_INFO:
				progress = 10; // first two stages 10%
				break;
			case NETWORK_JOIN_STATUS_WAITING:
				SetDParam(0, _network_join_waiting);
				DrawStringCentered(125, 46, STR_NETWORK_CONNECTING_WAITING, TC_GREY);
				progress = 15; // third stage is 15%
				break;
			case NETWORK_JOIN_STATUS_DOWNLOADING:
				SetDParam(0, _network_join_kbytes);
				SetDParam(1, _network_join_kbytes_total);
				DrawStringCentered(125, 46, STR_NETWORK_CONNECTING_DOWNLOADING, TC_GREY);
				/* Fallthrough */
			default: /* Waiting is 15%, so the resting receivement of map is maximum 70% */
				progress = 15 + _network_join_kbytes * (100 - 15) / _network_join_kbytes_total;
		}

		/* Draw nice progress bar :) */
		DrawFrameRect(20, 18, (int)((this->width - 20) * progress / 100), 28, 10, FR_NONE);
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget == 2) { //Disconnect button
			NetworkDisconnect();
			SwitchMode(SM_MENU);
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

static const Widget _network_join_status_window_widget[] = {
{    WWT_CAPTION,   RESIZE_NONE,    14,     0,   249,     0,    13, STR_NETWORK_CONNECTING, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   249,    14,    84, 0x0,                    STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,    75,   175,    69,    80, STR_NETWORK_DISCONNECT, STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _network_join_status_window_desc = {
	WDP_CENTER, WDP_CENTER, 250, 85, 250, 85,
	WC_NETWORK_STATUS_WINDOW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_MODAL,
	_network_join_status_window_widget,
	NULL,
};

void ShowJoinStatusWindow()
{
	DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
	new NetworkJoinStatusWindow(&_network_join_status_window_desc);
}

static void SendChat(const char *buf, DestType type, int dest)
{
	if (StrEmpty(buf)) return;
	if (!_network_server) {
		SEND_COMMAND(PACKET_CLIENT_CHAT)((NetworkAction)(NETWORK_ACTION_CHAT + type), type, dest, buf);
	} else {
		NetworkServer_HandleChat((NetworkAction)(NETWORK_ACTION_CHAT + type), type, dest, buf, NETWORK_SERVER_INDEX);
	}
}


struct NetworkChatWindow : public QueryStringBaseWindow {
	DestType dtype;
	int dest;

	NetworkChatWindow (const WindowDesc *desc, DestType type, int dest) : QueryStringBaseWindow(desc)
	{
		this->LowerWidget(2);
		this->dtype   = type;
		this->dest    = dest;
		this->afilter = CS_ALPHANUMERAL;
		InitializeTextBuffer(&this->text, this->edit_str_buf, lengthof(this->edit_str_buf), 0);

		InvalidateWindowData(WC_NEWS_WINDOW, 0, this->height);
		SetBit(_no_scroll, SCROLL_CHAT); // do not scroll the game with the arrow-keys

		_chat_tab_completion_active = false;

		this->FindWindowPlacementAndResize(desc);
	}

	~NetworkChatWindow ()
	{
		InvalidateWindowData(WC_NEWS_WINDOW, 0, 0);
		ClrBit(_no_scroll, SCROLL_CHAT);
	}

	/**
	 * Find the next item of the list of things that can be auto-completed.
	 * @param item The current indexed item to return. This function can, and most
	 *     likely will, alter item, to skip empty items in the arrays.
	 * @return Returns the char that matched to the index.
	 */
	const char *ChatTabCompletionNextItem(uint *item)
	{
		static char chat_tab_temp_buffer[64];

		/* First, try clients */
		if (*item < MAX_CLIENT_INFO) {
			/* Skip inactive clients */
			while (_network_client_info[*item].client_index == NETWORK_EMPTY_INDEX && *item < MAX_CLIENT_INFO) (*item)++;
			if (*item < MAX_CLIENT_INFO) return _network_client_info[*item].client_name;
		}

		/* Then, try townnames */
		/* Not that the following assumes all town indices are adjacent, ie no
		* towns have been deleted. */
		if (*item <= (uint)MAX_CLIENT_INFO + GetMaxTownIndex()) {
			const Town *t;

			FOR_ALL_TOWNS_FROM(t, *item - MAX_CLIENT_INFO) {
				/* Get the town-name via the string-system */
				SetDParam(0, t->index);
				GetString(chat_tab_temp_buffer, STR_TOWN, lastof(chat_tab_temp_buffer));
				return &chat_tab_temp_buffer[0];
			}
		}

		return NULL;
	}

	/**
	 * Find what text to complete. It scans for a space from the left and marks
	 *  the word right from that as to complete. It also writes a \0 at the
	 *  position of the space (if any). If nothing found, buf is returned.
	 */
	static char *ChatTabCompletionFindText(char *buf)
	{
		char *p = strrchr(buf, ' ');
		if (p == NULL) return buf;

		*p = '\0';
		return p + 1;
	}

	/**
	 * See if we can auto-complete the current text of the user.
	 */
	void ChatTabCompletion()
	{
		static char _chat_tab_completion_buf[lengthof(this->edit_str_buf)];
		Textbuf *tb = &this->text;
		size_t len, tb_len;
		uint item;
		char *tb_buf, *pre_buf;
		const char *cur_name;
		bool second_scan = false;

		item = 0;

		/* Copy the buffer so we can modify it without damaging the real data */
		pre_buf = (_chat_tab_completion_active) ? strdup(_chat_tab_completion_buf) : strdup(tb->buf);

		tb_buf  = ChatTabCompletionFindText(pre_buf);
		tb_len  = strlen(tb_buf);

		while ((cur_name = ChatTabCompletionNextItem(&item)) != NULL) {
			item++;

			if (_chat_tab_completion_active) {
				/* We are pressing TAB again on the same name, is there an other name
				*  that starts with this? */
				if (!second_scan) {
					size_t offset;
					size_t length;

					/* If we are completing at the begin of the line, skip the ': ' we added */
					if (tb_buf == pre_buf) {
						offset = 0;
						length = tb->length - 2;
					} else {
						/* Else, find the place we are completing at */
						offset = strlen(pre_buf) + 1;
						length = tb->length - offset;
					}

					/* Compare if we have a match */
					if (strlen(cur_name) == length && strncmp(cur_name, tb->buf + offset, length) == 0) second_scan = true;

					continue;
				}

				/* Now any match we make on _chat_tab_completion_buf after this, is perfect */
			}

			len = strlen(cur_name);
			if (tb_len < len && strncasecmp(cur_name, tb_buf, tb_len) == 0) {
				/* Save the data it was before completion */
				if (!second_scan) snprintf(_chat_tab_completion_buf, lengthof(_chat_tab_completion_buf), "%s", tb->buf);
				_chat_tab_completion_active = true;

				/* Change to the found name. Add ': ' if we are at the start of the line (pretty) */
				if (pre_buf == tb_buf) {
					snprintf(tb->buf, lengthof(this->edit_str_buf), "%s: ", cur_name);
				} else {
					snprintf(tb->buf, lengthof(this->edit_str_buf), "%s %s", pre_buf, cur_name);
				}

				/* Update the textbuffer */
				UpdateTextBufferSize(&this->text);

				this->SetDirty();
				free(pre_buf);
				return;
			}
		}

		if (second_scan) {
			/* We walked all posibilities, and the user presses tab again.. revert to original text */
			strcpy(tb->buf, _chat_tab_completion_buf);
			_chat_tab_completion_active = false;

			/* Update the textbuffer */
			UpdateTextBufferSize(&this->text);

			this->SetDirty();
		}
		free(pre_buf);
	}

	virtual void OnPaint()
	{
		static const StringID chat_captions[] = {
			STR_NETWORK_CHAT_ALL_CAPTION,
			STR_NETWORK_CHAT_COMPANY_CAPTION,
			STR_NETWORK_CHAT_CLIENT_CAPTION
		};

		DrawWindowWidgets(this);

		assert((uint)this->dtype < lengthof(chat_captions));
		DrawStringRightAligned(this->widget[2].left - 2, this->widget[2].top + 1, chat_captions[this->dtype], TC_BLACK);
		this->DrawEditBox(2);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case 2:
				ShowOnScreenKeyboard(this, 2, 0, 3);
				break;

			case 3: /* Send */
				SendChat(this->text.buf, this->dtype, this->dest);
			/* FALLTHROUGH */
			case 0: /* Cancel */ delete this; break;
		}
	}

	virtual void OnMouseLoop()
	{
		this->HandleEditBox(2);
	}

	virtual bool OnKeyPress(uint16 key, uint16 keycode)
	{
		bool cont = true;
		if (keycode == WKC_TAB) {
			ChatTabCompletion();
		} else {
			_chat_tab_completion_active = false;
			switch (this->HandleEditBoxKey(2, key, keycode, cont)) {
				case 1: /* Return */
					SendChat(this->text.buf, this->dtype, this->dest);
				/* FALLTHROUGH */
				case 2: /* Escape */ delete this; break;
			}
		}
		return cont;
	}
};

static const Widget _chat_window_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE,  14,   0,  10,  0, 13, STR_00C5,                  STR_018B_CLOSE_WINDOW},
{      WWT_PANEL, RESIZE_RIGHT, 14,  11, 319,  0, 13, 0x0,                       STR_NULL}, // background
{    WWT_EDITBOX, RESIZE_RIGHT, 14,  75, 257,  1, 12, STR_NETWORK_CHAT_OSKTITLE, STR_NULL}, // text box
{ WWT_PUSHTXTBTN, RESIZE_LR,    14, 258, 319,  1, 12, STR_NETWORK_SEND,          STR_NULL}, // send button
{   WIDGETS_END},
};

static const WindowDesc _chat_window_desc = {
	WDP_CENTER, -26, 320, 14, 640, 14, // x, y, width, height
	WC_SEND_NETWORK_MSG, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET,
	_chat_window_widgets,
	NULL
};

void ShowNetworkChatQueryWindow(DestType type, int dest)
{
	DeleteWindowById(WC_SEND_NETWORK_MSG, 0);
	new NetworkChatWindow (&_chat_window_desc, type, dest);
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
	NetworkCompanyPasswordWindow(const WindowDesc *desc, Window *parent) : QueryStringBaseWindow(desc)
	{
		this->parent = parent;
		this->afilter = CS_ALPHANUMERAL;
		InitializeTextBuffer(&this->text, this->edit_str_buf, min(lengthof(_network_default_company_pass), lengthof(this->edit_str_buf)), 0);

		this->FindWindowPlacementAndResize(desc);
	}

	void OnOk()
	{
		if (this->IsWidgetLowered(NCPWW_SAVE_AS_DEFAULT_PASSWORD)) {
			snprintf(_network_default_company_pass, lengthof(_network_default_company_pass), "%s", this->edit_str_buf);
		}

		/* empty password is a '*' because of console argument */
		if (StrEmpty(this->edit_str_buf)) snprintf(this->edit_str_buf, lengthof(this->edit_str_buf), "*");
		char *password = this->edit_str_buf;
		NetworkChangeCompanyPassword(1, &password);
	}

	virtual void OnPaint()
	{
		DrawWindowWidgets(this);
		this->DrawEditBox(4);
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

			case NCPWW_PASSWORD:
				ShowOnScreenKeyboard(this, NCPWW_PASSWORD, 2, 1);
				break;
		}
	}

	virtual void OnMouseLoop()
	{
		this->HandleEditBox(4);
	}

	virtual bool OnKeyPress(uint16 key, uint16 keycode)
	{
		bool cont;
		switch (this->HandleEditBoxKey(4, key, keycode, cont)) {
			case 1: // Return
				this->OnOk();
				/* FALL THROUGH */

			case 2: // Escape
				delete this;
				break;
		}
		return cont;
	}
};

static const Widget _ncp_window_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE, 14,   0,  10,  0, 13, STR_00C5,                          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION, RESIZE_NONE, 14,  11, 299,  0, 13, STR_COMPANY_PASSWORD_CAPTION,      STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL, RESIZE_NONE, 14,   0, 299, 14, 50, 0x0,                               STR_NULL},
{       WWT_TEXT, RESIZE_NONE, 14,   5, 100, 19, 30, STR_COMPANY_PASSWORD,              STR_NULL},
{    WWT_EDITBOX, RESIZE_NONE, 14, 101, 294, 19, 30, STR_SET_COMPANY_PASSWORD,          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 14, 101, 294, 35, 46, STR_MAKE_DEFAULT_COMPANY_PASSWORD, STR_MAKE_DEFAULT_COMPANY_PASSWORD_TIP},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 14,   0, 149, 51, 62, STR_012E_CANCEL,                   STR_COMPANY_PASSWORD_CANCEL},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 14, 150, 299, 51, 62, STR_012F_OK,                       STR_COMPANY_PASSWORD_OK},
{   WIDGETS_END},
};

static const WindowDesc _ncp_window_desc = {
	WDP_AUTO, WDP_AUTO, 300, 63, 300, 63,
	WC_COMPANY_PASSWORD_WINDOW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_ncp_window_widgets,
	NULL
};

void ShowNetworkCompanyPasswordWindow(Window *parent)
{
	DeleteWindowById(WC_COMPANY_PASSWORD_WINDOW, 0);

	new NetworkCompanyPasswordWindow(&_ncp_window_desc, parent);
}

#endif /* ENABLE_NETWORK */
