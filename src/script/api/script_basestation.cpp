/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_basestation.cpp Implementation of ScriptBaseStation. */

#include "../../stdafx.h"
#include "script_basestation.hpp"
#include "script_error.hpp"
#include "../../station_base.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../station_cmd.h"
#include "../../waypoint_cmd.h"
#include "../../timer/timer_game_calendar.h"
#include "table/strings.h"

#include "../../safeguards.h"

/* static */ bool ScriptBaseStation::IsValidBaseStation(StationID station_id)
{
	EnforceDeityOrCompanyModeValid(false);
	const BaseStation *st = ::BaseStation::GetIfValid(station_id);
	return st != nullptr && (st->owner == ScriptObject::GetCompany() || ScriptCompanyMode::IsDeity() || st->owner == OWNER_NONE);
}

/* static */ std::optional<std::string> ScriptBaseStation::GetName(StationID station_id)
{
	if (!IsValidBaseStation(station_id)) return std::nullopt;

	::SetDParam(0, station_id);
	return GetString(::Station::IsValidID(station_id) ? STR_STATION_NAME : STR_WAYPOINT_NAME);
}

/* static */ bool ScriptBaseStation::SetName(StationID station_id, Text *name)
{
	CCountedPtr<Text> counter(name);

	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, IsValidBaseStation(station_id));
	EnforcePrecondition(false, name != nullptr);
	const std::string &text = name->GetDecodedText();
	EnforcePreconditionEncodedText(false, text);
	EnforcePreconditionCustomError(false, ::Utf8StringLength(text) < MAX_LENGTH_STATION_NAME_CHARS, ScriptError::ERR_PRECONDITION_STRING_TOO_LONG);

	if (::Station::IsValidID(station_id)) {
		return ScriptObject::Command<CMD_RENAME_STATION>::Do(station_id, text);
	} else {
		return ScriptObject::Command<CMD_RENAME_WAYPOINT>::Do(station_id, text);
	}
}

/* static */ TileIndex ScriptBaseStation::GetLocation(StationID station_id)
{
	if (!IsValidBaseStation(station_id)) return INVALID_TILE;

	return ::BaseStation::Get(station_id)->xy;
}

/* static */ ScriptDate::Date ScriptBaseStation::GetConstructionDate(StationID station_id)
{
	if (!IsValidBaseStation(station_id)) return ScriptDate::DATE_INVALID;

	return (ScriptDate::Date)::BaseStation::Get(station_id)->build_date.base();
}
