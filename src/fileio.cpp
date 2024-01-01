/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fileio.cpp Standard In/Out file operations */

#include "stdafx.h"
#include "fileio_func.h"
#include "spriteloader/spriteloader.hpp"
#include "debug.h"
#include "fios.h"
#include "string_func.h"
#include "tar_type.h"
#ifdef _WIN32
#include <windows.h>
# define access _taccess
#elif defined(__HAIKU__)
#include <Path.h>
#include <storage/FindDirectory.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif
#include <sys/stat.h>
#include <sstream>
#include <filesystem>

#include "safeguards.h"

/** Whether the working directory should be scanned. */
static bool _do_scan_working_directory = true;

extern std::string _config_file;
extern std::string _highscore_file;

static const char * const _subdirs[] = {
	"",
	"save" PATHSEP,
	"save" PATHSEP "autosave" PATHSEP,
	"scenario" PATHSEP,
	"scenario" PATHSEP "heightmap" PATHSEP,
	"gm" PATHSEP,
	"data" PATHSEP,
	"baseset" PATHSEP,
	"newgrf" PATHSEP,
	"lang" PATHSEP,
	"ai" PATHSEP,
	"ai" PATHSEP "library" PATHSEP,
	"game" PATHSEP,
	"game" PATHSEP "library" PATHSEP,
	"screenshot" PATHSEP,
};
static_assert(lengthof(_subdirs) == NUM_SUBDIRS);

/**
 * The search paths OpenTTD could search through.
 * At least one of the slots has to be filled with a path.
 * An empty string tells that there is no such path for the
 * current operating system.
 */
std::array<std::string, NUM_SEARCHPATHS> _searchpaths;
std::vector<Searchpath> _valid_searchpaths;
std::array<TarList, NUM_SUBDIRS> _tar_list;
TarFileList _tar_filelist[NUM_SUBDIRS];

typedef std::map<std::string, std::string> TarLinkList;
static TarLinkList _tar_linklist[NUM_SUBDIRS]; ///< List of directory links

extern bool FiosIsValidFile(const std::string &path, const struct dirent *ent, struct stat *sb);

/**
 * Checks whether the given search path is a valid search path
 * @param sp the search path to check
 * @return true if the search path is valid
 */
static bool IsValidSearchPath(Searchpath sp)
{
	return sp < _searchpaths.size() && !_searchpaths[sp].empty();
}

static void FillValidSearchPaths(bool only_local_path)
{
	_valid_searchpaths.clear();

	std::set<std::string> seen{};
	for (Searchpath sp = SP_FIRST_DIR; sp < NUM_SEARCHPATHS; sp++) {
		if (sp == SP_WORKING_DIR) continue;

		if (only_local_path) {
			switch (sp) {
				case SP_WORKING_DIR:      // Can be influence by "-c" option.
				case SP_BINARY_DIR:       // Most likely contains all the language files.
				case SP_AUTODOWNLOAD_DIR: // Otherwise we cannot download in-game content.
					break;

				default:
					continue;
			}
		}

		if (IsValidSearchPath(sp)) {
			if (seen.count(_searchpaths[sp]) != 0) continue;
			seen.insert(_searchpaths[sp]);
			_valid_searchpaths.emplace_back(sp);
		}
	}

	/* The working-directory is special, as it is controlled by _do_scan_working_directory.
	 * Only add the search path if it isn't already in the set. To preserve the same order
	 * as the enum, insert it in the front. */
	if (IsValidSearchPath(SP_WORKING_DIR) && seen.count(_searchpaths[SP_WORKING_DIR]) == 0) {
		_valid_searchpaths.insert(_valid_searchpaths.begin(), SP_WORKING_DIR);
	}
}

/**
 * Check whether the given file exists
 * @param filename the file to try for existence.
 * @param subdir the subdirectory to look in
 * @return true if and only if the file can be opened
 */
bool FioCheckFileExists(const std::string &filename, Subdirectory subdir)
{
	FILE *f = FioFOpenFile(filename, "rb", subdir);
	if (f == nullptr) return false;

	FioFCloseFile(f);
	return true;
}

/**
 * Test whether the given filename exists.
 * @param filename the file to test.
 * @return true if and only if the file exists.
 */
bool FileExists(const std::string &filename)
{
	return access(OTTD2FS(filename).c_str(), 0) == 0;
}

/**
 * Close a file in a safe way.
 */
void FioFCloseFile(FILE *f)
{
	fclose(f);
}

/**
 * Find a path to the filename in one of the search directories.
 * @param subdir Subdirectory to try.
 * @param filename Filename to look for.
 * @return String containing the path if the path was found, else an empty string.
 */
std::string FioFindFullPath(Subdirectory subdir, const std::string &filename)
{
	assert(subdir < NUM_SUBDIRS);

	for (Searchpath sp : _valid_searchpaths) {
		std::string buf = FioGetDirectory(sp, subdir);
		buf += filename;
		if (FileExists(buf)) return buf;
#if !defined(_WIN32)
		/* Be, as opening files, aware that sometimes the filename
		 * might be in uppercase when it is in lowercase on the
		 * disk. Of course Windows doesn't care about casing. */
		if (strtolower(buf, _searchpaths[sp].size() - 1) && FileExists(buf)) return buf;
#endif
	}

	return {};
}

std::string FioGetDirectory(Searchpath sp, Subdirectory subdir)
{
	assert(subdir < NUM_SUBDIRS);
	assert(sp < NUM_SEARCHPATHS);

	return _searchpaths[sp] + _subdirs[subdir];
}

