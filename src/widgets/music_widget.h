/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file music_widget.h Types related to the music widgets. */

#ifndef WIDGETS_MUSIC_WIDGET_H
#define WIDGETS_MUSIC_WIDGET_H

/** Widgets of the #MusicTrackSelectionWindow class. */
enum MusicTrackSelectionWidgets : WidgetID {
	WID_MTS_CAPTION,    ///< Window caption.
	WID_MTS_LIST_LEFT,  ///< Left button.
	WID_MTS_PLAYLIST,   ///< Playlist.
	WID_MTS_LIST_RIGHT, ///< Right button.
	WID_MTS_MUSICSET,   ///< Music set selection.
	WID_MTS_ALL,        ///< All button.
	WID_MTS_OLD,        ///< Old button.
	WID_MTS_NEW,        ///< New button.
	WID_MTS_EZY,        ///< Ezy button.
	WID_MTS_CUSTOM1,    ///< Custom1 button.
	WID_MTS_CUSTOM2,    ///< Custom2 button.
	WID_MTS_CLEAR,      ///< Clear button.
};

/** Widgets of the #MusicWindow class. */
enum MusicWidgets : WidgetID {
	WID_M_PREV,        ///< Previous button.
	WID_M_NEXT,        ///< Next button.
	WID_M_STOP,        ///< Stop button.
	WID_M_PLAY,        ///< Play button.
	WID_M_SLIDERS,     ///< Sliders.
	WID_M_MUSIC_VOL,   ///< Music volume.
	WID_M_EFFECT_VOL,  ///< Effect volume.
	WID_M_BACKGROUND,  ///< Background of the window.
	WID_M_TRACK,       ///< Track playing.
	WID_M_TRACK_NR,    ///< Track number.
	WID_M_TRACK_TITLE, ///< Track title.
	WID_M_TRACK_NAME,  ///< Track name.
	WID_M_SHUFFLE,     ///< Shuffle button.
	WID_M_PROGRAMME,   ///< Program button.
	WID_M_ALL,         ///< All button.
	WID_M_OLD,         ///< Old button.
	WID_M_NEW,         ///< New button.
	WID_M_EZY,         ///< Ezy button.
	WID_M_CUSTOM1,     ///< Custom1 button.
	WID_M_CUSTOM2,     ///< Custom2 button.
};

#endif /* WIDGETS_MUSIC_WIDGET_H */
