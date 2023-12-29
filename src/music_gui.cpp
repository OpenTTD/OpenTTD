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
#include "core/mem_func.hpp"
#include "error.h"
#include "core/geometry_func.hpp"
#include "string_func.h"
#include "settings_type.h"
#include "settings_gui.h"
#include "widgets/dropdown_func.h"
#include "widgets/dropdown_type.h"
#include "widgets/slider_func.h"
#include "mixer.h"

#include "widgets/music_widget.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "safeguards.h"


struct MusicSystem {
	struct PlaylistEntry : MusicSongInfo {
		const MusicSet *set;  ///< music set the song comes from
		uint set_index;        ///< index of song in set

		PlaylistEntry(const MusicSet *set, uint set_index) : MusicSongInfo(set->songinfo[set_index]), set(set), set_index(set_index) { }
		bool IsValid() const { return !this->songname.empty(); }
	};
	typedef std::vector<PlaylistEntry> Playlist;

	enum PlaylistChoices {
		PLCH_ALLMUSIC,
		PLCH_OLDSTYLE,
		PLCH_NEWSTYLE,
		PLCH_EZYSTREET,
		PLCH_CUSTOM1,
		PLCH_CUSTOM2,
		PLCH_THEMEONLY,
		PLCH_MAX,
	};

	Playlist active_playlist;    ///< current play order of songs, including any shuffle
	Playlist displayed_playlist; ///< current playlist as displayed in GUI, never in shuffled order
	Playlist music_set;          ///< all songs in current music set, in set order

	PlaylistChoices selected_playlist;

	void BuildPlaylists();

	void ChangePlaylist(PlaylistChoices pl);
	void ChangeMusicSet(const std::string &set_name);
	void Shuffle();
	void Unshuffle();

	void Play();
	void Stop();
	void Next();
	void Prev();
	void CheckStatus();

	bool IsPlaying() const;
	bool IsShuffle() const;
	PlaylistEntry GetCurrentSong() const;

	bool IsCustomPlaylist() const;
	void PlaylistAdd(size_t song_index);
	void PlaylistRemove(size_t song_index);
	void PlaylistClear();

private:
	uint GetSetIndex();
	void SetPositionBySetIndex(uint set_index);
	void ChangePlaylistPosition(int ofs);
	int playlist_position;

	void SaveCustomPlaylist(PlaylistChoices pl);

	Playlist standard_playlists[PLCH_MAX];
};

MusicSystem _music;


/** Rebuild all playlists for the current music set */
void MusicSystem::BuildPlaylists()
{
	const MusicSet *set = BaseMusic::GetUsedSet();

	/* Clear current playlists */
	for (size_t i = 0; i < lengthof(this->standard_playlists); ++i) this->standard_playlists[i].clear();
	this->music_set.clear();

	/* Build standard playlists, and a list of available music */
	for (uint i = 0; i < NUM_SONGS_AVAILABLE; i++) {
		PlaylistEntry entry(set, i);
		if (!entry.IsValid()) continue;

		this->music_set.push_back(entry);

		/* Add theme song to theme-only playlist */
		if (i == 0) this->standard_playlists[PLCH_THEMEONLY].push_back(entry);

		/* Don't add the theme song to standard playlists */
		if (i > 0) {
			this->standard_playlists[PLCH_ALLMUSIC].push_back(entry);
			uint theme = (i - 1) / NUM_SONGS_CLASS;
			this->standard_playlists[PLCH_OLDSTYLE + theme].push_back(entry);
		}
	}

	/* Load custom playlists
	 * Song index offsets are 1-based, zero indicates invalid/end-of-list value */
	for (uint i = 0; i < NUM_SONGS_PLAYLIST; i++) {
		if (_settings_client.music.custom_1[i] > 0 && _settings_client.music.custom_1[i] <= NUM_SONGS_AVAILABLE) {
			PlaylistEntry entry(set, _settings_client.music.custom_1[i] - 1);
			if (entry.IsValid()) this->standard_playlists[PLCH_CUSTOM1].push_back(entry);
		}
		if (_settings_client.music.custom_2[i] > 0 && _settings_client.music.custom_2[i] <= NUM_SONGS_AVAILABLE) {
			PlaylistEntry entry(set, _settings_client.music.custom_2[i] - 1);
			if (entry.IsValid()) this->standard_playlists[PLCH_CUSTOM2].push_back(entry);
		}
	}
}

/**
 * Switch to another playlist, or reload the current one.
 * @param pl Playlist to select
 */