std::string FioFindDirectory(Subdirectory subdir)
{
	/* Find and return the first valid directory */
	for (Searchpath sp : _valid_searchpaths) {
		std::string ret = FioGetDirectory(sp, subdir);
		if (FileExists(ret)) return ret;
	}

	/* Could not find the directory, fall back to a base path */
	return _personal_dir;
}

static FILE *FioFOpenFileSp(const std::string &filename, const char *mode, Searchpath sp, Subdirectory subdir, size_t *filesize)
{
#if defined(_WIN32)
	/* fopen is implemented as a define with ellipses for
	 * Unicode support (prepend an L). As we are not sending
	 * a string, but a variable, it 'renames' the variable,
	 * so make that variable to makes it compile happily */
	wchar_t Lmode[5];
	MultiByteToWideChar(CP_ACP, 0, mode, -1, Lmode, lengthof(Lmode));
#endif
	FILE *f = nullptr;
	std::string buf;

	if (subdir == NO_DIRECTORY) {
		buf = filename;
	} else {
		buf = _searchpaths[sp] + _subdirs[subdir] + filename;
	}

#if defined(_WIN32)
	if (mode[0] == 'r' && GetFileAttributes(OTTD2FS(buf).c_str()) == INVALID_FILE_ATTRIBUTES) return nullptr;
#endif

	f = fopen(buf.c_str(), mode);
#if !defined(_WIN32)
	if (f == nullptr && strtolower(buf, subdir == NO_DIRECTORY ? 0 : _searchpaths[sp].size() - 1) ) {
		f = fopen(buf.c_str(), mode);
	}
#endif
	if (f != nullptr && filesize != nullptr) {
		/* Find the size of the file */
		fseek(f, 0, SEEK_END);
		*filesize = ftell(f);
		fseek(f, 0, SEEK_SET);
	}
	return f;
}

/**
 * Opens a file from inside a tar archive.
 * @param entry The entry to open.
 * @param[out] filesize If not \c nullptr, size of the opened file.
 * @return File handle of the opened file, or \c nullptr if the file is not available.
 * @note The file is read from within the tar file, and may not return \c EOF after reading the whole file.
 */
FILE *FioFOpenFileTar(const TarFileListEntry &entry, size_t *filesize)
{
	FILE *f = fopen(entry.tar_filename.c_str(), "rb");
	if (f == nullptr) return f;

	if (fseek(f, entry.position, SEEK_SET) < 0) {
		fclose(f);
		return nullptr;
	}

	if (filesize != nullptr) *filesize = entry.size;
	return f;
}

/**
 * Opens a OpenTTD file somewhere in a personal or global directory.
 * @param filename Name of the file to open.
 * @param subdir Subdirectory to open.
 * @return File handle of the opened file, or \c nullptr if the file is not available.
 */
FILE *FioFOpenFile(const std::string &filename, const char *mode, Subdirectory subdir, size_t *filesize)
{
	FILE *f = nullptr;

	assert(subdir < NUM_SUBDIRS || subdir == NO_DIRECTORY);

	for (Searchpath sp : _valid_searchpaths) {
		f = FioFOpenFileSp(filename, mode, sp, subdir, filesize);
		if (f != nullptr || subdir == NO_DIRECTORY) break;
	}

	/* We can only use .tar in case of data-dir, and read-mode */
	if (f == nullptr && mode[0] == 'r' && subdir != NO_DIRECTORY) {
		/* Filenames in tars are always forced to be lowercase */
		std::string resolved_name = filename;
		strtolower(resolved_name);

		/* Resolve ".." */
		std::istringstream ss(resolved_name);
		std::vector<std::string> tokens;
		std::string token;
		while (std::getline(ss, token, PATHSEPCHAR)) {
			if (token == "..") {
				if (tokens.size() < 2) return nullptr;
				tokens.pop_back();
			} else if (token == ".") {
				/* Do nothing. "." means current folder, but you can create tar files with "." in the path.
				 * This confuses our file resolver. So, act like this folder doesn't exist. */
			} else {
				tokens.push_back(token);
			}
		}

		resolved_name.clear();
		bool first = true;
		for (const std::string &token : tokens) {
			if (!first) {
				resolved_name += PATHSEP;
			}
			resolved_name += token;
			first = false;
		}

		/* Resolve ONE directory link */
		for (const auto &link : _tar_linklist[subdir]) {
			const std::string &src = link.first;
			size_t len = src.length();
			if (resolved_name.length() >= len && resolved_name[len - 1] == PATHSEPCHAR && src.compare(0, len, resolved_name, 0, len) == 0) {
				/* Apply link */
				resolved_name.replace(0, len, link.second);
				break; // Only resolve one level
			}
		}

		TarFileList::iterator it = _tar_filelist[subdir].find(resolved_name);
		if (it != _tar_filelist[subdir].end()) {
			f = FioFOpenFileTar(it->second, filesize);
		}
	}

	/* Sometimes a full path is given. To support
	 * the 'subdirectory' must be 'removed'. */
	if (f == nullptr && subdir != NO_DIRECTORY) {
		switch (subdir) {
			case BASESET_DIR:
				f = FioFOpenFile(filename, mode, OLD_GM_DIR, filesize);
				if (f != nullptr) break;
				FALLTHROUGH;
			case NEWGRF_DIR:
				f = FioFOpenFile(filename, mode, OLD_DATA_DIR, filesize);
				break;

			default:
				f = FioFOpenFile(filename, mode, NO_DIRECTORY, filesize);
				break;
		}
	}

	return f;
}

