/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file base_media_base.h Generic functions for replacing base data (graphics, sounds). */

#ifndef BASE_MEDIA_BASE_H
#define BASE_MEDIA_BASE_H

#include "fileio_func.h"
#include "textfile_type.h"
#include "textfile_gui.h"
#include "3rdparty/md5/md5.h"
#include <unordered_map>

struct IniFile;
struct IniGroup;
struct IniItem;
struct ContentInfo;

/** Structure holding filename and MD5 information about a single file */
struct MD5File {
	/** The result of a checksum check */
	enum ChecksumResult : uint8_t {
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

/** Defines the traits of a BaseSet type. */
template <class T> struct BaseSetTraits;

/**
 * Information about a single base set.
 * @tparam T the real class we're going to be
 */
template <class T>
struct BaseSet {
	typedef std::unordered_map<std::string, std::string, StringHash, std::equal_to<>> TranslatedStrings;

	/** Number of files in this set */
	static constexpr size_t NUM_FILES = BaseSetTraits<T>::num_files;

	/** Whether to search in the tars or not. */
	static constexpr bool SEARCH_IN_TARS = BaseSetTraits<T>::search_in_tars;

	/** BaseSet type name. */
	static constexpr std::string_view SET_TYPE = BaseSetTraits<T>::set_type;

	std::string name;              ///< The name of the base set
	std::string url;               ///< URL for information about the base set
	TranslatedStrings description; ///< Description of the base set
	uint32_t shortname = 0; ///< Four letter short variant of the name
	std::vector<uint32_t> version; ///< The version of this base set
	bool fallback = false; ///< This set is a fallback set, i.e. it should be used only as last resort

	std::array<MD5File, BaseSet<T>::NUM_FILES> files{}; ///< All files part of this set
	uint found_files = 0; ///< Number of the files that could be found
	uint valid_files = 0; ///< Number of the files that could be found and are valid

	/**
	 * Get the number of missing files.
	 * @return the number
	 */
	int GetNumMissing() const
	{
		return BaseSet<T>::NUM_FILES - this->found_files;
	}

	/**
	 * Get the number of invalid files.
	 * @note a missing file is invalid too!
	 * @return the number
	 */
	int GetNumInvalid() const
	{
		return BaseSet<T>::NUM_FILES - this->valid_files;
	}

	void LogError(std::string_view full_filename, std::string_view detail, int level = 0) const;
	const IniItem *GetMandatoryItem(std::string_view full_filename, const IniGroup &group, std::string_view name) const;

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
	const std::string &GetDescription(std::string_view isocode) const
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
		for (const auto &file : this->files) {
			auto textfile = ::GetTextfile(type, BASESET_DIR, file.filename);
			if (textfile.has_value()) {
				return textfile;
			}
		}
		return std::nullopt;
	}

	/**
	 * Get the internal names of the files in this set.
	 * @return the internal filenames
	 */
	static std::span<const std::string_view> GetFilenames();
};

/**
 * Base for all base media (graphics, sounds)
 * @tparam Tbase_set the real set we're going to be
 */
template <class Tbase_set>
class BaseMedia : FileScanner {
protected:
	static inline std::vector<std::unique_ptr<Tbase_set>> available_sets; ///< All available sets
	static inline std::vector<std::unique_ptr<Tbase_set>> duplicate_sets; ///< All sets that aren't available, but needed for not downloading base sets when a newer version than the one on BaNaNaS is loaded.
	static inline const Tbase_set *used_set; ///< The currently used set

	bool AddFile(const std::string &filename, size_t basepath_length, const std::string &tar_filename) override;

	/**
	 * Get the extension that is used to identify this set.
	 * @return the extension
	 */
	static std::string_view GetExtension();

	/**
	 * Return the duplicate sets.
	 * @return The duplicate sets.
	 */
	 static std::span<const std::unique_ptr<Tbase_set>> GetDuplicateSets() { return BaseMedia<Tbase_set>::duplicate_sets; }
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

	/**
	 * Return the available sets.
	 * @return The available sets.
	 */
	static std::span<const std::unique_ptr<Tbase_set>> GetAvailableSets() { return BaseMedia<Tbase_set>::available_sets; }

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
	static bool HasSet(const ContentInfo &ci, bool md5sum);
};

/**
 * Check whether there's a base set matching some information.
 * @param ci The content info to compare it to.
 * @param md5sum Should the MD5 checksum be tested as well?
 * @param s The list with sets.
 * @return The filename of the first file of the base set, or \c std::nullopt if there is no match.
 */
template <class Tbase_set>
std::optional<std::string_view> TryGetBaseSetFile(const ContentInfo &ci, bool md5sum, std::span<const std::unique_ptr<Tbase_set>> sets);

#endif /* BASE_MEDIA_BASE_H */
