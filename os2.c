#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "hal.h"

#include <direct.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <dos.h>

#include <os2.h>

#if defined(WITH_SDL)
#include <SDL.h>
#endif

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
	r = da->mtime < db->mtime ? -1 : 1;
	else
		r = stricmp(da->title[0] ? da->title : da->name, db->title[0] ? db->title : db->name);

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
	dir = opendir(_fios_path[0] ? _fios_path : "C:\\");
	if (dir != NULL) {
		while ((dirent = readdir(dir))) {
			sprintf (filename, "%s\\%s", _fios_path, dirent->d_name);
			if (!stat(filename, &sb)) {
				if (S_ISDIR(sb.st_mode)) {
					if (dirent->d_name[0] != '.') {
						fios = FiosAlloc();
						fios->mtime = 0;
						fios->type = FIOS_TYPE_DIR;
						fios->title[0] = 0;
						sprintf(fios->name, "%s\\ (Directory)", dirent->d_name);
					}
				}
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

	/*      Show savegame files
	 *      .SAV    OpenTTD saved game
	 *      .SS1    Transport Tycoon Deluxe preset game
	 *      .SV1    Transport Tycoon Deluxe (Patch) saved game
	 *      .SV2    Transport Tycoon Deluxe (Patch) saved 2-player game
	 */
	dir = opendir(_fios_path[0] ? _fios_path : "C:\\");
	if (dir != NULL) {
		while ((dirent = readdir(dir))) {
			sprintf (filename, "%s\\%s", _fios_path, dirent->d_name);
			if (!stat(filename, &sb)) {
				if (!S_ISDIR(sb.st_mode)) {
					char *t = strrchr(dirent->d_name, '.');
					if (t && !stricmp(t, ".sav")) { // OpenTTD
						*t = 0; // cut extension
						fios = FiosAlloc();
						fios->type = FIOS_TYPE_FILE;
						fios->mtime = sb.st_mtime;
						fios->title[0] = 0;
						ttd_strlcpy(fios->name, dirent->d_name, sizeof(fios->name));
					} else if (mode == SLD_LOAD_GAME || mode == SLD_LOAD_SCENARIO) {
						int ext = 0; // start of savegame extensions in _old_extensions[]
						if (t && ((ext++, !stricmp(t, ".ss1")) || (ext++, !stricmp(t, ".sv1")) || (ext++, !stricmp(t, ".sv2"))) ) { // TTDLX(Patch)
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

	qsort(_fios_items + sort_start, _fios_count - sort_start, sizeof(FiosItem), compare_FiosItems);

	// Drives
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
				fios->title[0] = disk + 'A'-1;
				fios->title[1] = ':';
				fios->title[2] = 0;
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

	// Show subdirectories first
	dir = opendir(_fios_path[0] ? _fios_path : "C:\\");
	if (dir != NULL) {
		while ((dirent = readdir(dir))) {
			sprintf (filename, "%s\\%s", _fios_path, dirent->d_name);
			if (!stat(filename, &sb)) {
				if (S_ISDIR(sb.st_mode)) {
					if (dirent->d_name[0] != '.') {
						fios = FiosAlloc();
						fios->mtime = 0;
						fios->type = FIOS_TYPE_DIR;
						fios->title[0] = 0;
						sprintf(fios->name, "%s\\ (Directory)", dirent->d_name);
					}
				}
			}
		}
		closedir(dir);
	}

	// this is where to start sorting
	sort_start = _fios_count;

	/*      Show scenario files
	 *      .SCN    OpenTTD style scenario file
	 *      .SV0    Transport Tycoon Deluxe (Patch) scenario
	 *      .SS0    Transport Tycoon Deluxe preset scenario
	 */
	dir = opendir(_fios_path[0] ? _fios_path : "C:\\");
	if (dir != NULL) {
		while ((dirent = readdir(dir))) {
			sprintf (filename, "%s\\%s", _fios_path, dirent->d_name);
			if (!stat(filename, &sb)) {
				if (!S_ISDIR(sb.st_mode)) {
					char *t = strrchr(dirent->d_name, '.');
					if (t && !stricmp(t, ".scn")) { // OpenTTD
						*t = 0; // cut extension
						fios = FiosAlloc();
						fios->type = FIOS_TYPE_SCENARIO;
						fios->mtime = sb.st_mtime;
						fios->title[0] = 0;
						ttd_strlcpy(fios->name, dirent->d_name, sizeof(fios->name)-3);
					} else if (mode == SLD_LOAD_GAME || mode == SLD_LOAD_SCENARIO || mode == SLD_NEW_GAME) {
						int ext = 3; // start of scenario extensions in _old_extensions[]
						if (t && ((ext++, !stricmp(t, ".sv0")) || (ext++, !stricmp(t, ".ss0"))) ) { // TTDLX(Patch)
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
				fios->title[0] = disk + 'A'-1;
				fios->title[1] = ':';
				fios->title[2] = 0;
			}
		}

		_dos_setdrive(save, &total);
	}

	*num = _fios_count;
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
		s = strrchr(path, '\\');
		if (s != NULL) *s = 0;
		break;

	case FIOS_TYPE_DIR:
		s = strchr((char*)item->name, '\\');
		if (s) *s = 0;
		while (*path) path++;
		*path++ = '\\';
		strcpy(path, item->name);
		break;

	case FIOS_TYPE_FILE:
		FiosMakeSavegameName(str_buffr, item->name);
		return str_buffr;

	case FIOS_TYPE_OLDFILE:
		sprintf(str_buffr, "%s\\%s.%s", _fios_path, item->name, _old_extensions[item->old_extension]);
		return str_buffr;

	case FIOS_TYPE_SCENARIO:
		sprintf(str_buffr, "%s\\%s.scn", path, item->name);
		return str_buffr;

	case FIOS_TYPE_OLD_SCENARIO:
		sprintf(str_buffr, "%s\\%s.%s", path, item->name, _old_extensions[item->old_extension]);
		return str_buffr;
	}

	return NULL;
}

// Get descriptive texts.
// Returns a path as well as a
//  string describing the path.
StringID FiosGetDescText(const char **path)
{
	struct diskfree_t free;
	char drive;
	
	*path = _fios_path[0] ? _fios_path : "C:\\";
	drive = 'B' - *path[0];

	_getdiskfree(drive, &free);
		
	SetDParam(0, free.avail_clusters * free.sectors_per_cluster * free.bytes_per_sector);
	return STR_4005_BYTES_FREE;
}

void FiosMakeSavegameName(char *buf, const char *name)
{
	if(_game_mode==GM_EDITOR)
		sprintf(buf, "%s\\%s.scn", _fios_path, name);
	else
		sprintf(buf, "%s\\%s.sav", _fios_path, name);
}

void FiosDelete(const char *name)
{
	char *path = str_buffr;
	FiosMakeSavegameName(path, name);
	unlink(path);
}

const DriverDesc _video_driver_descs[] = {
	{"null",	"Null Video Driver",    &_null_video_driver,    0},
#if defined(WITH_SDL)
	{ "sdl",	"SDL Video Driver",	     &_sdl_video_driver,	     1},
#endif
	{ "dedicated", "Dedicated Video Driver", &_dedicated_video_driver, 0},
	{ NULL,	 NULL,								   NULL,								   0}
};

const DriverDesc _sound_driver_descs[] = {
	{"null",	"Null Sound Driver",    &_null_sound_driver,	    0},
#if defined(WITH_SDL)
	{ "sdl",	"SDL Sound Driver",	     &_sdl_sound_driver,		     1},
#endif
	{ NULL,	 NULL,								   NULL,									   0}
};

const DriverDesc _music_driver_descs[] = {
	{   "null",     "Null Music Driver",	    &_null_music_driver,	    0},
	{     NULL,     NULL,									   NULL,									   0}
};

/* GetOSVersion returns the minimal required version of OS to be able to use that driver.
	 Not needed for *nix. */
byte GetOSVersion()
{
	return 2;  // any arbitrary number bigger then 0
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
		*s = 0;
		chdir(exe);
		*s = '\\';
	}
}

void ShowInfo(const char *str)
{
	puts(str);
}

void ShowOSErrorBox(const char *buf)
{
	WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, buf, "OpenTTD", 263, MB_OK | MB_APPLMODAL | MB_MOVEABLE | MB_ERROR);
// TODO: FIX, doesn't always appear
}

int CDECL main(int argc, char* argv[])
{
	// change the working directory to enable doubleclicking in UIs
	ChangeWorkingDirectory(argv[0]);

	_random_seeds[0][1] = _random_seeds[0][0] = time(NULL);


	return ttd_main(argc, argv);
}

void DeterminePaths()
{
	char *s;

	_path.game_data_dir = malloc( MAX_PATH );
	ttd_strlcpy(_path.game_data_dir, GAME_DATA_DIR, MAX_PATH);
	#if defined SECOND_DATA_DIR
	_path.second_data_dir = malloc( MAX_PATH );
	ttd_strlcpy( _path.second_data_dir, SECOND_DATA_DIR, MAX_PATH);
	#endif

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
	ttd_strlcpy(_path.personal_dir, PERSONAL_DIR, MAX_PATH);

	// check if absolute or relative path
	s = strchr(_path.personal_dir, '\\');

	// add absolute path
	if (s==NULL || _path.personal_dir != s) {
		getcwd(_path.personal_dir, MAX_PATH);
		s = strchr(_path.personal_dir, 0);
		*s++ = '\\';
		ttd_strlcpy(s, PERSONAL_DIR, MAX_PATH);
	}

#endif /* defined(USE_HOMEDIR) */

	s = strchr(_path.personal_dir, 0);

	// append a / ?
	if (s[-1] != '\\') { s[0] = '\\'; s[1] = 0; }

	_path.save_dir = str_fmt("%ssave", _path.personal_dir);
	_path.autosave_dir = str_fmt("%s\\autosave", _path.save_dir);
	_path.scenario_dir = str_fmt("%sscenario", _path.personal_dir);
	_path.gm_dir = str_fmt("%sgm\\", _path.game_data_dir);
	_path.data_dir = str_fmt("%sdata\\", _path.game_data_dir);
	_config_file = str_fmt("%sopenttd.cfg", _path.personal_dir);
	
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

