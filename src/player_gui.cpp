/* $Id$ */

/** @file player_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "strings.h"
#include "functions.h"
#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "player.h"
#include "command.h"
#include "vehicle.h"
#include "economy.h"
#include "network/network.h"
#include "variables.h"
#include "roadveh.h"
#include "train.h"
#include "aircraft.h"
#include "date.h"
#include "newgrf.h"
#include "network/network_data.h"
#include "network/network_client.h"
#include "network/network_gui.h"
#include "player_face.h"

static void DoShowPlayerFinances(PlayerID player, bool show_small, bool show_stickied);
static void DoSelectPlayerFace(PlayerID player, bool show_big);

static void DrawPlayerEconomyStats(const Player *p, byte mode)
{
	int x, y, i, j, year;
	const Money (*tbl)[13];
	Money sum, cost;
	StringID str;

	if (!(mode & 1)) { // normal sized economics window (mode&1) is minimized status
		/* draw categories */
		DrawStringCenterUnderline(61, 15, STR_700F_EXPENDITURE_INCOME, TC_FROMSTRING);
		for (i = 0; i != 13; i++)
			DrawString(2, 27 + i * 10, STR_7011_CONSTRUCTION + i, TC_FROMSTRING);
		DrawStringRightAligned(111, 27 + 10 * 13 + 2, STR_7020_TOTAL, TC_FROMSTRING);

		/* draw the price columns */
		year = _cur_year - 2;
		j = 3;
		x = 215;
		tbl = p->yearly_expenses + 2;
		do {
			if (year >= p->inaugurated_year) {
				SetDParam(0, year);
				DrawStringRightAlignedUnderline(x, 15, STR_7010, TC_FROMSTRING);
				sum = 0;
				for (i = 0; i != 13; i++) {
					/* draw one row in the price column */
					cost = (*tbl)[i];
					if (cost != 0) {
						sum += cost;

						str = STR_701E;
						if (cost < 0) { cost = -cost; str++; }
						SetDParam(0, cost);
						DrawStringRightAligned(x, 27 + i * 10, str, TC_FROMSTRING);
					}
				}

				str = STR_701E;
				if (sum < 0) { sum = -sum; str++; }
				SetDParam(0, sum);
				DrawStringRightAligned(x, 27 + 13 * 10 + 2, str, TC_FROMSTRING);

				GfxFillRect(x - 75, 27 + 10 * 13, x, 27 + 10 * 13, 215);
				x += 95;
			}
			year++;
			tbl--;
		} while (--j != 0);

		y = 171;

		/* draw max loan aligned to loan below (y += 10) */
		SetDParam(0, _economy.max_loan);
		DrawString(202, y + 10, STR_MAX_LOAN, TC_FROMSTRING);
	} else {
		y = 15;
	}

	DrawString(2, y, STR_7026_BANK_BALANCE, TC_FROMSTRING);
	SetDParam(0, p->player_money);
	DrawStringRightAligned(182, y, STR_7028, TC_FROMSTRING);

	y += 10;

	DrawString(2, y, STR_7027_LOAN, TC_FROMSTRING);
	SetDParam(0, p->current_loan);
	DrawStringRightAligned(182, y, STR_7028, TC_FROMSTRING);

	y += 12;

	GfxFillRect(182 - 75, y - 2, 182, y - 2, 215);

	SetDParam(0, p->player_money - p->current_loan);
	DrawStringRightAligned(182, y, STR_7028, TC_FROMSTRING);
}

static const Widget _player_finances_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,               STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   379,     0,    13, STR_700E_FINANCES,      STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,   380,   394,     0,    13, SPR_LARGE_SMALL_WINDOW, STR_7075_TOGGLE_LARGE_SMALL_WINDOW},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   395,   406,     0,    13, 0x0,                    STR_STICKY_BUTTON},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   406,    14,   169, 0x0,                    STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   406,   170,   203, 0x0,                    STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,   202,   204,   215, STR_7029_BORROW,        STR_7035_INCREASE_SIZE_OF_LOAN},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   203,   406,   204,   215, STR_702A_REPAY,         STR_7036_REPAY_PART_OF_LOAN},
{   WIDGETS_END},
};

static const Widget _player_finances_small_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,               STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   253,     0,    13, STR_700E_FINANCES,      STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,   254,   267,     0,    13, SPR_LARGE_SMALL_WINDOW, STR_7075_TOGGLE_LARGE_SMALL_WINDOW},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   268,   279,     0,    13, 0x0,                    STR_STICKY_BUTTON},
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,                    STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   279,    14,    47, STR_NULL,               STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,   139,    48,    59, STR_7029_BORROW,        STR_7035_INCREASE_SIZE_OF_LOAN},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   140,   279,    48,    59, STR_702A_REPAY,         STR_7036_REPAY_PART_OF_LOAN},
{   WIDGETS_END},
};


static void PlayerFinancesWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		PlayerID player = (PlayerID)w->window_number;
		const Player *p = GetPlayer(player);

		/* Recheck the size of the window as it might need to be resized due to the local player changing */
		int new_height = ((player != _local_player) ? 0 : 12) + ((WP(w, def_d).data_1 != 0) ? 48 : 204);
		if (w->height != new_height) {
			/* Make window dirty before and after resizing */
			SetWindowDirty(w);
			w->height = new_height;
			SetWindowDirty(w);

			w->SetWidgetHiddenState(6, player != _local_player);
			w->SetWidgetHiddenState(7, player != _local_player);
		}

		/* Borrow button only shows when there is any more money to loan */
		w->SetWidgetDisabledState(6, p->current_loan == _economy.max_loan);

		/* Repay button only shows when there is any more money to repay */
		w->SetWidgetDisabledState(7, player != _local_player || p->current_loan == 0);

		SetDParam(0, p->index);
		SetDParam(1, p->index);
		SetDParam(2, LOAN_INTERVAL);
		DrawWindowWidgets(w);

		DrawPlayerEconomyStats(p, (byte)WP(w, def_d).data_1);
	} break;

	case WE_CLICK:
		switch (e->we.click.widget) {
		case 2: {/* toggle size */
			byte mode = (byte)WP(w, def_d).data_1;
			bool stickied = !!(w->flags4 & WF_STICKY);
			PlayerID player = (PlayerID)w->window_number;
			DeleteWindow(w);
			DoShowPlayerFinances(player, !HasBit(mode, 0), stickied);
		} break;

		case 6: /* increase loan */
			DoCommandP(0, 0, _ctrl_pressed, NULL, CMD_INCREASE_LOAN | CMD_MSG(STR_702C_CAN_T_BORROW_ANY_MORE_MONEY));
			break;

		case 7: /* repay loan */
			DoCommandP(0, 0, _ctrl_pressed, NULL, CMD_DECREASE_LOAN | CMD_MSG(STR_702F_CAN_T_REPAY_LOAN));
			break;
		}
		break;
	}
}

static const WindowDesc _player_finances_desc = {
	WDP_AUTO, WDP_AUTO, 407, 216, 407, 216,
	WC_FINANCES, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_player_finances_widgets,
	PlayerFinancesWndProc
};

static const WindowDesc _player_finances_small_desc = {
	WDP_AUTO, WDP_AUTO, 280, 60, 280, 60,
	WC_FINANCES, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_player_finances_small_widgets,
	PlayerFinancesWndProc
};

static void DoShowPlayerFinances(PlayerID player, bool show_small, bool show_stickied)
{
	if (!IsValidPlayer(player)) return;

	Window *w = AllocateWindowDescFront(show_small ? &_player_finances_small_desc : &_player_finances_desc, player);
	if (w != NULL) {
		w->caption_color = w->window_number;
		WP(w, def_d).data_1 = show_small;
		if (show_stickied) w->flags4 |= WF_STICKY;
	}
}

void ShowPlayerFinances(PlayerID player)
{
	DoShowPlayerFinances(player, false, false);
}

/* List of colours for the livery window */
static const StringID _colour_dropdown[] = {
	STR_00D1_DARK_BLUE,
	STR_00D2_PALE_GREEN,
	STR_00D3_PINK,
	STR_00D4_YELLOW,
	STR_00D5_RED,
	STR_00D6_LIGHT_BLUE,
	STR_00D7_GREEN,
	STR_00D8_DARK_GREEN,
	STR_00D9_BLUE,
	STR_00DA_CREAM,
	STR_00DB_MAUVE,
	STR_00DC_PURPLE,
	STR_00DD_ORANGE,
	STR_00DE_BROWN,
	STR_00DF_GREY,
	STR_00E0_WHITE,
	INVALID_STRING_ID
};

/* Association of liveries to livery classes */
static const LiveryClass livery_class[LS_END] = {
	LC_OTHER,
	LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL,
	LC_ROAD, LC_ROAD,
	LC_SHIP, LC_SHIP,
	LC_AIRCRAFT, LC_AIRCRAFT, LC_AIRCRAFT,
	LC_ROAD, LC_ROAD,
};

/* Number of liveries in each class, used to determine the height of the livery window */
static const byte livery_height[] = {
	1,
	11,
	4,
	2,
	3,
};

struct livery_d {
	uint32 sel;
	LiveryClass livery_class;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(livery_d));

