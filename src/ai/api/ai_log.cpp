/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_log.cpp Implementation of AILog. */

#include "../../stdafx.h"
#include "ai_log.hpp"
#include "../../core/alloc_func.hpp"
#include "../../company_func.h"
#include "../../debug.h"
#include "../../window_func.h"

/* static */ void AILog::Info(const char *message)
{
	AILog::Log(LOG_INFO, message);
}

/* static */ void AILog::Warning(const char *message)
{
	AILog::Log(LOG_WARNING, message);
}

/* static */ void AILog::Error(const char *message)
{
	AILog::Log(LOG_ERROR, message);
}

/* static */ void AILog::Log(AILog::AILogType level, const char *message)
{
	if (AIObject::GetLogPointer() == NULL) {
		AIObject::GetLogPointer() = new LogData();
		LogData *log = (LogData *)AIObject::GetLogPointer();

		log->lines = CallocT<char *>(80);
		log->type = CallocT<AILog::AILogType>(80);
		log->count = 80;
		log->pos = log->count - 1;
		log->used = 0;
	}
	LogData *log = (LogData *)AIObject::GetLogPointer();

	/* Go to the next log-line */
	log->pos = (log->pos + 1) % log->count;

	if (log->used != log->count) log->used++;

	/* Free last message, and write new message */
	free(log->lines[log->pos]);
	log->lines[log->pos] = strdup(message);
	log->type[log->pos] = level;

	/* Cut string after first \n */
	char *p;
	while ((p = strchr(log->lines[log->pos], '\n')) != NULL) {
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
	DEBUG(ai, level, "[%d] [%c] %s", (uint)_current_company, logc, log->lines[log->pos]);
	InvalidateWindowData(WC_AI_DEBUG, 0, _current_company);
}

/* static */ void AILog::FreeLogPointer()
{
	LogData *log = (LogData *)AIObject::GetLogPointer();

	for (int i = 0; i < log->count; i++) {
		free(log->lines[i]);
	}

	free(log->lines);
	free(log->type);
	delete log;
}
