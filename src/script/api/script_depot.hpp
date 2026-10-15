/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_depot.hpp Everything to query depots. */

#ifndef SCRIPT_DEPOT_HPP
#define SCRIPT_DEPOT_HPP

#include "script_error.hpp"
#include "script_object.hpp"

/**
 * Class that handles all depot related functions.
 * @api ai game
 */

class ScriptDepot : public ScriptObject {
public:
	/**
	 * All depot related error messages.
	 *
	 * @see ScriptErrorType
	 */
	enum ErrorMessages {
		/** Base for depot related errors */
		ERR_DEPOT_BASE = ScriptError::ERR_CAT_DEPOT << ScriptError::ERR_CAT_BIT_SIZE,
	};

	/**
	 * Get the name of a depot.
	 * @param tile The depot tile to get the name of.
	 * @return The name of the station.
	 */
	static std::optional<std::string> GetName(TileIndex tile);

	/**
	 * Set the name a depot.
	 * @param tile The depot tile to set the name of.
	 * @param name The new name of the depot (can be either a raw string, or a ScriptText object).
	 * @pre name != null && len(name) != 0.
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @exception ScriptError::ERR_NAME_IS_NOT_UNIQUE
	 * @return True if the name was changed.
	 */
	static bool SetName(TileIndex tile, Text *name);

};

#endif /* SCRIPT_DEPOT_HPP */
