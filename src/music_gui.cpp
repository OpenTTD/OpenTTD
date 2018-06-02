/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file music_gui.cpp GUI for the music playback. */

#include "stdafx.h"
#include "openttd.h"
#include "base_media_base.h"
#include "music/music_driver.hpp"
#include "window_gui.h"
#include "strings_func.h"
#include "window_func.h"
#include "sound_func.h"
#include "gfx_func.h"
#include "zoom_func.h"
#include "core/random_func.hpp"
#include "error.h"
#include "core/geometry_func.hpp"
#include "string_func.h"
#include "settings_type.h"
#include "settings_gui.h"
#include "widgets/dropdown_func.h"
#include "widgets/dropdown_type.h"

#include "widgets/music_widget.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "safeguards.h"

/**
 * Get the name of the song.
 * @param index of the song.
 * @return the name of the song.
 */
static const char *GetSongName(int index)
{
	return BaseMusic::GetUsedSet()->song_name[index];
}

/**
 * Get the track number of the song.
 * @param index of the song.
 * @return the track number of the song.
 */
static int GetTrackNumber(int index)
{
	return BaseMusic::GetUsedSet()->track_nr[index];
}

/** The currently played song */
static byte _music_wnd_cursong = 1;
/** Whether a song is currently played */
static bool _song_is_active = false;

/** Indices of the songs in the current playlist */
static byte _cur_playlist[NUM_SONGS_PLAYLIST + 1];

/** Indices of all songs */
static byte _playlist_all[NUM_SONGS_AVAILABLE + 1];
/** Indices of all old style songs */
static byte _playlist_old_style[NUM_SONGS_CLASS + 1];
/** Indices of all new style songs */
static byte _playlist_new_style[NUM_SONGS_CLASS + 1];
/** Indices of all ezy street songs */
static byte _playlist_ezy_street[NUM_SONGS_CLASS + 1];

assert_compile(lengthof(_settings_client.music.custom_1) == NUM_SONGS_PLAYLIST + 1);
assert_compile(lengthof(_settings_client.music.custom_2) == NUM_SONGS_PLAYLIST + 1);

/** The different playlists that can be played. */
static byte * const _playlists[] = {
	_playlist_all,
	_playlist_old_style,
	_playlist_new_style,
	_playlist_ezy_street,
	_settings_client.music.custom_1,
	_settings_client.music.custom_2,
};

/**
 * Validate a playlist.
 * @param playlist The playlist to validate.
 * @param last The last location in the list.
 */
void ValidatePlaylist(byte *playlist, byte *last)
{
	while (*playlist != 0 && playlist <= last) {
		/* Song indices are saved off-by-one so 0 is "nothing". */
		if (*playlist <= NUM_SONGS_AVAILABLE && !StrEmpty(GetSongName(*playlist - 1))) {
			playlist++;
			continue;
		}
		for (byte *p = playlist; *p != 0 && p <= last; p++) {
			p[0] = p[1];
		}
	}

	/* Make sure the list is null terminated. */
	*last = 0;
}

/** Prepare the playlists */
void InitializeMusic()
{
	uint j = 0;
	for (uint i = 0; i < NUM_SONGS_AVAILABLE; i++) {
		if (StrEmpty(GetSongName(i))) continue;
		_playlist_all[j++] = i + 1;
	}
	/* Terminate the list */
	_playlist_all[j] = 0;

	/* Now make the 'styled' playlists */
	for (uint k = 0; k < NUM_SONG_CLASSES; k++) {
		j = 0;
		for (uint i = 0; i < NUM_SONGS_CLASS; i++) {
			int id = k * NUM_SONGS_CLASS + i + 1;
			if (StrEmpty(GetSongName(id))) continue;
			_playlists[k + 1][j++] = id + 1;
		}
		/* Terminate the list */
		_playlists[k + 1][j] = 0;
	}

	ValidatePlaylist(_settings_client.music.custom_1, lastof(_settings_client.music.custom_1));
	ValidatePlaylist(_settings_client.music.custom_2, lastof(_settings_client.music.custom_2));

	if (BaseMusic::GetUsedSet()->num_available < _music_wnd_cursong) {
		/* If there are less songs than the currently played song,
		 * just pause and reset to no song. */
		_music_wnd_cursong = 0;
		_song_is_active = false;
	}
}

