/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file base_media_base.h Generic functions for replacing base data (graphics, sounds). */

#ifndef BASE_MEDIA_BASE_H
#define BASE_MEDIA_BASE_H

#include "fileio_func.h"
#include "gfx_type.h"
#include "textfile_type.h"
#include "textfile_gui.h"
#include "3rdparty/md5/md5.h"
#include <unordered_map>

/* Forward declare these; can't do 'struct X' in functions as older GCCs barf on that */
struct IniFile;
struct ContentInfo;

/** Structure holding filename and MD5 information about a single file */
struct MD5File {
	/** The result of a checksum check */
	enum ChecksumResult {
		CR_UNKNOWN,  ///< The file has not been checked yet
		CR_MATCH,    ///< The file did exist and the md5 checksum did match
		CR_MISMATCH, ///< The file did exist, just the md5 checksum did not match
		CR_NO_FILE,  ///< The file did not exist
	};

	std::string filename;        ///< filename
	MD5Hash hash;                ///< md5 sum of the file
	std::string missing_warning; ///< warning when this file is missing
	ChecksumResult check_result; ///< cached result of md5 check

	ChecksumResult CheckMD5(Subdirectory subdir, size_t max_size) const;
};

/**
 * Information about a single base set.
 * @tparam T the real class we're going to be
 * @tparam Tnum_files the number of files in the set
 * @tparam Tsearch_in_tars whether to search in the tars or not
 */
template <class T, size_t Tnum_files, bool Tsearch_in_tars>
struct BaseSet {
	typedef std::unordered_map<std::string, std::string> TranslatedStrings;

	/** Number of files in this set */
	static const size_t NUM_FILES = Tnum_files;

	/** Whether to search in the tars or not. */
	static const bool SEARCH_IN_TARS = Tsearch_in_tars;

	/** Internal names of the files in this set. */
	static const char * const *file_names;

	std::string name;              ///< The name of the base set
	std::string url;               ///< URL for information about the base set
	TranslatedStrings description; ///< Description of the base set
	uint32_t shortname;              ///< Four letter short variant of the name
	uint32_t version;                ///< The version of this base set
	bool fallback;                 ///< This set is a fallback set, i.e. it should be used only as last resort

	MD5File files[NUM_FILES];      ///< All files part of this set
	uint found_files;              ///< Number of the files that could be found
	uint valid_files;              ///< Number of the files that could be found and are valid

	T *next;                       ///< The next base set in this list

	/** Free everything we allocated */
	~BaseSet()
	{
		delete this->next;
	}

	/**
	 * Get the number of missing files.
	 * @return the number
	 */
	int GetNumMissing() const
	{
		return Tnum_files - this->found_files;
	}

	/**
	 * Get the number of invalid files.
	 * @note a missing file is invalid too!
	 * @return the number
	 */
	int GetNumInvalid() const
	{
		return Tnum_files - this->valid_files;
	}

	bool FillSetDetails(const IniFile &ini, const std::string &path, const std::string &full_filename, bool allow_empty_filename = true);
	void CopyCompatibleConfig([[maybe_unused]] const T &src) {}

	/**
	 * Get the description for the given ISO code.
	 * It falls back to the first two characters of the ISO code in case
	 * no match could be made with the full ISO code. If even then the
	 * matching fails the default is returned.
	 * @param isocode the isocode to search for
	 * @return the description
	 */
	const std::string &GetDescription(const std::string &isocode) const
	{
		if (!isocode.empty()) {
			/* First the full ISO code */
			auto desc = this->description.find(isocode);
			if (desc != this->description.end()) return desc->second;

			/* Then the first two characters */
			desc = this->description.find(isocode.substr(0, 2));
			if (desc != this->description.end()) return desc->second;
		}
		/* Then fall back */
		return this->description.at(std::string{});
	}

	/**
	 * Get string to use when listing this set in the settings window.
	 * If there are no invalid files, then this is just the set name,
	 * otherwise a string is formatted including the number of invalid files.
	 * @return the string to display.
	 */
	std::string GetListLabel() const
	{
		if (this->GetNumInvalid() == 0) return this->name;

		SetDParamStr(0, this->name);
		SetDParam(1, this->GetNumInvalid());
		return GetString(STR_BASESET_STATUS);
	}