void MusicSystem::ChangePlaylist(PlaylistChoices pl)
{
	assert(pl < PLCH_MAX && pl >= PLCH_ALLMUSIC);

	if (pl != PLCH_THEMEONLY) _settings_client.music.playlist = pl;

	if (_game_mode != GM_MENU || pl == PLCH_THEMEONLY) {
		this->displayed_playlist = this->standard_playlists[pl];
		this->active_playlist = this->displayed_playlist;
		this->selected_playlist = pl;
		this->playlist_position = 0;

		if (_settings_client.music.shuffle) this->Shuffle();
		if (_settings_client.music.playing) this->Play();
	}

	InvalidateWindowData(WC_MUSIC_TRACK_SELECTION, 0);
	InvalidateWindowData(WC_MUSIC_WINDOW, 0);
}

/**
 * Change to named music set, and reset playback.
 * @param set_name Name of music set to select
 */
void MusicSystem::ChangeMusicSet(const std::string &set_name)
{
	BaseMusic::SetSetByName(set_name);
	BaseMusic::ini_set = set_name;

	this->BuildPlaylists();
	this->ChangePlaylist(this->selected_playlist);

	InvalidateWindowData(WC_GAME_OPTIONS, WN_GAME_OPTIONS_GAME_OPTIONS, 0, true);
	InvalidateWindowData(WC_MUSIC_TRACK_SELECTION, 0, 1, true);
	InvalidateWindowData(WC_MUSIC_WINDOW, 0, 1, true);
}

/**
 * Set playlist position by set index.
 * @param set_index Set index to select.
 */
void MusicSystem::SetPositionBySetIndex(uint set_index)
{
	auto it = std::find_if(std::begin(this->active_playlist), std::end(this->active_playlist), [&set_index](const PlaylistEntry &ple) { return ple.set_index == set_index; });
	if (it != std::end(this->active_playlist)) this->playlist_position = std::distance(std::begin(this->active_playlist), it);
}

/**
 * Get set index from current playlist position.
 * @return current set index, or UINT_MAX if nothing is selected.
 */
uint MusicSystem::GetSetIndex()
{
	return static_cast<size_t>(this->playlist_position) < this->active_playlist.size()
		? this->active_playlist[this->playlist_position].set_index
		: UINT_MAX;
}

/**
 * Enable shuffle mode.
 */
void MusicSystem::Shuffle()
{
	_settings_client.music.shuffle = true;

	uint set_index = this->GetSetIndex();
	this->active_playlist = this->displayed_playlist;
	for (size_t i = 0; i < this->active_playlist.size(); i++) {
		size_t shuffle_index = InteractiveRandom() % (this->active_playlist.size() - i);
		std::swap(this->active_playlist[i], this->active_playlist[i + shuffle_index]);
	}
	this->SetPositionBySetIndex(set_index);

	InvalidateWindowData(WC_MUSIC_TRACK_SELECTION, 0);
	InvalidateWindowData(WC_MUSIC_WINDOW, 0);
}

/**
 * Disable shuffle mode.
 */
void MusicSystem::Unshuffle()
{
	_settings_client.music.shuffle = false;

	uint set_index = this->GetSetIndex();
	this->active_playlist = this->displayed_playlist;
	this->SetPositionBySetIndex(set_index);

	InvalidateWindowData(WC_MUSIC_TRACK_SELECTION, 0);
	InvalidateWindowData(WC_MUSIC_WINDOW, 0);
}

/** Start/restart playback at current song */
void MusicSystem::Play()
{
	/* Always set the playing flag, even if there is no music */
	_settings_client.music.playing = true;
	MusicDriver::GetInstance()->StopSong();
	/* Make sure playlist_position is a valid index, if playlist has changed etc. */
	this->ChangePlaylistPosition(0);

	/* If there is no music, don't try to play it */
	if (this->active_playlist.empty()) return;

	MusicSongInfo song = this->active_playlist[this->playlist_position];
	if (_game_mode == GM_MENU && this->selected_playlist == PLCH_THEMEONLY) song.loop = true;
	MusicDriver::GetInstance()->PlaySong(song);

	InvalidateWindowData(WC_MUSIC_WINDOW, 0);
}

/** Stop playback and set flag that we don't intend to play music */
void MusicSystem::Stop()
{
	MusicDriver::GetInstance()->StopSong();
	_settings_client.music.playing = false;

	InvalidateWindowData(WC_MUSIC_WINDOW, 0);
}

