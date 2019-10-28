/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_fatalerror.hpp The definition of Script_FatalError. */

#ifndef SCRIPT_FATALERROR_HPP
#define SCRIPT_FATALERROR_HPP

/**
 * A throw-class that is given when the script made a fatal error.
 */
class Script_FatalError {
public:
	/**
	 * Creates a "fatal error" exception.
	 * @param msg The message describing the cause of the fatal error.
	 */
	Script_FatalError(const char *msg) :
		msg(msg)
	{}

	/**
	 * The error message associated with the fatal error.
	 * @return The error message.
	 */
	const char *GetErrorMessage() { return msg; }

private:
	const char *msg; ///< The error message.
};

#endif /* SCRIPT_FATALERROR_HPP */