static void ShowColourDropDownMenu(Window *w, uint32 widget)
{
	uint32 used_colours = 0;
	const Livery *livery;
	LiveryScheme scheme;

	/* Disallow other player colours for the primary colour */
	if (HasBit(WP(w, livery_d).sel, LS_DEFAULT) && widget == 10) {
		const Player *p;
		FOR_ALL_PLAYERS(p) {
			if (p->is_active && p->index != _local_player) SetBit(used_colours, p->player_color);
		}
	}

	/* Get the first selected livery to use as the default dropdown item */
	for (scheme = LS_BEGIN; scheme < LS_END; scheme++) {
		if (HasBit(WP(w, livery_d).sel, scheme)) break;
	}
	if (scheme == LS_END) scheme = LS_DEFAULT;
	livery = &GetPlayer((PlayerID)w->window_number)->livery[scheme];

	ShowDropDownMenu(w, _colour_dropdown, widget == 10 ? livery->colour1 : livery->colour2, widget, used_colours, 0);
}

static void SelectPlayerLiveryWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_CREATE:
			w->LowerWidget(WP(w, livery_d).livery_class + 2);
			if (!_loaded_newgrf_features.has_2CC) {
				w->HideWidget(11);
				w->HideWidget(12);
			}
			break;

		case WE_PAINT: {
			const Player *p = GetPlayer((PlayerID)w->window_number);
			LiveryScheme scheme = LS_DEFAULT;
			int y = 51;

			/* Disable dropdown controls if no scheme is selected */
			w->SetWidgetDisabledState( 9, (WP(w, livery_d).sel == 0));
			w->SetWidgetDisabledState(10, (WP(w, livery_d).sel == 0));
			w->SetWidgetDisabledState(11, (WP(w, livery_d).sel == 0));
			w->SetWidgetDisabledState(12, (WP(w, livery_d).sel == 0));

			if (!(WP(w, livery_d).sel == 0)) {
				for (scheme = LS_BEGIN; scheme < LS_END; scheme++) {
					if (HasBit(WP(w, livery_d).sel, scheme)) break;
				}
				if (scheme == LS_END) scheme = LS_DEFAULT;
			}

			SetDParam(0, STR_00D1_DARK_BLUE + p->livery[scheme].colour1);
			SetDParam(1, STR_00D1_DARK_BLUE + p->livery[scheme].colour2);

			DrawWindowWidgets(w);

			for (scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
				if (livery_class[scheme] == WP(w, livery_d).livery_class) {
					bool sel = HasBit(WP(w, livery_d).sel, scheme) != 0;

					if (scheme != LS_DEFAULT) {
						DrawSprite(p->livery[scheme].in_use ? SPR_BOX_CHECKED : SPR_BOX_EMPTY, PAL_NONE, 2, y);
					}

					DrawString(15, y, STR_LIVERY_DEFAULT + scheme, sel ? TC_WHITE : TC_BLACK);

					DrawSprite(SPR_SQUARE, GENERAL_SPRITE_COLOR(p->livery[scheme].colour1), 152, y);
					DrawString(165, y, STR_00D1_DARK_BLUE + p->livery[scheme].colour1, sel ? TC_WHITE : TC_GOLD);

					if (_loaded_newgrf_features.has_2CC) {
						DrawSprite(SPR_SQUARE, GENERAL_SPRITE_COLOR(p->livery[scheme].colour2), 277, y);
						DrawString(290, y, STR_00D1_DARK_BLUE + p->livery[scheme].colour2, sel ? TC_WHITE : TC_GOLD);
					}

					y += 14;
				}
			}
			break;
		}

		case WE_CLICK: {
			switch (e->we.click.widget) {
				/* Livery Class buttons */
				case 2:
				case 3:
				case 4:
				case 5:
				case 6: {
					LiveryScheme scheme;

					w->RaiseWidget(WP(w, livery_d).livery_class + 2);
					WP(w, livery_d).livery_class = (LiveryClass)(e->we.click.widget - 2);
					WP(w, livery_d).sel = 0;
					w->LowerWidget(WP(w, livery_d).livery_class + 2);

					/* Select the first item in the list */
					for (scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
						if (livery_class[scheme] == WP(w, livery_d).livery_class) {
							WP(w, livery_d).sel = 1 << scheme;
							break;
						}
					}
					w->height = 49 + livery_height[WP(w, livery_d).livery_class] * 14;
					w->widget[13].bottom = w->height - 1;
					w->widget[13].data = livery_height[WP(w, livery_d).livery_class] << 8 | 1;
					MarkWholeScreenDirty();
					break;
				}

				case 9:
				case 10: /* First colour dropdown */
					ShowColourDropDownMenu(w, 10);
					break;

				case 11:
				case 12: /* Second colour dropdown */
					ShowColourDropDownMenu(w, 12);
					break;

				case 13: {
					LiveryScheme scheme;
					LiveryScheme j = (LiveryScheme)((e->we.click.pt.y - 48) / 14);

					for (scheme = LS_BEGIN; scheme <= j; scheme++) {
						if (livery_class[scheme] != WP(w, livery_d).livery_class) j++;
						if (scheme >= LS_END) return;
					}
					if (j >= LS_END) return;

					/* If clicking on the left edge, toggle using the livery */
					if (e->we.click.pt.x < 10) {
						DoCommandP(0, j | (2 << 8), !GetPlayer((PlayerID)w->window_number)->livery[j].in_use, NULL, CMD_SET_PLAYER_COLOR);
					}

					if (_ctrl_pressed) {
						ToggleBit(WP(w, livery_d).sel, j);
					} else {
						WP(w, livery_d).sel = 1 << j;
					}
					SetWindowDirty(w);
					break;
				}
			}
			break;
		}

		case WE_DROPDOWN_SELECT: {
			LiveryScheme scheme;

			for (scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
				if (HasBit(WP(w, livery_d).sel, scheme)) {
					DoCommandP(0, scheme | (e->we.dropdown.button == 10 ? 0 : 256), e->we.dropdown.index, NULL, CMD_SET_PLAYER_COLOR);
				}
			}
			break;
		}
	}
}

static const Widget _select_player_livery_2cc_widgets[] = {
{ WWT_CLOSEBOX, RESIZE_NONE, 14,   0,  10,   0,  13, STR_00C5,                  STR_018B_CLOSE_WINDOW },
{  WWT_CAPTION, RESIZE_NONE, 14,  11, 399,   0,  13, STR_7007_NEW_COLOR_SCHEME, STR_018C_WINDOW_TITLE_DRAG_THIS },
{   WWT_IMGBTN, RESIZE_NONE, 14,   0,  21,  14,  35, SPR_IMG_COMPANY_GENERAL,   STR_LIVERY_GENERAL_TIP },
{   WWT_IMGBTN, RESIZE_NONE, 14,  22,  43,  14,  35, SPR_IMG_TRAINLIST,         STR_LIVERY_TRAIN_TIP },
{   WWT_IMGBTN, RESIZE_NONE, 14,  44,  65,  14,  35, SPR_IMG_TRUCKLIST,         STR_LIVERY_ROADVEH_TIP },
{   WWT_IMGBTN, RESIZE_NONE, 14,  66,  87,  14,  35, SPR_IMG_SHIPLIST,          STR_LIVERY_SHIP_TIP },
{   WWT_IMGBTN, RESIZE_NONE, 14,  88, 109,  14,  35, SPR_IMG_AIRPLANESLIST,     STR_LIVERY_AIRCRAFT_TIP },
{    WWT_PANEL, RESIZE_NONE, 14, 110, 399,  14,  35, 0x0,                       STR_NULL },
{    WWT_PANEL, RESIZE_NONE, 14,   0, 149,  36,  47, 0x0,                       STR_NULL },
{  WWT_TEXTBTN, RESIZE_NONE, 14, 150, 262,  36,  47, STR_02BD,                  STR_LIVERY_PRIMARY_TIP },
{  WWT_TEXTBTN, RESIZE_NONE, 14, 263, 274,  36,  47, STR_0225,                  STR_LIVERY_PRIMARY_TIP },
{  WWT_TEXTBTN, RESIZE_NONE, 14, 275, 387,  36,  47, STR_02E1,                  STR_LIVERY_SECONDARY_TIP },
{  WWT_TEXTBTN, RESIZE_NONE, 14, 388, 399,  36,  47, STR_0225,                  STR_LIVERY_SECONDARY_TIP },
{   WWT_MATRIX, RESIZE_NONE, 14,   0, 399,  48,  48 + 1 * 14, (1 << 8) | 1,     STR_LIVERY_PANEL_TIP },
{ WIDGETS_END },
};

static const WindowDesc _select_player_livery_2cc_desc = {
	WDP_AUTO, WDP_AUTO, 400, 49 + 1 * 14, 400, 49 + 1 * 14,
	WC_PLAYER_COLOR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_select_player_livery_2cc_widgets,
	SelectPlayerLiveryWndProc
};


