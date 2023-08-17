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

	void Initialize() override;

	/**
	 * Select a random AI.
	 * @return A random AI from the pool.
	 */
	class AIInfo *SelectRandomAI() const;

	/**
	 * Check if we have an AI by name and version available in our list.
	 * @param name The name of the AI.
	 * @param version The version of the AI, or -1 if you want the latest.
	 * @param force_exact_match Only match name+version, never latest.
	 * @return nullptr if no match found, otherwise the AI that matched.
	 */
	class AIInfo *FindInfo(const std::string &name, int version, bool force_exact_match);

	/**
	 * Set the Dummy AI.
	 */
	void SetDummyAI(class AIInfo *info);

protected:
	std::string GetScriptName(ScriptInfo *info) override;
	const char *GetFileName() const override { return PATHSEP "info.nut"; }
	Subdirectory GetDirectory() const override { return AI_DIR; }
	const char *GetScannerName() const override { return "AIs"; }
	void RegisterAPI(class Squirrel *engine) override;

private:
	AIInfo *info_dummy; ///< The dummy AI.
};

class AIScannerLibrary : public ScriptScanner {
public:
	void Initialize() override;

	/**
	 * Find a library in the pool.
	 * @param library The library name to find.
	 * @param version The version the library should have.
	 * @return The library if found, nullptr otherwise.
	 */
	class AILibrary *FindLibrary(const std::string &library, int version);

protected:
	std::string GetScriptName(ScriptInfo *info) override;
	const char *GetFileName() const override { return PATHSEP "library.nut"; }
	Subdirectory GetDirectory() const override { return AI_LIBRARY_DIR; }
	const char *GetScannerName() const override { return "AI Libraries"; }
	void RegisterAPI(class Squirrel *engine) override;
};

#endif /* AI_SCANNER_HPP */
