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
#include "fileio_func.h"
#include "music.h"
#include "music/music_driver.hpp"
#include "window_gui.h"
#include "strings_func.h"
#include "window_func.h"
#include "sound_func.h"
#include "gfx_func.h"
#include "core/random_func.hpp"

#include "table/strings.h"
#include "table/sprites.h"

static byte _music_wnd_cursong;
static bool _song_is_active;
static byte _cur_playlist[NUM_SONGS_PLAYLIST];



static byte _playlist_all[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 0
};

static byte _playlist_old_style[] = {
	1, 8, 2, 9, 14, 15, 19, 13, 0
};

static byte _playlist_new_style[] = {
	6, 11, 10, 17, 21, 18, 5, 0
};

static byte _playlist_ezy_street[] = {
	12, 7, 16, 3, 20, 4, 0
};

static byte * const _playlists[] = {
	_playlist_all,
	_playlist_old_style,
	_playlist_new_style,
	_playlist_ezy_street,
	msf.custom_1,
	msf.custom_2,
};

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
	_music_driver->SetVolume(new_vol);
}

static void DoPlaySong()
{
	char filename[MAX_PATH];
	FioFindFullPath(filename, lengthof(filename), GM_DIR,
			_origin_songs_specs[_music_wnd_cursong - 1].filename);
	_music_driver->PlaySong(filename);
}

static void DoStopMusic()
{
	_music_driver->StopSong();
}

