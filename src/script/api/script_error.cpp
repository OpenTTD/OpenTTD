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

ScriptError::ScriptErrorMap ScriptError::error_map = ScriptError::ScriptErrorMap();
ScriptError::ScriptErrorMapString ScriptError::error_map_string = ScriptError::ScriptErrorMapString();

/* static */ ScriptErrorType ScriptError::GetLastError()
{
	return ScriptObject::GetLastError();
}

/* static */ char *ScriptError::GetLastErrorString()
{
	return strdup((*error_map_string.find(ScriptError::GetLastError())).second);
}

/* static */ ScriptErrorType ScriptError::StringToError(StringID internal_string_id)
{
	uint index = GB(internal_string_id, 11, 5);
	switch (GB(internal_string_id, 11, 5)) {
		case 26: case 28: case 29: case 30: // NewGRF strings.
			return ERR_NEWGRF_SUPPLIED_ERROR;

		/* DO NOT SWAP case 14 and 4 because that will break StringToError due
		 * to the index dependency that relies on FALL THROUGHs. */
		case 14: if (index < 0xE4) break; // Player name
		case 4:  if (index < 0xC0) break; // Town name
		case 15: // Custom name
		case 31: // Dynamic strings
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
