/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fileio.cpp Standard In/Out file operations */

#include "stdafx.h"
#include "fileio_func.h"
#include "debug.h"
#include "fios.h"
#include "string_func.h"
#include "tar_type.h"
#ifdef WIN32
#include <windows.h>
#elif defined(__HAIKU__)
#include <Path.h>
#include <storage/FindDirectory.h>
#else
#if defined(OPENBSD) || defined(DOS)
#include <unistd.h>
#endif
#include <pwd.h>
#endif
#include <sys/stat.h>
#include <algorithm>

/*************************************************/
/* FILE IO ROUTINES ******************************/
/*************************************************/

/** Size of the #Fio data buffer. */
#define FIO_BUFFER_SIZE 512

/** Structure for keeping several open files with just one data buffer. */
struct Fio {
	byte *buffer, *buffer_end;             ///< position pointer in local buffer and last valid byte of buffer
	size_t pos;                            ///< current (system) position in file
	FILE *cur_fh;                          ///< current file handle
	const char *filename;                  ///< current filename
	FILE *handles[MAX_FILE_SLOTS];         ///< array of file handles we can have open
	byte buffer_start[FIO_BUFFER_SIZE];    ///< local buffer when read from file
	const char *filenames[MAX_FILE_SLOTS]; ///< array of filenames we (should) have open
	char *shortnames[MAX_FILE_SLOTS];      ///< array of short names for spriteloader's use
#if defined(LIMITED_FDS)
	uint open_handles;                     ///< current amount of open handles
	uint usage_count[MAX_FILE_SLOTS];      ///< count how many times this file has been opened
#endif /* LIMITED_FDS */
};

static Fio _fio; ///< #Fio instance.

/** Whether the working directory should be scanned. */
static bool _do_scan_working_directory = true;

extern char *_config_file;
extern char *_highscore_file;

/** Get current position in file. */
size_t FioGetPos()
{
	return _fio.pos + (_fio.buffer - _fio.buffer_end);
}

/**
 * Get the filename associated with a slot.
 * @param slot Index of queried file.
 * @return Name of the file.
 */
const char *FioGetFilename(uint8 slot)
{
	return _fio.shortnames[slot];
}

void FioSeekTo(size_t pos, int mode)
{
	if (mode == SEEK_CUR) pos += FioGetPos();
	_fio.buffer = _fio.buffer_end = _fio.buffer_start + FIO_BUFFER_SIZE;
	_fio.pos = pos;
	fseek(_fio.cur_fh, _fio.pos, SEEK_SET);
}

#if defined(LIMITED_FDS)
static void FioRestoreFile(int slot)
{
	/* Do we still have the file open, or should we reopen it? */
	if (_fio.handles[slot] == NULL) {
		DEBUG(misc, 6, "Restoring file '%s' in slot '%d' from disk", _fio.filenames[slot], slot);
		FioOpenFile(slot, _fio.filenames[slot]);
	}
	_fio.usage_count[slot]++;
}
#endif /* LIMITED_FDS */

/* Seek to a file and a position */
void FioSeekToFile(uint8 slot, size_t pos)
{
	FILE *f;
#if defined(LIMITED_FDS)
	/* Make sure we have this file open */
	FioRestoreFile(slot);
#endif /* LIMITED_FDS */
	f = _fio.handles[slot];
	assert(f != NULL);
	_fio.cur_fh = f;
	_fio.filename = _fio.filenames[slot];
	FioSeekTo(pos, SEEK_SET);
}

byte FioReadByte()
{
	if (_fio.buffer == _fio.buffer_end) {
		_fio.buffer = _fio.buffer_start;
		size_t size = fread(_fio.buffer, 1, FIO_BUFFER_SIZE, _fio.cur_fh);
		_fio.pos += size;
		_fio.buffer_end = _fio.buffer_start + size;

		if (size == 0) return 0;
	}
	return *_fio.buffer++;
}

void FioSkipBytes(int n)
{
	for (;;) {
		int m = min(_fio.buffer_end - _fio.buffer, n);
		_fio.buffer += m;
		n -= m;
		if (n == 0) break;
		FioReadByte();
		n--;
	}
}

uint16 FioReadWord()
{
	byte b = FioReadByte();
	return (FioReadByte() << 8) | b;
}

uint32 FioReadDword()
{
	uint b = FioReadWord();
	return (FioReadWord() << 16) | b;
}

void FioReadBlock(void *ptr, size_t size)
{
	FioSeekTo(FioGetPos(), SEEK_SET);
	_fio.pos += fread(ptr, 1, size, _fio.cur_fh);
}

static inline void FioCloseFile(int slot)
{
	if (_fio.handles[slot] != NULL) {
		fclose(_fio.handles[slot]);

		free(_fio.shortnames[slot]);
		_fio.shortnames[slot] = NULL;

		_fio.handles[slot] = NULL;
#if defined(LIMITED_FDS)
		_fio.open_handles--;
#endif /* LIMITED_FDS */
	}
}

void FioCloseAll()
{
	for (int i = 0; i != lengthof(_fio.handles); i++) {
		FioCloseFile(i);
	}
}

