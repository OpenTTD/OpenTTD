/* $Id$ */

/** @file fileio.cpp Standard In/Out file operations */

#include "stdafx.h"
#include "openttd.h"
#include "fileio.h"
#include "functions.h"
#include "string.h"
#include "macros.h"
#include "variables.h"
#include "debug.h"
#include "fios.h"
#ifdef WIN32
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

/*************************************************/
/* FILE IO ROUTINES ******************************/
/*************************************************/

#define FIO_BUFFER_SIZE 512
#define MAX_HANDLES 64

struct Fio {
	byte *buffer, *buffer_end;          ///< position pointer in local buffer and last valid byte of buffer
	uint32 pos;                         ///< current (system) position in file
	FILE *cur_fh;                       ///< current file handle
	const char *filename;               ///< current filename
	FILE *handles[MAX_HANDLES];         ///< array of file handles we can have open
	byte buffer_start[FIO_BUFFER_SIZE]; ///< local buffer when read from file
	const char *filenames[MAX_HANDLES]; ///< array of filenames we (should) have open
#if defined(LIMITED_FDS)
	uint open_handles;                  ///< current amount of open handles
	uint usage_count[MAX_HANDLES];      ///< count how many times this file has been opened
#endif /* LIMITED_FDS */
};

static Fio _fio;

/* Get current position in file */
uint32 FioGetPos()
{
	return _fio.pos + (_fio.buffer - _fio.buffer_start) - FIO_BUFFER_SIZE;
}

const char *FioGetFilename()
{
	return _fio.filename;
}

void FioSeekTo(uint32 pos, int mode)
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
void FioSeekToFile(uint8 slot, uint32 pos)
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
		_fio.pos += FIO_BUFFER_SIZE;
		fread(_fio.buffer = _fio.buffer_start, 1, FIO_BUFFER_SIZE, _fio.cur_fh);
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

void FioReadBlock(void *ptr, uint size)
{
	FioSeekTo(FioGetPos(), SEEK_SET);
	_fio.pos += size;
	fread(ptr, 1, size, _fio.cur_fh);
}

static inline void FioCloseFile(int slot)
{
	if (_fio.handles[slot] != NULL) {
		fclose(_fio.handles[slot]);
		_fio.handles[slot] = NULL;
#if defined(LIMITED_FDS)
		_fio.open_handles--;
#endif /* LIMITED_FDS */
	}
}

void FioCloseAll()
{
	int i;

	for (i = 0; i != lengthof(_fio.handles); i++)
		FioCloseFile(i);
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
	if (f == NULL) error("Cannot open file '%s'", filename);
	uint32 pos = ftell(f);

	FioCloseFile(slot); // if file was opened before, close it
	_fio.handles[slot] = f;
	_fio.filenames[slot] = filename;
#if defined(LIMITED_FDS)
	_fio.usage_count[slot] = 0;
	_fio.open_handles++;
#endif /* LIMITED_FDS */
	FioSeekToFile(slot, pos);
}

const char *_subdirs[NUM_SUBDIRS] = {
	"",
	"save" PATHSEP,
	"save" PATHSEP "autosave" PATHSEP,
	"scenario" PATHSEP,
	"scenario" PATHSEP "heightmap" PATHSEP,
	"gm" PATHSEP,
	"data" PATHSEP,
	"lang" PATHSEP
};

const char *_searchpaths[NUM_SEARCHPATHS];

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

	fclose(f);
	return true;
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

FILE *FioFOpenFileSp(const char *filename, const char *mode, Searchpath sp, Subdirectory subdir)
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
		ttd_strlcpy(buf, filename, lengthof(buf));
	} else {
		snprintf(buf, lengthof(buf), "%s%s%s", _searchpaths[sp], _subdirs[subdir], filename);
	}

	f = fopen(buf, mode);
#if !defined(WIN32)
	if (f == NULL) {
		strtolower(buf + strlen(_searchpaths[sp]) - 1);
		f = fopen(buf, mode);
	}
#endif
	return f;
}

/** Opens OpenTTD files somewhere in a personal or global directory */
FILE *FioFOpenFile(const char *filename, const char *mode, Subdirectory subdir)
{
	FILE *f = NULL;
	Searchpath sp;

	assert(subdir < NUM_SUBDIRS || subdir == NO_DIRECTORY);

	FOR_ALL_SEARCHPATHS(sp) {
		f = FioFOpenFileSp(filename, mode, sp, subdir);
		if (f != NULL || subdir == NO_DIRECTORY) break;
	}

	return f;
}