static const Widget _select_player_livery_widgets[] = {
{ WWT_CLOSEBOX, RESIZE_NONE, 14,   0,  10,   0,  13, STR_00C5,                  STR_018B_CLOSE_WINDOW },
{  WWT_CAPTION, RESIZE_NONE, 14,  11, 274,   0,  13, STR_7007_NEW_COLOR_SCHEME, STR_018C_WINDOW_TITLE_DRAG_THIS },
{   WWT_IMGBTN, RESIZE_NONE, 14,   0,  21,  14,  35, SPR_IMG_COMPANY_GENERAL,   STR_LIVERY_GENERAL_TIP },
{   WWT_IMGBTN, RESIZE_NONE, 14,  22,  43,  14,  35, SPR_IMG_TRAINLIST,         STR_LIVERY_TRAIN_TIP },
{   WWT_IMGBTN, RESIZE_NONE, 14,  44,  65,  14,  35, SPR_IMG_TRUCKLIST,         STR_LIVERY_ROADVEH_TIP },
{   WWT_IMGBTN, RESIZE_NONE, 14,  66,  87,  14,  35, SPR_IMG_SHIPLIST,          STR_LIVERY_SHIP_TIP },
{   WWT_IMGBTN, RESIZE_NONE, 14,  88, 109,  14,  35, SPR_IMG_AIRPLANESLIST,     STR_LIVERY_AIRCRAFT_TIP },
{    WWT_PANEL, RESIZE_NONE, 14, 110, 274,  14,  35, 0x0,                       STR_NULL },
{    WWT_PANEL, RESIZE_NONE, 14,   0, 149,  36,  47, 0x0,                       STR_NULL },
{  WWT_TEXTBTN, RESIZE_NONE, 14, 150, 262,  36,  47, STR_02BD,                  STR_LIVERY_PRIMARY_TIP },
{  WWT_TEXTBTN, RESIZE_NONE, 14, 263, 274,  36,  47, STR_0225,                  STR_LIVERY_PRIMARY_TIP },
{  WWT_TEXTBTN, RESIZE_NONE, 14, 275, 275,  36,  47, STR_02E1,                  STR_LIVERY_SECONDARY_TIP },
{  WWT_TEXTBTN, RESIZE_NONE, 14, 275, 275,  36,  47, STR_0225,                  STR_LIVERY_SECONDARY_TIP },
{   WWT_MATRIX, RESIZE_NONE, 14,   0, 274,  48,  48 + 1 * 14, (1 << 8) | 1,     STR_LIVERY_PANEL_TIP },
{ WIDGETS_END },
};

static const WindowDesc _select_player_livery_desc = {
	WDP_AUTO, WDP_AUTO, 275, 49 + 1 * 14, 275, 49 + 1 * 14,
	WC_PLAYER_COLOR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_select_player_livery_widgets,
	SelectPlayerLiveryWndProc
};

/**
 * Draws the face of a player.
 * @param pf    the player's face
 * @param color the (background) color of the gradient
 * @param x     x-position to draw the face
 * @param y     y-position to draw the face
 */
void DrawPlayerFace(PlayerFace pf, int color, int x, int y)
{
	GenderEthnicity ge = (GenderEthnicity)GetPlayerFaceBits(pf, PFV_GEN_ETHN, GE_WM);

	bool has_moustache   = !HasBit(ge, GENDER_FEMALE) && GetPlayerFaceBits(pf, PFV_HAS_MOUSTACHE,   ge) != 0;
	bool has_tie_earring = !HasBit(ge, GENDER_FEMALE) || GetPlayerFaceBits(pf, PFV_HAS_TIE_EARRING, ge) != 0;
	bool has_glasses     = GetPlayerFaceBits(pf, PFV_HAS_GLASSES, ge) != 0;
	SpriteID pal;

	/* Modify eye colour palette only if 2 or more valid values exist */
	if (_pf_info[PFV_EYE_COLOUR].valid_values[ge] < 2) {
		pal = PAL_NONE;
	} else {
		switch (GetPlayerFaceBits(pf, PFV_EYE_COLOUR, ge)) {
			default: NOT_REACHED();
			case 0: pal = PALETTE_TO_BROWN; break;
			case 1: pal = PALETTE_TO_BLUE;  break;
			case 2: pal = PALETTE_TO_GREEN; break;
		}
	}

	/* Draw the gradient (background) */
	DrawSprite(SPR_GRADIENT, GENERAL_SPRITE_COLOR(color), x, y);

	for (PlayerFaceVariable pfv = PFV_CHEEKS; pfv < PFV_END; pfv++) {
		switch (pfv) {
			case PFV_MOUSTACHE:   if (!has_moustache)   continue; break;
			case PFV_LIPS:        /* FALL THROUGH */
			case PFV_NOSE:        if (has_moustache)    continue; break;
			case PFV_TIE_EARRING: if (!has_tie_earring) continue; break;
			case PFV_GLASSES:     if (!has_glasses)     continue; break;
			default: break;
		}
		DrawSprite(GetPlayerFaceSprite(pf, pfv, ge), (pfv == PFV_EYEBROWS) ? pal : PAL_NONE, x, y);
	}
}

/**
 * Names of the widgets. Keep them in the same order as in the widget array.
 * Do not change the order of the widgets from PFW_WIDGET_HAS_MOUSTACHE_EARRING to PFW_WIDGET_GLASSES_R,
 * this order is needed for the WE_CLICK event of DrawFaceStringLabel().
 */
enum PlayerFaceWindowWidgets {
	PFW_WIDGET_CLOSEBOX = 0,
	PFW_WIDGET_CAPTION,
	PFW_WIDGET_TOGGLE_LARGE_SMALL,
	PFW_WIDGET_SELECT_FACE,
	PFW_WIDGET_CANCEL,
	PFW_WIDGET_ACCEPT,
	PFW_WIDGET_MALE,
	PFW_WIDGET_FEMALE,
	PFW_WIDGET_RANDOM_NEW_FACE,
	PFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON,
	/* from here is the advanced player face selection window */
	PFW_WIDGET_LOAD,
	PFW_WIDGET_FACECODE,
	PFW_WIDGET_SAVE,
	PFW_WIDGET_ETHNICITY_EUR,
	PFW_WIDGET_ETHNICITY_AFR,
	PFW_WIDGET_HAS_MOUSTACHE_EARRING,
	PFW_WIDGET_HAS_GLASSES,
	PFW_WIDGET_EYECOLOUR_L,
	PFW_WIDGET_EYECOLOUR,
	PFW_WIDGET_EYECOLOUR_R,
	PFW_WIDGET_CHIN_L,
	PFW_WIDGET_CHIN,
	PFW_WIDGET_CHIN_R,
	PFW_WIDGET_EYEBROWS_L,
	PFW_WIDGET_EYEBROWS,
	PFW_WIDGET_EYEBROWS_R,
	PFW_WIDGET_LIPS_MOUSTACHE_L,
	PFW_WIDGET_LIPS_MOUSTACHE,
	PFW_WIDGET_LIPS_MOUSTACHE_R,
	PFW_WIDGET_NOSE_L,
	PFW_WIDGET_NOSE,
	PFW_WIDGET_NOSE_R,
	PFW_WIDGET_HAIR_L,
	PFW_WIDGET_HAIR,
	PFW_WIDGET_HAIR_R,
	PFW_WIDGET_JACKET_L,
	PFW_WIDGET_JACKET,
	PFW_WIDGET_JACKET_R,
	PFW_WIDGET_COLLAR_L,
	PFW_WIDGET_COLLAR,
	PFW_WIDGET_COLLAR_R,
	PFW_WIDGET_TIE_EARRING_L,
	PFW_WIDGET_TIE_EARRING,
	PFW_WIDGET_TIE_EARRING_R,
	PFW_WIDGET_GLASSES_L,
	PFW_WIDGET_GLASSES,
	PFW_WIDGET_GLASSES_R,
};

/** Widget description for the normal/simple player face selection dialog */
static const Widget _select_player_face_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW},              // PFW_WIDGET_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   174,     0,    13, STR_7043_FACE_SELECTION, STR_018C_WINDOW_TITLE_DRAG_THIS},    // PFW_WIDGET_CAPTION
{     WWT_IMGBTN,   RESIZE_NONE,    14,   175,   189,     0,    13, SPR_LARGE_SMALL_WINDOW,  STR_FACE_ADVANCED_TIP},              // PFW_WIDGET_TOGGLE_LARGE_SMALL
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   189,    14,   150, 0x0,                     STR_NULL},                           // PFW_WIDGET_SELECT_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    94,   151,   162, STR_012E_CANCEL,         STR_7047_CANCEL_NEW_FACE_SELECTION}, // PFW_WIDGET_CANCEL
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    95,   189,   151,   162, STR_012F_OK,             STR_7048_ACCEPT_NEW_FACE_SELECTION}, // PFW_WIDGET_ACCEPT
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    95,   187,    75,    86, STR_7044_MALE,           STR_7049_SELECT_MALE_FACES},         // PFW_WIDGET_MALE
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    95,   187,    87,    98, STR_7045_FEMALE,         STR_704A_SELECT_FEMALE_FACES},       // PFW_WIDGET_FEMALE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     2,    93,   137,   148, STR_7046_NEW_FACE,       STR_704B_GENERATE_RANDOM_NEW_FACE},  // PFW_WIDGET_RANDOM_NEW_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    95,   187,    16,    27, STR_FACE_ADVANCED,       STR_FACE_ADVANCED_TIP},              // PFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON
{   WIDGETS_END},
};

