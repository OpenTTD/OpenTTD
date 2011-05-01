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

#include "../fileio_type.h"

/** Scanner to help finding scripts. */
class ScriptScanner {
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
	 * Rescan for scripts.
	 * @param info_file_name The name of the 'info.nut' file.
	 * @param search_dir The subdirecotry to search for scripts.
	 */
	void ScanScriptDir(const char *info_file_name, Subdirectory search_dir);

private:
	/**
	 * Scan a dir for scripts.
	 */
	void ScanDir(const char *dirname, const char *info_file_name);

protected:
	class Squirrel *engine; ///< The engine we're scanning with.
	char main_script[1024]; ///< The name of the current main script.
};

#endif /* SCRIPT_SCANNER_HPP */