/**
 * Create a directory with the given name
 * @param name the new name of the directory
 */
void FioCreateDirectory(const char *name)
{
#if defined(WIN32) || defined(WINCE)
	CreateDirectory(OTTD2FS(name), NULL);
#elif defined(OS2) && !defined(__INNOTEK_LIBC__)
	mkdir(OTTD2FS(name));
#else
	mkdir(OTTD2FS(name), 0755);
#endif
}

/**
 * Appends, if necessary, the path separator character to the end of the string.
 * It does not add the path separator to zero-sized strings.
 * @param buf    string to append the separator to
 * @param buflen the length of the buf
 */
void AppendPathSeparator(char *buf, size_t buflen)
{
	size_t s = strlen(buf);

	/* Length of string + path separator + '\0' */
	if (s != 0 && buf[s - 1] != PATHSEPCHAR && s + 2 < buflen) {
		buf[s]     = PATHSEPCHAR;
		buf[s + 1] = '\0';
	}
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
		getcwd(dest, MAX_PATH);
		AppendPathSeparator(dest, MAX_PATH);
		ttd_strlcat(dest, dir, MAX_PATH);
	}
	AppendPathSeparator(dest, MAX_PATH);

	return dest;
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
void ChangeWorkingDirectory(const char *exe)
{
#ifdef WITH_COCOA
	char *app_bundle = strchr(exe, '.');
	while (app_bundle != NULL && strncasecmp(app_bundle, ".app", 4) != 0) app_bundle = strchr(&app_bundle[1], '.');

	if (app_bundle != NULL) app_bundle[0] = '\0';
#endif /* WITH_COCOA */
	char *s = strrchr(exe, PATHSEPCHAR);
	if (s != NULL) {
		*s = '\0';
		chdir(exe);
		*s = PATHSEPCHAR;
	}
#ifdef WITH_COCOA
	if (app_bundle != NULL) app_bundle[0] = '.';
#endif /* WITH_COCOA */
}

/**
 * Determine the base (personal dir and game data dir) paths
 * @param exe the path to the executable
 */
void DetermineBasePaths(const char *exe)
{
	char tmp[MAX_PATH];
#if defined(__MORPHOS__) || defined(__AMIGA__) || !defined(WITH_PERSONAL_DIR)
	_searchpaths[SP_PERSONAL_DIR] = NULL;
#else
	const char *homedir = getenv("HOME");

	if (homedir == NULL) {
		const struct passwd *pw = getpwuid(getuid());
		homedir = (pw == NULL) ? "" : pw->pw_dir;
	}

	snprintf(tmp, MAX_PATH, "%s" PATHSEP "%s", homedir, PERSONAL_DIR);
	AppendPathSeparator(tmp, MAX_PATH);

	_searchpaths[SP_PERSONAL_DIR] = strdup(tmp);
#endif
	_searchpaths[SP_SHARED_DIR] = NULL;

#if defined(__MORPHOS__) || defined(__AMIGA__)
	_searchpaths[SP_WORKING_DIR] = NULL;
#else
	getcwd(tmp, MAX_PATH);
	AppendPathSeparator(tmp, MAX_PATH);
	_searchpaths[SP_WORKING_DIR] = strdup(tmp);
#endif

	/* Change the working directory to that one of the executable */
	ChangeWorkingDirectory((char*)exe);
	getcwd(tmp, MAX_PATH);
	AppendPathSeparator(tmp, MAX_PATH);
	_searchpaths[SP_BINARY_DIR] = strdup(tmp);

#if defined(__MORPHOS__) || defined(__AMIGA__)
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
	FOR_ALL_SEARCHPATHS(sp) DEBUG(misc, 4, "%s added as search path", _searchpaths[sp]);

	if (_config_file != NULL) {
		_personal_dir = strdup(_config_file);
		char *end = strrchr(_personal_dir , PATHSEPCHAR);
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
	_log_file = str_fmt("%sopenttd.log",  _personal_dir);

	char *save_dir     = str_fmt("%s%s", _personal_dir, FioGetSubdirectory(SAVE_DIR));
	char *autosave_dir = str_fmt("%s%s", _personal_dir, FioGetSubdirectory(AUTOSAVE_DIR));

	/* Make the necessary folders */
	FioCreateDirectory(_personal_dir);
	FioCreateDirectory(save_dir);
	FioCreateDirectory(autosave_dir);

	free(save_dir);
	free(autosave_dir);
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
