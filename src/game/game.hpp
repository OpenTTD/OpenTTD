/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game.hpp Base functions for all Games. */

#ifndef GAME_HPP
#define GAME_HPP

#include "../core/string_compare_type.hpp"
#include <map>

/** A list that maps AI names to their AIInfo object. */
typedef std::map<const char *, class ScriptInfo *, StringCompare> ScriptInfoList;

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
	 * Uninitialize the Game system.
	 */
	static void Uninitialize(bool keepConfig);

	/**
	 * Get the current GameScript instance.
	 */
	static class GameInstance *GetGameInstance() { return Game::instance; }

	static void Rescan();
	static void ResetConfig();

	/** Wrapper function for GameScanner::GetConsoleList */
	static char *GetConsoleList(char *p, const char *last, bool newest_only = false);
	/** Wrapper function for GameScanner::GetInfoList */
	static const ScriptInfoList *GetInfoList();
	/** Wrapper function for GameScanner::GetUniqueInfoList */
	static const ScriptInfoList *GetUniqueInfoList();
	/** Wrapper function for GameScannerInfo::FindInfo */
	static class GameInfo *FindInfo(const char *name, int version, bool force_exact_match);

private:
	static uint frame_counter;             ///< Tick counter for the Game code.
	static class GameInstance *instance;   ///< Instance to the current active Game.
	static class GameScannerInfo *scanner; ///< Scanner for Game scripts.
	static class GameInfo *info;           ///< Current selected GameInfo.
};

#endif /* GAME_HPP */
