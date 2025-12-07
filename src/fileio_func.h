/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fileio_func.h Functions for Standard In/Out file operations */

#ifndef FILEIO_FUNC_H
#define FILEIO_FUNC_H

#include "core/enum_type.hpp"
#include "fileio_type.h"

std::optional<FileHandle> FioFOpenFile(std::string_view filename, std::string_view mode, Subdirectory subdir, size_t *filesize = nullptr);
bool FioCheckFileExists(std::string_view filename, Subdirectory subdir);
std::string FioFindFullPath(Subdirectory subdir, std::string_view filename);
std::string FioGetDirectory(Searchpath sp, Subdirectory subdir);
std::string FioFindDirectory(Subdirectory subdir);
void FioCreateDirectory(const std::string &name);
bool FioRemove(const std::string &filename);

std::string_view FiosGetScreenshotDir();

void SanitizeFilename(std::string &filename);
void AppendPathSeparator(std::string &buf);
void DeterminePaths(std::string_view exe, bool only_local_path);
std::unique_ptr<char[]> ReadFileToMem(const std::string &filename, size_t &lenp, size_t maxsize);
bool FileExists(std::string_view filename);
bool ExtractTar(const std::string &tar_filename, Subdirectory subdir);

extern std::string _personal_dir; ///< custom directory for personal settings, saves, newgrf, etc.
extern std::vector<Searchpath> _valid_searchpaths;

/** Helper for scanning for files with a given name */
class FileScanner {
protected:
	Subdirectory subdir; ///< The current sub directory we are searching through
public:
	/** Destruct the proper one... */
	virtual ~FileScanner() = default;

	uint Scan(std::string_view extension, Subdirectory sd, bool tars = true, bool recursive = true);
	uint Scan(std::string_view extension, const std::string &directory, bool recursive = true);

	/**
	 * Add a file with the given filename.
	 * @param filename        the full path to the file to read
	 * @param basepath_length amount of characters to chop of before to get a
	 *                        filename relative to the search path.
	 * @param tar_filename    the name of the tar file the file is read from.
	 * @return true if the file is added.
	 */
	virtual bool AddFile(const std::string &filename, size_t basepath_length, const std::string &tar_filename) = 0;
};

/** Helper for scanning for files with tar as extension */
class TarScanner : FileScanner {
	uint DoScan(Subdirectory sd);
public:
	/** The mode of tar scanning. */
	enum class Mode : uint8_t {
		Baseset, ///< Scan for base sets.
		NewGRF, ///< Scan for non-base sets.
		AI, ///< Scan for AIs and its libraries.
		Scenario, ///< Scan for scenarios and heightmaps.
		Game, ///< Scan for game scripts.
	};
	using Modes = EnumBitSet<Mode, uint8_t>;

	static constexpr Modes MODES_ALL = {Mode::Baseset, Mode::NewGRF, Mode::AI, Mode::Scenario, Mode::Game}; ///< Scan for everything.

	bool AddFile(const std::string &filename, size_t basepath_length, const std::string &tar_filename = {}) override;

	bool AddFile(Subdirectory sd, const std::string &filename);

	/** Do the scan for Tars. */
	static uint DoScan(TarScanner::Modes modes);
};

#endif /* FILEIO_FUNC_H */
