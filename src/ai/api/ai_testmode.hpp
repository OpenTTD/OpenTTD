/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_testmode.hpp Switch the AI to Test Mode. */

#ifndef AI_TESTMODE_HPP
#define AI_TESTMODE_HPP

#include "ai_object.hpp"

/**
 * Class to switch current mode to Test Mode.
 * If you create an instance of this class, the mode will be switched to
 *   Testing. The original mode is stored and recovered from when ever the
 *   instance is destroyed.
 * In Test mode all the commands you execute aren't really executed. The
 *   system only checks if it would be able to execute your requests, and what
 *   the cost would be.
 */
class AITestMode : public AIObject {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AITestMode"; }

private:
	AIModeProc *last_mode;   ///< The previous mode we were in.
	AIObject *last_instance; ///< The previous instace of the mode.

protected:
	/**
	 * The callback proc for Testing mode.
	 */
	static bool ModeProc();

public:
	/**
	 * Creating instance of this class switches the build mode to Testing.
	 * @note When the instance is destroyed, he restores the mode that was
	 *   current when the instance was created!
	 */
	AITestMode();

	/**
	 * Destroying this instance reset the building mode to the mode it was
	 *   in when the instance was created.
	 */
	~AITestMode();
};

#endif /* AI_TESTMODE_HPP */
