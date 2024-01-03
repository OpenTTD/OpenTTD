/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file command.cpp Handling of commands. */

#include "stdafx.h"
#include "landscape.h"
#include "error.h"
#include "gui.h"
#include "command_func.h"
#include "network/network_type.h"
#include "network/network.h"
#include "genworld.h"
#include "strings_func.h"
#include "texteff.hpp"
#include "town.h"
#include "timer/timer_game_calendar.h"
#include "company_func.h"
#include "company_base.h"
#include "signal_func.h"
#include "core/backup_type.hpp"
#include "object_base.h"
#include "autoreplace_cmd.h"
#include "company_cmd.h"
#include "depot_cmd.h"
#include "economy_cmd.h"
#include "engine_cmd.h"
#include "goal_cmd.h"
#include "group_cmd.h"
#include "industry_cmd.h"
#include "league_cmd.h"
#include "landscape_cmd.h"
#include "misc_cmd.h"
#include "news_cmd.h"
#include "object_cmd.h"
#include "order_cmd.h"
#include "rail_cmd.h"
#include "road_cmd.h"
#include "roadveh_cmd.h"
#include "settings_cmd.h"
#include "signs_cmd.h"
#include "station_cmd.h"
#include "story_cmd.h"
#include "subsidy_cmd.h"
#include "terraform_cmd.h"
#include "timetable_cmd.h"
#include "town_cmd.h"
#include "train_cmd.h"
#include "tree_cmd.h"
#include "tunnelbridge_cmd.h"
#include "vehicle_cmd.h"
#include "viewport_cmd.h"
#include "water_cmd.h"
#include "waypoint_cmd.h"
#include "misc/endian_buffer.hpp"
#include "string_func.h"

#include "table/strings.h"

#include "safeguards.h"


int RecursiveCommandCounter::_counter = 0;


/**
 * Define a command with the flags which belongs to it.
 *
 * This struct connects a command handler function with the flags created with
 * the #CMD_AUTO, #CMD_OFFLINE and #CMD_SERVER values.
 */
struct CommandInfo {
	const char *name;   ///< A human readable name for the procedure
	CommandFlags flags; ///< The (command) flags to that apply to this command
	CommandType type;   ///< The type of command.
};
/* Helpers to generate the master command table from the command traits. */
template <typename T>
inline constexpr CommandInfo CommandFromTrait() noexcept { return { T::name, T::flags, T::type }; };

template<typename T, T... i>
inline constexpr auto MakeCommandsFromTraits(std::integer_sequence<T, i...>) noexcept {
	return std::array<CommandInfo, sizeof...(i)>{{ CommandFromTrait<CommandTraits<static_cast<Commands>(i)>>()... }};
}

/**
 * The master command table
 *
 * This table contains all possible CommandProc functions with
 * the flags which belongs to it. The indices are the same
 * as the value from the CMD_* enums.
 */
static constexpr auto _command_proc_table = MakeCommandsFromTraits(std::make_integer_sequence<std::underlying_type_t<Commands>, CMD_END>{});


/*!
 * This function range-checks a cmd.
 *
 * @param cmd The integer value of a command
 * @return true if the command is valid (and got a CommandProc function)
 */
bool IsValidCommand(Commands cmd)
{
	return cmd < _command_proc_table.size();
}

/*!
 * This function mask the parameter with CMD_ID_MASK and returns
 * the flags which belongs to the given command.
 *
 * @param cmd The integer value of the command
 * @return The flags for this command
 */
CommandFlags GetCommandFlags(Commands cmd)
{
	assert(IsValidCommand(cmd));

	return _command_proc_table[cmd].flags;
}

/*!
 * This function mask the parameter with CMD_ID_MASK and returns
 * the name which belongs to the given command.
 *
 * @param cmd The integer value of the command
 * @return The name for this command
 */
const char *GetCommandName(Commands cmd)
{
	assert(IsValidCommand(cmd));

	return _command_proc_table[cmd].name;
}

/**
 * Returns whether the command is allowed while the game is paused.
 * @param cmd The command to check.
 * @return True if the command is allowed while paused, false otherwise.
 */
