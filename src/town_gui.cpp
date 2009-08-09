/* $Id$ */

/** @file town_gui.cpp GUI for towns. */

#include "stdafx.h"
#include "openttd.h"
#include "town.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "company_func.h"
#include "company_base.h"
#include "company_gui.h"
#include "network/network.h"
#include "variables.h"
#include "strings_func.h"
#include "sound_func.h"
#include "economy_func.h"
#include "tilehighlight_func.h"
#include "sortlist_type.h"
#include "road_cmd.h"
#include "landscape_type.h"
#include "landscape.h"
#include "cargotype.h"
#include "tile_map.h"

#include "table/sprites.h"
#include "table/strings.h"

typedef GUIList<const Town*> GUITownList;

/** Widget numbers of the town authority window. */
enum TownAuthorityWidgets {
	TWA_CLOSEBOX,
	TWA_CAPTION,
	TWA_RATING_INFO,
	TWA_COMMAND_LIST,
	TWA_SCROLLBAR,
	TWA_ACTION_INFO,
	TWA_EXECUTE,
};

static const Widget _town_authority_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_BROWN,     0,    10,     0,    13, STR_BLACK_CROSS,             STR_TOOLTIP_CLOSE_WINDOW},               // TWA_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_BROWN,    11,   316,     0,    13, STR_LOCAL_AUTHORITY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},     // TWA_CAPTION
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_BROWN,     0,   316,    14,   105, 0x0,                         STR_NULL},                               // TWA_RATING_INFO
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_BROWN,     0,   304,   106,   157, 0x0,                         STR_LOCAL_AUTHORITY_ACTIONS_TOOLTIP},    // TWA_COMMAND_LIST
{  WWT_SCROLLBAR,   RESIZE_NONE,  COLOUR_BROWN,   305,   316,   106,   157, 0x0,                         STR_TOOLTIP_VSCROLL_BAR_SCROLLS_LIST},   // TWA_SCROLLBAR
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_BROWN,     0,   316,   158,   209, 0x0,                         STR_NULL},                               // TWA_ACTION_INFO
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_BROWN,     0,   316,   210,   221, STR_LOCAL_AUTHORITY_DO_IT_BUTTON, STR_LOCAL_AUTHORITY_DO_IT_TOOLTIP}, // TWA_EXECUTE
{   WIDGETS_END},
};

static const NWidgetPart _nested_town_authority_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN, TWA_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_BROWN, TWA_CAPTION), SetDataTip(STR_LOCAL_AUTHORITY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, TWA_RATING_INFO), SetMinimalSize(317, 92), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN, TWA_COMMAND_LIST), SetMinimalSize(305, 52), SetDataTip(0x0, STR_LOCAL_AUTHORITY_ACTIONS_TOOLTIP), EndContainer(),
		NWidget(WWT_SCROLLBAR, COLOUR_BROWN, TWA_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, TWA_ACTION_INFO), SetMinimalSize(317, 52), EndContainer(),
	NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TWA_EXECUTE),  SetMinimalSize(317, 12), SetDataTip(STR_LOCAL_AUTHORITY_DO_IT_BUTTON, STR_LOCAL_AUTHORITY_DO_IT_TOOLTIP),
};

extern const byte _town_action_costs[8];

