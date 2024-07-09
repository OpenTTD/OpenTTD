/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file win32.h declarations of functions for MS windows systems */

#ifndef WIN32_H
#define WIN32_H

bool MyShowCursor(bool show, bool toggle = false);

char *convert_from_fs(const std::wstring_view src, std::span<char> dst_buf);
wchar_t *convert_to_fs(const std::string_view src, std::span<wchar_t> dst_buf);

void Win32SetCurrentLocaleName(const char *iso_code);
int OTTDStringCompare(std::string_view s1, std::string_view s2);
int Win32StringContains(const std::string_view str, const std::string_view value, bool case_insensitive);

#endif /* WIN32_H */