static void SelectSongToPlay()
{
	uint i = 0;
	uint j = 0;

	memset(_cur_playlist, 0, sizeof(_cur_playlist));
	do {
		/* We are now checking for the existence of that file prior
		 * to add it to the list of available songs */
		if (FioCheckFileExists(_origin_songs_specs[_playlists[msf.playlist][i] - 1].filename, GM_DIR)) {
			_cur_playlist[j] = _playlists[msf.playlist][i];
			j++;
		}
	} while (_playlists[msf.playlist][++i] != 0 && j < lengthof(_cur_playlist) - 1);

	/* Do not shuffle when on the intro-start window, as the song to play has to be the original TTD Theme*/
	if (msf.shuffle && _game_mode != GM_MENU) {
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

static void PlayPlaylistSong()
{
	if (_cur_playlist[0] == 0) {
		SelectSongToPlay();
		/* if there is not songs in the playlist, it may indicate
		 * no file on the gm folder, or even no gm folder.
		 * Stop the playback, then */
		if (_cur_playlist[0] == 0) {
			_song_is_active = false;
			_music_wnd_cursong = 0;
			msf.playing = false;
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

void MusicLoop()
{
	if (!msf.playing && _song_is_active) {
		StopMusic();
	} else if (msf.playing && !_song_is_active) {
		PlayPlaylistSong();
	}

	if (!_song_is_active) return;

	if (!_music_driver->IsSongPlaying()) {
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
	msf.playlist = list;
	InvalidateWindowData(WC_MUSIC_TRACK_SELECTION, 0);
	InvalidateWindowData(WC_MUSIC_WINDOW, 0);
}

enum MusicTrackSelectionWidgets {
	MTSW_CLOSE,
	MTSW_CAPTION,
	MTSW_BACKGROUND,
	MTSW_TRACKLIST,
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

struct MusicTrackSelectionWindow : public Window {
	MusicTrackSelectionWindow(const WindowDesc *desc, WindowNumber number) : Window()
	{
		this->InitNested(desc, number);
		this->LowerWidget(MTSW_LIST_LEFT);
		this->LowerWidget(MTSW_LIST_RIGHT);
		this->SetWidgetDisabledState(MTSW_CLEAR, msf.playlist <= 3);
		this->LowerWidget(MTSW_ALL + msf.playlist);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case MTSW_PLAYLIST:
				SetDParam(0, STR_MUSIC_PLAYLIST_ALL + msf.playlist);
				break;
		}
	}

	virtual void OnInvalidateData(int data = 0)
	{
		for (int i = 0; i < 6; i++) {
			this->SetWidgetLoweredState(MTSW_ALL + i, i == msf.playlist);
		}
		this->SetWidgetDisabledState(MTSW_CLEAR, msf.playlist <= 3);
		this->SetDirty();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		switch (widget) {
			case MTSW_PLAYLIST: {
				Dimension d = {0, 0};

				for (int i = 0; i < 6; i++) {
					SetDParam(0, STR_MUSIC_PLAYLIST_ALL + i);
					d = maxdim(d, GetStringBoundingBox(STR_PLAYLIST_PROGRAM));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
			} break;

			case MTSW_LIST_LEFT: case MTSW_LIST_RIGHT: {
				Dimension d = {0, 0};

				for (uint i = 1; i <= NUM_SONGS_AVAILABLE; i++) {
					SetDParam(0, i);
					SetDParam(1, 2);
					SetDParam(2, SPECSTR_SONGNAME);
					SetDParam(3, i);
					Dimension d2 = GetStringBoundingBox(STR_PLAYLIST_TRACK_NAME);
					d.width = max(d.width, d2.width);
					d.height += d2.height;
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
			} break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case MTSW_LIST_LEFT: {
				GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, 0);

				int y = r.top + WD_FRAMERECT_TOP;
				for (uint i = 1; i <= NUM_SONGS_AVAILABLE; i++) {
					SetDParam(0, i);
					SetDParam(1, 2);
					SetDParam(2, SPECSTR_SONGNAME);
					SetDParam(3, i);
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_PLAYLIST_TRACK_NAME);
					y += FONT_HEIGHT_SMALL;
				}
			} break;

			case MTSW_LIST_RIGHT: {
				GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, 0);

				int y = r.top + WD_FRAMERECT_TOP;
				for (const byte *p = _playlists[msf.playlist]; *p != 0; p++) {
					uint i = *p;
					SetDParam(0, i);
					SetDParam(1, 2);
					SetDParam(2, SPECSTR_SONGNAME);
					SetDParam(3, i);
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_PLAYLIST_TRACK_NAME);
					y += FONT_HEIGHT_SMALL;
				}
			} break;
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case MTSW_LIST_LEFT: { // add to playlist
				int y = (pt.y - this->GetWidget<NWidgetBase>(widget)->pos_y) / FONT_HEIGHT_SMALL;

				if (msf.playlist < 4) return;
				if (!IsInsideMM(y, 0, NUM_SONGS_AVAILABLE)) return;

				byte *p = _playlists[msf.playlist];
				for (uint i = 0; i != NUM_SONGS_PLAYLIST - 1; i++) {
					if (p[i] == 0) {
						p[i] = y + 1;
						p[i + 1] = 0;
						this->SetDirty();
						SelectSongToPlay();
						break;
					}
				}
			} break;

			case MTSW_LIST_RIGHT: { // remove from playlist
				int y = (pt.y - this->GetWidget<NWidgetBase>(widget)->pos_y) / FONT_HEIGHT_SMALL;

				if (msf.playlist < 4) return;
				if (!IsInsideMM(y, 0, NUM_SONGS_AVAILABLE)) return;

				byte *p = _playlists[msf.playlist];
				for (uint i = y; i != NUM_SONGS_PLAYLIST - 1; i++) {
					p[i] = p[i + 1];
				}

				this->SetDirty();
				SelectSongToPlay();
			} break;

			case MTSW_CLEAR: // clear
				_playlists[msf.playlist][0] = 0;
				this->SetDirty();
				StopMusic();
				SelectSongToPlay();
				break;

			case MTSW_ALL: case MTSW_OLD: case MTSW_NEW:
			case MTSW_EZY: case MTSW_CUSTOM1: case MTSW_CUSTOM2: // set playlist
				SelectPlaylist(widget - MTSW_ALL);
				StopMusic();
				SelectSongToPlay();
				break;
		}
	}
};

static const NWidgetPart _nested_music_track_selection_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, MTSW_CLOSE),
		NWidget(WWT_CAPTION, COLOUR_GREY, MTSW_CAPTION), SetDataTip(STR_PLAYLIST_MUSIC_PROGRAM_SELECTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, MTSW_BACKGROUND),
		NWidget(NWID_HORIZONTAL), SetPIP(2, 4, 2),
			/* Left panel. */
			NWidget(NWID_VERTICAL),
				NWidget(WWT_LABEL, COLOUR_GREY, MTSW_TRACKLIST), SetDataTip(STR_PLAYLIST_TRACK_INDEX, STR_NULL),
				NWidget(WWT_PANEL, COLOUR_GREY, MTSW_LIST_LEFT), SetMinimalSize(180, 194), SetDataTip(0x0, STR_PLAYLIST_TOOLTIP_CLICK_TO_ADD_TRACK), EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 2),
			EndContainer(),
			/* Middle buttons. */
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetMinimalSize(60, 30), // Space above the first button from the title bar.
				NWidget(WWT_TEXTBTN, COLOUR_GREY, MTSW_ALL), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_ALL, STR_MUSIC_TOOLTIP_SELECT_ALL_TRACKS_PROGRAM),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, MTSW_OLD), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_OLD_STYLE, STR_MUSIC_TOOLTIP_SELECT_OLD_STYLE_MUSIC),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, MTSW_NEW), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_NEW_STYLE, STR_MUSIC_TOOLTIP_SELECT_NEW_STYLE_MUSIC),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, MTSW_EZY), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_EZY_STREET, STR_MUSIC_TOOLTIP_SELECT_EZY_STREET_STYLE),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, MTSW_CUSTOM1), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_CUSTOM_1, STR_MUSIC_TOOLTIP_SELECT_CUSTOM_1_USER_DEFINED),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, MTSW_CUSTOM2), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_CUSTOM_2, STR_MUSIC_TOOLTIP_SELECT_CUSTOM_2_USER_DEFINED),
				NWidget(NWID_SPACER), SetMinimalSize(0, 16), // Space above 'clear' button
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, MTSW_CLEAR), SetFill(1, 0), SetDataTip(STR_PLAYLIST_CLEAR, STR_PLAYLIST_TOOLTIP_CLEAR_CURRENT_PROGRAM_CUSTOM1),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
			/* Right panel. */
			NWidget(NWID_VERTICAL),
				NWidget(WWT_LABEL, COLOUR_GREY, MTSW_PLAYLIST), SetDataTip(STR_PLAYLIST_PROGRAM, STR_NULL),
				NWidget(WWT_PANEL, COLOUR_GREY, MTSW_LIST_RIGHT), SetMinimalSize(180, 194), SetDataTip(0x0, STR_PLAYLIST_TOOLTIP_CLICK_TO_REMOVE_TRACK), EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 2),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _music_track_selection_desc(
	104, 131, 432, 218,
	WC_MUSIC_TRACK_SELECTION, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_nested_music_track_selection_widgets, lengthof(_nested_music_track_selection_widgets)
);

