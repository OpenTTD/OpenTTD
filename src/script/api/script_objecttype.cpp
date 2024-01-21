/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_objecttype.cpp Implementation of ScriptObjectType. */

#include "../../stdafx.h"

#include "script_objecttype.h"

#include "script_error.h"
#include "script_map.h"
#include "../../object_cmd.h"

#include "../../safeguards.h"

/* static */ bool ScriptObjectType::IsValidObjectType(ObjectType object_type)
{
	if (object_type >= ObjectSpec::Count()) return false;
	return ObjectSpec::Get(object_type)->IsEverAvailable();
}

/* static */ std::optional<std::string> ScriptObjectType::GetName(ObjectType object_type)
{
	EnforcePrecondition(std::nullopt, IsValidObjectType(object_type));

	return GetString(ObjectSpec::Get(object_type)->name);
}

/* static */ SQInteger ScriptObjectType::GetViews(ObjectType object_type)
{
	EnforcePrecondition(0, IsValidObjectType(object_type));

	return ObjectSpec::Get(object_type)->views;
}

/* static */ bool ScriptObjectType::BuildObject(ObjectType object_type, SQInteger view, TileIndex tile)
{
	EnforceDeityOrCompanyModeValid(false);
	EnforcePrecondition(false, IsValidObjectType(object_type));
	EnforcePrecondition(false, view >= 0 && view < GetViews(object_type));
	EnforcePrecondition(false, ScriptMap::IsValidTile(tile));

	return ScriptObject::Command<CMD_BUILD_OBJECT>::Do(tile, object_type, view);
}

/* static */ ObjectType ScriptObjectType::ResolveNewGRFID(SQInteger grfid, SQInteger grf_local_id)
{
	EnforcePrecondition(INVALID_OBJECT_TYPE, IsInsideBS(grf_local_id, 0x00, NUM_OBJECTS_PER_GRF));

	grfid = BSWAP32(GB(grfid, 0, 32)); // Match people's expectations.
	return _object_mngr.GetID(grf_local_id, grfid);
}
