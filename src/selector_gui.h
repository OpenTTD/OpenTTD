/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file selector_gui.h Functions/types etc. related to the selector widget. */

#ifndef SELECTOR_GUI_H
#define SELECTOR_GUI_H

#include "stdafx.h"
#include "querystring_gui.h"
#include "stringfilter_type.h"
#include "strings_type.h"
#include "widget_type.h"
#include "window_gui.h"
#include "window_type.h"
#include "safeguards.h"

struct SelectorWidget {

	std::optional<int> selected_id; ///< ID of the currently selected item
	std::vector<bool> shown; ///< A vector that determines which items are shown e.g on the graph. Not to be confused with filtered_list
	std::vector<uint32_t> list; ///< A list of all the items
	StringFilter string_filter; ///< The filter that checks wether an item should be displayed based on the editbox

	/** A list of items displayed in WID_SELECTION_MATRIX after the editbox filtering
	 * It is a strict subset of SelectorWidget::list */
	std::vector<uint32_t> filtered_list;

	static std::unique_ptr<NWidgetBase> MakeSelectorWidgetUI();

	void Init(Window *w);
	void OnClick(Point pt, WidgetID widget, int click_count);
	void OnMouseOver(Point pt, WidgetID widget);
	void OnInvalidateData(int data, bool gui_scope);
	void UpdateWidgetSize(WidgetID widget, Dimension &size, const Dimension &padding, Dimension &fill, Dimension &resize);
	void OnResize();
	void OnEditboxChanged(WidgetID wid);
	void DrawWidget(const Rect &r, WidgetID widget);
	void RebuildList();

	virtual ~SelectorWidget() {};

private:
	Window *parent_window; ///< The parent window pointer

	int row_height; ///< The height of 1 row in the WID_SELECTOR_MATRIX widget

	Scrollbar *vscroll; ///< The vertical scrollbar

	/** A QueryString modifiable by the WID_SELECTOR_EDITBOX widget */
	QueryString editbox{MAX_LENGTH_COMPANY_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_COMPANY_NAME_CHARS};

	/**
	 * Draws "section" (line) of the scrollable list.
	 * Called by #DrawWidget
	 * @param id The "id" of the item to be drawn, can mean different things depending on what the selector widget is displaying.
	 * e.g when this widget displays companies, the id is a unique and valid CompanyID
	 * @param r The rectangle where the item is to be drawn.
	 *
	 * @see CargoSelectorWidget::DrawSection()
	 * @see CompanySelectorWidget::DrawSection()
	 * @see SelectorWidget::PopulateList()
	 * @see SelectorWidget::DrawWidget()
	 */
	virtual void DrawSection(uint id, const Rect &r) = 0;

	/**
	 * Repopulates the list that the widget uses to keep track of different selectable items
	 * @see CargoSelectorWidget::PopulateList()
	 * @see CompanySelectorWidget::PopulateList()
	 */
	virtual void PopulateList() = 0;

	/**
	 * Called when the widget selection or visibility of some item is changed.
	 * Used to update things that depend on the selector widget's data.
	 */
	virtual void OnChanged() = 0;
};

struct CargoSelectorWidget : SelectorWidget {
	virtual void DrawSection(uint id, const Rect &r) override;
	virtual void PopulateList() override;
};

struct CompanySelectorWidget : SelectorWidget {
	virtual void DrawSection(uint id, const Rect &r) override;
	virtual void PopulateList() override;
};

#endif /* SELECTOR_GUI_H */
