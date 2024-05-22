/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_log_types.hpp Data types for script log messages. */

#ifndef SCRIPT_LOG_TYPES_HPP
#define SCRIPT_LOG_TYPES_HPP

namespace ScriptLogTypes {
	/**
	 * Log levels; The value is also feed to Debug() lvl.
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
	struct LogLine {
		std::string text; ///< The text
		ScriptLogType type; ///< Text type
		uint width; ///< The text width
	};

	/**
	 * Internal representation of the log-data inside the script.
	 *  This has no use for you, as script writer.
	 * @api -all
	 */
	using LogData = std::deque<LogLine>; ///< The log type
};

#endif /* SCRIPT_LOG_TYPES_HPP */
