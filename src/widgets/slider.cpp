/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file slider.cpp Implementation of the horizontal slider widget. */

#include "../stdafx.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../strings_func.h"
#include "../zoom_func.h"
#include "slider_func.h"

#include "../safeguards.h"


/**
 * Draw a volume slider widget with know at given value
 * @param r     Rectangle to draw the widget in
 * @param value Value to put the slider at
 */
void DrawVolumeSliderWidget(Rect r, byte value)
{
	static const int slider_width = 3;

	/* Draw a wedge indicating low to high volume level. */
	const int ha = (r.bottom - r.top) / 5;
	int wx1 = r.left, wx2 = r.right;
	if (_current_text_dir == TD_RTL) std::swap(wx1, wx2);
	const uint shadow = _colour_gradient[COLOUR_GREY][3];
	const uint fill = _colour_gradient[COLOUR_GREY][6];
	const uint light = _colour_gradient[COLOUR_GREY][7];
	const std::vector<Point> wedge{ Point{wx1, r.bottom - ha}, Point{wx2, r.top + ha}, Point{wx2, r.bottom - ha} };
	GfxFillPolygon(wedge, fill);
	GfxDrawLine(wedge[0].x, wedge[0].y, wedge[2].x, wedge[2].y, light);
	GfxDrawLine(wedge[1].x, wedge[1].y, wedge[2].x, wedge[2].y, _current_text_dir == TD_RTL ? shadow : light);
	GfxDrawLine(wedge[0].x, wedge[0].y, wedge[1].x, wedge[1].y, shadow);

	/* Draw a slider handle indicating current volume level. */
	const int sw = ScaleGUITrad(slider_width);
	if (_current_text_dir == TD_RTL) value = 127 - value;
	const int x = r.left + (value * (r.right - r.left - sw) / 127);
	DrawFrameRect(x, r.top, x + sw, r.bottom, COLOUR_GREY, FR_NONE);
}

/**
 * Handle click on a volume slider widget to change the value
 * @param r      Rectangle of the widget
 * @param pt     Clicked point
 * @param value[in,out] Volume value to modify
 * @return       True if the volume setting was modified
 */
bool ClickVolumeSliderWidget(Rect r, Point pt, byte &value)
{
	byte new_vol = Clamp((pt.x - r.left) * 127 / (r.right - r.left), 0, 127);
	if (_current_text_dir == TD_RTL) new_vol = 127 - new_vol;

	/* Clamp to make sure min and max are properly settable */
	if (new_vol > 124) new_vol = 127;
	if (new_vol < 3) new_vol = 0;
	if (new_vol != value) {
		value = new_vol;
		return true;
	}

	return false;
}
