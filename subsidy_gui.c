/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "functions.h"
#include "window.h"
#include "station.h"
#include "industry.h"
#include "town.h"
#include "player.h"
#include "gfx.h"
#include "economy.h"
#include "variables.h"
#include "date.h"

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

	/* determine from coordinate for subsidy and try to scroll to it */
	offs = s->from;
	if (s->age >= 12) {
		xy = GetStation(offs)->xy;
	} else if (s->cargo_type == CT_PASSENGERS || s->cargo_type == CT_MAIL) {
		xy = GetTown(offs)->xy;
	} else {
		xy = GetIndustry(offs)->xy;

	}
	if (!ScrollMainWindowToTile(xy)) {
		/* otherwise determine to coordinate for subsidy and scroll to it */
		offs = s->to;
		if (s->age >= 12) {
			xy = GetStation(offs)->xy;
		} else if (s->cargo_type == CT_PASSENGERS || s->cargo_type == CT_MAIL || s->cargo_type == CT_GOODS || s->cargo_type == CT_FOOD) {
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

	y = 15;
	x = 1;
	DrawString(x, y, STR_2026_SUBSIDIES_ON_OFFER_FOR, 0);
	y += 10;
	num = 0;

	for (s = _subsidies; s != endof(_subsidies); s++) {
		if (s->cargo_type != CT_INVALID && s->age < 12) {
			int x2;

			SetupSubsidyDecodeParam(s, 1);
			x2 = DrawString(x + 2, y, STR_2027_FROM_TO, 0);

			SetDParam(0, _date - ymd.day + 384 - s->age * 32);
			DrawString(x2, y, STR_2028_BY, 0);
			y += 10;
			num++;
		}
	}

	if (num == 0) {
		DrawString(x + 2, y, STR_202A_NONE, 0);
		y += 10;
	}

	DrawString(x, y + 1, STR_202B_SERVICES_ALREADY_SUBSIDISED, 0);
	y += 10;
	num = 0;

	for (s = _subsidies; s != endof(_subsidies); s++) {
		if (s->cargo_type != CT_INVALID && s->age >= 12) {
			const Player *p;
			int xt;

			SetupSubsidyDecodeParam(s, 1);

			p = GetPlayer(GetStation(s->to)->owner);
			SetDParam(3, p->name_1);
			SetDParam(4, p->name_2);

			xt = DrawString(x + 2, y, STR_202C_FROM_TO, 0);

			SetDParam(0, _date - ymd.day + 768 - s->age * 32);
			DrawString(xt, y, STR_202D_UNTIL, 0);
			y += 10;
			num++;
		}
	}

	if (num == 0) DrawString(x + 2, y, STR_202A_NONE, 0);
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
{   WWT_CLOSEBOX,   RESIZE_NONE, 13,   0,  10,   0,  13, STR_00C5,           STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE, 13,  11, 617,   0,  13, STR_2025_SUBSIDIES, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE, 13, 618, 629,   0,  13, STR_NULL,           STR_STICKY_BUTTON},
{      WWT_PANEL,   RESIZE_NONE, 13,   0, 629,  14, 126, 0x0,                STR_01FD_CLICK_ON_SERVICE_TO_CENTER},
{   WIDGETS_END},
};

static const WindowDesc _subsidies_list_desc = {
	-1, -1, 630, 127,
	WC_SUBSIDIES_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_subsidies_list_widgets,
	SubsidiesListWndProc
};


void ShowSubsidiesList(void)
{
	AllocateWindowDescFront(&_subsidies_list_desc, 0);
}