/** Skip to next track */
void MusicSystem::Next()
{
	this->ChangePlaylistPosition(+1);
	if (_settings_client.music.playing) this->Play();

	InvalidateWindowData(WC_MUSIC_WINDOW, 0);
}

/** Skip to previous track */
void MusicSystem::Prev()
{
	this->ChangePlaylistPosition(-1);
	if (_settings_client.music.playing) this->Play();

	InvalidateWindowData(WC_MUSIC_WINDOW, 0);
}

/** Check that music is playing if it should, and that appropriate playlist is active for game/main menu */
void MusicSystem::CheckStatus()
{
	if ((_game_mode == GM_MENU) != (this->selected_playlist == PLCH_THEMEONLY)) {
		/* Make sure the theme-only playlist is active when on the title screen, and not during gameplay */
		this->ChangePlaylist((_game_mode == GM_MENU) ? PLCH_THEMEONLY : (PlaylistChoices)_settings_client.music.playlist);
	}
	if (this->active_playlist.empty()) return;
	/* If we were supposed to be playing, but music has stopped, move to next song */
	if (this->IsPlaying() && !MusicDriver::GetInstance()->IsSongPlaying()) this->Next();
}

/** Is the player getting music right now? */
bool MusicSystem::IsPlaying() const
{
	return _settings_client.music.playing && !this->active_playlist.empty();
}

/** Is shuffle mode enabled? */
bool MusicSystem::IsShuffle() const
{
	return _settings_client.music.shuffle;
}

/** Return the current song, or a dummy if none */
MusicSystem::PlaylistEntry MusicSystem::GetCurrentSong() const
{
	if (!this->IsPlaying()) return PlaylistEntry(BaseMusic::GetUsedSet(), 0);
	return this->active_playlist[this->playlist_position];
}

/** Is one of the custom playlists selected? */
bool MusicSystem::IsCustomPlaylist() const
{
	return (this->selected_playlist == PLCH_CUSTOM1) || (this->selected_playlist == PLCH_CUSTOM2);
}

/**
 * Append a song to a custom playlist.
 * Always adds to the currently active playlist.
 * @param song_index Index of song in the current music set to add
 */
void MusicSystem::PlaylistAdd(size_t song_index)
{
	if (!this->IsCustomPlaylist()) return;

	/* Pick out song from the music set */
	if (song_index >= this->music_set.size()) return;
	PlaylistEntry entry = this->music_set[song_index];

	/* Check for maximum length */
	if (this->standard_playlists[this->selected_playlist].size() >= NUM_SONGS_PLAYLIST) return;

	/* Add it to the appropriate playlist, and the display */
	this->standard_playlists[this->selected_playlist].push_back(entry);
	this->displayed_playlist.push_back(entry);

	/* Add it to the active playlist, if playback is shuffled select a random position to add at */
	if (this->active_playlist.empty()) {
		this->active_playlist.push_back(entry);
		if (this->IsPlaying()) this->Play();
	} else if (this->IsShuffle()) {
		/* Generate a random position between 0 and n (inclusive, new length) to insert at */
		size_t maxpos = this->displayed_playlist.size();
		size_t newpos = InteractiveRandom() % maxpos;
		this->active_playlist.insert(this->active_playlist.begin() + newpos, entry);
		/* Make sure to shift up the current playback position if the song was inserted before it */
		if ((int)newpos <= this->playlist_position) this->playlist_position++;
	} else {
		this->active_playlist.push_back(entry);
	}

	this->SaveCustomPlaylist(this->selected_playlist);

	InvalidateWindowData(WC_MUSIC_TRACK_SELECTION, 0);
}

/**
 * Remove a song from a custom playlist.
 * @param song_index Index in the custom playlist to remove.
 */
void MusicSystem::PlaylistRemove(size_t song_index)
{
	if (!this->IsCustomPlaylist()) return;

	Playlist &pl = this->standard_playlists[this->selected_playlist];
	if (song_index >= pl.size()) return;

	/* Remove from "simple" playlists */
	PlaylistEntry song = pl[song_index];
	pl.erase(pl.begin() + song_index);
	this->displayed_playlist.erase(this->displayed_playlist.begin() + song_index);

	/* Find in actual active playlist (may be shuffled) and remove,
	 * if it's the current song restart playback */
	for (size_t i = 0; i < this->active_playlist.size(); i++) {
		Playlist::iterator s2 = this->active_playlist.begin() + i;
		if (s2->filename == song.filename && s2->cat_index == song.cat_index) {
			this->active_playlist.erase(s2);
			if ((int)i == this->playlist_position && this->IsPlaying()) this->Play();
			break;
		}
	}

	this->SaveCustomPlaylist(this->selected_playlist);

	InvalidateWindowData(WC_MUSIC_TRACK_SELECTION, 0);
}

