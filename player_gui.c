/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "functions.h"
#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "player.h"
#include "command.h"
#include "vehicle.h"
#include "economy.h"
#include "network.h"
#include "variables.h"
#include "train.h"

#ifdef ENABLE_NETWORK
#include "network_data.h"
#include "network_client.h"
#endif

static void DoShowPlayerFinances(PlayerID player, bool show_small, bool show_stickied);


static void DrawPlayerEconomyStats(const Player *p, byte mode)
{
	int x,y,i,j,year;
	const int64 (*tbl)[13];
	int64 sum, cost;
	StringID str;

	if (!(mode & 1)) { // normal sized economics window (mode&1) is minimized status
		/* draw categories */
		DrawStringCenterUnderline(61, 15, STR_700F_EXPENDITURE_INCOME, 0);
		for(i=0; i!=13; i++)
			DrawString(2, 27 + i*10, STR_7011_CONSTRUCTION + i, 0);
		DrawStringRightAligned(111, 27 + 10*13 + 2, STR_7020_TOTAL, 0);

		/* draw the price columns */
		year = _cur_year - 2;
		j = 3;
		x = 215;
		tbl = p->yearly_expenses + 2;
		do {
			if (year >= p->inaugurated_year) {
				SetDParam(0, year + 1920);
				DrawStringCenterUnderline(x-17, 15, STR_7010, 0);
				sum = 0;
				for(i=0; i!=13; i++) {
					/* draw one row in the price column */
					cost = (*tbl)[i];
					if (cost != 0) {
						sum += cost;

						str = STR_701E;
						if (cost < 0) { cost = -cost; str++; }
						SetDParam64(0, cost);
						DrawStringRightAligned(x, 27+i*10, str, 0);
					}
				}

				str = STR_701E;
				if (sum < 0) { sum = -sum; str++; }
				SetDParam64(0, sum);
				DrawStringRightAligned(x, 27 + 13*10 + 2, str, 0);

				GfxFillRect(x - 75, 27 + 10*13, x, 27 + 10*13, 215);
				x += 95;
			}
			year++;
			tbl--;
		} while (--j != 0);

		y = 171;

		// draw max loan aligned to loan below (y += 10)
		SetDParam64(0, (uint64)_economy.max_loan);
		DrawString(202, y+10, STR_MAX_LOAN, 0);

	} else
		y = 15;

	DrawString(2, y, STR_7026_BANK_BALANCE, 0);
	SetDParam64(0, p->money64);
	DrawStringRightAligned(182, y, STR_7028, 0);

	y += 10;

	DrawString(2, y, STR_7027_LOAN, 0);
	SetDParam64(0, p->current_loan);
	DrawStringRightAligned(182, y, STR_7028, 0);

	y += 12;

	GfxFillRect(182 - 75, y-2, 182, y-2, 215);

	SetDParam64(0, p->money64 - p->current_loan);
	DrawStringRightAligned(182, y, STR_7028, 0);
}

static const Widget _player_finances_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,					STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   379,     0,    13, STR_700E_FINANCES,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,   380,   394,     0,    13, 0x2AA,							STR_7075_TOGGLE_LARGE_SMALL_WINDOW},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   395,   406,     0,    13, 0x0,								STR_STICKY_BUTTON},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   406,    14,   169, 0x0,								STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   406,   170,   203, 0x0,								STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,   202,   204,   215, STR_7029_BORROW,		STR_7035_INCREASE_SIZE_OF_LOAN},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   203,   406,   204,   215, STR_702A_REPAY,		STR_7036_REPAY_PART_OF_LOAN},
{   WIDGETS_END},
};

static const Widget _other_player_finances_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,					STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   379,     0,    13, STR_700E_FINANCES,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,   380,   394,     0,    13, 0x2AA,							STR_7075_TOGGLE_LARGE_SMALL_WINDOW},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   395,   406,     0,    13, 0x0,								STR_STICKY_BUTTON},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   406,    14,   169, 0x0,								STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   406,   170,   203, 0x0,								STR_NULL},
{   WIDGETS_END},
};

