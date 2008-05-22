/* $Id$ */

/** @file town_gui.cpp GUI for towns. */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "town.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "player_func.h"
#include "player_base.h"
#include "player_gui.h"
#include "network/network.h"
#include "variables.h"
#include "strings_func.h"
#include "sound_func.h"
#include "economy_func.h"
#include "core/alloc_func.hpp"
#include "settings_type.h"
#include "tilehighlight_func.h"
#include "string_func.h"

#include "table/sprites.h"
#include "table/strings.h"

extern bool GenerateTowns();
static int _scengen_town_size = 1; // depress medium-sized towns per default

static const Widget _town_authority_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    13,     0,    10,     0,    13, STR_00C5,                 STR_018B_CLOSE_WINDOW},              // TWA_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,    13,    11,   316,     0,    13, STR_2022_LOCAL_AUTHORITY, STR_018C_WINDOW_TITLE_DRAG_THIS},    // TWA_CAPTION
{      WWT_PANEL,   RESIZE_NONE,    13,     0,   316,    14,   105, 0x0,                      STR_NULL},                           // TWA_RATING_INFO
{      WWT_PANEL,   RESIZE_NONE,    13,     0,   304,   106,   157, 0x0,                      STR_2043_LIST_OF_THINGS_TO_DO_AT},   // TWA_COMMAND_LIST
{  WWT_SCROLLBAR,   RESIZE_NONE,    13,   305,   316,   106,   157, 0x0,                      STR_0190_SCROLL_BAR_SCROLLS_LIST},   // TWA_SCROLLBAR
{      WWT_PANEL,   RESIZE_NONE,    13,     0,   316,   158,   209, 0x0,                      STR_NULL},                           // TWA_ACTION_INFO
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,     0,   316,   210,   221, STR_2042_DO_IT,           STR_2044_CARRY_OUT_THE_HIGHLIGHTED}, // TWA_EXECUTE
{   WIDGETS_END},
};

extern const byte _town_action_costs[8];

enum TownActions {
	TACT_NONE             = 0x00,

	TACT_ADVERTISE_SMALL  = 0x01,
	TACT_ADVERTISE_MEDIUM = 0x02,
	TACT_ADVERTISE_LARGE  = 0x04,
	TACT_ROAD_REBUILD     = 0x08,
	TACT_BUILD_STATUE     = 0x10,
	TACT_FOUND_BUILDINGS  = 0x20,
	TACT_BUY_RIGHTS       = 0x40,
	TACT_BRIBE            = 0x80,

	TACT_ADVERTISE        = TACT_ADVERTISE_SMALL | TACT_ADVERTISE_MEDIUM | TACT_ADVERTISE_LARGE,
	TACT_CONSTRUCTION     = TACT_ROAD_REBUILD | TACT_BUILD_STATUE | TACT_FOUND_BUILDINGS,
	TACT_FUNDS            = TACT_BUY_RIGHTS | TACT_BRIBE,
	TACT_ALL              = TACT_ADVERTISE | TACT_CONSTRUCTION | TACT_FUNDS,
};

DECLARE_ENUM_AS_BIT_SET(TownActions);

/** Get a list of available actions to do at a town.
 * @param nump if not NULL add put the number of available actions in it
 * @param pid the player that is querying the town
 * @param t the town that is queried
 * @return bitmasked value of enabled actions
 */
uint GetMaskOfTownActions(int *nump, PlayerID pid, const Town *t)
{
	int num = 0;
	TownActions buttons = TACT_NONE;

	/* Spectators and unwanted have no options */
	if (pid != PLAYER_SPECTATOR && !(_patches.bribe && t->unwanted[pid])) {

		/* Things worth more than this are not shown */
		Money avail = GetPlayer(pid)->player_money + _price.station_value * 200;
		Money ref = _price.build_industry >> 8;

		/* Check the action bits for validity and
		 * if they are valid add them */
		for (uint i = 0; i != lengthof(_town_action_costs); i++) {
			const TownActions cur = (TownActions)(1 << i);

			/* Is the player not able to bribe ? */
			if (cur == TACT_BRIBE && (!_patches.bribe || t->ratings[pid] >= RATING_BRIBE_MAXIMUM))
				continue;

			/* Is the player not able to buy exclusive rights ? */
			if (cur == TACT_BUY_RIGHTS && !_patches.exclusive_rights)
				continue;

			/* Is the player not able to build a statue ? */
			if (cur == TACT_BUILD_STATUE && HasBit(t->statues, pid))
				continue;

			if (avail >= _town_action_costs[i] * ref) {
				buttons |= cur;
				num++;
			}
		}
	}

	if (nump != NULL) *nump = num;
	return buttons;
}

