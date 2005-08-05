/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "hal.h"
#include "variables.h"
#include "string.h"
#include "table/strings.h"
#include "gfx.h"
#include "gui.h"
#include "functions.h"
#include "macros.h"

#include <direct.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <process.h>
#include <time.h>
#include <dos.h>

#define INCL_DOS
#define INCL_WIN
#define INCL_WINCLIPBOARD

#include <os2.h>

#include <i86.h>

static inline int strcasecmp(const char* s1, const char* s2)
{
	return stricmp(s1, s2);
}

static char *_fios_path;
static char *_fios_save_path;
static char *_fios_scn_path;
static FiosItem *_fios_items;
static int _fios_count, _fios_alloc;

static FiosItem *FiosAlloc(void)
{
	if (_fios_count == _fios_alloc) {
		_fios_alloc += 256;
		_fios_items = realloc(_fios_items, _fios_alloc * sizeof(FiosItem));
	}
	return &_fios_items[_fios_count++];
}

int compare_FiosItems(const void *a, const void *b)
{
	const FiosItem *da = (const FiosItem *)a;
	const FiosItem *db = (const FiosItem *)b;
	int r;

	if (_savegame_sort_order < 2) // sort by date
		r = da->mtime < db->mtime ? -1 : 1;
	else
		r = strcasecmp(da->title, db->title);

	if (_savegame_sort_order & 1) r = -r;
	return r;
}


static DIR *my_opendir(char *path, char *file)
{
	char paths[MAX_PATH];

	append_path(paths, path, file);
	return opendir(paths);
}

static void append_path(char *out, const char *path, const char *file)
{
	if (path[2] == '\\' && path[3] == '\0')
		sprintf(out, "%s%s", path, file);
	else
		sprintf(out, "%s\\%s", path, file);
}

