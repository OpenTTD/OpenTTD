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
	 * Log levels; The value is also feed to DEBUG() lvl.
	 *  This has no use for you, as script writer.
	 * @api -all
	 */
	enum ScriptLogType {
		LOG_SQ_ERROR = 0, ///< Squirrel printed an error.
		LOG_ERROR = 1,    ///< User printed an error.
		LOG_SQ_INFO = 2,  ///< Squirrel printed some info.
		LOG_WARNING = 3,  ///< User printed some warning.
		LOG_INFO = 4,     ///< User printed some info.
	};

	/**
	 * Internal representation of the log-data inside the script.
	 *  This has no use for you, as script writer.
	 * @api -all
	 */
	struct LogData {
		char **lines;           ///< The log-lines.
		ScriptLog::ScriptLogType *type; ///< Per line, which type of log it was.
		int count;              ///< Total amount of log-lines possible.
		int pos;                ///< Current position in lines.
		int used;               ///< Total amount of used log-lines.
	};

	/**
	 * Print an Info message to the logs.
	 * @param message The message to log.
	 * @note Special characters such as U+0000-U+0019 and U+E000-U+E1FF are not supported and removed or replaced by a question mark. This includes newlines and tabs.
	 */
	static void Info(const char *message);

	/**
	 * Print a Warning message to the logs.
	 * @param message The message to log.
	 * @note Special characters such as U+0000-U+0019 and U+E000-U+E1FF are not supported and removed or replaced by a question mark. This includes newlines and tabs.
	 */
	static void Warning(const char *message);

	/**
	 * Print an Error message to the logs.
	 * @param message The message to log.
	 * @note Special characters such as U+0000-U+0019 and U+E000-U+E1FF are not supported and removed or replaced by a question mark. This includes newlines and tabs.
	 */
	static void Error(const char *message);

	/**
	 * Free the log pointer.
	 * @api -all
	 */
	static void FreeLogPointer();

private:
	/**
	 * Internal command to log the message in a common way.
	 */
	static void Log(ScriptLog::ScriptLogType level, const char *message);
};

#endif /* SCRIPT_LOG_HPP */