/**
 * Get the position of the Nth set bit.
 *
 * If there is no Nth bit set return -1
 *
 * @param bits The value to search in
 * @param n The Nth set bit from which we want to know the position
 * @return The position of the Nth set bit
 */
static int GetNthSetBit(uint32 bits, int n)
{
	if (n >= 0) {
		uint i;
		FOR_EACH_SET_BIT(i, bits) {
			n--;
			if (n < 0) return i;
		}
	}
	return -1;
}

struct TownAuthorityWindow : Window {
private:
	Town *town;
	int sel_index;

	enum TownAuthorityWidget {
		TWA_CLOSEBOX = 0,
		TWA_CAPTION,
		TWA_RATING_INFO,
		TWA_COMMAND_LIST,
		TWA_SCROLLBAR,
		TWA_ACTION_INFO,
		TWA_EXECUTE,
	};

public:
	TownAuthorityWindow(const WindowDesc *desc, WindowNumber window_number) :
			Window(desc, window_number), sel_index(-1)
	{
		this->town = GetTown(this->window_number);
		this->vscroll.cap = 5;

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		int numact;
		uint buttons = GetMaskOfTownActions(&numact, _local_player, this->town);

		SetVScrollCount(this, numact + 1);

		if (this->sel_index != -1 && !HasBit(buttons, this->sel_index)) {
			this->sel_index = -1;
		}

		this->SetWidgetDisabledState(6, this->sel_index == -1);

		SetDParam(0, this->window_number);
		this->DrawWidgets();

		DrawString(2, 15, STR_2023_TRANSPORT_COMPANY_RATINGS, TC_FROMSTRING);

		/* Draw list of players */
		int y = 25;

		const Player *p;
		FOR_ALL_PLAYERS(p) {
			if (p->is_active && (HasBit(this->town->have_ratings, p->index) || this->town->exclusivity == p->index)) {
				DrawPlayerIcon(p->index, 2, y);

				SetDParam(0, p->index);
				SetDParam(1, p->index);

				int r = this->town->ratings[p->index];
				StringID str;
				(str = STR_3035_APPALLING, r <= RATING_APPALLING) || // Apalling
				(str++,                    r <= RATING_VERYPOOR)  || // Very Poor
				(str++,                    r <= RATING_POOR)      || // Poor
				(str++,                    r <= RATING_MEDIOCRE)  || // Mediocore
				(str++,                    r <= RATING_GOOD)      || // Good
				(str++,                    r <= RATING_VERYGOOD)  || // Very Good
				(str++,                    r <= RATING_EXCELLENT) || // Excellent
				(str++,                    true);                    // Outstanding

				SetDParam(2, str);
				if (this->town->exclusivity == p->index) { // red icon for player with exclusive rights
					DrawSprite(SPR_BLOT, PALETTE_TO_RED, 18, y);
				}

				DrawString(28, y, STR_2024, TC_FROMSTRING);
				y += 10;
			}
		}
		y = 107;
		int pos = this->vscroll.pos;

		if (--pos < 0) {
			DrawString(2, y, STR_2045_ACTIONS_AVAILABLE, TC_FROMSTRING);
			y += 10;
		}

		for (int i = 0; buttons; i++, buttons >>= 1) {
			if (pos <= -5) break; ///< Draw only the 5 fitting lines

			if ((buttons & 1) && --pos < 0) {
				DrawString(3, y, STR_2046_SMALL_ADVERTISING_CAMPAIGN + i, TC_ORANGE);
				y += 10;
			}
		}

		if (this->sel_index != -1) {
			SetDParam(1, (_price.build_industry >> 8) * _town_action_costs[this->sel_index]);
			SetDParam(0, STR_2046_SMALL_ADVERTISING_CAMPAIGN + this->sel_index);
			DrawStringMultiLine(2, 159, STR_204D_INITIATE_A_SMALL_LOCAL + this->sel_index, 313);
		}
	}

