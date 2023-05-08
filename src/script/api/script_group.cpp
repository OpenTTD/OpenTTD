/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_group.cpp Implementation of ScriptGroup. */

#include "../../stdafx.h"
#include "script_group.hpp"
#include "script_engine.hpp"
#include "../script_instance.hpp"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../autoreplace_func.h"
#include "../../settings_func.h"
#include "../../vehicle_base.h"
#include "../../autoreplace_cmd.h"
#include "../../group_cmd.h"
#include "../../settings_cmd.h"
#include "table/strings.h"

#include "../../safeguards.h"

/* static */ bool ScriptGroup::IsValidGroup(GroupID group_id)
{
	EnforceDeityOrCompanyModeValid(false);
	const Group *g = ::Group::GetIfValid(group_id);
	return g != nullptr && g->owner == ScriptObject::GetCompany();
}

/* static */ ScriptGroup::GroupID ScriptGroup::CreateGroup(ScriptVehicle::VehicleType vehicle_type, GroupID parent_group_id)
{
	EnforceCompanyModeValid(GROUP_INVALID);
	if (!ScriptObject::Command<CMD_CREATE_GROUP>::Do(&ScriptInstance::DoCommandReturnGroupID, (::VehicleType)vehicle_type, parent_group_id)) return GROUP_INVALID;

	/* In case of test-mode, we return GroupID 0 */
	return (ScriptGroup::GroupID)0;
}

/* static */ bool ScriptGroup::DeleteGroup(GroupID group_id)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, IsValidGroup(group_id));

	return ScriptObject::Command<CMD_DELETE_GROUP>::Do(group_id);
}

/* static */ ScriptVehicle::VehicleType ScriptGroup::GetVehicleType(GroupID group_id)
{
	if (!IsValidGroup(group_id)) return ScriptVehicle::VT_INVALID;

	return (ScriptVehicle::VehicleType)((::VehicleType)::Group::Get(group_id)->vehicle_type);
}

/* static */ bool ScriptGroup::SetName(GroupID group_id, Text *name)
{
	CCountedPtr<Text> counter(name);

	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, IsValidGroup(group_id));
	EnforcePrecondition(false, name != nullptr);
	const std::string &text = name->GetDecodedText();
	EnforcePreconditionEncodedText(false, text);
	EnforcePreconditionCustomError(false, ::Utf8StringLength(text) < MAX_LENGTH_GROUP_NAME_CHARS, ScriptError::ERR_PRECONDITION_STRING_TOO_LONG);

	return ScriptObject::Command<CMD_ALTER_GROUP>::Do(AlterGroupMode::Rename, group_id, 0, text);
}

/* static */ std::optional<std::string> ScriptGroup::GetName(GroupID group_id)
{
	if (!IsValidGroup(group_id)) return std::nullopt;

	::SetDParam(0, group_id);
	return GetString(STR_GROUP_NAME);
}

/* static */ bool ScriptGroup::SetParent(GroupID group_id, GroupID parent_group_id)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, IsValidGroup(group_id));
	EnforcePrecondition(false, IsValidGroup(parent_group_id));

	return ScriptObject::Command<CMD_ALTER_GROUP>::Do(AlterGroupMode::SetParent, group_id, parent_group_id, {});
}

/* static */ ScriptGroup::GroupID ScriptGroup::GetParent(GroupID group_id)
{
	EnforcePrecondition((ScriptGroup::GroupID)INVALID_GROUP, IsValidGroup(group_id));

	const Group *g = ::Group::GetIfValid(group_id);
	return (ScriptGroup::GroupID)g->parent;
}

/* static */ bool ScriptGroup::EnableAutoReplaceProtection(GroupID group_id, bool enable)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, IsValidGroup(group_id));

	return ScriptObject::Command<CMD_SET_GROUP_FLAG>::Do(group_id, GroupFlags::GF_REPLACE_PROTECTION, enable, false);
}

/* static */ bool ScriptGroup::GetAutoReplaceProtection(GroupID group_id)
{
	if (!IsValidGroup(group_id)) return false;

	return HasBit(::Group::Get(group_id)->flags, GroupFlags::GF_REPLACE_PROTECTION);
}

/* static */ SQInteger ScriptGroup::GetNumEngines(GroupID group_id, EngineID engine_id)
{
	EnforceCompanyModeValid(-1);
	if (!IsValidGroup(group_id) && group_id != GROUP_DEFAULT && group_id != GROUP_ALL) return -1;

	return GetGroupNumEngines(ScriptObject::GetCompany(), group_id, engine_id);
}

/* static */ SQInteger ScriptGroup::GetNumVehicles(GroupID group_id, ScriptVehicle::VehicleType vehicle_type)
{
	EnforceCompanyModeValid(-1);
	bool valid_group = IsValidGroup(group_id);
	if (!valid_group && group_id != GROUP_DEFAULT && group_id != GROUP_ALL) return -1;
	if (!valid_group && (vehicle_type < ScriptVehicle::VT_RAIL || vehicle_type > ScriptVehicle::VT_AIR)) return -1;

	return GetGroupNumVehicle(ScriptObject::GetCompany(), group_id, valid_group ? ::Group::Get(group_id)->vehicle_type : (::VehicleType)vehicle_type);
}