bool IsCommandAllowedWhilePaused(Commands cmd)
{
	/* Lookup table for the command types that are allowed for a given pause level setting. */
	static const int command_type_lookup[] = {
		CMDPL_ALL_ACTIONS,     ///< CMDT_LANDSCAPE_CONSTRUCTION
		CMDPL_NO_LANDSCAPING,  ///< CMDT_VEHICLE_CONSTRUCTION
		CMDPL_NO_LANDSCAPING,  ///< CMDT_MONEY_MANAGEMENT
		CMDPL_NO_CONSTRUCTION, ///< CMDT_VEHICLE_MANAGEMENT
		CMDPL_NO_CONSTRUCTION, ///< CMDT_ROUTE_MANAGEMENT
		CMDPL_NO_CONSTRUCTION, ///< CMDT_OTHER_MANAGEMENT
		CMDPL_NO_CONSTRUCTION, ///< CMDT_COMPANY_SETTING
		CMDPL_NO_ACTIONS,      ///< CMDT_SERVER_SETTING
		CMDPL_NO_ACTIONS,      ///< CMDT_CHEAT
	};
	static_assert(lengthof(command_type_lookup) == CMDT_END);

	assert(IsValidCommand(cmd));
	return _game_mode == GM_EDITOR || command_type_lookup[_command_proc_table[cmd].type] <= _settings_game.construction.command_pause_level;
}

/*!
 * This functions returns the money which can be used to execute a command.
 * This is either the money of the current company or INT64_MAX if there
 * is no such a company "at the moment" like the server itself.
 *
 * @return The available money of a company or INT64_MAX
 */
Money GetAvailableMoneyForCommand()
{
	CompanyID company = _current_company;
	if (!Company::IsValidID(company)) return INT64_MAX;
	return Company::Get(company)->money;
}


/**
 * Prepare for calling a command proc.
 * @param top_level Top level of command execution, i.e. command from a command.
 * @param test Test run of command?
 */
void CommandHelperBase::InternalDoBefore(bool top_level, bool test)
{
	if (top_level) _cleared_object_areas.clear();
	if (test) SetTownRatingTestMode(true);
}

/**
 * Process result after calling a command proc.
 * @param[in,out] res Command result, may be modified.
 * @param flags Command flags.
 * @param top_level Top level of command execution, i.e. command from a command.
 * @param test Test run of command?
 */
void CommandHelperBase::InternalDoAfter(CommandCost &res, DoCommandFlag flags, bool top_level, bool test)
{
	if (test) {
		SetTownRatingTestMode(false);

		if (res.Succeeded() && top_level && !(flags & DC_QUERY_COST) && !(flags & DC_BANKRUPT)) {
			CheckCompanyHasMoney(res); // CheckCompanyHasMoney() modifies 'res' to an error if it fails.
		}
	} else {
		/* If top-level, subtract the money. */
		if (res.Succeeded() && top_level && !(flags & DC_BANKRUPT)) {
			SubtractMoneyFromCompany(res);
		}
	}
}

/**
 * Decide what to do with the command depending on current game state.
 * @param cmd Command to execute.
 * @param flags Command flags.
 * @param tile Tile of command execution.
 * @param err_message Message prefix to show on error.
 * @param network_command Does this command come from the network?
 * @return error state + do only cost estimation? + send to network only?
 */
std::tuple<bool, bool, bool> CommandHelperBase::InternalPostBefore(Commands cmd, CommandFlags flags, TileIndex tile, StringID err_message, bool network_command)
{
	/* Cost estimation is generally only done when the
	 * local user presses shift while doing something.
	 * However, in case of incoming network commands,
	 * map generation or the pause button we do want
	 * to execute. */
	bool estimate_only = _shift_pressed && IsLocalCompany() && !_generating_world && !network_command && !(flags & CMD_NO_EST);

	/* We're only sending the command, so don't do
	 * fancy things for 'success'. */
	bool only_sending = _networking && !network_command;

	if (_pause_mode != PM_UNPAUSED && !IsCommandAllowedWhilePaused(cmd) && !estimate_only) {
		ShowErrorMessage(err_message, STR_ERROR_NOT_ALLOWED_WHILE_PAUSED, WL_INFO, TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE);
		return { true, estimate_only, only_sending };
	} else {
		return { false, estimate_only, only_sending };
	}
}

/**
 * Process result of executing a command, possibly displaying any error to the player.
 * @param res Command result.
 * @param tile Tile of command execution.
 * @param estimate_only Is this just cost estimation?
 * @param only_sending Was the command only sent to network?
 * @param err_message Message prefix to show on error.
 * @param my_cmd Is the command from this client?
 */