#if defined(LIMITED_FDS)
static void FioFreeHandle()
{
	/* If we are about to open a file that will exceed the limit, close a file */
	if (_fio.open_handles + 1 == LIMITED_FDS) {
		uint i, count;
		int slot;

		count = UINT_MAX;
		slot = -1;
		/* Find the file that is used the least */
		for (i = 0; i < lengthof(_fio.handles); i++) {
			if (_fio.handles[i] != NULL && _fio.usage_count[i] < count) {
				count = _fio.usage_count[i];
				slot  = i;
			}
		}
		assert(slot != -1);
		DEBUG(misc, 6, "Closing filehandler '%s' in slot '%d' because of fd-limit", _fio.filenames[slot], slot);
		FioCloseFile(slot);
	}
}
#endif /* LIMITED_FDS */

void FioOpenFile(int slot, const char *filename)
{
	FILE *f;

#if defined(LIMITED_FDS)
	FioFreeHandle();
#endif /* LIMITED_FDS */
	f = FioFOpenFile(filename);
	if (f == NULL) usererror("Cannot open file '%s'", filename);
	uint32 pos = ftell(f);

	FioCloseFile(slot); // if file was opened before, close it
	_fio.handles[slot] = f;
	_fio.filenames[slot] = filename;

	/* Store the filename without path and extension */
	const char *t = strrchr(filename, PATHSEPCHAR);
	_fio.shortnames[slot] = strdup(t == NULL ? filename : t);
	char *t2 = strrchr(_fio.shortnames[slot], '.');
	if (t2 != NULL) *t2 = '\0';
	strtolower(_fio.shortnames[slot]);

#if defined(LIMITED_FDS)
	_fio.usage_count[slot] = 0;
	_fio.open_handles++;
#endif /* LIMITED_FDS */
	FioSeekToFile(slot, pos);
}

static const char * const _subdirs[NUM_SUBDIRS] = {
	"",
	"save" PATHSEP,
	"save" PATHSEP "autosave" PATHSEP,
	"scenario" PATHSEP,
	"scenario" PATHSEP "heightmap" PATHSEP,
	"gm" PATHSEP,
	"data" PATHSEP,
	"lang" PATHSEP,
	"ai" PATHSEP,
	"ai" PATHSEP "library" PATHSEP,
};

const char *_searchpaths[NUM_SEARCHPATHS];
TarList _tar_list;
TarFileList _tar_filelist;

typedef std::map<std::string, std::string> TarLinkList;
static TarLinkList _tar_linklist; ///< List of directory links

/**
 * Check whether the given file exists
 * @param filename the file to try for existance
 * @param subdir the subdirectory to look in
 * @return true if and only if the file can be opened
 */
bool FioCheckFileExists(const char *filename, Subdirectory subdir)
{
	FILE *f = FioFOpenFile(filename, "rb", subdir);
	if (f == NULL) return false;

	FioFCloseFile(f);
	return true;
}

/**
 * Close a file in a safe way.
 */
void FioFCloseFile(FILE *f)
{
	fclose(f);
}

char *FioGetFullPath(char *buf, size_t buflen, Searchpath sp, Subdirectory subdir, const char *filename)
{
	assert(subdir < NUM_SUBDIRS);
	assert(sp < NUM_SEARCHPATHS);

	snprintf(buf, buflen, "%s%s%s", _searchpaths[sp], _subdirs[subdir], filename);
	return buf;
}

char *FioFindFullPath(char *buf, size_t buflen, Subdirectory subdir, const char *filename)
{
	Searchpath sp;
	assert(subdir < NUM_SUBDIRS);

	FOR_ALL_SEARCHPATHS(sp) {
		FioGetFullPath(buf, buflen, sp, subdir, filename);
		if (FileExists(buf)) break;
#if !defined(WIN32)
		/* Be, as opening files, aware that sometimes the filename
		 * might be in uppercase when it is in lowercase on the
		 * disk. Ofcourse Windows doesn't care about casing. */
		strtolower(buf + strlen(_searchpaths[sp]) - 1);
		if (FileExists(buf)) break;
#endif
	}

	return buf;
}

char *FioAppendDirectory(char *buf, size_t buflen, Searchpath sp, Subdirectory subdir)
{
	assert(subdir < NUM_SUBDIRS);
	assert(sp < NUM_SEARCHPATHS);

	snprintf(buf, buflen, "%s%s", _searchpaths[sp], _subdirs[subdir]);
	return buf;
}

char *FioGetDirectory(char *buf, size_t buflen, Subdirectory subdir)
{
	Searchpath sp;

	/* Find and return the first valid directory */
	FOR_ALL_SEARCHPATHS(sp) {
		char *ret = FioAppendDirectory(buf, buflen, sp, subdir);
		if (FileExists(buf)) return ret;
	}

	/* Could not find the directory, fall back to a base path */
	ttd_strlcpy(buf, _personal_dir, buflen);

	return buf;
}

