/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file music_widget.h Types related to the music widgets. */

#ifndef WIDGETS_MUSIC_WIDGET_H
#define WIDGETS_MUSIC_WIDGET_H

/** Widgets of the WC_MUSIC_TRACK_SELECTION. */
enum MusicTrackSelectionWidgets {
	MTSW_LIST_LEFT,
	MTSW_PLAYLIST,
	MTSW_LIST_RIGHT,
	MTSW_ALL,
	MTSW_OLD,
	MTSW_NEW,
	MTSW_EZY,
	MTSW_CUSTOM1,
	MTSW_CUSTOM2,
	MTSW_CLEAR,
};

/** Widgets of the WC_MUSIC_WINDOW. */
enum MusicWidgets {
	MW_PREV,
	MW_NEXT,
	MW_STOP,
	MW_PLAY,
	MW_SLIDERS,
	MW_MUSIC_VOL,
	MW_EFFECT_VOL,
	MW_BACKGROUND,
	MW_TRACK,
	MW_TRACK_NR,
	MW_TRACK_TITLE,
	MW_TRACK_NAME,
	MW_SHUFFLE,
	MW_PROGRAMME,
	MW_ALL,
	MW_OLD,
	MW_NEW,
	MW_EZY,
	MW_CUSTOM1,
	MW_CUSTOM2,
};

#endif /* WIDGETS_MUSIC_WIDGET_H */
