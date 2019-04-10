/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_sign.hpp Everything to query and build signs. */

#ifndef SCRIPT_SIGN_HPP
#define SCRIPT_SIGN_HPP

#include "script_company.hpp"
#include "script_error.hpp"

/**
 * Class that handles all sign related functions.
 * @api ai game
 */
class ScriptSign : public ScriptObject {
public:
	/**
	 * All sign related error messages.
	 */
	enum ErrorMessages {

		/** Base for sign building related errors */
		ERR_SIGN_BASE = ScriptError::ERR_CAT_SIGN << ScriptError::ERR_CAT_BIT_SIZE,

		/** Too many signs have been placed */
		ERR_SIGN_TOO_MANY_SIGNS,             // [STR_ERROR_TOO_MANY_SIGNS]
	};

	/**
	 * Checks whether the given sign index is valid.
	 * @param sign_id The index to check.
	 * @return True if and only if the sign is valid.
	 */
	static bool IsValidSign(SignID sign_id);

	/**
	 * Set the name of a sign.
	 * @param sign_id The sign to set the name for.
	 * @param name The name for the sign (can be either a raw string, or a ScriptText object).
	 * @pre IsValidSign(sign_id).
	 * @pre name != nullptr && len(name) != 0.
	 * @exception ScriptError::ERR_NAME_IS_NOT_UNIQUE
	 * @return True if and only if the name was changed.
	 */
	static bool SetName(SignID sign_id, Text *name);

	/**
	 * Get the name of the sign.
	 * @param sign_id The sign to get the name of.
	 * @pre IsValidSign(sign_id).
	 * @return The name of the sign.
	 */
	static char *GetName(SignID sign_id);

	/**
	 * Get the owner of a sign.
	 * @param sign_id The sign to get the owner of.
	 * @pre IsValidSign(sign_id).
	 * @return The owner the sign has.
	 * @api -ai
	 */
	static ScriptCompany::CompanyID GetOwner(SignID sign_id);

	/**
	 * Gets the location of the sign.
	 * @param sign_id The sign to get the location of.
	 * @pre IsValidSign(sign_id).
	 * @return The location of the sign.
	 */
	static TileIndex GetLocation(SignID sign_id);

	/**
	 * Builds a sign on the map.
	 * @param location The place to build the sign.
	 * @param name The text to place on the sign (can be either a raw string, or a ScriptText object).
	 * @pre ScriptMap::IsValidTile(location).
	 * @pre name != nullptr && len(name) != 0.
	 * @exception ScriptSign::ERR_SIGN_TOO_MANY_SIGNS
	 * @return The SignID of the build sign (use IsValidSign() to check for validity).
	 *   In test-mode it returns 0 if successful, or any other value to indicate
	 *   failure.
	 */
	static SignID BuildSign(TileIndex location, Text *name);

	/**
	 * Removes a sign from the map.
	 * @param sign_id The sign to remove.
	 * @pre IsValidSign(sign_id).
	 * @return True if and only if the sign has been removed.
	 */
	static bool RemoveSign(SignID sign_id);
};

#endif /* SCRIPT_SIGN_HPP */