/**
 * Create a directory with the given name
 * If the parent directory does not exist, it will try to create that as well.
 * @param name the new name of the directory
 */
void FioCreateDirectory(const std::string &name)
{
	auto p = name.find_last_of(PATHSEPCHAR);
	if (p != std::string::npos) {
		std::string dirname = name.substr(0, p);
		DIR *dir = ttd_opendir(dirname.c_str());
		if (dir == nullptr) {
			FioCreateDirectory(dirname); // Try creating the parent directory, if we couldn't open it
		} else {
			closedir(dir);
		}
	}

	/* Ignore directory creation errors; they'll surface later on, and most
	 * of the time they are 'directory already exists' errors anyhow. */
#if defined(_WIN32)
	CreateDirectory(OTTD2FS(name).c_str(), nullptr);
#else
	mkdir(OTTD2FS(name).c_str(), 0755);
#endif
}

/**
 * Appends, if necessary, the path separator character to the end of the string.
 * It does not add the path separator to zero-sized strings.
 * @param buf  string to append the separator to
 * @return true iff the operation succeeded
 */
void AppendPathSeparator(std::string &buf)
{
	if (buf.empty()) return;

	if (buf.back() != PATHSEPCHAR) buf.push_back(PATHSEPCHAR);
}

static void TarAddLink(const std::string &srcParam, const std::string &destParam, Subdirectory subdir)
{
	std::string src = srcParam;
	std::string dest = destParam;
	/* Tar internals assume lowercase */
	std::transform(src.begin(), src.end(), src.begin(), tolower);
	std::transform(dest.begin(), dest.end(), dest.begin(), tolower);

	TarFileList::iterator dest_file = _tar_filelist[subdir].find(dest);
	if (dest_file != _tar_filelist[subdir].end()) {
		/* Link to file. Process the link like the destination file. */
		_tar_filelist[subdir].insert(TarFileList::value_type(src, dest_file->second));
	} else {
		/* Destination file not found. Assume 'link to directory'
		 * Append PATHSEPCHAR to 'src' and 'dest' if needed */
		const std::string src_path = ((*src.rbegin() == PATHSEPCHAR) ? src : src + PATHSEPCHAR);
		const std::string dst_path = (dest.length() == 0 ? "" : ((*dest.rbegin() == PATHSEPCHAR) ? dest : dest + PATHSEPCHAR));
		_tar_linklist[subdir].insert(TarLinkList::value_type(src_path, dst_path));
	}
}

/**
 * Simplify filenames from tars.
 * Replace '/' by #PATHSEPCHAR, and force 'name' to lowercase.
 * @param name Filename to process.
 */
static void SimplifyFileName(std::string &name)
{
	for (char &c : name) {
		/* Force lowercase */
		c = std::tolower(c);
#if (PATHSEPCHAR != '/')
		/* Tar-files always have '/' path-separator, but we want our PATHSEPCHAR */
		if (c == '/') c = PATHSEPCHAR;
#endif
	}
}

/**
 * Perform the scanning of a particular subdirectory.
 * @param sd The subdirectory to scan.
 * @return The number of found tar files.
 */
uint TarScanner::DoScan(Subdirectory sd)
{
	_tar_filelist[sd].clear();
	_tar_list[sd].clear();
	uint num = this->Scan(".tar", sd, false);
	if (sd == BASESET_DIR || sd == NEWGRF_DIR) num += this->Scan(".tar", OLD_DATA_DIR, false);
	return num;
}

/* static */ uint TarScanner::DoScan(TarScanner::Mode mode)
{
	Debug(misc, 2, "Scanning for tars");
	TarScanner fs;
	uint num = 0;
	if (mode & TarScanner::BASESET) {
		num += fs.DoScan(BASESET_DIR);
	}
	if (mode & TarScanner::NEWGRF) {
		num += fs.DoScan(NEWGRF_DIR);
	}
	if (mode & TarScanner::AI) {
		num += fs.DoScan(AI_DIR);
		num += fs.DoScan(AI_LIBRARY_DIR);
	}
	if (mode & TarScanner::GAME) {
		num += fs.DoScan(GAME_DIR);
		num += fs.DoScan(GAME_LIBRARY_DIR);
	}
	if (mode & TarScanner::SCENARIO) {
		num += fs.DoScan(SCENARIO_DIR);
		num += fs.DoScan(HEIGHTMAP_DIR);
	}
	Debug(misc, 2, "Scan complete, found {} files", num);
	return num;
}

/**
 * Add a single file to the scanned files of a tar, circumventing the scanning code.
 * @param sd       The sub directory the file is in.
 * @param filename The name of the file to add.
 * @return True if the additions went correctly.
 */
bool TarScanner::AddFile(Subdirectory sd, const std::string &filename)
{
	this->subdir = sd;
	return this->AddFile(filename, 0);
}

/**
 * Helper to extract a string for the tar header. We must assume that the tar
 * header contains garbage and is malicious. So, we cannot rely on the string
 * being properly terminated.
 * As such, do not use strlen to determine the actual length (explicitly or
 * implictly via the std::string constructor), but also do not create a string
 * of the buffer length as that makes the string contain essentially garbage.
 * @param buffer The buffer to read from.
 * @param buffer_length The length of the buffer to read from.
 * @return The string data.
 */