	virtual void OnDoubleClick(Point pt, int widget) { HandleClick(pt, widget, true); }
	virtual void OnClick(Point pt, int widget) { HandleClick(pt, widget, false); }

	void HandleClick(Point pt, int widget, bool double_click)
	{
		switch (widget) {
			case TWA_COMMAND_LIST: {
				int y = (pt.y - 0x6B) / 10;

				if (!IsInsideMM(y, 0, 5)) return;

				y = GetNthSetBit(GetMaskOfTownActions(NULL, _local_player, this->town), y + this->vscroll.pos - 1);
				if (y >= 0) {
					this->sel_index = y;
					this->SetDirty();
				}
				/* Fall through to clicking in case we are double-clicked */
				if (!double_click || y < 0) break;
			}

			case TWA_EXECUTE:
				DoCommandP(this->town->xy, this->window_number, this->sel_index, NULL, CMD_DO_TOWN_ACTION | CMD_MSG(STR_00B4_CAN_T_DO_THIS));
				break;
		}
	}

	virtual void OnHundredthTick()
	{
		this->SetDirty();
	}
};

static const WindowDesc _town_authority_desc = {
	WDP_AUTO, WDP_AUTO, 317, 222, 317, 222,
	WC_TOWN_AUTHORITY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_town_authority_widgets,
};

static void ShowTownAuthorityWindow(uint town)
{
	AllocateWindowDescFront<TownAuthorityWindow>(&_town_authority_desc, town);
}

struct TownViewWindow : Window {
private:
	Town *town;

	enum TownViewWidget {
		TVW_CAPTION = 1,
		TVW_STICKY,
		TVW_CENTERVIEW = 6,
		TVW_SHOWAUTORITY,
		TVW_CHANGENAME,
		TVW_EXPAND,
		TVW_DELETE,
	};

public:
	TownViewWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->town = GetTown(this->window_number);
		bool ingame = _game_mode != GM_EDITOR;

		this->flags4 |= WF_DISABLE_VP_SCROLL;
		InitializeWindowViewport(this, 3, 17, 254, 86, this->town->xy, ZOOM_LVL_TOWN);

		if (this->town->larger_town) this->widget[TVW_CAPTION].data = STR_CITY;
		this->SetWidgetHiddenState(TVW_DELETE, ingame);  // hide delete button on game mode
		this->SetWidgetHiddenState(TVW_EXPAND, ingame);  // hide expand button on game mode
		this->SetWidgetHiddenState(TVW_SHOWAUTORITY, !ingame); // hide autority button on editor mode

		if (ingame) {
			/* resize caption bar */
			this->widget[TVW_CAPTION].right = this->widget[TVW_STICKY].left -1;
			/* move the rename from top on scenario to bottom in game */
			this->widget[TVW_CHANGENAME].top = this->widget[TVW_EXPAND].top;
			this->widget[TVW_CHANGENAME].bottom = this->widget[TVW_EXPAND].bottom;
			this->widget[TVW_CHANGENAME].right = this->widget[TVW_STICKY].right;
		}

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		/* disable renaming town in network games if you are not the server */
		this->SetWidgetDisabledState(TVW_CHANGENAME, _networking && !_network_server);

		SetDParam(0, this->town->index);
		this->DrawWidgets();

		SetDParam(0, this->town->population);
		SetDParam(1, this->town->num_houses);
		DrawString(2, 107, STR_2006_POPULATION, TC_FROMSTRING);

		SetDParam(0, this->town->act_pass);
		SetDParam(1, this->town->max_pass);
		DrawString(2, 117, STR_200D_PASSENGERS_LAST_MONTH_MAX, TC_FROMSTRING);

		SetDParam(0, this->town->act_mail);
		SetDParam(1, this->town->max_mail);
		DrawString(2, 127, STR_200E_MAIL_LAST_MONTH_MAX, TC_FROMSTRING);

		this->DrawViewport();
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case TVW_CENTERVIEW: /* scroll to location */
				if (_ctrl_pressed) {
					ShowExtraViewPortWindow(this->town->xy);
				} else {
					ScrollMainWindowToTile(this->town->xy);
				}
				break;

			case TVW_SHOWAUTORITY: /* town authority */
				ShowTownAuthorityWindow(this->window_number);
				break;