static void SkipToPrevSong()
{
	byte *b = _cur_playlist;
	byte *p = b;
	byte t;

	if (b[0] == 0) return; // empty playlist

	do p++; while (p[0] != 0); // find the end

	t = *--p; // and copy the bytes
	while (p != b) {
		p--;
		p[1] = p[0];
	}
	*b = t;

	_song_is_active = false;
}

static void SkipToNextSong()
{
	byte *b = _cur_playlist;
	byte t;

	t = b[0];
	if (t != 0) {
		while (b[1] != 0) {
			b[0] = b[1];
			b++;
		}
		b[0] = t;
	}

	_song_is_active = false;
}

static void MusicVolumeChanged(byte new_vol)
{
	MusicDriver::GetInstance()->SetVolume(new_vol);
}

static void DoPlaySong()
{
	char filename[MAX_PATH];
	if (FioFindFullPath(filename, lastof(filename), BASESET_DIR, BaseMusic::GetUsedSet()->files[_music_wnd_cursong - 1].filename) == NULL) {
		FioFindFullPath(filename, lastof(filename), OLD_GM_DIR, BaseMusic::GetUsedSet()->files[_music_wnd_cursong - 1].filename);
	}
	MusicDriver::GetInstance()->PlaySong(filename);
	SetWindowDirty(WC_MUSIC_WINDOW, 0);
}

static void DoStopMusic()
{
	MusicDriver::GetInstance()->StopSong();
	SetWindowDirty(WC_MUSIC_WINDOW, 0);
}

/** Reload the active playlist data from playlist selection and shuffle setting */
static void ResetPlaylist()
{
	uint i = 0;
	uint j = 0;

	memset(_cur_playlist, 0, sizeof(_cur_playlist));
	do {
		/* File is the index into the file table of the music set. The play list uses 0 as 'no entry',
		 * so we need to subtract 1. In case of 'no entry' (file = -1), just skip adding it outright. */
		int file = _playlists[_settings_client.music.playlist][i] - 1;
		if (file >= 0) {
			const char *filename = BaseMusic::GetUsedSet()->files[file].filename;
			/* We are now checking for the existence of that file prior
			 * to add it to the list of available songs */
			if (!StrEmpty(filename) && FioCheckFileExists(filename, BASESET_DIR)) {
				_cur_playlist[j] = _playlists[_settings_client.music.playlist][i];
				j++;
			}
		}
	} while (_playlists[_settings_client.music.playlist][++i] != 0 && j < lengthof(_cur_playlist) - 1);

	/* Do not shuffle when on the intro-start window, as the song to play has to be the original TTD Theme*/
	if (_settings_client.music.shuffle && _game_mode != GM_MENU) {
		i = 500;
		do {
			uint32 r = InteractiveRandom();
			byte *a = &_cur_playlist[GB(r, 0, 5)];
			byte *b = &_cur_playlist[GB(r, 8, 5)];

			if (*a != 0 && *b != 0) {
				byte t = *a;
				*a = *b;
				*b = t;
			}
		} while (--i);
	}
}

static void StopMusic()
{
	_music_wnd_cursong = 0;
	DoStopMusic();
	_song_is_active = false;
	SetWindowWidgetDirty(WC_MUSIC_WINDOW, 0, 9);
}

/** Begin playing the next song on the playlist */
static void PlayPlaylistSong()
{
	if (_cur_playlist[0] == 0) {
		ResetPlaylist();
		/* if there is not songs in the playlist, it may indicate
		 * no file on the gm folder, or even no gm folder.
		 * Stop the playback, then */
		if (_cur_playlist[0] == 0) {
			_song_is_active = false;
			_music_wnd_cursong = 0;
			_settings_client.music.playing = false;
			return;
		}
	}
	_music_wnd_cursong = _cur_playlist[0];
	DoPlaySong();
	_song_is_active = true;

	SetWindowWidgetDirty(WC_MUSIC_WINDOW, 0, 9);
}

void ResetMusic()
{
	_music_wnd_cursong = 1;
	DoPlaySong();
}

/**
 * Check music playback status and start/stop/song-finished.
 * Called from main loop.
 */
