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
	 * @param name The name of the game script.
	 * @param version The version of the game script, or -1 if you want the latest.
	 * @param force_exact_match Only match name+version, never latest.
	 * @return nullptr if no match found, otherwise the game script that matched.
	 */
	class GameInfo *FindInfo(const std::string &name, int version, bool force_exact_match);

protected:
	std::string GetScriptName(ScriptInfo *info) override;
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
	 * @return The library if found, nullptr otherwise.
	 */
	class GameLibrary *FindLibrary(const std::string &library, int version);

protected:
	std::string GetScriptName(ScriptInfo *info) override;
	const char *GetFileName() const override { return PATHSEP "library.nut"; }
	Subdirectory GetDirectory() const override { return GAME_LIBRARY_DIR; }
	const char *GetScannerName() const override { return "GS Libraries"; }
	void RegisterAPI(class Squirrel *engine) override;
};

#endif /* GAME_SCANNER_HPP */
