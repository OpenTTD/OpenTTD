/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_execmode.hpp Switch the script to Execute Mode. */

#ifndef SCRIPT_EXECMODE_HPP
#define SCRIPT_EXECMODE_HPP

#include "script_object.hpp"

/**
 * Class to switch current mode to Execute Mode.
 * If you create an instance of this class, the mode will be switched to
 *   Execute. The original mode is stored and recovered from when ever the
 *   instance is destroyed.
 * In Execute mode all commands you do are executed for real.
 * @api ai game
 */
class ScriptExecMode : public ScriptObject {
private:
	ScriptModeProc *last_mode;   ///< The previous mode we were in.
	ScriptObject *last_instance; ///< The previous instance of the mode.

protected:
	/**
	 * The callback proc for Execute mode.
	 */
	static bool ModeProc();

public:
	/**
	 * Creating instance of this class switches the build mode to Execute.
	 * @note When the instance is destroyed, it restores the mode that was
	 *   current when the instance was created!
	 */
	ScriptExecMode();

	/**
	 * Destroying this instance reset the building mode to the mode it was
	 *   in when the instance was created.
	 */
	~ScriptExecMode();

	/**
	 * @api -all
	 */
	void FinalRelease() override;
};

#endif /* SCRIPT_EXECMODE_HPP */