void CommandHelperBase::InternalPostResult(const CommandCost &res, TileIndex tile, bool estimate_only, bool only_sending, StringID err_message, bool my_cmd)
{
	int x = TileX(tile) * TILE_SIZE;
	int y = TileY(tile) * TILE_SIZE;

	if (res.Failed()) {
		/* Only show the error when it's for us. */
		if (estimate_only || (IsLocalCompany() && err_message != 0 && my_cmd)) {
			ShowErrorMessage(err_message, x, y, res);
		}
	} else if (estimate_only) {
		ShowEstimatedCostOrIncome(res.GetCost(), x, y);
	} else if (!only_sending && tile != 0 && IsLocalCompany() && _game_mode != GM_EDITOR) {
		/* Only show the cost animation when we did actually
		 * execute the command, i.e. we're not sending it to
		 * the server, when it has cost the local company
		 * something. Furthermore in the editor there is no
		 * concept of cost, so don't show it there either. */
		ShowCostOrIncomeAnimation(x, y, GetSlopePixelZ(x, y), res.GetCost());
	}
}

/** Helper to make a desync log for a command. */
void CommandHelperBase::LogCommandExecution(Commands cmd, StringID err_message, const CommandDataBuffer &args, bool failed)
{
	Debug(desync, 1, "{}: {:08x}; {:02x}; {:02x}; {:08x}; {:08x}; {} ({})", failed ? "cmdf" : "cmd", (uint32_t)TimerGameCalendar::date.base(), TimerGameCalendar::date_fract, (int)_current_company, cmd, err_message, FormatArrayAsHex(args), GetCommandName(cmd));
}

/**
 * Prepare for the test run of a command proc call.
 * @param cmd_flags Command flags.
 * @param[in,out] cur_company Backup of current company at start of command execution.
 * @return True if test run can go ahead, false on error.
 */
bool CommandHelperBase::InternalExecutePrepTest(CommandFlags cmd_flags, TileIndex, Backup<CompanyID> &cur_company)
{
	/* Always execute server and spectator commands as spectator */
	bool exec_as_spectator = (cmd_flags & (CMD_SPECTATOR | CMD_SERVER)) != 0;

	/* If the company isn't valid it may only do server command or start a new company!
	 * The server will ditch any server commands a client sends to it, so effectively
	 * this guards the server from executing functions for an invalid company. */
	if (_game_mode == GM_NORMAL && !exec_as_spectator && !Company::IsValidID(_current_company) && !(_current_company == OWNER_DEITY && (cmd_flags & CMD_DEITY) != 0)) {
		return false;
	}

	if (exec_as_spectator) cur_company.Change(COMPANY_SPECTATOR);

	/* Enter test mode. */
	_cleared_object_areas.clear();
	SetTownRatingTestMode(true);
	BasePersistentStorageArray::SwitchMode(PSM_ENTER_TESTMODE);
	return true;
}

/**
 * Validate result of test run and prepare for real execution.
 * @param cmd_flags Command flags.
 * @param[in,out] res Command result of test run, may be modified.
 * @param estimate_only Is this just cost estimation?
 * @param network_command Does this command come from the network?
 * @param[in,out] cur_company Backup of current company at start of command execution.
 * @return True if test run can go ahead, false on error.
 */
std::tuple<bool, bool, bool> CommandHelperBase::InternalExecuteValidateTestAndPrepExec(CommandCost &res, CommandFlags cmd_flags, bool estimate_only, bool network_command, [[maybe_unused]] Backup<CompanyID> &cur_company)
{
	BasePersistentStorageArray::SwitchMode(PSM_LEAVE_TESTMODE);
	SetTownRatingTestMode(false);

	/* Make sure we're not messing things up here. */
	assert((cmd_flags & (CMD_SPECTATOR | CMD_SERVER)) != 0 ? _current_company == COMPANY_SPECTATOR : cur_company.Verify());

	/* If the command fails, we're doing an estimate
	 * or the player does not have enough money
	 * (unless it's a command where the test and
	 * execution phase might return different costs)
	 * we bail out here. */
	bool test_and_exec_can_differ = (cmd_flags & CMD_NO_TEST) != 0;
	if (res.Failed() || estimate_only || (!test_and_exec_can_differ && !CheckCompanyHasMoney(res))) {
		return { true, !_networking || _generating_world || network_command, false };
	}

	bool send_net = _networking && !_generating_world && !network_command;

	if (!send_net) {
		/* Prepare for command execution. */
		_cleared_object_areas.clear();
		BasePersistentStorageArray::SwitchMode(PSM_ENTER_COMMAND);
	}

	return { false, _debug_desync_level >= 1, send_net };
}