	/**
	 * Calculate and check the MD5 hash of the supplied file.
	 * @param file The file get the hash of.
	 * @param subdir The sub directory to get the files from.
	 * @return
	 * - #CR_MATCH if the MD5 hash matches
	 * - #CR_MISMATCH if the MD5 does not match
	 * - #CR_NO_FILE if the file misses
	 */
	static MD5File::ChecksumResult CheckMD5(const MD5File *file, Subdirectory subdir)
	{
		return file->CheckMD5(subdir, SIZE_MAX);
	}

	/**
	 * Search a textfile file next to this base media.
	 * @param type The type of the textfile to search for.
	 * @return The filename for the textfile.
	 */
	std::optional<std::string> GetTextfile(TextfileType type) const
	{
		for (uint i = 0; i < NUM_FILES; i++) {
			auto textfile = ::GetTextfile(type, BASESET_DIR, this->files[i].filename);
			if (textfile.has_value()) {
				return textfile;
			}
		}
		return std::nullopt;
	}
};

/**
 * Base for all base media (graphics, sounds)
 * @tparam Tbase_set the real set we're going to be
 */
template <class Tbase_set>
class BaseMedia : FileScanner {
protected:
	static Tbase_set *available_sets; ///< All available sets
	static Tbase_set *duplicate_sets; ///< All sets that aren't available, but needed for not downloading base sets when a newer version than the one on BaNaNaS is loaded.
	static const Tbase_set *used_set; ///< The currently used set

	bool AddFile(const std::string &filename, size_t basepath_length, const std::string &tar_filename) override;

	/**
	 * Get the extension that is used to identify this set.
	 * @return the extension
	 */
	static const char *GetExtension();
public:
	/**
	 * Determine the graphics pack that has to be used.
	 * The one with the most correct files wins.
	 * @return true if a best set has been found.
	 */
	static bool DetermineBestSet();

	/** Do the scan for files. */
	static uint FindSets()
	{
		BaseMedia<Tbase_set> fs;
		/* Searching in tars is only done in the old "data" directories basesets. */
		uint num = fs.Scan(GetExtension(), Tbase_set::SEARCH_IN_TARS ? OLD_DATA_DIR : OLD_GM_DIR, Tbase_set::SEARCH_IN_TARS);
		return num + fs.Scan(GetExtension(), BASESET_DIR, Tbase_set::SEARCH_IN_TARS);
	}

	static Tbase_set *GetAvailableSets();

	static bool SetSet(const Tbase_set *set);
	static bool SetSetByName(const std::string &name);
	static bool SetSetByShortname(uint32_t shortname);
	static void GetSetsList(std::back_insert_iterator<std::string> &output_iterator);
	static int GetNumSets();
	static int GetIndexOfUsedSet();
	static const Tbase_set *GetSet(int index);
	static const Tbase_set *GetUsedSet();

	/**
	 * Check whether we have an set with the exact characteristics as ci.
	 * @param ci the characteristics to search on (shortname and md5sum)
	 * @param md5sum whether to check the MD5 checksum
	 * @return true iff we have an set matching.
	 */
	static bool HasSet(const ContentInfo *ci, bool md5sum);
};

template <class Tbase_set> /* static */ const Tbase_set *BaseMedia<Tbase_set>::used_set;
template <class Tbase_set> /* static */ Tbase_set *BaseMedia<Tbase_set>::available_sets;
template <class Tbase_set> /* static */ Tbase_set *BaseMedia<Tbase_set>::duplicate_sets;

/**
 * Check whether there's a base set matching some information.
 * @param ci The content info to compare it to.
 * @param md5sum Should the MD5 checksum be tested as well?
 * @param s The list with sets.
 * @return The filename of the first file of the base set, or \c nullptr if there is no match.
 */
template <class Tbase_set>
const char *TryGetBaseSetFile(const ContentInfo *ci, bool md5sum, const Tbase_set *s);

/** Types of graphics in the base graphics set */
enum GraphicsFileType {
	GFT_BASE,     ///< Base sprites for all climates
	GFT_LOGOS,    ///< Logos, landscape icons and original terrain generator sprites
	GFT_ARCTIC,   ///< Landscape replacement sprites for arctic
	GFT_TROPICAL, ///< Landscape replacement sprites for tropical
	GFT_TOYLAND,  ///< Landscape replacement sprites for toyland
	GFT_EXTRA,    ///< Extra sprites that were not part of the original sprites
	MAX_GFT,      ///< We are looking for this amount of GRFs
};

