/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_scanner.hpp declarations of the class for AI scanner */

#ifndef AI_SCANNER_HPP
#define AI_SCANNER_HPP

#include "../script/script_scanner.hpp"

class AIScannerInfo : public ScriptScanner {
public:
	AIScannerInfo();
	~AIScannerInfo();

	/* virtual */ void Initialize();

	/**
	 * Select a random AI.
	 * @return A random AI from the pool.
	 */
	class AIInfo *SelectRandomAI() const;

	/**
	 * Check if we have an AI by name and version available in our list.
	 * @param nameParam The name of the AI.
	 * @param versionParam The versionof the AI, or -1 if you want the latest.
	 * @param force_exact_match Only match name+version, never latest.
	 * @return NULL if no match found, otherwise the AI that matched.
	 */
	class AIInfo *FindInfo(const char *nameParam, int versionParam, bool force_exact_match);

	/**
	 * Set the Dummy AI.
	 */
	void SetDummyAI(class AIInfo *info);

protected:
	/* virtual */ void GetScriptName(ScriptInfo *info, char *name, int len);
	/* virtual */ const char *GetFileName() const { return PATHSEP "info.nut"; }
	/* virtual */ Subdirectory GetDirectory() const { return AI_DIR; }
	/* virtual */ const char *GetScannerName() const { return "AIs"; }
	/* virtual */ void RegisterAPI(class Squirrel *engine);

private:
	AIInfo *info_dummy; ///< The dummy AI.
};

class AIScannerLibrary : public ScriptScanner {
public:
	/* virtual */ void Initialize();

	/**
	 * Find a library in the pool.
	 * @param library The library name to find.
	 * @param version The version the library should have.
	 * @return The library if found, NULL otherwise.
	 */
	class AILibrary *FindLibrary(const char *library, int version);

protected:
	/* virtual */ void GetScriptName(ScriptInfo *info, char *name, int len);
	/* virtual */ const char *GetFileName() const { return PATHSEP "library.nut"; }
	/* virtual */ Subdirectory GetDirectory() const { return AI_LIBRARY_DIR; }
	/* virtual */ const char *GetScannerName() const { return "AI Libraries"; }
	/* virtual */ void RegisterAPI(class Squirrel *engine);
};

#endif /* AI_SCANNER_HPP */