/* static */ bool ScriptGroup::MoveVehicle(GroupID group_id, VehicleID vehicle_id)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, IsValidGroup(group_id) || group_id == GROUP_DEFAULT);
	EnforcePrecondition(false, ScriptVehicle::IsPrimaryVehicle(vehicle_id));

	return ScriptObject::Command<CMD_ADD_VEHICLE_GROUP>::Do(group_id, vehicle_id, false, VehicleListIdentifier{});
}

/* static */ bool ScriptGroup::EnableWagonRemoval(bool enable_removal)
{
	EnforceCompanyModeValid(false);
	if (HasWagonRemoval() == enable_removal) return true;

	return ScriptObject::Command<CMD_CHANGE_COMPANY_SETTING>::Do("company.renew_keep_length", enable_removal ? 1 : 0);
}

/* static */ bool ScriptGroup::HasWagonRemoval()
{
	EnforceCompanyModeValid(false);
	return ::Company::Get(ScriptObject::GetCompany())->settings.renew_keep_length;
}

/* static */ bool ScriptGroup::SetAutoReplace(GroupID group_id, EngineID engine_id_old, EngineID engine_id_new)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, IsValidGroup(group_id) || group_id == GROUP_DEFAULT || group_id == GROUP_ALL);
	EnforcePrecondition(false, ScriptEngine::IsBuildable(engine_id_new));

	return ScriptObject::Command<CMD_SET_AUTOREPLACE>::Do(group_id, engine_id_old, engine_id_new, false);
}

/* static */ EngineID ScriptGroup::GetEngineReplacement(GroupID group_id, EngineID engine_id)
{
	EnforceCompanyModeValid(::INVALID_ENGINE);
	if (!IsValidGroup(group_id) && group_id != GROUP_DEFAULT && group_id != GROUP_ALL) return ::INVALID_ENGINE;

	return ::EngineReplacementForCompany(Company::Get(ScriptObject::GetCompany()), engine_id, group_id);
}

/* static */ bool ScriptGroup::StopAutoReplace(GroupID group_id, EngineID engine_id)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, IsValidGroup(group_id) || group_id == GROUP_DEFAULT || group_id == GROUP_ALL);

	return ScriptObject::Command<CMD_SET_AUTOREPLACE>::Do(group_id, engine_id, ::INVALID_ENGINE, false);
}

/* static */ Money ScriptGroup::GetProfitThisYear(GroupID group_id)
{
	if (!IsValidGroup(group_id)) return -1;

	Money profit = 0;

	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->group_id != group_id) continue;
		if (!v->IsPrimaryVehicle()) continue;

		profit += v->GetDisplayProfitThisYear();
	}

	return profit;
}

/* static */ Money ScriptGroup::GetProfitLastYear(GroupID group_id)
{
	if (!IsValidGroup(group_id)) return -1;

	return ::Group::Get(group_id)->statistics.profit_last_year;
}

/* static */ SQInteger ScriptGroup::GetCurrentUsage(GroupID group_id)
{
	if (!IsValidGroup(group_id)) return -1;

	uint32_t occupancy = 0;
	uint32_t vehicle_count = 0;

	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->group_id != group_id) continue;
		if (!v->IsPrimaryVehicle()) continue;

		occupancy += v->trip_occupancy;
		vehicle_count++;
	}

	if (vehicle_count == 0) return -1;

	return occupancy / vehicle_count;
}

/* static */ bool ScriptGroup::SetPrimaryColour(GroupID group_id, ScriptCompany::Colours colour)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, IsValidGroup(group_id));

	return ScriptObject::Command<CMD_SET_GROUP_LIVERY>::Do(group_id, true, (::Colours)colour);
}

/* static */ bool ScriptGroup::SetSecondaryColour(GroupID group_id, ScriptCompany::Colours colour)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, IsValidGroup(group_id));

	return ScriptObject::Command<CMD_SET_GROUP_LIVERY>::Do(group_id, false, (::Colours)colour);
}

/* static */ ScriptCompany::Colours ScriptGroup::GetPrimaryColour(GroupID group_id)
{
	EnforcePrecondition(ScriptCompany::Colours::COLOUR_INVALID, IsValidGroup(group_id));

	const Group *g = ::Group::GetIfValid(group_id);
	if (!HasBit(g->livery.in_use, 0)) return ScriptCompany::Colours::COLOUR_INVALID;
	return (ScriptCompany::Colours)g->livery.colour1;
}

/* static */ ScriptCompany::Colours ScriptGroup::GetSecondaryColour(GroupID group_id)
{
	EnforcePrecondition(ScriptCompany::Colours::COLOUR_INVALID, IsValidGroup(group_id));

	const Group *g = ::Group::GetIfValid(group_id);
	if (!HasBit(g->livery.in_use, 1)) return ScriptCompany::Colours::COLOUR_INVALID;
	return (ScriptCompany::Colours)g->livery.colour2;
}
