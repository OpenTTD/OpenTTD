/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_log.cpp Implementation of ScriptLog. */

#include "../../stdafx.h"
#include "script_log.hpp"
#include "../../core/alloc_func.hpp"
#include "../../debug.h"
#include "../../window_func.h"
#include "../../string_func.h"

#include "../../safeguards.h"

/* static */ void ScriptLog::Info(const char *message)
{
	ScriptLog::Log(LOG_INFO, message);
}

/* static */ void ScriptLog::Warning(const char *message)
{
	ScriptLog::Log(LOG_WARNING, message);
}

/* static */ void ScriptLog::Error(const char *message)
{
	ScriptLog::Log(LOG_ERROR, message);
}

/* static */ void ScriptLog::Log(ScriptLog::ScriptLogType level, const char *message)
{
	if (ScriptObject::GetLogPointer() == nullptr) {
		ScriptObject::GetLogPointer() = new LogData();
		LogData *log = (LogData *)ScriptObject::GetLogPointer();

		log->lines = CallocT<char *>(400);
		log->type = CallocT<ScriptLog::ScriptLogType>(400);
		log->count = 400;
		log->pos = log->count - 1;
		log->used = 0;
	}
	LogData *log = (LogData *)ScriptObject::GetLogPointer();

	/* Go to the next log-line */
	log->pos = (log->pos + 1) % log->count;

	if (log->used != log->count) log->used++;

	/* Free last message, and write new message */
	free(log->lines[log->pos]);
	log->lines[log->pos] = stredup(message);
	log->type[log->pos] = level;

	/* Cut string after first \n */
	char *p;
	while ((p = strchr(log->lines[log->pos], '\n')) != nullptr) {
		*p = '\0';
		break;
	}

	char logc;

	switch (level) {
		case LOG_SQ_ERROR: logc = 'S'; break;
		case LOG_ERROR:    logc = 'E'; break;
		case LOG_SQ_INFO:  logc = 'P'; break;
		case LOG_WARNING:  logc = 'W'; break;
		case LOG_INFO:     logc = 'I'; break;
		default:           logc = '?'; break;
	}

	/* Also still print to debug window */
	DEBUG(script, level, "[%d] [%c] %s", (uint)ScriptObject::GetRootCompany(), logc, log->lines[log->pos]);
	InvalidateWindowData(WC_AI_DEBUG, 0, ScriptObject::GetRootCompany());
}

/* static */ void ScriptLog::FreeLogPointer()
{
	LogData *log = (LogData *)ScriptObject::GetLogPointer();

	for (int i = 0; i < log->count; i++) {
		free(log->lines[i]);
	}

	free(log->lines);
	free(log->type);
	delete log;
}
