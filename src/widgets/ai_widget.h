/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_widget.h Types related to the ai widgets. */

#ifndef WIDGETS_AI_WIDGET_H
#define WIDGETS_AI_WIDGET_H

#include "../textfile_type.h"

/** Widgets of the #AIConfigWindow class. */
enum AIConfigWidgets : WidgetID {
	WID_AIC_BACKGROUND,       ///< Window background.
	WID_AIC_DECREASE_NUMBER,  ///< Decrease the number of AIs.
	WID_AIC_INCREASE_NUMBER,  ///< Increase the number of AIs.
	WID_AIC_NUMBER,           ///< Number of AIs.
	WID_AIC_DECREASE_INTERVAL,///< Decrease the interval.
	WID_AIC_INCREASE_INTERVAL,///< Increase the interval.
	WID_AIC_INTERVAL,         ///< Interval between time AIs start.
	WID_AIC_LIST,             ///< List with currently selected AIs.
	WID_AIC_SCROLLBAR,        ///< Scrollbar to scroll through the selected AIs.
	WID_AIC_MOVE_UP,          ///< Move up button.
	WID_AIC_MOVE_DOWN,        ///< Move down button.
	WID_AIC_CHANGE,           ///< Select another AI button.
	WID_AIC_CONFIGURE,        ///< Change AI settings button.
	WID_AIC_OPEN_URL,         ///< Open AI URL.
	WID_AIC_TEXTFILE,         ///< Open AI readme, changelog (+1) or license (+2).
	WID_AIC_CONTENT_DOWNLOAD = WID_AIC_TEXTFILE + TFT_CONTENT_END, ///< Download content button.
};

#endif /* WIDGETS_AI_WIDGET_H */
