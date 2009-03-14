/* $Id$ */

/** @file ai_scanner.hpp declarations of the class for AI scanner */

#ifndef AI_SCANNER_HPP
#define AI_SCANNER_HPP

#include "../core/string_compare_type.hpp"
#include <map>

class AIScanner {
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
	class AIInfo *SelectRandomAI();

	/**
	 * Find an AI by name.
	 */
	class AIInfo *FindInfo(const char *name, int version);

	/**
	 * Get the list of available AIs for the console.
	 */
	char *GetAIConsoleList(char *p, const char *last);

	/**
	 * Get the list of all registered AIs.
	 */
	const AIInfoList *GetAIInfoList() { return &this->info_list; }

	/**
	 * Get the list of the newest version of all registered AIs.
	 */
	const AIInfoList *GetUniqueAIInfoList() { return &this->info_single_list; }

	/**
	 * Get the engine of the main squirrel handler (it indexes all available scripts).
	 */
	class Squirrel *GetEngine() { return this->engine; }

	/**
	 * Get the current main script the ScanDir is currently tracking.
	 */
	const char *GetMainScript() { return this->main_script; }

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
	 * Scan a dir for AIs.
	 *  For non-library-scan, if an AI is found, AIInfo is created, and the AI
	 *    is registered to the central system.
	 *  For library-scan, if a library is found, AILibrary is created, and the
	 *    library is registered to the central system.
	 */
	void ScanDir(const char *dirname, bool library_dir);

	AIInfo *info_dummy;
	AIInfoList info_list;
	AIInfoList info_single_list;
	AILibraryList library_list;
	class Squirrel *engine;
	char main_script[1024];
};

#endif /* AI_SCANNER_HPP */