struct TownAuthorityWindow : Window {
private:
	Town *town;
	int sel_index;

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

public:
	TownAuthorityWindow(const WindowDesc *desc, WindowNumber window_number) :
			Window(desc, window_number), sel_index(-1)
	{
		this->town = Town::Get(this->window_number);
		this->vscroll.cap = 5;

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		int numact;
		uint buttons = GetMaskOfTownActions(&numact, _local_company, this->town);

		SetVScrollCount(this, numact + 1);

		if (this->sel_index != -1 && !HasBit(buttons, this->sel_index)) {
			this->sel_index = -1;
		}

		this->SetWidgetDisabledState(6, this->sel_index == -1);

		SetDParam(0, this->window_number);
		this->DrawWidgets();

		int y = this->widget[TWA_RATING_INFO].top + 1;

		DrawString(this->widget[TWA_RATING_INFO].left + 2, this->widget[TWA_RATING_INFO].right - 2, y, STR_LOCAL_AUTHORITY_COMPANY_RATINGS);
		y += 10;

		/* Draw list of companies */
		const Company *c;
		FOR_ALL_COMPANIES(c) {
			if ((HasBit(this->town->have_ratings, c->index) || this->town->exclusivity == c->index)) {
				DrawCompanyIcon(c->index, 2, y);

				SetDParam(0, c->index);
				SetDParam(1, c->index);

				int r = this->town->ratings[c->index];
				StringID str;
				(str = STR_CARGO_RATING_APPALLING, r <= RATING_APPALLING) || // Apalling
				(str++,                    r <= RATING_VERYPOOR)  || // Very Poor
				(str++,                    r <= RATING_POOR)      || // Poor
				(str++,                    r <= RATING_MEDIOCRE)  || // Mediocore
				(str++,                    r <= RATING_GOOD)      || // Good
				(str++,                    r <= RATING_VERYGOOD)  || // Very Good
				(str++,                    r <= RATING_EXCELLENT) || // Excellent
				(str++,                    true);                    // Outstanding

				SetDParam(2, str);
				if (this->town->exclusivity == c->index) { // red icon for company with exclusive rights
					DrawSprite(SPR_BLOT, PALETTE_TO_RED, 18, y);
				}

				DrawString(this->widget[TWA_RATING_INFO].left + 28, this->widget[TWA_RATING_INFO].right - 2, y, STR_LOCAL_AUTHORITY_COMPANY_RATING);
				y += 10;
			}
		}

		if (y > this->widget[TWA_RATING_INFO].bottom) {
			/* If the company list is too big to fit, mark ourself dirty and draw again. */
			ResizeWindowForWidget(this, TWA_RATING_INFO, 0, y - this->widget[TWA_RATING_INFO].bottom);
			this->SetDirty();
			return;
		}

		y = this->widget[TWA_COMMAND_LIST].top + 1;
		int pos = this->vscroll.pos;

		if (--pos < 0) {
			DrawString(this->widget[TWA_COMMAND_LIST].left + 2, this->widget[TWA_COMMAND_LIST].right - 2, y, STR_LOCAL_AUTHORITY_ACTIONS_TITLE);
			y += 10;
		}

		for (int i = 0; buttons; i++, buttons >>= 1) {
			if (pos <= -5) break; ///< Draw only the 5 fitting lines

			if ((buttons & 1) && --pos < 0) {
				DrawString(this->widget[TWA_COMMAND_LIST].left + 3, this->widget[TWA_COMMAND_LIST].right - 2, y, STR_LOCAL_AUTHORITY_ACTION_SMALL_ADVERTISING_CAMPAIGN + i, TC_ORANGE);
				y += 10;
			}
		}

		if (this->sel_index != -1) {
			SetDParam(1, (_price.build_industry >> 8) * _town_action_costs[this->sel_index]);
			SetDParam(0, STR_LOCAL_AUTHORITY_ACTION_SMALL_ADVERTISING_CAMPAIGN + this->sel_index);
			DrawStringMultiLine(this->widget[TWA_ACTION_INFO].left + 2, this->widget[TWA_ACTION_INFO].right - 2, this->widget[TWA_ACTION_INFO].top + 1, this->widget[TWA_ACTION_INFO].bottom - 1, STR_LOCAL_AUTHORITY_ACTION_TOOLTIP_SMALL_ADVERTISING + this->sel_index);
		}
	}

	virtual void OnDoubleClick(Point pt, int widget) { HandleClick(pt, widget, true); }
	virtual void OnClick(Point pt, int widget) { HandleClick(pt, widget, false); }

	void HandleClick(Point pt, int widget, bool double_click)
	{
		switch (widget) {
			case TWA_COMMAND_LIST: {
				int y = (pt.y - this->widget[TWA_COMMAND_LIST].top - 1) / 10;

				if (!IsInsideMM(y, 0, 5)) return;

				y = GetNthSetBit(GetMaskOfTownActions(NULL, _local_company, this->town), y + this->vscroll.pos - 1);
				if (y >= 0) {
					this->sel_index = y;
					this->SetDirty();
				}
				/* Fall through to clicking in case we are double-clicked */
				if (!double_click || y < 0) break;
			}

			case TWA_EXECUTE:
				DoCommandP(this->town->xy, this->window_number, this->sel_index, CMD_DO_TOWN_ACTION | CMD_MSG(STR_ERROR_CAN_T_DO_THIS));
				break;
		}
	}

	virtual void OnHundredthTick()
	{
		this->SetDirty();
	}
};

static const WindowDesc _town_authority_desc(
	WDP_AUTO, WDP_AUTO, 317, 222, 317, 222,
	WC_TOWN_AUTHORITY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_town_authority_widgets, _nested_town_authority_widgets, lengthof(_nested_town_authority_widgets)
);

static void ShowTownAuthorityWindow(uint town)
{
	AllocateWindowDescFront<TownAuthorityWindow>(&_town_authority_desc, town);
}

/** Widget numbers of the town view window. */
enum TownViewWidgets {
	TVW_CLOSEBOX,
	TVW_CAPTION,
	TVW_STICKY,
	TVW_VIEWPORTPANEL,
	TVW_VIEWPORTINSET,
	TVW_INFOPANEL,
	TVW_CENTERVIEW,
	TVW_SHOWAUTHORITY,
	TVW_CHANGENAME,
	TVW_EXPAND,
	TVW_DELETE,
};