static std::string ExtractString(char *buffer, size_t buffer_length)
{
	size_t length = 0;
	for (; length < buffer_length && buffer[length] != '\0'; length++) {}
	return StrMakeValid(std::string_view(buffer, length));
}

bool TarScanner::AddFile(const std::string &filename, size_t, [[maybe_unused]] const std::string &tar_filename)
{
	/* No tar within tar. */
	assert(tar_filename.empty());

	/* The TAR-header, repeated for every file */
	struct TarHeader {
		char name[100];      ///< Name of the file
		char mode[8];
		char uid[8];
		char gid[8];
		char size[12];       ///< Size of the file, in ASCII octals
		char mtime[12];
		char chksum[8];
		char typeflag;
		char linkname[100];
		char magic[6];
		char version[2];
		char uname[32];
		char gname[32];
		char devmajor[8];
		char devminor[8];
		char prefix[155];    ///< Path of the file

		char unused[12];
	};

	/* Check if we already seen this file */
	TarList::iterator it = _tar_list[this->subdir].find(filename);
	if (it != _tar_list[this->subdir].end()) return false;

	FILE *f = fopen(filename.c_str(), "rb");
	/* Although the file has been found there can be
	 * a number of reasons we cannot open the file.
	 * Most common case is when we simply have not
	 * been given read access. */
	if (f == nullptr) return false;

	_tar_list[this->subdir][filename] = std::string{};

	TarLinkList links; ///< Temporary list to collect links

	TarHeader th;
	size_t num = 0, pos = 0;

	/* Make a char of 512 empty bytes */
	char empty[512];
	memset(&empty[0], 0, sizeof(empty));

	for (;;) { // Note: feof() always returns 'false' after 'fseek()'. Cool, isn't it?
		size_t num_bytes_read = fread(&th, 1, 512, f);
		if (num_bytes_read != 512) break;
		pos += num_bytes_read;

		/* Check if we have the new tar-format (ustar) or the old one (a lot of zeros after 'link' field) */
		if (strncmp(th.magic, "ustar", 5) != 0 && memcmp(&th.magic, &empty[0], 512 - offsetof(TarHeader, magic)) != 0) {
			/* If we have only zeros in the block, it can be an end-of-file indicator */
			if (memcmp(&th, &empty[0], 512) == 0) continue;

			Debug(misc, 0, "The file '{}' isn't a valid tar-file", filename);
			fclose(f);
			return false;
		}

		std::string name;

		/* The prefix contains the directory-name */
		if (th.prefix[0] != '\0') {
			name = ExtractString(th.prefix, lengthof(th.prefix));
			name += PATHSEP;
		}

		/* Copy the name of the file in a safe way at the end of 'name' */
		name += ExtractString(th.name, lengthof(th.name));

		/* The size of the file, for some strange reason, this is stored as a string in octals. */
		std::string size = ExtractString(th.size, lengthof(th.size));
		size_t skip = size.empty() ? 0 : std::stoul(size, nullptr, 8);

		switch (th.typeflag) {
			case '\0':
			case '0': { // regular file
				if (name.empty()) break;

				/* Store this entry in the list */
				TarFileListEntry entry;
				entry.tar_filename = filename;
				entry.size         = skip;
				entry.position     = pos;

				/* Convert to lowercase and our PATHSEPCHAR */
				SimplifyFileName(name);

				Debug(misc, 6, "Found file in tar: {} ({} bytes, {} offset)", name, skip, pos);
				if (_tar_filelist[this->subdir].insert(TarFileList::value_type(name, entry)).second) num++;

				break;
			}

			case '1': // hard links
			case '2': { // symbolic links
				/* Copy the destination of the link in a safe way at the end of 'linkname' */
				std::string link = ExtractString(th.linkname, lengthof(th.linkname));

				if (name.empty() || link.empty()) break;

				/* Convert to lowercase and our PATHSEPCHAR */
				SimplifyFileName(name);
				SimplifyFileName(link);

				/* Only allow relative links */
				if (link[0] == PATHSEPCHAR) {
					Debug(misc, 5, "Ignoring absolute link in tar: {} -> {}", name, link);
					break;
				}

				/* Process relative path.
				 * Note: The destination of links must not contain any directory-links. */
				std::string dest = (std::filesystem::path(name).remove_filename() /= link).lexically_normal().string();
				if (dest[0] == PATHSEPCHAR || StrStartsWith(dest, "..")) {
					Debug(misc, 5, "Ignoring link pointing outside of data directory: {} -> {}", name, link);
					break;
				}

				/* Store links in temporary list */
				Debug(misc, 6, "Found link in tar: {} -> {}", name, dest);
				links.insert(TarLinkList::value_type(name, dest));

				break;
			}

			case '5': // directory
				/* Convert to lowercase and our PATHSEPCHAR */
				SimplifyFileName(name);

				/* Store the first directory name we detect */
				Debug(misc, 6, "Found dir in tar: {}", name);
				if (_tar_list[this->subdir][filename].empty()) _tar_list[this->subdir][filename] = name;
				break;

			default:
				/* Ignore other types */
				break;
		}

		/* Skip to the next block.. */
		skip = Align(skip, 512);
		if (fseek(f, skip, SEEK_CUR) < 0) {
			Debug(misc, 0, "The file '{}' can't be read as a valid tar-file", filename);
			fclose(f);
			return false;
		}
		pos += skip;
	}

	Debug(misc, 4, "Found tar '{}' with {} new files", filename, num);
	fclose(f);

	/* Resolve file links and store directory links.
	 * We restrict usage of links to two cases:
	 *  1) Links to directories:
	 *      Both the source path and the destination path must NOT contain any further links.
	 *      When resolving files at most one directory link is resolved.
	 *  2) Links to files:
	 *      The destination path must NOT contain any links.
	 *      The source path may contain one directory link.
	 */
	for (auto &it : links) {
		TarAddLink(it.first, it.second, this->subdir);
	}

	return true;
}

