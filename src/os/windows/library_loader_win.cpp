/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file library_loader_win.cpp Implementation of the LibraryLoader for Windows */

#include "../../stdafx.h"

#include <windows.h>

#include "../../library_loader.h"
#include "../../3rdparty/fmt/format.h"

#include "../../safeguards.h"

static std::string GetLoadError()
{
	auto error_code = GetLastError();

	wchar_t buffer[512];
	if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error_code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, lengthof(buffer), nullptr) == 0) {
		return fmt::format("Unknown error {}", error_code);
	}

	return FS2OTTD(buffer);
}

void *LibraryLoader::OpenLibrary(const std::string &filename)
{
	void *h = ::LoadLibraryW(OTTD2FS(filename).c_str());
	if (h == nullptr) {
		this->error = GetLoadError();
	}

	return h;
}

void LibraryLoader::CloseLibrary()
{
	HMODULE handle = static_cast<HMODULE>(this->handle);

	::FreeLibrary(handle);
}

void *LibraryLoader::GetSymbol(const std::string &symbol_name)
{
	HMODULE handle = static_cast<HMODULE>(this->handle);

	void *p = reinterpret_cast<void *>(::GetProcAddress(handle, symbol_name.c_str()));
	if (p == nullptr) {
		this->error = GetLoadError();
	}

	return p;
}
