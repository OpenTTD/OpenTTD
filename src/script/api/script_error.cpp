/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_error.cpp Implementation of ScriptError. */

#include "../../stdafx.h"
#include "script_error.hpp"
#include "../../core/bitmath_func.hpp"
#include "../../string_func.h"
#include "../../strings_func.h"

#include "../../safeguards.h"

ScriptError::ScriptErrorMap ScriptError::error_map = ScriptError::ScriptErrorMap();
ScriptError::ScriptErrorMapString ScriptError::error_map_string = ScriptError::ScriptErrorMapString();

/* static */ ScriptErrorType ScriptError::GetLastError()
{
	return ScriptObject::GetLastError();
}

/* static */ char *ScriptError::GetLastErrorString()
{
	return stredup((*error_map_string.find(ScriptError::GetLastError())).second);
}

/* static */ ScriptErrorType ScriptError::StringToError(StringID internal_string_id)
{
	uint index = GetStringIndex(internal_string_id);
	switch (GetStringTab(internal_string_id)) {
		case TEXT_TAB_NEWGRF_START:
		case TEXT_TAB_GAMESCRIPT_START:
			return ERR_NEWGRF_SUPPLIED_ERROR; // NewGRF strings.

		case TEXT_TAB_SPECIAL:
			if (index < 0xE4) break; // Player name
			/* FALL THROUGH */
		case TEXT_TAB_TOWN:
			if (index < 0xC0) break; // Town name
			/* These strings are 'random' and have no meaning.
			 * They actually shouldn't even be returned as error messages. */
			return ERR_UNKNOWN;

		default:
			break;
	}

	ScriptErrorMap::iterator it = error_map.find(internal_string_id);
	if (it == error_map.end()) return ERR_UNKNOWN;
	return (*it).second;
}

/* static */ void ScriptError::RegisterErrorMap(StringID internal_string_id, ScriptErrorType ai_error_msg)
{
	error_map[internal_string_id] = ai_error_msg;
}

/* static */ void ScriptError::RegisterErrorMapString(ScriptErrorType ai_error_msg, const char *message)
{
	error_map_string[ai_error_msg] = message;
}

/* static */ ScriptError::ErrorCategories ScriptError::GetErrorCategory()
{
	return (ScriptError::ErrorCategories)(GetLastError() >> (uint)ERR_CAT_BIT_SIZE);
}