/**
 * Extract the tar with the given filename in the directory
 * where the tar resides.
 * @param tar_filename the name of the tar to extract.
 * @param subdir The sub directory the tar is in.
 * @return false on failure.
 */
bool ExtractTar(const std::string &tar_filename, Subdirectory subdir)
{
	TarList::iterator it = _tar_list[subdir].find(tar_filename);
	/* We don't know the file. */
	if (it == _tar_list[subdir].end()) return false;

	const auto &dirname = (*it).second;

	/* The file doesn't have a sub directory! */
	if (dirname.empty()) {
		Debug(misc, 3, "Extracting {} failed; archive rejected, the contents must be in a sub directory", tar_filename);
		return false;
	}

	std::string filename = tar_filename;
	auto p = filename.find_last_of(PATHSEPCHAR);
	/* The file's path does not have a separator? */
	if (p == std::string::npos) return false;

	filename.replace(p + 1, std::string::npos, dirname);
	Debug(misc, 8, "Extracting {} to directory {}", tar_filename, filename);
	FioCreateDirectory(filename);

	for (auto &it2 : _tar_filelist[subdir]) {
		if (tar_filename != it2.second.tar_filename) continue;

		filename.replace(p + 1, std::string::npos, it2.first);

		Debug(misc, 9, "  extracting {}", filename);

		/* First open the file in the .tar. */
		size_t to_copy = 0;
		std::unique_ptr<FILE, FileDeleter> in(FioFOpenFileTar(it2.second, &to_copy));
		if (!in) {
			Debug(misc, 6, "Extracting {} failed; could not open {}", filename, tar_filename);
			return false;
		}

		/* Now open the 'output' file. */
		std::unique_ptr<FILE, FileDeleter> out(fopen(filename.c_str(), "wb"));
		if (!out) {
			Debug(misc, 6, "Extracting {} failed; could not open {}", filename, filename);
			return false;
		}

		/* Now read from the tar and write it into the file. */
		char buffer[4096];
		size_t read;
		for (; to_copy != 0; to_copy -= read) {
			read = fread(buffer, 1, std::min(to_copy, lengthof(buffer)), in.get());
			if (read <= 0 || fwrite(buffer, 1, read, out.get()) != read) break;
		}

		if (to_copy != 0) {
			Debug(misc, 6, "Extracting {} failed; still {} bytes to copy", filename, to_copy);
			return false;
		}
	}

	Debug(misc, 9, "  extraction successful");
	return true;
}

#if defined(_WIN32)
/**
 * Determine the base (personal dir and game data dir) paths
 * @param exe the path from the current path to the executable
 * @note defined in the OS related files (win32.cpp, unix.cpp etc)
 */
extern void DetermineBasePaths(const char *exe);
#else /* defined(_WIN32) */

/**
 * Changes the working directory to the path of the give executable.
 * For OSX application bundles '.app' is the required extension of the bundle,
 * so when we crop the path to there, when can remove the name of the bundle
 * in the same way we remove the name from the executable name.
 * @param exe the path to the executable
 */
static bool ChangeWorkingDirectoryToExecutable(const char *exe)
{
	std::string path = exe;

#ifdef WITH_COCOA
	for (size_t pos = path.find_first_of('.'); pos != std::string::npos; pos = path.find_first_of('.', pos + 1)) {
		if (StrEqualsIgnoreCase(path.substr(pos, 4), ".app")) {
			path.erase(pos);
			break;
		}
	}
#endif /* WITH_COCOA */

	size_t pos = path.find_last_of(PATHSEPCHAR);
	if (pos == std::string::npos) return false;

	path.erase(pos);

	if (chdir(path.c_str()) != 0) {
		Debug(misc, 0, "Directory with the binary does not exist?");
		return false;
	}

	return true;
}

/**
 * Whether we should scan the working directory.
 * It should not be scanned if it's the root or
 * the home directory as in both cases a big data
 * directory can cause huge amounts of unrelated
 * files scanned. Furthermore there are nearly no
 * use cases for the home/root directory to have
 * OpenTTD directories.
 * @return true if it should be scanned.
 */
bool DoScanWorkingDirectory()
{
	/* No working directory, so nothing to do. */
	if (_searchpaths[SP_WORKING_DIR].empty()) return false;

	/* Working directory is root, so do nothing. */
	if (_searchpaths[SP_WORKING_DIR] == PATHSEP) return false;

	/* No personal/home directory, so the working directory won't be that. */
	if (_searchpaths[SP_PERSONAL_DIR].empty()) return true;

	std::string tmp = _searchpaths[SP_WORKING_DIR] + PERSONAL_DIR;
	AppendPathSeparator(tmp);

	return _searchpaths[SP_PERSONAL_DIR] != tmp;
}

/**
 * Gets the home directory of the user.
 * May return an empty string in the unlikely scenario that the home directory cannot be found.
 * @return User's home directory
 */
