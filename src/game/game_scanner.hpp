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
	void Initialize() override;

	/**
	 * Check if we have a game by name and version available in our list.
	 * @param nameParam The name of the game script.
	 * @param versionParam The version of the game script, or -1 if you want the latest.
	 * @param force_exact_match Only match name+version, never latest.
	 * @return NULL if no match found, otherwise the game script that matched.
	 */
	class GameInfo *FindInfo(const char *nameParam, int versionParam, bool force_exact_match);

protected:
	void GetScriptName(ScriptInfo *info, char *name, const char *last) override;
	const char *GetFileName() const override { return PATHSEP "info.nut"; }
	Subdirectory GetDirectory() const override { return GAME_DIR; }
	const char *GetScannerName() const override { return "Game Scripts"; }
	void RegisterAPI(class Squirrel *engine) override;
};


class GameScannerLibrary : public ScriptScanner {
public:
	void Initialize() override;

	/**
	 * Find a library in the pool.
	 * @param library The library name to find.
	 * @param version The version the library should have.
	 * @return The library if found, NULL otherwise.
	 */
	class GameLibrary *FindLibrary(const char *library, int version);

protected:
	void GetScriptName(ScriptInfo *info, char *name, const char *last) override;
	const char *GetFileName() const override { return PATHSEP "library.nut"; }
	Subdirectory GetDirectory() const override { return GAME_LIBRARY_DIR; }
	const char *GetScannerName() const override { return "GS Libraries"; }
	void RegisterAPI(class Squirrel *engine) override;
};

#endif /* GAME_SCANNER_HPP */