/**
 * Remove all songs from the current custom playlist.
 * Effectively stops playback too.
 */
void MusicSystem::PlaylistClear()
{
	if (!this->IsCustomPlaylist()) return;

	this->standard_playlists[this->selected_playlist].clear();
	this->ChangePlaylist(this->selected_playlist);

	this->SaveCustomPlaylist(this->selected_playlist);
}

/**
 * Change playlist position pointer by the given offset, making sure to keep it within valid range.
 * If the playlist is empty, position is always set to 0.
 * @param ofs Amount to move playlist position by.
 */
void MusicSystem::ChangePlaylistPosition(int ofs)
{
	if (this->active_playlist.empty()) {
		this->playlist_position = 0;
	} else {
		this->playlist_position += ofs;
		while (this->playlist_position >= (int)this->active_playlist.size()) this->playlist_position -= (int)this->active_playlist.size();
		while (this->playlist_position < 0) this->playlist_position += (int)this->active_playlist.size();
	}
}

/**
 * Save a custom playlist to settings after modification.
 * @param pl Playlist to store back
 */
void MusicSystem::SaveCustomPlaylist(PlaylistChoices pl)
{
	byte *settings_pl;
	if (pl == PLCH_CUSTOM1) {
		settings_pl = _settings_client.music.custom_1;
	} else if (pl == PLCH_CUSTOM2) {
		settings_pl = _settings_client.music.custom_2;
	} else {
		return;
	}

	size_t num = 0;
	MemSetT(settings_pl, 0, NUM_SONGS_PLAYLIST);

	for (const auto &song : this->standard_playlists[pl]) {
		/* Music set indices in the settings playlist are 1-based, 0 means unused slot */
		settings_pl[num++] = (byte)song.set_index + 1;
	}
}


/**
 * Check music playback status and start/stop/song-finished.
 * Called from main loop.
 */
void MusicLoop()
{
	_music.CheckStatus();
}

/**
 * Change the configured music set and reset playback
 * @param index Index of music set to switch to
 */
void ChangeMusicSet(int index)
{
	if (BaseMusic::GetIndexOfUsedSet() == index) return;
	_music.ChangeMusicSet(BaseMusic::GetSet(index)->name);
}

/**
 * Prepare the music system for use.
 * Called from \c InitializeGame
 */