struct TownViewWindow : Window {
private:
	Town *town;

public:
	enum {
		TVW_HEIGHT_NORMAL = 150,
	};

	TownViewWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->town = Town::Get(this->window_number);
		bool ingame = _game_mode != GM_EDITOR;

		this->flags4 |= WF_DISABLE_VP_SCROLL;
		int width = this->widget[TVW_VIEWPORTINSET].right - this->widget[TVW_VIEWPORTINSET].left - 1;
		int height = this->widget[TVW_VIEWPORTINSET].bottom - this->widget[TVW_VIEWPORTINSET].top - 1;
		InitializeWindowViewport(this, this->widget[TVW_VIEWPORTINSET].left + 1, this->widget[TVW_VIEWPORTINSET].top + 1, width, height, this->town->xy, ZOOM_LVL_TOWN);

		if (this->town->larger_town) this->widget[TVW_CAPTION].data = STR_TOWN_VIEW_CITY_CAPTION;

		if (ingame) {
			/* Hide the expand button, and put the authority button over it. */
			this->HideWidget(TVW_EXPAND);
			this->widget[TVW_SHOWAUTHORITY].right = this->widget[TVW_EXPAND].right;
			/* Resize caption bar */
			this->widget[TVW_CAPTION].right = this->widget[TVW_STICKY].left -1;
			/* Hide the delete button, and move the rename button from top on scenario to bottom in game. */
			this->HideWidget(TVW_DELETE);
			this->widget[TVW_CHANGENAME].top = this->widget[TVW_EXPAND].top;
			this->widget[TVW_CHANGENAME].bottom = this->widget[TVW_EXPAND].bottom;
			this->widget[TVW_CHANGENAME].right = this->widget[TVW_STICKY].right;
		} else {
			/* Hide the authority button, and put the expand button over it. */
			this->HideWidget(TVW_SHOWAUTHORITY);
			this->widget[TVW_EXPAND].left = this->widget[TVW_SHOWAUTHORITY].left;
		}

		this->ResizeWindowAsNeeded();

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		/* disable renaming town in network games if you are not the server */
		this->SetWidgetDisabledState(TVW_CHANGENAME, _networking && !_network_server);

		SetDParam(0, this->town->index);
		this->DrawWidgets();

		uint y = 107;

		SetDParam(0, this->town->population);
		SetDParam(1, this->town->num_houses);
		DrawString(2, this->width - 2, y, STR_TOWN_VIEW_POPULATION_HOUSES);

		SetDParam(0, this->town->act_pass);
		SetDParam(1, this->town->max_pass);
		DrawString(2, this->width - 2, y += 10, STR_TOWN_VIEW_PASSENGERS_LAST_MONTH_MAX);

		SetDParam(0, this->town->act_mail);
		SetDParam(1, this->town->max_mail);
		DrawString(2, this->width - 2, y += 10, STR_TOWN_VIEW_MAIL_LAST_MONTH_MAX);

		uint cargo_needed_for_growth = 0;
		switch (_settings_game.game_creation.landscape) {
			case LT_ARCTIC:
				if (TilePixelHeight(this->town->xy) >= LowestSnowLine()) cargo_needed_for_growth = 1;
				break;

			case LT_TROPIC:
				if (GetTropicZone(this->town->xy) == TROPICZONE_DESERT) cargo_needed_for_growth = 2;
				break;

			default: break;
		}

		if (cargo_needed_for_growth > 0) {
			DrawString(2, this->width - 2, y += 10, STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH);

			CargoID first_food_cargo = CT_INVALID;
			StringID food_name = STR_CARGO_PLURAL_FOOD;
			CargoID first_water_cargo = CT_INVALID;
			StringID water_name = STR_CARGO_PLURAL_WATER;
			for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
				const CargoSpec *cs = CargoSpec::Get(cid);
				if (first_food_cargo == CT_INVALID && cs->town_effect == TE_FOOD) {
					first_food_cargo = cid;
					food_name = cs->name;
				}
				if (first_water_cargo == CT_INVALID && cs->town_effect == TE_WATER) {
					first_water_cargo = cid;
					water_name = cs->name;
				}
			}

			if (first_food_cargo != CT_INVALID && this->town->act_food > 0) {
				SetDParam(0, first_food_cargo);
				SetDParam(1, this->town->act_food);
				DrawString(2, this->width - 2, y += 10, STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_LAST_MONTH);
			} else {
				SetDParam(0, food_name);
				DrawString(2, this->width - 2, y += 10, STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_REQUIRED);
			}