void MusicLoop()
{
	if (!_settings_client.music.playing && _song_is_active) {
		StopMusic();
	} else if (_settings_client.music.playing && !_song_is_active) {
		PlayPlaylistSong();
	}

	if (!_song_is_active) return;

	if (!MusicDriver::GetInstance()->IsSongPlaying()) {
		if (_game_mode != GM_MENU) {
			StopMusic();
			SkipToNextSong();
			PlayPlaylistSong();
		} else {
			ResetMusic();
		}
	}
}

static void SelectPlaylist(byte list)
{
	_settings_client.music.playlist = list;
	InvalidateWindowData(WC_MUSIC_TRACK_SELECTION, 0);
	InvalidateWindowData(WC_MUSIC_WINDOW, 0);
}

/**
 * Change the configured music set and reset playback
 * @param index Index of music set to switch to
 */
void ChangeMusicSet(int index)
{
	if (BaseMusic::GetIndexOfUsedSet() == index) return;

	/* Resume playback after switching?
	 * Always if music is already playing, and also if the user is switching
	 * away from an empty music set.
	 * If the user switches away from an empty set, assume it's because they
	 * want to hear music now. */
	bool shouldplay = _song_is_active || (BaseMusic::GetUsedSet()->num_available == 0);
	StopMusic();

	const char *name = BaseMusic::GetSet(index)->name;
	BaseMusic::SetSet(name);
	free(BaseMusic::ini_set);
	BaseMusic::ini_set = stredup(name);

	InitializeMusic();
	ResetPlaylist();
	_settings_client.music.playing = shouldplay;

	InvalidateWindowData(WC_MUSIC_TRACK_SELECTION, 0);
	InvalidateWindowData(WC_MUSIC_WINDOW, 0);
	InvalidateWindowData(WC_GAME_OPTIONS, WN_GAME_OPTIONS_GAME_OPTIONS, 0, true);
}

