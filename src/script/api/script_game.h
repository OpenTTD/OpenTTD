/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_game.h Everything to manipulate the current running game. */

#ifndef SCRIPT_GAME_HPP
#define SCRIPT_GAME_HPP

#include "script_object.h"
#include "../../landscape_type.h"

/**
 * Class that handles some game related functions.
 * @api game
 */
class ScriptGame : public ScriptObject {
public:
	/**
	 * Type of landscapes known in the game.
	 */
	enum LandscapeType {
		/* Note: these values represent part of the in-game LandscapeType enum */
		LT_TEMPERATE  = ::LT_TEMPERATE, ///< Temperate climate.
		LT_ARCTIC     = ::LT_ARCTIC,    ///< Arctic climate.
		LT_TROPIC     = ::LT_TROPIC,    ///< Tropic climate.
		LT_TOYLAND    = ::LT_TOYLAND,   ///< Toyland climate.
	};

	/**
	 * Pause the server.
	 * @return True if the action succeeded.
	 */
	static bool Pause();

	/**
	 * Unpause the server.
	 * @return True if the action succeeded.
	 */
	static bool Unpause();

	/**
	 * Check if the game is paused.
	 * @return True if and only if the game is paused (by which-ever means).
	 * @note That a game is paused, doesn't always means you can unpause it. If
	 *  the game has been manually paused, or because of the pause_on_join in
	 *  Multiplayer for example, you cannot unpause the game.
	 */
	static bool IsPaused();

	/**
	 * Get the current landscape.
	 * @return The type of landscape.
	 */
	static LandscapeType GetLandscape();

	/**
	 * Is this a multiplayer game?
	 * @return True if this is a server in a multiplayer game.
	 */
	static bool IsMultiplayer();
};

#endif /* SCRIPT_GAME_HPP */