static void ShowMusicTrackSelection()
{
	AllocateWindowDescFront<MusicTrackSelectionWindow>(&_music_track_selection_desc, 0);
}

enum MusicWidgets {
	MW_CLOSE,
	MW_CAPTION,
	MW_PREV,
	MW_NEXT,
	MW_STOP,
	MW_PLAY,
	MW_SLIDERS,
	MW_MUSIC_VOL,
	MW_GAUGE,
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

struct MusicWindow : public Window {
	static const int slider_bar_height = 4;
	static const int slider_height = 7;
	static const int slider_width = 3;

	MusicWindow(const WindowDesc *desc, WindowNumber number) : Window()
	{
		this->InitNested(desc, number);
		this->LowerWidget(msf.playlist + MW_ALL);
		this->SetWidgetLoweredState(MW_SHUFFLE, msf.shuffle);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		switch (widget) {
			/* Make sure that MW_SHUFFLE and MW_PROGRAMME have the same size.
			 * This can't be done by using NC_EQUALSIZE as the MW_INFO is
			 * between those widgets and of different size. */
			case MW_SHUFFLE: case MW_PROGRAMME: {
				Dimension d = maxdim(GetStringBoundingBox(STR_MUSIC_PROGRAM), GetStringBoundingBox(STR_MUSIC_SHUFFLE));
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
			} break;

			case MW_TRACK_NR: {
				Dimension d = GetStringBoundingBox(STR_MUSIC_TRACK_NONE);
				d.width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				d.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				*size = maxdim(*size, d);
			} break;

			case MW_TRACK_NAME: {
				Dimension d = GetStringBoundingBox(STR_MUSIC_TITLE_NONE);
				for (int i = 0; i < NUM_SONGS_AVAILABLE; i++) {
					SetDParam(0, SPECSTR_SONGNAME);
					SetDParam(1, i);
					d = maxdim(d, GetStringBoundingBox(STR_MUSIC_TITLE_NAME));
				}
				d.width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				d.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				*size = maxdim(*size, d);
			} break;

			/* Hack-ish: set the proper widget data; only needs to be done once
			 * per (Re)Init as that's the only time the language changes. */
			case MW_PREV: this->GetWidget<NWidgetCore>(MW_PREV)->widget_data = _dynlang.text_dir == TD_RTL ? SPR_IMG_SKIP_TO_NEXT : SPR_IMG_SKIP_TO_PREV; break;
			case MW_NEXT: this->GetWidget<NWidgetCore>(MW_NEXT)->widget_data = _dynlang.text_dir == TD_RTL ? SPR_IMG_SKIP_TO_PREV : SPR_IMG_SKIP_TO_NEXT; break;
			case MW_PLAY: this->GetWidget<NWidgetCore>(MW_PLAY)->widget_data = _dynlang.text_dir == TD_RTL ? SPR_IMG_PLAY_MUSIC_RTL : SPR_IMG_PLAY_MUSIC; break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case MW_GAUGE:
				GfxFillRect(r.left, r.top, r.right, r.bottom, 0);

				for (uint i = 0; i != 8; i++) {
					int colour = 0xD0;
					if (i > 4) {
						colour = 0xBF;
						if (i > 6) {
							colour = 0xB8;
						}
					}
					GfxFillRect(r.left, r.bottom - i * 2, r.right, r.bottom - i * 2, colour);
				}
				break;

			case MW_TRACK_NR: {
				GfxFillRect(r.left + 1, r.top + 1, r.right, r.bottom, 0);
				StringID str = STR_MUSIC_TRACK_NONE;
				if (_song_is_active != 0 && _music_wnd_cursong != 0) {
					SetDParam(0, _music_wnd_cursong);
					SetDParam(0, 2);
					str = STR_MUSIC_TRACK_DIGIT;
				}
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, str);
			} break;

			case MW_TRACK_NAME: {
				GfxFillRect(r.left, r.top + 1, r.right - 1, r.bottom, 0);
				StringID str = STR_MUSIC_TITLE_NONE;
				if (_song_is_active != 0 && _music_wnd_cursong != 0) {
					str = STR_MUSIC_TITLE_NAME;
					SetDParam(0, SPECSTR_SONGNAME);
					SetDParam(1, _music_wnd_cursong);
				}
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, str, TC_FROMSTRING, SA_CENTER);
			} break;

