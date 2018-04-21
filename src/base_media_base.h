/* $Id$ */

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
#include "core/smallmap_type.hpp"
#include "gfx_type.h"
#include "textfile_type.h"
#include "textfile_gui.h"

/* Forward declare these; can't do 'struct X' in functions as older GCCs barf on that */
struct IniFile;
struct ContentInfo;

/** Structure holding filename and MD5 information about a single file */
struct MD5File {
	/** The result of a checksum check */
	enum ChecksumResult {
		CR_MATCH,    ///< The file did exist and the md5 checksum did match
		CR_MISMATCH, ///< The file did exist, just the md5 checksum did not match
		CR_NO_FILE,  ///< The file did not exist
	};

	const char *filename;        ///< filename
	uint8 hash[16];              ///< md5 sum of the file
	const char *missing_warning; ///< warning when this file is missing

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
	typedef SmallMap<const char *, const char *> TranslatedStrings;

	/** Number of files in this set */
	static const size_t NUM_FILES = Tnum_files;

	/** Whether to search in the tars or not. */
	static const bool SEARCH_IN_TARS = Tsearch_in_tars;

	/** Internal names of the files in this set. */
	static const char * const *file_names;

	const char *name;              ///< The name of the base set
	TranslatedStrings description; ///< Description of the base set
	uint32 shortname;              ///< Four letter short variant of the name
	uint32 version;                ///< The version of this base set
	bool fallback;                 ///< This set is a fallback set, i.e. it should be used only as last resort

	MD5File files[NUM_FILES];      ///< All files part of this set
	uint found_files;              ///< Number of the files that could be found
	uint valid_files;              ///< Number of the files that could be found and are valid

	T *next;                       ///< The next base set in this list

	/** Free everything we allocated */
	~BaseSet()
	{
		free(this->name);

		for (TranslatedStrings::iterator iter = this->description.Begin(); iter != this->description.End(); iter++) {
			free(iter->first);
			free(iter->second);
		}

		for (uint i = 0; i < NUM_FILES; i++) {
			free(this->files[i].filename);
			free(this->files[i].missing_warning);
		}

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

	bool FillSetDetails(IniFile *ini, const char *path, const char *full_filename, bool allow_empty_filename = true);

	/**
	 * Get the description for the given ISO code.
	 * It falls back to the first two characters of the ISO code in case
	 * no match could be made with the full ISO code. If even then the
	 * matching fails the default is returned.
	 * @param isocode the isocode to search for
	 * @return the description
	 */
	const char *GetDescription(const char *isocode = NULL) const
	{
		if (isocode != NULL) {
			/* First the full ISO code */
			for (TranslatedStrings::const_iterator iter = this->description.Begin(); iter != this->description.End(); iter++) {
				if (strcmp(iter->first, isocode) == 0) return iter->second;
			}
			/* Then the first two characters */
			for (TranslatedStrings::const_iterator iter = this->description.Begin(); iter != this->description.End(); iter++) {
				if (strncmp(iter->first, isocode, 2) == 0) return iter->second;
			}
		}
		/* Then fall back */
		return this->description.Begin()->second;
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
	 * @return The filename for the textfile, \c NULL otherwise.
	 */
	const char *GetTextfile(TextfileType type) const
	{
		for (uint i = 0; i < NUM_FILES; i++) {
			const char *textfile = ::GetTextfile(type, BASESET_DIR, this->files[i].filename);
			if (textfile != NULL) {
				return textfile;
			}
		}
		return NULL;
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

	/* virtual */ bool AddFile(const char *filename, size_t basepath_length, const char *tar_filename);

	/**
	 * Get the extension that is used to identify this set.
	 * @return the extension
	 */
	static const char *GetExtension();
public:
	/** The set as saved in the config file. */
	static const char *ini_set;

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

	static bool SetSet(const char *name);
	static char *GetSetsList(char *p, const char *last);
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

template <class Tbase_set> /* static */ const char *BaseMedia<Tbase_set>::ini_set;
template <class Tbase_set> /* static */ const Tbase_set *BaseMedia<Tbase_set>::used_set;
template <class Tbase_set> /* static */ Tbase_set *BaseMedia<Tbase_set>::available_sets;
template <class Tbase_set> /* static */ Tbase_set *BaseMedia<Tbase_set>::duplicate_sets;

/**
 * Check whether there's a base set matching some information.
 * @param ci The content info to compare it to.
 * @param md5sum Should the MD5 checksum be tested as well?
 * @param s The list with sets.
 * @return The filename of the first file of the base set, or \c NULL if there is no match.
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

/** All data of a graphics set. */
struct GraphicsSet : BaseSet<GraphicsSet, MAX_GFT, true> {
	PaletteType palette;       ///< Palette of this graphics set
	BlitterType blitter;       ///< Blitter of this graphics set

	bool FillSetDetails(struct IniFile *ini, const char *path, const char *full_filename);

	static MD5File::ChecksumResult CheckMD5(const MD5File *file, Subdirectory subdir);
};

/** All data/functions related with replacing the base graphics. */
class BaseGraphics : public BaseMedia<GraphicsSet> {
public:
};

/** All data of a sounds set. */
struct SoundsSet : BaseSet<SoundsSet, 1, true> {
};

/** All data/functions related with replacing the base sounds */
class BaseSounds : public BaseMedia<SoundsSet> {
public:
};

/** Maximum number of songs in the 'class' playlists. */
static const uint NUM_SONGS_CLASS     = 10;
/** Number of classes for songs */
static const uint NUM_SONG_CLASSES    = 3;
/** Maximum number of songs in the full playlist; theme song + the classes */
static const uint NUM_SONGS_AVAILABLE = 1 + NUM_SONG_CLASSES * NUM_SONGS_CLASS;

/** Maximum number of songs in the (custom) playlist */
static const uint NUM_SONGS_PLAYLIST  = 32;

/** All data of a music set. */
struct MusicSet : BaseSet<MusicSet, NUM_SONGS_AVAILABLE, false> {
	/** The name of the different songs. */
	char song_name[NUM_SONGS_AVAILABLE][32];
	byte track_nr[NUM_SONGS_AVAILABLE];
	byte num_available;

	bool FillSetDetails(struct IniFile *ini, const char *path, const char *full_filename);
};

/** All data/functions related with replacing the base music */
class BaseMusic : public BaseMedia<MusicSet> {
public:
};

#endif /* BASE_MEDIA_BASE_H */