static std::string GetHomeDir()
{
#ifdef __HAIKU__
	BPath path;
	find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	return std::string(path.Path());
#else
	const char *home_env = std::getenv("HOME"); // Stack var, shouldn't be freed
	if (home_env != nullptr) return std::string(home_env);

	const struct passwd *pw = getpwuid(getuid());
	if (pw != nullptr) return std::string(pw->pw_dir);
#endif
	return {};
}

/**
 * Determine the base (personal dir and game data dir) paths
 * @param exe the path to the executable
 */
void DetermineBasePaths(const char *exe)
{
	std::string tmp;
	const std::string homedir = GetHomeDir();
#ifdef USE_XDG
	const char *xdg_data_home = std::getenv("XDG_DATA_HOME");
	if (xdg_data_home != nullptr) {
		tmp = xdg_data_home;
		tmp += PATHSEP;
		tmp += PERSONAL_DIR[0] == '.' ? &PERSONAL_DIR[1] : PERSONAL_DIR;
		AppendPathSeparator(tmp);
		_searchpaths[SP_PERSONAL_DIR_XDG] = tmp;

		tmp += "content_download";
		AppendPathSeparator(tmp);
		_searchpaths[SP_AUTODOWNLOAD_PERSONAL_DIR_XDG] = tmp;
	} else if (!homedir.empty()) {
		tmp = homedir;
		tmp += PATHSEP ".local" PATHSEP "share" PATHSEP;
		tmp += PERSONAL_DIR[0] == '.' ? &PERSONAL_DIR[1] : PERSONAL_DIR;
		AppendPathSeparator(tmp);
		_searchpaths[SP_PERSONAL_DIR_XDG] = tmp;

		tmp += "content_download";
		AppendPathSeparator(tmp);
		_searchpaths[SP_AUTODOWNLOAD_PERSONAL_DIR_XDG] = tmp;
	} else {
		_searchpaths[SP_PERSONAL_DIR_XDG].clear();
		_searchpaths[SP_AUTODOWNLOAD_PERSONAL_DIR_XDG].clear();
	}
#endif

#if !defined(WITH_PERSONAL_DIR)
	_searchpaths[SP_PERSONAL_DIR].clear();
#else
	if (!homedir.empty()) {
		tmp = homedir;
		tmp += PATHSEP;
		tmp += PERSONAL_DIR;
		AppendPathSeparator(tmp);
		_searchpaths[SP_PERSONAL_DIR] = tmp;

		tmp += "content_download";
		AppendPathSeparator(tmp);
		_searchpaths[SP_AUTODOWNLOAD_PERSONAL_DIR] = tmp;
	} else {
		_searchpaths[SP_PERSONAL_DIR].clear();
		_searchpaths[SP_AUTODOWNLOAD_PERSONAL_DIR].clear();
	}
#endif

#if defined(WITH_SHARED_DIR)
	tmp = SHARED_DIR;
	AppendPathSeparator(tmp);
	_searchpaths[SP_SHARED_DIR] = tmp;
#else
	_searchpaths[SP_SHARED_DIR].clear();
#endif

	char cwd[MAX_PATH];
	if (getcwd(cwd, MAX_PATH) == nullptr) *cwd = '\0';

	if (_config_file.empty()) {
		/* Get the path to working directory of OpenTTD. */
		tmp = cwd;
		AppendPathSeparator(tmp);
		_searchpaths[SP_WORKING_DIR] = tmp;

		_do_scan_working_directory = DoScanWorkingDirectory();
	} else {
		/* Use the folder of the config file as working directory. */
		size_t end = _config_file.find_last_of(PATHSEPCHAR);
		if (end == std::string::npos) {
			/* _config_file is not in a folder, so use current directory. */
			tmp = cwd;
			AppendPathSeparator(tmp);
			_searchpaths[SP_WORKING_DIR] = tmp;
		} else {
			_searchpaths[SP_WORKING_DIR] = _config_file.substr(0, end + 1);
		}
	}

	/* Change the working directory to that one of the executable */
	if (ChangeWorkingDirectoryToExecutable(exe)) {
		char buf[MAX_PATH];
		if (getcwd(buf, lengthof(buf)) == nullptr) {
			tmp.clear();
		} else {
			tmp = buf;
		}
		AppendPathSeparator(tmp);
		_searchpaths[SP_BINARY_DIR] = tmp;
	} else {
		_searchpaths[SP_BINARY_DIR].clear();
	}

	if (cwd[0] != '\0') {
		/* Go back to the current working directory. */
		if (chdir(cwd) != 0) {
			Debug(misc, 0, "Failed to return to working directory!");
		}
	}

#if !defined(GLOBAL_DATA_DIR)
	_searchpaths[SP_INSTALLATION_DIR].clear();
#else
	tmp = GLOBAL_DATA_DIR;
	AppendPathSeparator(tmp);
	_searchpaths[SP_INSTALLATION_DIR] = tmp;
#endif
#ifdef WITH_COCOA
extern void CocoaSetApplicationBundleDir();
	CocoaSetApplicationBundleDir();
#else
	_searchpaths[SP_APPLICATION_BUNDLE_DIR].clear();
#endif
}
#endif /* defined(_WIN32) */

std::string _personal_dir;

/**
 * Acquire the base paths (personal dir and game data dir),
 * fill all other paths (save dir, autosave dir etc) and
 * make the save and scenario directories.
 * @param exe the path from the current path to the executable
 * @param only_local_path Whether we shouldn't fill searchpaths with global folders.
 */
