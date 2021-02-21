/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file win32.h declarations of functions for MS windows systems */

#ifndef WIN32_H
#define WIN32_H

#include <windows.h>
bool MyShowCursor(bool show, bool toggle = false);

typedef void (*Function)(int);
bool LoadLibraryList(Function proc[], const char *dll);

char *convert_from_fs(const TCHAR *name, char *utf8_buf, size_t buflen);
TCHAR *convert_to_fs(const char *name, TCHAR *utf16_buf, size_t buflen, bool console_cp = false);

/* Function shortcuts for UTF-8 <> UNICODE conversion. These functions use an
 * internal buffer of max 512 characters. */
# define MB_TO_WIDE(str) OTTD2FS(str)
# define WIDE_TO_MB(str) FS2OTTD(str)

#if defined(__MINGW32__) && !defined(__MINGW64__)
#define SHGFP_TYPE_CURRENT 0
#endif /* __MINGW32__ */

void Win32SetCurrentLocaleName(const char *iso_code);
int OTTDStringCompare(const char *s1, const char *s2);
bool IsWindowsVistaOrGreater();

#endif /* WIN32_H */