			case TVW_CHANGENAME: /* rename */
				SetDParam(0, this->window_number);
				ShowQueryString(STR_TOWN, STR_2007_RENAME_TOWN, 31, 130, this, CS_ALPHANUMERAL);
				break;

			case TVW_EXPAND: /* expand town - only available on Scenario editor */
				ExpandTown(this->town);
				break;

			case TVW_DELETE: /* delete town - only available on Scenario editor */
				delete this->town;
				break;
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (!StrEmpty(str)) {
			_cmd_text = str;
			DoCommandP(0, this->window_number, 0, NULL,
				CMD_RENAME_TOWN | CMD_MSG(STR_2008_CAN_T_RENAME_TOWN));
		}
	}
};


static const Widget _town_view_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    13,     0,    10,     0,    13, STR_00C5,                 STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    13,    11,   172,     0,    13, STR_2005,                 STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    13,   248,   259,     0,    13, 0x0,                      STR_STICKY_BUTTON},
{      WWT_PANEL,   RESIZE_NONE,    13,     0,   259,    14,   105, 0x0,                      STR_NULL},
{      WWT_INSET,   RESIZE_NONE,    13,     2,   257,    16,   103, 0x0,                      STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    13,     0,   259,   106,   137, 0x0,                      STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,     0,    85,   138,   149, STR_00E4_LOCATION,        STR_200B_CENTER_THE_MAIN_VIEW_ON},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,    86,   171,   138,   149, STR_2020_LOCAL_AUTHORITY, STR_2021_SHOW_INFORMATION_ON_LOCAL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,   172,   247,     0,    13, STR_0130_RENAME,          STR_200C_CHANGE_TOWN_NAME},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,    86,   171,   138,   149, STR_023C_EXPAND,          STR_023B_INCREASE_SIZE_OF_TOWN},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,   172,   259,   138,   149, STR_0290_DELETE,          STR_0291_DELETE_THIS_TOWN_COMPLETELY},
{   WIDGETS_END},
};

static const WindowDesc _town_view_desc = {
	WDP_AUTO, WDP_AUTO, 260, 150, 260, 150,
	WC_TOWN_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_town_view_widgets,
};

void ShowTownViewWindow(TownID town)
{
	AllocateWindowDescFront<TownViewWindow>(&_town_view_desc, town);
}

static const Widget _town_directory_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    13,     0,    10,     0,    13, STR_00C5,               STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    13,    11,   195,     0,    13, STR_2000_TOWNS,         STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    13,   196,   207,     0,    13, 0x0,                    STR_STICKY_BUTTON},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,     0,    98,    14,    25, STR_SORT_BY_NAME,       STR_SORT_ORDER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,    99,   195,    14,    25, STR_SORT_BY_POPULATION, STR_SORT_ORDER_TIP},
{      WWT_PANEL, RESIZE_BOTTOM,    13,     0,   195,    26,   189, 0x0,                    STR_200A_TOWN_NAMES_CLICK_ON_NAME},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    13,   196,   207,    14,   189, 0x0,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    13,     0,   195,   190,   201, 0x0,                    STR_NULL},
{  WWT_RESIZEBOX,     RESIZE_TB,    13,   196,   207,   190,   201, 0x0,                    STR_RESIZE_BUTTON},
{   WIDGETS_END},
};


/* used to get a sorted list of the towns */
static uint _num_town_sort;

static char _bufcache[64];
static const Town* _last_town;

static int CDECL TownNameSorter(const void *a, const void *b)
{
	const Town* ta = *(const Town**)a;
	const Town* tb = *(const Town**)b;
	char buf1[64];
	int r;

	SetDParam(0, ta->index);
	GetString(buf1, STR_TOWN, lastof(buf1));

	/* If 'b' is the same town as in the last round, use the cached value
	 *  We do this to speed stuff up ('b' is called with the same value a lot of
	 *  times after eachother) */
	if (tb != _last_town) {
		_last_town = tb;
		SetDParam(0, tb->index);
		GetString(_bufcache, STR_TOWN, lastof(_bufcache));
	}

	r = strcmp(buf1, _bufcache);
	if (_town_sort_order & 1) r = -r;
	return r;
}

static int CDECL TownPopSorter(const void *a, const void *b)
{
	const Town* ta = *(const Town**)a;
	const Town* tb = *(const Town**)b;
	int r = ta->population - tb->population;
	if (_town_sort_order & 1) r = -r;
	return r;
}

