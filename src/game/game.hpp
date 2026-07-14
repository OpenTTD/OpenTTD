/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file game.hpp Base functions for all Games. */

#ifndef GAME_HPP
#define GAME_HPP

#include "game_scanner.hpp"

#include "../script/api/script_event_types.hpp"

/**
 * Main Game class. Contains all functions needed to start, stop, save and load Game Scripts.
 */
class Game {
public:
	/**
	 * Called every game-tick to let Game do something.
	 */
	static void GameLoop();

	/**
	 * Initialize the Game system.
	 */
	static void Initialize();

	/**
	 * Start up a new GameScript.
	 */
	static void StartNew();

	/**
	 * Uninitialize the Game system.
	 * @param keepConfig Should we keep GameConfigs, or can we free that memory?
	 */
	static void Uninitialize(bool keepConfig);

	/**
	 * Suspends the Game Script and then pause the execution of the script. The
	 * script will not be resumed from its suspended state until the script
	 * has been unpaused.
	 */
	static void Pause();

	/**
	 * Resume execution of the Game Script. This function will not actually execute
	 * the script, but set a flag so that the script is executed by the usual
	 * mechanism that executes the script.
	 */
	static void Unpause();

	/**
	 * Checks if the Game Script is paused.
	 * @return true if the Game Script is paused, otherwise false.
	 */
	static bool IsPaused();

	/**
	 * Queue a new event for the game script.
	 * @param event The event.
	 */
	static void NewEvent(class ScriptEvent *event);

	/**
	 * Get the current GameInfo.
	 * @return The info, or nullptr when there is no Game script.
	 */
	static class GameInfo *GetInfo() { return Game::info; }

	/**
	 * Rescans all searchpaths for available Game scripts. If a used Game script is no longer
	 * found it is removed from the config.
	 */
	static void Rescan();

	/**
	 * Reset all GameConfigs, and make them reload their GameInfo.
	 * If the GameInfo could no longer be found, an error is reported to the user.
	 */
	static void ResetConfig();

	/**
	 * Save data from a GameScript to a savegame.
	 */
	static void Save();

	/** @copydoc ScriptScanner::GetConsoleList */
	static void GetConsoleList(std::back_insert_iterator<std::string> &output_iterator, bool newest_only);
	/** @copydoc ScriptScanner::GetConsoleList */
	static void GetConsoleLibraryList(std::back_insert_iterator<std::string> &output_iterator, bool newest_only);
	/** @copydoc ScriptScanner::GetInfoList */
	static const ScriptInfoList *GetInfoList();
	/** @copydoc ScriptScanner::GetUniqueInfoList */
	static const ScriptInfoList *GetUniqueInfoList();
	/** @copydoc ScriptConfig::FindInfo */
	static class GameInfo *FindInfo(const std::string &name, int version, bool force_exact_match);
	/** @copydoc ScriptInstance::FindLibrary */
	static class GameLibrary *FindLibrary(const std::string &library, int version);

	/**
	 * Get the current active instance.
	 * @return The current Game script instance.
	 */
	static class GameInstance *GetInstance() { return Game::instance.get(); }

	/**
	 * Reset the current active instance.
	 */
	static void ResetInstance();

	/** Wrapper function for GameScanner::HasGame */
	static bool HasGame(const ContentInfo &ci, bool md5sum);
	static bool HasGameLibrary(const ContentInfo &ci, bool md5sum);
	/** Gets the ScriptScanner instance that is used to find Game scripts */
	static GameScannerInfo *GetScannerInfo();
	/** Gets the ScriptScanner instance that is used to find Game Libraries */
	static GameScannerLibrary *GetScannerLibrary();

private:
	static uint frame_counter; ///< Tick counter for the Game code.
	static std::unique_ptr<GameInstance> instance; ///< Instance to the current active Game.
	static std::unique_ptr<GameScannerInfo> scanner_info; ///< Scanner for Game scripts.
	static std::unique_ptr<GameScannerLibrary> scanner_library; ///< Scanner for GS Libraries.
	static GameInfo *info; ///< Current selected GameInfo.
};

#endif /* GAME_HPP */
