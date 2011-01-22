/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_basestation.cpp Implementation of AIBaseStation. */

#include "../../stdafx.h"
#include "ai_basestation.hpp"
#include "../../station_base.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../company_func.h"
#include "table/strings.h"

/* static */ bool AIBaseStation::IsValidBaseStation(StationID station_id)
{
	const BaseStation *st = ::BaseStation::GetIfValid(station_id);
	return st != NULL && (st->owner == _current_company || st->owner == OWNER_NONE);
}

/* static */ char *AIBaseStation::GetName(StationID station_id)
{
	if (!IsValidBaseStation(station_id)) return NULL;

	static const int len = 64;
	char *name = MallocT<char>(len);

	::SetDParam(0, station_id);
	::GetString(name, ::Station::IsValidID(station_id) ? STR_STATION_NAME : STR_WAYPOINT_NAME, &name[len - 1]);
	return name;
}

/* static */ bool AIBaseStation::SetName(StationID station_id, const char *name)
{
	EnforcePrecondition(false, IsValidBaseStation(station_id));
	EnforcePrecondition(false, !::StrEmpty(name));
	EnforcePreconditionCustomError(false, ::Utf8StringLength(name) < MAX_LENGTH_STATION_NAME_CHARS, AIError::ERR_PRECONDITION_STRING_TOO_LONG);

	return AIObject::DoCommand(0, station_id, 0, ::Station::IsValidID(station_id) ? CMD_RENAME_STATION : CMD_RENAME_WAYPOINT, name);
}

/* static */ TileIndex AIBaseStation::GetLocation(StationID station_id)
{
	if (!IsValidBaseStation(station_id)) return INVALID_TILE;

	return ::BaseStation::Get(station_id)->xy;
}

/* static */ int32 AIBaseStation::GetConstructionDate(StationID station_id)
{
	if (!IsValidBaseStation(station_id)) return -1;

	return ::BaseStation::Get(station_id)->build_date;
}
