/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_object.cpp Implementation of ScriptObject. */

#include "../../stdafx.h"
#include "../../script/squirrel.hpp"
#include "../../company_func.h"
#include "../../company_base.h"
#include "../../network/network.h"
#include "../../genworld.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../command_func.h"

#include "../script_storage.hpp"
#include "../script_instance.hpp"
#include "../script_fatalerror.hpp"
#include "script_error.hpp"
#include "../../debug.h"

#include "../../safeguards.h"

/**
 * Get the storage associated with the current ScriptInstance.
 * @return The storage.
 */
static ScriptStorage *GetStorage()
{
	return ScriptObject::GetActiveInstance()->GetStorage();
}


/* static */ ScriptInstance *ScriptObject::ActiveInstance::active = nullptr;

ScriptObject::ActiveInstance::ActiveInstance(ScriptInstance *instance) : alc_scope(instance->engine)
{
	this->last_active = ScriptObject::ActiveInstance::active;
	ScriptObject::ActiveInstance::active = instance;
}

ScriptObject::ActiveInstance::~ActiveInstance()
{
	ScriptObject::ActiveInstance::active = this->last_active;
}

/* static */ ScriptInstance *ScriptObject::GetActiveInstance()
{
	assert(ScriptObject::ActiveInstance::active != nullptr);
	return ScriptObject::ActiveInstance::active;
}


/* static */ void ScriptObject::SetDoCommandDelay(uint ticks)
{
	assert(ticks > 0);
	GetStorage()->delay = ticks;
}

/* static */ uint ScriptObject::GetDoCommandDelay()
{
	return GetStorage()->delay;
}

/* static */ void ScriptObject::SetDoCommandMode(ScriptModeProc *proc, ScriptObject *instance)
{
	GetStorage()->mode = proc;
	GetStorage()->mode_instance = instance;
}

/* static */ ScriptModeProc *ScriptObject::GetDoCommandMode()
{
	return GetStorage()->mode;
}

/* static */ ScriptObject *ScriptObject::GetDoCommandModeInstance()
{
	return GetStorage()->mode_instance;
}

/* static */ void ScriptObject::SetDoCommandAsyncMode(ScriptAsyncModeProc *proc, ScriptObject *instance)
{
	GetStorage()->async_mode = proc;
	GetStorage()->async_mode_instance = instance;
}

/* static */ ScriptAsyncModeProc *ScriptObject::GetDoCommandAsyncMode()
{
	return GetStorage()->async_mode;
}

/* static */ ScriptObject *ScriptObject::GetDoCommandAsyncModeInstance()
{
	return GetStorage()->async_mode_instance;
}

/* static */ void ScriptObject::SetLastCommand(const CommandDataBuffer &data, Commands cmd)
{
	ScriptStorage *s = GetStorage();
	Debug(script, 6, "SetLastCommand company={:02d} cmd={} data={}", s->root_company, cmd, FormatArrayAsHex(data));
	s->last_data = data;
	s->last_cmd = cmd;
}

/* static */ bool ScriptObject::CheckLastCommand(const CommandDataBuffer &data, Commands cmd)
{
	ScriptStorage *s = GetStorage();
	Debug(script, 6, "CheckLastCommand company={:02d} cmd={} data={}", s->root_company, cmd, FormatArrayAsHex(data));
	if (s->last_cmd != cmd) return false;
	if (s->last_data != data) return false;
	return true;
}

/* static */ void ScriptObject::SetDoCommandCosts(Money value)
{
	GetStorage()->costs = CommandCost(INVALID_EXPENSES, value); // Expense type is never read.
}

/* static */ void ScriptObject::IncreaseDoCommandCosts(Money value)
{
	GetStorage()->costs.AddCost(value);
}

/* static */ Money ScriptObject::GetDoCommandCosts()
{
	return GetStorage()->costs.GetCost();
}

/* static */ void ScriptObject::SetLastError(ScriptErrorType last_error)
{
	GetStorage()->last_error = last_error;
}

/* static */ ScriptErrorType ScriptObject::GetLastError()
{
	return GetStorage()->last_error;
}

/* static */ void ScriptObject::SetLastCost(Money last_cost)
{
	GetStorage()->last_cost = last_cost;
}

/* static */ Money ScriptObject::GetLastCost()
{
	return GetStorage()->last_cost;
}

/* static */ void ScriptObject::SetRoadType(RoadType road_type)
{
	GetStorage()->road_type = road_type;
}

/* static */ RoadType ScriptObject::GetRoadType()
{
	return GetStorage()->road_type;
}

/* static */ void ScriptObject::SetRailType(RailType rail_type)
{
	GetStorage()->rail_type = rail_type;
}

/* static */ RailType ScriptObject::GetRailType()
{
	return GetStorage()->rail_type;
}

/* static */ void ScriptObject::SetLastCommandRes(bool res)
{
	GetStorage()->last_command_res = res;
}

/* static */ bool ScriptObject::GetLastCommandRes()
{
	return GetStorage()->last_command_res;
}

/* static */ void ScriptObject::SetLastCommandResData(CommandDataBuffer data)
{
	GetStorage()->last_cmd_ret = std::move(data);
}

/* static */ const CommandDataBuffer &ScriptObject::GetLastCommandResData()
{
	return GetStorage()->last_cmd_ret;
}