// Get a list of savegames
FiosItem *FiosGetSavegameList(int *num, int mode)
{
	FiosItem *fios;
	DIR *dir;
	struct dirent *dirent;
	struct stat sb;
	int sort_start;
	char filename[MAX_PATH];

	if (_fios_save_path == NULL) {
		_fios_save_path = malloc(MAX_PATH);
		strcpy(_fios_save_path, _path.save_dir);
	}

	_fios_path = _fios_scn_path;

	// Parent directory, only if not of the type C:\.
	if (_fios_path[3] != '\0') {
		fios = FiosAlloc();
		fios->type = FIOS_TYPE_PARENT;
		fios->mtime = 0;
		strcpy(fios->name, "..");
		strcpy(fios->title, ".. (Parent directory)");
	}

	// Show subdirectories first
	dir = my_opendir(_fios_path, "*.*");
	if (dir != NULL) {
		while ((dirent = readdir(dir)) != NULL) {
			append_path(filename, _fios_path, dirent->d_name);
			if (!stat(filename, &sb) && S_ISDIR(sb.st_mode) &&
					strcmp(dirent->d_name, ".") != 0 &&
					strcmp(dirent->d_name, "..") != 0) {
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_DIR;
				fios->mtime = 0;
				ttd_strlcpy(fios->name, dirent->d_name, lengthof(fios->name));
				snprintf(fios->title, lengthof(fios->title), "%s\\ (Directory)", dirent->d_name);
			}
		}
		closedir(dir);
	}

	{
		/* XXX ugly global variables ... */
		byte order = _savegame_sort_order;
		_savegame_sort_order = 2; // sort ascending by name
		qsort(_fios_items, _fios_count, sizeof(FiosItem), compare_FiosItems);
		_savegame_sort_order = order;
	}

	// this is where to start sorting
	sort_start = _fios_count;

	/* Show savegame files
	 * .SAV OpenTTD saved game
	 * .SS1 Transport Tycoon Deluxe preset game
	 * .SV1 Transport Tycoon Deluxe (Patch) saved game
	 * .SV2 Transport Tycoon Deluxe (Patch) saved 2-player game
	 */
	dir = my_opendir(_fios_path, "*.*");
	if (dir != NULL) {
		while ((dirent = readdir(dir)) != NULL) {
			char *t;

			append_path(filename, _fios_path, dirent->d_name);
			if (stat(filename, &sb) || S_ISDIR(sb.st_mode)) continue;

			t = strrchr(dirent->d_name, '.');
			if (t != NULL && strcasecmp(t, ".sav") == 0) { // OpenTTD
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_FILE;
				fios->mtime = sb.st_mtime;
				ttd_strlcpy(fios->name, dirent->d_name, lengthof(fios->name));

				*t = '\0'; // strip extension
				ttd_strlcpy(fios->title, dirent->d_name, lengthof(fios->title));
			} else if (mode == SLD_LOAD_GAME || mode == SLD_LOAD_SCENARIO) {
				if (t != NULL && (
							strcasecmp(t, ".ss1") == 0 ||
							strcasecmp(t, ".sv1") == 0 ||
							strcasecmp(t, ".sv2") == 0
						)) { // TTDLX(Patch)
					fios = FiosAlloc();
					fios->type = FIOS_TYPE_OLDFILE;
					fios->mtime = sb.st_mtime;
					ttd_strlcpy(fios->name, dirent->d_name, lengthof(fios->name));
					GetOldSaveGameName(fios->title, filename);
				}
			}
		}
		closedir(dir);
	}

	qsort(_fios_items + sort_start, _fios_count - sort_start, sizeof(FiosItem), compare_FiosItems);

	// Drives
	{
		uint save;
		uint disk;
		uint total;

		/* save original drive */
		_dos_getdrive(&save);

		/* get available drive letters */
		for (disk = 1; disk < 27; ++disk)
		{
			uint disk2;

			_dos_setdrive(disk, &total);
			_dos_getdrive(&disk2);

			if (disk == disk2)
			{
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_DRIVE;
				sprintf(fios->name, "%c:", 'A' + disk - 1);
				sprintf(fios->title, "%c:", 'A' + disk - 1);
			}
		}

		_dos_setdrive(save, &total);
	}

	*num = _fios_count;
	return _fios_items;
}

