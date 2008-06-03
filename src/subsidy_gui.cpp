/* $Id$ */

/** @file subsidy_gui.cpp GUI for subsidies. */

#include "stdafx.h"
#include "openttd.h"
#include "station_base.h"
#include "industry.h"
#include "town.h"
#include "economy_func.h"
#include "variables.h"
#include "cargotype.h"
#include "window_gui.h"
#include "strings_func.h"
#include "date_func.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "gui.h"

#include "table/strings.h"

struct SubsidyListWindow : Window {
	SubsidyListWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget != 3) return;

		int y = pt.y - 25;

		if (y < 0) return;

		uint num = 0;
		for (const Subsidy *s = _subsidies; s != endof(_subsidies); s++) {
			if (s->cargo_type != CT_INVALID && s->age < 12) {
				y -= 10;
				if (y < 0) {
					this->HandleClick(s);
					return;
				}
				num++;
			}
		}

		if (num == 0) {
			y -= 10; // "None"
			if (y < 0) return;
		}

		y -= 11; // "Services already subsidised:"
		if (y < 0) return;

		for (const Subsidy *s = _subsidies; s != endof(_subsidies); s++) {
			if (s->cargo_type != CT_INVALID && s->age >= 12) {
				y -= 10;
				if (y < 0) {
					this->HandleClick(s);
					return;
				}
			}
		}
	}

	void HandleClick(const Subsidy *s)
	{
		TownEffect te = GetCargo(s->cargo_type)->town_effect;
		TileIndex xy;

		/* determine from coordinate for subsidy and try to scroll to it */
		uint offs = s->from;
		if (s->age >= 12) {
			xy = GetStation(offs)->xy;
		} else if (te == TE_PASSENGERS || te == TE_MAIL) {
			xy = GetTown(offs)->xy;
		} else {
			xy = GetIndustry(offs)->xy;
		}

		if (_ctrl_pressed || !ScrollMainWindowToTile(xy)) {
			if (_ctrl_pressed) ShowExtraViewPortWindow(xy);

			/* otherwise determine to coordinate for subsidy and scroll to it */
			offs = s->to;
			if (s->age >= 12) {
				xy = GetStation(offs)->xy;
			} else if (te == TE_PASSENGERS || te == TE_MAIL || te == TE_GOODS || te == TE_FOOD) {
				xy = GetTown(offs)->xy;
			} else {
				xy = GetIndustry(offs)->xy;
			}

			if (_ctrl_pressed) {
				ShowExtraViewPortWindow(xy);
			} else {
				ScrollMainWindowToTile(xy);
			}
		}
	}

	virtual void OnPaint()
	{
		YearMonthDay ymd;
		const Subsidy *s;

		this->DrawWidgets();

		ConvertDateToYMD(_date, &ymd);

		int width = this->width - 13;  // scroll bar = 11 + pixel each side
		int y = 15;
		int x = 1;

		/* Section for drawing the offered subisidies */
		DrawStringTruncated(x, y, STR_2026_SUBSIDIES_ON_OFFER_FOR, TC_FROMSTRING, width);
		y += 10;
		uint num = 0;

		for (s = _subsidies; s != endof(_subsidies); s++) {
			if (s->cargo_type != CT_INVALID && s->age < 12) {
				int x2;

				/* Displays the two offered towns */
				SetupSubsidyDecodeParam(s, 1);
				x2 = DrawStringTruncated(x + 2, y, STR_2027_FROM_TO, TC_FROMSTRING, width - 2);

				if (width - x2 > 10) {
					/* Displays the deadline before voiding the proposal */
					SetDParam(0, _date - ymd.day + 384 - s->age * 32);
					DrawStringTruncated(x2, y, STR_2028_BY, TC_FROMSTRING, width - x2);
				}

				y += 10;
				num++;
			}
		}

		if (num == 0) {
			DrawStringTruncated(x + 2, y, STR_202A_NONE, TC_FROMSTRING, width - 2);
			y += 10;
		}

		/* Section for drawing the already granted subisidies */
		DrawStringTruncated(x, y + 1, STR_202B_SERVICES_ALREADY_SUBSIDISED, TC_FROMSTRING, width);
		y += 10;
		num = 0;

		for (s = _subsidies; s != endof(_subsidies); s++) {
			if (s->cargo_type != CT_INVALID && s->age >= 12) {
				int xt;

				SetupSubsidyDecodeParam(s, 1);

				PlayerID player = GetStation(s->to)->owner;
				SetDParam(3, player);

				/* Displays the two connected stations */
				xt = DrawStringTruncated(x + 2, y, STR_202C_FROM_TO, TC_FROMSTRING, width - 2);

				/* Displays the date where the granted subsidy will end */
				if ((xt > 3) && (width - xt) > 9 ) { // do not draw if previous drawing failed or if it will overlap on scrollbar
					SetDParam(0, _date - ymd.day + 768 - s->age * 32);
					DrawStringTruncated(xt, y, STR_202D_UNTIL, TC_FROMSTRING, width - xt);
				}
				y += 10;
				num++;
			}
		}

		if (num == 0) DrawStringTruncated(x + 2, y, STR_202A_NONE, TC_FROMSTRING, width - 2);
	}
};

static const Widget _subsidies_list_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE,   13,   0,  10,   0,  13, STR_00C5,           STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION, RESIZE_RIGHT,  13,  11, 307,   0,  13, STR_2025_SUBSIDIES, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX, RESIZE_LR,     13, 308, 319,   0,  13, STR_NULL,           STR_STICKY_BUTTON},
{      WWT_PANEL, RESIZE_RB,     13,   0, 307,  14, 126, 0x0,                STR_01FD_CLICK_ON_SERVICE_TO_CENTER},
{  WWT_SCROLLBAR, RESIZE_LRB,    13, 308, 319,  14, 114, 0x0,                STR_0190_SCROLL_BAR_SCROLLS_LIST},
{  WWT_RESIZEBOX, RESIZE_LRTB,   13, 308, 319, 115, 126, 0x0,                STR_RESIZE_BUTTON},

{   WIDGETS_END},
};

static const WindowDesc _subsidies_list_desc = {
	WDP_AUTO, WDP_AUTO, 320, 127, 320, 127,
	WC_SUBSIDIES_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_subsidies_list_widgets,
};


void ShowSubsidiesList()
{
	AllocateWindowDescFront<SubsidyListWindow>(&_subsidies_list_desc, 0);
}