void DeterminePaths(const char *exe, bool only_local_path)
{
	DetermineBasePaths(exe);
	FillValidSearchPaths(only_local_path);

#ifdef USE_XDG
	std::string config_home;
	const std::string homedir = GetHomeDir();
	const char *xdg_config_home = std::getenv("XDG_CONFIG_HOME");
	if (xdg_config_home != nullptr) {
		config_home = xdg_config_home;
		config_home += PATHSEP;
		config_home += PERSONAL_DIR[0] == '.' ? &PERSONAL_DIR[1] : PERSONAL_DIR;
	} else if (!homedir.empty()) {
		/* Defaults to ~/.config */
		config_home = homedir;
		config_home += PATHSEP ".config" PATHSEP;
		config_home += PERSONAL_DIR[0] == '.' ? &PERSONAL_DIR[1] : PERSONAL_DIR;
	}
	AppendPathSeparator(config_home);
#endif

	for (Searchpath sp : _valid_searchpaths) {
		if (sp == SP_WORKING_DIR && !_do_scan_working_directory) continue;
		Debug(misc, 3, "{} added as search path", _searchpaths[sp]);
	}

	std::string config_dir;
	if (!_config_file.empty()) {
		config_dir = _searchpaths[SP_WORKING_DIR];
	} else {
		std::string personal_dir = FioFindFullPath(BASE_DIR, "openttd.cfg");
		if (!personal_dir.empty()) {
			auto end = personal_dir.find_last_of(PATHSEPCHAR);
			if (end != std::string::npos) personal_dir.erase(end + 1);
			config_dir = personal_dir;
		} else {
#ifdef USE_XDG
			/* No previous configuration file found. Use the configuration folder from XDG. */
			config_dir = config_home;
#else
			static const Searchpath new_openttd_cfg_order[] = {
					SP_PERSONAL_DIR, SP_BINARY_DIR, SP_WORKING_DIR, SP_SHARED_DIR, SP_INSTALLATION_DIR
				};

			config_dir.clear();
			for (uint i = 0; i < lengthof(new_openttd_cfg_order); i++) {
				if (IsValidSearchPath(new_openttd_cfg_order[i])) {
					config_dir = _searchpaths[new_openttd_cfg_order[i]];
					break;
				}
			}
#endif
		}
		_config_file = config_dir + "openttd.cfg";
	}

	Debug(misc, 1, "{} found as config directory", config_dir);

	_highscore_file = config_dir + "hs.dat";
	extern std::string _hotkeys_file;
	_hotkeys_file = config_dir + "hotkeys.cfg";
	extern std::string _windows_file;
	_windows_file = config_dir + "windows.cfg";
	extern std::string _private_file;
	_private_file = config_dir + "private.cfg";
	extern std::string _secrets_file;
	_secrets_file = config_dir + "secrets.cfg";

#ifdef USE_XDG
	if (config_dir == config_home) {
		/* We are using the XDG configuration home for the config file,
		 * then store the rest in the XDG data home folder. */
		_personal_dir = _searchpaths[SP_PERSONAL_DIR_XDG];
		if (only_local_path) {
			/* In case of XDG and we only want local paths and we detected that
			 * the user either manually indicated the XDG path or didn't use
			 * "-c" option, we change the working-dir to the XDG personal-dir,
			 * as this is most likely what the user is expecting. */
			_searchpaths[SP_WORKING_DIR] = _searchpaths[SP_PERSONAL_DIR_XDG];
		}
	} else
#endif
	{
		_personal_dir = config_dir;
	}

	/* Make the necessary folders */
	FioCreateDirectory(config_dir);
#if defined(WITH_PERSONAL_DIR)
	FioCreateDirectory(_personal_dir);
#endif

	Debug(misc, 1, "{} found as personal directory", _personal_dir);

	static const Subdirectory default_subdirs[] = {
		SAVE_DIR, AUTOSAVE_DIR, SCENARIO_DIR, HEIGHTMAP_DIR, BASESET_DIR, NEWGRF_DIR, AI_DIR, AI_LIBRARY_DIR, GAME_DIR, GAME_LIBRARY_DIR, SCREENSHOT_DIR
	};

	for (uint i = 0; i < lengthof(default_subdirs); i++) {
		FioCreateDirectory(_personal_dir + _subdirs[default_subdirs[i]]);
	}

	/* If we have network we make a directory for the autodownloading of content */
	_searchpaths[SP_AUTODOWNLOAD_DIR] = _personal_dir + "content_download" PATHSEP;
	Debug(misc, 3, "{} added as search path", _searchpaths[SP_AUTODOWNLOAD_DIR]);
	FioCreateDirectory(_searchpaths[SP_AUTODOWNLOAD_DIR]);
	FillValidSearchPaths(only_local_path);

	/* Create the directory for each of the types of content */
	const Subdirectory dirs[] = { SCENARIO_DIR, HEIGHTMAP_DIR, BASESET_DIR, NEWGRF_DIR, AI_DIR, AI_LIBRARY_DIR, GAME_DIR, GAME_LIBRARY_DIR };
	for (uint i = 0; i < lengthof(dirs); i++) {
		FioCreateDirectory(FioGetDirectory(SP_AUTODOWNLOAD_DIR, dirs[i]));
	}

	extern std::string _log_file;
	_log_file = _personal_dir + "openttd.log";
}

/**
 * Sanitizes a filename, i.e. removes all illegal characters from it.
 * @param filename the filename
 */