			if (cargo_needed_for_growth > 1) {
				if (first_water_cargo != CT_INVALID && this->town->act_water > 0) {
					SetDParam(0, first_water_cargo);
					SetDParam(1, this->town->act_water);
					DrawString(2, this->width - 2, y += 10, STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_LAST_MONTH);
				} else {
					SetDParam(0, water_name);
					DrawString(2, this->width - 2, y += 10, STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH_REQUIRED);
				}
			}
		}

		this->DrawViewport();

		/* only show the town noise, if the noise option is activated. */
		if (_settings_game.economy.station_noise_level) {
			SetDParam(0, this->town->noise_reached);
			SetDParam(1, this->town->MaxTownNoise());
			DrawString(2, this->width - 2, y += 10, STR_TOWN_VIEW_NOISE_IN_TOWN);
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case TVW_CENTERVIEW: // scroll to location
				if (_ctrl_pressed) {
					ShowExtraViewPortWindow(this->town->xy);
				} else {
					ScrollMainWindowToTile(this->town->xy);
				}
				break;

			case TVW_SHOWAUTHORITY: // town authority
				ShowTownAuthorityWindow(this->window_number);
				break;

			case TVW_CHANGENAME: // rename
				SetDParam(0, this->window_number);
				ShowQueryString(STR_TOWN_NAME, STR_TOWN_VIEW_RENAME_TOWN_BUTTON, MAX_LENGTH_TOWN_NAME_BYTES, MAX_LENGTH_TOWN_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT);
				break;

			case TVW_EXPAND: // expand town - only available on Scenario editor
				ExpandTown(this->town);
				break;

			case TVW_DELETE: // delete town - only available on Scenario editor
				delete this->town;
				break;
		}
	}

	void ResizeWindowAsNeeded()
	{
		int aimed_height = TVW_HEIGHT_NORMAL;

		switch (_settings_game.game_creation.landscape) {
			case LT_ARCTIC:
				if (TilePixelHeight(this->town->xy) >= LowestSnowLine()) aimed_height += 20;
				break;

			case LT_TROPIC:
				if (GetTropicZone(this->town->xy) == TROPICZONE_DESERT) aimed_height += 30;
				break;

			default: break;
		}

		if (_settings_game.economy.station_noise_level) aimed_height += 10;

		if (this->height != aimed_height) ResizeWindowForWidget(this, TVW_INFOPANEL, 0, aimed_height - this->height);
	}

	virtual void OnInvalidateData(int data = 0)
	{
		/* Called when setting station noise have changed, in order to resize the window */
		this->SetDirty(); // refresh display for current size. This will allow to avoid glitches when downgrading
		this->ResizeWindowAsNeeded();
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		DoCommandP(0, this->window_number, 0, CMD_RENAME_TOWN | CMD_MSG(STR_ERROR_CAN_T_RENAME_TOWN), NULL, str);
	}
};


static const Widget _town_view_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_BROWN,     0,    10,     0,    13, STR_BLACK_CROSS,                      STR_TOOLTIP_CLOSE_WINDOW},              // TVW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_BROWN,    11,   171,     0,    13, STR_TOWN_VIEW_TOWN_CAPTION,           STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},    // TVW_CAPTION
{  WWT_STICKYBOX,   RESIZE_NONE,  COLOUR_BROWN,   248,   259,     0,    13, 0x0,                                  STR_TOOLTIP_STICKY},                     // TVW_STICKY
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_BROWN,     0,   259,    14,   105, 0x0,                                  STR_NULL},                              // TVW_VIEWPORTPANEL
{      WWT_INSET,   RESIZE_NONE,  COLOUR_BROWN,     2,   257,    16,   103, 0x0,                                  STR_NULL},                              // TVW_VIEWPORTINSET
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_BROWN,     0,   259,   106,   137, 0x0,                                  STR_NULL},                              // TVW_INFOPANEL
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_BROWN,     0,    85,   138,   149, STR_BUTTON_LOCATION,                  STR_TOWN_VIEW_CENTER_TOOLTIP},          // TVW_CENTERVIEW
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_BROWN,    86,   127,   138,   149, STR_TOWN_VIEW_LOCAL_AUTHORITY_BUTTON, STR_TOWN_VIEW_LOCAL_AUTHORITY_TOOLTIP}, // TVW_SHOWAUTHORITY
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_BROWN,   172,   247,     0,    13, STR_BUTTON_RENAME,                    STR_TOWN_VIEW_RENAME_TOOLTIP},          // TVW_CHANGENAME
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_BROWN,   128,   171,   138,   149, STR_TOWN_VIEW_EXPAND_BUTTON,          STR_TOWN_VIEW_EXPAND_TOOLTIP},          // TVW_EXPAND
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_BROWN,   172,   259,   138,   149, STR_TOWN_VIEW_DELETE_BUTTON,          STR_TOWN_VIEW_DELETE_TOOLTIP},          // TVW_DELETE
{   WIDGETS_END},
};

