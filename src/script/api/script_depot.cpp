/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_depot.cpp Implementation of ScriptDepot. */

#include "../../stdafx.h"

#include "script_depot.hpp"

#include "../../depot_base.h"
#include "../../depot_cmd.h"
#include "../../depot_map.h"
#include "../../strings_func.h"
#include "script_object.hpp"

#include "table/strings.h"

#include "../../safeguards.h"

/* static */ std::optional<std::string> ScriptDepot::GetName(TileIndex tile)
{
	if (!IsRailDepotTile(tile) && !IsRoadDepotTile(tile) && !IsShipDepotTile(tile)) return std::nullopt;
	VehicleType type = GetDepotVehicleType(tile);

	return ::StrMakeValid(::GetString(STR_DEPOT_NAME, type, ::Depot::GetByTile(tile)->index), {});
}

/* static */ bool ScriptDepot::SetName(TileIndex tile, Text *name)
{
	ScriptObjectRef counter(name);

	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, IsRailDepotTile(tile) || IsRoadDepotTile(tile) || IsShipDepotTile(tile));
	EnforcePrecondition(false, name != nullptr);
	const std::string &text = name->GetDecodedText();
	EnforcePreconditionEncodedText(false, text);
	EnforcePreconditionCustomError(false, ::Utf8StringLength(text) < MAX_LENGTH_DEPOT_NAME_CHARS, ScriptError::ERR_PRECONDITION_STRING_TOO_LONG);

	return ScriptObject::Command<Commands::RenameDepot>::Do(::Depot::GetByTile(tile)->index, text);
}