static const Widget _other_player_finances_small_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,					STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   253,     0,    13, STR_700E_FINANCES,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,   254,   267,     0,    13, 0x2AA,							STR_7075_TOGGLE_LARGE_SMALL_WINDOW},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   268,   279,     0,    13, 0x0,								STR_STICKY_BUTTON},
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,								STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   279,    14,    47, 0x0,								STR_NULL},
{   WIDGETS_END},
};

static const Widget _player_finances_small_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,					STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   253,     0,    13, STR_700E_FINANCES,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,   254,   267,     0,    13, 0x2AA,							STR_7075_TOGGLE_LARGE_SMALL_WINDOW},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   268,   279,     0,    13, 0x0,								STR_STICKY_BUTTON},
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,								STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   279,    14,    47, 0x0,								STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,   139,    48,    59, STR_7029_BORROW,		STR_7035_INCREASE_SIZE_OF_LOAN},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   140,   279,    48,    59, STR_702A_REPAY,		STR_7036_REPAY_PART_OF_LOAN},
{   WIDGETS_END},
};


static void PlayerFinancesWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		PlayerID player = w->window_number;
		const Player* p = GetPlayer(player);

		w->disabled_state = p->current_loan != 0 ? 0 : (1 << 7);

		SetDParam(0, p->name_1);
		SetDParam(1, p->name_2);
		SetDParam(2, GetPlayerNameString(player, 3));
		SetDParam(4, 10000);
		DrawWindowWidgets(w);

		DrawPlayerEconomyStats(p, (byte)WP(w,def_d).data_1);
	} break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 2: {/* toggle size */
			byte mode = (byte)WP(w,def_d).data_1;
			bool stickied = !!(w->flags4 & WF_STICKY);
			PlayerID player = w->window_number;
			DeleteWindow(w);
			DoShowPlayerFinances(player, !HASBIT(mode, 0), stickied);
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
	-1,-1, 407, 216,
	WC_FINANCES,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_player_finances_widgets,
	PlayerFinancesWndProc
};

static const WindowDesc _player_finances_small_desc = {
	-1,-1, 280, 60,
	WC_FINANCES,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_player_finances_small_widgets,
	PlayerFinancesWndProc
};

static const WindowDesc _other_player_finances_desc = {
	-1,-1, 407, 204,
	WC_FINANCES,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_other_player_finances_widgets,
	PlayerFinancesWndProc
};

static const WindowDesc _other_player_finances_small_desc = {
	-1,-1, 280, 48,
	WC_FINANCES,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_other_player_finances_small_widgets,
	PlayerFinancesWndProc
};

static const WindowDesc * const desc_table[2*2] = {
	&_player_finances_desc,&_player_finances_small_desc,
	&_other_player_finances_desc,&_other_player_finances_small_desc,
};

static void DoShowPlayerFinances(PlayerID player, bool show_small, bool show_stickied)
{
	Window *w;
	int mode;

	mode = (player != _local_player) * 2 + show_small;
	w = AllocateWindowDescFront(desc_table[mode], player);
	if (w != NULL) {
		w->caption_color = w->window_number;
		WP(w,def_d).data_1 = mode;
		if (show_stickied) {
			w->flags4 |= WF_STICKY;
		}
	}
}

void ShowPlayerFinances(PlayerID player)
{
	DoShowPlayerFinances(player, false, false);
}

static void SelectPlayerColorWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		const Player* p;
		uint used_colors = 0;
		int num_free = 16;
		int x,y,pos;
		int i;

		FOR_ALL_PLAYERS(p) {
			if (p->is_active) {
				SETBIT(used_colors, p->player_color);
				num_free--;
			}
		}
		WP(w,def_d).data_1 = used_colors;
		SetVScrollCount(w, num_free);
		DrawWindowWidgets(w);

		x = 2;
		y = 17;
		pos = w->vscroll.pos;

		for(i=0; i!=16; i++) {
			if (!(used_colors & 1) && --pos < 0 && pos >= -8) {
				DrawString(x + 30, y, STR_00D1_DARK_BLUE + i, 2);
				DrawSprite((i << 16) + 0x3078C1A, x + 14, y + 4);
				y += 14;
			}
			used_colors >>= 1;
		}
	} break;

	case WE_CLICK:
		if (e->click.widget == 2) {
			int item = (e->click.pt.y - 13) / 14;
			uint used_colors;
			int i;

			if ((uint)item >= 8)
				return;
			item += w->vscroll.pos;
			used_colors = WP(w,def_d).data_1;

			for(i=0; i!=16; i++) {
				if (!(used_colors & 1) && --item < 0) {
					DoCommandP(0, 0, i, NULL, CMD_SET_PLAYER_COLOR);
					DeleteWindow(w);
					break;
				}
				used_colors >>= 1;
			}
		}
		break;
	}
}

