/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_scanner.hpp declarations of the class for Game scanner */

#ifndef GAME_SCANNER_HPP
#define GAME_SCANNER_HPP

#include "../script/script_scanner.hpp"

class GameScannerInfo : public ScriptScanner {
public:
	/* virtual */ void Initialize();

	/**
	 * Check if we have a game by name and version available in our list.
	 * @param nameParam The name of the game script.
	 * @param versionParam The versionof the game script, or -1 if you want the latest.
	 * @param force_exact_match Only match name+version, never latest.
	 * @return NULL if no match found, otherwise the game script that matched.
	 */
	class GameInfo *FindInfo(const char *nameParam, int versionParam, bool force_exact_match);

protected:
	/* virtual */ void GetScriptName(ScriptInfo *info, char *name, int len);
	/* virtual */ const char *GetFileName() const { return PATHSEP "info.nut"; }
	/* virtual */ Subdirectory GetDirectory() const { return GAME_DIR; }
	/* virtual */ const char *GetScannerName() const { return "Game Scripts"; }
	/* virtual */ void RegisterAPI(class Squirrel *engine);
};


class GameScannerLibrary : public ScriptScanner {
public:
	/* virtual */ void Initialize();

	/**
	 * Find a library in the pool.
	 * @param library The library name to find.
	 * @param version The version the library should have.
	 * @return The library if found, NULL otherwise.
	 */
	class GameLibrary *FindLibrary(const char *library, int version);

protected:
	/* virtual */ void GetScriptName(ScriptInfo *info, char *name, int len);
	/* virtual */ const char *GetFileName() const { return PATHSEP "library.nut"; }
	/* virtual */ Subdirectory GetDirectory() const { return GAME_LIBRARY_DIR; }
	/* virtual */ const char *GetScannerName() const { return "GS Libraries"; }
	/* virtual */ void RegisterAPI(class Squirrel *engine);
};

#endif /* GAME_SCANNER_HPP */
