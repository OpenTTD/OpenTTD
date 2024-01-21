/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_suspend.h The Script_Suspend tracks the suspension of a script. */

#ifndef SCRIPT_SUSPEND_HPP
#define SCRIPT_SUSPEND_HPP

/**
 * The callback function when a script suspends.
 */
typedef void (Script_SuspendCallbackProc)(class ScriptInstance *instance);

/**
 * A throw-class that is given when the script wants to suspend.
 */
class Script_Suspend {
public:
	/**
	 * Create the suspend exception.
	 * @param time The amount of ticks to suspend.
	 * @param callback The callback to call when the script may resume again.
	 */
	Script_Suspend(int time, Script_SuspendCallbackProc *callback) :
		time(time),
		callback(callback)
	{}

	/**
	 * Get the amount of ticks the script should be suspended.
	 * @return The amount of ticks to suspend the script.
	 */
	int GetSuspendTime() { return time; }

	/**
	 * Get the callback to call when the script can run again.
	 * @return The callback function to run.
	 */
	Script_SuspendCallbackProc *GetSuspendCallback() { return callback; }

private:
	int time;                             ///< Amount of ticks to suspend the script.
	Script_SuspendCallbackProc *callback; ///< Callback function to call when the script can run again.
};

#endif /* SCRIPT_SUSPEND_HPP */
