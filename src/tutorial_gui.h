/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tutorial_gui.h GUI for tutorial window. */

#ifndef TUTORIAL_GUI_H
#define TUTORIAL_GUI_H
#include "stdafx.h"
#include "window_gui.h"
#include "window_func.h"
#include "widgets/tutorial_widget.h"
#include "gfx_func.h"

#include "table/strings.h"

void ShowTutorialWindow(bool force_show = false);
void DrawWidget(const Rect&r,WidgetID widget);
void LoadPagesFromStrings();
void UpdateUIForPage(uint index);

#endif /* TUTORIAL_GUI_H */
