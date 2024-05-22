/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_asyncmode.hpp Switch the script instance to Async Mode. */

#ifndef SCRIPT_ASYNCMODE_HPP
#define SCRIPT_ASYNCMODE_HPP

#include "script_object.hpp"

/**
 * Class to switch current mode to Async Mode.
 * If you create an instance of this class, the mode will be switched to
 *   either Asynchronous or Non-Asynchronous mode.
 *   The original mode is stored and recovered from when ever the instance is destroyed.
 * In Asynchronous mode all the commands you execute are queued for later execution. The
 *   system checks if it would be able to execute your requests, and returns what
 *   the cost would be. The actual cost and whether the command succeeded when the command
 *   is eventually executed may differ from what was reported to the script.
 * @api game
 */
class ScriptAsyncMode : public ScriptObject {
private:
	ScriptAsyncModeProc *last_mode; ///< The previous mode we were in.
	ScriptObject *last_instance;    ///< The previous instance of the mode.

protected:
	static bool AsyncModeProc();
	static bool NonAsyncModeProc();

public:
#ifndef DOXYGEN_API
	/**
	 * The constructor wrapper from Squirrel.
	 */
	ScriptAsyncMode(HSQUIRRELVM vm);
#else
	/**
	 * Creating instance of this class switches the build mode to Asynchronous or Non-Asynchronous (normal).
	 * @note When the instance is destroyed, it restores the mode that was
	 *   current when the instance was created!
	 * @param asynchronous Whether the new mode should be Asynchronous, if true, or Non-Asynchronous, if false.
	 */
	ScriptAsyncMode(bool asynchronous);
#endif /* DOXYGEN_API */

	/**
	 * Destroying this instance resets the asynchronous mode to the mode it was
	 *   in when the instance was created.
	 */
	~ScriptAsyncMode();

	/**
	 * @api -all
	 */
	void FinalRelease() override;
};

#endif /* SCRIPT_ASYNCMODE_HPP */