static FILE *FioFOpenFileSp(const char *filename, const char *mode, Searchpath sp, Subdirectory subdir, size_t *filesize)
{
#if defined(WIN32) && defined(UNICODE)
	/* fopen is implemented as a define with ellipses for
	 * Unicode support (prepend an L). As we are not sending
	 * a string, but a variable, it 'renames' the variable,
	 * so make that variable to makes it compile happily */
	wchar_t Lmode[5];
	MultiByteToWideChar(CP_ACP, 0, mode, -1, Lmode, lengthof(Lmode));
#endif
	FILE *f = NULL;
	char buf[MAX_PATH];

	if (subdir == NO_DIRECTORY) {
		strecpy(buf, filename, lastof(buf));
	} else {
		snprintf(buf, lengthof(buf), "%s%s%s", _searchpaths[sp], _subdirs[subdir], filename);
	}

#if defined(WIN32)
	if (mode[0] == 'r' && GetFileAttributes(OTTD2FS(buf)) == INVALID_FILE_ATTRIBUTES) return NULL;
#endif

	f = fopen(buf, mode);
#if !defined(WIN32)
	if (f == NULL) {
		strtolower(buf + ((subdir == NO_DIRECTORY) ? 0 : strlen(_searchpaths[sp]) - 1));
		f = fopen(buf, mode);
	}
#endif
	if (f != NULL && filesize != NULL) {
		/* Find the size of the file */
		fseek(f, 0, SEEK_END);
		*filesize = ftell(f);
		fseek(f, 0, SEEK_SET);
	}
	return f;
}

FILE *FioFOpenFileTar(TarFileListEntry *entry, size_t *filesize)
{
	FILE *f = fopen(entry->tar_filename, "rb");
	if (f == NULL) return f;

	fseek(f, entry->position, SEEK_SET);
	if (filesize != NULL) *filesize = entry->size;
	return f;
}

/** Opens OpenTTD files somewhere in a personal or global directory */
FILE *FioFOpenFile(const char *filename, const char *mode, Subdirectory subdir, size_t *filesize)
{
	FILE *f = NULL;
	Searchpath sp;

	assert(subdir < NUM_SUBDIRS || subdir == NO_DIRECTORY);

	FOR_ALL_SEARCHPATHS(sp) {
		f = FioFOpenFileSp(filename, mode, sp, subdir, filesize);
		if (f != NULL || subdir == NO_DIRECTORY) break;
	}

	/* We can only use .tar in case of data-dir, and read-mode */
	if (f == NULL && mode[0] == 'r') {
		static const uint MAX_RESOLVED_LENGTH = 2 * (100 + 100 + 155) + 1; // Enough space to hold two filenames plus link. See 'TarHeader'.
		char resolved_name[MAX_RESOLVED_LENGTH];

		/* Filenames in tars are always forced to be lowercase */
		strecpy(resolved_name, filename, lastof(resolved_name));
		strtolower(resolved_name);

		size_t resolved_len = strlen(resolved_name);

		/* Resolve ONE directory link */
		for (TarLinkList::iterator link = _tar_linklist.begin(); link != _tar_linklist.end(); link++) {
			const std::string &src = link->first;
			size_t len = src.length();
			if (resolved_len >= len && resolved_name[len - 1] == PATHSEPCHAR && strncmp(src.c_str(), resolved_name, len) == 0) {
				/* Apply link */
				char resolved_name2[MAX_RESOLVED_LENGTH];
				const std::string &dest = link->second;
				strecpy(resolved_name2, &(resolved_name[len]), lastof(resolved_name2));
				strecpy(resolved_name, dest.c_str(), lastof(resolved_name));
				strecpy(&(resolved_name[dest.length()]), resolved_name2, lastof(resolved_name));
				break; // Only resolve one level
			}
		}

		TarFileList::iterator it = _tar_filelist.find(resolved_name);
		if (it != _tar_filelist.end()) {
			f = FioFOpenFileTar(&((*it).second), filesize);
		}
	}

	/* Sometimes a full path is given. To support
	 * the 'subdirectory' must be 'removed'. */
	if (f == NULL && subdir != NO_DIRECTORY) {
		f = FioFOpenFile(filename, mode, NO_DIRECTORY, filesize);
	}

	return f;
}

/**
 * Create a directory with the given name
 * @param name the new name of the directory
 */
static void FioCreateDirectory(const char *name)
{
#if defined(WIN32) || defined(WINCE)
	CreateDirectory(OTTD2FS(name), NULL);
#elif defined(OS2) && !defined(__INNOTEK_LIBC__)
	mkdir(OTTD2FS(name));
#elif defined(__MORPHOS__) || defined(__AMIGAOS__)
	char buf[MAX_PATH];
	ttd_strlcpy(buf, name, MAX_PATH);

	size_t len = strlen(name) - 1;
	if (buf[len] == '/') {
		buf[len] = '\0'; // Kill pathsep, so mkdir() will not fail
	}

	mkdir(OTTD2FS(buf), 0755);
#else
	mkdir(OTTD2FS(name), 0755);
#endif
}

/**
 * Appends, if necessary, the path separator character to the end of the string.
 * It does not add the path separator to zero-sized strings.
 * @param buf    string to append the separator to
 * @param buflen the length of the buf
 * @return true iff the operation succeeded
 */
bool AppendPathSeparator(char *buf, size_t buflen)
{
	size_t s = strlen(buf);

	/* Length of string + path separator + '\0' */
	if (s != 0 && buf[s - 1] != PATHSEPCHAR) {
		if (s + 2 >= buflen) return false;

		buf[s]     = PATHSEPCHAR;
		buf[s + 1] = '\0';
	}

	return true;
}

