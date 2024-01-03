/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_testmode.hpp Switch the script instance to Test Mode. */

#ifndef SCRIPT_TESTMODE_HPP
#define SCRIPT_TESTMODE_HPP

#include "script_object.hpp"

/**
 * Class to switch current mode to Test Mode.
 * If you create an instance of this class, the mode will be switched to
 *   Testing. The original mode is stored and recovered from when ever the
 *   instance is destroyed.
 * In Test mode all the commands you execute aren't really executed. The
 *   system only checks if it would be able to execute your requests, and what
 *   the cost would be.
 * @api ai game
 */
class ScriptTestMode : public ScriptObject {
private:
	ScriptModeProc *last_mode;   ///< The previous mode we were in.
	ScriptObject *last_instance; ///< The previous instance of the mode.

protected:
	/**
	 * The callback proc for Testing mode.
	 */
	static bool ModeProc();

public:
	/**
	 * Creating instance of this class switches the build mode to Testing.
	 * @note When the instance is destroyed, it restores the mode that was
	 *   current when the instance was created!
	 */
	ScriptTestMode();

	/**
	 * Destroying this instance reset the building mode to the mode it was
	 *   in when the instance was created.
	 */
	~ScriptTestMode();

	/**
	 * @api -all
	 */
	void FinalRelease() override;
};

#endif /* SCRIPT_TESTMODE_HPP */
