/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file slider.cpp Implementation of the horizontal slider widget. */

#include "stdafx.h"
#include "gfx_func.h"
#include "palette_func.h"
#include "slider_func.h"
#include "strings_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "safeguards.h"

static const int SLIDER_WIDTH = 3;

/**
 * Draw a slider widget with knob at given value
 * @param r Rectangle to draw the widget in
 * @param min_value Minimum value of slider
 * @param max_value Maximum value of slider
 * @param nmarks Number of marks to display (when mark_func is provided.)
 * @param value Value to put the slider at
 * @param mark_func Callback function to get the StringID to draw on a mark.
 */
void DrawSliderWidget(Rect r, int min_value, int max_value, int nmarks, int value, SliderMarkFunc *mark_func)
{
	/* Allow space for labels. We assume they are in the small font. */
	if (mark_func != nullptr) r.bottom -= GetCharacterHeight(FS_SMALL) + WidgetDimensions::scaled.hsep_normal;

	max_value -= min_value;

	/* Draw a wedge indicating low to high value. */
	const int ha = (r.bottom - r.top) / 5;
	const int sw = ScaleGUITrad(SLIDER_WIDTH);
	const int t = WidgetDimensions::scaled.bevel.top; /* Thickness of lines */
	int wx1 = r.left  + sw / 2;
	int wx2 = r.right - sw / 2;
	if (_current_text_dir == TD_RTL) std::swap(wx1, wx2);
	const uint shadow = GetColourGradient(COLOUR_GREY, SHADE_DARK);
	const uint fill = GetColourGradient(COLOUR_GREY, SHADE_LIGHTER);
	const uint light = GetColourGradient(COLOUR_GREY, SHADE_LIGHTEST);
	const std::vector<Point> wedge{ Point{wx1, r.bottom - ha}, Point{wx2, r.top + ha}, Point{wx2, r.bottom - ha} };
	GfxFillPolygon(wedge, fill);
	GfxDrawLine(wedge[0].x, wedge[0].y, wedge[2].x, wedge[2].y, light, t);
	GfxDrawLine(wedge[1].x, wedge[1].y, wedge[2].x, wedge[2].y, _current_text_dir == TD_RTL ? shadow : light, t);
	GfxDrawLine(wedge[0].x, wedge[0].y, wedge[1].x, wedge[1].y, shadow, t);

	int x;
	if (mark_func != nullptr) {
		for (int mark = 0; mark < nmarks; ++mark) {
			const int mark_value = (max_value * mark) / (nmarks - 1);

			const StringID str = mark_func(nmarks, mark, mark_value + min_value);
			if (str == INVALID_STRING_ID) continue;

			x = mark_value;
			if (_current_text_dir == TD_RTL) x = max_value - mark_value;
			x = r.left + (x * (r.right - r.left - sw) / max_value) + sw / 2;
			GfxDrawLine(x, r.bottom - ha + 1, x, r.bottom + (str == STR_NULL ? 0 : WidgetDimensions::scaled.hsep_normal), shadow, t);
			if (str == STR_NULL) continue;

			Dimension d = GetStringBoundingBox(str, FS_SMALL);
			x = Clamp(x - d.width / 2, r.left, r.right - d.width);
			DrawString(x, x + d.width, r.bottom + 1 + WidgetDimensions::scaled.hsep_normal, str, TC_BLACK, SA_CENTER, false, FS_SMALL);
		}
	}

	/* Draw a slider handle indicating current value. */
	value -= min_value;
	if (_current_text_dir == TD_RTL) value = max_value - value;
	x = r.left + (value * (r.right - r.left - sw) / max_value);
	DrawFrameRect(x, r.top, x + sw, r.bottom, COLOUR_GREY, FR_NONE);
}

/**
 * Handle click on a slider widget to change the value
 * @param r      Rectangle of the widget
 * @param pt     Clicked point
 * @param min_value Minimum value of slider
 * @param max_value Maximum value of slider
 * @param nmarks Number of marks displayed. Value will be rounded to nearest mark.
 * @param value[in,out] Value to modify
 * @return       True if the value setting was modified
 */
bool ClickSliderWidget(Rect r, Point pt, int min_value, int max_value, int nmarks, int &value)
{
	max_value -= min_value;

	const int sw = ScaleGUITrad(SLIDER_WIDTH);
	int new_value = Clamp((pt.x - r.left - sw / 2) * max_value / (r.right - r.left - sw), 0, max_value);
	if (_current_text_dir == TD_RTL) new_value = max_value - new_value;
	new_value += min_value;

	if (nmarks > 0) {
		const int step = max_value / (nmarks - 1);
		new_value = ((new_value + step / 2) / step) * step;
	}

	if (new_value != value) {
		value = new_value;
		return true;
	}

	return false;
}
