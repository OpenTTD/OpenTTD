/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file settings_gui.h Functions for setting GUIs. */

#ifndef SETTING_GUI_H
#define SETTING_GUI_H

#include "gfx_type.h"
#include "widgets/dropdown_type.h"

/** Width of setting buttons */
#define SETTING_BUTTON_WIDTH  ((int)NWidgetScrollbar::GetHorizontalDimension().width * 2)
/** Height of setting buttons */
#define SETTING_BUTTON_HEIGHT ((int)NWidgetScrollbar::GetHorizontalDimension().height)

void DrawArrowButtons(int x, int y, Colours button_colour, byte state, bool clickable_left, bool clickable_right);
void DrawDropDownButton(int x, int y, Colours button_colour, bool state, bool clickable);
void DrawBoolButton(int x, int y, bool state, bool clickable);

template <class T>
DropDownList BuildSetDropDownList(int *selected_index)
{
	int n = T::GetNumSets();
	*selected_index = T::GetIndexOfUsedSet();
	DropDownList list;
	for (int i = 0; i < n; i++) {
		list.push_back(std::make_unique<DropDownListStringItem>(T::GetSet(i)->GetListLabel(), i, false));
	}
	return list;
}


/* Actually implemented in music_gui.cpp */
void ChangeMusicSet(int index);

#endif /* SETTING_GUI_H */

