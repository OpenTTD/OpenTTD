/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file transparency_widget.h Types related to the transparency widgets. */

#ifndef WIDGETS_TRANSPARENCY_WIDGET_H
#define WIDGETS_TRANSPARENCY_WIDGET_H

/** Widgets of the WC_TRANSPARENCY_TOOLBAR. */
enum TransparencyToolbarWidgets {
	/* Button row. */
	TTW_WIDGET_BEGIN,                    ///< First toggle button.
	TTW_WIDGET_SIGNS = TTW_WIDGET_BEGIN, ///< Signs background transparency toggle button.
	TTW_WIDGET_TREES,                    ///< Trees transparency toggle button.
	TTW_WIDGET_HOUSES,                   ///< Houses transparency toggle button.
	TTW_WIDGET_INDUSTRIES,               ///< industries transparency toggle button.
	TTW_WIDGET_BUILDINGS,                ///< Company buildings and structures transparency toggle button.
	TTW_WIDGET_BRIDGES,                  ///< Bridges transparency toggle button.
	TTW_WIDGET_STRUCTURES,               ///< Object structure transparency toggle button.
	TTW_WIDGET_CATENARY,                 ///< Catenary transparency toggle button.
	TTW_WIDGET_LOADING,                  ///< Loading indicators transparency toggle button.
	TTW_WIDGET_END,                      ///< End of toggle buttons.

	/* Panel with buttons for invisibility */
	TTW_WIDGET_BUTTONS,                  ///< Panel with 'invisibility' buttons.
};

#endif /* WIDGETS_TRANSPARENCY_WIDGET_H */
