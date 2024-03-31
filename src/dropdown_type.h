/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dropdown_type.h Types related to the drop down widget. */

#ifndef DROPDOWN_TYPE_H
#define DROPDOWN_TYPE_H

#include "window_type.h"
#include "gfx_func.h"
#include "gfx_type.h"
#include "palette_func.h"
#include "window_gui.h"

/**
 * Base list item class from which others are derived.
 */
class DropDownListItem {
public:
	int result; ///< Result value to return to window on selection.
	bool masked; ///< Masked and unselectable item.
	bool shaded; ///< Shaded item, affects text colour.

	explicit DropDownListItem(int result, bool masked = false, bool shaded = false) : result(result), masked(masked), shaded(shaded) {}
	virtual ~DropDownListItem() = default;

	virtual bool Selectable() const { return true; }
	virtual uint Height() const { return 0; }
	virtual uint Width() const { return 0; }

	virtual void Draw(const Rect &full, const Rect &, bool, Colours bg_colour) const
	{
		if (this->masked) GfxFillRect(full, GetColourGradient(bg_colour, SHADE_LIGHT), FILLRECT_CHECKER);
	}

	TextColour GetColour(bool sel) const
	{
		if (this->shaded) return (sel ? TC_SILVER : TC_GREY) | TC_NO_SHADE;
		return sel ? TC_WHITE : TC_BLACK;
	}
};

/**
 * A drop down list is a collection of drop down list items.
 */
typedef std::vector<std::unique_ptr<const DropDownListItem>> DropDownList;

void ShowDropDownListAt(Window *w, DropDownList &&list, int selected, WidgetID button, Rect wi_rect, Colours wi_colour, bool instant_close = false, bool persist = false);

void ShowDropDownList(Window *w, DropDownList &&list, int selected, WidgetID button, uint width = 0, bool instant_close = false, bool persist = false);

Dimension GetDropDownListDimension(const DropDownList &list);

void ReplaceDropDownList(Window *parent, DropDownList &&list);

#endif /* DROPDOWN_TYPE_H */
