/* $Id$ */

/** @file subsidy_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "strings.h"
#include "functions.h"
#include "station.h"
#include "industry.h"
#include "town.h"
#include "player.h"
#include "gfx.h"
#include "economy.h"
#include "variables.h"
#include "date.h"
#include "cargotype.h"
#include "window_gui.h"

static void HandleSubsidyClick(int y)
{
	const Subsidy *s;
	uint num;
	int offs;
	TileIndex xy;

	if (y < 0) return;

	num = 0;
	for (s = _subsidies; s != endof(_subsidies); s++) {
		if (s->cargo_type != CT_INVALID && s->age < 12) {
			y -= 10;
			if (y < 0) goto handle_click;
			num++;
		}
	}

	if (num == 0) {
		y -= 10;
		if (y < 0) return;
	}

	y -= 11;
	if (y < 0) return;

	for (s = _subsidies; s != endof(_subsidies); s++) {
		if (s->cargo_type != CT_INVALID && s->age >= 12) {
			y -= 10;
			if (y < 0) goto handle_click;
		}
	}
	return;

handle_click:

	TownEffect te = GetCargo(s->cargo_type)->town_effect;

	/* determine from coordinate for subsidy and try to scroll to it */
	offs = s->from;
	if (s->age >= 12) {
		xy = GetStation(offs)->xy;
	} else if (te == TE_PASSENGERS || te == TE_MAIL) {
		xy = GetTown(offs)->xy;
	} else {
		xy = GetIndustry(offs)->xy;
	}

	if (!ScrollMainWindowToTile(xy)) {
		/* otherwise determine to coordinate for subsidy and scroll to it */
		offs = s->to;
		if (s->age >= 12) {
			xy = GetStation(offs)->xy;
		} else if (te == TE_PASSENGERS || te == TE_MAIL || te == TE_GOODS || te == TE_FOOD) {
			xy = GetTown(offs)->xy;
		} else {
			xy = GetIndustry(offs)->xy;
		}
		ScrollMainWindowToTile(xy);
	}
}

static void DrawSubsidiesWindow(const Window *w)
{
	YearMonthDay ymd;
	const Subsidy *s;
	uint num;
	int x;
	int y;

	DrawWindowWidgets(w);

	ConvertDateToYMD(_date, &ymd);

	int width = w->width - 2;
	y = 15;
	x = 1;
	DrawStringTruncated(x, y, STR_2026_SUBSIDIES_ON_OFFER_FOR, TC_FROMSTRING, width);
	y += 10;
	num = 0;

	for (s = _subsidies; s != endof(_subsidies); s++) {
		if (s->cargo_type != CT_INVALID && s->age < 12) {
			int x2;

			SetupSubsidyDecodeParam(s, 1);
			x2 = DrawStringTruncated(x + 2, y, STR_2027_FROM_TO, TC_FROMSTRING, width);

			SetDParam(0, _date - ymd.day + 384 - s->age * 32);
			DrawStringTruncated(x2, y, STR_2028_BY, TC_FROMSTRING, width - x2);
			y += 10;
			num++;
		}
	}

	if (num == 0) {
		DrawStringTruncated(x + 2, y, STR_202A_NONE, TC_FROMSTRING, width - 2);
		y += 10;
	}

	DrawStringTruncated(x, y + 1, STR_202B_SERVICES_ALREADY_SUBSIDISED, TC_FROMSTRING, width);
	y += 10;
	num = 0;

	for (s = _subsidies; s != endof(_subsidies); s++) {
		if (s->cargo_type != CT_INVALID && s->age >= 12) {
			int xt;

			SetupSubsidyDecodeParam(s, 1);

			PlayerID player = GetStation(s->to)->owner;
			SetDParam(3, player);

			xt = DrawStringTruncated(x + 2, y, STR_202C_FROM_TO, TC_FROMSTRING, width - 2);

			SetDParam(0, _date - ymd.day + 768 - s->age * 32);
			DrawStringTruncated(xt, y, STR_202D_UNTIL, TC_FROMSTRING, width - xt);
			y += 10;
			num++;
		}
	}

	if (num == 0) DrawStringTruncated(x + 2, y, STR_202A_NONE, TC_FROMSTRING, width - 2);
}

static void SubsidiesListWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: DrawSubsidiesWindow(w); break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case 3:
					HandleSubsidyClick(e->we.click.pt.y - 25);
					break;
			}
		break;
	}
}

static const Widget _subsidies_list_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE,  13,   0,  10,   0,  13, STR_00C5,           STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION, RESIZE_RIGHT, 13,  11, 307,   0,  13, STR_2025_SUBSIDIES, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX, RESIZE_LR,    13, 308, 319,   0,  13, STR_NULL,           STR_STICKY_BUTTON},
{      WWT_PANEL, RESIZE_RIGHT, 13,   0, 319,  14, 126, 0x0,                STR_01FD_CLICK_ON_SERVICE_TO_CENTER},
{   WIDGETS_END},
};

static const WindowDesc _subsidies_list_desc = {
	WDP_AUTO, WDP_AUTO, 320, 127, 630, 127,
	WC_SUBSIDIES_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_subsidies_list_widgets,
	SubsidiesListWndProc
};


void ShowSubsidiesList()
{
	AllocateWindowDescFront(&_subsidies_list_desc, 0);
}
