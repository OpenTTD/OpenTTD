/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_debug_widget.h Types related to the newgrf debug widgets. */

#ifndef WIDGETS_NEWGRF_DEBUG_WIDGET_H
#define WIDGETS_NEWGRF_DEBUG_WIDGET_H

/** Widgets of the #NewGRFInspectWindow class. */
enum NewGRFInspectWidgets : WidgetID {
	WID_NGRFI_CAPTION,   ///< The caption bar of course.
	WID_NGRFI_PARENT,    ///< Inspect the parent.
	WID_NGRFI_VEH_PREV,  ///< Go to previous vehicle in chain.
	WID_NGRFI_VEH_NEXT,  ///< Go to next vehicle in chain.
	WID_NGRFI_VEH_CHAIN, ///< Display for vehicle chain.
	WID_NGRFI_MAINPANEL, ///< Panel widget containing the actual data.
	WID_NGRFI_SCROLLBAR, ///< Scrollbar.
};

/** Widgets of the #SpriteAlignerWindow class. */
enum SpriteAlignerWidgets : WidgetID {
	WID_SA_CAPTION,     ///< Caption of the window.
	WID_SA_PREVIOUS,    ///< Skip to the previous sprite.
	WID_SA_GOTO,        ///< Go to a given sprite.
	WID_SA_NEXT,        ///< Skip to the next sprite.
	WID_SA_UP,          ///< Move the sprite up.
	WID_SA_LEFT,        ///< Move the sprite to the left.
	WID_SA_RIGHT,       ///< Move the sprite to the right.
	WID_SA_DOWN,        ///< Move the sprite down.
	WID_SA_SPRITE,      ///< The actual sprite.
	WID_SA_OFFSETS_ABS, ///< The sprite offsets (absolute).
	WID_SA_OFFSETS_REL, ///< The sprite offsets (relative).
	WID_SA_PICKER,      ///< Sprite picker.
	WID_SA_LIST,        ///< Queried sprite list.
	WID_SA_SCROLLBAR,   ///< Scrollbar for sprite list.
	WID_SA_ZOOM,        ///< Zoom level buttons (from ZOOM_LVL_BEGIN to ZOOM_LVL_END).
	WID_SA_ZOOM_LAST = WID_SA_ZOOM + ZOOM_LVL_END - 1, ///< Marker for last zoom level button.
	WID_SA_RESET_REL,   ///< Reset relative sprite offset
	WID_SA_CENTRE,      ///< Toggle centre sprite.
	WID_SA_CROSSHAIR,   ///< Toggle crosshair.
};

#endif /* WIDGETS_NEWGRF_DEBUG_WIDGET_H */
