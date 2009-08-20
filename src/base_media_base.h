/* $Id$ */

/** @file base_media_base.h Generic functions for replacing base data (graphics, sounds). */

#ifndef BASE_MEDIA_BASE_H
#define BASE_MEDIA_BASE_H

#include "fileio_func.h"

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

	ChecksumResult CheckMD5() const;
};

/**
 * Information about a single base set.
 * @tparam T the real class we're going to be
 * @tparam Tnum_files the number of files in the set
 */
template <class T, size_t Tnum_files>
struct BaseSet {
	/** Number of files in this set */
	static const size_t NUM_FILES = Tnum_files;

	/** Internal names of the files in this set. */
	static const char **file_names;

	const char *name;          ///< The name of the base set
	const char *description;   ///< Description of the base set
	uint32 shortname;          ///< Four letter short variant of the name
	uint32 version;            ///< The version of this base set

	MD5File files[NUM_FILES];  ///< All files part of this set
	uint found_files;          ///< Number of the files that could be found
	uint valid_files;          ///< Number of the files that could be found and are valid

	T *next;                   ///< The next base set in this list

	/** Free everything we allocated */
	~BaseSet()
	{
		free((void*)this->name);
		free((void*)this->description);
		for (uint i = 0; i < NUM_FILES; i++) {
			free((void*)this->files[i].filename);
			free((void*)this->files[i].missing_warning);
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

	/**
	 * Read the set information from a loaded ini.
	 * @param ini      the ini to read from
	 * @param path     the path to this ini file (for filenames)
	 * @return true if loading was successful.
	 */
	bool FillSetDetails(IniFile *ini, const char *path);
};

/**
 * Base for all base media (graphics, sounds)
 * @tparam Tbase_set the real set we're going to be
 */
template <class Tbase_set>
class BaseMedia : FileScanner {
protected:
	static Tbase_set *available_sets; ///< All available sets
	static const Tbase_set *used_set; ///< The currently used set

	/* virtual */ bool AddFile(const char *filename, size_t basepath_length);

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
		return fs.Scan(GetExtension(), DATA_DIR);
	}

	/**
	 * Set the set to be used.
	 * @param name of the set to use
	 * @return true if it could be loaded
	 */
	static bool SetSet(const char *name);

	/**
	 * Returns a list with the sets.
	 * @param p    where to print to
	 * @param last the last character to print to
	 * @return the last printed character
	 */
	static char *GetSetsList(char *p, const char *last);

	/**
	 * Count the number of available graphics sets.
	 * @return the number of sets
	 */
	static int GetNumSets();

	/**
	 * Get the index of the currently active graphics set
	 * @return the current set's index
	 */
	static int GetIndexOfUsedSet();

	/**
	 * Get the name of the graphics set at the specified index
	 * @return the name of the set
	 */
	static const Tbase_set *GetSet(int index);
	/**
	 * Return the used set.
	 * @return the used set.
	 */
	static const Tbase_set *GetUsedSet();

	/**
	 * Check whether we have an set with the exact characteristics as ci.
	 * @param ci the characteristics to search on (shortname and md5sum)
	 * @param md5sum whether to check the MD5 checksum
	 * @return true iff we have an set matching.
	 */
	static bool HasSet(const ContentInfo *ci, bool md5sum);
};


/** Types of graphics in the base graphics set */
enum GraphicsFileType {
	GFT_BASE,     ///< Base sprites for all climates
	GFT_LOGOS,    ///< Logos, landscape icons and original terrain generator sprites
	GFT_ARCTIC,   ///< Landscape replacement sprites for arctic
	GFT_TROPICAL, ///< Landscape replacement sprites for tropical
	GFT_TOYLAND,  ///< Landscape replacement sprites for toyland
	GFT_EXTRA,    ///< Extra sprites that were not part of the original sprites
	MAX_GFT       ///< We are looking for this amount of GRFs
};

/** All data of a graphics set. */
struct GraphicsSet : BaseSet<GraphicsSet, MAX_GFT> {
	PaletteType palette;       ///< Palette of this graphics set

	bool FillSetDetails(struct IniFile *ini, const char *path);
};

/** All data/functions related with replacing the base graphics. */
class BaseGraphics : public BaseMedia<GraphicsSet> {
public:
	/**
	 * Determine the palette of the current graphics set.
	 */
	static void DeterminePalette();
};

/** All data of a sounds set. */
struct SoundsSet : BaseSet<SoundsSet, 1> {
};

/** All data/functions related with replacing the base sounds */
class BaseSounds : public BaseMedia<SoundsSet> {
public:
};

#endif /* BASE_MEDIA_BASE_H */
