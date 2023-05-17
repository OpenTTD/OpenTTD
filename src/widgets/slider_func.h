/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file slider_type.h Types related to the horizontal slider widget. */

#ifndef WIDGETS_SLIDER_TYPE_H
#define WIDGETS_SLIDER_TYPE_H

#include "../window_type.h"
#include "../gfx_func.h"

void DrawSliderWidget(Rect r, int min_value, int max_value, int value, const std::map<int, StringID> &labels);
bool ClickSliderWidget(Rect r, Point pt, int min_value, int max_value, int &value);

inline bool ClickSliderWidget(Rect r, Point pt, int min_value, int max_value, byte &value)
{
	int tmp_value = value;
	if (!ClickSliderWidget(r, pt, min_value, max_value, tmp_value)) return false;
	value = tmp_value;
	return true;
}

#endif /* WIDGETS_SLIDER_TYPE_H */