static const NWidgetPart _nested_town_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN, TVW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_BROWN, TVW_CAPTION), SetDataTip(STR_TOWN_VIEW_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TVW_CHANGENAME), SetMinimalSize(76, 14), SetDataTip(STR_BUTTON_RENAME, STR_TOWN_VIEW_RENAME_TOOLTIP),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN, TVW_STICKY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, TVW_VIEWPORTPANEL),
		NWidget(WWT_INSET, COLOUR_BROWN, TVW_VIEWPORTINSET), SetMinimalSize(256, 88), SetPadding(2, 2, 2, 2), EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, TVW_INFOPANEL), SetMinimalSize(260, 32), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TVW_CENTERVIEW), SetMinimalSize(86, 12), SetDataTip(STR_BUTTON_LOCATION, STR_TOWN_VIEW_CENTER_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TVW_SHOWAUTHORITY), SetMinimalSize(42, 12), SetDataTip(STR_TOWN_VIEW_LOCAL_AUTHORITY_BUTTON, STR_TOWN_VIEW_LOCAL_AUTHORITY_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TVW_EXPAND), SetMinimalSize(44, 12), SetDataTip(STR_TOWN_VIEW_EXPAND_BUTTON, STR_TOWN_VIEW_EXPAND_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TVW_DELETE), SetMinimalSize(88, 12), SetDataTip(STR_TOWN_VIEW_DELETE_BUTTON, STR_TOWN_VIEW_DELETE_TOOLTIP),
	EndContainer(),
};

static const WindowDesc _town_view_desc(
	WDP_AUTO, WDP_AUTO, 260, TownViewWindow::TVW_HEIGHT_NORMAL, 260, TownViewWindow::TVW_HEIGHT_NORMAL,
	WC_TOWN_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_town_view_widgets, _nested_town_view_widgets, lengthof(_nested_town_view_widgets)
);

void ShowTownViewWindow(TownID town)
{
	AllocateWindowDescFront<TownViewWindow>(&_town_view_desc, town);
}

/** Widget numbers of town directory window. */
enum TownDirectoryWidgets {
	TDW_CLOSEBOX,
	TDW_CAPTION,
	TDW_STICKYBOX,
	TDW_SORTNAME,
	TDW_SORTPOPULATION,
	TDW_CENTERTOWN,
	TDW_SCROLLBAR,
	TDW_BOTTOM_PANEL,
	TDW_BOTTOM_TEXT,
	TDW_RESIZEBOX,
};

static const NWidgetPart _nested_town_directory_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN, TDW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_BROWN, TDW_CAPTION), SetDataTip(STR_TOWN_DIRECTORY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN, TDW_STICKYBOX),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TDW_SORTNAME), SetMinimalSize(99, 12), SetDataTip(STR_SORT_BY_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, TDW_SORTPOPULATION), SetMinimalSize(97, 12), SetDataTip(STR_SORT_BY_POPULATION, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_BROWN, TDW_CENTERTOWN), SetMinimalSize(196, 164), SetDataTip(0x0, STR_TOWN_DIRECTORY_LIST_TOOLTIP),
							SetFill(1, 0), SetResize(0, 10), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_BROWN, TDW_BOTTOM_PANEL),
				NWidget(WWT_TEXT, COLOUR_BROWN, TDW_BOTTOM_TEXT), SetPadding(2, 0, 0, 2), SetMinimalSize(196, 12), SetFill(1, 0), SetDataTip(STR_TOWN_POPULATION, STR_NULL),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_SCROLLBAR, COLOUR_BROWN, TDW_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN, TDW_RESIZEBOX),
		EndContainer(),
	EndContainer(),
};

/** Town directory window class. */
struct TownDirectoryWindow : public Window {
private:
	/* Runtime saved values */
	static Listing last_sorting;
	static const Town *last_town;
	int townline_height; ///< Height of a single town line in the town directory window.

	/* Constants for sorting towns */
	static GUITownList::SortFunction * const sorter_funcs[];

	GUITownList towns;

	void BuildSortTownList()
	{
		if (this->towns.NeedRebuild()) {
			this->towns.Clear();

			const Town *t;
			FOR_ALL_TOWNS(t) {
				*this->towns.Append() = t;
			}

			this->towns.Compact();
			this->towns.RebuildDone();
			SetVScrollCount(this, this->towns.Length()); // Update scrollbar as well.
		}
		/* Always sort the towns. */
		last_town = NULL;
		this->towns.Sort();
	}

