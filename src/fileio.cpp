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
#ifndef WIN32
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
	FILE *handles[MAX_HANDLES];         ///< array of file handles we can have open
	byte buffer_start[FIO_BUFFER_SIZE]; ///< local buffer when read from file
#if defined(LIMITED_FDS)
	uint open_handles;                  ///< current amount of open handles
	const char *filename[MAX_HANDLES];  ///< array of filenames we (should) have open
	uint usage_count[MAX_HANDLES];      ///< count how many times this file has been opened
#endif /* LIMITED_FDS */
};

static Fio _fio;

/* Get current position in file */
uint32 FioGetPos()
{
	return _fio.pos + (_fio.buffer - _fio.buffer_start) - FIO_BUFFER_SIZE;
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
		DEBUG(misc, 6, "Restoring file '%s' in slot '%d' from disk", _fio.filename[slot], slot);
		FioOpenFile(slot, _fio.filename[slot]);
	}
	_fio.usage_count[slot]++;
}
#endif /* LIMITED_FDS */

/* Seek to a file and a position */
void FioSeekToFile(uint32 pos)
{
	FILE *f;
#if defined(LIMITED_FDS)
	/* Make sure we have this file open */
	FioRestoreFile(pos >> 24);
#endif /* LIMITED_FDS */
	f = _fio.handles[pos >> 24];
	assert(f != NULL);
	_fio.cur_fh = f;
	FioSeekTo(GB(pos, 0, 24), SEEK_SET);
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
		DEBUG(misc, 6, "Closing filehandler '%s' in slot '%d' because of fd-limit", _fio.filename[slot], slot);
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
	if (f == NULL) error("Cannot open file '%s%s'", _paths.data_dir, filename);

	FioCloseFile(slot); // if file was opened before, close it
	_fio.handles[slot] = f;
#if defined(LIMITED_FDS)
	_fio.filename[slot] = filename;
	_fio.usage_count[slot] = 0;
	_fio.open_handles++;
#endif /* LIMITED_FDS */
	FioSeekToFile(slot << 24);
}

/**
 * Check whether the given file exists
 * @param filename the file to try for existance
 * @return true if and only if the file can be opened
 */
bool FioCheckFileExists(const char *filename)
{
	FILE *f = FioFOpenFile(filename);
	if (f == NULL) return false;

	fclose(f);
	return true;
}

/**
 * Opens the file with the given name
 * @param filename the file to open (in either data_dir or second_data_dir)
 * @return the opened file or NULL when it failed.
 */
FILE *FioFOpenFile(const char *filename)
{
	FILE *f;
	char buf[MAX_PATH];

	snprintf(buf, lengthof(buf), "%s%s", _paths.data_dir, filename);

	f = fopen(buf, "rb");
#if !defined(WIN32)
	if (f == NULL) {
		strtolower(buf + strlen(_paths.data_dir) - 1);
		f = fopen(buf, "rb");

#if defined SECOND_DATA_DIR
		/* tries in the 2nd data directory */
		if (f == NULL) {
			snprintf(buf, lengthof(buf), "%s%s", _paths.second_data_dir, filename);
			strtolower(buf + strlen(_paths.second_data_dir) - 1);
			f = fopen(buf, "rb");
		}
#endif
	}
#endif

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
 * Determine the base (personal dir and game data dir) paths
 * @note defined in the OS related files (os2.cpp, win32.cpp, unix.cpp etc)
 */
extern void DetermineBasePaths();

/**
 * Acquire the base paths (personal dir and game data dir),
 * fill all other paths (save dir, autosave dir etc) and
 * make the save and scenario directories.
 * @todo for save_dir, autosave_dir, scenario_dir and heightmap_dir the
 *       assumption is that there is no path separator, however for gm_dir
 *       lang_dir and data_dir that assumption is made.
 *       This inconsistency should be resolved.
 */
void DeterminePaths()
{
	DetermineBasePaths();

	_paths.save_dir      = str_fmt("%ssave", _paths.personal_dir);
	_paths.autosave_dir  = str_fmt("%s" PATHSEP "autosave", _paths.save_dir);
	_paths.scenario_dir  = str_fmt("%sscenario", _paths.personal_dir);
	_paths.heightmap_dir = str_fmt("%s" PATHSEP "heightmap", _paths.scenario_dir);
	_paths.gm_dir        = str_fmt("%sgm" PATHSEP, _paths.game_data_dir);
	_paths.data_dir      = str_fmt("%sdata" PATHSEP, _paths.game_data_dir);
#if defined(CUSTOM_LANG_DIR)
	/* Sets the search path for lng files to the custom one */
	_paths.lang_dir = MallocT<char>(MAX_PATH);
	ttd_strlcpy(_paths.lang_dir, CUSTOM_LANG_DIR, MAX_PATH);
#else
	_paths.lang_dir = str_fmt("%slang" PATHSEP, _paths.game_data_dir);
#endif

	if (_config_file == NULL) {
		_config_file = str_fmt("%sopenttd.cfg", _paths.personal_dir);
	}

	_highscore_file = str_fmt("%shs.dat", _paths.personal_dir);
	_log_file = str_fmt("%sopenttd.log",  _paths.personal_dir);

	/* Make (auto)save and scenario folder */
	FioCreateDirectory(_paths.save_dir);
	FioCreateDirectory(_paths.autosave_dir);
	FioCreateDirectory(_paths.scenario_dir);
	FioCreateDirectory(_paths.heightmap_dir);
}