/**
 * Allocates and files a variable with the full path
 * based on the given directory.
 * @param dir the directory to base the path on
 * @return the malloced full path
 */
char *BuildWithFullPath(const char *dir)
{
	char *dest = MallocT<char>(MAX_PATH);
	ttd_strlcpy(dest, dir, MAX_PATH);

	/* Check if absolute or relative path */
	const char *s = strchr(dest, PATHSEPCHAR);

	/* Add absolute path */
	if (s == NULL || dest != s) {
		if (getcwd(dest, MAX_PATH) == NULL) *dest = '\0';
		AppendPathSeparator(dest, MAX_PATH);
		ttd_strlcat(dest, dir, MAX_PATH);
	}
	AppendPathSeparator(dest, MAX_PATH);

	return dest;
}

const char *FioTarFirstDir(const char *tarname)
{
	TarList::iterator it = _tar_list.find(tarname);
	if (it == _tar_list.end()) return NULL;
	return (*it).second.dirname;
}

static void TarAddLink(const std::string &srcParam, const std::string &destParam)
{
	std::string src = srcParam;
	std::string dest = destParam;
	/* Tar internals assume lowercase */
	std::transform(src.begin(), src.end(), src.begin(), tolower);
	std::transform(dest.begin(), dest.end(), dest.begin(), tolower);

	TarFileList::iterator dest_file = _tar_filelist.find(dest);
	if (dest_file != _tar_filelist.end()) {
		/* Link to file. Process the link like the destination file. */
		_tar_filelist.insert(TarFileList::value_type(src, dest_file->second));
	} else {
		/* Destination file not found. Assume 'link to directory'
		 * Append PATHSEPCHAR to 'src' and 'dest' if needed */
		const std::string src_path = ((*src.rbegin() == PATHSEPCHAR) ? src : src + PATHSEPCHAR);
		const std::string dst_path = (dest.length() == 0 ? "" : ((*dest.rbegin() == PATHSEPCHAR) ? dest : dest + PATHSEPCHAR));
		_tar_linklist.insert(TarLinkList::value_type(src_path, dst_path));
	}
}

void FioTarAddLink(const char *src, const char *dest)
{
	TarAddLink(src, dest);
}

/**
 * Simplify filenames from tars.
 * Replace '/' by PATHSEPCHAR, and force 'name' to lowercase.
 * @param name Filename to process.
 */
static void SimplifyFileName(char *name)
{
	/* Force lowercase */
	strtolower(name);

	/* Tar-files always have '/' path-seperator, but we want our PATHSEPCHAR */
#if (PATHSEPCHAR != '/')
	for (char *n = name; *n != '\0'; n++) if (*n == '/') *n = PATHSEPCHAR;
#endif
}

/* static */ uint TarScanner::DoScan()
{
	_tar_filelist.clear();
	_tar_list.clear();

	DEBUG(misc, 1, "Scanning for tars");
	TarScanner fs;
	uint num = fs.Scan(".tar", DATA_DIR, false);
	num += fs.Scan(".tar", AI_DIR, false);
	num += fs.Scan(".tar", AI_LIBRARY_DIR, false);
	num += fs.Scan(".tar", SCENARIO_DIR, false);
	DEBUG(misc, 1, "Scan complete, found %d files", num);
	return num;
}

