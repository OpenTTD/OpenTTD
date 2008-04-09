/* $Id$ */

#ifndef WIDGETS_DROPDOWN_TYPE_H
#define WIDGETS_DROPDOWN_TYPE_H

#include "../window_type.h"
#include <list>

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
	virtual StringID String() const;
};

/**
 * Common string list item.
 */
class DropDownListStringItem : public DropDownListItem {
public:
	StringID string; ///< String ID of item

	DropDownListStringItem(StringID string, int result, bool masked) : DropDownListItem(result, masked), string(string) {}
	virtual ~DropDownListStringItem() {}

	StringID String() const;
};

/**
 * String list item with parameters.
 */
class DropDownListParamStringItem : public DropDownListStringItem {
public:
	uint64 decode_params[10]; ///< Parameters of the string

	DropDownListParamStringItem(StringID string, int result, bool masked) : DropDownListStringItem(string, result, masked) {}
	virtual ~DropDownListParamStringItem() {}

	StringID String() const;
	void SetParam(uint index, uint64 value) { decode_params[index] = value; }
};

/**
 * A drop down list is a collection of drop down list items.
 */
typedef std::list<DropDownListItem *> DropDownList;

/**
 * Show a drop down list.
 * @param w        Parent window for the list.
 * @param list     Prepopulated DropDownList. Will be deleted when the list is
 *                 closed.
 * @param selected The initially selected list item.
 * @param button   The widget within the parent window that is used to determine
 *                 the list's location.
 * @param width    Override the width determined by the selected widget.
 */
void ShowDropDownList(Window *w, DropDownList *list, int selected, int button, uint width = 0);

#endif /* WIDGETS_DROPDOWN_TYPE_H */