static const Widget _select_player_color_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   149,     0,    13, STR_7007_NEW_COLOR_SCHEME, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   137,    14,   127, 0x0, STR_7034_CLICK_ON_SELECTED_NEW_COLOR},
{  WWT_SCROLLBAR,   RESIZE_NONE,    14,   138,   149,    14,   127, 0x0, STR_0190_SCROLL_BAR_SCROLLS_LIST},
{   WIDGETS_END},
};

static const WindowDesc _select_player_color_desc = {
	-1,-1, 150, 128,
	WC_PLAYER_COLOR,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_select_player_color_widgets,
	SelectPlayerColorWndProc
};

static void SelectPlayerFaceWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		Player *p;
		w->click_state = (w->click_state & ~(1<<5|1<<6)) | ((1<<5) << WP(w,facesel_d).gender);
		DrawWindowWidgets(w);
		p = GetPlayer(w->window_number);
		DrawPlayerFace(WP(w,facesel_d).face, p->player_color, 2, 16);
	} break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 3: DeleteWindow(w); break;
		case 4: /* ok click */
			DoCommandP(0, 0, WP(w,facesel_d).face, NULL, CMD_SET_PLAYER_FACE);
			DeleteWindow(w);
			break;
		case 5: /* male click */
		case 6: /* female click */
			WP(w,facesel_d).gender = e->click.widget - 5;
			SetWindowDirty(w);
			break;
		case 7:
			WP(w,facesel_d).face = (WP(w,facesel_d).gender << 31) + GB(InteractiveRandom(), 0, 31);
			SetWindowDirty(w);
			break;
		}
		break;
	}
}

static const Widget _select_player_face_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,								STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   189,     0,    13, STR_7043_FACE_SELECTION,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   189,    14,   136, 0x0,											STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    94,   137,   148, STR_012E_CANCEL,					STR_7047_CANCEL_NEW_FACE_SELECTION},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    95,   189,   137,   148, STR_012F_OK,							STR_7048_ACCEPT_NEW_FACE_SELECTION},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    95,   187,    25,    36, STR_7044_MALE,						STR_7049_SELECT_MALE_FACES},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    95,   187,    37,    48, STR_7045_FEMALE,					STR_704A_SELECT_FEMALE_FACES},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    95,   187,    79,    90, STR_7046_NEW_FACE,				STR_704B_GENERATE_RANDOM_NEW_FACE},
{   WIDGETS_END},
};

static const WindowDesc _select_player_face_desc = {
	-1,-1, 190, 149,
	WC_PLAYER_FACE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_select_player_face_widgets,
	SelectPlayerFaceWndProc
};

static const Widget _my_player_company_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   359,     0,    13, STR_7001,                STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   359,    14,   157, 0x0,                     STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    89,   158,   169, STR_7004_NEW_FACE,       STR_7030_SELECT_NEW_FACE_FOR_PRESIDENT},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    90,   179,   158,   169, STR_7005_COLOR_SCHEME,   STR_7031_CHANGE_THE_COMPANY_VEHICLE},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   180,   269,   158,   169, STR_7009_PRESIDENT_NAME, STR_7032_CHANGE_THE_PRESIDENT_S},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   270,   359,   158,   169, STR_7008_COMPANY_NAME,   STR_7033_CHANGE_THE_COMPANY_NAME},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   266,   355,    18,    29, STR_706F_BUILD_HQ,       STR_7070_BUILD_COMPANY_HEADQUARTERS},
{      WWT_EMPTY,   RESIZE_NONE,    14,     0,   355,    32,    43, 0x0,                     STR_NULL},
{      WWT_EMPTY,   RESIZE_NONE,    14,     0,   355,    32,    43, 0x0,                     STR_NULL},
{      WWT_EMPTY,   RESIZE_NONE,    14,     0,   355,    32,    43, 0x0,                     STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   266,   355,   138,   149, STR_COMPANY_PASSWORD,    STR_COMPANY_PASSWORD_TOOLTIP},
{   WIDGETS_END},
};