/** Widget description for the advanced player face selection dialog */
static const Widget _select_player_face_adv_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW},              // PFW_WIDGET_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   204,     0,    13, STR_7043_FACE_SELECTION, STR_018C_WINDOW_TITLE_DRAG_THIS},    // PFW_WIDGET_CAPTION
{     WWT_IMGBTN,   RESIZE_NONE,    14,   205,   219,     0,    13, SPR_LARGE_SMALL_WINDOW,  STR_FACE_SIMPLE_TIP},                // PFW_WIDGET_TOGGLE_LARGE_SMALL
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   219,    14,   207, 0x0,                     STR_NULL},                           // PFW_WIDGET_SELECT_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    94,   208,   219, STR_012E_CANCEL,         STR_7047_CANCEL_NEW_FACE_SELECTION}, // PFW_WIDGET_CANCEL
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    95,   219,   208,   219, STR_012F_OK,             STR_7048_ACCEPT_NEW_FACE_SELECTION}, // PFW_WIDGET_ACCEPT
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    96,   156,    32,    43, STR_7044_MALE,           STR_7049_SELECT_MALE_FACES},         // PFW_WIDGET_MALE
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   157,   217,    32,    43, STR_7045_FEMALE,         STR_704A_SELECT_FEMALE_FACES},       // PFW_WIDGET_FEMALE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     2,    93,   137,   148, STR_RANDOM,              STR_704B_GENERATE_RANDOM_NEW_FACE},  // PFW_WIDGET_RANDOM_NEW_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    95,   217,    16,    27, STR_FACE_SIMPLE,         STR_FACE_SIMPLE_TIP},                // PFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     2,    93,   158,   169, STR_FACE_LOAD,           STR_FACE_LOAD_TIP},                  // PFW_WIDGET_LOAD
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     2,    93,   170,   181, STR_FACE_FACECODE,       STR_FACE_FACECODE_TIP},              // PFW_WIDGET_FACECODE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     2,    93,   182,   193, STR_FACE_SAVE,           STR_FACE_SAVE_TIP},                  // PFW_WIDGET_SAVE
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    96,   156,    46,    57, STR_FACE_EUROPEAN,       STR_FACE_SELECT_EUROPEAN},           // PFW_WIDGET_ETHNICITY_EUR
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   157,   217,    46,    57, STR_FACE_AFRICAN,        STR_FACE_SELECT_AFRICAN},            // PFW_WIDGET_ETHNICITY_AFR
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   175,   217,    60,    71, STR_EMPTY,               STR_FACE_MOUSTACHE_EARRING_TIP},     // PFW_WIDGET_HAS_MOUSTACHE_EARRING
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   175,   217,    72,    83, STR_EMPTY,               STR_FACE_GLASSES_TIP},               // PFW_WIDGET_HAS_GLASSES
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    175,  183,   110,   121, SPR_ARROW_LEFT,          STR_FACE_EYECOLOUR_TIP},             // PFW_WIDGET_EYECOLOUR_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    184,  208,   110,   121, STR_EMPTY,               STR_FACE_EYECOLOUR_TIP},             // PFW_WIDGET_EYECOLOUR
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    209,  217,   110,   121, SPR_ARROW_RIGHT,         STR_FACE_EYECOLOUR_TIP},             // PFW_WIDGET_EYECOLOUR_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    175,  183,   158,   169, SPR_ARROW_LEFT,          STR_FACE_CHIN_TIP},                  // PFW_WIDGET_CHIN_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    184,  208,   158,   169, STR_EMPTY,               STR_FACE_CHIN_TIP},                  // PFW_WIDGET_CHIN
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    209,  217,   158,   169, SPR_ARROW_RIGHT,         STR_FACE_CHIN_TIP},                  // PFW_WIDGET_CHIN_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    175,  183,    98,   109, SPR_ARROW_LEFT,          STR_FACE_EYEBROWS_TIP},              // PFW_WIDGET_EYEBROWS_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    184,  208,    98,   109, STR_EMPTY,               STR_FACE_EYEBROWS_TIP},              // PFW_WIDGET_EYEBROWS
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    209,  217,    98,   109, SPR_ARROW_RIGHT,         STR_FACE_EYEBROWS_TIP},              // PFW_WIDGET_EYEBROWS_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    175,  183,   146,   157, SPR_ARROW_LEFT,          STR_FACE_LIPS_MOUSTACHE_TIP},        // PFW_WIDGET_LIPS_MOUSTACHE_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    184,  208,   146,   157, STR_EMPTY,               STR_FACE_LIPS_MOUSTACHE_TIP},        // PFW_WIDGET_LIPS_MOUSTACHE
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    209,  217,   146,   157, SPR_ARROW_RIGHT,         STR_FACE_LIPS_MOUSTACHE_TIP},        // PFW_WIDGET_LIPS_MOUSTACHE_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    175,  183,   134,   145, SPR_ARROW_LEFT,          STR_FACE_NOSE_TIP},                  // PFW_WIDGET_NOSE_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    184,  208,   134,   145, STR_EMPTY,               STR_FACE_NOSE_TIP},                  // PFW_WIDGET_NOSE
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    209,  217,   134,   145, SPR_ARROW_RIGHT,         STR_FACE_NOSE_TIP},                  // PFW_WIDGET_NOSE_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    175,  183,    86,    97, SPR_ARROW_LEFT,          STR_FACE_HAIR_TIP},                  // PFW_WIDGET_HAIR_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    184,  208,    86,    97, STR_EMPTY,               STR_FACE_HAIR_TIP},                  // PFW_WIDGET_HAIR
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    209,  217,    86,    97, SPR_ARROW_RIGHT,         STR_FACE_HAIR_TIP},                  // PFW_WIDGET_HAIR_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    175,  183,   170,   181, SPR_ARROW_LEFT,          STR_FACE_JACKET_TIP},                // PFW_WIDGET_JACKET_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    184,  208,   170,   181, STR_EMPTY,               STR_FACE_JACKET_TIP},                // PFW_WIDGET_JACKET
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    209,  217,   170,   181, SPR_ARROW_RIGHT,         STR_FACE_JACKET_TIP},                // PFW_WIDGET_JACKET_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    175,  183,   182,   193, SPR_ARROW_LEFT,          STR_FACE_COLLAR_TIP},                // PFW_WIDGET_COLLAR_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    184,  208,   182,   193, STR_EMPTY,               STR_FACE_COLLAR_TIP},                // PFW_WIDGET_COLLAR
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    209,  217,   182,   193, SPR_ARROW_RIGHT,         STR_FACE_COLLAR_TIP},                // PFW_WIDGET_COLLAR_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    175,  183,   194,   205, SPR_ARROW_LEFT,          STR_FACE_TIE_EARRING_TIP},           // PFW_WIDGET_TIE_EARRING_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    184,  208,   194,   205, STR_EMPTY,               STR_FACE_TIE_EARRING_TIP},           // PFW_WIDGET_TIE_EARRING
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    209,  217,   194,   205, SPR_ARROW_RIGHT,         STR_FACE_TIE_EARRING_TIP},           // PFW_WIDGET_TIE_EARRING_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    175,  183,   122,   133, SPR_ARROW_LEFT,          STR_FACE_GLASSES_TIP_2},             // PFW_WIDGET_GLASSES_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    184,  208,   122,   133, STR_EMPTY,               STR_FACE_GLASSES_TIP_2},             // PFW_WIDGET_GLASSES
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,    209,  217,   122,   133, SPR_ARROW_RIGHT,         STR_FACE_GLASSES_TIP_2},             // PFW_WIDGET_GLASSES_R
{   WIDGETS_END},
};

/**
 * Draw dynamic a label to the left of the button and a value in the button
 *
 * @param w              Window on which the widget is located
 * @param widget_index   index of this widget in the window
 * @param str            the label which will be draw
 * @param val            the value which will be draw
 * @param is_bool_widget is it a bool button
 */
void DrawFaceStringLabel(const Window *w, byte widget_index, StringID str, uint8 val, bool is_bool_widget)
{
	/* Write the label in gold (0x2) to the left of the button. */
	DrawStringRightAligned(w->widget[widget_index].left - (is_bool_widget ? 5 : 14), w->widget[widget_index].top + 1, str, TC_GOLD);

	if (!w->IsWidgetDisabled(widget_index)) {
		if (is_bool_widget) {
			/* if it a bool button write yes or no */
			str = (val != 0) ? STR_FACE_YES : STR_FACE_NO;
		} else {
			/* else write the value + 1 */
			SetDParam(0, val + 1);
			str = STR_JUST_INT;
		}

		/* Draw the value/bool in white (0xC). If the button clicked adds 1px to x and y text coordinates (IsWindowWidgetLowered()). */
		DrawStringCentered(w->widget[widget_index].left + (w->widget[widget_index].right - w->widget[widget_index].left) / 2 +
			w->IsWidgetLowered(widget_index), w->widget[widget_index].top + 1 + w->IsWidgetLowered(widget_index), str, TC_WHITE);
	}
}

/**
 * Player face selection window event definition
 *
 * @param w window pointer
 * @param e event been triggered
 */
