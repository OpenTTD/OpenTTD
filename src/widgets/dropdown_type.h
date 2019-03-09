/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dropdown_type.h Types related to the drop down widget. */

#ifndef WIDGETS_DROPDOWN_TYPE_H
#define WIDGETS_DROPDOWN_TYPE_H

#include "../window_type.h"
#include "../gfx_func.h"
#include "../core/smallvec_type.hpp"
#include "table/strings.h"

/**
 * Base list item class from which others are derived. If placed in a list it
 * will appear as a horizontal line in the menu.
 */
class DropDownListItem {
public:
	int result;  ///< Result code to return to window on selection
	bool masked; ///< Masked and unselectable item

	DropDownListItem(int result, bool masked) : result(result), masked(masked) {}
	virtual ~DropDownListItem() {}

	virtual bool Selectable() const { return false; }
	virtual uint Height(uint width) const { return FONT_HEIGHT_NORMAL; }
	virtual uint Width() const { return 0; }
	virtual void Draw(int left, int right, int top, int bottom, bool sel, Colours bg_colour) const;
};

/**
 * Common string list item.
 */
class DropDownListStringItem : public DropDownListItem {
public:
	StringID string; ///< String ID of item

	DropDownListStringItem(StringID string, int result, bool masked) : DropDownListItem(result, masked), string(string) {}
	virtual ~DropDownListStringItem() {}

	virtual bool Selectable() const { return true; }
	virtual uint Width() const;
	virtual void Draw(int left, int right, int top, int bottom, bool sel, Colours bg_colour) const;
	virtual StringID String() const { return this->string; }

	static int CDECL NatSortFunc(const DropDownListItem * const *first, const DropDownListItem * const *second);
};

/**
 * String list item with parameters.
 */
class DropDownListParamStringItem : public DropDownListStringItem {
public:
	uint64 decode_params[10]; ///< Parameters of the string

	DropDownListParamStringItem(StringID string, int result, bool masked) : DropDownListStringItem(string, result, masked) {}
	virtual ~DropDownListParamStringItem() {}

	virtual StringID String() const;
	virtual void SetParam(uint index, uint64 value) { decode_params[index] = value; }
};

/**
 * List item containing a C char string.
 */
class DropDownListCharStringItem : public DropDownListStringItem {
public:
	const char *raw_string;

	DropDownListCharStringItem(const char *raw_string, int result, bool masked) : DropDownListStringItem(STR_JUST_RAW_STRING, result, masked), raw_string(raw_string) {}
	virtual ~DropDownListCharStringItem() {}

	virtual StringID String() const;
};

/**
 * A drop down list is a collection of drop down list items.
 */
typedef AutoDeleteSmallVector<const DropDownListItem *, 4> DropDownList;

void ShowDropDownListAt(Window *w, const DropDownList *list, int selected, int button, Rect wi_rect, Colours wi_colour, bool auto_width = false, bool instant_close = false);

void ShowDropDownList(Window *w, const DropDownList *list, int selected, int button, uint width = 0, bool auto_width = false, bool instant_close = false);

#endif /* WIDGETS_DROPDOWN_TYPE_H */