	/** Sort by town name */
	static int CDECL TownNameSorter(const Town * const *a, const Town * const *b)
	{
		static char buf_cache[64];
		const Town *ta = *a;
		const Town *tb = *b;
		char buf[64];

		SetDParam(0, ta->index);
		GetString(buf, STR_TOWN_NAME, lastof(buf));

		/* If 'b' is the same town as in the last round, use the cached value
		 * We do this to speed stuff up ('b' is called with the same value a lot of
		 * times after eachother) */
		if (tb != last_town) {
			last_town = tb;
			SetDParam(0, tb->index);
			GetString(buf_cache, STR_TOWN_NAME, lastof(buf_cache));
		}

		return strcmp(buf, buf_cache);
	}

	/** Sort by population */
	static int CDECL TownPopulationSorter(const Town * const *a, const Town * const *b)
	{
		return (*a)->population - (*b)->population;
	}

public:
	TownDirectoryWindow(const WindowDesc *desc) : Window()
	{
		this->towns.SetListing(this->last_sorting);
		this->towns.SetSortFuncs(this->sorter_funcs);
		this->towns.ForceRebuild();
		this->BuildSortTownList();

		this->townline_height = FONT_HEIGHT_NORMAL;
		this->InitNested(desc, 0);
		this->vscroll.cap = this->nested_array[TDW_CENTERTOWN]->current_y / this->resize.step_height;
	}