// Get a list of scenarios
FiosItem *FiosGetScenarioList(int *num, int mode)
{
	FiosItem *fios;
	DIR *dir;
	struct dirent *dirent;
	struct stat sb;
	int sort_start;
	char filename[MAX_PATH];

	if (_fios_scn_path == NULL) {
		_fios_scn_path = malloc(MAX_PATH);
		strcpy(_fios_scn_path, _path.scenario_dir);
	}

	_fios_path = _fios_scn_path;

	// Parent directory, only if not of the type C:\.
	if (_fios_path[3] != '\0' && mode != SLD_NEW_GAME) {
		fios = FiosAlloc();
		fios->type = FIOS_TYPE_PARENT;
		fios->mtime = 0;
		strcpy(fios->title, ".. (Parent directory)");
	}

	// Show subdirectories first
	dir = my_opendir(_fios_path, "*.*");
	if (dir != NULL) {
		while ((dirent = readdir(dir)) != NULL) {
			append_path(filename, _fios_path, dirent->d_name);
			if (!stat(filename, &sb) && S_ISDIR(sb.st_mode) &&
					strcmp(dirent->d_name, ".") != 0 &&
					strcmp(dirent->d_name, "..") != 0) {
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_DIR;
				fios->mtime = 0;
				ttd_strlcpy(fios->name, dirent->d_name, lengthof(fios->name));
				snprintf(fios->title, lengthof(fios->title), "%s\\ (Directory)", dirent->d_name);
			}
		}
		closedir(dir);
	}

	// this is where to start sorting
	sort_start = _fios_count;

	/* Show scenario files
	 * .SCN OpenTTD style scenario file
	 * .SV0 Transport Tycoon Deluxe (Patch) scenario
	 * .SS0 Transport Tycoon Deluxe preset scenario
	 */
	dir = my_opendir(_fios_path, "*.*");
	if (dir != NULL) {
		while ((dirent = readdir(dir)) != NULL) {
			char *t;

			append_path(filename, _fios_path, dirent->d_name);
			if (stat(filename, &sb) || S_ISDIR(sb.st_mode)) continue;

			t = strrchr(dirent->d_name, '.');
			if (t != NULL && strcasecmp(t, ".scn") == 0) { // OpenTTD
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_SCENARIO;
				fios->mtime = sb.st_mtime;
				ttd_strlcpy(fios->name, dirent->d_name, lengthof(fios->name));

				*t = '\0'; // strip extension
				ttd_strlcpy(fios->title, dirent->d_name, lengthof(fios->title));
			} else if (mode == SLD_LOAD_GAME || mode == SLD_LOAD_SCENARIO ||
					mode == SLD_NEW_GAME) {
				if (t != NULL && (
							strcasecmp(t, ".sv0") == 0 ||
							strcasecmp(t, ".ss0") == 0
						)) { // TTDLX(Patch)
					fios = FiosAlloc();
					fios->type = FIOS_TYPE_OLD_SCENARIO;
					fios->mtime = sb.st_mtime;
					GetOldScenarioGameName(fios->title, filename);
					ttd_strlcpy(fios->name, dirent->d_name, lengthof(fios->name));
				}
			}
		}
		closedir(dir);
	}

	qsort(_fios_items + sort_start, _fios_count - sort_start, sizeof(FiosItem), compare_FiosItems);

	// Drives
	if (mode != SLD_NEW_GAME)
	{
		unsigned save, disk, disk2, total;

		/* save original drive */
		_dos_getdrive(&save);

		/* get available drive letters */

		for (disk = 1; disk < 27; ++disk)
		{
			_dos_setdrive(disk, &total);
			_dos_getdrive(&disk2);

			if (disk == disk2)
			{
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_DRIVE;
				sprintf(fios->name, "%c:", 'A' + disk - 1);
				sprintf(fios->title, "%c:", 'A' + disk - 1);
			}
		}

		_dos_setdrive(save, &total);
	}

	*num = _fios_count;
	return _fios_items;
}


// Free the list of savegames
void FiosFreeSavegameList(void)
{
	free(_fios_items);
	_fios_items = NULL;
	_fios_alloc = _fios_count = 0;
}

// Browse to
char *FiosBrowseTo(const FiosItem *item)
{
	char *path = _fios_path;
	char *s;

	switch (item->type) {
		case FIOS_TYPE_DRIVE:
			sprintf(path, "%c:\\", item->title[0]);
			break;

		case FIOS_TYPE_PARENT:
			s = strrchr(path, '\\');
			if (s != NULL) *s = '\0';
			break;

		case FIOS_TYPE_DIR:
			s = strchr(item->name, '\\');
			if (s != NULL) *s = '\0';
			if (path[3]!= '\0' ) strcat(path, "\\");
			strcat(path, item->name);
			break;

		case FIOS_TYPE_FILE:
		case FIOS_TYPE_OLDFILE:
		case FIOS_TYPE_SCENARIO:
		case FIOS_TYPE_OLD_SCENARIO: {
			static char str_buffr[512];

			sprintf(str_buffr, "%s\\%s", path, item->name);
			return str_buffr;
		}
	}

	return NULL;
}

/**
 * Get descriptive texts. Returns the path and free space
 * left on the device
 * @param path string describing the path
 * @param tfs total free space in megabytes, optional (can be NULL)
 * @return StringID describing the path (free space or failure)
 */
StringID FiosGetDescText(const char **path, uint32 *tot)
{
	struct diskfree_t free;
	char drive;

	*path = _fios_path;
	drive = *path[0] - 'A' + 1;

	if (tot != NULL && _getdiskfree(drive, &free) == 0) {
		*tot = free.avail_clusters * free.sectors_per_cluster * free.bytes_per_sector;
		return STR_4005_BYTES_FREE;
	}

	return STR_4006_UNABLE_TO_READ_DRIVE;
}

