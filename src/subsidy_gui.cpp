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
#include "timer/timer_game_calendar.h"
#include "viewport_func.h"
#include "gui.h"
#include "subsidy_func.h"
#include "subsidy_base.h"
#include "core/geometry_func.hpp"

#include "widgets/subsidy_widget.h"

#include "table/strings.h"

#include "safeguards.h"

struct SubsidyListWindow : Window {
	Scrollbar *vscroll;

	SubsidyListWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_SUL_SCROLLBAR);
		this->FinishInitNested(window_number);
		this->OnInvalidateData(0);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget != WID_SUL_PANEL) return;

		int y = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_SUL_PANEL, WidgetDimensions::scaled.framerect.top);
		int num = 0;
		for (const Subsidy *s : Subsidy::Iterate()) {
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

		for (const Subsidy *s : Subsidy::Iterate()) {
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
			case SourceType::Industry: xy = Industry::Get(s->src)->location.tile; break;
			case SourceType::Town:     xy =     Town::Get(s->src)->xy; break;
			default: NOT_REACHED();
		}

		if (_ctrl_pressed || !ScrollMainWindowToTile(xy)) {
			if (_ctrl_pressed) ShowExtraViewportWindow(xy);

			/* otherwise determine dst coordinate for subsidy and scroll to it */
			switch (s->dst_type) {
				case SourceType::Industry: xy = Industry::Get(s->dst)->location.tile; break;
				case SourceType::Town:     xy =     Town::Get(s->dst)->xy; break;
				default: NOT_REACHED();
			}

			if (_ctrl_pressed) {
				ShowExtraViewportWindow(xy);
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
		for (const Subsidy *s : Subsidy::Iterate()) {
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

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget != WID_SUL_PANEL) return;
		Dimension d = maxdim(GetStringBoundingBox(STR_SUBSIDIES_OFFERED_TITLE), GetStringBoundingBox(STR_SUBSIDIES_SUBSIDISED_TITLE));

		resize->height = GetCharacterHeight(FS_NORMAL);

		d.height *= 5;
		d.width += WidgetDimensions::scaled.framerect.Horizontal();
		d.height += WidgetDimensions::scaled.framerect.Vertical();
		*size = maxdim(*size, d);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_SUL_PANEL) return;

		TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::date);

		Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);

		int pos = -this->vscroll->GetPosition();
		const int cap = this->vscroll->GetCapacity();

		/* Section for drawing the offered subsidies */
		if (IsInsideMM(pos, 0, cap)) DrawString(tr.left, tr.right, tr.top + pos * GetCharacterHeight(FS_NORMAL), STR_SUBSIDIES_OFFERED_TITLE);
		pos++;

		uint num = 0;
		for (const Subsidy *s : Subsidy::Iterate()) {
			if (!s->IsAwarded()) {
				if (IsInsideMM(pos, 0, cap)) {
					/* Displays the two offered towns */
					SetupSubsidyDecodeParam(s, SubsidyDecodeParamType::Gui);
					SetDParam(7, TimerGameCalendar::date - ymd.day + s->remaining * 32);
					DrawString(tr.left, tr.right, tr.top + pos * GetCharacterHeight(FS_NORMAL), STR_SUBSIDIES_OFFERED_FROM_TO);
				}
				pos++;
				num++;
			}
		}

		if (num == 0) {
			if (IsInsideMM(pos, 0, cap)) DrawString(tr.left, tr.right, tr.top + pos * GetCharacterHeight(FS_NORMAL), STR_SUBSIDIES_NONE);
			pos++;
		}

		/* Section for drawing the already granted subsidies */
		pos++;
		if (IsInsideMM(pos, 0, cap)) DrawString(tr.left, tr.right, tr.top + pos * GetCharacterHeight(FS_NORMAL), STR_SUBSIDIES_SUBSIDISED_TITLE);
		pos++;
		num = 0;

		for (const Subsidy *s : Subsidy::Iterate()) {
			if (s->IsAwarded()) {
				if (IsInsideMM(pos, 0, cap)) {
					SetupSubsidyDecodeParam(s, SubsidyDecodeParamType::Gui);
					SetDParam(7, s->awarded);
					SetDParam(8, TimerGameCalendar::date - ymd.day + s->remaining * 32);

					/* Displays the two connected stations */
					DrawString(tr.left, tr.right, tr.top + pos * GetCharacterHeight(FS_NORMAL), STR_SUBSIDIES_SUBSIDISED_FROM_TO);
				}
				pos++;
				num++;
			}
		}

		if (num == 0) {
			if (IsInsideMM(pos, 0, cap)) DrawString(tr.left, tr.right, tr.top + pos * GetCharacterHeight(FS_NORMAL), STR_SUBSIDIES_NONE);
			pos++;
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_SUL_PANEL);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
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
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN, WID_SUL_PANEL), SetDataTip(0x0, STR_SUBSIDIES_TOOLTIP_CLICK_ON_SERVICE_TO_CENTER), SetResize(1, 1), SetScrollbar(WID_SUL_SCROLLBAR), EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_SUL_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _subsidies_list_desc(__FILE__, __LINE__,
	WDP_AUTO, "list_subsidies", 500, 127,
	WC_SUBSIDIES_LIST, WC_NONE,
	0,
	std::begin(_nested_subsidies_list_widgets), std::end(_nested_subsidies_list_widgets)
);


void ShowSubsidiesList()
{
	AllocateWindowDescFront<SubsidyListWindow>(&_subsidies_list_desc, 0);
}