	~TownDirectoryWindow()
	{
		this->last_sorting = this->towns.GetListing();
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == TDW_BOTTOM_TEXT) SetDParam(0, GetWorldPopulation());
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case TDW_SORTNAME:
				if (this->towns.SortType() == 0) this->DrawSortButtonState(widget, this->towns.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;

			case TDW_SORTPOPULATION:
				if (this->towns.SortType() != 0) this->DrawSortButtonState(widget, this->towns.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;

			case TDW_CENTERTOWN: {
				int n = 0;
				int y = r.top + 2;
				for (uint i = this->vscroll.pos; i < this->towns.Length(); i++) {
					const Town *t = this->towns[i];

					assert(t->xy != INVALID_TILE);

					SetDParam(0, t->index);
					SetDParam(1, t->population);
					DrawString(r.left + 2, r.right - 2, y, STR_TOWN_DIRECTORY_TOWN);

					y += this->townline_height;
					if (++n == this->vscroll.cap) break; // max number of towns in 1 window
				}
			} break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		switch (widget) {
			case TDW_SORTNAME:
			case TDW_SORTPOPULATION: {
				Dimension d = GetStringBoundingBox(this->nested_array[widget]->widget_data);
				d.width += padding.width + WD_SORTBUTTON_ARROW_WIDTH * 2; // Doubled since the word is centered, also looks nice.
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
			case TDW_CENTERTOWN: {
				Dimension d = {0, 0};
				for (uint i = 0; i < this->towns.Length(); i++) {
					const Town *t = this->towns[i];

					assert(t != NULL);

					SetDParam(0, t->index);
					SetDParam(1, 10000000); // 10^7
					d = maxdim(d, GetStringBoundingBox(STR_TOWN_DIRECTORY_TOWN));
				}
				d.width += padding.width + 2 + 2; // Text is rendered with 2 pixel offset at both sides.
				d.height += padding.height;
				*size = maxdim(*size, d);
				resize->height = d.height;
				break;
			}
			case TDW_BOTTOM_TEXT: {
				SetDParam(0, 1000000000); // 10^9
				Dimension d = GetStringBoundingBox(STR_TOWN_POPULATION);
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case TDW_SORTNAME: // Sort by Name ascending/descending
				if (this->towns.SortType() == 0) {
					this->towns.ToggleSortOrder();
				} else {
					this->towns.SetSortType(0);
				}
				this->BuildSortTownList();
				this->SetDirty();
				break;

			case TDW_SORTPOPULATION: // Sort by Population ascending/descending
				if (this->towns.SortType() == 1) {
					this->towns.ToggleSortOrder();
				} else {
					this->towns.SetSortType(1);
				}
				this->BuildSortTownList();
				this->SetDirty();
				break;

			case TDW_CENTERTOWN: { // Click on Town Matrix
				uint16 id_v = (pt.y - this->nested_array[widget]->pos_y - 2) / this->townline_height;

				if (id_v >= this->vscroll.cap) return; // click out of bounds

				id_v += this->vscroll.pos;

				if (id_v >= this->towns.Length()) return; // click out of town bounds

				const Town *t = this->towns[id_v];
				assert(t != NULL);
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
		this->BuildSortTownList();
		this->SetDirty();
	}

	virtual void OnResize(Point delta)
	{
		this->vscroll.cap += delta.y / this->townline_height;
	}

	virtual void OnInvalidateData(int data)
	{
		if (data == 0) {
			this->towns.ForceRebuild();
		} else {
			this->towns.ForceResort();
		}
		this->BuildSortTownList();
	}
};

Listing TownDirectoryWindow::last_sorting = {false, 0};
const Town *TownDirectoryWindow::last_town = NULL;

/* Available town directory sorting functions */
GUITownList::SortFunction * const TownDirectoryWindow::sorter_funcs[] = {
	&TownNameSorter,
	&TownPopulationSorter,
};

static const WindowDesc _town_directory_desc(
	WDP_AUTO, WDP_AUTO, 208, 202, 208, 202,
	WC_TOWN_DIRECTORY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	NULL, _nested_town_directory_widgets, lengthof(_nested_town_directory_widgets)
);

void ShowTownDirectory()
{
	if (BringWindowToFrontById(WC_TOWN_DIRECTORY, 0)) return;
	new TownDirectoryWindow(&_town_directory_desc);
}

void CcBuildTown(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(SND_1F_SPLAT, tile);
		if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
	}
}

/** Widget numbers of town scenario editor window. */
enum TownScenarioEditorWidgets {
	TSEW_CLOSEBOX,
	TSEW_CAPTION,
	TSEW_STICKYBOX,
	TSEW_BACKGROUND,
	TSEW_NEWTOWN,
	TSEW_RANDOMTOWN,
	TSEW_MANYRANDOMTOWNS,
	TSEW_TOWNSIZE,
	TSEW_SIZE_SMALL,
	TSEW_SIZE_MEDIUM,
	TSEW_SIZE_LARGE,
	TSEW_SIZE_RANDOM,
	TSEW_CITY,
	TSEW_TOWNLAYOUT,
	TSEW_LAYOUT_ORIGINAL,
	TSEW_LAYOUT_BETTER,
	TSEW_LAYOUT_GRID2,
	TSEW_LAYOUT_GRID3,
	TSEW_LAYOUT_RANDOM,
};

static const NWidgetPart _nested_found_town_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, TSEW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, TSEW_CAPTION), SetDataTip(STR_FOUND_TOWN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN, TSEW_STICKYBOX),
	EndContainer(),
	/* Construct new town(s) buttons. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, TSEW_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 2),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_NEWTOWN), SetMinimalSize(156, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_NEW_TOWN_BUTTON, STR_FOUND_TOWN_NEW_TOWN_TOOLTIP), SetPadding(0, 2, 1, 2),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_RANDOMTOWN), SetMinimalSize(156, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_RANDOM_TOWN_BUTTON, STR_FOUND_TOWN_RANDOM_TOWN_TOOLTIP), SetPadding(0, 2, 1, 2),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_MANYRANDOMTOWNS), SetMinimalSize(156, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_MANY_RANDOM_TOWNS, STR_FOUND_TOWN_RANDOM_TOWNS_TOOLTIP), SetPadding(0, 2, 0, 2),
		/* Town size selection. */
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_LABEL, COLOUR_DARK_GREEN, TSEW_TOWNSIZE), SetMinimalSize(148, 14), SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_TITLE, STR_NULL),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(2, 0, 2),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_SIZE_SMALL), SetMinimalSize(78, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_SMALL_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_SIZE_MEDIUM), SetMinimalSize(78, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_MEDIUM_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 1),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(2, 0, 2),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_SIZE_LARGE), SetMinimalSize(78, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_INITIAL_SIZE_LARGE_BUTTON, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_SIZE_RANDOM), SetMinimalSize(78, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_SIZE_RANDOM, STR_FOUND_TOWN_INITIAL_SIZE_TOOLTIP),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_CITY), SetPadding(0, 2, 0, 2), SetMinimalSize(156, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_CITY, STR_FOUND_TOWN_CITY_TOOLTIP), SetFill(1, 0),
		/* Town roads selection. */
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_LABEL, COLOUR_DARK_GREEN, TSEW_TOWNLAYOUT), SetMinimalSize(148, 14), SetDataTip(STR_FOUND_TOWN_ROAD_LAYOUT, STR_NULL),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(2, 0, 2),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_LAYOUT_ORIGINAL), SetMinimalSize(78, 12), SetFill(1, 0), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_ORIGINAL, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_LAYOUT_BETTER), SetMinimalSize(78, 12), SetFill(1, 0), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_BETTER_ROADS, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 1),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(2, 0, 2),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_LAYOUT_GRID2), SetMinimalSize(78, 12), SetFill(1, 0), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_2X2_GRID, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_LAYOUT_GRID3), SetMinimalSize(78, 12), SetFill(1, 0), SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_3X3_GRID, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 1),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, TSEW_LAYOUT_RANDOM), SetPadding(0, 2, 0, 2), SetMinimalSize(0, 12), SetFill(1, 0),
										SetDataTip(STR_FOUND_TOWN_SELECT_LAYOUT_RANDOM, STR_FOUND_TOWN_SELECT_TOWN_ROAD_LAYOUT), SetFill(1, 0),
		NWidget(NWID_SPACER), SetMinimalSize(0, 2),
	EndContainer(),
};