/** Blitter type for base graphics sets. */
enum BlitterType {
	BLT_8BPP,       ///< Base set has 8 bpp sprites only.
	BLT_32BPP,      ///< Base set has both 8 bpp and 32 bpp sprites.
};

struct GRFConfig;

/** All data of a graphics set. */
struct GraphicsSet : BaseSet<GraphicsSet, MAX_GFT, true> {
private:
	mutable std::unique_ptr<GRFConfig> extra_cfg; ///< Parameters for extra GRF
public:
	PaletteType palette;       ///< Palette of this graphics set
	BlitterType blitter;       ///< Blitter of this graphics set

	GraphicsSet();
	~GraphicsSet();

	bool FillSetDetails(const IniFile &ini, const std::string &path, const std::string &full_filename);
	GRFConfig *GetExtraConfig() const { return this->extra_cfg.get(); }
	GRFConfig &GetOrCreateExtraConfig() const;
	bool IsConfigurable() const;
	void CopyCompatibleConfig(const GraphicsSet &src);

	static MD5File::ChecksumResult CheckMD5(const MD5File *file, Subdirectory subdir);
};

/** All data/functions related with replacing the base graphics. */
class BaseGraphics : public BaseMedia<GraphicsSet> {
public:
	/** Values loaded from config file. */
	struct Ini {
		std::string name;
		uint32_t shortname;                 ///< unique key for base set
		uint32_t extra_version;             ///< version of the extra GRF
		std::vector<uint32_t> extra_params; ///< parameters for the extra GRF
	};
	static inline Ini ini_data;

};

/** All data of a sounds set. */
struct SoundsSet : BaseSet<SoundsSet, 1, true> {
};

/** All data/functions related with replacing the base sounds */
class BaseSounds : public BaseMedia<SoundsSet> {
public:
	/** The set as saved in the config file. */
	static inline std::string ini_set;

};

/** Maximum number of songs in the 'class' playlists. */
static const uint NUM_SONGS_CLASS     = 10;
/** Number of classes for songs */
static const uint NUM_SONG_CLASSES    = 3;
/** Maximum number of songs in the full playlist; theme song + the classes */
static const uint NUM_SONGS_AVAILABLE = 1 + NUM_SONG_CLASSES * NUM_SONGS_CLASS;

/** Maximum number of songs in the (custom) playlist */
static const uint NUM_SONGS_PLAYLIST  = 32;

/* Functions to read DOS music CAT files, similar to but not quite the same as sound effect CAT files */
char *GetMusicCatEntryName(const std::string &filename, size_t entrynum);
byte *GetMusicCatEntryData(const std::string &filename, size_t entrynum, size_t &entrylen);

enum MusicTrackType {
	MTT_STANDARDMIDI, ///< Standard MIDI file
	MTT_MPSMIDI,      ///< MPS GM driver MIDI format (contained in a CAT file)
};

/** Metadata about a music track. */
struct MusicSongInfo {
	std::string songname;    ///< name of song displayed in UI
	byte tracknr;            ///< track number of song displayed in UI
	std::string filename;    ///< file on disk containing song (when used in MusicSet class)
	MusicTrackType filetype; ///< decoder required for song file
	int cat_index;           ///< entry index in CAT file, for filetype==MTT_MPSMIDI
	bool loop;               ///< song should play in a tight loop if possible, never ending
	int override_start;      ///< MIDI ticks to skip over in beginning
	int override_end;        ///< MIDI tick to end the song at (0 if no override)
};

/** All data of a music set. */
struct MusicSet : BaseSet<MusicSet, NUM_SONGS_AVAILABLE, false> {
	/** Data about individual songs in set. */
	MusicSongInfo songinfo[NUM_SONGS_AVAILABLE];
	/** Number of valid songs in set. */
	byte num_available;

	bool FillSetDetails(const IniFile &ini, const std::string &path, const std::string &full_filename);
};

/** All data/functions related with replacing the base music */
class BaseMusic : public BaseMedia<MusicSet> {
public:
	/** The set as saved in the config file. */
	static inline std::string ini_set;

};

#endif /* BASE_MEDIA_BASE_H */
