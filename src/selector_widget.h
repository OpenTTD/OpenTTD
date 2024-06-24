/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file selector_widget.h GUI that shows performance graphs. */

#ifndef SELECTOR_WIDGET_H
#define SELECTOR_WIDGET_H

#include "stdafx.h"
#include "querystring_gui.h"
#include "stringfilter_type.h"
#include "strings_type.h"
#include "widget_type.h"
#include "window_gui.h"
#include "window_type.h"
#include "safeguards.h"

class SelectorWidget {
public:
	/**
	 * This struct is like a config file for a utility,
	 * it makes the selector widget configurable and extencible.
	 * In it's current form it stores function pointers to functions
	 * that rebuild the lists and draw individual items in the matrix widget
	 */
	struct Profile {
		void (*DrawSection)(SelectorWidget *wid, int id, const Rect &r); ///< A function pointer to a function that draws 1 line of the list
		void (*RebuildList)(SelectorWidget *wid); ///< A function pointer that rebuilds the item list
	};

	static const Profile company_selector_profile; ///< One prebuilt profile for selection
	static const Profile cargo_selector_profile; ///< One prebuilt profile for selection

	std::optional<int> selected_id; ///< Id of the currently selected item
	std::vector<bool> shown; ///< A vector that determines which items are shown e.g on the graph. Not to be confused with filtered_list
	std::vector<uint32_t> list; ///< A list of all the items
	StringFilter string_filter; ///< The filter that checks weather an item should be displayed based on the editbox

	/** A list of items displayed in WID_SELECTION_MATRIX after the editbox filtering
	 * It is a strict subset of SelectorWidget::list */
	std::vector<uint32_t> filtered_list;

	static std::unique_ptr<NWidgetBase> MakeSelectorWidgetUI();

	void PreInit(Profile p);
	void Init(Window* w);
	void OnClick(Point pt, WidgetID widget, int click_count);
	void OnInvalidateData(int data, bool gui_scope);
	void UpdateWidgetSize(WidgetID widget, Dimension &size, const Dimension &padding, Dimension &fill, Dimension &resize);
	void OnResize();
	void OnEditboxChanged(WidgetID wid);
	void DrawWidget(const Rect &r, WidgetID widget);
	void RebuildList();
private:
	/**
	 * This enum stores the WidgetIDs of all the widgets that belong to this selector widget.
	 * It starts at a very high value, not to interfear with other widgets of the parrent window
	 */
	enum InternalWidgets {
		WID_SELECTOR_MATRIX = 42069,
		WID_SELECTOR_SCROLLBAR,
		WID_SELECTOR_EDITBOX,
		WID_SELECTOR_HIDEALL,
		WID_SELECTOR_SHOWALL,
		WID_SELECTOR_TOGGLE,
	};

	Profile profile; ///< currently selected profile

	Window *w; ///< The parrent window pointer

	int row_height; ///< The height of 1 row in the WID_SELECTOR_MATRIX widget

	Scrollbar* vscroll; ///< A pointer to the WID_SELECTOR_SCROLLBAR widget

	/** A QueryString modifiable by the WID_SELECTOR_EDITBOX widget */
	QueryString editbox{MAX_LENGTH_COMPANY_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_COMPANY_NAME_CHARS};

};
#endif /* SELECTOR_WIDGET_H */
