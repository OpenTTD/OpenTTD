/* $Id$ */

/** @file dropdown_func.h Functions related to the drop down widget. */

#ifndef WIDGETS_DROPDOWN_FUNC_H
#define WIDGETS_DROPDOWN_FUNC_H

/* Show drop down menu containing a fixed list of strings */
void ShowDropDownMenu(Window *w, const StringID *strings, int selected, int button, uint32 disabled_mask, uint32 hidden_mask, uint width = 0);

/* Hide drop down menu of a parent window */
void HideDropDownMenu(Window *pw);

#endif /* WIDGETS_DROPDOWN_FUNC_H */