static void SelectPlayerFaceWndProc(Window *w, WindowEvent *e)
{
	PlayerFace *pf = &WP(w, facesel_d).face; // pointer to the player face bits
	GenderEthnicity ge = (GenderEthnicity)GB(*pf, _pf_info[PFV_GEN_ETHN].offset, _pf_info[PFV_GEN_ETHN].length); // get the gender and ethnicity
	bool is_female = HasBit(ge, GENDER_FEMALE); // get the gender: 0 == male and 1 == female
	bool is_moust_male = !is_female && GetPlayerFaceBits(*pf, PFV_HAS_MOUSTACHE, ge) != 0; // is a male face with moustache

	switch (e->event) {
		case WE_PAINT:
			/* lower the non-selected gender button */
			w->SetWidgetLoweredState(PFW_WIDGET_MALE,  !is_female);
			w->SetWidgetLoweredState(PFW_WIDGET_FEMALE, is_female);

			/* advanced player face selection window */
			if (WP(w, facesel_d).advanced) {
				/* lower the non-selected ethnicity button */
				w->SetWidgetLoweredState(PFW_WIDGET_ETHNICITY_EUR, !HasBit(ge, ETHNICITY_BLACK));
				w->SetWidgetLoweredState(PFW_WIDGET_ETHNICITY_AFR,  HasBit(ge, ETHNICITY_BLACK));


				/* Disable dynamically the widgets which PlayerFaceVariable has less than 2 options
				* (or in other words you haven't any choice).
				* If the widgets depend on a HAS-variable and this is false the widgets will be disabled, too. */

				/* Eye colour buttons */
				w->SetWidgetsDisabledState(_pf_info[PFV_EYE_COLOUR].valid_values[ge] < 2,
					PFW_WIDGET_EYECOLOUR, PFW_WIDGET_EYECOLOUR_L, PFW_WIDGET_EYECOLOUR_R, WIDGET_LIST_END);

				/* Chin buttons */
				w->SetWidgetsDisabledState(_pf_info[PFV_CHIN].valid_values[ge] < 2,
					PFW_WIDGET_CHIN, PFW_WIDGET_CHIN_L, PFW_WIDGET_CHIN_R, WIDGET_LIST_END);

				/* Eyebrows buttons */
				w->SetWidgetsDisabledState(_pf_info[PFV_EYEBROWS].valid_values[ge] < 2,
					PFW_WIDGET_EYEBROWS, PFW_WIDGET_EYEBROWS_L, PFW_WIDGET_EYEBROWS_R, WIDGET_LIST_END);

				/* Lips or (if it a male face with a moustache) moustache buttons */
				w->SetWidgetsDisabledState(_pf_info[is_moust_male ? PFV_MOUSTACHE : PFV_LIPS].valid_values[ge] < 2,
					PFW_WIDGET_LIPS_MOUSTACHE, PFW_WIDGET_LIPS_MOUSTACHE_L, PFW_WIDGET_LIPS_MOUSTACHE_R, WIDGET_LIST_END);

				/* Nose buttons | male faces with moustache haven't any nose options */
				w->SetWidgetsDisabledState(_pf_info[PFV_NOSE].valid_values[ge] < 2 || is_moust_male,
					PFW_WIDGET_NOSE, PFW_WIDGET_NOSE_L, PFW_WIDGET_NOSE_R, WIDGET_LIST_END);

				/* Hair buttons */
				w->SetWidgetsDisabledState(_pf_info[PFV_HAIR].valid_values[ge] < 2,
					PFW_WIDGET_HAIR, PFW_WIDGET_HAIR_L, PFW_WIDGET_HAIR_R, WIDGET_LIST_END);

				/* Jacket buttons */
				w->SetWidgetsDisabledState(_pf_info[PFV_JACKET].valid_values[ge] < 2,
					PFW_WIDGET_JACKET, PFW_WIDGET_JACKET_L, PFW_WIDGET_JACKET_R, WIDGET_LIST_END);

				/* Collar buttons */
				w->SetWidgetsDisabledState(_pf_info[PFV_COLLAR].valid_values[ge] < 2,
					PFW_WIDGET_COLLAR, PFW_WIDGET_COLLAR_L, PFW_WIDGET_COLLAR_R, WIDGET_LIST_END);

				/* Tie/earring buttons | female faces without earring haven't any earring options */
				w->SetWidgetsDisabledState(_pf_info[PFV_TIE_EARRING].valid_values[ge] < 2 ||
						(is_female && GetPlayerFaceBits(*pf, PFV_HAS_TIE_EARRING, ge) == 0),
					PFW_WIDGET_TIE_EARRING, PFW_WIDGET_TIE_EARRING_L, PFW_WIDGET_TIE_EARRING_R, WIDGET_LIST_END);

				/* Glasses buttons | faces without glasses haven't any glasses options */
				w->SetWidgetsDisabledState(_pf_info[PFV_GLASSES].valid_values[ge] < 2 || GetPlayerFaceBits(*pf, PFV_HAS_GLASSES, ge) == 0,
					PFW_WIDGET_GLASSES, PFW_WIDGET_GLASSES_L, PFW_WIDGET_GLASSES_R, WIDGET_LIST_END);
			}

			DrawWindowWidgets(w);

			/* Draw dynamic button value and labels for the advanced player face selection window */
			if (WP(w, facesel_d).advanced) {
				if (is_female) {
					/* Only for female faces */
					DrawFaceStringLabel(w, PFW_WIDGET_HAS_MOUSTACHE_EARRING, STR_FACE_EARRING,   GetPlayerFaceBits(*pf, PFV_HAS_TIE_EARRING, ge), true );
					DrawFaceStringLabel(w, PFW_WIDGET_TIE_EARRING,           STR_FACE_EARRING,   GetPlayerFaceBits(*pf, PFV_TIE_EARRING,     ge), false);
				} else {
					/* Only for male faces */
					DrawFaceStringLabel(w, PFW_WIDGET_HAS_MOUSTACHE_EARRING, STR_FACE_MOUSTACHE, GetPlayerFaceBits(*pf, PFV_HAS_MOUSTACHE,   ge), true );
					DrawFaceStringLabel(w, PFW_WIDGET_TIE_EARRING,           STR_FACE_TIE,       GetPlayerFaceBits(*pf, PFV_TIE_EARRING,     ge), false);
				}
				if (is_moust_male) {
					/* Only for male faces with moustache */
					DrawFaceStringLabel(w, PFW_WIDGET_LIPS_MOUSTACHE,        STR_FACE_MOUSTACHE, GetPlayerFaceBits(*pf, PFV_MOUSTACHE,       ge), false);
				} else {
					/* Only for female faces or male faces without moustache */
					DrawFaceStringLabel(w, PFW_WIDGET_LIPS_MOUSTACHE,        STR_FACE_LIPS,      GetPlayerFaceBits(*pf, PFV_LIPS,            ge), false);
				}
				/* For all faces */
				DrawFaceStringLabel(w, PFW_WIDGET_HAS_GLASSES,           STR_FACE_GLASSES,     GetPlayerFaceBits(*pf, PFV_HAS_GLASSES,     ge), true );
				DrawFaceStringLabel(w, PFW_WIDGET_HAIR,                  STR_FACE_HAIR,        GetPlayerFaceBits(*pf, PFV_HAIR,            ge), false);
				DrawFaceStringLabel(w, PFW_WIDGET_EYEBROWS,              STR_FACE_EYEBROWS,    GetPlayerFaceBits(*pf, PFV_EYEBROWS,        ge), false);
				DrawFaceStringLabel(w, PFW_WIDGET_EYECOLOUR,             STR_FACE_EYECOLOUR,   GetPlayerFaceBits(*pf, PFV_EYE_COLOUR,      ge), false);
				DrawFaceStringLabel(w, PFW_WIDGET_GLASSES,               STR_FACE_GLASSES,     GetPlayerFaceBits(*pf, PFV_GLASSES,         ge), false);
				DrawFaceStringLabel(w, PFW_WIDGET_NOSE,                  STR_FACE_NOSE,        GetPlayerFaceBits(*pf, PFV_NOSE,            ge), false);
				DrawFaceStringLabel(w, PFW_WIDGET_CHIN,                  STR_FACE_CHIN,        GetPlayerFaceBits(*pf, PFV_CHIN,            ge), false);
				DrawFaceStringLabel(w, PFW_WIDGET_JACKET,                STR_FACE_JACKET,      GetPlayerFaceBits(*pf, PFV_JACKET,          ge), false);
				DrawFaceStringLabel(w, PFW_WIDGET_COLLAR,                STR_FACE_COLLAR,      GetPlayerFaceBits(*pf, PFV_COLLAR,          ge), false);
			}

			/* Draw the player face picture */
			DrawPlayerFace(*pf, GetPlayer((PlayerID)w->window_number)->player_color, 2, 16);
			break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				/* Toggle size, advanced/simple face selection */
				case PFW_WIDGET_TOGGLE_LARGE_SMALL:
				case PFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON:
					DoCommandP(0, 0, *pf, NULL, CMD_SET_PLAYER_FACE);
					DeleteWindow(w);
					DoSelectPlayerFace((PlayerID)w->window_number, !WP(w, facesel_d).advanced);
					break;

				/* Cancel button */
				case PFW_WIDGET_CANCEL:
					DeleteWindow(w);
					break;

				/* OK button */
				case PFW_WIDGET_ACCEPT:
					DoCommandP(0, 0, *pf, NULL, CMD_SET_PLAYER_FACE);
					DeleteWindow(w);
					break;

				/* Load button */
				case PFW_WIDGET_LOAD:
					*pf = _player_face;
					ScaleAllPlayerFaceBits(*pf);
					ShowErrorMessage(INVALID_STRING_ID, STR_FACE_LOAD_DONE, 0, 0);
					SetWindowDirty(w);
					break;

				/* 'Player face number' button, view and/or set player face number */
				case PFW_WIDGET_FACECODE:
					SetDParam(0, *pf);
					ShowQueryString(STR_JUST_INT, STR_FACE_FACECODE_CAPTION, 10 + 1, 0, w, CS_NUMERAL);
					break;

				/* Save button */
				case PFW_WIDGET_SAVE:
					_player_face = *pf;
					ShowErrorMessage(INVALID_STRING_ID, STR_FACE_SAVE_DONE, 0, 0);
					break;

				/* Toggle gender (male/female) button */
				case PFW_WIDGET_MALE:
				case PFW_WIDGET_FEMALE:
					SetPlayerFaceBits(*pf, PFV_GENDER, ge, e->we.click.widget - PFW_WIDGET_MALE);
					ScaleAllPlayerFaceBits(*pf);
					SetWindowDirty(w);
					break;

				/* Randomize face button */
				case PFW_WIDGET_RANDOM_NEW_FACE:
					RandomPlayerFaceBits(*pf, ge, WP(w, facesel_d).advanced);
					SetWindowDirty(w);
					break;

				/* Toggle ethnicity (european/african) button */
				case PFW_WIDGET_ETHNICITY_EUR:
				case PFW_WIDGET_ETHNICITY_AFR:
					SetPlayerFaceBits(*pf, PFV_ETHNICITY, ge, e->we.click.widget - PFW_WIDGET_ETHNICITY_EUR);
					ScaleAllPlayerFaceBits(*pf);
					SetWindowDirty(w);
					break;

				default:
					/* For all buttons from PFW_WIDGET_HAS_MOUSTACHE_EARRING to PFW_WIDGET_GLASSES_R is the same function.
					* Therefor is this combined function.
					* First it checks which PlayerFaceVariable will be change and then
					* a: invert the value for boolean variables
					* or b: it checks inside of IncreasePlayerFaceBits() if a left (_L) butten is pressed and then decrease else increase the variable */
					if (WP(w, facesel_d).advanced && e->we.click.widget >= PFW_WIDGET_HAS_MOUSTACHE_EARRING && e->we.click.widget <= PFW_WIDGET_GLASSES_R) {
						PlayerFaceVariable pfv; // which PlayerFaceVariable shall be edited

						if (e->we.click.widget < PFW_WIDGET_EYECOLOUR_L) { // Bool buttons
							switch (e->we.click.widget - PFW_WIDGET_HAS_MOUSTACHE_EARRING) {
								default: NOT_REACHED();
								case 0: pfv = is_female ? PFV_HAS_TIE_EARRING : PFV_HAS_MOUSTACHE; break; // Has earring/moustache button
								case 1: pfv = PFV_HAS_GLASSES; break; // Has glasses button
							}
							SetPlayerFaceBits(*pf, pfv, ge, !GetPlayerFaceBits(*pf, pfv, ge));
							ScaleAllPlayerFaceBits(*pf);

						} else { // Value buttons
							switch ((e->we.click.widget - PFW_WIDGET_EYECOLOUR_L) / 3) {
								default: NOT_REACHED();
								case 0: pfv = PFV_EYE_COLOUR; break;  // Eye colour buttons
								case 1: pfv = PFV_CHIN; break;        // Chin buttons
								case 2: pfv = PFV_EYEBROWS; break;    // Eyebrows buttons
								case 3: pfv = is_moust_male ? PFV_MOUSTACHE : PFV_LIPS; break; // Moustache or lips buttons
								case 4: pfv = PFV_NOSE; break;        // Nose buttons
								case 5: pfv = PFV_HAIR; break;        // Hair buttons
								case 6: pfv = PFV_JACKET; break;      // Jacket buttons
								case 7: pfv = PFV_COLLAR; break;      // Collar buttons
								case 8: pfv = PFV_TIE_EARRING; break; // Tie/earring buttons
								case 9: pfv = PFV_GLASSES; break;     // Glasses buttons
							}
							/* 0 == left (_L), 1 == middle or 2 == right (_R) - button click */
							IncreasePlayerFaceBits(*pf, pfv, ge, (((e->we.click.widget - PFW_WIDGET_EYECOLOUR_L) % 3) != 0) ? 1 : -1);
						}

						SetWindowDirty(w);
					}
					break;
			}
			break;

		case WE_ON_EDIT_TEXT:
			/* Set a new player face number */
			if (!StrEmpty(e->we.edittext.str)) {
				*pf = strtoul(e->we.edittext.str, NULL, 10);
				ScaleAllPlayerFaceBits(*pf);
				ShowErrorMessage(INVALID_STRING_ID, STR_FACE_FACECODE_SET, 0, 0);
				SetWindowDirty(w);
			} else {
				ShowErrorMessage(INVALID_STRING_ID, STR_FACE_FACECODE_ERR, 0, 0);
			}
			break;
	}
}