static const Widget _other_player_company_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,					STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   359,     0,    13, STR_7001,					STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   359,    14,   157, 0x0,								STR_NULL},
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,								STR_NULL},
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,								STR_NULL},
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,								STR_NULL},
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,								STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   266,   355,    18,    29, STR_7072_VIEW_HQ,	STR_7070_BUILD_COMPANY_HEADQUARTERS},
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,								STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,   179,   158,   169, STR_7077_BUY_25_SHARE_IN_COMPANY,	STR_7079_BUY_25_SHARE_IN_THIS_COMPANY},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   180,   359,   158,   169, STR_7078_SELL_25_SHARE_IN_COMPANY,	STR_707A_SELL_25_SHARE_IN_THIS_COMPANY},
{   WIDGETS_END},
};

static const Widget _my_player_company_bh_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   359,     0,    13, STR_7001,                STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   359,    14,   157, 0x0,                     STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    89,   158,   169, STR_7004_NEW_FACE,       STR_7030_SELECT_NEW_FACE_FOR_PRESIDENT},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,    90,   179,   158,   169, STR_7005_COLOR_SCHEME,   STR_7031_CHANGE_THE_COMPANY_VEHICLE},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   180,   269,   158,   169, STR_7009_PRESIDENT_NAME, STR_7032_CHANGE_THE_PRESIDENT_S},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   270,   359,   158,   169, STR_7008_COMPANY_NAME,   STR_7033_CHANGE_THE_COMPANY_NAME},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   266,   355,    18,    29, STR_7072_VIEW_HQ,        STR_7070_BUILD_COMPANY_HEADQUARTERS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   266,   355,    32,    43, STR_RELOCATE_HQ,         STR_RELOCATE_COMPANY_HEADQUARTERS},
{      WWT_EMPTY,   RESIZE_NONE,    14,     0,   355,    32,    43, 0x0,                     STR_NULL},
{      WWT_EMPTY,   RESIZE_NONE,    14,     0,   355,    32,    43, 0x0,                     STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   266,   355,   138,   149, STR_COMPANY_PASSWORD,    STR_COMPANY_PASSWORD_TOOLTIP},
{   WIDGETS_END},
};

