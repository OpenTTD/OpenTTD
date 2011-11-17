/* $Id$ */

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

char *convert_from_fs(const wchar_t *name, char *utf8_buf, size_t buflen);
wchar_t *convert_to_fs(const char *name, wchar_t *utf16_buf, size_t buflen);

/* Function shortcuts for UTF-8 <> UNICODE conversion. When unicode is not
 * defined these macros return the string passed to them, with UNICODE
 * they return a pointer to the converted string. The only difference between
 * XX_TO_YY and XX_TO_YY_BUFFER is that with the buffer variant you can
 * specify where to put the converted string (and how long it can be). Without
 * the buffer and internal buffer is used, of max 512 characters */
#if defined(UNICODE)
# define MB_TO_WIDE(str) OTTD2FS(str)
# define MB_TO_WIDE_BUFFER(str, buffer, buflen) convert_to_fs(str, buffer, buflen)
# define WIDE_TO_MB(str) FS2OTTD(str)
# define WIDE_TO_MB_BUFFER(str, buffer, buflen) convert_from_fs(str, buffer, buflen)
#else
extern uint _codepage; // local code-page in the system @see win32_v.cpp:WM_INPUTLANGCHANGE
# define MB_TO_WIDE(str) (str)
# define MB_TO_WIDE_BUFFER(str, buffer, buflen) (str)
# define WIDE_TO_MB(str) (str)
# define WIDE_TO_MB_BUFFER(str, buffer, buflen) (str)
#endif

/* Override SHGetFolderPath with our custom implementation */
#if defined(SHGetFolderPath)
#undef SHGetFolderPath
#endif
#define SHGetFolderPath OTTDSHGetFolderPath

HRESULT OTTDSHGetFolderPath(HWND, int, HANDLE, DWORD, LPTSTR);

#if defined(__MINGW32__)
#define SHGFP_TYPE_CURRENT 0
#endif /* __MINGW32__ */

#endif /* WIN32_H */
