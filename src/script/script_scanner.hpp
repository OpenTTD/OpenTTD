/* $Id$ */

/** @file script_scanner.hpp Declarations of the class for the script scanner. */

#ifndef SCRIPT_SCANNER_HPP
#define SCRIPT_SCANNER_HPP

#include "../fileio_type.h"

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
	 * @param info_flie_name The name of the 'info.nut' file.
	 * @param search_dir The subdirecotry to search for scripts.
	 */
	void ScanScriptDir(const char *info_file_name, Subdirectory search_dir);

private:
	/**
	 * Scan a dir for scripts.
	 */
	void ScanDir(const char *dirname, const char *info_file_name);

protected:
	class Squirrel *engine;
	char main_script[1024];
};

#endif /* SCRIPT_SCANNER_HPP */
