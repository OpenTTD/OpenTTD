/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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

static const int SLIDER_WIDTH = 5; ///< Width of the slider handle.

/**
 * Draw a slider widget with knob at given value
 * @param r Rectangle to draw the widget in
 * @param wedge_colour Colour to draw wedge.
 * @param handle_colour Colour to draw handle.
 * @param text_colour Colour of text.
 * @param min_value Minimum value of slider
 * @param max_value Maximum value of slider
 * @param nmarks Number of marks to display (when mark_func is provided.)
 * @param value Value to put the slider at
 * @param mark_func Callback function to get the StringID to draw on a mark.
 * @param soft_min Soft minimum value.
 * @param soft_max Soft maximum value.
 */
void DrawSliderWidget(Rect r, Colours wedge_colour, Colours handle_colour, TextColour text_colour, int min_value, int max_value, int nmarks, int value, SliderMarkFunc *mark_func, int soft_min, int soft_max)
{
	/* Allow space for labels. We assume they are in the small font. */
	Rect tr = r;
	if (mark_func != nullptr) {
		tr.top = tr.bottom - GetCharacterHeight(FontSize::Small) + 1;
		r.bottom = tr.top - WidgetDimensions::scaled.hsep_normal - 1;
	}

	const int ha = r.Height() / 5;
	const int sw = ScaleGUITrad(SLIDER_WIDTH);
	const int t = WidgetDimensions::scaled.bevel.top; /* Thickness of lines */

	/* Position the wedge so that slider can overlap the ends. */
	r.left += sw / 2;
	r.right -= sw / 2;

	const PixelColour shadow = GetColourGradient(wedge_colour, Shade::Dark);
	const PixelColour fill = GetColourGradient(wedge_colour, Shade::Lighter);
	const PixelColour light = GetColourGradient(wedge_colour, Shade::Lightest);

	/* Adjust the wedge direction for LTR/RTL languages. */
	int wx1 = r.left;
	int wx2 = r.right;
	if (_current_text_dir == TD_RTL) std::swap(wx1, wx2);

	/* Draw a wedge indicating low to high value. */
	const std::array<Point, 3> wedge{ Point{wx1, r.bottom - ha}, Point{wx2, r.top + ha}, Point{wx2, r.bottom - ha} };
	GfxFillPolygon(wedge, fill);
	GfxDrawLine(wedge[0].x, wedge[0].y, wedge[2].x, wedge[2].y, light, t);
	GfxDrawLine(wedge[1].x, wedge[1].y, wedge[2].x, wedge[2].y, _current_text_dir == TD_RTL ? shadow : light, t);
	GfxDrawLine(wedge[0].x, wedge[0].y, wedge[1].x, wedge[1].y, shadow, t);

	int range = max_value - min_value;
	int width = r.right - r.left;

	int x;
	if (mark_func != nullptr) {
		for (int mark = 0; mark < nmarks; ++mark) {
			int mark_value = range * mark / (nmarks - 1);

			auto str = mark_func(nmarks, mark, mark_value + min_value);
			if (!str.has_value()) continue;

			if (_current_text_dir == TD_RTL) mark_value = range - mark_value;
			x = r.left + (mark_value * width / range);

			GfxDrawLine(x, r.bottom - ha + 1, x, r.bottom + (str->empty() ? 0 : WidgetDimensions::scaled.hsep_normal), shadow, t);
			if (str->empty()) continue;

			Dimension d = GetStringBoundingBox(*str, FontSize::Small);
			x = Clamp(x - d.width / 2, r.left, r.right - d.width);
			DrawString(tr.WithX(x, x + d.width), *str, text_colour, {AlignmentH::Centre, AlignmentV::Middle}, false, FontSize::Small);

			if (soft_min > mark_value + min_value || soft_max < mark_value + min_value) {
				GfxFillRect(tr.WithX(x, x + d.width), GetColourGradient(wedge_colour, Shade::Light), FillRectMode::Checker);
			}
		}
	}

	if (soft_min > min_value) {
		x = (soft_min - min_value) * width / range - t;
		GfxFillRect(r.WithWidth(x, _current_text_dir == TD_RTL), GetColourGradient(wedge_colour, Shade::Light), FillRectMode::Checker);
	}

	if (soft_max < max_value) {
		x = (soft_max - min_value) * width / range + t;
		GfxFillRect(r.Indent(x, _current_text_dir == TD_RTL), GetColourGradient(wedge_colour, Shade::Light), FillRectMode::Checker);
	}

	/* Draw a slider handle indicating current value. */
	value -= min_value;
	if (_current_text_dir == TD_RTL) value = range - value;
	x = r.left + (value * width / range) - sw / 2;
	DrawFrameRect(x, r.top, x + sw - 1, r.bottom, handle_colour, {});
}

/**
 * Handle click on a slider widget to change the value
 * @param r      Rectangle of the widget
 * @param pt     Clicked point
 * @param min_value Minimum value of slider
 * @param max_value Maximum value of slider
 * @param nmarks Number of marks displayed. Value will be rounded to nearest mark.
 * @param[in,out] value Value to modify
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
