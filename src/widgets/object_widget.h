/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file object_widget.h Types related to the object widgets. */

#ifndef WIDGETS_OBJECT_WIDGET_H
#define WIDGETS_OBJECT_WIDGET_H

/** Widgets of the WC_BUILD_OBJECT. */
enum BuildObjectWidgets {
	BOW_CLASS_LIST,     ///< The list with classes.
	BOW_SCROLLBAR,      ///< The scrollbar associated with the list.
	BOW_OBJECT_MATRIX,  ///< The matrix with preview sprites.
	BOW_OBJECT_SPRITE,  ///< A preview sprite of the object.
	BOW_OBJECT_SIZE,    ///< The size of an object.
	BOW_INFO,           ///< Other information about the object (from the NewGRF).

	BOW_SELECT_MATRIX,  ///< Selection preview matrix of objects of a given class.
	BOW_SELECT_IMAGE,   ///< Preview image in the #BOW_SELECT_MATRIX.
	BOW_SELECT_SCROLL,  ///< Scrollbar next to the #BOW_SELECT_MATRIX.
};

#endif /* WIDGETS_OBJECT_WIDGET_H */