			case MW_MUSIC_VOL: case MW_EFFECT_VOL: {
				DrawString(r.left, r.right, r.top + 1, (widget == MW_MUSIC_VOL) ? STR_MUSIC_MUSIC_VOLUME : STR_MUSIC_EFFECTS_VOLUME, TC_FROMSTRING, SA_CENTER);
				int y = (r.top + r.bottom - slider_bar_height) / 2;
				DrawFrameRect(r.left, y, r.right, y + slider_bar_height, COLOUR_GREY, FR_LOWERED);
				DrawString(r.left, r.right, r.bottom - FONT_HEIGHT_SMALL, STR_MUSIC_MIN_MAX_RULER, TC_FROMSTRING, SA_CENTER);
				y = (r.top + r.bottom - slider_height) / 2;
				byte volume = (widget == MW_MUSIC_VOL) ? msf.music_vol : msf.effect_vol;
				int x = (volume * (r.right - r.left) / 127);
				if (_dynlang.text_dir == TD_RTL) {
					x = r.right - x;
				} else {
					x += r.left;
				}
				DrawFrameRect(x, y, x + slider_width, y + slider_height, COLOUR_GREY, FR_NONE);
			} break;
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void OnInvalidateData(int data = 0)
	{
		for (int i = 0; i < 6; i++) {
			this->SetWidgetLoweredState(MW_ALL + i, i == msf.playlist);
		}
		this->SetDirty();
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case MW_PREV: // skip to prev
				if (!_song_is_active) return;
				SkipToPrevSong();
				this->SetDirty();
				break;

			case MW_NEXT: // skip to next
				if (!_song_is_active) return;
				SkipToNextSong();
				this->SetDirty();
				break;

			case MW_STOP: // stop playing
				msf.playing = false;
				break;

			case MW_PLAY: // start playing
				msf.playing = true;
				break;

			case MW_MUSIC_VOL: case MW_EFFECT_VOL: { // volume sliders
				int x = pt.x - this->GetWidget<NWidgetBase>(widget)->pos_x;

				byte *vol = (widget == MW_MUSIC_VOL) ? &msf.music_vol : &msf.effect_vol;

				byte new_vol = x * 127 / this->GetWidget<NWidgetBase>(widget)->current_x;
				if (_dynlang.text_dir == TD_RTL) new_vol = 127 - new_vol;
				if (new_vol != *vol) {
					*vol = new_vol;
					if (widget == MW_MUSIC_VOL) MusicVolumeChanged(new_vol);
					this->SetDirty();
				}

				_left_button_clicked = false;
			} break;

			case MW_SHUFFLE: // toggle shuffle
				msf.shuffle ^= 1;
				this->SetWidgetLoweredState(MW_SHUFFLE, msf.shuffle);
				this->SetWidgetDirty(MW_SHUFFLE);
				StopMusic();
				SelectSongToPlay();
				this->SetDirty();
				break;

			case MW_PROGRAMME: // show track selection
				ShowMusicTrackSelection();
				break;

			case MW_ALL: case MW_OLD: case MW_NEW:
			case MW_EZY: case MW_CUSTOM1: case MW_CUSTOM2: // playlist
				SelectPlaylist(widget - MW_ALL);
				StopMusic();
				SelectSongToPlay();
				this->SetDirty();
				break;
		}
	}

#if 0
	virtual void OnTick()
	{
		this->SetWidgetDirty(MW_GAUGE);
	}
#endif
};

