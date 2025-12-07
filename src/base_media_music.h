/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file base_media_music.h Generic functions for replacing base music data. */

#ifndef BASE_MEDIA_MUSIC_H
#define BASE_MEDIA_MUSIC_H

#include "base_media_base.h"

/** Maximum number of songs in the 'class' playlists. */
static const uint NUM_SONGS_CLASS     = 10;
/** Number of classes for songs */
static const uint NUM_SONG_CLASSES    = 3;
/** Maximum number of songs in the full playlist; theme song + the classes */
static const uint NUM_SONGS_AVAILABLE = 1 + NUM_SONG_CLASSES * NUM_SONGS_CLASS;

/** Maximum number of songs in the (custom) playlist */
static const uint NUM_SONGS_PLAYLIST  = 32;

/* Functions to read DOS music CAT files, similar to but not quite the same as sound effect CAT files */
std::optional<std::string> GetMusicCatEntryName(const std::string &filename, size_t entrynum);
std::optional<std::vector<uint8_t>> GetMusicCatEntryData(const std::string &filename, size_t entrynum);

enum MusicTrackType : uint8_t {
	MTT_STANDARDMIDI, ///< Standard MIDI file
	MTT_MPSMIDI,      ///< MPS GM driver MIDI format (contained in a CAT file)
};

/** Metadata about a music track. */
struct MusicSongInfo {
	std::string songname; ///< name of song displayed in UI
	std::string filename; ///< file on disk containing song (when used in MusicSet class)
	int cat_index; ///< entry index in CAT file, for filetype==MTT_MPSMIDI
	int override_start; ///< MIDI ticks to skip over in beginning
	int override_end; ///< MIDI tick to end the song at (0 if no override)
	uint8_t tracknr; ///< track number of song displayed in UI
	MusicTrackType filetype; ///< decoder required for song file
	bool loop; ///< song should play in a tight loop if possible, never ending
};

template <> struct BaseSetTraits<struct MusicSet> {
	static constexpr size_t num_files = NUM_SONGS_AVAILABLE;
	static constexpr bool search_in_tars = false;
	static constexpr std::string_view set_type = "music";
};

/** All data of a music set. */
struct MusicSet : BaseSet<MusicSet> {
	/** Data about individual songs in set. */
	MusicSongInfo songinfo[NUM_SONGS_AVAILABLE];
	/** Number of valid songs in set. */
	uint8_t num_available = 0;

	bool FillSetDetails(const IniFile &ini, const std::string &path, const std::string &full_filename);
};

/** All data/functions related with replacing the base music */
class BaseMusic : public BaseMedia<MusicSet> {
public:
	/** The set as saved in the config file. */
	static inline std::string ini_set;
};

#endif /* BASE_MEDIA_MUSIC_H */