struct MusicTrackSelectionWindow : public Window {
	MusicTrackSelectionWindow(WindowDesc *desc, WindowNumber number) : Window(desc)
	{
		this->InitNested(number);
		this->LowerWidget(WID_MTS_LIST_LEFT);
		this->LowerWidget(WID_MTS_LIST_RIGHT);
		this->SetWidgetDisabledState(WID_MTS_CLEAR, _settings_client.music.playlist <= 3);
		this->LowerWidget(WID_MTS_ALL + _settings_client.music.playlist);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_MTS_PLAYLIST:
				SetDParam(0, STR_MUSIC_PLAYLIST_ALL + _settings_client.music.playlist);
				break;
			case WID_MTS_CAPTION:
				SetDParamStr(0, BaseMusic::GetUsedSet()->name);
				break;
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		for (int i = 0; i < 6; i++) {
			this->SetWidgetLoweredState(WID_MTS_ALL + i, i == _settings_client.music.playlist);
		}
		this->SetWidgetDisabledState(WID_MTS_CLEAR, _settings_client.music.playlist <= 3);
		this->SetDirty();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_MTS_PLAYLIST: {
				Dimension d = {0, 0};

				for (int i = 0; i < 6; i++) {
					SetDParam(0, STR_MUSIC_PLAYLIST_ALL + i);
					d = maxdim(d, GetStringBoundingBox(STR_PLAYLIST_PROGRAM));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case WID_MTS_LIST_LEFT: case WID_MTS_LIST_RIGHT: {
				Dimension d = {0, 0};

				for (uint i = 0; i < NUM_SONGS_AVAILABLE; i++) {
					const char *song_name = GetSongName(i);
					if (StrEmpty(song_name)) continue;

					SetDParam(0, GetTrackNumber(i));
					SetDParam(1, 2);
					SetDParamStr(2, GetSongName(i));
					Dimension d2 = GetStringBoundingBox(STR_PLAYLIST_TRACK_NAME);
					d.width = max(d.width, d2.width);
					d.height += d2.height;
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_MTS_LIST_LEFT: {
				GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, PC_BLACK);

				int y = r.top + WD_FRAMERECT_TOP;
				for (uint i = 0; i < NUM_SONGS_AVAILABLE; i++) {
					const char *song_name = GetSongName(i);
					if (StrEmpty(song_name)) continue;

					SetDParam(0, GetTrackNumber(i));
					SetDParam(1, 2);
					SetDParamStr(2, song_name);
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_PLAYLIST_TRACK_NAME);
					y += FONT_HEIGHT_SMALL;
				}
				break;
			}

			case WID_MTS_LIST_RIGHT: {
				GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, PC_BLACK);

				int y = r.top + WD_FRAMERECT_TOP;
				for (const byte *p = _playlists[_settings_client.music.playlist]; *p != 0; p++) {
					uint i = *p - 1;
					SetDParam(0, GetTrackNumber(i));
					SetDParam(1, 2);
					SetDParamStr(2, GetSongName(i));
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_PLAYLIST_TRACK_NAME);
					y += FONT_HEIGHT_SMALL;
				}
				break;
			}
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_MTS_LIST_LEFT: { // add to playlist
				int y = this->GetRowFromWidget(pt.y, widget, 0, FONT_HEIGHT_SMALL);

				if (_settings_client.music.playlist < 4) return;
				if (!IsInsideMM(y, 0, BaseMusic::GetUsedSet()->num_available)) return;

				byte *p = _playlists[_settings_client.music.playlist];
				for (uint i = 0; i != NUM_SONGS_PLAYLIST - 1; i++) {
					if (p[i] == 0) {
						/* Find the actual song number */
						for (uint j = 0; j < NUM_SONGS_AVAILABLE; j++) {
							if (GetTrackNumber(j) == y + 1) {
								p[i] = j + 1;
								break;
							}
						}
						p[i + 1] = 0;
						this->SetDirty();
						ResetPlaylist();
						break;
					}
				}
				break;
			}

			case WID_MTS_LIST_RIGHT: { // remove from playlist
				int y = this->GetRowFromWidget(pt.y, widget, 0, FONT_HEIGHT_SMALL);

				if (_settings_client.music.playlist < 4) return;
				if (!IsInsideMM(y, 0, NUM_SONGS_PLAYLIST)) return;

				byte *p = _playlists[_settings_client.music.playlist];
				for (uint i = y; i != NUM_SONGS_PLAYLIST - 1; i++) {
					p[i] = p[i + 1];
				}

				this->SetDirty();
				ResetPlaylist();
				break;
			}

			case WID_MTS_MUSICSET: {
				int selected = 0;
				DropDownList *dropdown = BuildMusicSetDropDownList(&selected);
				ShowDropDownList(this, dropdown, selected, widget, 0, true, false);
				break;
			}

			case WID_MTS_CLEAR: // clear
				for (uint i = 0; _playlists[_settings_client.music.playlist][i] != 0; i++) _playlists[_settings_client.music.playlist][i] = 0;
				this->SetDirty();
				StopMusic();
				ResetPlaylist();
				break;

			case WID_MTS_ALL: case WID_MTS_OLD: case WID_MTS_NEW:
			case WID_MTS_EZY: case WID_MTS_CUSTOM1: case WID_MTS_CUSTOM2: // set playlist
				SelectPlaylist(widget - WID_MTS_ALL);
				StopMusic();
				ResetPlaylist();
				break;
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case WID_MTS_MUSICSET:
				ChangeMusicSet(index);
				break;
			default:
				NOT_REACHED();
				break;
		}
	}
};

