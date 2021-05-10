/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file date_gui.cpp Graphical selection of a date. */

#include "stdafx.h"
#include "strings_func.h"
#include "date_func.h"
#include "window_func.h"
#include "window_gui.h"
#include "date_gui.h"
#include "core/geometry_func.hpp"

#include "widgets/dropdown_type.h"
#include "widgets/date_widget.h"

#include "safeguards.h"


/** Window to select a date graphically by using dropdowns */
struct SetDateWindow : Window {
	SetDateCallback *callback; ///< Callback to call when a date has been selected
	YearMonthDay date; ///< The currently selected date
	Year min_year;     ///< The minimum year in the year dropdown
	Year max_year;     ///< The maximum year (inclusive) in the year dropdown

	/**
	 * Create the new 'set date' window
	 * @param desc the window description
	 * @param window_number number of the window
	 * @param parent the parent window, i.e. if this closes we should close too
	 * @param initial_date the initial date to show
	 * @param min_year the minimum year to show in the year dropdown
	 * @param max_year the maximum year (inclusive) to show in the year dropdown
	 * @param callback the callback to call once a date has been selected
	 */
	SetDateWindow(WindowDesc *desc, WindowNumber window_number, Window *parent, Date initial_date, Year min_year, Year max_year, SetDateCallback *callback) :
			Window(desc),
			callback(callback),
			min_year(std::max(MIN_YEAR, min_year)),
			max_year(std::min(MAX_YEAR, max_year))
	{
		assert(this->min_year <= this->max_year);
		this->parent = parent;
		this->InitNested(window_number);

		if (initial_date == 0) initial_date = _date;
		ConvertDateToYMD(initial_date, &this->date);
		this->date.year = Clamp(this->date.year, min_year, max_year);
	}

	Point OnInitialPosition(int16 sm_width, int16 sm_height, int window_number) override
	{
		Point pt = { this->parent->left + this->parent->width / 2 - sm_width / 2, this->parent->top + this->parent->height / 2 - sm_height / 2 };
		return pt;
	}

	/**
	 * Helper function to construct the dropdown.
	 * @param widget the dropdown widget to create the dropdown for
	 */
	void ShowDateDropDown(int widget)
	{
		int selected;
		DropDownList list;

		switch (widget) {
			default: NOT_REACHED();

			case WID_SD_DAY:
				for (uint i = 0; i < 31; i++) {
					list.emplace_back(new DropDownListStringItem(STR_DAY_NUMBER_1ST + i, i + 1, false));
				}
				selected = this->date.day;
				break;

			case WID_SD_MONTH:
				for (uint i = 0; i < 12; i++) {
					list.emplace_back(new DropDownListStringItem(STR_MONTH_JAN + i, i, false));
				}
				selected = this->date.month;
				break;

			case WID_SD_YEAR:
				for (Year i = this->min_year; i <= this->max_year; i++) {
					DropDownListParamStringItem *item = new DropDownListParamStringItem(STR_JUST_INT, i, false);
					item->SetParam(0, i);
					list.emplace_back(item);
				}
				selected = this->date.year;
				break;
		}

		ShowDropDownList(this, std::move(list), selected, widget);
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		Dimension d = {0, 0};
		switch (widget) {
			default: return;

			case WID_SD_DAY:
				for (uint i = 0; i < 31; i++) {
					d = maxdim(d, GetStringBoundingBox(STR_DAY_NUMBER_1ST + i));
				}
				break;

			case WID_SD_MONTH:
				for (uint i = 0; i < 12; i++) {
					d = maxdim(d, GetStringBoundingBox(STR_MONTH_JAN + i));
				}
				break;

			case WID_SD_YEAR:
				SetDParamMaxValue(0, this->max_year);
				d = maxdim(d, GetStringBoundingBox(STR_JUST_INT));
				break;
		}

		d.width += padding.width;
		d.height += padding.height;
		*size = d;
	}

	void SetStringParameters(int widget) const override
	{
		switch (widget) {
			case WID_SD_DAY:   SetDParam(0, this->date.day - 1 + STR_DAY_NUMBER_1ST); break;
			case WID_SD_MONTH: SetDParam(0, this->date.month + STR_MONTH_JAN); break;
			case WID_SD_YEAR:  SetDParam(0, this->date.year); break;
		}
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		switch (widget) {
			case WID_SD_DAY:
			case WID_SD_MONTH:
			case WID_SD_YEAR:
				ShowDateDropDown(widget);
				break;

			case WID_SD_SET_DATE:
				if (this->callback != nullptr) this->callback(this, ConvertYMDToDate(this->date.year, this->date.month, this->date.day));
				delete this;
				break;
		}
	}

	void OnDropdownSelect(int widget, int index) override
	{
		switch (widget) {
			case WID_SD_DAY:
				this->date.day = index;
				break;

			case WID_SD_MONTH:
				this->date.month = index;
				break;

			case WID_SD_YEAR:
				this->date.year = index;
				break;
		}
		this->SetDirty();
	}
};

/** Widgets for the date setting window. */
static const NWidgetPart _nested_set_date_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_DATE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_VERTICAL), SetPIP(6, 6, 6),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(6, 6, 6),
				NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_SD_DAY), SetFill(1, 0), SetDataTip(STR_JUST_STRING, STR_DATE_DAY_TOOLTIP),
				NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_SD_MONTH), SetFill(1, 0), SetDataTip(STR_JUST_STRING, STR_DATE_MONTH_TOOLTIP),
				NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_SD_YEAR), SetFill(1, 0), SetDataTip(STR_JUST_INT, STR_DATE_YEAR_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SD_SET_DATE), SetMinimalSize(100, 12), SetDataTip(STR_DATE_SET_DATE, STR_DATE_SET_DATE_TOOLTIP),
				NWidget(NWID_SPACER), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer()
};

/** Description of the date setting window. */
static WindowDesc _set_date_desc(
	WDP_CENTER, nullptr, 0, 0,
	WC_SET_DATE, WC_NONE,
	0,
	_nested_set_date_widgets, lengthof(_nested_set_date_widgets)
);

/**
 * Create the new 'set date' window
 * @param window_number number for the window
 * @param parent the parent window, i.e. if this closes we should close too
 * @param initial_date the initial date to show
 * @param min_year the minimum year to show in the year dropdown
 * @param max_year the maximum year (inclusive) to show in the year dropdown
 * @param callback the callback to call once a date has been selected
 */
void ShowSetDateWindow(Window *parent, int window_number, Date initial_date, Year min_year, Year max_year, SetDateCallback *callback)
{
	DeleteWindowByClass(WC_SET_DATE);
	new SetDateWindow(&_set_date_desc, window_number, parent, initial_date, min_year, max_year, callback);
}