bool TarScanner::AddFile(const char *filename, size_t basepath_length)
{
	/* The TAR-header, repeated for every file */
	typedef struct TarHeader {
		char name[100];      ///< Name of the file
		char mode[8];
		char uid[8];
		char gid[8];
		char size[12];       ///< Size of the file, in ASCII
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
	} TarHeader;

	/* Check if we already seen this file */
	TarList::iterator it = _tar_list.find(filename);
	if (it != _tar_list.end()) return false;

	FILE *f = fopen(filename, "rb");
	/* Although the file has been found there can be
	 * a number of reasons we cannot open the file.
	 * Most common case is when we simply have not
	 * been given read access. */
	if (f == NULL) return false;

	const char *dupped_filename = strdup(filename);
	_tar_list[filename].filename = dupped_filename;
	_tar_list[filename].dirname = NULL;

	TarLinkList links; ///< Temporary list to collect links

	TarHeader th;
	char buf[sizeof(th.name) + 1], *end;
	char name[sizeof(th.prefix) + 1 + sizeof(th.name) + 1];
	char link[sizeof(th.linkname) + 1];
	char dest[sizeof(th.prefix) + 1 + sizeof(th.name) + 1 + 1 + sizeof(th.linkname) + 1];
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

			DEBUG(misc, 0, "The file '%s' isn't a valid tar-file", filename);
			return false;
		}

		name[0] = '\0';
		size_t len = 0;

		/* The prefix contains the directory-name */
		if (th.prefix[0] != '\0') {
			memcpy(name, th.prefix, sizeof(th.prefix));
			name[sizeof(th.prefix)] = '\0';
			len = strlen(name);
			name[len] = PATHSEPCHAR;
			len++;
		}

		/* Copy the name of the file in a safe way at the end of 'name' */
		memcpy(&name[len], th.name, sizeof(th.name));
		name[len + sizeof(th.name)] = '\0';

		/* Calculate the size of the file.. for some strange reason this is stored as a string */
		memcpy(buf, th.size, sizeof(th.size));
		buf[sizeof(th.size)] = '\0';
		size_t skip = strtoul(buf, &end, 8);

		switch (th.typeflag) {
			case '\0':
			case '0': { // regular file
				/* Ignore empty files */
				if (skip == 0) break;

				if (strlen(name) == 0) break;

				/* Store this entry in the list */
				TarFileListEntry entry;
				entry.tar_filename = dupped_filename;
				entry.size         = skip;
				entry.position     = pos;

				/* Convert to lowercase and our PATHSEPCHAR */
				SimplifyFileName(name);

				DEBUG(misc, 6, "Found file in tar: %s (" PRINTF_SIZE " bytes, " PRINTF_SIZE " offset)", name, skip, pos);
				if (_tar_filelist.insert(TarFileList::value_type(name, entry)).second) num++;

				break;
			}

			case '1': // hard links
			case '2': { // symbolic links
				/* Copy the destination of the link in a safe way at the end of 'linkname' */
				memcpy(link, th.linkname, sizeof(th.linkname));
				link[sizeof(th.linkname)] = '\0';

				if (strlen(name) == 0 || strlen(link) == 0) break;

				/* Convert to lowercase and our PATHSEPCHAR */
				SimplifyFileName(name);
				SimplifyFileName(link);

				/* Only allow relative links */
				if (link[0] == PATHSEPCHAR) {
					DEBUG(misc, 1, "Ignoring absolute link in tar: %s -> %s", name, link);
					break;
				}

				/* Process relative path.
				 * Note: The destination of links must not contain any directory-links. */
				strecpy(dest, name, lastof(dest));
				char *destpos = strrchr(dest, PATHSEPCHAR);
				if (destpos == NULL) destpos = dest;
				*destpos = '\0';

				char *pos = link;
				while (*pos != '\0') {
					char *next = strchr(link, PATHSEPCHAR);
					if (next == NULL) next = pos + strlen(pos);

					/* Skip '.' (current dir) */
					if (next != pos + 1 || pos[0] != '.') {
						if (next == pos + 2 && pos[0] == '.' && pos[1] == '.') {
							/* level up */
							if (dest[0] == '\0') {
								DEBUG(misc, 1, "Ignoring link pointing outside of data directory: %s -> %s", name, link);
								break;
							}

							/* Truncate 'dest' after last PATHSEPCHAR.
							 * This assumes that the truncated part is a real directory and not a link. */
							destpos = strrchr(dest, PATHSEPCHAR);
							if (destpos == NULL) destpos = dest;
						} else {
							/* Append at end of 'dest' */
							if (destpos != dest) *(destpos++) = PATHSEPCHAR;
							strncpy(destpos, pos, next - pos); // Safe as we do '\0'-termination ourselves
							destpos += next - pos;
						}
						*destpos = '\0';
					}

					pos = next;
				}

				/* Store links in temporary list */
				DEBUG(misc, 6, "Found link in tar: %s -> %s", name, dest);
				links.insert(TarLinkList::value_type(name, dest));

				break;
			}

			case '5': // directory
				/* Convert to lowercase and our PATHSEPCHAR */
				SimplifyFileName(name);

				/* Store the first directory name we detect */
				DEBUG(misc, 6, "Found dir in tar: %s", name);
				if (_tar_list[filename].dirname == NULL) _tar_list[filename].dirname = strdup(name);
				break;

			default:
				/* Ignore other types */
				break;
		}

		/* Skip to the next block.. */
		skip = Align(skip, 512);
		fseek(f, skip, SEEK_CUR);
		pos += skip;
	}

	DEBUG(misc, 1, "Found tar '%s' with " PRINTF_SIZE " new files", filename, num);
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
	for (TarLinkList::iterator link = links.begin(); link != links.end(); link++) {
		const std::string &src = link->first;
		const std::string &dest = link->second;
		TarAddLink(src, dest);
	}

	return true;
}

/**
 * Extract the tar with the given filename in the directory
 * where the tar resides.
 * @param tar_filename the name of the tar to extract.
 * @return false on failure.
 */
