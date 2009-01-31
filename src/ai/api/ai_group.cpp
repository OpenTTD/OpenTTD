/* $Id$ */

/** @file ai_group.cpp Implementation of AIGroup. */

#include "ai_group.hpp"
#include "ai_engine.hpp"
#include "../ai_instance.hpp"
#include "../../company_func.h"
#include "../../group.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../core/alloc_func.hpp"
#include "../../command_func.h"
#include "../../autoreplace_func.h"
#include "table/strings.h"

/* static */ bool AIGroup::IsValidGroup(GroupID group_id)
{
	return ::IsValidGroupID(group_id) && ::GetGroup(group_id)->owner == _current_company;
}

/* static */ AIGroup::GroupID AIGroup::CreateGroup(AIVehicle::VehicleType vehicle_type)
{
	if (!AIObject::DoCommand(0, (::VehicleType)vehicle_type, 0, CMD_CREATE_GROUP, NULL, &AIInstance::DoCommandReturnGroupID)) return GROUP_INVALID;

	/* In case of test-mode, we return GroupID 0 */
	return (AIGroup::GroupID)0;
}

/* static */ bool AIGroup::DeleteGroup(GroupID group_id)
{
	EnforcePrecondition(false, IsValidGroup(group_id));

	return AIObject::DoCommand(0, group_id, 0, CMD_DELETE_GROUP);
}

/* static */ AIVehicle::VehicleType AIGroup::GetVehicleType(GroupID group_id)
{
	if (!IsValidGroup(group_id)) return AIVehicle::VT_INVALID;

	return (AIVehicle::VehicleType)((::VehicleType)::GetGroup(group_id)->vehicle_type);
}

/* static */ bool AIGroup::SetName(GroupID group_id, const char *name)
{
	EnforcePrecondition(false, IsValidGroup(group_id));
	EnforcePrecondition(false, !::StrEmpty(name));
	EnforcePreconditionCustomError(false, ::strlen(name) < MAX_LENGTH_GROUP_NAME_BYTES, AIError::ERR_PRECONDITION_STRING_TOO_LONG);

	return AIObject::DoCommand(0, group_id, 0, CMD_RENAME_GROUP, name);
}

/* static */ char *AIGroup::GetName(GroupID group_id)
{
	if (!IsValidGroup(group_id)) return NULL;

	static const int len = 64;
	char *group_name = MallocT<char>(len);

	::SetDParam(0, group_id);
	::GetString(group_name, STR_GROUP_NAME, &group_name[len - 1]);
	return group_name;
}

/* static */ bool AIGroup::EnableAutoReplaceProtection(GroupID group_id, bool enable)
{
	EnforcePrecondition(false, IsValidGroup(group_id));

	return AIObject::DoCommand(0, group_id, enable ? 1 : 0, CMD_SET_GROUP_REPLACE_PROTECTION);
}

/* static */ bool AIGroup::GetAutoReplaceProtection(GroupID group_id)
{
	if (!IsValidGroup(group_id)) return false;

	return ::GetGroup(group_id)->replace_protection;
}

/* static */ int32 AIGroup::GetNumEngines(GroupID group_id, EngineID engine_id)
{
	if (!IsValidGroup(group_id) && group_id != GROUP_DEFAULT && group_id != GROUP_ALL) return -1;

	return GetGroupNumEngines(_current_company, group_id, engine_id);
}

/* static */ bool AIGroup::MoveVehicle(GroupID group_id, VehicleID vehicle_id)
{
	EnforcePrecondition(false, IsValidGroup(group_id) || group_id == GROUP_DEFAULT);
	EnforcePrecondition(false, AIVehicle::IsValidVehicle(vehicle_id));

	return AIObject::DoCommand(0, group_id, vehicle_id, CMD_ADD_VEHICLE_GROUP);
}

/* static */ bool AIGroup::EnableWagonRemoval(bool enable_removal)
{
	if (HasWagonRemoval() == enable_removal) return true;

	return AIObject::DoCommand(0, 5, enable_removal ? 1 : 0, CMD_SET_AUTOREPLACE);
}

/* static */ bool AIGroup::HasWagonRemoval()
{
	return ::GetCompany(_current_company)->renew_keep_length;
}

/* static */ bool AIGroup::SetAutoReplace(GroupID group_id, EngineID engine_id_old, EngineID engine_id_new)
{
	EnforcePrecondition(false, IsValidGroup(group_id) || group_id == GROUP_ALL);
	EnforcePrecondition(false, AIEngine::IsValidEngine(engine_id_new));

	return AIObject::DoCommand(0, 3 | (group_id << 16), (engine_id_new << 16) | engine_id_old, CMD_SET_AUTOREPLACE);
}

/* static */ EngineID AIGroup::GetEngineReplacement(GroupID group_id, EngineID engine_id)
{
	if (!IsValidGroup(group_id) && group_id != GROUP_ALL) return ::INVALID_ENGINE;

	return ::EngineReplacementForCompany(GetCompany(_current_company), engine_id, group_id);
}

/* static */ bool AIGroup::StopAutoReplace(GroupID group_id, EngineID engine_id)
{
	EnforcePrecondition(false, IsValidGroup(group_id) || group_id == GROUP_ALL);

	return AIObject::DoCommand(0, 3 | (group_id << 16), (::INVALID_ENGINE << 16) | engine_id, CMD_SET_AUTOREPLACE);
}
