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


void DrawVolumeSliderWidget(Rect r, byte value);
bool ClickVolumeSliderWidget(Rect r, Point pt, byte &value);


#endif /* WIDGETS_SLIDER_TYPE_H */