static void MakeSortedTownList()
{
	const Town* t;
	uint n = 0;

	/* Create array for sorting */
	_town_sort = ReallocT(_town_sort, GetMaxTownIndex() + 1);

	FOR_ALL_TOWNS(t) _town_sort[n++] = t;

	_num_town_sort = n;

	_last_town = NULL; // used for "cache"
	qsort((void*)_town_sort, n, sizeof(_town_sort[0]), _town_sort_order & 2 ? TownPopSorter : TownNameSorter);

	DEBUG(misc, 3, "Resorting towns list");
}


struct TownDirectoryWindow : public Window {
private:
	enum TownDirectoryWidget {
		TDW_SORTNAME = 3,
		TDW_SORTPOPULATION,
		TDW_CENTERTOWN,
	};

public:
	TownDirectoryWindow(const WindowDesc *desc) : Window(desc, 0)
	{
		this->vscroll.cap = 16;
		this->resize.step_height = 10;
		this->resize.height = this->height - 10 * 6; // minimum of 10 items in the list, each item 10 high

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		if (_town_sort_dirty) {
			_town_sort_dirty = false;
			MakeSortedTownList();
		}

		SetVScrollCount(this, _num_town_sort);

		this->DrawWidgets();
		this->DrawSortButtonState((_town_sort_order <= 1) ? TDW_SORTNAME : TDW_SORTPOPULATION, _town_sort_order & 1 ? SBS_DOWN : SBS_UP);

		{
			int n = 0;
			uint16 i = this->vscroll.pos;
			int y = 28;

			while (i < _num_town_sort) {
				const Town* t = _town_sort[i];

				assert(t->xy);

				SetDParam(0, t->index);
				SetDParam(1, t->population);
				DrawString(2, y, STR_2057, TC_FROMSTRING);

				y += 10;
				i++;
				if (++n == this->vscroll.cap) break; // max number of towns in 1 window
			}

			SetDParam(0, GetWorldPopulation());
			DrawString(3, this->height - 12 + 2, STR_TOWN_POPULATION, TC_FROMSTRING);
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case TDW_SORTNAME: /* Sort by Name ascending/descending */
				_town_sort_order = (_town_sort_order == 0) ? 1 : 0;
				_town_sort_dirty = true;
				this->SetDirty();
				break;

			case TDW_SORTPOPULATION: /* Sort by Population ascending/descending */
				_town_sort_order = (_town_sort_order == 2) ? 3 : 2;
				_town_sort_dirty = true;
				this->SetDirty();
				break;

			case TDW_CENTERTOWN: { /* Click on Town Matrix */
				const Town* t;

				uint16 id_v = (pt.y - 28) / 10;

				if (id_v >= this->vscroll.cap) return; // click out of bounds

				id_v += this->vscroll.pos;

				if (id_v >= _num_town_sort) return; // click out of town bounds

				t = _town_sort[id_v];
				assert(t->xy);
				if (_ctrl_pressed) {
					ShowExtraViewPortWindow(t->xy);
				} else {
					ScrollMainWindowToTile(t->xy);
				}
				break;
			}
		}
	}

	virtual void OnHundredthTick()
	{
		this->SetDirty();
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		this->vscroll.cap += delta.y / 10;
	}
};

static const WindowDesc _town_directory_desc = {
	WDP_AUTO, WDP_AUTO, 208, 202, 208, 202,
	WC_TOWN_DIRECTORY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_town_directory_widgets,
};

void ShowTownDirectory()
{
	new TownDirectoryWindow(&_town_directory_desc);
}

void CcBuildTown(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(SND_1F_SPLAT, tile);
		ResetObjectToPlace();
	}
}

static void PlaceProc_Town(TileIndex tile)
{
	uint32 size = min(_scengen_town_size, (int)TSM_CITY);
	uint32 mode = _scengen_town_size > TSM_CITY ? TSM_CITY : TSM_FIXED;
	DoCommandP(tile, size, mode, CcBuildTown, CMD_BUILD_TOWN | CMD_MSG(STR_0236_CAN_T_BUILD_TOWN_HERE));
}

