/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_storage.hpp Defines ScriptStorage and includes all files required for it. */

#ifndef SCRIPT_STORAGE_HPP
#define SCRIPT_STORAGE_HPP

#include <queue>

#include "../command_type.h"
#include "../company_type.h"
#include "../rail_type.h"
#include "../road_type.h"

#include "script_types.hpp"
#include "script_log_types.hpp"
#include "script_object.hpp"

class ScriptEvent;

/* This is a "struct", so we can forward declare it, and use as incomplete type. */
struct ScriptEventQueue : std::queue<ScriptObjectRef<ScriptEvent>> {
};

/**
 * The callback function for Mode-classes.
 */
typedef bool (ScriptModeProc)();

/**
 * The callback function for Async Mode-classes.
 */
typedef bool (ScriptAsyncModeProc)();

/**
 * The storage for each script. It keeps track of important information.
 */
class ScriptStorage {
friend class ScriptObject;
private:
	ScriptModeProc *mode = nullptr; ///< The current build mode we are int.
	class ScriptObject *mode_instance = nullptr; ///< The instance belonging to the current build mode.
	ScriptAsyncModeProc *async_mode = nullptr; ///< The current command async mode we are in.
	class ScriptObject *async_mode_instance = nullptr; ///< The instance belonging to the current command async mode.
	CompanyID root_company = INVALID_OWNER; ///< The root company, the company that the script really belongs to.
	CompanyID company = INVALID_OWNER; ///< The current company.

	uint delay = 1; ///< The ticks of delay each DoCommand has.
	bool allow_do_command = true; ///< Is the usage of DoCommands restricted?

	CommandCost costs; ///< The costs the script is tracking.
	Money last_cost = 0; ///< The last cost of the command.
	ScriptErrorType last_error{}; ///< The last error of the command.
	bool last_command_res = true; ///< The last result of the command.

	CommandDataBuffer last_data; ///< The last data passed to a command.
	Commands last_cmd = CMD_END; ///< The last cmd passed to a command.
	CommandDataBuffer last_cmd_ret; ///< The extra data returned by the last command.

	std::vector<int> callback_value; ///< The values which need to survive a callback.

	RoadType road_type = INVALID_ROADTYPE; ///< The current roadtype we build.
	RailType rail_type = INVALID_RAILTYPE; ///< The current railtype we build.

	ScriptEventQueue event_queue; ///< Event queue for this script.
	ScriptLogTypes::LogData log_data; ///< Log data storage.

public:
	ScriptStorage();
	~ScriptStorage();
};

#endif /* SCRIPT_STORAGE_HPP */
