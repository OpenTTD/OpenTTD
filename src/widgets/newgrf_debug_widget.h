/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_debug_widget.h Types related to the newgrf debug widgets. */

#ifndef WIDGETS_NEWGRF_DEBUG_WIDGET_H
#define WIDGETS_NEWGRF_DEBUG_WIDGET_H

/** Widgets of the WC_NEWGRF_INSPECT. */
enum NewGRFInspectWidgets {
	NIW_CAPTION,   ///< The caption bar ofcourse
	NIW_PARENT,    ///< Inspect the parent
	NIW_MAINPANEL, ///< Panel widget containing the actual data
	NIW_SCROLLBAR, ///< Scrollbar
};

/** Widgets of the WC_SPRITE_ALIGNER. */
enum SpriteAlignerWidgets {
	SAW_CAPTION,  ///< Caption of the window
	SAW_PREVIOUS, ///< Skip to the previous sprite
	SAW_GOTO,     ///< Go to a given sprite
	SAW_NEXT,     ///< Skip to the next sprite
	SAW_UP,       ///< Move the sprite up
	SAW_LEFT,     ///< Move the sprite to the left
	SAW_RIGHT,    ///< Move the sprite to the right
	SAW_DOWN,     ///< Move the sprite down
	SAW_SPRITE,   ///< The actual sprite
	SAW_OFFSETS,  ///< The sprite offsets
	SAW_PICKER,   ///< Sprite picker
	SAW_LIST,     ///< Queried sprite list
	SAW_SCROLLBAR,///< Scrollbar for sprite list
};

#endif /* WIDGETS_NEWGRF_DEBUG_WIDGET_H */
