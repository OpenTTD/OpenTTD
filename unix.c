#include "stdafx.h"
#include "ttd.h"
#include "hal.h"

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>

#if defined(__linux__)
#include <sys/statvfs.h>
#endif


#if defined(WITH_SDL)
#include <SDL.h>
#endif

#ifdef __MORPHOS__
#include <exec/types.h>
ULONG __stack = (1024*1024)*2; // maybe not that much is needed actually ;)
#endif /* __MORPHOS__ */

static char *_fios_path;
static char *_fios_save_path;
static char *_fios_scn_path;
static FiosItem *_fios_items;
static int _fios_count, _fios_alloc;

static FiosItem *FiosAlloc()
{
	if (_fios_count == _fios_alloc) {
		_fios_alloc += 256;
		_fios_items = realloc(_fios_items, _fios_alloc * sizeof(FiosItem));
	}
	return &_fios_items[_fios_count++];
}

int compare_FiosItems (const void *a, const void *b) {
	const FiosItem *da = (const FiosItem *) a;
	const FiosItem *db = (const FiosItem *) b;
	int r;

	if (_savegame_sort_order < 2) // sort by date
    r = da->mtime < db->mtime ? 1 : -1;
	else	
		r = strcmp(da->title[0] ? da->title : da->name, db->title[0] ? db->title : db->name);

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

	if(_game_mode==GM_EDITOR)
		_fios_path = _fios_scn_path;
	else
		_fios_path = _fios_save_path;
	
	// Parent directory, only if not in root already.
	if (_fios_path[1] != 0) {
		fios = FiosAlloc();
		fios->type = FIOS_TYPE_PARENT;
		fios->mtime = 0;
		sprintf(fios->title, ".. (Parent directory)");
	}

	// Show subdirectories first
	dir = opendir(_fios_path[0] ? _fios_path : "/");
	if (dir != NULL) {
		while ((dirent = readdir(dir))) {
			sprintf (filename, "%s/%s", _fios_path, dirent->d_name);
			if (!stat(filename, &sb)) {
				if (S_ISDIR(sb.st_mode)) {
					if (dirent->d_name[0] != '.') {
						fios = FiosAlloc();
						fios->mtime = 0;
						fios->type = FIOS_TYPE_DIR;
						fios->title[0] = 0;
						sprintf(fios->name, "%s/ (Directory)", dirent->d_name);
					}
				}
			}
		}
		closedir(dir);
	}

	// this is where to start sorting
	sort_start = _fios_count;

	/*	Show savegame files
	 *	.SAV	OpenTTD saved game
	 *	.SS1	Transport Tycoon Deluxe preset game
	 *	.SV1	Transport Tycoon Deluxe (Patch) saved game
	 *	.SV2	Transport Tycoon Deluxe (Patch) saved 2-player game
	 */
	dir = opendir(_fios_path[0] ? _fios_path : "/");
	if (dir != NULL) {
		while ((dirent = readdir(dir))) {
			sprintf (filename, "%s/%s", _fios_path, dirent->d_name);
			if (!stat(filename, &sb)) {
				if (!S_ISDIR(sb.st_mode)) {
					char *t = strrchr(dirent->d_name, '.');
					if (t && !strcmp(t, ".sav")) { // OpenTTD
						*t = 0; // cut extension
						fios = FiosAlloc();
						fios->type = FIOS_TYPE_FILE;
						fios->mtime = sb.st_mtime;
						fios->title[0] = 0;
						ttd_strlcpy(fios->name, dirent->d_name, sizeof(fios->name));
					} else if (mode == SLD_LOAD_GAME || mode == SLD_LOAD_SCENARIO) {
						int ext = 0; // start of savegame extensions in _old_extensions[]
						if (t && ((ext++, !strcmp(t, ".ss1")) || (ext++, !strcmp(t, ".sv1")) || (ext++, !strcmp(t, ".sv2"))) ) { // TTDLX(Patch)
							*t = 0; // cut extension
							fios = FiosAlloc();
							fios->old_extension = ext-1;
							fios->type = FIOS_TYPE_OLDFILE;
							fios->mtime = sb.st_mtime;
							ttd_strlcpy(fios->name, dirent->d_name, sizeof(fios->name));
							GetOldSaveGameName(fios->title, filename);
						}
					}
				}
			}
		}
		closedir(dir);
	}

	*num = _fios_count;
	qsort(_fios_items + sort_start, _fios_count - sort_start, sizeof(FiosItem), compare_FiosItems);
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

	// Show subdirectories first
	dir = opendir(_fios_path[0] ? _fios_path : "/");
	if (dir != NULL) {
		while ((dirent = readdir(dir))) {
			sprintf (filename, "%s/%s", _fios_path, dirent->d_name);
			if (!stat(filename, &sb)) {
				if (S_ISDIR(sb.st_mode)) {
					if (dirent->d_name[0] != '.') {
						fios = FiosAlloc();
						fios->mtime = 0;
						fios->type = FIOS_TYPE_DIR;
						fios->title[0] = 0;
						sprintf(fios->name, "%s/ (Directory)", dirent->d_name);
					}
				}
			}
		}
		closedir(dir);
	}

	// this is where to start sorting
	sort_start = _fios_count;

	/*	Show scenario files
	 *	.SCN	OpenTTD style scenario file
	 *	.SV0	Transport Tycoon Deluxe (Patch) scenario
	 *	.SS0	Transport Tycoon Deluxe preset scenario
	 */
	dir = opendir(_fios_path[0] ? _fios_path : "/");
	if (dir != NULL) {
		while ((dirent = readdir(dir))) {
			sprintf (filename, "%s/%s", _fios_path, dirent->d_name);
			if (!stat(filename, &sb)) {
				if (!S_ISDIR(sb.st_mode)) {
					char *t = strrchr(dirent->d_name, '.');
					if (t && !strcmp(t, ".scn")) { // OpenTTD
						*t = 0; // cut extension
						fios = FiosAlloc();
						fios->type = FIOS_TYPE_SCENARIO;
						fios->mtime = sb.st_mtime;
						fios->title[0] = 0;
						ttd_strlcpy(fios->name, dirent->d_name, sizeof(fios->name)-3);
					} else if (mode == SLD_LOAD_GAME || mode == SLD_LOAD_SCENARIO || mode == SLD_NEW_GAME) {
						int ext = 3; // start of scenario extensions in _old_extensions[]
						if (t && ((ext++, !strcmp(t, ".sv0")) || (ext++, !strcmp(t, ".ss0"))) ) { // TTDLX(Patch)
							*t = 0; // cut extension
							fios = FiosAlloc();
							fios->old_extension = ext-1;
							fios->type = FIOS_TYPE_OLD_SCENARIO;
							fios->mtime = sb.st_mtime;
							GetOldScenarioGameName(fios->title, filename);
							ttd_strlcpy(fios->name, dirent->d_name, sizeof(fios->name)-3);
						}
					}
				}
			}
		}
		closedir(dir);
	}

	*num = _fios_count;
	qsort(_fios_items + sort_start, _fios_count - sort_start, sizeof(FiosItem), compare_FiosItems);
	return _fios_items;
}