void InitializeMusic()
{
	_music.BuildPlaylists();
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

	void SetStringParameters(WidgetID widget) const override
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
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		for (int i = 0; i < 6; i++) {
			this->SetWidgetLoweredState(WID_MTS_ALL + i, i == _settings_client.music.playlist);
		}
		this->SetWidgetDisabledState(WID_MTS_CLEAR, _settings_client.music.playlist <= 3);

		if (data == 1) {
			this->ReInit();
		} else {
			this->SetDirty();
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
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

				for (const auto &song : _music.music_set) {
					SetDParam(0, song.tracknr);
					SetDParam(1, 2);
					SetDParamStr(2, song.songname);
					Dimension d2 = GetStringBoundingBox(STR_PLAYLIST_TRACK_NAME);
					d.width = std::max(d.width, d2.width);
					d.height += d2.height;
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_MTS_LIST_LEFT: {
				GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), PC_BLACK);

				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				for (const auto &song : _music.music_set) {
					SetDParam(0, song.tracknr);
					SetDParam(1, 2);
					SetDParamStr(2, song.songname);
					DrawString(tr, STR_PLAYLIST_TRACK_NAME);
					tr.top += GetCharacterHeight(FS_SMALL);
				}
				break;
			}

			case WID_MTS_LIST_RIGHT: {
				GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), PC_BLACK);

				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				for (const auto &song : _music.active_playlist) {
					SetDParam(0, song.tracknr);
					SetDParam(1, 2);
					SetDParamStr(2, song.songname);
					DrawString(tr, STR_PLAYLIST_TRACK_NAME);
					tr.top += GetCharacterHeight(FS_SMALL);
				}
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_MTS_LIST_LEFT: { // add to playlist
				int y = this->GetRowFromWidget(pt.y, widget, 0, GetCharacterHeight(FS_SMALL));
				_music.PlaylistAdd(y);
				break;
			}

			case WID_MTS_LIST_RIGHT: { // remove from playlist
				int y = this->GetRowFromWidget(pt.y, widget, 0, GetCharacterHeight(FS_SMALL));
				_music.PlaylistRemove(y);
				break;
			}

			case WID_MTS_MUSICSET: {
				int selected = 0;
				ShowDropDownList(this, BuildSetDropDownList<BaseMusic>(&selected), selected, widget);
				break;
			}

			case WID_MTS_CLEAR: // clear
				_music.PlaylistClear();
				break;

			case WID_MTS_ALL: case WID_MTS_OLD: case WID_MTS_NEW:
			case WID_MTS_EZY: case WID_MTS_CUSTOM1: case WID_MTS_CUSTOM2: // set playlist
				_music.ChangePlaylist((MusicSystem::PlaylistChoices)(widget - WID_MTS_ALL));
				break;
		}
	}

	void OnDropdownSelect(WidgetID widget, int index) override
	{
		switch (widget) {
			case WID_MTS_MUSICSET:
				ChangeMusicSet(index);
				break;
			default:
				NOT_REACHED();
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
				NWidget(WWT_LABEL, COLOUR_GREY), SetFill(1, 0), SetDataTip(STR_PLAYLIST_TRACK_INDEX, STR_NULL),
				NWidget(WWT_PANEL, COLOUR_GREY, WID_MTS_LIST_LEFT), SetFill(1, 1), SetMinimalSize(180, 194), SetDataTip(0x0, STR_PLAYLIST_TOOLTIP_CLICK_TO_ADD_TRACK), EndContainer(),
				NWidget(NWID_SPACER), SetFill(1, 0), SetMinimalSize(0, 2),
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
				NWidget(WWT_LABEL, COLOUR_GREY, WID_MTS_PLAYLIST), SetFill(1, 0), SetDataTip(STR_PLAYLIST_PROGRAM, STR_NULL),
				NWidget(WWT_PANEL, COLOUR_GREY, WID_MTS_LIST_RIGHT), SetFill(1, 1), SetMinimalSize(180, 194), SetDataTip(0x0, STR_PLAYLIST_TOOLTIP_CLICK_TO_REMOVE_TRACK), EndContainer(),
				NWidget(NWID_SPACER), SetFill(1, 0), SetMinimalSize(0, 2),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _music_track_selection_desc(__FILE__, __LINE__,
	WDP_AUTO, nullptr, 0, 0,
	WC_MUSIC_TRACK_SELECTION, WC_NONE,
	0,
	std::begin(_nested_music_track_selection_widgets), std::end(_nested_music_track_selection_widgets)
);

static void ShowMusicTrackSelection()
{
	AllocateWindowDescFront<MusicTrackSelectionWindow>(&_music_track_selection_desc, 0);
}

struct MusicWindow : public Window {
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
			WID_M_ALL, WID_M_OLD, WID_M_NEW, WID_M_EZY, WID_M_CUSTOM1, WID_M_CUSTOM2
			);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
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
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case WID_M_TRACK_NAME: {
				Dimension d = GetStringBoundingBox(STR_MUSIC_TITLE_NONE);
				for (const auto &song : _music.music_set) {
					SetDParamStr(0, song.songname);
					d = maxdim(d, GetStringBoundingBox(STR_MUSIC_TITLE_NAME));
				}
				d.width += padding.width;
				d.height += padding.height;
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

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_M_TRACK_NR: {
				GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel.left, WidgetDimensions::scaled.bevel.top, 0, WidgetDimensions::scaled.bevel.bottom), PC_BLACK);
				if (BaseMusic::GetUsedSet()->num_available == 0) {
					break;
				}
				StringID str = STR_MUSIC_TRACK_NONE;
				if (_music.IsPlaying()) {
					SetDParam(0, _music.GetCurrentSong().tracknr);
					SetDParam(1, 2);
					str = STR_MUSIC_TRACK_DIGIT;
				}
				DrawString(r.Shrink(WidgetDimensions::scaled.framerect), str);
				break;
			}

			case WID_M_TRACK_NAME: {
				GfxFillRect(r.Shrink(0, WidgetDimensions::scaled.bevel.top, WidgetDimensions::scaled.bevel.right, WidgetDimensions::scaled.bevel.bottom), PC_BLACK);
				StringID str = STR_MUSIC_TITLE_NONE;
				MusicSystem::PlaylistEntry entry(_music.GetCurrentSong());
				if (BaseMusic::GetUsedSet()->num_available == 0) {
					str = STR_MUSIC_TITLE_NOMUSIC;
				} else if (_music.IsPlaying()) {
					str = STR_MUSIC_TITLE_NAME;
					SetDParamStr(0, entry.songname);
				}
				DrawString(r.Shrink(WidgetDimensions::scaled.framerect), str, TC_FROMSTRING, SA_HOR_CENTER);
				break;
			}

			case WID_M_MUSIC_VOL:
				DrawSliderWidget(r, 0, INT8_MAX, _settings_client.music.music_vol, {});
				break;

			case WID_M_EFFECT_VOL:
				DrawSliderWidget(r, 0, INT8_MAX, _settings_client.music.effect_vol, {});
				break;
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		for (int i = 0; i < 6; i++) {
			this->SetWidgetLoweredState(WID_M_ALL + i, i == _settings_client.music.playlist);
		}

		UpdateDisabledButtons();

		if (data == 1) {
			this->ReInit();
		} else {
			this->SetDirty();
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_M_PREV: // skip to prev
				_music.Prev();
				break;

			case WID_M_NEXT: // skip to next
				_music.Next();
				break;

			case WID_M_STOP: // stop playing
				_music.Stop();
				break;

			case WID_M_PLAY: // start playing
				_music.Play();
				break;

			case WID_M_MUSIC_VOL: case WID_M_EFFECT_VOL: { // volume sliders
				byte &vol = (widget == WID_M_MUSIC_VOL) ? _settings_client.music.music_vol : _settings_client.music.effect_vol;
				if (ClickSliderWidget(this->GetWidget<NWidgetBase>(widget)->GetCurrentRect(), pt, 0, INT8_MAX, vol)) {
					if (widget == WID_M_MUSIC_VOL) {
						MusicDriver::GetInstance()->SetVolume(vol);
					} else {
						SetEffectVolume(vol);
					}
					this->SetWidgetDirty(widget);
					SetWindowClassesDirty(WC_GAME_OPTIONS);
				}

				if (click_count > 0) this->mouse_capture_widget = widget;
				break;
			}

			case WID_M_SHUFFLE: // toggle shuffle
				if (_music.IsShuffle()) {
					_music.Unshuffle();
				} else {
					_music.Shuffle();
				}
				this->SetWidgetLoweredState(WID_M_SHUFFLE, _music.IsShuffle());
				this->SetWidgetDirty(WID_M_SHUFFLE);
				break;

			case WID_M_PROGRAMME: // show track selection
				ShowMusicTrackSelection();
				break;

			case WID_M_ALL: case WID_M_OLD: case WID_M_NEW:
			case WID_M_EZY: case WID_M_CUSTOM1: case WID_M_CUSTOM2: // playlist
				_music.ChangePlaylist((MusicSystem::PlaylistChoices)(widget - WID_M_ALL));
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
			NWidget(NWID_HORIZONTAL), SetPIP(4, 0, 4),
				NWidget(NWID_VERTICAL),
					NWidget(WWT_LABEL, COLOUR_GREY, -1), SetFill(1, 0), SetDataTip(STR_MUSIC_MUSIC_VOLUME, STR_NULL),
					NWidget(WWT_EMPTY, COLOUR_GREY, WID_M_MUSIC_VOL), SetMinimalSize(67, 0), SetPadding(2), SetMinimalTextLines(1, 0), SetFill(1, 0), SetDataTip(0x0, STR_MUSIC_TOOLTIP_DRAG_SLIDERS_TO_SET_MUSIC),
				EndContainer(),
				NWidget(NWID_VERTICAL),
					NWidget(WWT_LABEL, COLOUR_GREY, -1), SetFill(1, 0), SetDataTip(STR_MUSIC_EFFECTS_VOLUME, STR_NULL),
					NWidget(WWT_EMPTY, COLOUR_GREY, WID_M_EFFECT_VOL), SetMinimalSize(67, 0), SetPadding(2), SetMinimalTextLines(1, 0), SetFill(1, 0), SetDataTip(0x0, STR_MUSIC_TOOLTIP_DRAG_SLIDERS_TO_SET_MUSIC),
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

static WindowDesc _music_window_desc(__FILE__, __LINE__,
	WDP_AUTO, "music", 0, 0,
	WC_MUSIC_WINDOW, WC_NONE,
	0,
	std::begin(_nested_music_window_widgets), std::end(_nested_music_window_widgets)
);

void ShowMusicWindow()
{
	AllocateWindowDescFront<MusicWindow>(&_music_window_desc, 0);
}