bool ExtractTar(const char *tar_filename)
{
	TarList::iterator it = _tar_list.find(tar_filename);
	/* We don't know the file. */
	if (it == _tar_list.end()) return false;

	const char *dirname = (*it).second.dirname;

	/* The file doesn't have a sub directory! */
	if (dirname == NULL) return false;

	char filename[MAX_PATH];
	strecpy(filename, tar_filename, lastof(filename));
	char *p = strrchr(filename, PATHSEPCHAR);
	/* The file's path does not have a separator? */
	if (p == NULL) return false;

	p++;
	strecpy(p, dirname, lastof(filename));
	DEBUG(misc, 8, "Extracting %s to directory %s", tar_filename, filename);
	FioCreateDirectory(filename);

	for (TarFileList::iterator it2 = _tar_filelist.begin(); it2 != _tar_filelist.end(); it2++) {
		if (strcmp((*it2).second.tar_filename, tar_filename) != 0) continue;

		strecpy(p, (*it2).first.c_str(), lastof(filename));

		DEBUG(misc, 9, "  extracting %s", filename);

		/* First open the file in the .tar. */
		size_t to_copy = 0;
		FILE *in = FioFOpenFileTar(&(*it2).second, &to_copy);
		if (in == NULL) {
			DEBUG(misc, 6, "Extracting %s failed; could not open %s", filename, tar_filename);
			return false;
		}

		/* Now open the 'output' file. */
		FILE *out = fopen(filename, "wb");
		if (out == NULL) {
			DEBUG(misc, 6, "Extracting %s failed; could not open %s", filename, filename);
			fclose(in);
			return false;
		}

		/* Now read from the tar and write it into the file. */
		char buffer[4096];
		size_t read;
		for (; to_copy != 0; to_copy -= read) {
			read = fread(buffer, 1, min(to_copy, lengthof(buffer)), in);
			if (read <= 0 || fwrite(buffer, 1, read, out) != read) break;
		}

		/* Close everything up. */
		fclose(in);
		fclose(out);

		if (to_copy != 0) {
			DEBUG(misc, 6, "Extracting %s failed; still %i bytes to copy", filename, (int)to_copy);
			return false;
		}
	}

	DEBUG(misc, 9, "  extraction successful");
	return true;
}

#if defined(WIN32) || defined(WINCE)
/**
 * Determine the base (personal dir and game data dir) paths
 * @param exe the path from the current path to the executable
 * @note defined in the OS related files (os2.cpp, win32.cpp, unix.cpp etc)
 */
extern void DetermineBasePaths(const char *exe);
#else /* defined(WIN32) || defined(WINCE) */

/**
 * Changes the working directory to the path of the give executable.
 * For OSX application bundles '.app' is the required extension of the bundle,
 * so when we crop the path to there, when can remove the name of the bundle
 * in the same way we remove the name from the executable name.
 * @param exe the path to the executable
 */
static bool ChangeWorkingDirectoryToExecutable(const char *exe)
{
	bool success = false;
#ifdef WITH_COCOA
	char *app_bundle = strchr(exe, '.');
	while (app_bundle != NULL && strncasecmp(app_bundle, ".app", 4) != 0) app_bundle = strchr(&app_bundle[1], '.');

	if (app_bundle != NULL) app_bundle[0] = '\0';
#endif /* WITH_COCOA */
	char *s = const_cast<char *>(strrchr(exe, PATHSEPCHAR));
	if (s != NULL) {
		*s = '\0';
#if defined(__DJGPP__)
		/* If we want to go to the root, we can't use cd C:, but we must use '/' */
		if (s[-1] == ':') chdir("/");
#endif
		if (chdir(exe) != 0) {
			DEBUG(misc, 0, "Directory with the binary does not exist?");
		} else {
			success = true;
		}
		*s = PATHSEPCHAR;
	}
#ifdef WITH_COCOA
	if (app_bundle != NULL) app_bundle[0] = '.';
#endif /* WITH_COCOA */
	return success;
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
	if (_searchpaths[SP_WORKING_DIR] == NULL) return false;

	/* Working directory is root, so do nothing. */
	if (strcmp(_searchpaths[SP_WORKING_DIR], PATHSEP) == 0) return false;

	/* No personal/home directory, so the working directory won't be that. */
	if (_searchpaths[SP_PERSONAL_DIR] == NULL) return true;

	char tmp[MAX_PATH];
	snprintf(tmp, lengthof(tmp), "%s%s", _searchpaths[SP_WORKING_DIR], PERSONAL_DIR);
	AppendPathSeparator(tmp, MAX_PATH);
	return strcmp(tmp, _searchpaths[SP_PERSONAL_DIR]) != 0;
}

/**
 * Determine the base (personal dir and game data dir) paths
 * @param exe the path to the executable
 */
