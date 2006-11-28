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

#include <direct.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <dos.h>

#define INCL_WIN
#define INCL_WINCLIPBOARD

#include <os2.h>
#include <i86.h>

bool FiosIsRoot(const char *file)
{
	return path[3] == '\0';
}

void FiosGetDrives(void)
{
	FiosItem *fios;
	unsigned disk, disk2, save, total;

	_dos_getdrive(&save); // save original drive

	/* get an available drive letter */
	for (disk = 1;; disk++) {
		_dos_setdrive(disk, &total);
		if (disk >= total) return;
		_dos_getdrive(&disk2);

		if (disk == disk2) {
			FiosItem *fios = FiosAlloc();
			fios->type = FIOS_TYPE_DRIVE;
			fios->mtime = 0;
			snprintf(fios->name, lengthof(fios->name),  "%c:", 'A' + disk - 1);
			ttd_strlcpy(fios->title, fios->name, lengthof(fios->title));
		}
	}

	_dos_setdrive(save, &total); // restore the original drive
}

bool FiosGetDiskFreeSpace(const char *path, uint32 *tot)
{
	struct diskfree_t free;
	char drive = path[0] - 'A' + 1;

	if (tot != NULL && _getdiskfree(drive, &free) == 0) {
		*tot = free.avail_clusters * free.sectors_per_cluster * free.bytes_per_sector;
		return true;
	}

	return false;
}

bool FiosIsValidFile(const char *path, const struct dirent *ent, struct stat *sb)
{
	char filename[MAX_PATH];

	snprintf(filename, lengthof(filename), "%s" PATHSEP "%s", path, ent->d_name);
	if (stat(filename, sb) != 0) return false;

	return (ent->d_name[0] != '.'); // hidden file
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

	_random_seeds[1][1] = _random_seeds[1][0] = _random_seeds[0][1] = _random_seeds[0][0] = time(NULL);

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
	_path.heightmap_dir = str_fmt("%sscenario\\heightmap", _path.personal_dir);
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
	mkdir(_path.heightmap_dir);
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

	return false;
}


void CSleep(int milliseconds)
{
	delay(milliseconds);
}

const char *FS2OTTD(const char *name) {return name;}
const char *OTTD2FS(const char *name) {return name;}
