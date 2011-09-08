/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_scanner.hpp Declarations of the class for the script scanner. */

#ifndef SCRIPT_SCANNER_HPP
#define SCRIPT_SCANNER_HPP

#include "../fileio_func.h"

/** Scanner to help finding scripts. */
class ScriptScanner : public FileScanner {
public:
	ScriptScanner();
	~ScriptScanner();

	/**
	 * Get the engine of the main squirrel handler (it indexes all available scripts).
	 */
	class Squirrel *GetEngine() { return this->engine; }

	/**
	 * Get the current main script the ScanDir is currently tracking.
	 */
	const char *GetMainScript() { return this->main_script; }

	/**
	 * Get the current tar file the ScanDir is currently tracking.
	 */
	const char *GetTarFile() { return this->tar_file; }

	/* virtual */ bool AddFile(const char *filename, size_t basepath_length, const char *tar_filename);

protected:
	class Squirrel *engine; ///< The engine we're scanning with.
	char *main_script;      ///< The name of the current main script.
	char *tar_file;         ///< The filename of the tar for the main script.
};

#endif /* SCRIPT_SCANNER_HPP */