/** normal/simple player face selection window description */
static const WindowDesc _select_player_face_desc = {
	WDP_AUTO, WDP_AUTO, 190, 163, 190, 163,
	WC_PLAYER_FACE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_select_player_face_widgets,
	SelectPlayerFaceWndProc
};

/** advanced player face selection window description */
static const WindowDesc _select_player_face_adv_desc = {
	WDP_AUTO, WDP_AUTO, 220, 220, 220, 220,
	WC_PLAYER_FACE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_select_player_face_adv_widgets,
	SelectPlayerFaceWndProc
};

/**
 * Open the simple/advanced player face selection window
 *
 * @param player the player which face shall be edited
 * @param adv    simple or advanced player face selection window
 *
 * @pre is player a valid player
 */
static void DoSelectPlayerFace(PlayerID player, bool adv)
{
	if (!IsValidPlayer(player)) return;

	Window *w = AllocateWindowDescFront(adv ? &_select_player_face_adv_desc : &_select_player_face_desc, player); // simple or advanced window

	if (w != NULL) {
		w->caption_color = w->window_number;
		WP(w, facesel_d).face = GetPlayer((PlayerID)w->window_number)->face;
		WP(w, facesel_d).advanced = adv;
	}
}


/* Names of the widgets. Keep them in the same order as in the widget array */
enum PlayerCompanyWindowWidgets {
	PCW_WIDGET_CLOSEBOX = 0,
	PCW_WIDGET_CAPTION,
	PCW_WIDGET_FACE,
	PCW_WIDGET_NEW_FACE,
	PCW_WIDGET_COLOR_SCHEME,
	PCW_WIDGET_PRESIDENT_NAME,
	PCW_WIDGET_COMPANY_NAME,
	PCW_WIDGET_BUILD_VIEW_HQ,
	PCW_WIDGET_RELOCATE_HQ,
	PCW_WIDGET_BUY_SHARE,
	PCW_WIDGET_SELL_SHARE,
	PCW_WIDGET_COMPANY_PASSWORD,
};

static const Widget _player_company_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   359,     0,    13, STR_7001,                          STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   359,    14,   157, 0x0,                               STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    89,   158,   169, STR_7004_NEW_FACE,                 STR_7030_SELECT_NEW_FACE_FOR_PRESIDENT},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    90,   179,   158,   169, STR_7005_COLOR_SCHEME,             STR_7031_CHANGE_THE_COMPANY_VEHICLE},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   180,   269,   158,   169, STR_7009_PRESIDENT_NAME,           STR_7032_CHANGE_THE_PRESIDENT_S},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   270,   359,   158,   169, STR_7008_COMPANY_NAME,             STR_7033_CHANGE_THE_COMPANY_NAME},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   266,   355,    18,    29, STR_7072_VIEW_HQ,                  STR_7070_BUILD_COMPANY_HEADQUARTERS},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   266,   355,    32,    43, STR_RELOCATE_HQ,                   STR_RELOCATE_COMPANY_HEADQUARTERS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,   179,   158,   169, STR_7077_BUY_25_SHARE_IN_COMPANY,  STR_7079_BUY_25_SHARE_IN_THIS_COMPANY},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   180,   359,   158,   169, STR_7078_SELL_25_SHARE_IN_COMPANY, STR_707A_SELL_25_SHARE_IN_THIS_COMPANY},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   266,   355,   138,   149, STR_COMPANY_PASSWORD,              STR_COMPANY_PASSWORD_TOOLTIP},
{   WIDGETS_END},
};


/**
 * Draws text "Vehicles:" and number of all vehicle types, or "(none)"
 * @param player ID of player to print statistics of
 */
static void DrawPlayerVehiclesAmount(PlayerID player)
{
	const int x = 110;
	int y = 63;
	const Vehicle *v;
	uint train = 0;
	uint road  = 0;
	uint air   = 0;
	uint ship  = 0;

	DrawString(x, y, STR_7039_VEHICLES, TC_FROMSTRING);

	FOR_ALL_VEHICLES(v) {
		if (v->owner == player) {
			switch (v->type) {
				case VEH_TRAIN:    if (IsFrontEngine(v)) train++; break;
				case VEH_ROAD:     if (IsRoadVehFront(v)) road++; break;
				case VEH_AIRCRAFT: if (IsNormalAircraft(v)) air++; break;
				case VEH_SHIP:     ship++; break;
				default: break;
			}
		}
	}

	if (train + road + air + ship == 0) {
		DrawString(x + 70, y, STR_7042_NONE, TC_FROMSTRING);
	} else {
		if (train != 0) {
			SetDParam(0, train);
			DrawString(x + 70, y, STR_TRAINS, TC_FROMSTRING);
			y += 10;
		}

		if (road != 0) {
			SetDParam(0, road);
			DrawString(x + 70, y, STR_ROAD_VEHICLES, TC_FROMSTRING);
			y += 10;
		}

		if (air != 0) {
			SetDParam(0, air);
			DrawString(x + 70, y, STR_AIRCRAFT, TC_FROMSTRING);
			y += 10;
		}

		if (ship != 0) {
			SetDParam(0, ship);
			DrawString(x + 70, y, STR_SHIPS, TC_FROMSTRING);
		}
	}
}

