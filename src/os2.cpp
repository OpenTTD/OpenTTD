/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "string.h"
#include "table/strings.h"
#include "gfx.h"
#include "gui.h"
#include "functions.h"
#include "macros.h"
#include "fios.h"

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#ifndef __INNOTEK_LIBC__
	#include <dos.h>
#endif

#define INCL_WIN
#define INCL_WINCLIPBOARD

#include <os2.h>
#ifndef __INNOTEK_LIBC__
	#include <i86.h>
#endif

bool FiosIsRoot(const char *file)
{
	return file[3] == '\0';
}

void FiosGetDrives()
{
	unsigned disk, disk2, save, total;

#ifndef __INNOTEK_LIBC__
	_dos_getdrive(&save); // save original drive
#else
	save = _getdrive(); // save original drive
	total = 'z';
#endif

	/* get an available drive letter */
#ifndef __INNOTEK_LIBC__
	for (disk = 1;; disk++) {
		_dos_setdrive(disk, &total);
#else
	for (disk = 'A';; disk++) {
		_chdrive(disk);
#endif
		if (disk >= total) return;

#ifndef __INNOTEK_LIBC__
		_dos_getdrive(&disk2);
#else
		disk2 = _getdrive();
#endif

		if (disk == disk2) {
			FiosItem *fios = FiosAlloc();
			fios->type = FIOS_TYPE_DRIVE;
			fios->mtime = 0;
#ifndef __INNOTEK_LIBC__
			snprintf(fios->name, lengthof(fios->name),  "%c:", 'A' + disk - 1);
#else
			snprintf(fios->name, lengthof(fios->name),  "%c:", disk);
#endif
			ttd_strlcpy(fios->title, fios->name, lengthof(fios->title));
		}
	}

	/* Restore the original drive */
#ifndef __INNOTEK_LIBC__
	_dos_setdrive(save, &total);
#else
	_chdrive(save);
#endif
}

bool FiosGetDiskFreeSpace(const char *path, uint32 *tot)
{
#ifndef __INNOTEK_LIBC__
	struct diskfree_t free;
	char drive = path[0] - 'A' + 1;

	if (tot != NULL && _getdiskfree(drive, &free) == 0) {
		*tot = free.avail_clusters * free.sectors_per_cluster * free.bytes_per_sector;
		return true;
	}

	return false;
#else
	uint32 free = 0;

#ifdef HAS_STATVFS
	{
		struct statvfs s;

		if (statvfs(path, &s) != 0) return false;
		free = (uint64)s.f_frsize * s.f_bavail >> 20;
	}
#endif
	if (tot != NULL) *tot = free;
	return true;
#endif
}

bool FiosIsValidFile(const char *path, const struct dirent *ent, struct stat *sb)
{
	char filename[MAX_PATH];

	snprintf(filename, lengthof(filename), "%s" PATHSEP "%s", path, ent->d_name);
	return stat(filename, sb) == 0;
}

bool FiosIsHiddenFile(const struct dirent *ent)
{
	return ent->d_name[0] == '.';
}


static void ChangeWorkingDirectory(char *exe)
{
	char *s = strrchr(exe, PATHSEPCHAR);

	if (s != NULL) {
		*s = '\0';
		chdir(exe);
		*s = PATHSEPCHAR;
	}
}

void ShowInfo(const unsigned char *str)
{
	HAB hab;
	HMQ hmq;
	ULONG rc;

	// init PM env.
	hmq = WinCreateMsgQueue((hab = WinInitialize(0)), 0);

	// display the box
	rc = WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, str, (const unsigned char *)"OpenTTD", 0, MB_OK | MB_MOVEABLE | MB_INFORMATION);

	// terminate PM env.
	WinDestroyMsgQueue(hmq);
	WinTerminate(hab);
}

void ShowOSErrorBox(const unsigned char *buf)
{
	HAB hab;
	HMQ hmq;
	ULONG rc;

	// init PM env.
	hmq = WinCreateMsgQueue((hab = WinInitialize(0)), 0);

	// display the box
	rc = WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, buf, (const unsigned char *)"OpenTTD", 0, MB_OK | MB_MOVEABLE | MB_ERROR);

	// terminate PM env.
	WinDestroyMsgQueue(hmq);
	WinTerminate(hab);
}

