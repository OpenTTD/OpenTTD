/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file library_loader_unix.cpp Implementation of the LibraryLoader for Linux / MacOS */

#include "../../stdafx.h"

#include <dlfcn.h>

#include "../../library_loader.h"

#include "../../safeguards.h"

/* Emscripten cannot dynamically load other files. */
#if defined(__EMSCRIPTEN__)

void *LibraryLoader::OpenLibrary(const std::string &)
{
	this->error = "Dynamic loading is not supported on this platform.";
	return nullptr;
}

void LibraryLoader::CloseLibrary()
{
}

void *LibraryLoader::GetSymbol(const std::string &)
{
	this->error = "Dynamic loading is not supported on this platform.";
	return nullptr;
}

#else

void *LibraryLoader::OpenLibrary(const std::string &filename)
{
	void *h = dlopen(filename.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (h == nullptr) {
		this->error = dlerror();
	}

	return h;
}

void LibraryLoader::CloseLibrary()
{
	dlclose(this->handle);
}

void *LibraryLoader::GetSymbol(const std::string &symbol_name)
{
	void *p = dlsym(this->handle, symbol_name.c_str());
	if (p == nullptr) {
		this->error = dlerror();
	}

	return p;
}

#endif /* __EMSCRIPTEN__ */