int GetAmountOwnedBy(const Player *p, PlayerID owner)
{
	return (p->share_owners[0] == owner) +
				 (p->share_owners[1] == owner) +
				 (p->share_owners[2] == owner) +
				 (p->share_owners[3] == owner);
}

/**
 * Draws list of all companies with shares
 * @param p pointer to the Player structure
 */
static void DrawCompanyOwnerText(const Player *p)
{
	const Player *p2;
	uint num = 0;
	const byte height = GetCharacterHeight(FS_NORMAL);

	FOR_ALL_PLAYERS(p2) {
		uint amt = GetAmountOwnedBy(p, p2->index);
		if (amt != 0) {
			SetDParam(0, amt * 25);
			SetDParam(1, p2->index);

			DrawString(120, (num++) * height + 116, STR_707D_OWNED_BY, TC_FROMSTRING);
		}
	}
}

/**
 * Player company window event definition
 *
 * @param w window pointer
 * @param e event been triggered
 */
static void PlayerCompanyWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			const Player *p = GetPlayer((PlayerID)w->window_number);
			bool local = w->window_number == _local_player;

			w->SetWidgetHiddenState(PCW_WIDGET_NEW_FACE,       !local);
			w->SetWidgetHiddenState(PCW_WIDGET_COLOR_SCHEME,   !local);
			w->SetWidgetHiddenState(PCW_WIDGET_PRESIDENT_NAME, !local);
			w->SetWidgetHiddenState(PCW_WIDGET_COMPANY_NAME,   !local);
			w->widget[PCW_WIDGET_BUILD_VIEW_HQ].data = (local && p->location_of_house == 0) ? STR_706F_BUILD_HQ : STR_7072_VIEW_HQ;
			if (local && p->location_of_house != 0) w->widget[PCW_WIDGET_BUILD_VIEW_HQ].type = WWT_PUSHTXTBTN; //HQ is already built.
			w->SetWidgetDisabledState(PCW_WIDGET_BUILD_VIEW_HQ, !local && p->location_of_house == 0);
			w->SetWidgetHiddenState(PCW_WIDGET_RELOCATE_HQ,      !local || p->location_of_house == 0);
			w->SetWidgetHiddenState(PCW_WIDGET_BUY_SHARE,        local);
			w->SetWidgetHiddenState(PCW_WIDGET_SELL_SHARE,       local);
			w->SetWidgetHiddenState(PCW_WIDGET_COMPANY_PASSWORD, !local || !_networking);

			if (!local) {
				if (_patches.allow_shares) { // Shares are allowed
					/* If all shares are owned by someone (none by nobody), disable buy button */
					w->SetWidgetDisabledState(PCW_WIDGET_BUY_SHARE, GetAmountOwnedBy(p, PLAYER_SPECTATOR) == 0 ||
							/* Only 25% left to buy. If the player is human, disable buying it up.. TODO issues! */
							(GetAmountOwnedBy(p, PLAYER_SPECTATOR) == 1 && !p->is_ai) ||
							/* Spectators cannot do anything of course */
							_local_player == PLAYER_SPECTATOR);

					/* If the player doesn't own any shares, disable sell button */
					w->SetWidgetDisabledState(PCW_WIDGET_SELL_SHARE, (GetAmountOwnedBy(p, _local_player) == 0) ||
							/* Spectators cannot do anything of course */
							_local_player == PLAYER_SPECTATOR);
				} else { // Shares are not allowed, disable buy/sell buttons
					w->DisableWidget(PCW_WIDGET_BUY_SHARE);
					w->DisableWidget(PCW_WIDGET_SELL_SHARE);
				}
			}

			SetDParam(0, p->index);
			SetDParam(1, p->index);

			DrawWindowWidgets(w);

			/* Player face */
			DrawPlayerFace(p->face, p->player_color, 2, 16);

			/* "xxx (Manager)" */
			SetDParam(0, p->index);
			DrawStringMultiCenter(48, 141, STR_7037_PRESIDENT, 94);

			/* "Inaugurated:" */
			SetDParam(0, p->inaugurated_year);
			DrawString(110, 23, STR_7038_INAUGURATED, TC_FROMSTRING);

			/* "Colour scheme:" */
			DrawString(110, 43, STR_7006_COLOR_SCHEME, TC_FROMSTRING);
			/* Draw company-colour bus */
			DrawSprite(SPR_VEH_BUS_SW_VIEW, PLAYER_SPRITE_COLOR(p->index), 215, 44);

			/* "Vehicles:" */
			DrawPlayerVehiclesAmount((PlayerID)w->window_number);

			/* "Company value:" */
			SetDParam(0, CalculateCompanyValue(p));
			DrawString(110, 106, STR_7076_COMPANY_VALUE, TC_FROMSTRING);

			/* Shares list */
			DrawCompanyOwnerText(p);

			break;
		}

		case WE_CLICK:
			switch (e->we.click.widget) {
				case PCW_WIDGET_NEW_FACE: DoSelectPlayerFace((PlayerID)w->window_number, false); break;

				case PCW_WIDGET_COLOR_SCHEME: {
					Window *wf = AllocateWindowDescFront(_loaded_newgrf_features.has_2CC ? &_select_player_livery_2cc_desc : &_select_player_livery_desc, w->window_number);
					if (wf != NULL) {
						wf->caption_color = wf->window_number;
						WP(wf, livery_d).livery_class = LC_OTHER;
						WP(wf, livery_d).sel = 1;
						wf->LowerWidget(2);
					}
					break;
				}

				case PCW_WIDGET_PRESIDENT_NAME: {
					const Player *p = GetPlayer((PlayerID)w->window_number);
					WP(w, def_d).byte_1 = 0;
					SetDParam(0, p->index);
					ShowQueryString(STR_PLAYER_NAME, STR_700B_PRESIDENT_S_NAME, 31, 94, w, CS_ALPHANUMERAL);
					break;
				}

				case PCW_WIDGET_COMPANY_NAME: {
					Player *p = GetPlayer((PlayerID)w->window_number);
					WP(w, def_d).byte_1 = 1;
					SetDParam(0, p->index);
					ShowQueryString(STR_COMPANY_NAME, STR_700A_COMPANY_NAME, 31, 150, w, CS_ALPHANUMERAL);
					break;
				}

				case PCW_WIDGET_BUILD_VIEW_HQ: {
					TileIndex tile = GetPlayer((PlayerID)w->window_number)->location_of_house;
					if (tile == 0) {
						if ((byte)w->window_number != _local_player)
							return;
						SetObjectToPlaceWnd(SPR_CURSOR_HQ, PAL_NONE, VHM_RECT, w);
						SetTileSelectSize(2, 2);
						w->LowerWidget(PCW_WIDGET_BUILD_VIEW_HQ);
						InvalidateWidget(w, PCW_WIDGET_BUILD_VIEW_HQ);
					} else {
						ScrollMainWindowToTile(tile);
					}
					break;
				}

				case PCW_WIDGET_RELOCATE_HQ:
					SetObjectToPlaceWnd(SPR_CURSOR_HQ, PAL_NONE, VHM_RECT, w);
					SetTileSelectSize(2, 2);
					w->LowerWidget(PCW_WIDGET_RELOCATE_HQ);
					InvalidateWidget(w, PCW_WIDGET_RELOCATE_HQ);
					break;

				case PCW_WIDGET_BUY_SHARE:
					DoCommandP(0, w->window_number, 0, NULL, CMD_BUY_SHARE_IN_COMPANY | CMD_MSG(STR_707B_CAN_T_BUY_25_SHARE_IN_THIS));
					break;

				case PCW_WIDGET_SELL_SHARE:
					DoCommandP(0, w->window_number, 0, NULL, CMD_SELL_SHARE_IN_COMPANY | CMD_MSG(STR_707C_CAN_T_SELL_25_SHARE_IN));
					break;

#ifdef ENABLE_NETWORK
				case PCW_WIDGET_COMPANY_PASSWORD:
					if (w->window_number == _local_player) ShowNetworkCompanyPasswordWindow();
					break;
#endif /* ENABLE_NETWORK */
			}
			break;

		case WE_MOUSELOOP:
			/* redraw the window every now and then */
			if ((++w->vscroll.pos & 0x1F) == 0) SetWindowDirty(w);
			break;

		case WE_PLACE_OBJ:
			if (DoCommandP(e->we.place.tile, 0, 0, NULL, CMD_BUILD_COMPANY_HQ | CMD_NO_WATER | CMD_MSG(STR_7071_CAN_T_BUILD_COMPANY_HEADQUARTERS)))
				ResetObjectToPlace();
				w->widget[PCW_WIDGET_BUILD_VIEW_HQ].type = WWT_PUSHTXTBTN; // this button can now behave as a normal push button
				RaiseWindowButtons(w);
			break;

		case WE_ABORT_PLACE_OBJ:
			RaiseWindowButtons(w);
			break;

		case WE_DESTROY:
			DeleteWindowById(WC_PLAYER_FACE, w->window_number);
			if (w->window_number == _local_player) DeleteWindowById(WC_COMPANY_PASSWORD_WINDOW, 0);
			break;

		case WE_ON_EDIT_TEXT:
			if (StrEmpty(e->we.edittext.str)) return;

			_cmd_text = e->we.edittext.str;
			switch (WP(w, def_d).byte_1) {
				case 0: /* Change president name */
					DoCommandP(0, 0, 0, NULL, CMD_CHANGE_PRESIDENT_NAME | CMD_MSG(STR_700D_CAN_T_CHANGE_PRESIDENT));
					break;
				case 1: /* Change company name */
					DoCommandP(0, 0, 0, NULL, CMD_CHANGE_COMPANY_NAME | CMD_MSG(STR_700C_CAN_T_CHANGE_COMPANY_NAME));
					break;
			}
			break;
	}
}


