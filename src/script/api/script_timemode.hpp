/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_timemode.hpp Switch the time mode. */

#ifndef SCRIPT_TIMEMODE_HPP
#define SCRIPT_TIMEMODE_HPP

#include "script_object.hpp"

/**
 * Class to switch the current time.
 * If you create an instance of this class, the mode will be switched to either calendar time or economy time mode.
 * @note Destroying this object will restore the previous time mode.
 * @api ai game
 */
class ScriptTimeMode : public ScriptObject {
private:
	bool last_time_mode; ///< The last time mode we were using.
public:
	/**
	 * Creating an instance of this class switches the time mode used for queries and commands.
	 * Calendar time is used by OpenTTD for technology like vehicle introductions and expiration, and variable snowline. It can be sped up or slowed down by the player.
	 * Economy time always runs at the same pace and handles things like cargo production, everything related to money, etc.
	 * @param Calendar Should we use calendar time mode? (Set to false for economy time mode.)
	 */
	ScriptTimeMode(bool calendar);

	/**
	 * Destroying this instance resets the time mode to the mode it was in when the instance was created.
	 */
	~ScriptTimeMode();

	/**
	 * Check if the script is operating in calendar time mode, or in economy time mode. See ScriptTimeMode() for more information.
	 * @return True if we are in calendar time mode, false if we are in economy time mode.
	 */
	static bool IsCalendarMode();
};

#endif /* SCRIPT_TIMEMODE_HPP */
