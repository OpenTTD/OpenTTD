/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "window.h"
#include "string.h"
#include "table/strings.h"
#include "hal.h"
#include "variables.h"

#include "music/bemidi.h"
#include "music/extmidi.h"
#include "music/null_m.h"

#include "sound/null_s.h"
#include "sound/sdl_s.h"

#include "video/dedicated_v.h"
#include "video/null_v.h"
#include "video/sdl_v.h"

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <signal.h>
#if !defined(__MORPHOS__) && !defined(__AMIGA__)
 #include <pthread.h>
#endif

#if (defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L) || defined(__GLIBC__)
	#define HAS_STATVFS
#endif

#ifdef HAS_STATVFS
#include <sys/statvfs.h>
#endif


#if defined(WITH_SDL)
#include <SDL.h>
#endif

#ifdef __MORPHOS__
#include <exec/types.h>
ULONG __stack = (1024*1024)*2; // maybe not that much is needed actually ;)
#endif /* __MORPHOS__ */

#ifdef __AMIGA__
#warning add stack symbol to avoid that user needs to set stack manually (tokai)
// ULONG __stack =
#endif

#if defined(__APPLE__)
#include "os/macosx/macos.h"
#endif

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

	_fios_path = _fios_save_path;

	// Parent directory, only if not in root already.
	if (_fios_path[1] != '\0') {
		fios = FiosAlloc();
		fios->type = FIOS_TYPE_PARENT;
		fios->mtime = 0;
		strcpy(fios->name, "..");
		strcpy(fios->title, ".. (Parent directory)");
	}

	// Show subdirectories first
	dir = opendir(_fios_path[0] != '\0' ? _fios_path : "/");
	if (dir != NULL) {
		while ((dirent = readdir(dir)) != NULL) {
			snprintf(filename, lengthof(filename), "%s/%s",
				_fios_path, dirent->d_name);
			if (!stat(filename, &sb) && S_ISDIR(sb.st_mode) &&
					dirent->d_name[0] != '.') {
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_DIR;
				fios->mtime = 0;
				ttd_strlcpy(fios->name, dirent->d_name, lengthof(fios->name));
				snprintf(fios->title, lengthof(fios->title),
					"%s/ (Directory)", dirent->d_name);
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
	dir = opendir(_fios_path[0] != '\0' ? _fios_path : "/");
	if (dir != NULL) {
		while ((dirent = readdir(dir)) != NULL) {
			char *t;

			snprintf(filename, lengthof(filename), "%s/%s",
				_fios_path, dirent->d_name);
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
	*num = _fios_count;
	return _fios_items;
}

// Get a list of scenarios
// FIXME: Gross code duplication with FiosGetSavegameList()
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
	if (_fios_path[1] != '\0' && mode != SLD_NEW_GAME) {
		fios = FiosAlloc();
		fios->type = FIOS_TYPE_PARENT;
		fios->mtime = 0;
		strcpy(fios->title, ".. (Parent directory)");
	}

	// Show subdirectories first
	dir = opendir(_fios_path[0] ? _fios_path : "/");
	if (dir != NULL) {
		while ((dirent = readdir(dir)) != NULL) {
			snprintf(filename, lengthof(filename), "%s/%s",
				_fios_path, dirent->d_name);
			if (!stat(filename, &sb) && S_ISDIR(sb.st_mode) &&
					dirent->d_name[0] != '.') {
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_DIR;
				fios->mtime = 0;
				ttd_strlcpy(fios->name, dirent->d_name, lengthof(fios->name));
				snprintf(fios->title, lengthof(fios->title), "%s/ (Directory)", dirent->d_name);
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
	dir = opendir(_fios_path[0] ? _fios_path : "/");
	if (dir != NULL) {
		while ((dirent = readdir(dir)) != NULL) {
			char *t;

			snprintf(filename, lengthof(filename), "%s/%s", _fios_path, dirent->d_name);
			if (stat(filename, &sb) || S_ISDIR(sb.st_mode)) continue;

			t = strrchr(dirent->d_name, '.');
			if (t != NULL && strcasecmp(t, ".scn") == 0) { // OpenTTD
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_SCENARIO;
				fios->mtime = sb.st_mtime;
				ttd_strlcpy(fios->name, dirent->d_name, lengthof(fios->name));

				*t  = '\0'; // strip extension
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
		case FIOS_TYPE_PARENT:
			s = strrchr(path, '/');
			if (s != NULL) *s = '\0';
			break;

		case FIOS_TYPE_DIR:
			s = strchr(item->name, '/');
			if (s != NULL) *s = '\0';
			strcat(path, "/");
			strcat(path, item->name);
			break;

		case FIOS_TYPE_FILE:
		case FIOS_TYPE_OLDFILE:
		case FIOS_TYPE_SCENARIO:
		case FIOS_TYPE_OLD_SCENARIO: {
			static char str_buffr[512];

			sprintf(str_buffr, "%s/%s", path, item->name);
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
	uint32 free = 0;
	*path = _fios_path[0] != '\0' ? _fios_path : "/";

#ifdef HAS_STATVFS
	{
		struct statvfs s;

		if (statvfs(*path, &s) == 0) {
			free = (uint64)s.f_frsize * s.f_bavail >> 20;
		} else
			return STR_4006_UNABLE_TO_READ_DRIVE;
	}
#endif
	if (tot != NULL) *tot = free;
	return STR_4005_BYTES_FREE;
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

	sprintf(buf, "%s/%s%s", _fios_path, name, extension);
}

void FiosDelete(const char *name)
{
	char path[512];

	snprintf(path, lengthof(path), "%s/%s", _fios_path, name);
	unlink(path);
}

const DriverDesc _video_driver_descs[] = {
	{"null",	"Null Video Driver",	&_null_video_driver,	0},
#if defined(WITH_SDL)
	{ "sdl",	"SDL Video Driver",		&_sdl_video_driver,		1},
#endif
	{ "dedicated", "Dedicated Video Driver", &_dedicated_video_driver, 0},
	{ NULL,		NULL,									NULL,									0}
};

const DriverDesc _sound_driver_descs[] = {
	{"null",	"Null Sound Driver",	&_null_sound_driver,		0},
#if defined(WITH_SDL)
	{ "sdl",	"SDL Sound Driver",		&_sdl_sound_driver,			1},
#endif
	{ NULL,		NULL,									NULL,										0}
};

#if defined(__APPLE__)
#define EXTMIDI_PRI 2
#else
#define EXTMIDI_PRI 0
#endif

const DriverDesc _music_driver_descs[] = {
#ifndef __BEOS__
#if !defined(__MORPHOS__) && !defined(__AMIGA__)
// MorphOS and AmigaOS have no music support
	{"extmidi",	"External MIDI Driver",	&_extmidi_music_driver,	EXTMIDI_PRI},
#endif
#endif
#ifdef __BEOS__
	{ "bemidi",	"BeOS MIDI Driver",			&_bemidi_music_driver,	1},
#endif
	{   "null",	"Null Music Driver",		&_null_music_driver,		1},
	{     NULL,	NULL,										NULL,										0}
};

/* GetOSVersion returns the minimal required version of OS to be able to use that driver.
	 Not needed for *nix. */
byte GetOSVersion(void)
{
	return 2;  // any arbitrary number bigger than 0
				// numbers lower than 2 breaks default music selection on mac
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
		while ((dirent = readdir(dir)) != NULL) {
			char *t = strrchr(dirent->d_name, '.');

			if (t != NULL && strcmp(t, ".lng") == 0) {
				languages[num++] = strdup(dirent->d_name);
				if (num == max) break;
			}
		}
		closedir(dir);
	}

	qsort(languages, num, sizeof(char*), LanguageCompareFunc);
	return num;
}

#if defined(__BEOS__) || defined(__linux__)
static void ChangeWorkingDirectory(char *exe)
{
	char *s = strrchr(exe, '/');
	if (s != NULL) {
		*s = '\0';
		chdir(exe);
		*s = '/';
	}
}
#endif

void ShowInfo(const char *str)
{
	puts(str);
}

void ShowOSErrorBox(const char *buf)
{
#if defined(__APPLE__)
	// this creates an NSAlertPanel with the contents of 'buf'
	// this is the native and nicest way to do this on OSX
	ShowMacDialog( buf, "See readme for more info\nMost likely you are missing files from the original TTD", "Quit" );
#else
	// all systems, but OSX
	fprintf(stderr, "\033[1;31mError: %s\033[0;39m\n", buf);
#endif
}

int CDECL main(int argc, char* argv[])
{
	// change the working directory to enable doubleclicking in UIs
#if defined(__BEOS__) || defined(__linux__)
	ChangeWorkingDirectory(argv[0]);
#endif

	_random_seeds[0][1] = _random_seeds[0][0] = time(NULL);
	SeedMT(_random_seeds[0][1]);

	signal(SIGPIPE, SIG_IGN);

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
	s = strchr(_path.personal_dir, '/');

	// add absolute path
	if (s == NULL || _path.personal_dir != s) {
		getcwd(_path.personal_dir, MAX_PATH);
		s = strchr(_path.personal_dir, 0);
		*s++ = '/';
		ttd_strlcpy(s, PERSONAL_DIR, MAX_PATH);
	}

#endif /* defined(USE_HOMEDIR) */

	s = strchr(_path.personal_dir, 0);

	// append a / ?
	if (s[-1] != '/') strcpy(s, "/");

	_path.save_dir = str_fmt("%ssave", _path.personal_dir);
	_path.autosave_dir = str_fmt("%s/autosave", _path.save_dir);
	_path.scenario_dir = str_fmt("%sscenario", _path.personal_dir);
	_path.gm_dir = str_fmt("%sgm/", _path.game_data_dir);
	_path.data_dir = str_fmt("%sdata/", _path.game_data_dir);

	if (_config_file == NULL)
		_config_file = str_fmt("%sopenttd.cfg", _path.personal_dir);

	_highscore_file = str_fmt("%shs.dat", _path.personal_dir);
	_log_file = str_fmt("%sopenttd.log", _path.personal_dir);

#if defined CUSTOM_LANG_DIR
	// sets the search path for lng files to the custom one
	_path.lang_dir = malloc( MAX_PATH );
	ttd_strlcpy( _path.lang_dir, CUSTOM_LANG_DIR, MAX_PATH);
#else
	_path.lang_dir = str_fmt("%slang/", _path.game_data_dir);
#endif

	// create necessary folders
	mkdir(_path.personal_dir, 0755);
	mkdir(_path.save_dir, 0755);
	mkdir(_path.autosave_dir, 0755);
	mkdir(_path.scenario_dir, 0755);
}

bool InsertTextBufferClipboard(Textbuf *tb)
{
	return false;
}

/** Dummy stubs as MorphOS/ AmigaOS does not really
 *  know about a thread concept nor has a working libpthread */
#if defined(__MORPHOS__) || defined(__AMIGA__)
 typedef int pthread_t;
 #define pthread_create(thread, attr, function, arg) (true)
 #define pthread_join(thread, retval)
#endif

static pthread_t thread1 = 0;

bool CreateOTTDThread(void *func, void *param)
{
	return pthread_create(&thread1, NULL, func, param) == 0;
}

void CloseOTTDThread(void) {return;}

void JoinOTTDThread(void)
{
	if (thread1 == 0) return;

	pthread_join(thread1, NULL);
}



#ifdef ENABLE_NETWORK

// multi os compatible sleep function

#ifdef __AMIGA__
// usleep() implementation
#	include <devices/timer.h>
#	include <dos/dos.h>

	extern struct Device      *TimerBase    = NULL;
	extern struct MsgPort     *TimerPort    = NULL;
	extern struct timerequest *TimerRequest = NULL;
#endif // __AMIGA__

void CSleep(int milliseconds)
{
	#if !defined(__BEOS__) && !defined(__AMIGA__)
		usleep(milliseconds * 1000);
	#endif
	#ifdef __BEOS__
		snooze(milliseconds * 1000);
	#endif
	#if defined(__AMIGA__)
	{
		ULONG signals;
		ULONG TimerSigBit = 1 << TimerPort->mp_SigBit;

		// send IORequest
		TimerRequest->tr_node.io_Command = TR_ADDREQUEST;
		TimerRequest->tr_time.tv_secs    = (milliseconds * 1000) / 1000000;
		TimerRequest->tr_time.tv_micro   = (milliseconds * 1000) % 1000000;
		SendIO((struct IORequest *)TimerRequest);

		if (!((signals = Wait(TimerSigBit | SIGBREAKF_CTRL_C)) & TimerSigBit) ) {
			AbortIO((struct IORequest *)TimerRequest);
		}
		WaitIO((struct IORequest *)TimerRequest);
	}
	#endif // __AMIGA__
}

#endif /* ENABLE_NETWORK */