/* static */ void ScriptObject::SetAllowDoCommand(bool allow)
{
	GetStorage()->allow_do_command = allow;
}

/* static */ bool ScriptObject::GetAllowDoCommand()
{
	return GetStorage()->allow_do_command;
}

/* static */ void ScriptObject::SetCompany(CompanyID company)
{
	if (GetStorage()->root_company == INVALID_OWNER) GetStorage()->root_company = company;
	GetStorage()->company = company;

	_current_company = company;
}

/* static */ CompanyID ScriptObject::GetCompany()
{
	return GetStorage()->company;
}

/* static */ CompanyID ScriptObject::GetRootCompany()
{
	return GetStorage()->root_company;
}

/* static */ bool ScriptObject::CanSuspend()
{
	Squirrel *squirrel = ScriptObject::GetActiveInstance()->engine;
	return GetStorage()->allow_do_command && squirrel->CanSuspend();
}

/* static */ void *&ScriptObject::GetEventPointer()
{
	return GetStorage()->event_data;
}

/* static */ ScriptLogTypes::LogData &ScriptObject::GetLogData()
{
	return GetStorage()->log_data;
}

/* static */ std::string ScriptObject::GetString(StringID string)
{
	return ::StrMakeValid(::GetString(string));
}

/* static */ void ScriptObject::SetCallbackVariable(int index, int value)
{
	if (static_cast<size_t>(index) >= GetStorage()->callback_value.size()) GetStorage()->callback_value.resize(index + 1);
	GetStorage()->callback_value[index] = value;
}

/* static */ int ScriptObject::GetCallbackVariable(int index)
{
	return GetStorage()->callback_value[index];
}

/* static */ CommandCallbackData *ScriptObject::GetDoCommandCallback()
{
	return ScriptObject::GetActiveInstance()->GetDoCommandCallback();
}

std::tuple<bool, bool, bool, bool> ScriptObject::DoCommandPrep()
{
	if (!ScriptObject::CanSuspend()) {
		throw Script_FatalError("You are not allowed to execute any DoCommand (even indirect) in your constructor, Save(), Load(), and any valuator.");
	}

	/* Are we only interested in the estimate costs? */
	bool estimate_only = GetDoCommandMode() != nullptr && !GetDoCommandMode()();

	/* Should the command be executed asynchronously? */
	bool asynchronous = GetDoCommandAsyncMode() != nullptr && GetDoCommandAsyncMode()();

	bool networking = _networking && !_generating_world;

	if (!ScriptCompanyMode::IsDeity() && !ScriptCompanyMode::IsValid()) {
		ScriptObject::SetLastError(ScriptError::ERR_PRECONDITION_INVALID_COMPANY);
		return { true, estimate_only, asynchronous, networking };
	}

	return { false, estimate_only, asynchronous, networking };
}

bool ScriptObject::DoCommandProcessResult(const CommandCost &res, Script_SuspendCallbackProc *callback, bool estimate_only, bool asynchronous)
{
	/* Set the default callback to return a true/false result of the DoCommand */
	if (callback == nullptr) callback = &ScriptInstance::DoCommandReturn;

	/* We failed; set the error and bail out */
	if (res.Failed()) {
		SetLastError(ScriptError::StringToError(res.GetErrorMessage()));
		return false;
	}

	/* No error, then clear it. */
	SetLastError(ScriptError::ERR_NONE);

	/* Estimates, update the cost for the estimate and be done */
	if (estimate_only) {
		IncreaseDoCommandCosts(res.GetCost());
		return true;
	}

	/* Costs of this operation. */
	SetLastCost(res.GetCost());
	SetLastCommandRes(true);

	if (_generating_world || asynchronous) {
		IncreaseDoCommandCosts(res.GetCost());
		if (!_generating_world) {
			/* Charge a nominal fee for asynchronously executed commands */
			Squirrel *engine = ScriptObject::GetActiveInstance()->engine;
			Squirrel::DecreaseOps(engine->GetVM(), 100);
		}
		if (callback != nullptr) {
			/* Insert return value into to stack and throw a control code that
			 * the return value in the stack should be used. */
			callback(GetActiveInstance());
			throw SQInteger(1);
		}
		return true;
	} else if (_networking) {
		/* Suspend the script till the command is really executed. */
		throw Script_Suspend(-(int)GetDoCommandDelay(), callback);
	} else {
		IncreaseDoCommandCosts(res.GetCost());

		/* Suspend the script player for 1+ ticks, so it simulates multiplayer. This
		 *  both avoids confusion when a developer launched the script in a
		 *  multiplayer game, but also gives time for the GUI and human player
		 *  to interact with the game. */
		throw Script_Suspend(GetDoCommandDelay(), callback);
	}

	NOT_REACHED();
}


/* static */ Randomizer ScriptObject::random_states[OWNER_END];

Randomizer &ScriptObject::GetRandomizer(Owner owner)
{
	return ScriptObject::random_states[owner];
}

void ScriptObject::InitializeRandomizers()
{
	Randomizer random = _random;
	for (Owner owner = OWNER_BEGIN; owner < OWNER_END; owner++) {
		ScriptObject::GetRandomizer(owner).SetSeed(random.Next());
	}
}
