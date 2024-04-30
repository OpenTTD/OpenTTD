/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fileio_func.h Functions for Standard In/Out file operations */

#ifndef FILEIO_FUNC_H
#define FILEIO_FUNC_H

#include "core/enum_type.hpp"
#include "fileio_type.h"

void FioFCloseFile(FILE *f);
FILE *FioFOpenFile(const std::string &filename, const char *mode, Subdirectory subdir, size_t *filesize = nullptr);
bool FioCheckFileExists(const std::string &filename, Subdirectory subdir);
std::string FioFindFullPath(Subdirectory subdir, const std::string &filename);
std::string FioGetDirectory(Searchpath sp, Subdirectory subdir);
std::string FioFindDirectory(Subdirectory subdir);
void FioCreateDirectory(const std::string &name);
bool FioRemove(const std::string &filename);

const char *FiosGetScreenshotDir();

void SanitizeFilename(std::string &filename);
void AppendPathSeparator(std::string &buf);
void DeterminePaths(const char *exe, bool only_local_path);
std::unique_ptr<char[]> ReadFileToMem(const std::string &filename, size_t &lenp, size_t maxsize);
bool FileExists(const std::string &filename);
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
	enum Mode {
		NONE     = 0,      ///< Scan nothing.
		BASESET  = 1 << 0, ///< Scan for base sets.
		NEWGRF   = 1 << 1, ///< Scan for non-base sets.
		AI       = 1 << 2, ///< Scan for AIs and its libraries.
		SCENARIO = 1 << 3, ///< Scan for scenarios and heightmaps.
		GAME     = 1 << 4, ///< Scan for game scripts.
		ALL      = BASESET | NEWGRF | AI | SCENARIO | GAME, ///< Scan for everything.
	};

	bool AddFile(const std::string &filename, size_t basepath_length, const std::string &tar_filename = {}) override;

	bool AddFile(Subdirectory sd, const std::string &filename);

	/** Do the scan for Tars. */
	static uint DoScan(TarScanner::Mode mode);
};

DECLARE_ENUM_AS_BIT_SET(TarScanner::Mode)

/** Auto-close a file upon scope exit. */
class FileCloser {
	FILE *f;

public:
	FileCloser(FILE *_f) : f(_f) {}
	~FileCloser()
	{
		fclose(f);
	}
};

/** Helper to manage a FILE with a \c std::unique_ptr. */
struct FileDeleter {
	void operator()(FILE *f)
	{
		if (f != nullptr) fclose(f);
	}
};

/** Helper type for RAII management of a FILE that gets closed when it goes out of scope. */
using AutoCloseFile = std::unique_ptr<FILE, FileDeleter>;

#endif /* FILEIO_FUNC_H */