void FiosMakeSavegameName(char *buf, const char *name)
{
	const char* extension;
	const char* period;

	if (_game_mode == GM_EDITOR)
		extension = ".scn";
	else
		extension = ".sav";

	// Don't append the extension, if it is already there
	period = strrchr(name, '.');
	if (period != NULL && strcasecmp(period, extension) == 0) extension = "";

	sprintf(buf, "%s\\%s%s", _fios_path, name, extension);
}

bool FiosDelete(const char *name)
{
	char path[512];

	snprintf(path, lengthof(path), "%s\\%s", _fios_path, name);
	return unlink(path) == 0;
}

bool FileExists(const char *filename)
{
	return access(filename, 0) == 0;
}

static int LanguageCompareFunc(const void *a, const void *b)
{
	return strcmp(*(const char* const *)a, *(const char* const *)b);
}

int GetLanguageList(char **languages, int max)
{
	DIR *dir;
	struct dirent *dirent;
	int num = 0;

	dir = opendir(_path.lang_dir);
	if (dir != NULL) {
		while ((dirent = readdir(dir))) {
			char *t = strrchr(dirent->d_name, '.');
			if (t && !strcmp(t, ".lng")) {
				languages[num++] = strdup(dirent->d_name);
				if (num == max) break;
			}
		}
		closedir(dir);
	}

	qsort(languages, num, sizeof(char*), LanguageCompareFunc);
	return num;
}

static void ChangeWorkingDirectory(char *exe)
{
	char *s = strrchr(exe, '\\');
	if (s != NULL) {
		*s = '\0';
		chdir(exe);
		*s = '\\';
	}
}

void ShowInfo(const char *str)
{
	HAB hab;
	HMQ hmq;
	ULONG rc;

	// init PM env.
	hmq = WinCreateMsgQueue((hab = WinInitialize(0)), 0);

	// display the box
	rc = WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, str, "OpenTTD", 0, MB_OK | MB_MOVEABLE | MB_INFORMATION);

	// terminate PM env.
	WinDestroyMsgQueue(hmq);
	WinTerminate(hab);
}

void ShowOSErrorBox(const char *buf)
{
	HAB hab;
	HMQ hmq;
	ULONG rc;

	// init PM env.
	hmq = WinCreateMsgQueue((hab = WinInitialize(0)), 0);

	// display the box
	rc = WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, buf, "OpenTTD", 0, MB_OK | MB_MOVEABLE | MB_ERROR);

	// terminate PM env.
	WinDestroyMsgQueue(hmq);
	WinTerminate(hab);
}

int CDECL main(int argc, char* argv[])
{
	// change the working directory to enable doubleclicking in UIs
	ChangeWorkingDirectory(argv[0]);

	_random_seeds[0][1] = _random_seeds[0][0] = time(NULL);


	return ttd_main(argc, argv);
}