static void DrawPlayerVehiclesAmount(PlayerID player)
{
	const int x = 110;
	int y = 72;
	const Vehicle* v;
	uint train = 0;
	uint road  = 0;
	uint air   = 0;
	uint ship  = 0;

	DrawString(x, y, STR_7039_VEHICLES, 0);

	FOR_ALL_VEHICLES(v) {
		if (v->owner == player) {
			switch (v->type) {
				case VEH_Train:    if (IsFrontEngine(v)) train++; break;
				case VEH_Road:     road++; break;
				case VEH_Aircraft: if (v->subtype <= 2) air++; break;
				case VEH_Ship:     ship++; break;
				default: break;
			}
		}
	}

	if (train+road+air+ship == 0) {
		DrawString(x+70, y, STR_7042_NONE, 0);
	} else {
		if (train != 0) {
			SetDParam(0, train);
			DrawString(x + 70, y, STR_TRAINS, 0);
			y += 10;
		}

		if (road != 0) {
			SetDParam(0, road);
			DrawString(x + 70, y, STR_ROAD_VEHICLES, 0);
			y += 10;
		}

		if (air != 0) {
			SetDParam(0, air);
			DrawString(x + 70, y, STR_AIRCRAFT, 0);
			y += 10;
		}

		if (ship != 0) {
			SetDParam(0, ship);
			DrawString(x + 70, y, STR_SHIPS, 0);
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

static void DrawCompanyOwnerText(const Player *p)
{
	const Player* p2;
	int num = -1;

	FOR_ALL_PLAYERS(p2) {
		uint amt = GetAmountOwnedBy(p, p2->index);
		if (amt != 0) {
			num++;

			SetDParam(num * 3 + 0, amt * 25);
			SetDParam(num * 3 + 1, p2->name_1);
			SetDParam(num * 3 + 2, p2->name_2);

			if (num != 0) break;
		}
	}

	if (num >= 0) DrawString(120, 124, STR_707D_OWNED_BY + num, 0);
}

static void PlayerCompanyWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const Player* p = GetPlayer(w->window_number);
		uint32 dis = 0;

		if (!IsWindowOfPrototype(w, _other_player_company_widgets)) {
			AssignWidgetToWindow(w, (p->location_of_house != 0) ? _my_player_company_bh_widgets : _my_player_company_widgets);

			if (!_networking) SETBIT(w->hidden_state, 11); // hide company-password widget
		} else {
			if (p->location_of_house == 0) SETBIT(dis, 7);

			if (_patches.allow_shares) { /* shares are allowed */
				/* If all shares are owned by someone (none by nobody), disable buy button */
				if (GetAmountOwnedBy(p, OWNER_SPECTATOR) == 0) SETBIT(dis, 9);

				/* Only 25% left to buy. If the player is human, disable buying it up.. TODO issues! */
				if (GetAmountOwnedBy(p, OWNER_SPECTATOR) == 1 && !p->is_ai) SETBIT(dis, 9);

				/* If the player doesn't own any shares, disable sell button */
				if (GetAmountOwnedBy(p, _local_player) == 0) SETBIT(dis, 10);

				/* Spectators cannot do anything of course */
				if (_local_player == OWNER_SPECTATOR) dis |= (1 << 9) | (1 << 10);
			} else /* shares are not allowed, disable buy/sell buttons */
				dis |= (1 << 9) | (1 << 10);
		}

		SetDParam(0, p->name_1);
		SetDParam(1, p->name_2);
		SetDParam(2, GetPlayerNameString((byte)w->window_number, 3));

		w->disabled_state = dis;
		DrawWindowWidgets(w);

		SetDParam(0, p->inaugurated_year + 1920);
		DrawString(110, 25, STR_7038_INAUGURATED, 0);

		DrawPlayerVehiclesAmount(w->window_number);

		DrawString(110,48, STR_7006_COLOR_SCHEME, 0);
		// Draw company-colour bus (0xC19)
		DrawSprite(PLAYER_SPRITE_COLOR(p->index) + (0xC19 | PALETTE_MODIFIER_COLOR), 215, 49);

		DrawPlayerFace(p->face, p->player_color, 2, 16);

		SetDParam(0, p->president_name_1);
		SetDParam(1, p->president_name_2);
		DrawStringMultiCenter(48, 141, STR_7037_PRESIDENT, 94);

		SetDParam64(0, CalculateCompanyValue(p));
		DrawString(110, 114, STR_7076_COMPANY_VALUE, 0);

		DrawCompanyOwnerText(p);
	} break;

	case WE_CLICK:
		switch (e->click.widget) {
		case 3: { /* select face */
			Window *wf = AllocateWindowDescFront(&_select_player_face_desc, w->window_number);
			if (wf != NULL) {
				wf->caption_color = w->window_number;
				WP(wf,facesel_d).face = GetPlayer(wf->window_number)->face;
				WP(wf,facesel_d).gender = 0;
			}
		} break;

		case 4: {/* change color */
			Window *wf = AllocateWindowDescFront(&_select_player_color_desc,w->window_number);
			if (wf != NULL) {
				wf->caption_color = wf->window_number;
				wf->vscroll.cap = 8;
			}
		} break;

		case 5: {/* change president name */
			const Player* p = GetPlayer(w->window_number);
			WP(w, def_d).byte_1 = 0;
			SetDParam(0, p->president_name_2);
			ShowQueryString(p->president_name_1, STR_700B_PRESIDENT_S_NAME, 31, 94, w->window_class, w->window_number);
		} break;

		case 6: {/* change company name */
			Player *p = GetPlayer(w->window_number);
			WP(w,def_d).byte_1 = 1;
			SetDParam(0, p->name_2);
			ShowQueryString(p->name_1, STR_700A_COMPANY_NAME, 31, 150, w->window_class, w->window_number);
		} break;

		case 7: {/* build hq */
			TileIndex tile = GetPlayer(w->window_number)->location_of_house;
			if (tile == 0) {
				if ((byte)w->window_number != _local_player)
					return;
				SetObjectToPlaceWnd(SPR_CURSOR_HQ, 1, w);
				SetTileSelectSize(2, 2);
			} else {
				ScrollMainWindowToTile(tile);
			}
		} break;

		case 8: /* relocate HQ */
			SetObjectToPlaceWnd(SPR_CURSOR_HQ, 1, w);
			SetTileSelectSize(2, 2);
			break;
		case 9: /* buy 25% */
			DoCommandP(0, w->window_number, 0, NULL, CMD_BUY_SHARE_IN_COMPANY | CMD_MSG(STR_707B_CAN_T_BUY_25_SHARE_IN_THIS));
			break;

		case 10: /* sell 25% */
			DoCommandP(0, w->window_number, 0, NULL, CMD_SELL_SHARE_IN_COMPANY | CMD_MSG(STR_707C_CAN_T_SELL_25_SHARE_IN));
			break;
		case 11: { /* Password protect company */
			#ifdef ENABLE_NETWORK
			if (!IsWindowOfPrototype(w, _other_player_company_widgets)) {
				WP(w,def_d).byte_1 = 2;
				ShowQueryString(BindCString(_network_player_info[_local_player].password),
					STR_SET_COMPANY_PASSWORD, sizeof(_network_player_info[_local_player].password), 250, w->window_class, w->window_number);
			}
			#endif
		}	break;
		}

	case WE_MOUSELOOP:
		/* redraw the window every now and then */
		if ((++w->vscroll.pos & 0x1F) == 0)
			SetWindowDirty(w);
		break;

	case WE_PLACE_OBJ: {
		if (DoCommandP(e->place.tile, 0, 0, NULL, CMD_BUILD_COMPANY_HQ | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_7071_CAN_T_BUILD_COMPANY_HEADQUARTERS)))
			ResetObjectToPlace();
		break;
	}


	case WE_DESTROY:
		DeleteWindowById(WC_PLAYER_COLOR, w->window_number);
		DeleteWindowById(WC_PLAYER_FACE, w->window_number);
		break;

	case WE_ON_EDIT_TEXT: {
		char *b = e->edittext.str;

		// empty string is allowed for password
		if (*b == '\0' && WP(w,def_d).byte_1 != 2) return;

		_cmd_text = b;
		switch (WP(w,def_d).byte_1) {
		case 0: /* Change president name */
			DoCommandP(0, 0, 0, NULL, CMD_CHANGE_PRESIDENT_NAME | CMD_MSG(STR_700D_CAN_T_CHANGE_PRESIDENT));
			break;
		case 1: /* Change company name */
			DoCommandP(0, 0, 0, NULL, CMD_CHANGE_COMPANY_NAME | CMD_MSG(STR_700C_CAN_T_CHANGE_COMPANY_NAME));
			break;
		#ifdef ENABLE_NETWORK
		case 2: /* Change company password */
			if (*b == '\0') *b = '*'; // empty password is a '*' because of console argument
			NetworkChangeCompanyPassword(1, &b);
		#endif
		}
	} break;

	}
}