/** Found a town window class. */
struct FoundTownWindow : Window
{
private:
	static TownSize town_size;
	static bool city;
	static TownLayout town_layout;

public:
	FoundTownWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);
		town_layout = _settings_game.economy.town_layout;
		city = false;
		this->UpdateButtons();
	}

	void UpdateButtons()
	{
		for (int i = TSEW_SIZE_SMALL; i <= TSEW_SIZE_RANDOM; i++) {
			this->SetWidgetLoweredState(i, i == TSEW_SIZE_SMALL + town_size);
		}

		this->SetWidgetLoweredState(TSEW_CITY, city);

		for (int i = TSEW_LAYOUT_ORIGINAL; i <= TSEW_LAYOUT_RANDOM; i++) {
			this->SetWidgetLoweredState(i, i == TSEW_LAYOUT_ORIGINAL + town_layout);
		}

		this->SetDirty();
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case TSEW_NEWTOWN:
				HandlePlacePushButton(this, TSEW_NEWTOWN, SPR_CURSOR_TOWN, HT_RECT, PlaceProc_Town);
				break;

			case TSEW_RANDOMTOWN: {
				this->HandleButtonClick(TSEW_RANDOMTOWN);
				_generating_world = true;
				UpdateNearestTownForRoadTiles(true);
				const Town *t = CreateRandomTown(20, town_size, city, town_layout);
				UpdateNearestTownForRoadTiles(false);
				_generating_world = false;

				if (t == NULL) {
					ShowErrorMessage(STR_ERROR_NO_SPACE_FOR_TOWN, STR_ERROR_CAN_T_GENERATE_TOWN, 0, 0);
				} else {
					ScrollMainWindowToTile(t->xy);
				}
			} break;

			case TSEW_MANYRANDOMTOWNS:
				this->HandleButtonClick(TSEW_MANYRANDOMTOWNS);

				_generating_world = true;
				UpdateNearestTownForRoadTiles(true);
				if (!GenerateTowns(town_layout)) {
					ShowErrorMessage(STR_ERROR_NO_SPACE_FOR_TOWN, STR_ERROR_CAN_T_GENERATE_TOWN, 0, 0);
				}
				UpdateNearestTownForRoadTiles(false);
				_generating_world = false;
				break;

			case TSEW_SIZE_SMALL: case TSEW_SIZE_MEDIUM: case TSEW_SIZE_LARGE: case TSEW_SIZE_RANDOM:
				town_size = (TownSize)(widget - TSEW_SIZE_SMALL);
				this->UpdateButtons();
				break;

			case TSEW_CITY:
				city ^= true;
				this->SetWidgetLoweredState(TSEW_CITY, city);
				this->SetDirty();
				break;

			case TSEW_LAYOUT_ORIGINAL: case TSEW_LAYOUT_BETTER: case TSEW_LAYOUT_GRID2:
			case TSEW_LAYOUT_GRID3: case TSEW_LAYOUT_RANDOM:
				town_layout = (TownLayout)(widget - TSEW_LAYOUT_ORIGINAL);
				this->UpdateButtons();
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
		this->UpdateButtons();
	}

	static void PlaceProc_Town(TileIndex tile)
	{
		uint32 townnameparts;
		if (!GenerateTownName(&townnameparts)) {
			ShowErrorMessage(STR_ERROR_TOO_MANY_TOWNS, STR_ERROR_CAN_T_BUILD_TOWN_HERE, 0, 0);
			return;
		}

		DoCommandP(tile, town_size | city << 2 | town_layout << 3, townnameparts, CMD_BUILD_TOWN | CMD_MSG(STR_ERROR_CAN_T_BUILD_TOWN_HERE), CcBuildTown);
	}
};

TownSize FoundTownWindow::town_size = TS_MEDIUM; // select medium-sized towns per default;
bool FoundTownWindow::city;
TownLayout FoundTownWindow::town_layout;

static const WindowDesc _found_town_desc(
	WDP_AUTO, WDP_AUTO, 160, 162, 160, 162,
	WC_FOUND_TOWN, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_CONSTRUCTION,
	NULL, _nested_found_town_widgets, lengthof(_nested_found_town_widgets)
);

void ShowBuildTownWindow()
{
	if (_game_mode != GM_EDITOR && !Company::IsValidID(_local_company)) return;
	AllocateWindowDescFront<FoundTownWindow>(&_found_town_desc, 0);
}
