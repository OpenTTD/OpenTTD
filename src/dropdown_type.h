/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file dropdown_type.h Types related to the drop down widget. */

#ifndef DROPDOWN_TYPE_H
#define DROPDOWN_TYPE_H

#include "core/enum_type.hpp"
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
	/** Ensure the destructor of the sub classes are called as well. */
	virtual ~DropDownListItem() = default;

	/**
	 * Can this dropdown item be selected?
	 * @return Whether this item can be selected.
	 */
	virtual bool Selectable() const { return true; }

	/**
	 * The height of this item.
	 * @return The height.
	 */
	virtual uint Height() const { return 0; }

	/**
	 * The width of this item.
	 * @return The width.
	 */
	virtual uint Width() const { return 0; }

	/**
	 * Callback when this item is clicked.
	 * @param r The bounds of this item.
	 * @param pt The location within the bounds where the item is clicked.
	 * @return The 'click_result' for the OnDropdownClose callback on the dropdown's parent.
	 */
	virtual int OnClick([[maybe_unused]] const Rect &r, [[maybe_unused]] const Point &pt) const
	{
		return -1;
	}

	/**
	 * Callback for drawing this item.
	 * @param full The full bounds of the item including padding.
	 * @param r The bounds to draw the item in.
	 * @param sel Whether the item is elected or not.
	 * @param click_result When selected, the previously set 'click_result' otherwise -1.
	 * @param bg_colour The background color for the item.
	 */
	virtual void Draw(const Rect &full, [[maybe_unused]] const Rect &r, [[maybe_unused]] bool sel, [[maybe_unused]] int click_result, Colours bg_colour) const
	{
		if (this->masked) GfxFillRect(full, GetColourGradient(bg_colour, SHADE_LIGHT), FILLRECT_CHECKER);
	}

	/**
	 * Get the colour of the text.
	 * @param sel Whether the item is selected or not.
	 * @return The text colour.
	 */
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

/** Configuration options for the created DropDownLists. */
enum class DropDownOption : uint8_t {
	InstantClose, ///< Set if releasing mouse button should close the list regardless of where the cursor is.
	Persist, ///< Set if this dropdown should stay open after an option is selected.
};
using DropDownOptions = EnumBitSet<DropDownOption, uint8_t>;

void ShowDropDownListAt(Window *w, DropDownList &&list, int selected, WidgetID button, Rect wi_rect, Colours wi_colour, DropDownOptions options = {});

void ShowDropDownList(Window *w, DropDownList &&list, int selected, WidgetID button, uint width = 0, DropDownOptions options = {});

Dimension GetDropDownListDimension(const DropDownList &list);

void ReplaceDropDownList(Window *parent, DropDownList &&list, std::optional<int> selected_result = std::nullopt);

#endif /* DROPDOWN_TYPE_H */