static const NWidgetPart _nested_music_track_selection_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_MTS_CAPTION), SetDataTip(STR_PLAYLIST_MUSIC_SELECTION_SETNAME, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_MTS_MUSICSET), SetDataTip(STR_PLAYLIST_CHANGE_SET, STR_PLAYLIST_TOOLTIP_CHANGE_SET),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_HORIZONTAL), SetPIP(2, 4, 2),
			/* Left panel. */
			NWidget(NWID_VERTICAL),
				NWidget(WWT_LABEL, COLOUR_GREY), SetDataTip(STR_PLAYLIST_TRACK_INDEX, STR_NULL),
				NWidget(WWT_PANEL, COLOUR_GREY, WID_MTS_LIST_LEFT), SetMinimalSize(180, 194), SetDataTip(0x0, STR_PLAYLIST_TOOLTIP_CLICK_TO_ADD_TRACK), EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 2),
			EndContainer(),
			/* Middle buttons. */
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetMinimalSize(60, 30), // Space above the first button from the title bar.
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_MTS_ALL), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_ALL, STR_MUSIC_TOOLTIP_SELECT_ALL_TRACKS_PROGRAM),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_MTS_OLD), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_OLD_STYLE, STR_MUSIC_TOOLTIP_SELECT_OLD_STYLE_MUSIC),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_MTS_NEW), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_NEW_STYLE, STR_MUSIC_TOOLTIP_SELECT_NEW_STYLE_MUSIC),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_MTS_EZY), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_EZY_STREET, STR_MUSIC_TOOLTIP_SELECT_EZY_STREET_STYLE),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_MTS_CUSTOM1), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_CUSTOM_1, STR_MUSIC_TOOLTIP_SELECT_CUSTOM_1_USER_DEFINED),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_MTS_CUSTOM2), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_CUSTOM_2, STR_MUSIC_TOOLTIP_SELECT_CUSTOM_2_USER_DEFINED),
				NWidget(NWID_SPACER), SetMinimalSize(0, 16), // Space above 'clear' button
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_MTS_CLEAR), SetFill(1, 0), SetDataTip(STR_PLAYLIST_CLEAR, STR_PLAYLIST_TOOLTIP_CLEAR_CURRENT_PROGRAM_CUSTOM1),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
			/* Right panel. */
			NWidget(NWID_VERTICAL),
				NWidget(WWT_LABEL, COLOUR_GREY, WID_MTS_PLAYLIST), SetDataTip(STR_PLAYLIST_PROGRAM, STR_NULL),
				NWidget(WWT_PANEL, COLOUR_GREY, WID_MTS_LIST_RIGHT), SetMinimalSize(180, 194), SetDataTip(0x0, STR_PLAYLIST_TOOLTIP_CLICK_TO_REMOVE_TRACK), EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 2),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _music_track_selection_desc(
	WDP_AUTO, "music_track", 0, 0,
	WC_MUSIC_TRACK_SELECTION, WC_NONE,
	0,
	_nested_music_track_selection_widgets, lengthof(_nested_music_track_selection_widgets)
);

static void ShowMusicTrackSelection()
{
	AllocateWindowDescFront<MusicTrackSelectionWindow>(&_music_track_selection_desc, 0);
}

struct MusicWindow : public Window {
	static const int slider_width = 3;

	MusicWindow(WindowDesc *desc, WindowNumber number) : Window(desc)
	{
		this->InitNested(number);
		this->LowerWidget(_settings_client.music.playlist + WID_M_ALL);
		this->SetWidgetLoweredState(WID_M_SHUFFLE, _settings_client.music.shuffle);

		UpdateDisabledButtons();
	}