static const Widget _scen_edit_town_gen_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                 STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   147,     0,    13, STR_0233_TOWN_GENERATION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,     7,   148,   159,     0,    13, 0x0,                      STR_STICKY_BUTTON},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   159,    14,    94, 0x0,                      STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   157,    16,    27, STR_0234_NEW_TOWN,        STR_0235_CONSTRUCT_NEW_TOWN},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   157,    29,    40, STR_023D_RANDOM_TOWN,     STR_023E_BUILD_TOWN_IN_RANDOM_LOCATION},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   157,    42,    53, STR_MANY_RANDOM_TOWNS,    STR_RANDOM_TOWNS_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,    53,    68,    79, STR_02A1_SMALL,           STR_02A4_SELECT_TOWN_SIZE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    54,   105,    68,    79, STR_02A2_MEDIUM,          STR_02A4_SELECT_TOWN_SIZE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   106,   157,    68,    79, STR_02A3_LARGE,           STR_02A4_SELECT_TOWN_SIZE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   157,    81,    92, STR_SCENARIO_EDITOR_CITY, STR_02A4_SELECT_TOWN_SIZE},
{      WWT_LABEL,   RESIZE_NONE,     7,     0,   147,    54,    67, STR_02A5_TOWN_SIZE,       STR_NULL},
{   WIDGETS_END},
};

struct ScenarioEditorTownGenerationWindow : Window
{
private:
	enum TownScenarioEditorWidget {
		TSEW_NEWTOWN = 4,
		TSEW_RANDOMTOWN,
		TSEW_MANYRANDOMTOWNS,
		TSEW_SMALLTOWN,
		TSEW_MEDIUMTOWN,
		TSEW_LARGETOWN,
		TSEW_CITY,
	};

public:

	ScenarioEditorTownGenerationWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->LowerWidget(_scengen_town_size + TSEW_SMALLTOWN);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case TSEW_NEWTOWN:
				HandlePlacePushButton(this, TSEW_NEWTOWN, SPR_CURSOR_TOWN, VHM_RECT, PlaceProc_Town);
				break;

			case TSEW_RANDOMTOWN: {
				Town *t;
				uint size = min(_scengen_town_size, (int)TSM_CITY);
				TownSizeMode mode = _scengen_town_size > TSM_CITY ? TSM_CITY : TSM_FIXED;

				this->HandleButtonClick(TSEW_RANDOMTOWN);
				_generating_world = true;
				t = CreateRandomTown(20, mode, size);
				_generating_world = false;

				if (t == NULL) {
					ShowErrorMessage(STR_NO_SPACE_FOR_TOWN, STR_CANNOT_GENERATE_TOWN, 0, 0);
				} else {
					ScrollMainWindowToTile(t->xy);
				}
			} break;

			case TSEW_MANYRANDOMTOWNS:
				this->HandleButtonClick(TSEW_MANYRANDOMTOWNS);

				_generating_world = true;
				if (!GenerateTowns()) ShowErrorMessage(STR_NO_SPACE_FOR_TOWN, STR_CANNOT_GENERATE_TOWN, 0, 0);
				_generating_world = false;
				break;

			case TSEW_SMALLTOWN: case TSEW_MEDIUMTOWN: case TSEW_LARGETOWN: case TSEW_CITY:
				this->RaiseWidget(_scengen_town_size + TSEW_SMALLTOWN);
				_scengen_town_size = widget - TSEW_SMALLTOWN;
				this->LowerWidget(_scengen_town_size + TSEW_SMALLTOWN);
				this->SetDirty();
				break;
		}
	}

	virtual void OnTimeout()
	{
		this->RaiseWidget(TSEW_RANDOMTOWN);
		this->RaiseWidget(TSEW_MANYRANDOMTOWNS);
		this->SetDirty();
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		_place_proc(tile);
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseButtons();
		this->LowerWidget(_scengen_town_size + TSEW_SMALLTOWN);
		this->SetDirty();
	}
};

static const WindowDesc _scen_edit_town_gen_desc = {
	WDP_AUTO, WDP_AUTO, 160, 95, 160, 95,
	WC_SCEN_TOWN_GEN, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_scen_edit_town_gen_widgets,
};

void ShowBuildTownWindow()
{
	if (_game_mode != GM_EDITOR && !IsValidPlayer(_current_player)) return;
	AllocateWindowDescFront<ScenarioEditorTownGenerationWindow>(&_scen_edit_town_gen_desc, 0);
}