// Free the list of savegames
void FiosFreeSavegameList()
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

	switch(item->type) {
	case FIOS_TYPE_PARENT:
		s = strrchr(path, '/');
		if (s != NULL) *s = 0;
		break;

	case FIOS_TYPE_DIR:
		s = strchr((char*)item->name, '/');
		if (s) *s = 0;
		while (*path) path++;
		*path++ = '/';
		strcpy(path, item->name);
		break;

	case FIOS_TYPE_FILE:
		FiosMakeSavegameName(str_buffr, item->name);
		return str_buffr;

	case FIOS_TYPE_OLDFILE:
		sprintf(str_buffr, "%s/%s.%s", _fios_path, item->name, _old_extensions[item->old_extension]);
		return str_buffr;
	
	case FIOS_TYPE_SCENARIO:
		sprintf(str_buffr, "%s/%s.scn", path, item->name);
		return str_buffr;
	
	case FIOS_TYPE_OLD_SCENARIO:
		sprintf(str_buffr, "%s/%s.%s", path, item->name, _old_extensions[item->old_extension]);
		return str_buffr;
	}

	return NULL;
}

// Get descriptive texts.
// Returns a path as well as a
//  string describing the path.
StringID FiosGetDescText(char **path)
{
	*path = _fios_path[0] ? _fios_path : "/";

#if defined(__linux__)
	{
	struct statvfs s;

	if (statvfs(*path, &s) == 0)
	{
		uint64 tot = (uint64)s.f_bsize * s.f_bavail;
		SET_DPARAM32(0, (uint32)(tot >> 20));
		return STR_4005_BYTES_FREE; 
	}
	else
		return STR_4006_UNABLE_TO_READ_DRIVE;
	}
#else
	SET_DPARAM32(0, 0);
	return STR_4005_BYTES_FREE; 
#endif
}

void FiosMakeSavegameName(char *buf, const char *name)
{
	if(_game_mode==GM_EDITOR)
		sprintf(buf, "%s/%s.scn", _fios_path, name);
	else
		sprintf(buf, "%s/%s.sav", _fios_path, name);
}

