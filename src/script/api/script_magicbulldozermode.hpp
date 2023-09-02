/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_magicbulldozermode.hpp Switch on the magic bulldozer. */

#ifndef SCRIPT_MAGICBULLDOZERMODE_HPP
#define SCRIPT_MAGICBULLDOZERMODE_HPP

#include "script_object.hpp"

/**
 * Class to switch on the magic bulldozer mode.
 * If you create an instance of this class, the magic bulldozer mode flag will
 *  be set to the given value.
 *  If the flag is set, clear tile commands will have magic bulldozer
 *  functionality.
 *  The original value of the flag is stored and recovered from when ever the
 *  instance is destroyed.
 * If the flag is set and is not valid during an action, the error
 *  ERR_PRECONDITION_INVALID_COMPANY will be returned.
 *  This will happen if the action is not commanded by a deity.
 * @api game
 */
class ScriptMagicBulldozerMode : public ScriptObject {
private:
	bool last_bulldozermode; ///< The previous value of the mode flag.

public:
	/**
	 * Creating instance of this class switches the value of the magic
	 *  bulldozer flag used when clearing tiles.
	 * @param mode Whether or not the magic bulldozer mode is switch on.
	 * @note When the instance is destroyed, it restores the value of the flag
	 *  that was current when the instance was created!
	 */
	ScriptMagicBulldozerMode(bool mode);

	/**
	 * Destroying this instance resets the mode flag to that what it was when
	 *  the instance was created.
	 */
	~ScriptMagicBulldozerMode();
};

#endif /* SCRIPT_MAGICBULLDOZERMODE_HPP */
