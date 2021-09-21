/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_objecttype.cpp Implementation of ScriptObjectType. */

#include "../../stdafx.h"

#include "script_objecttype.hpp"

#include "script_error.hpp"
#include "script_map.hpp"

#include "../../safeguards.h"

/* static */ bool ScriptObjectType::IsValidObjectType(ObjectType object_type)
{
	if (object_type >= NUM_OBJECTS) return false;
	return ObjectSpec::Get(object_type)->IsEverAvailable();
}

/* static */ char *ScriptObjectType::GetName(ObjectType object_type)
{
	EnforcePrecondition(nullptr, IsValidObjectType(object_type));

	return GetString(ObjectSpec::Get(object_type)->name);
}

/* static */ uint8 ScriptObjectType::GetViews(ObjectType object_type)
{
	EnforcePrecondition(0, IsValidObjectType(object_type));

	return ObjectSpec::Get(object_type)->views;
}

/* static */ bool ScriptObjectType::BuildObject(ObjectType object_type, uint8 view, TileIndex tile)
{
	EnforcePrecondition(false, IsValidObjectType(object_type));
	EnforcePrecondition(false, ScriptMap::IsValidTile(tile));

	return ScriptObject::DoCommand(tile, object_type, view, CMD_BUILD_OBJECT);
}
