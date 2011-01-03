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

#include <squirrel.h>
#include "../script/script_scanner.hpp"
#include "../core/string_compare_type.hpp"
#include "ai.hpp"
#include <map>

/**
 * Class that scans for available AIs.
 */
class AIScanner : public ScriptScanner {
public:
	AIScanner();
	~AIScanner();

	/**
	 * Import a library inside the Squirrel VM.
	 */
	bool ImportLibrary(const char *library, const char *class_name, int version, HSQUIRRELVM vm, class AIController *controller);

	/**
	 * Register a library to be put in the available list.
	 */
	void RegisterLibrary(class AILibrary *library);

	/**
	 * Register an AI to be put in the available list.
	 */
	void RegisterAI(class AIInfo *info);

	void SetDummyAI(class AIInfo *info) { this->info_dummy = info; }

	/**
	 * Select a Random AI.
	 */
	class AIInfo *SelectRandomAI() const;

	/**
	 * Find an AI by name.
	 */
	class AIInfo *FindInfo(const char *name, int version, bool force_exact_match);

	/**
	 * Get the list of available AIs for the console.
	 */
	char *GetAIConsoleList(char *p, const char *last) const;

	/**
	 * Get the list of available AI Libraries for the console.
	 */
	char *GetAIConsoleLibraryList(char *p, const char *last) const;

	/**
	 * Get the list of all registered AIs.
	 */
	const AIInfoList *GetAIInfoList() { return &this->info_list; }

	/**
	 * Get the list of the newest version of all registered AIs.
	 */
	const AIInfoList *GetUniqueAIInfoList() { return &this->info_single_list; }

	/**
	 * Rescan the AI dir for scripts.
	 */
	void RescanAIDir();

#if defined(ENABLE_NETWORK)
	bool HasAI(const struct ContentInfo *ci, bool md5sum);
#endif
private:
	typedef std::map<const char *, class AILibrary *, StringCompare> AILibraryList;

	/**
	 * Scan the AI dir for scripts.
	 */
	void ScanAIDir();

	/**
	 * Reset all allocated lists.
	 */
	void Reset();

	AIInfo *info_dummy;
	AIInfoList info_list;
	AIInfoList info_single_list;
	AILibraryList library_list;
};

#endif /* AI_SCANNER_HPP */
