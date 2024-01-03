/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dropdown_func.h Functions related to the drop down widget. */

#ifndef WIDGETS_DROPDOWN_FUNC_H
#define WIDGETS_DROPDOWN_FUNC_H

#include "../window_gui.h"

/* Show drop down menu containing a fixed list of strings */
void ShowDropDownMenu(Window *w, const StringID *strings, int selected, WidgetID button, uint32_t disabled_mask, uint32_t hidden_mask, uint width = 0);

#endif /* WIDGETS_DROPDOWN_FUNC_H */
