/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_scanner.hpp Declarations of the class for the script scanner. */

#ifndef SCRIPT_SCANNER_HPP
#define SCRIPT_SCANNER_HPP

#include <map>
#include "../fileio_func.h"
#include "../core/string_compare_type.hpp"

typedef std::map<const char *, class ScriptInfo *, StringCompare> ScriptInfoList; ///< Type for the list of scripts.

/** Scanner to help finding scripts. */
class ScriptScanner : public FileScanner {
public:
	ScriptScanner();
	virtual ~ScriptScanner();

	virtual void Initialize() = 0;

	/**
	 * Get the engine of the main squirrel handler (it indexes all available scripts).
	 */
	class Squirrel *GetEngine() { return this->engine; }

	/**
	 * Get the current main script the ScanDir is currently tracking.
	 */
	std::string GetMainScript() { return this->main_script; }

	/**
	 * Get the current tar file the ScanDir is currently tracking.
	 */
	std::string GetTarFile() { return this->tar_file; }

	/**
	 * Get the list of all registered scripts.
	 */
	const ScriptInfoList *GetInfoList() { return &this->info_list; }

	/**
	 * Get the list of the latest version of all registered scripts.
	 */
	const ScriptInfoList *GetUniqueInfoList() { return &this->info_single_list; }

	/**
	 * Register a ScriptInfo to the scanner.
	 */
	void RegisterScript(class ScriptInfo *info);

	/**
	 * Get the list of registered scripts to print on the console.
	 */
	char *GetConsoleList(char *p, const char *last, bool newest_only) const;

	/**
	 * Check whether we have a script with the exact characteristics as ci.
	 * @param ci The characteristics to search on (shortname and md5sum).
	 * @param md5sum Whether to check the MD5 checksum.
	 * @return True iff we have a script matching.
	 */
	bool HasScript(const struct ContentInfo *ci, bool md5sum);

	/**
	 * Find a script of a #ContentInfo
	 * @param ci The information to compare to.
	 * @param md5sum Whether to check the MD5 checksum.
	 * @return A filename of a file of the content, else \c nullptr.
	 */
	const char *FindMainScript(const ContentInfo *ci, bool md5sum);

	bool AddFile(const std::string &filename, size_t basepath_length, const std::string &tar_filename) override;

	/**
	 * Rescan the script dir.
	 */
	void RescanDir();

protected:
	class Squirrel *engine;  ///< The engine we're scanning with.
	std::string main_script; ///< The full path of the script.
	std::string tar_file;    ///< If, which tar file the script was in.

	ScriptInfoList info_list;        ///< The list of all script.
	ScriptInfoList info_single_list; ///< The list of all unique script. The best script (highest version) is shown.

	/**
	 * Initialize the scanner.
	 * @param name The name of the scanner ("AIScanner", "GSScanner", ..).
	 */
	void Initialize(const char *name);

	/**
	 * Get the script name how to store the script in memory.
	 */
	virtual void GetScriptName(ScriptInfo *info, char *name, const char *last) = 0;

	/**
	 * Get the filename to scan for this type of script.
	 */
	virtual const char *GetFileName() const = 0;

	/**
	 * Get the directory to scan in.
	 */
	virtual Subdirectory GetDirectory() const = 0;

	/**
	 * Register the API for this ScriptInfo.
	 */
	virtual void RegisterAPI(class Squirrel *engine) = 0;

	/**
	 * Get the type of the script, in plural.
	 */
	virtual const char *GetScannerName() const = 0;

	/**
	 * Reset all allocated lists.
	 */
	void Reset();

	/**
	 * Reset the engine to ensure a clean environment for further steps.
	 */
	void ResetEngine();
};

#endif /* SCRIPT_SCANNER_HPP */
