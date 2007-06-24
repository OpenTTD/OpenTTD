/* $Id$ */

/** @file os2.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "string.h"
#include "table/strings.h"
#include "gfx.h"
#include "gui.h"
#include "functions.h"
#include "macros.h"
#include "fileio.h"
#include "fios.h" // opendir/readdir/closedir

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

void ShowInfo(const char *str)
{
	HAB hab;
	HMQ hmq;
	ULONG rc;

	// init PM env.
	hmq = WinCreateMsgQueue((hab = WinInitialize(0)), 0);

	// display the box
	rc = WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, (const unsigned char *)str, (const unsigned char *)"OpenTTD", 0, MB_OK | MB_MOVEABLE | MB_INFORMATION);

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
	rc = WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, (const unsigned char *)buf, (const unsigned char *)"OpenTTD", 0, MB_OK | MB_MOVEABLE | MB_ERROR);

	// terminate PM env.
	WinDestroyMsgQueue(hmq);
	WinTerminate(hab);
}

int CDECL main(int argc, char* argv[])
{
	_random_seeds[1][1] = _random_seeds[1][0] = _random_seeds[0][1] = _random_seeds[0][0] = time(NULL);

	return ttd_main(argc, argv);
}

/**
 * Insert a chunk of text from the clipboard onto the textbuffer. Get TEXT clipboard
 * and append this up to the maximum length (either absolute or screenlength). If maxlength
 * is zero, we don't care about the screenlength but only about the physical length of the string
 * @param tb Textbuf type to be changed
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