static const NWidgetPart _nested_music_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, MW_CLOSE),
		NWidget(WWT_CAPTION, COLOUR_GREY, MW_CAPTION), SetDataTip(STR_MUSIC_JAZZ_JUKEBOX_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),

	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, MW_PREV), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_SKIP_TO_PREV, STR_MUSIC_TOOLTIP_SKIP_TO_PREVIOUS_TRACK),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, MW_NEXT), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_SKIP_TO_NEXT, STR_MUSIC_TOOLTIP_SKIP_TO_NEXT_TRACK_IN_SELECTION),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, MW_STOP), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_STOP_MUSIC, STR_MUSIC_TOOLTIP_STOP_PLAYING_MUSIC),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, MW_PLAY), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_PLAY_MUSIC, STR_MUSIC_TOOLTIP_START_PLAYING_MUSIC),
		NWidget(WWT_PANEL, COLOUR_GREY, MW_SLIDERS),
			NWidget(NWID_HORIZONTAL), SetPIP(20, 0, 20),
				NWidget(WWT_EMPTY, COLOUR_GREY, MW_MUSIC_VOL), SetMinimalSize(67, 22), SetFill(0, 0), SetDataTip(0x0, STR_MUSIC_TOOLTIP_DRAG_SLIDERS_TO_SET_MUSIC),
				NWidget(WWT_PANEL, COLOUR_GREY, MW_GAUGE), SetMinimalSize(16, 20), SetPadding(1, 11, 1, 11), SetFill(0, 0), EndContainer(),
				NWidget(WWT_EMPTY, COLOUR_GREY, MW_EFFECT_VOL), SetMinimalSize(67, 22), SetFill(0, 0), SetDataTip(0x0, STR_MUSIC_TOOLTIP_DRAG_SLIDERS_TO_SET_MUSIC),
				NWidget(NWID_SPACER), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, MW_BACKGROUND),
		NWidget(NWID_HORIZONTAL), SetPIP(6, 0, 6),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, MW_SHUFFLE), SetMinimalSize(50, 8), SetDataTip(STR_MUSIC_SHUFFLE, STR_MUSIC_TOOLTIP_TOGGLE_PROGRAM_SHUFFLE),
			NWidget(NWID_VERTICAL), SetPadding(0, 0, 3, 3),
				NWidget(WWT_LABEL, COLOUR_GREY, MW_TRACK), SetDataTip(STR_MUSIC_TRACK, STR_NULL),
				NWidget(WWT_PANEL, COLOUR_GREY, MW_TRACK_NR), EndContainer(),
			EndContainer(),
			NWidget(NWID_VERTICAL), SetPadding(0, 3, 3, 0),
				NWidget(WWT_LABEL, COLOUR_GREY, MW_TRACK_TITLE), SetFill(1, 0), SetDataTip(STR_MUSIC_XTITLE, STR_NULL),
				NWidget(WWT_PANEL, COLOUR_GREY, MW_TRACK_NAME), EndContainer(),
			EndContainer(),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, MW_PROGRAMME), SetMinimalSize(50, 8), SetDataTip(STR_MUSIC_PROGRAM, STR_MUSIC_TOOLTIP_SHOW_MUSIC_TRACK_SELECTION),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, MW_ALL), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_ALL, STR_MUSIC_TOOLTIP_SELECT_ALL_TRACKS_PROGRAM),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, MW_OLD), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_OLD_STYLE, STR_MUSIC_TOOLTIP_SELECT_OLD_STYLE_MUSIC),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, MW_NEW), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_NEW_STYLE, STR_MUSIC_TOOLTIP_SELECT_NEW_STYLE_MUSIC),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, MW_EZY), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_EZY_STREET, STR_MUSIC_TOOLTIP_SELECT_EZY_STREET_STYLE),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, MW_CUSTOM1), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_CUSTOM_1, STR_MUSIC_TOOLTIP_SELECT_CUSTOM_1_USER_DEFINED),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, MW_CUSTOM2), SetFill(1, 0), SetDataTip(STR_MUSIC_PLAYLIST_CUSTOM_2, STR_MUSIC_TOOLTIP_SELECT_CUSTOM_2_USER_DEFINED),
	EndContainer(),
};

static const WindowDesc _music_window_desc(
	0, 22, 300, 66,
	WC_MUSIC_WINDOW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_nested_music_window_widgets, lengthof(_nested_music_window_widgets)
);

void ShowMusicWindow()
{
	AllocateWindowDescFront<MusicWindow>(&_music_window_desc, 0);
}