void SanitizeFilename(std::string &filename)
{
	for (auto &c : filename) {
		switch (c) {
			/* The following characters are not allowed in filenames
			 * on at least one of the supported operating systems: */
			case ':': case '\\': case '*': case '?': case '/':
			case '<': case '>': case '|': case '"':
				c = '_';
				break;
		}
	}
}

/**
 * Load a file into memory.
 * @param filename Name of the file to load.
 * @param[out] lenp Length of loaded data.
 * @param maxsize Maximum size to load.
 * @return Pointer to new memory containing the loaded data, or \c nullptr if loading failed.
 * @note If \a maxsize less than the length of the file, loading fails.
 */
std::unique_ptr<char[]> ReadFileToMem(const std::string &filename, size_t &lenp, size_t maxsize)
{
	FILE *in = fopen(filename.c_str(), "rb");
	if (in == nullptr) return nullptr;

	FileCloser fc(in);

	fseek(in, 0, SEEK_END);
	size_t len = ftell(in);
	fseek(in, 0, SEEK_SET);
	if (len > maxsize) return nullptr;

	std::unique_ptr<char[]> mem = std::make_unique<char[]>(len + 1);

	mem.get()[len] = 0;
	if (fread(mem.get(), len, 1, in) != 1) return nullptr;

	lenp = len;
	return mem;
}

/**
 * Helper to see whether a given filename matches the extension.
 * @param extension The extension to look for.
 * @param filename  The filename to look in for the extension.
 * @return True iff the extension is nullptr, or the filename ends with it.
 */
static bool MatchesExtension(const char *extension, const char *filename)
{
	if (extension == nullptr) return true;

	const char *ext = strrchr(filename, extension[0]);
	return ext != nullptr && StrEqualsIgnoreCase(ext, extension);
}

/**
 * Scan a single directory (and recursively its children) and add
 * any graphics sets that are found.
 * @param fs              the file scanner to add the files to
 * @param extension       the extension of files to search for.
 * @param path            full path we're currently at
 * @param basepath_length from where in the path are we 'based' on the search path
 * @param recursive       whether to recursively search the sub directories
 */
static uint ScanPath(FileScanner *fs, const char *extension, const char *path, size_t basepath_length, bool recursive)
{
	uint num = 0;
	struct stat sb;
	struct dirent *dirent;
	DIR *dir;

	if (path == nullptr || (dir = ttd_opendir(path)) == nullptr) return 0;

	while ((dirent = readdir(dir)) != nullptr) {
		std::string d_name = FS2OTTD(dirent->d_name);

		if (!FiosIsValidFile(path, dirent, &sb)) continue;

		std::string filename(path);
		filename += d_name;

		if (S_ISDIR(sb.st_mode)) {
			/* Directory */
			if (!recursive) continue;
			if (d_name == "." || d_name == "..") continue;
			AppendPathSeparator(filename);
			num += ScanPath(fs, extension, filename.c_str(), basepath_length, recursive);
		} else if (S_ISREG(sb.st_mode)) {
			/* File */
			if (MatchesExtension(extension, filename.c_str()) && fs->AddFile(filename, basepath_length, {})) num++;
		}
	}

	closedir(dir);

	return num;
}

/**
 * Scan the given tar and add graphics sets when it finds one.
 * @param fs        the file scanner to scan for
 * @param extension the extension of files to search for.
 * @param tar       the tar to search in.
 */
static uint ScanTar(FileScanner *fs, const char *extension, const TarFileList::value_type &tar)
{
	uint num = 0;
	const auto &filename = tar.first;

	if (MatchesExtension(extension, filename.c_str()) && fs->AddFile(filename, 0, tar.second.tar_filename)) num++;

	return num;
}

/**
 * Scan for files with the given extension in the given search path.
 * @param extension the extension of files to search for.
 * @param sd        the sub directory to search in.
 * @param tars      whether to search in the tars too.
 * @param recursive whether to search recursively
 * @return the number of found files, i.e. the number of times that
 *         AddFile returned true.
 */
uint FileScanner::Scan(const char *extension, Subdirectory sd, bool tars, bool recursive)
{
	this->subdir = sd;

	uint num = 0;

	for (Searchpath sp : _valid_searchpaths) {
		/* Don't search in the working directory */
		if (sp == SP_WORKING_DIR && !_do_scan_working_directory) continue;

		std::string path = FioGetDirectory(sp, sd);
		num += ScanPath(this, extension, path.c_str(), path.size(), recursive);
	}

	if (tars && sd != NO_DIRECTORY) {
		for (const auto &tar : _tar_filelist[sd]) {
			num += ScanTar(this, extension, tar);
		}
	}

	switch (sd) {
		case BASESET_DIR:
			num += this->Scan(extension, OLD_GM_DIR, tars, recursive);
			FALLTHROUGH;
		case NEWGRF_DIR:
			num += this->Scan(extension, OLD_DATA_DIR, tars, recursive);
			break;

		default: break;
	}

	return num;
}

/**
 * Scan for files with the given extension in the given search path.
 * @param extension the extension of files to search for.
 * @param directory the sub directory to search in.
 * @param recursive whether to search recursively
 * @return the number of found files, i.e. the number of times that
 *         AddFile returned true.
 */
uint FileScanner::Scan(const char *extension, const std::string &directory, bool recursive)
{
	std::string path(directory);
	AppendPathSeparator(path);
	return ScanPath(this, extension, path.c_str(), path.size(), recursive);
}