static const WindowDesc _player_company_desc = {
	WDP_AUTO, WDP_AUTO, 360, 170, 360, 170,
	WC_COMPANY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_player_company_widgets,
	PlayerCompanyWndProc
};

void ShowPlayerCompany(PlayerID player)
{
	Window *w;

	if (!IsValidPlayer(player)) return;

	w = AllocateWindowDescFront(&_player_company_desc, player);
	if (w != NULL) w->caption_color = w->window_number;
}



static void BuyCompanyWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		Player *p = GetPlayer((PlayerID)w->window_number);
		SetDParam(0, STR_COMPANY_NAME);
		SetDParam(1, p->index);
		DrawWindowWidgets(w);

		DrawPlayerFace(p->face, p->player_color, 2, 16);

		SetDParam(0, p->index);
		SetDParam(1, p->bankrupt_value);
		DrawStringMultiCenter(214, 65, STR_705B_WE_ARE_LOOKING_FOR_A_TRANSPORT, 238);
		break;
	}

	case WE_CLICK:
		switch (e->we.click.widget) {
		case 3:
			DeleteWindow(w);
			break;
		case 4: {
			DoCommandP(0, w->window_number, 0, NULL, CMD_BUY_COMPANY | CMD_MSG(STR_7060_CAN_T_BUY_COMPANY));
			break;
		}
		}
		break;
	}
}

static const Widget _buy_company_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     5,     0,    10,     0,    13, STR_00C5,              STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     5,    11,   333,     0,    13, STR_00B3_MESSAGE_FROM, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     5,     0,   333,    14,   136, 0x0,                   STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,     5,   148,   207,   117,   128, STR_00C9_NO,           STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,     5,   218,   277,   117,   128, STR_00C8_YES,          STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _buy_company_desc = {
	153, 171, 334, 137, 334, 137,
	WC_BUY_COMPANY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_buy_company_widgets,
	BuyCompanyWndProc
};


void ShowBuyCompanyDialog(uint player)
{
	AllocateWindowDescFront(&_buy_company_desc, player);
}

/********** HIGHSCORE and ENDGAME windows */

/* Always draw a maximized window and within there the centered background */
static void SetupHighScoreEndWindow(Window *w, uint *x, uint *y)
{
	uint i;
	/* resize window to "full-screen" */
	w->width = _screen.width;
	w->height = _screen.height;
	w->widget[0].right = w->width - 1;
	w->widget[0].bottom = w->height - 1;

	DrawWindowWidgets(w);

	/* Center Highscore/Endscreen background */
	*x = max(0, (_screen.width  / 2) - (640 / 2));
	*y = max(0, (_screen.height / 2) - (480 / 2));
	for (i = 0; i < 10; i++) // the image is split into 10 50px high parts
		DrawSprite(WP(w, highscore_d).background_img + i, PAL_NONE, *x, *y + (i * 50));
}

extern StringID EndGameGetPerformanceTitleFromValue(uint value);

/** End game window shown at the end of the game */
static void EndGameWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const Player *p;
		uint x, y;

		SetupHighScoreEndWindow(w, &x, &y);

		if (!IsValidPlayer(_local_player)) break;

		p = GetPlayer(_local_player);
		/* We need to get performance from last year because the image is shown
		 * at the start of the new year when these things have already been copied */
		if (WP(w, highscore_d).background_img == SPR_TYCOON_IMG2_BEGIN) { // Tycoon of the century \o/
			SetDParam(0, p->index);
			SetDParam(1, p->index);
			SetDParam(2, EndGameGetPerformanceTitleFromValue(p->old_economy[0].performance_history));
			DrawStringMultiCenter(x + (640 / 2), y + 107, STR_021C_OF_ACHIEVES_STATUS, 640);
		} else {
			SetDParam(0, p->index);
			SetDParam(1, EndGameGetPerformanceTitleFromValue(p->old_economy[0].performance_history));
			DrawStringMultiCenter(x + (640 / 2), y + 157, STR_021B_ACHIEVES_STATUS, 640);
		}
	} break;
	case WE_CLICK: /* Close the window (and show the highscore window) */
		DeleteWindow(w);
		break;
	case WE_DESTROY: /* Show the highscore window when this one is closed */
		if (!_networking) DoCommandP(0, 0, 0, NULL, CMD_PAUSE); // unpause
		ShowHighscoreTable(w->window_number, WP(w, highscore_d).rank);
		break;
	}
}

static void HighScoreWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const HighScore *hs = _highscore_table[w->window_number];
		uint x, y;
		uint8 i;

		SetupHighScoreEndWindow(w, &x, &y);

		SetDParam(0, _patches.ending_year);
		SetDParam(1, w->window_number + STR_6801_EASY);
		DrawStringMultiCenter(x + (640 / 2), y + 62, !_networking ? STR_0211_TOP_COMPANIES_WHO_REACHED : STR_TOP_COMPANIES_NETWORK_GAME, 500);

		/* Draw Highscore peepz */
		for (i = 0; i < lengthof(_highscore_table[0]); i++) {
			SetDParam(0, i + 1);
			DrawString(x + 40, y + 140 + (i * 55), STR_0212, TC_BLACK);

			if (hs[i].company[0] != '\0') {
				TextColour colour = (WP(w, highscore_d).rank == (int8)i) ? TC_RED : TC_BLACK; // draw new highscore in red

				DoDrawString(hs[i].company, x + 71, y + 140 + (i * 55), colour);
				SetDParam(0, hs[i].title);
				SetDParam(1, hs[i].score);
				DrawString(x + 71, y + 160 + (i * 55), STR_HIGHSCORE_STATS, colour);
			}
		}
	} break;

	case WE_CLICK: /* Onclick to close window, and in destroy event handle the rest */
		DeleteWindow(w);
		break;

	case WE_DESTROY: /* Get back all the hidden windows */
		if (_game_mode != GM_MENU) ShowVitalWindows();

		if (!_networking) DoCommandP(0, 0, 0, NULL, CMD_PAUSE); // unpause
		break;
	}
	}

static const Widget _highscore_widgets[] = {
{      WWT_PANEL, RESIZE_NONE, 16, 0, 640, 0, 480, 0x0, STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _highscore_desc = {
	0, 0, 641, 481, 641, 481,
	WC_HIGHSCORE, WC_NONE,
	0,
	_highscore_widgets,
	HighScoreWndProc
};

static const WindowDesc _endgame_desc = {
	0, 0, 641, 481, 641, 481,
	WC_ENDSCREEN, WC_NONE,
	0,
	_highscore_widgets,
	EndGameWndProc
};

/** Show the highscore table for a given difficulty. When called from
 * endgame ranking is set to the top5 element that was newly added
 * and is thus highlighted */
void ShowHighscoreTable(int difficulty, int8 ranking)
{
	Window *w;

	/* pause game to show the chart */
	if (!_networking) DoCommandP(0, 1, 0, NULL, CMD_PAUSE);

	/* Close all always on-top windows to get a clean screen */
	if (_game_mode != GM_MENU) HideVitalWindows();

	DeleteWindowByClass(WC_HIGHSCORE);
	w = AllocateWindowDesc(&_highscore_desc);

	if (w != NULL) {
		MarkWholeScreenDirty();
		w->window_number = difficulty; // show highscore chart for difficulty...
		WP(w, highscore_d).background_img = SPR_HIGHSCORE_CHART_BEGIN; // which background to show
		WP(w, highscore_d).rank = ranking;
	}
}

/** Show the endgame victory screen in 2050. Update the new highscore
 * if it was high enough */
void ShowEndGameChart()
{
	Window *w;

	/* Dedicated server doesn't need the highscore window */
	if (_network_dedicated) return;
	/* Pause in single-player to have a look at the highscore at your own leisure */
	if (!_networking) DoCommandP(0, 1, 0, NULL, CMD_PAUSE);

	HideVitalWindows();
	DeleteWindowByClass(WC_ENDSCREEN);
	w = AllocateWindowDesc(&_endgame_desc);

	if (w != NULL) {
		MarkWholeScreenDirty();

		WP(w, highscore_d).background_img = SPR_TYCOON_IMG1_BEGIN;

		if (_local_player != PLAYER_SPECTATOR) {
			const Player *p = GetPlayer(_local_player);
			if (p->old_economy[0].performance_history == SCORE_MAX)
				WP(w, highscore_d).background_img = SPR_TYCOON_IMG2_BEGIN;
		}

		/* In a network game show the endscores of the custom difficulty 'network' which is the last one
		 * as well as generate a TOP5 of that game, and not an all-time top5. */
		if (_networking) {
			w->window_number = lengthof(_highscore_table) - 1;
			WP(w, highscore_d).rank = SaveHighScoreValueNetwork();
		} else {
			/* in single player _local player is always valid */
			const Player *p = GetPlayer(_local_player);
			w->window_number = _opt.diff_level;
			WP(w, highscore_d).rank = SaveHighScoreValue(p);
		}
	}
}
