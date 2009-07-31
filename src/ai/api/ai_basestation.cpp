/* $Id$ */

/** @file ai_basestation.cpp Implementation of AIBaseStation. */

#include "ai_basestation.hpp"
#include "../../base_station_base.h"
#include "../../station_base.h"
#include "../../command_func.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../company_func.h"
#include "../../core/alloc_func.hpp"
#include "table/strings.h"

/* static */ bool AIBaseStation::IsValidBaseStation(StationID station_id)
{
	const BaseStation *st = ::BaseStation::GetIfValid(station_id);
	return st != NULL && st->owner == _current_company;
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
	EnforcePreconditionCustomError(false, ::strlen(name) < MAX_LENGTH_STATION_NAME_BYTES, AIError::ERR_PRECONDITION_STRING_TOO_LONG);

	return AIObject::DoCommand(0, station_id, 0, ::Station::IsValidID(station_id) ? CMD_RENAME_STATION : CMD_RENAME_WAYPOINT, name);
}

/* static */ TileIndex AIBaseStation::GetLocation(StationID station_id)
{
	if (!IsValidBaseStation(station_id)) return INVALID_TILE;

	return ::BaseStation::Get(station_id)->xy;
}
