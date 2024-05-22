/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_log.cpp Implementation of ScriptLog. */

#include "../../stdafx.h"
#include "script_log_types.hpp"
#include "script_log.hpp"
#include "../../core/alloc_func.hpp"
#include "../../debug.h"
#include "../../window_func.h"
#include "../../string_func.h"

#include "../../safeguards.h"

/* static */ void ScriptLog::Info(const std::string &message)
{
	ScriptLog::Log(ScriptLogTypes::LOG_INFO, message);
}

/* static */ void ScriptLog::Warning(const std::string &message)
{
	ScriptLog::Log(ScriptLogTypes::LOG_WARNING, message);
}

/* static */ void ScriptLog::Error(const std::string &message)
{
	ScriptLog::Log(ScriptLogTypes::LOG_ERROR, message);
}

/* static */ void ScriptLog::Log(ScriptLogTypes::ScriptLogType level, const std::string &message)
{
	ScriptLogTypes::LogData &logdata = ScriptObject::GetLogData();

	/* Limit the log to 400 lines. */
	if (logdata.size() >= 400U) logdata.pop_front();

	auto &line = logdata.emplace_back();
	line.type = level;

	/* Cut string after first \n */
	line.text = message.substr(0, message.find_first_of('\n'));

	char logc;

	switch (level) {
		case ScriptLogTypes::LOG_SQ_ERROR: logc = 'S'; break;
		case ScriptLogTypes::LOG_ERROR:    logc = 'E'; break;
		case ScriptLogTypes::LOG_SQ_INFO:  logc = 'P'; break;
		case ScriptLogTypes::LOG_WARNING:  logc = 'W'; break;
		case ScriptLogTypes::LOG_INFO:     logc = 'I'; break;
		default:                           logc = '?'; break;
	}

	/* Also still print to debug window */
	Debug(script, level, "[{}] [{}] {}", (uint)ScriptObject::GetRootCompany(), logc, line.text);
	InvalidateWindowClassesData(WC_SCRIPT_DEBUG, ScriptObject::GetRootCompany());
}