void DeterminePaths(void)
{
	char *s;

	_path.game_data_dir = malloc(MAX_PATH);
	ttd_strlcpy(_path.game_data_dir, GAME_DATA_DIR, MAX_PATH);
	#if defined SECOND_DATA_DIR
	_path.second_data_dir = malloc(MAX_PATH);
	ttd_strlcpy(_path.second_data_dir, SECOND_DATA_DIR, MAX_PATH);
	#endif

#if defined(USE_HOMEDIR)
	{
		const char *homedir = getenv("HOME");

		if (homedir == NULL) {
			const struct passwd *pw = getpwuid(getuid());
			if (pw != NULL) homedir = pw->pw_dir;
		}

		_path.personal_dir = str_fmt("%s" PATHSEP "%s", homedir, PERSONAL_DIR);
	}

#else /* not defined(USE_HOMEDIR) */

	_path.personal_dir = malloc(MAX_PATH);
	ttd_strlcpy(_path.personal_dir, PERSONAL_DIR, MAX_PATH);

	// check if absolute or relative path
	s = strchr(_path.personal_dir, '\\');

	// add absolute path
	if (s == NULL || _path.personal_dir != s) {
		getcwd(_path.personal_dir, MAX_PATH);
		s = strchr(_path.personal_dir, 0);
		*s++ = '\\';
		ttd_strlcpy(s, PERSONAL_DIR, MAX_PATH);
	}

#endif /* defined(USE_HOMEDIR) */

	s = strchr(_path.personal_dir, 0);

	// append a / ?
	if (s[-1] != '\\') strcpy(s, "\\");

	_path.save_dir = str_fmt("%ssave", _path.personal_dir);
	_path.autosave_dir = str_fmt("%s\\autosave", _path.save_dir);
	_path.scenario_dir = str_fmt("%sscenario", _path.personal_dir);
	_path.gm_dir = str_fmt("%sgm\\", _path.game_data_dir);
	_path.data_dir = str_fmt("%sdata\\", _path.game_data_dir);

	if (_config_file == NULL)
		_config_file = str_fmt("%sopenttd.cfg", _path.personal_dir);

	_highscore_file = str_fmt("%shs.dat", _path.personal_dir);
	_log_file = str_fmt("%sopenttd.log", _path.personal_dir);

#if defined CUSTOM_LANG_DIR
	// sets the search path for lng files to the custom one
	_path.lang_dir = malloc( MAX_PATH );
	ttd_strlcpy( _path.lang_dir, CUSTOM_LANG_DIR, MAX_PATH);
#else
	_path.lang_dir = str_fmt("%slang\\", _path.game_data_dir);
#endif

	// create necessary folders
	mkdir(_path.personal_dir);
	mkdir(_path.save_dir);
	mkdir(_path.autosave_dir);
	mkdir(_path.scenario_dir);
}

/**
 * Insert a chunk of text from the clipboard onto the textbuffer. Get TEXT clipboard
 * and append this up to the maximum length (either absolute or screenlength). If maxlength
 * is zero, we don't care about the screenlength but only about the physical length of the string
 * @param tb @Textbuf type to be changed
 * @return Return true on successfull change of Textbuf, or false otherwise
 */
bool InsertTextBufferClipboard(Textbuf *tb)
{
	HAB hab = 0;

	if (WinOpenClipbrd(hab))
	{
		const char* text = (const char*)WinQueryClipbrdData(hab, CF_TEXT);

		if (text != NULL)
		{
			uint length = 0;
			uint width = 0;
			const char* i;

			for (i = text; IsValidAsciiChar(*i); i++)
			{
				uint w;

				if (tb->length + length >= tb->maxlength - 1) break;

				w = GetCharacterWidth((byte)*i);
				if (tb->maxwidth != 0 && width + tb->width + w > tb->maxwidth) break;

				width += w;
				length++;
			}

			memmove(tb->buf + tb->caretpos + length, tb->buf + tb->caretpos, tb->length - tb->caretpos + 1);
			memcpy(tb->buf + tb->caretpos, text, length);
			tb->width += width;
			tb->caretxoffs += width;
			tb->length += length;
			tb->caretpos += length;

			WinCloseClipbrd(hab);
			return true;
		}

		WinCloseClipbrd(hab);
	}

	return false;
}

static TID thread1 = 0;

// The thread function must be declared and compiled using _Optlink linkage, apparently
// It seems to work, though :)

bool CreateOTTDThread(void *func, void *param)
{
	thread1 = _beginthread(func, NULL, 32768, param);

	if (thread1 == -1)
		return(false);

	return(true);
}

void JoinOTTDThread(void)
{
	if (thread1 == 0)
		return;

	DosWaitThread(&thread1, DCWW_WAIT);
}

void CSleep(int milliseconds)
{
	delay(milliseconds);
}