	void UpdateDisabledButtons()
	{
		/* Disable music control widgets if there is no music
		 * -- except Programme button! So you can still select a music set. */
		this->SetWidgetsDisabledState(
			BaseMusic::GetUsedSet()->num_available == 0,
			WID_M_PREV, WID_M_NEXT, WID_M_STOP, WID_M_PLAY, WID_M_SHUFFLE,
			WID_M_ALL, WID_M_OLD, WID_M_NEW, WID_M_EZY, WID_M_CUSTOM1, WID_M_CUSTOM2,
			WIDGET_LIST_END
			);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			/* Make sure that WID_M_SHUFFLE and WID_M_PROGRAMME have the same size.
			 * This can't be done by using NC_EQUALSIZE as the WID_M_INFO is
			 * between those widgets and of different size. */
			case WID_M_SHUFFLE: case WID_M_PROGRAMME: {
				Dimension d = maxdim(GetStringBoundingBox(STR_MUSIC_PROGRAM), GetStringBoundingBox(STR_MUSIC_SHUFFLE));
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case WID_M_TRACK_NR: {
				Dimension d = GetStringBoundingBox(STR_MUSIC_TRACK_NONE);
				d.width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				d.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				*size = maxdim(*size, d);
				break;
			}

			case WID_M_TRACK_NAME: {
				Dimension d = GetStringBoundingBox(STR_MUSIC_TITLE_NONE);
				for (uint i = 0; i < NUM_SONGS_AVAILABLE; i++) {
					SetDParamStr(0, GetSongName(i));
					d = maxdim(d, GetStringBoundingBox(STR_MUSIC_TITLE_NAME));
				}
				d.width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				d.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				*size = maxdim(*size, d);
				break;
			}

			/* Hack-ish: set the proper widget data; only needs to be done once
			 * per (Re)Init as that's the only time the language changes. */
			case WID_M_PREV: this->GetWidget<NWidgetCore>(WID_M_PREV)->widget_data = _current_text_dir == TD_RTL ? SPR_IMG_SKIP_TO_NEXT : SPR_IMG_SKIP_TO_PREV; break;
			case WID_M_NEXT: this->GetWidget<NWidgetCore>(WID_M_NEXT)->widget_data = _current_text_dir == TD_RTL ? SPR_IMG_SKIP_TO_PREV : SPR_IMG_SKIP_TO_NEXT; break;
			case WID_M_PLAY: this->GetWidget<NWidgetCore>(WID_M_PLAY)->widget_data = _current_text_dir == TD_RTL ? SPR_IMG_PLAY_MUSIC_RTL : SPR_IMG_PLAY_MUSIC; break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_M_TRACK_NR: {
				GfxFillRect(r.left + 1, r.top + 1, r.right, r.bottom, PC_BLACK);
				if (BaseMusic::GetUsedSet()->num_available == 0) {
					break;
				}
				StringID str = STR_MUSIC_TRACK_NONE;
				if (_song_is_active != 0 && _music_wnd_cursong != 0) {
					SetDParam(0, GetTrackNumber(_music_wnd_cursong - 1));
					SetDParam(1, 2);
					str = STR_MUSIC_TRACK_DIGIT;
				}
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, str);
				break;
			}

			case WID_M_TRACK_NAME: {
				GfxFillRect(r.left, r.top + 1, r.right - 1, r.bottom, PC_BLACK);
				StringID str = STR_MUSIC_TITLE_NONE;
				if (BaseMusic::GetUsedSet()->num_available == 0) {
					str = STR_MUSIC_TITLE_NOMUSIC;
				} else if (_song_is_active != 0 && _music_wnd_cursong != 0) {
					str = STR_MUSIC_TITLE_NAME;
					SetDParamStr(0, GetSongName(_music_wnd_cursong - 1));
				}
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, str, TC_FROMSTRING, SA_HOR_CENTER);
				break;
			}

			case WID_M_MUSIC_VOL: case WID_M_EFFECT_VOL: {
				int sw = ScaleGUITrad(slider_width);
				int hsw = sw / 2;
				DrawFrameRect(r.left + hsw, r.top + 2, r.right - hsw, r.bottom - 2, COLOUR_GREY, FR_LOWERED);
				byte volume = (widget == WID_M_MUSIC_VOL) ? _settings_client.music.music_vol : _settings_client.music.effect_vol;
				if (_current_text_dir == TD_RTL) volume = 127 - volume;
				int x = r.left + (volume * (r.right - r.left - sw) / 127);
				DrawFrameRect(x, r.top, x + sw, r.bottom, COLOUR_GREY, FR_NONE);
				break;
			}
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		for (int i = 0; i < 6; i++) {
			this->SetWidgetLoweredState(WID_M_ALL + i, i == _settings_client.music.playlist);
		}

		UpdateDisabledButtons();

		this->SetDirty();
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_M_PREV: // skip to prev
				if (!_song_is_active) return;
				SkipToPrevSong();
				this->SetDirty();
				break;

			case WID_M_NEXT: // skip to next
				if (!_song_is_active) return;
				SkipToNextSong();
				this->SetDirty();
				break;

			case WID_M_STOP: // stop playing
				_settings_client.music.playing = false;
				break;

			case WID_M_PLAY: // start playing
				_settings_client.music.playing = true;
				break;

			case WID_M_MUSIC_VOL: case WID_M_EFFECT_VOL: { // volume sliders
				int x = pt.x - this->GetWidget<NWidgetBase>(widget)->pos_x;

				byte *vol = (widget == WID_M_MUSIC_VOL) ? &_settings_client.music.music_vol : &_settings_client.music.effect_vol;

				byte new_vol = x * 127 / this->GetWidget<NWidgetBase>(widget)->current_x;
				if (_current_text_dir == TD_RTL) new_vol = 127 - new_vol;
				/* Clamp to make sure min and max are properly settable */
				if (new_vol > 124) new_vol = 127;
				if (new_vol < 3) new_vol = 0;
				if (new_vol != *vol) {
					*vol = new_vol;
					if (widget == WID_M_MUSIC_VOL) MusicVolumeChanged(new_vol);
					this->SetDirty();
				}

				_left_button_clicked = false;
				break;
			}

