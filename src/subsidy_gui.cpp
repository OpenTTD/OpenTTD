/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file subsidy_gui.cpp GUI for subsidies. */

#include "stdafx.h"
#include "industry.h"
#include "town.h"
#include "window_gui.h"
#include "strings_func.h"
#include "date_func.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "gui.h"
#include "subsidy_func.h"
#include "subsidy_base.h"
#include "core/geometry_func.hpp"

#include "table/strings.h"

/** Widget numbers for the subsidy list window. */
enum SubsidyListWidgets {
	SLW_PANEL,
	SLW_SCROLLBAR,
};

struct SubsidyListWindow : Window {
	Scrollbar *vscroll;

	SubsidyListWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->CreateNestedTree(desc);
		this->vscroll = this->GetScrollbar(SLW_SCROLLBAR);
		this->FinishInitNested(desc, window_number);
		this->OnInvalidateData(0);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		if (widget != SLW_PANEL) return;

		int y = this->vscroll->GetScrolledRowFromWidget(pt.y, this, SLW_PANEL, WD_FRAMERECT_TOP);
		int num = 0;
		const Subsidy *s;
		FOR_ALL_SUBSIDIES(s) {
			if (!s->IsAwarded()) {
				y--;
				if (y == 0) {
					this->HandleClick(s);
					return;
				}
				num++;
			}
		}

		if (num == 0) {
			y--; // "None"
			if (y < 0) return;
		}

		y -= 2; // "Services already subsidised:"
		if (y < 0) return;

		FOR_ALL_SUBSIDIES(s) {
			if (s->IsAwarded()) {
				y--;
				if (y == 0) {
					this->HandleClick(s);
					return;
				}
			}
		}
	}

	void HandleClick(const Subsidy *s)
	{
		/* determine src coordinate for subsidy and try to scroll to it */
		TileIndex xy;
		switch (s->src_type) {
			case ST_INDUSTRY: xy = Industry::Get(s->src)->location.tile; break;
			case ST_TOWN:     xy =     Town::Get(s->src)->xy; break;
			default: NOT_REACHED();
		}

		if (_ctrl_pressed || !ScrollMainWindowToTile(xy)) {
			if (_ctrl_pressed) ShowExtraViewPortWindow(xy);

			/* otherwise determine dst coordinate for subsidy and scroll to it */
			switch (s->dst_type) {
				case ST_INDUSTRY: xy = Industry::Get(s->dst)->location.tile; break;
				case ST_TOWN:     xy =     Town::Get(s->dst)->xy; break;
				default: NOT_REACHED();
			}

			if (_ctrl_pressed) {
				ShowExtraViewPortWindow(xy);
			} else {
				ScrollMainWindowToTile(xy);
			}
		}
	}

	/**
	 * Count the number of lines in this window.
	 * @return the number of lines
	 */
	uint CountLines()
	{
		/* Count number of (non) awarded subsidies */
		uint num_awarded = 0;
		uint num_not_awarded = 0;
		const Subsidy *s;
		FOR_ALL_SUBSIDIES(s) {
			if (!s->IsAwarded()) {
				num_not_awarded++;
			} else {
				num_awarded++;
			}
		}

		/* Count the 'none' lines */
		if (num_awarded     == 0) num_awarded = 1;
		if (num_not_awarded == 0) num_not_awarded = 1;

		/* Offered, accepted and an empty line before the accepted ones. */
		return 3 + num_awarded + num_not_awarded;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != SLW_PANEL) return;
		Dimension d = maxdim(GetStringBoundingBox(STR_SUBSIDIES_OFFERED_TITLE), GetStringBoundingBox(STR_SUBSIDIES_SUBSIDISED_TITLE));

		resize->height = d.height;

		d.height *= 5;
		d.width += padding.width + WD_FRAMERECT_RIGHT + WD_FRAMERECT_LEFT;
		d.height += padding.height + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
		*size = maxdim(*size, d);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != SLW_PANEL) return;

		YearMonthDay ymd;
		ConvertDateToYMD(_date, &ymd);

		int right = r.right - WD_FRAMERECT_RIGHT;
		int y = r.top + WD_FRAMERECT_TOP;
		int x = r.left + WD_FRAMERECT_LEFT;

		int pos = -this->vscroll->GetPosition();
		const int cap = this->vscroll->GetCapacity();

		/* Section for drawing the offered subisidies */
		if (IsInsideMM(pos, 0, cap)) DrawString(x, right, y + pos * FONT_HEIGHT_NORMAL, STR_SUBSIDIES_OFFERED_TITLE);
		pos++;

		uint num = 0;
		const Subsidy *s;
		FOR_ALL_SUBSIDIES(s) {
			if (!s->IsAwarded()) {
				if (IsInsideMM(pos, 0, cap)) {
					/* Displays the two offered towns */
					SetupSubsidyDecodeParam(s, true);
					SetDParam(7, _date - ymd.day + s->remaining * 32);
					DrawString(x, right, y + pos * FONT_HEIGHT_NORMAL, STR_SUBSIDIES_OFFERED_FROM_TO);
				}
				pos++;
				num++;
			}
		}

		if (num == 0) {
			if (IsInsideMM(pos, 0, cap)) DrawString(x, right, y + pos * FONT_HEIGHT_NORMAL, STR_SUBSIDIES_NONE);
			pos++;
		}

		/* Section for drawing the already granted subisidies */
		pos++;
		if (IsInsideMM(pos, 0, cap)) DrawString(x, right, y + pos * FONT_HEIGHT_NORMAL, STR_SUBSIDIES_SUBSIDISED_TITLE);
		pos++;
		num = 0;

		FOR_ALL_SUBSIDIES(s) {
			if (s->IsAwarded()) {
				if (IsInsideMM(pos, 0, cap)) {
					SetupSubsidyDecodeParam(s, true);
					SetDParam(7, s->awarded);
					SetDParam(8, _date - ymd.day + s->remaining * 32);

					/* Displays the two connected stations */
					DrawString(x, right, y + pos * FONT_HEIGHT_NORMAL, STR_SUBSIDIES_SUBSIDISED_FROM_TO);
				}
				pos++;
				num++;
			}
		}

		if (num == 0) {
			if (IsInsideMM(pos, 0, cap)) DrawString(x, right, y + pos * FONT_HEIGHT_NORMAL, STR_SUBSIDIES_NONE);
			pos++;
		}
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, SLW_PANEL);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		this->vscroll->SetCount(this->CountLines());
	}
};

static const NWidgetPart _nested_subsidies_list_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_SUBSIDIES_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN, SLW_PANEL), SetDataTip(0x0, STR_SUBSIDIES_TOOLTIP_CLICK_ON_SERVICE_TO_CENTER), SetResize(1, 1), SetScrollbar(SLW_SCROLLBAR), EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, SLW_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _subsidies_list_desc(
	WDP_AUTO, 500, 127,
	WC_SUBSIDIES_LIST, WC_NONE,
	0,
	_nested_subsidies_list_widgets, lengthof(_nested_subsidies_list_widgets)
);


void ShowSubsidiesList()
{
	AllocateWindowDescFront<SubsidyListWindow>(&_subsidies_list_desc, 0);
}