int CDECL main(int argc, char* argv[])
{
	// change the working directory to enable doubleclicking in UIs
	ChangeWorkingDirectory(argv[0]);

	_random_seeds[1][1] = _random_seeds[1][0] = _random_seeds[0][1] = _random_seeds[0][0] = time(NULL);

	return ttd_main(argc, argv);
}

void DeterminePaths()
{
	char *s;

	_paths.game_data_dir = (char *)malloc(MAX_PATH);
	ttd_strlcpy(_paths.game_data_dir, GAME_DATA_DIR, MAX_PATH);
#if defined SECOND_DATA_DIR
	_paths.second_data_dir = malloc(MAX_PATH);
	ttd_strlcpy(_paths.second_data_dir, SECOND_DATA_DIR, MAX_PATH);
#endif

#if defined(USE_HOMEDIR)
	{
		const char *homedir = getenv("HOME");

		if (homedir == NULL) {
			const struct passwd *pw = getpwuid(getuid());
			if (pw != NULL) homedir = pw->pw_dir;
		}

		_paths.personal_dir = str_fmt("%s" PATHSEP "%s", homedir, PERSONAL_DIR);
	}

#else /* not defined(USE_HOMEDIR) */

	_paths.personal_dir = (char *)malloc(MAX_PATH);
	ttd_strlcpy(_paths.personal_dir, PERSONAL_DIR, MAX_PATH);

	// check if absolute or relative path
	s = strchr(_paths.personal_dir, PATHSEPCHAR);

	// add absolute path
	if (s == NULL || _paths.personal_dir != s) {
		getcwd(_paths.personal_dir, MAX_PATH);
		s = strchr(_paths.personal_dir, 0);
		*s++ = PATHSEPCHAR;
		ttd_strlcpy(s, PERSONAL_DIR, MAX_PATH);
	}

#endif /* defined(USE_HOMEDIR) */

	s = strchr(_paths.personal_dir, 0);

	// append a / ?
	if (s[-1] != PATHSEPCHAR) strcpy(s, PATHSEP);

	_paths.save_dir = str_fmt("%ssave", _paths.personal_dir);
	_paths.autosave_dir = str_fmt("%s" PATHSEP "autosave", _paths.save_dir);
	_paths.scenario_dir = str_fmt("%sscenario", _paths.personal_dir);
	_paths.heightmap_dir = str_fmt("%sscenario" PATHSEP "heightmap", _paths.personal_dir);
	_paths.gm_dir = str_fmt("%sgm" PATHSEP, _paths.game_data_dir);
	_paths.data_dir = str_fmt("%sdata" PATHSEP, _paths.game_data_dir);

	if (_config_file == NULL)
		_config_file = str_fmt("%sopenttd.cfg", _paths.personal_dir);

	_highscore_file = str_fmt("%shs.dat", _paths.personal_dir);
	_log_file = str_fmt("%sopenttd.log", _paths.personal_dir);

#if defined CUSTOM_LANG_DIR
	// sets the search path for lng files to the custom one
	_paths.lang_dir = malloc( MAX_PATH );
	ttd_strlcpy( _paths.lang_dir, CUSTOM_LANG_DIR, MAX_PATH);
#else
	_paths.lang_dir = str_fmt("%slang" PATHSEP, _paths.game_data_dir);
#endif

	// create necessary folders
#ifndef __INNOTEK_LIBC__
	mkdir(_paths.personal_dir);
	mkdir(_paths.save_dir);
	mkdir(_paths.autosave_dir);
	mkdir(_paths.scenario_dir);
	mkdir(_paths.heightmap_dir);
#else
	mkdir(_paths.personal_dir, 0755);
	mkdir(_paths.save_dir, 0755);
	mkdir(_paths.autosave_dir, 0755);
	mkdir(_paths.scenario_dir, 0755);
	mkdir(_paths.heightmap_dir, 0755);
#endif
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
/* XXX -- Currently no clipboard support implemented with GCC */
#ifndef __INNOTEK_LIBC__
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

				w = GetCharacterWidth(FS_NORMAL, (byte)*i);
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
#endif
	return false;
}


void CSleep(int milliseconds)
{
#ifndef __INNOTEK_LIBC__
 	delay(milliseconds);
#else
	usleep(milliseconds * 1000);
#endif
}

const char *FS2OTTD(const char *name) {return name;}
const char *OTTD2FS(const char *name) {return name;}
