/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file intro_widget.h Types related to the intro widgets. */

#ifndef WIDGETS_INTRO_WIDGET_H
#define WIDGETS_INTRO_WIDGET_H

/** Widgets of the WC_SELECT_GAME. */
enum SelectGameIntroWidgets {
	SGI_GENERATE_GAME,
	SGI_LOAD_GAME,
	SGI_PLAY_SCENARIO,
	SGI_PLAY_HEIGHTMAP,
	SGI_EDIT_SCENARIO,
	SGI_PLAY_NETWORK,
	SGI_TEMPERATE_LANDSCAPE,
	SGI_ARCTIC_LANDSCAPE,
	SGI_TROPIC_LANDSCAPE,
	SGI_TOYLAND_LANDSCAPE,
	SGI_TRANSLATION_SELECTION,
	SGI_TRANSLATION,
	SGI_OPTIONS,
	SGI_DIFFICULTIES,
	SGI_SETTINGS_OPTIONS,
	SGI_GRF_SETTINGS,
	SGI_CONTENT_DOWNLOAD,
	SGI_AI_SETTINGS,
	SGI_EXIT,
};

#endif /* WIDGETS_INTRO_WIDGET_H */