void FiosDelete(const char *name)
{
	char *path = str_buffr;
	FiosMakeSavegameName(path, name);
	unlink(path);
}

const DriverDesc _video_driver_descs[] = {
	{"null", "Null Video Driver",				&_null_video_driver,	0},
#if defined(WITH_SDL)
	{"sdl", "SDL Video Driver",					&_sdl_video_driver,	1},
#endif
	{NULL}
};

const DriverDesc _sound_driver_descs[] = {
	{"null", "Null Sound Driver",			&_null_sound_driver,	0},
#if defined(WITH_SDL)
	{"sdl", "SDL Sound Driver",				&_sdl_sound_driver,	1},
#endif
	{NULL}
};

#if defined(__APPLE__)
#define EXTMIDI_PRI 2
#else
#define EXTMIDI_PRI 0
#endif

const DriverDesc _music_driver_descs[] = {
#ifndef __BEOS__
#ifndef __MORPHOS__
// MorphOS have no music support
	{"extmidi", "External MIDI Driver",	&_extmidi_music_driver,	EXTMIDI_PRI},
#endif
#endif
#ifdef __BEOS__
	{"bemidi", "BeOS MIDI Driver", &_bemidi_music_driver, 1},
#endif	
	{"null", "Null Music Driver",		&_null_music_driver,	1},
	{NULL}
};

bool FileExists(const char *filename)
{
	return access(filename, 0) == 0;	
}

static int LanguageCompareFunc(const void *a, const void *b)
{
	return strcmp(*(char**)a, *(char**)b);
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

void ChangeWorkingDirectory(char *exe)
{
	char *s = strrchr(exe, '/');
	if (s != NULL) {
		*s = 0;
		chdir(exe);
		*s = '/';
	}
}

void ShowInfo(const char *str)
{
	puts(str);
}

void ShowOSErrorBox(const char *buf)
{
	fprintf(stderr, "\033[1;31mError: %s\033[0;39m\n", buf);
	
#if defined(__APPLE__)
	// this opens the crash log opener script
	system("./Crash_Log_Opener.app");
#endif
}

int CDECL main(int argc, char* argv[])
{
	// change the working directory to enable doubleclicking in UIs
#if defined(__BEOS__)
	ChangeWorkingDirectory(argv[0]);
#endif
#if defined(__linux__)
	ChangeWorkingDirectory(argv[0]);
#endif

	_random_seed_2 = _random_seed_1 = time(NULL);


	return ttd_main(argc, argv);
}

void DeterminePaths()
{
	char *s;

	_path.game_data_dir = malloc( MAX_PATH );
	strcpy(_path.game_data_dir, GAME_DATA_DIR);

#if defined(USE_HOMEDIR)
	{
		char *homedir; 
		homedir = getenv("HOME");
		
		if(!homedir) {
			struct passwd *pw = getpwuid(getuid());
			if (pw) homedir = pw->pw_dir;
		}
		
		_path.personal_dir = str_fmt("%s" PATHSEP "%s", homedir, PERSONAL_DIR);
	}
	
#else /* not defined(USE_HOMEDIR) */

	_path.personal_dir = malloc( MAX_PATH );
	strcpy(_path.personal_dir, PERSONAL_DIR);
	
	// check if absolute or relative path
	s = strchr(_path.personal_dir, '/');
	
	// add absolute path
	if (s==NULL || _path.personal_dir != s) {
		getcwd(_path.personal_dir, MAX_PATH);
		s = strchr(_path.personal_dir, 0);
		*s++ = '/';
		strcpy(s, PERSONAL_DIR);
	}
	
#endif /* defined(USE_HOMEDIR) */

	s = strchr(_path.personal_dir, 0);
	
	// append a / ?
	if (s[-1] != '/') { s[0] = '/'; s[1] = 0; }
	
	_path.save_dir = str_fmt("%ssave", _path.personal_dir);
	_path.autosave_dir = str_fmt("%s/autosave", _path.save_dir);
	_path.scenario_dir = str_fmt("%sscenario", _path.personal_dir);
	_path.gm_dir = str_fmt("%sgm/", _path.game_data_dir);
	_path.data_dir = str_fmt("%sdata/", _path.game_data_dir);
	_path.lang_dir = str_fmt("%slang/", _path.game_data_dir);
	
	_config_file = str_fmt("%sopenttd.cfg", _path.personal_dir);
	
	// make (auto)save and scenario folder
	mkdir(_path.save_dir, 0755);
	mkdir(_path.autosave_dir, 0755);
	mkdir(_path.scenario_dir, 0755);
}

