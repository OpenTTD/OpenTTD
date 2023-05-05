/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_log.hpp Everything to handle and issue log messages. */

#ifndef SCRIPT_LOG_HPP
#define SCRIPT_LOG_HPP

#include "script_object.hpp"

/**
 * Class that handles all log related functions.
 * @api ai game
 */
class ScriptLog : public ScriptObject {
	/* ScriptController needs access to Enum and Log, in order to keep the flow from
	 *  OpenTTD core to script API clear and simple. */
	friend class ScriptController;

public:
	/**
	 * Print an Info message to the logs.
	 * @param message The message to log.
	 * @note Special characters such as U+0000-U+0019 and U+E000-U+E1FF are not supported and removed or replaced by a question mark. This includes newlines and tabs.
	 */
	static void Info(const std::string &message);

	/**
	 * Print a Warning message to the logs.
	 * @param message The message to log.
	 * @note Special characters such as U+0000-U+0019 and U+E000-U+E1FF are not supported and removed or replaced by a question mark. This includes newlines and tabs.
	 */
	static void Warning(const std::string &message);

	/**
	 * Print an Error message to the logs.
	 * @param message The message to log.
	 * @note Special characters such as U+0000-U+0019 and U+E000-U+E1FF are not supported and removed or replaced by a question mark. This includes newlines and tabs.
	 */
	static void Error(const std::string &message);

private:
	/**
	 * Internal command to log the message in a common way.
	 */
	static void Log(ScriptLogTypes::ScriptLogType level, const std::string &message);
};

#endif /* SCRIPT_LOG_HPP */
