/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_sign.hpp Everything to query and build signs. */

#ifndef AI_SIGN_HPP
#define AI_SIGN_HPP

#include "ai_error.hpp"

/**
 * Class that handles all sign related functions.
 */
class AISign : public AIObject {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AISign"; }

	/**
	 * All sign related error messages.
	 */
	enum ErrorMessages {

		/** Base for sign building related errors */
		ERR_SIGN_BASE = AIError::ERR_CAT_SIGN << AIError::ERR_CAT_BIT_SIZE,

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
	 * @param name The name for the sign.
	 * @pre IsValidSign(sign_id).
	 * @pre 'name' must have at least one character.
	 * @pre 'name' must have at most 30 characters.
	 * @exception AIError::ERR_NAME_IS_NOT_UNIQUE
	 * @return True if and only if the name was changed.
	 */
	static bool SetName(SignID sign_id, const char *name);

	/**
	 * Get the name of the sign.
	 * @param sign_id The sign to get the name of.
	 * @pre IsValidSign(sign_id).
	 * @return The name of the sign.
	 */
	static char *GetName(SignID sign_id);

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
	 * @param text The text to place on the sign.
	 * @pre AIMap::IsValidTile(location).
	 * @pre 'text' must have at least one character.
	 * @pre 'text' must have at most 30 characters.
	 * @exception AISign::ERR_SIGN_TOO_MANY_SIGNS
	 * @return The SignID of the build sign (use IsValidSign() to check for validity).
	 *   In test-mode it returns 0 if successful, or any other value to indicate
	 *   failure.
	 */
	static SignID BuildSign(TileIndex location, const char *text);

	/**
	 * Removes a sign from the map.
	 * @param sign_id The sign to remove.
	 * @pre IsValidSign(sign_id).
	 * @return True if and only if the sign has been removed.
	 */
	static bool RemoveSign(SignID sign_id);
};

#endif /* AI_SIGN_HPP */
