/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file macos.h Functions related to MacOS support. */

#ifndef MACOS_H
#define MACOS_H

/** Helper function displaying a message the best possible way. */
void ShowMacDialog(std::string_view title, std::string_view message, std::string_view button_label);

std::tuple<int, int, int> GetMacOSVersion();

bool IsMonospaceFont(CFStringRef name);

void MacOSSetThreadName(const std::string &name);

uint64_t MacOSGetPhysicalMemory();


/** Deleter that calls CFRelease rather than deleting the pointer. */
template <typename T> struct CFDeleter {
	void operator()(T *p)
	{
		if (p) ::CFRelease(p);
	}
};

/** Specialisation of std::unique_ptr for CoreFoundation objects. */
template <typename T>
using CFAutoRelease = std::unique_ptr<typename std::remove_pointer<T>::type, CFDeleter<typename std::remove_pointer<T>::type>>;

#endif /* MACOS_H */