			case WID_M_SHUFFLE: // toggle shuffle
				_settings_client.music.shuffle ^= 1;
				this->SetWidgetLoweredState(WID_M_SHUFFLE, _settings_client.music.shuffle);
				this->SetWidgetDirty(WID_M_SHUFFLE);
				StopMusic();
				ResetPlaylist();
				this->SetDirty();
				break;

			case WID_M_PROGRAMME: // show track selection
				ShowMusicTrackSelection();
				break;

			case WID_M_ALL: case WID_M_OLD: case WID_M_NEW:
			case WID_M_EZY: case WID_M_CUSTOM1: case WID_M_CUSTOM2: // playlist
				SelectPlaylist(widget - WID_M_ALL);
				StopMusic();
				ResetPlaylist();
				this->SetDirty();
				break;
		}
	}
};

static const NWidgetPart _nested_music_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_MUSIC_JAZZ_JUKEBOX_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),

	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_PANEL, COLOUR_GREY, -1), SetFill(1, 1), EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_M_PREV), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_SKIP_TO_PREV, STR_MUSIC_TOOLTIP_SKIP_TO_PREVIOUS_TRACK),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_M_NEXT), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_SKIP_TO_NEXT, STR_MUSIC_TOOLTIP_SKIP_TO_NEXT_TRACK_IN_SELECTION),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_M_STOP), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_STOP_MUSIC, STR_MUSIC_TOOLTIP_STOP_PLAYING_MUSIC),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_M_PLAY), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_PLAY_MUSIC, STR_MUSIC_TOOLTIP_START_PLAYING_MUSIC),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, -1), SetFill(1, 1), EndContainer(),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_M_SLIDERS),
			NWidget(NWID_HORIZONTAL), SetPIP(20, 20, 20),
				NWidget(NWID_VERTICAL),
					NWidget(WWT_LABEL, COLOUR_GREY, -1), SetFill(1, 0), SetDataTip(STR_MUSIC_MUSIC_VOLUME, STR_NULL),
					NWidget(WWT_EMPTY, COLOUR_GREY, WID_M_MUSIC_VOL), SetMinimalSize(67, 0), SetMinimalTextLines(1, 0), SetFill(1, 0), SetDataTip(0x0, STR_MUSIC_TOOLTIP_DRAG_SLIDERS_TO_SET_MUSIC),
					NWidget(NWID_HORIZONTAL),
						NWidget(WWT_LABEL, COLOUR_GREY, -1), SetDataTip(STR_MUSIC_RULER_MIN, STR_NULL),
						NWidget(WWT_LABEL, COLOUR_GREY, -1), SetDataTip(STR_MUSIC_RULER_MARKER, STR_NULL), SetFill(1, 0),
						NWidget(WWT_LABEL, COLOUR_GREY, -1), SetDataTip(STR_MUSIC_RULER_MARKER, STR_NULL), SetFill(1, 0),
						NWidget(WWT_LABEL, COLOUR_GREY, -1), SetDataTip(STR_MUSIC_RULER_MARKER, STR_NULL), SetFill(1, 0),
						NWidget(WWT_LABEL, COLOUR_GREY, -1), SetDataTip(STR_MUSIC_RULER_MARKER, STR_NULL), SetFill(1, 0),
						NWidget(WWT_LABEL, COLOUR_GREY, -1), SetDataTip(STR_MUSIC_RULER_MARKER, STR_NULL), SetFill(1, 0),
						NWidget(WWT_LABEL, COLOUR_GREY, -1), SetDataTip(STR_MUSIC_RULER_MAX, STR_NULL),
					EndContainer(),
				EndContainer(),
				NWidget(NWID_VERTICAL),
					NWidget(WWT_LABEL, COLOUR_GREY, -1), SetFill(1, 0), SetDataTip(STR_MUSIC_EFFECTS_VOLUME, STR_NULL),
					NWidget(WWT_EMPTY, COLOUR_GREY, WID_M_EFFECT_VOL), SetMinimalSize(67, 0), SetMinimalTextLines(1, 0), SetFill(1, 0), SetDataTip(0x0, STR_MUSIC_TOOLTIP_DRAG_SLIDERS_TO_SET_MUSIC),
					NWidget(NWID_HORIZONTAL),
						NWidget(WWT_LABEL, COLOUR_GREY, -1), SetDataTip(STR_MUSIC_RULER_MIN, STR_NULL),
						NWidget(WWT_LABEL, COLOUR_GREY, -1), SetDataTip(STR_MUSIC_RULER_MARKER, STR_NULL), SetFill(1, 0),
						NWidget(WWT_LABEL, COLOUR_GREY, -1), SetDataTip(STR_MUSIC_RULER_MARKER, STR_NULL), SetFill(1, 0),
						NWidget(WWT_LABEL, COLOUR_GREY, -1), SetDataTip(STR_MUSIC_RULER_MARKER, STR_NULL), SetFill(1, 0),
						NWidget(WWT_LABEL, COLOUR_GREY, -1), SetDataTip(STR_MUSIC_RULER_MARKER, STR_NULL), SetFill(1, 0),
						NWidget(WWT_LABEL, COLOUR_GREY, -1), SetDataTip(STR_MUSIC_RULER_MARKER, STR_NULL), SetFill(1, 0),
						NWidget(WWT_LABEL, COLOUR_GREY, -1), SetDataTip(STR_MUSIC_RULER_MAX, STR_NULL),
					EndContainer(),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_M_BACKGROUND),
		NWidget(NWID_HORIZONTAL), SetPIP(6, 0, 6),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetFill(0, 1),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_M_SHUFFLE), SetMinimalSize(50, 8), SetDataTip(STR_MUSIC_SHUFFLE, STR_MUSIC_TOOLTIP_TOGGLE_PROGRAM_SHUFFLE),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
			NWidget(NWID_VERTICAL), SetPadding(0, 0, 3, 3),
				NWidget(WWT_LABEL, COLOUR_GREY, WID_M_TRACK), SetFill(0, 0), SetDataTip(STR_MUSIC_TRACK, STR_NULL),
				NWidget(WWT_PANEL, COLOUR_GREY, WID_M_TRACK_NR), EndContainer(),
			EndContainer(),
			NWidget(NWID_VERTICAL), SetPadding(0, 3, 3, 0),
				NWidget(WWT_LABEL, COLOUR_GREY, WID_M_TRACK_TITLE), SetFill(1, 0), SetDataTip(STR_MUSIC_XTITLE, STR_NULL),
				NWidget(WWT_PANEL, COLOUR_GREY, WID_M_TRACK_NAME), SetFill(1, 0), EndContainer(),
			EndContainer(),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetFill(0, 1),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_M_PROGRAMME), SetMinimalSize(50, 8), SetDataTip(STR_MUSIC_PROGRAM, STR_MUSIC_TOOLTIP_SHOW_MUSIC_TRACK_SELECTION),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_M_ALL), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_ALL, STR_MUSIC_TOOLTIP_SELECT_ALL_TRACKS_PROGRAM),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_M_OLD), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_OLD_STYLE, STR_MUSIC_TOOLTIP_SELECT_OLD_STYLE_MUSIC),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_M_NEW), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_NEW_STYLE, STR_MUSIC_TOOLTIP_SELECT_NEW_STYLE_MUSIC),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_M_EZY), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_EZY_STREET, STR_MUSIC_TOOLTIP_SELECT_EZY_STREET_STYLE),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_M_CUSTOM1), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_CUSTOM_1, STR_MUSIC_TOOLTIP_SELECT_CUSTOM_1_USER_DEFINED),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_M_CUSTOM2), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_CUSTOM_2, STR_MUSIC_TOOLTIP_SELECT_CUSTOM_2_USER_DEFINED),
	EndContainer(),
};

static WindowDesc _music_window_desc(
	WDP_AUTO, "music", 0, 0,
	WC_MUSIC_WINDOW, WC_NONE,
	0,
	_nested_music_window_widgets, lengthof(_nested_music_window_widgets)
);

void ShowMusicWindow()
{
	AllocateWindowDescFront<MusicWindow>(&_music_window_desc, 0);
}