void DetermineBasePaths(const char *exe)
{
	char tmp[MAX_PATH];
#if defined(__MORPHOS__) || defined(__AMIGA__) || defined(DOS) || defined(OS2) || !defined(WITH_PERSONAL_DIR)
	_searchpaths[SP_PERSONAL_DIR] = NULL;
#else
#ifdef __HAIKU__
	BPath path;
	find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	const char *homedir = path.Path();
#else
	const char *homedir = getenv("HOME");

	if (homedir == NULL) {
		const struct passwd *pw = getpwuid(getuid());
		homedir = (pw == NULL) ? "" : pw->pw_dir;
	}
#endif

	snprintf(tmp, MAX_PATH, "%s" PATHSEP "%s", homedir, PERSONAL_DIR);
	AppendPathSeparator(tmp, MAX_PATH);

	_searchpaths[SP_PERSONAL_DIR] = strdup(tmp);
#endif

#if defined(WITH_SHARED_DIR)
	snprintf(tmp, MAX_PATH, "%s", SHARED_DIR);
	AppendPathSeparator(tmp, MAX_PATH);
	_searchpaths[SP_SHARED_DIR] = strdup(tmp);
#else
	_searchpaths[SP_SHARED_DIR] = NULL;
#endif

#if defined(__MORPHOS__) || defined(__AMIGA__)
	_searchpaths[SP_WORKING_DIR] = NULL;
#else
	if (getcwd(tmp, MAX_PATH) == NULL) *tmp = '\0';
	AppendPathSeparator(tmp, MAX_PATH);
	_searchpaths[SP_WORKING_DIR] = strdup(tmp);
#endif

	_do_scan_working_directory = DoScanWorkingDirectory();

	/* Change the working directory to that one of the executable */
	if (ChangeWorkingDirectoryToExecutable(exe)) {
		if (getcwd(tmp, MAX_PATH) == NULL) *tmp = '\0';
		AppendPathSeparator(tmp, MAX_PATH);
		_searchpaths[SP_BINARY_DIR] = strdup(tmp);
	} else {
		_searchpaths[SP_BINARY_DIR] = NULL;
	}

	if (_searchpaths[SP_WORKING_DIR] != NULL) {
		/* Go back to the current working directory. */
		if (chdir(_searchpaths[SP_WORKING_DIR]) != 0) {
			DEBUG(misc, 0, "Failed to return to working directory!");
		}
	}

#if defined(__MORPHOS__) || defined(__AMIGA__) || defined(DOS) || defined(OS2)
	_searchpaths[SP_INSTALLATION_DIR] = NULL;
#else
	snprintf(tmp, MAX_PATH, "%s", GLOBAL_DATA_DIR);
	AppendPathSeparator(tmp, MAX_PATH);
	_searchpaths[SP_INSTALLATION_DIR] = strdup(tmp);
#endif
#ifdef WITH_COCOA
extern void cocoaSetApplicationBundleDir();
	cocoaSetApplicationBundleDir();
#else
	_searchpaths[SP_APPLICATION_BUNDLE_DIR] = NULL;
#endif
}
#endif /* defined(WIN32) || defined(WINCE) */

char *_personal_dir;

/**
 * Acquire the base paths (personal dir and game data dir),
 * fill all other paths (save dir, autosave dir etc) and
 * make the save and scenario directories.
 * @param exe the path from the current path to the executable
 */
void DeterminePaths(const char *exe)
{
	DetermineBasePaths(exe);

	Searchpath sp;
	FOR_ALL_SEARCHPATHS(sp) {
		if (sp == SP_WORKING_DIR && !_do_scan_working_directory) continue;
		DEBUG(misc, 4, "%s added as search path", _searchpaths[sp]);
	}

	if (_config_file != NULL) {
		_personal_dir = strdup(_config_file);
		char *end = strrchr(_personal_dir, PATHSEPCHAR);
		if (end == NULL) {
			_personal_dir[0] = '\0';
		} else {
			end[1] = '\0';
		}
	} else {
		char personal_dir[MAX_PATH];
		FioFindFullPath(personal_dir, lengthof(personal_dir), BASE_DIR, "openttd.cfg");

		if (FileExists(personal_dir)) {
			char *end = strrchr(personal_dir, PATHSEPCHAR);
			if (end != NULL) end[1] = '\0';
			_personal_dir = strdup(personal_dir);
			_config_file = str_fmt("%sopenttd.cfg", _personal_dir);
		} else {
			static const Searchpath new_openttd_cfg_order[] = {
					SP_PERSONAL_DIR, SP_BINARY_DIR, SP_WORKING_DIR, SP_SHARED_DIR, SP_INSTALLATION_DIR
				};

			for (uint i = 0; i < lengthof(new_openttd_cfg_order); i++) {
				if (IsValidSearchPath(new_openttd_cfg_order[i])) {
					_personal_dir = strdup(_searchpaths[new_openttd_cfg_order[i]]);
					_config_file = str_fmt("%sopenttd.cfg", _personal_dir);
					break;
				}
			}
		}
	}

	DEBUG(misc, 3, "%s found as personal directory", _personal_dir);

	_highscore_file = str_fmt("%shs.dat", _personal_dir);
	extern char *_hotkeys_file;
	_hotkeys_file = str_fmt("%shotkeys.cfg",  _personal_dir);

	/* Make the necessary folders */
#if !defined(__MORPHOS__) && !defined(__AMIGA__) && defined(WITH_PERSONAL_DIR)
	FioCreateDirectory(_personal_dir);
#endif

	static const Subdirectory default_subdirs[] = {
		SAVE_DIR, AUTOSAVE_DIR, SCENARIO_DIR, HEIGHTMAP_DIR
	};

	for (uint i = 0; i < lengthof(default_subdirs); i++) {
		char *dir = str_fmt("%s%s", _personal_dir, _subdirs[default_subdirs[i]]);
		FioCreateDirectory(dir);
		free(dir);
	}

	/* If we have network we make a directory for the autodownloading of content */
	_searchpaths[SP_AUTODOWNLOAD_DIR] = str_fmt("%s%s", _personal_dir, "content_download" PATHSEP);
#ifdef ENABLE_NETWORK
	FioCreateDirectory(_searchpaths[SP_AUTODOWNLOAD_DIR]);

	/* Create the directory for each of the types of content */
	const Subdirectory dirs[] = { SCENARIO_DIR, HEIGHTMAP_DIR, DATA_DIR, AI_DIR, AI_LIBRARY_DIR, GM_DIR };
	for (uint i = 0; i < lengthof(dirs); i++) {
		char *tmp = str_fmt("%s%s", _searchpaths[SP_AUTODOWNLOAD_DIR], _subdirs[dirs[i]]);
		FioCreateDirectory(tmp);
		free(tmp);
	}

	extern char *_log_file;
	_log_file = str_fmt("%sopenttd.log",  _personal_dir);
#else /* ENABLE_NETWORK */
	/* If we don't have networking, we don't need to make the directory. But
	 * if it exists we keep it, otherwise remove it from the search paths. */
	if (!FileExists(_searchpaths[SP_AUTODOWNLOAD_DIR]))  {
		free((void*)_searchpaths[SP_AUTODOWNLOAD_DIR]);
		_searchpaths[SP_AUTODOWNLOAD_DIR] = NULL;
	}
#endif /* ENABLE_NETWORK */

	TarScanner::DoScan();
}