static const WindowDesc _my_player_company_desc = {
	-1, -1, 360, 170,
	WC_COMPANY, 0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_my_player_company_widgets,
	PlayerCompanyWndProc
};

static const WindowDesc _other_player_company_desc = {
	-1,-1, 360, 170,
	WC_COMPANY,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_other_player_company_widgets,
	PlayerCompanyWndProc
};

void ShowPlayerCompany(PlayerID player)
{
	Window* w;

	w = AllocateWindowDescFront(player == _local_player ? &_my_player_company_desc : &_other_player_company_desc, player);
	if (w != NULL) w->caption_color = w->window_number;
}



static void BuyCompanyWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		Player *p = GetPlayer(w->window_number);
		SetDParam(0, p->name_1);
		SetDParam(1, p->name_2);
		DrawWindowWidgets(w);

		DrawPlayerFace(p->face, p->player_color, 2, 16);

		SetDParam(0, p->name_1);
		SetDParam(1, p->name_2);
		SetDParam(2, p->bankrupt_value);
		DrawStringMultiCenter(214, 65, STR_705B_WE_ARE_LOOKING_FOR_A_TRANSPORT, 238);
		break;
	}

	case WE_CLICK:
		switch(e->click.widget) {
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
{   WWT_CLOSEBOX,   RESIZE_NONE,     5,     0,    10,     0,    13, STR_00C5,							STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     5,    11,   333,     0,    13, STR_00B3_MESSAGE_FROM,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,     5,     0,   333,    14,   136, 0x0,										STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,     5,   148,   207,   117,   128, STR_00C9_NO,						STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,     5,   218,   277,   117,   128, STR_00C8_YES,					STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _buy_company_desc = {
	153,171, 334, 137,
	WC_BUY_COMPANY,0,
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
	// resize window to "full-screen"
	w->width = _screen.width;
	w->height = _screen.height;
	w->widget[0].right = w->width - 1;
	w->widget[0].bottom = w->height - 1;

	DrawWindowWidgets(w);

	/* Center Highscore/Endscreen background */
	*x = max(0, (_screen.width  / 2) - (640 / 2));
	*y = max(0, (_screen.height / 2) - (480 / 2));
	for (i = 0; i < 10; i++) // the image is split into 10 50px high parts
		DrawSprite(WP(w, highscore_d).background_img + i, *x, *y + (i * 50));
}

extern StringID EndGameGetPerformanceTitleFromValue(uint value);

/* End game window shown at the end of the game */
static void EndGameWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const Player *p = GetPlayer(_local_player);
		uint x, y;

		SetupHighScoreEndWindow(w, &x, &y);

		/* We need to get performance from last year because the image is shown
		 * at the start of the new year when these things have already been copied */
		if (WP(w, highscore_d).background_img == SPR_TYCOON_IMG2_BEGIN) { // Tycoon of the century \o/
			SetDParam(0, p->president_name_1);
			SetDParam(1, p->president_name_2);
			SetDParam(2, p->name_1);
			SetDParam(3, p->name_2);
			SetDParam(4, EndGameGetPerformanceTitleFromValue(p->old_economy[0].performance_history));
			DrawStringMultiCenter(x + (640 / 2), y + 107, STR_021C_OF_ACHIEVES_STATUS, 640);
		} else {
			SetDParam(0, p->name_1);
			SetDParam(1, p->name_2);
			SetDParam(2, EndGameGetPerformanceTitleFromValue(p->old_economy[0].performance_history));
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

		SetDParam(0, _patches.ending_date);
		SetDParam(1, w->window_number + STR_6801_EASY);
		DrawStringMultiCenter(x + (640 / 2), y + 62, !_networking ? STR_0211_TOP_COMPANIES_WHO_REACHED : STR_TOP_COMPANIES_NETWORK_GAME, 500);

		/* Draw Highscore peepz */
		for (i = 0; i < lengthof(_highscore_table[0]); i++) {
			SetDParam(0, i + 1);
			DrawString(x + 40, y + 140 + (i * 55), STR_0212, 0x10);

			if (hs[i].company[0] != '\0') {
				uint16 colour = (WP(w, highscore_d).rank == (int8)i) ? 0x3 : 0x10; // draw new highscore in red

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
	0, 0, 641, 481,
	WC_HIGHSCORE,0,
	0,
	_highscore_widgets,
	HighScoreWndProc
};

static const WindowDesc _endgame_desc = {
	0, 0, 641, 481,
	WC_ENDSCREEN,0,
	0,
	_highscore_widgets,
	EndGameWndProc
};

/* Show the highscore table for a given difficulty. When called from
 * endgame ranking is set to the top5 element that was newly added
 * and is thus highlighted */
void ShowHighscoreTable(int difficulty, int8 ranking)
{
	Window *w;

	// pause game to show the chart
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

/* Show the endgame victory screen in 2050. Update the new highscore
 * if it was high enough */
void ShowEndGameChart(void)
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

		if (_local_player != OWNER_SPECTATOR) {
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
			// in single player _local player is always valid
			const Player *p = GetPlayer(_local_player);
			w->window_number = _opt.diff_level;
			WP(w, highscore_d).rank = SaveHighScoreValue(p);
		}
	}
}
