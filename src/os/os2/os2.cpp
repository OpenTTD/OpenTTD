/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file os2.cpp OS2 related OS support. */

#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../gui.h"
#include "../../fileio_func.h"
#include "../../fios.h"
#include "../../string_func.h"
#include "../../textbuf_gui.h"
#include "../../thread.h"

#include "table/strings.h"

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#ifndef __INNOTEK_LIBC__
#	include <dos.h>
#endif

#include "../../safeguards.h"

#define INCL_WIN
#define INCL_WINCLIPBOARD

#include <os2.h>
#ifndef __INNOTEK_LIBC__
#	include <i86.h>
#endif

bool FiosIsRoot(const std::string &file)
{
	return file.size() == 3; // C:\...
}

void FiosGetDrives(FileList &file_list)
{
	uint disk, disk2, save, total;

#ifndef __INNOTEK_LIBC__
	_dos_getdrive(&save); // save original drive
#else
	save = _getdrive(); // save original drive
	char wd[MAX_PATH];
	getcwd(wd, MAX_PATH);
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
		if (disk >= total)  break;

#ifndef __INNOTEK_LIBC__
		_dos_getdrive(&disk2);
#else
		disk2 = _getdrive();
#endif

		if (disk == disk2) {
			FiosItem *fios = file_list.Append();
			fios->type = FIOS_TYPE_DRIVE;
			fios->mtime = 0;
#ifndef __INNOTEK_LIBC__
			fios->name += 'A' + disk - 1;
#else
			fios->name += (char)disk;
#endif
			fios->name += ':';
			fios->title = fios->name;
		}
	}

	/* Restore the original drive */
#ifndef __INNOTEK_LIBC__
	_dos_setdrive(save, &total);
#else
	chdir(wd);
#endif
}

std::optional<uint64_t> FiosGetDiskFreeSpace(const std::string &path)
{
#ifndef __INNOTEK_LIBC__
	struct diskfree_t free;
	char drive = path[0] - 'A' + 1;

	if (_getdiskfree(drive, &free) == 0) {
		return free.avail_clusters * free.sectors_per_cluster * free.bytes_per_sector;
	}
#elif defined(HAS_STATVFS)
	struct statvfs s;

	if (statvfs(path.c_str(), &s) == 0) return static_cast<uint64_t>(s.f_frsize) * s.f_bavail;
#endif
	return std::nullopt;
}

bool FiosIsValidFile(const std::string &path, const struct dirent *ent, struct stat *sb)
{
	std::string filename = fmt::format("{}" PATHSEP "{}", path, ent->d_name);
	return stat(filename.c_str(), sb) == 0;
}

bool FiosIsHiddenFile(const struct dirent *ent)
{
	return ent->d_name[0] == '.';
}

void ShowInfoI(const std::string &str)
{
	HAB hab;
	HMQ hmq;
	ULONG rc;

	/* init PM env. */
	hmq = WinCreateMsgQueue((hab = WinInitialize(0)), 0);

	/* display the box */
	rc = WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, (const unsigned char *)str.c_str(), (const unsigned char *)"OpenTTD", 0, MB_OK | MB_MOVEABLE | MB_INFORMATION);

	/* terminate PM env. */
	WinDestroyMsgQueue(hmq);
	WinTerminate(hab);
}

void ShowOSErrorBox(const char *buf, bool system)
{
	HAB hab;
	HMQ hmq;
	ULONG rc;

	/* init PM env. */
	hmq = WinCreateMsgQueue((hab = WinInitialize(0)), 0);

	/* display the box */
	rc = WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, (const unsigned char *)buf, (const unsigned char *)"OpenTTD", 0, MB_OK | MB_MOVEABLE | MB_ERROR);

	/* terminate PM env. */
	WinDestroyMsgQueue(hmq);
	WinTerminate(hab);
}

std::optional<std::string> GetClipboardContents()
{
/* XXX -- Currently no clipboard support implemented with GCC */
#ifndef __INNOTEK_LIBC__
	HAB hab = 0;

	if (WinOpenClipbrd(hab)) {
		const char *text = (const char *)WinQueryClipbrdData(hab, CF_TEXT);

		if (text != nullptr) {
			std::string result = text;
			WinCloseClipbrd(hab);
			return result;
		}

		WinCloseClipbrd(hab);
	}
#endif
	return std::nullopt;
}


void OSOpenBrowser(const char *url)
{
	// stub only
	Debug(misc, 0, "Failed to open url: {}", url);
}

void SetCurrentThreadName(const char *)
{
}
