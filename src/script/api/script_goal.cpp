/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_goal.cpp Implementation of ScriptGoal. */

#include "../../stdafx.h"
#include "script_goal.hpp"
#include "script_error.hpp"
#include "script_industry.hpp"
#include "script_map.hpp"
#include "script_town.hpp"
#include "../script_instance.hpp"
#include "../../command_type.h"
#include "../../settings_type.h"
#include "../../openttd.h"
#include "../../network/network.h"
#include "../../goal_base.h"
#include "../../string_func.h"

/* static */ bool ScriptGoal::IsValidGoal(GoalID goal_id)
{
	return ::Goal::IsValidID(goal_id);
}

/* static */ ScriptGoal::GoalID ScriptGoal::New(ScriptCompany::CompanyID company, const char *goal, GoalType type, uint32 destination)
{
	EnforcePrecondition(GOAL_INVALID, !StrEmpty(goal));
	EnforcePrecondition(GOAL_INVALID, company == ScriptCompany::COMPANY_INVALID || ScriptCompany::ResolveCompanyID(company) != ScriptCompany::COMPANY_INVALID);
	EnforcePrecondition(GOAL_INVALID, (type == GT_NONE && destination == 0) || (type == GT_TILE && ScriptMap::IsValidTile(destination)) || (type == GT_INDUSTRY && ScriptIndustry::IsValidIndustry(destination)) || (type == GT_TOWN && ScriptTown::IsValidTown(destination)) || (type == GT_COMPANY && ScriptCompany::ResolveCompanyID((ScriptCompany::CompanyID)destination) != ScriptCompany::COMPANY_INVALID));

	uint8 c = company;
	if (company == ScriptCompany::COMPANY_INVALID) c = INVALID_COMPANY;

	if (!ScriptObject::DoCommand(0, type | (c << 8), destination, CMD_CREATE_GOAL, goal, &ScriptInstance::DoCommandReturnGoalID)) return GOAL_INVALID;

	/* In case of test-mode, we return GoalID 0 */
	return (ScriptGoal::GoalID)0;
}

/* static */ bool ScriptGoal::Remove(GoalID goal_id)
{
	EnforcePrecondition(false, IsValidGoal(goal_id));

	return ScriptObject::DoCommand(0, goal_id, 0, CMD_REMOVE_GOAL);
}