/**
 * Sanitizes a filename, i.e. removes all illegal characters from it.
 * @param filename the "\0" terminated filename
 */
void SanitizeFilename(char *filename)
{
	for (; *filename != '\0'; filename++) {
		switch (*filename) {
			/* The following characters are not allowed in filenames
			 * on at least one of the supported operating systems: */
			case ':': case '\\': case '*': case '?': case '/':
			case '<': case '>': case '|': case '"':
				*filename = '_';
				break;
		}
	}
}

void *ReadFileToMem(const char *filename, size_t *lenp, size_t maxsize)
{
	FILE *in = fopen(filename, "rb");
	if (in == NULL) return NULL;

	fseek(in, 0, SEEK_END);
	size_t len = ftell(in);
	fseek(in, 0, SEEK_SET);
	if (len > maxsize) {
		fclose(in);
		return NULL;
	}
	byte *mem = MallocT<byte>(len + 1);
	mem[len] = 0;
	if (fread(mem, len, 1, in) != 1) {
		fclose(in);
		free(mem);
		return NULL;
	}
	fclose(in);

	*lenp = len;
	return mem;
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
	extern bool FiosIsValidFile(const char *path, const struct dirent *ent, struct stat *sb);

	uint num = 0;
	struct stat sb;
	struct dirent *dirent;
	DIR *dir;

	if (path == NULL || (dir = ttd_opendir(path)) == NULL) return 0;

	while ((dirent = readdir(dir)) != NULL) {
		const char *d_name = FS2OTTD(dirent->d_name);
		char filename[MAX_PATH];

		if (!FiosIsValidFile(path, dirent, &sb)) continue;

		snprintf(filename, lengthof(filename), "%s%s", path, d_name);

		if (S_ISDIR(sb.st_mode)) {
			/* Directory */
			if (!recursive) continue;
			if (strcmp(d_name, ".") == 0 || strcmp(d_name, "..") == 0) continue;
			if (!AppendPathSeparator(filename, lengthof(filename))) continue;
			num += ScanPath(fs, extension, filename, basepath_length, recursive);
		} else if (S_ISREG(sb.st_mode)) {
			/* File */
			if (extension != NULL) {
				char *ext = strrchr(filename, '.');

				/* If no extension or extension isn't .grf, skip the file */
				if (ext == NULL) continue;
				if (strcasecmp(ext, extension) != 0) continue;
			}

			if (fs->AddFile(filename, basepath_length)) num++;
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
static uint ScanTar(FileScanner *fs, const char *extension, TarFileList::iterator tar)
{
	uint num = 0;
	const char *filename = (*tar).first.c_str();

	if (extension != NULL) {
		const char *ext = strrchr(filename, '.');

		/* If no extension or extension isn't .grf, skip the file */
		if (ext == NULL) return false;
		if (strcasecmp(ext, extension) != 0) return false;
	}

	if (fs->AddFile(filename, 0)) num++;

	return num;
}

/**
 * Scan for files with the given extention in the given search path.
 * @param extension the extension of files to search for.
 * @param sd        the sub directory to search in.
 * @param tars      whether to search in the tars too.
 * @param recursive whether to search recursively
 * @return the number of found files, i.e. the number of times that
 *         AddFile returned true.
 */
uint FileScanner::Scan(const char *extension, Subdirectory sd, bool tars, bool recursive)
{
	Searchpath sp;
	char path[MAX_PATH];
	TarFileList::iterator tar;
	uint num = 0;

	FOR_ALL_SEARCHPATHS(sp) {
		/* Don't search in the working directory */
		if (sp == SP_WORKING_DIR && !_do_scan_working_directory) continue;

		FioAppendDirectory(path, MAX_PATH, sp, sd);
		num += ScanPath(this, extension, path, strlen(path), recursive);
	}

	if (tars) {
		FOR_ALL_TARS(tar) {
			num += ScanTar(this, extension, tar);
		}
	}

	return num;
}

/**
 * Scan for files with the given extention in the given search path.
 * @param extension the extension of files to search for.
 * @param directory the sub directory to search in.
 * @param recursive whether to search recursively
 * @return the number of found files, i.e. the number of times that
 *         AddFile returned true.
 */
uint FileScanner::Scan(const char *extension, const char *directory, bool recursive)
{
	char path[MAX_PATH];
	strecpy(path, directory, lastof(path));
	if (!AppendPathSeparator(path, lengthof(path))) return 0;
	return ScanPath(this, extension, path, strlen(path), recursive);
}