/**
 * Process the result of a command test run and execution run.
 * @param cmd Command that was executed.
 * @param cmd_flags Command flags.
 * @param res_test Command result of test run.
 * @param tes_exec Command result of real run.
 * @param extra_cash Additional cash required for successful command execution.
 * @param tile Tile of command execution.
 * @param[in,out] cur_company Backup of current company at start of command execution.
 * @return Final command result.
 */
CommandCost CommandHelperBase::InternalExecuteProcessResult(Commands cmd, CommandFlags cmd_flags, [[maybe_unused]] const CommandCost &res_test, const CommandCost &res_exec, Money extra_cash, TileIndex tile, Backup<CompanyID> &cur_company)
{
	BasePersistentStorageArray::SwitchMode(PSM_LEAVE_COMMAND);

	if (cmd == CMD_COMPANY_CTRL) {
		cur_company.Trash();
		/* We are a new company                  -> Switch to new local company.
		 * We were closed down                   -> Switch to spectator
		 * Some other company opened/closed down -> The outside function will switch back */
		_current_company = _local_company;
	} else {
		/* Make sure nothing bad happened, like changing the current company. */
		assert((cmd_flags & (CMD_SPECTATOR | CMD_SERVER)) != 0 ? _current_company == COMPANY_SPECTATOR : cur_company.Verify());
		cur_company.Restore();
	}

	/* If the test and execution can differ we have to check the
	 * return of the command. Otherwise we can check whether the
	 * test and execution have yielded the same result,
	 * i.e. cost and error state are the same. */
	bool test_and_exec_can_differ = (cmd_flags & CMD_NO_TEST) != 0;
	if (!test_and_exec_can_differ) {
		assert(res_test.GetCost() == res_exec.GetCost() && res_test.Failed() == res_exec.Failed()); // sanity check
	} else if (res_exec.Failed()) {
		return res_exec;
	}

	/* If we're needing more money and we haven't done
	 * anything yet, ask for the money! */
	if (extra_cash != 0 && res_exec.GetCost() == 0) {
		/* It could happen we removed rail, thus gained money, and deleted something else.
		 * So make sure the signal buffer is empty even in this case */
		UpdateSignalsInBuffer();
		SetDParam(0, extra_cash);
		return CommandCost(STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY);
	}

	/* update last build coordinate of company. */
	if (tile != 0) {
		Company *c = Company::GetIfValid(_current_company);
		if (c != nullptr) c->last_build_coordinate = tile;
	}

	SubtractMoneyFromCompany(res_exec);

	/* Record if there was a command issues during pause; ignore pause/other setting related changes. */
	if (_pause_mode != PM_UNPAUSED && _command_proc_table[cmd].type != CMDT_SERVER_SETTING) _pause_mode |= PM_COMMAND_DURING_PAUSE;

	/* update signals if needed */
	UpdateSignalsInBuffer();

	return res_exec;
}


/**
 * Adds the cost of the given command return value to this cost.
 * Also takes a possible error message when it is set.
 * @param ret The command to add the cost of.
 */
void CommandCost::AddCost(const CommandCost &ret)
{
	this->AddCost(ret.cost);
	if (this->success && !ret.success) {
		this->message = ret.message;
		this->success = false;
	}
}

/**
 * Values to put on the #TextRefStack for the error message.
 * There is only one static instance of the array, just like there is only one
 * instance of normal DParams.
 */
uint32_t CommandCost::textref_stack[16];

/**
 * Activate usage of the NewGRF #TextRefStack for the error message.
 * @param grffile NewGRF that provides the #TextRefStack
 * @param num_registers number of entries to copy from the temporary NewGRF registers
 */
void CommandCost::UseTextRefStack(const GRFFile *grffile, uint num_registers)
{
	extern TemporaryStorageArray<int32_t, 0x110> _temp_store;

	assert(num_registers < lengthof(textref_stack));
	this->textref_stack_grffile = grffile;
	this->textref_stack_size = num_registers;
	for (uint i = 0; i < num_registers; i++) {
		textref_stack[i] = _temp_store.GetValue(0x100 + i);
	}
}
