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
#include "date_func.h"
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

#include <array>

#include "table/strings.h"

#include "safeguards.h"


/**
 * Define a command with the flags which belongs to it.
 *
 * This struct connects a command handler function with the flags created with
 * the #CMD_AUTO, #CMD_OFFLINE and #CMD_SERVER values.
 */
struct CommandInfo {
	CommandProc *proc;  ///< The procedure to actually executing
	const char *name;   ///< A human readable name for the procedure
	CommandFlags flags; ///< The (command) flags to that apply to this command
	CommandType type;   ///< The type of command.
};
/* Helpers to generate the master command table from the command traits. */

template <typename T>
inline constexpr CommandInfo CommandFromTrait() noexcept { return { T::proc, T::name, T::flags, T::type }; };

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
 * This function range-checks a cmd, and checks if the cmd is not nullptr
 *
 * @param cmd The integer value of a command
 * @return true if the command is valid (and got a CommandProc function)
 */
bool IsValidCommand(Commands cmd)
{
	return cmd < _command_proc_table.size() && _command_proc_table[cmd].proc != nullptr;
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


static int _docommand_recursive = 0;

/**
 * Shorthand for calling the long DoCommand with a container.
 *
 * @param container Container with (almost) all information
 * @param flags Flags for the command and how to execute the command
 * @see CommandProc
 * @return the cost
 */
CommandCost DoCommand(const CommandContainer *container, DoCommandFlag flags)
{
	return DoCommand(flags, container->cmd, container->tile, container->p1, container->p2, container->text);
}

/*!
 * This function executes a given command with the parameters from the #CommandProc parameter list.
 * Depending on the flags parameter it execute or test a command.
 *
 * @param flags Flags for the command and how to execute the command
 * @param cmd The command-id to execute (a value of the CMD_* enums)
 * @param tile The tile to apply the command on (for the #CommandProc)
 * @param p1 Additional data for the command (for the #CommandProc)
 * @param p2 Additional data for the command (for the #CommandProc)
 * @param text The text to pass
 * @see CommandProc
 * @return the cost
 */
CommandCost DoCommand(DoCommandFlag flags, Commands cmd, TileIndex tile, uint32 p1, uint32 p2, const std::string &text)
{
	CommandCost res;

	/* Do not even think about executing out-of-bounds tile-commands */
	if (tile != 0 && (tile >= MapSize() || (!IsValidTile(tile) && (flags & DC_ALL_TILES) == 0))) return CMD_ERROR;

	/* Chop of any CMD_MSG or other flags; we don't need those here */
	CommandProc *proc = _command_proc_table[cmd].proc;

	_docommand_recursive++;

	/* only execute the test call if it's toplevel, or we're not execing. */
	if (_docommand_recursive == 1 || !(flags & DC_EXEC) ) {
		if (_docommand_recursive == 1) _cleared_object_areas.clear();
		SetTownRatingTestMode(true);
		res = proc(flags & ~DC_EXEC, tile, p1, p2, text);
		SetTownRatingTestMode(false);
		if (res.Failed()) {
			goto error;
		}

		if (_docommand_recursive == 1 &&
				!(flags & DC_QUERY_COST) &&
				!(flags & DC_BANKRUPT) &&
				!CheckCompanyHasMoney(res)) { // CheckCompanyHasMoney() modifies 'res' to an error if it fails.
			goto error;
		}

		if (!(flags & DC_EXEC)) {
			_docommand_recursive--;
			return res;
		}
	}

	/* Execute the command here. All cost-relevant functions set the expenses type
	 * themselves to the cost object at some point */
	if (_docommand_recursive == 1) _cleared_object_areas.clear();
	res = proc(flags, tile, p1, p2, text);
	if (res.Failed()) {
error:
		_docommand_recursive--;
		return res;
	}

	/* if toplevel, subtract the money. */
	if (--_docommand_recursive == 0 && !(flags & DC_BANKRUPT)) {
		SubtractMoneyFromCompany(res);
	}

	return res;
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


/*!
 * Toplevel network safe docommand function for the current company. Must not be called recursively.
 * The callback is called when the command succeeded or failed. The parameters
 * \a tile, \a p1, and \a p2 are from the #CommandProc function. The parameter \a cmd is the command to execute.
 * The parameter \a my_cmd is used to indicate if the command is from a company or the server.
 *
 * @param cmd The command to execute (a CMD_* value)
 * @param callback A callback function to call after the command is finished
 * @param my_cmd indicator if the command is from a company or server (to display error messages for a user)
 * @param network_command execute the command without sending it on the network
 * @param tile The tile to perform a command on (see #CommandProc)
 * @param p1 Additional data for the command (see #CommandProc)
 * @param p2 Additional data for the command (see #CommandProc)
 * @param text The text to pass
 * @return \c true if the command succeeded, else \c false.
 */
static bool DoCommandP(Commands cmd, StringID err_message, CommandCallback *callback, bool my_cmd, bool network_command, TileIndex tile, uint32 p1, uint32 p2, const std::string &text)
{
	/* Cost estimation is generally only done when the
	 * local user presses shift while doing something.
	 * However, in case of incoming network commands,
	 * map generation or the pause button we do want
	 * to execute. */
	bool estimate_only = _shift_pressed && IsLocalCompany() &&
			!_generating_world &&
			!network_command &&
			!(GetCommandFlags(cmd) & CMD_NO_EST);

	/* We're only sending the command, so don't do
	 * fancy things for 'success'. */
	bool only_sending = _networking && !network_command;

	/* Where to show the message? */
	int x = TileX(tile) * TILE_SIZE;
	int y = TileY(tile) * TILE_SIZE;

	if (_pause_mode != PM_UNPAUSED && !IsCommandAllowedWhilePaused(cmd) && !estimate_only) {
		ShowErrorMessage(err_message, STR_ERROR_NOT_ALLOWED_WHILE_PAUSED, WL_INFO, x, y);
		return false;
	}

	/* Only set p2 when the command does not come from the network. */
	if (!network_command && GetCommandFlags(cmd) & CMD_CLIENT_ID && p2 == 0) p2 = CLIENT_ID_SERVER;

	CommandCost res = DoCommandPInternal(cmd, err_message, callback, my_cmd, estimate_only, network_command, tile, p1, p2, text);
	if (res.Failed()) {
		/* Only show the error when it's for us. */
		if (estimate_only || (IsLocalCompany() && err_message != 0 && my_cmd)) {
			ShowErrorMessage(err_message, res.GetErrorMessage(), WL_INFO, x, y, res.GetTextRefStackGRF(), res.GetTextRefStackSize(), res.GetTextRefStack());
		}
	} else if (estimate_only) {
		ShowEstimatedCostOrIncome(res.GetCost(), x, y);
	} else if (!only_sending && res.GetCost() != 0 && tile != 0 && IsLocalCompany() && _game_mode != GM_EDITOR) {
		/* Only show the cost animation when we did actually
		 * execute the command, i.e. we're not sending it to
		 * the server, when it has cost the local company
		 * something. Furthermore in the editor there is no
		 * concept of cost, so don't show it there either. */
		ShowCostOrIncomeAnimation(x, y, GetSlopePixelZ(x, y), res.GetCost());
	}

	if (!estimate_only && !only_sending && callback != nullptr) {
		callback(res, cmd, tile, p1, p2, text);
	}

	return res.Succeeded();
}

/**
 * Shortcut for the long DoCommandP when having a container with the data.
 * @param container the container with information.
 * @param my_cmd indicator if the command is from a company or server (to display error messages for a user)
 * @param network_command execute the command without sending it on the network
 * @return true if the command succeeded, else false
 */
bool DoCommandP(const CommandContainer *container, bool my_cmd, bool network_command)
{
	return DoCommandP(container->cmd, container->err_msg, container->callback, my_cmd, network_command, container->tile, container->p1, container->p2, container->text);
}

/**
 * Shortcut for the long DoCommandP when not using a callback or error message.
 * @param cmd The command to execute (a CMD_* value)
 * @param tile The tile to perform a command on (see #CommandProc)
 * @param p1 Additional data for the command (see #CommandProc)
 * @param p2 Additional data for the command (see #CommandProc)
 * @param text The text to pass
 * @return \c true if the command succeeded, else \c false.
 */
bool DoCommandP(Commands cmd, TileIndex tile, uint32 p1, uint32 p2, const std::string &text)
{
	return DoCommandP(cmd, STR_NULL, nullptr, true, false, tile, p1, p2, text);
}

/**
 * Shortcut for the long DoCommandP when not using an error message.
 * @param cmd The command to execute (a CMD_* value)
 * @param callback A callback function to call after the command is finished
 * @param tile The tile to perform a command on (see #CommandProc)
 * @param p1 Additional data for the command (see #CommandProc)
 * @param p2 Additional data for the command (see #CommandProc)
 * @param text The text to pass
 * @return \c true if the command succeeded, else \c false.
 */
bool DoCommandP(Commands cmd, CommandCallback *callback, TileIndex tile, uint32 p1, uint32 p2, const std::string &text)
{
	return DoCommandP(cmd, STR_NULL, callback, true, false, tile, p1, p2, text);
}

/**
 * Shortcut for the long DoCommandP when not using a callback.
 * @param cmd The command to execute (a CMD_* value)
 * @param err_message Message prefix to show on error
 * @param tile The tile to perform a command on (see #CommandProc)
 * @param p1 Additional data for the command (see #CommandProc)
 * @param p2 Additional data for the command (see #CommandProc)
 * @param text The text to pass
 * @return \c true if the command succeeded, else \c false.
 */
bool DoCommandP(Commands cmd, StringID err_message, TileIndex tile, uint32 p1, uint32 p2, const std::string &text)
{
	return DoCommandP(cmd, err_message, nullptr, true, false, tile, p1, p2, text);
}

/*!
 * Toplevel network safe docommand function for the current company. Must not be called recursively.
 * The callback is called when the command succeeded or failed. The parameters
 * \a tile, \a p1, and \a p2 are from the #CommandProc function. The parameter \a cmd is the command to execute.
 *
 * @param cmd The command to execute (a CMD_* value)
 * @param err_message Message prefix to show on error
 * @param callback A callback function to call after the command is finished
 * @param tile The tile to perform a command on (see #CommandProc)
 * @param p1 Additional data for the command (see #CommandProc)
 * @param p2 Additional data for the command (see #CommandProc)
 * @param text The text to pass
 * @return \c true if the command succeeded, else \c false.
 */
bool DoCommandP(Commands cmd, StringID err_message, CommandCallback *callback, TileIndex tile, uint32 p1, uint32 p2, const std::string &text)
{
	return DoCommandP(cmd, err_message, callback, true, false, tile, p1, p2, text);
}

/**
 * Helper to deduplicate the code for returning.
 * @param cmd   the command cost to return.
 */
#define return_dcpi(cmd) { _docommand_recursive = 0; return cmd; }

/*!
 * Helper function for the toplevel network safe docommand function for the current company.
 *
 * @param cmd The command to execute (a CMD_* value)
 * @param err_message Message prefix to show on error
 * @param callback A callback function to call after the command is finished
 * @param my_cmd indicator if the command is from a company or server (to display error messages for a user)
 * @param estimate_only whether to give only the estimate or also execute the command
 * @param tile The tile to perform a command on (see #CommandProc)
 * @param p1 Additional data for the command (see #CommandProc)
 * @param p2 Additional data for the command (see #CommandProc)
 * @param text The text to pass
 * @return the command cost of this function.
 */
CommandCost DoCommandPInternal(Commands cmd, StringID err_message, CommandCallback *callback, bool my_cmd, bool estimate_only, bool network_command, TileIndex tile, uint32 p1, uint32 p2, const std::string &text)
{
	/* Prevent recursion; it gives a mess over the network */
	assert(_docommand_recursive == 0);
	_docommand_recursive = 1;

	/* Reset the state. */
	_additional_cash_required = 0;

	/* Get pointer to command handler */
	assert(cmd < _command_proc_table.size());
	CommandProc *proc = _command_proc_table[cmd].proc;
	/* Shouldn't happen, but you never know when someone adds
	 * NULLs to the _command_proc_table. */
	assert(proc != nullptr);

	/* Command flags are used internally */
	CommandFlags cmd_flags = GetCommandFlags(cmd);
	/* Flags get send to the DoCommand */
	DoCommandFlag flags = CommandFlagsToDCFlags(cmd_flags);

	/* Make sure p2 is properly set to a ClientID. */
	assert(!(cmd_flags & CMD_CLIENT_ID) || p2 != 0);

	/* Do not even think about executing out-of-bounds tile-commands */
	if (tile != 0 && (tile >= MapSize() || (!IsValidTile(tile) && (cmd_flags & CMD_ALL_TILES) == 0))) return_dcpi(CMD_ERROR);

	/* Always execute server and spectator commands as spectator */
	bool exec_as_spectator = (cmd_flags & (CMD_SPECTATOR | CMD_SERVER)) != 0;

	/* If the company isn't valid it may only do server command or start a new company!
	 * The server will ditch any server commands a client sends to it, so effectively
	 * this guards the server from executing functions for an invalid company. */
	if (_game_mode == GM_NORMAL && !exec_as_spectator && !Company::IsValidID(_current_company) && !(_current_company == OWNER_DEITY && (cmd_flags & CMD_DEITY) != 0)) {
		return_dcpi(CMD_ERROR);
	}

	Backup<CompanyID> cur_company(_current_company, FILE_LINE);
	if (exec_as_spectator) cur_company.Change(COMPANY_SPECTATOR);

	bool test_and_exec_can_differ = (cmd_flags & CMD_NO_TEST) != 0;

	/* Test the command. */
	_cleared_object_areas.clear();
	SetTownRatingTestMode(true);
	BasePersistentStorageArray::SwitchMode(PSM_ENTER_TESTMODE);
	CommandCost res = proc(flags, tile, p1, p2, text);
	BasePersistentStorageArray::SwitchMode(PSM_LEAVE_TESTMODE);
	SetTownRatingTestMode(false);

	/* Make sure we're not messing things up here. */
	assert(exec_as_spectator ? _current_company == COMPANY_SPECTATOR : cur_company.Verify());

	/* If the command fails, we're doing an estimate
	 * or the player does not have enough money
	 * (unless it's a command where the test and
	 * execution phase might return different costs)
	 * we bail out here. */
	if (res.Failed() || estimate_only ||
			(!test_and_exec_can_differ && !CheckCompanyHasMoney(res))) {
		if (!_networking || _generating_world || network_command) {
			/* Log the failed command as well. Just to be able to be find
			 * causes of desyncs due to bad command test implementations. */
			Debug(desync, 1, "cmdf: {:08x}; {:02x}; {:02x}; {:06x}; {:08x}; {:08x}; {:08x}; {:08x}; \"{}\" ({})", _date, _date_fract, (int)_current_company, tile, p1, p2, cmd, err_message, text, GetCommandName(cmd));
		}
		cur_company.Restore();
		return_dcpi(res);
	}

	/*
	 * If we are in network, and the command is not from the network
	 * send it to the command-queue and abort execution
	 */
	if (_networking && !_generating_world && !network_command) {
		NetworkSendCommand(cmd, err_message, callback, _current_company, tile, p1, p2, text);
		cur_company.Restore();

		/* Don't return anything special here; no error, no costs.
		 * This way it's not handled by DoCommand and only the
		 * actual execution of the command causes messages. Also
		 * reset the storages as we've not executed the command. */
		return_dcpi(CommandCost());
	}
	Debug(desync, 1, "cmd: {:08x}; {:02x}; {:02x}; {:06x}; {:08x}; {:08x}; {:08x}; {:08x}; \"{}\" ({})", _date, _date_fract, (int)_current_company, tile, p1, p2, cmd, err_message, text, GetCommandName(cmd));

	/* Actually try and execute the command. If no cost-type is given
	 * use the construction one */
	_cleared_object_areas.clear();
	BasePersistentStorageArray::SwitchMode(PSM_ENTER_COMMAND);
	CommandCost res2 = proc(flags | DC_EXEC, tile, p1, p2, text);
	BasePersistentStorageArray::SwitchMode(PSM_LEAVE_COMMAND);

	if (cmd == CMD_COMPANY_CTRL) {
		cur_company.Trash();
		/* We are a new company                  -> Switch to new local company.
		 * We were closed down                   -> Switch to spectator
		 * Some other company opened/closed down -> The outside function will switch back */
		_current_company = _local_company;
	} else {
		/* Make sure nothing bad happened, like changing the current company. */
		assert(exec_as_spectator ? _current_company == COMPANY_SPECTATOR : cur_company.Verify());
		cur_company.Restore();
	}

	/* If the test and execution can differ we have to check the
	 * return of the command. Otherwise we can check whether the
	 * test and execution have yielded the same result,
	 * i.e. cost and error state are the same. */
	if (!test_and_exec_can_differ) {
		assert(res.GetCost() == res2.GetCost() && res.Failed() == res2.Failed()); // sanity check
	} else if (res2.Failed()) {
		return_dcpi(res2);
	}

	/* If we're needing more money and we haven't done
	 * anything yet, ask for the money! */
	if (_additional_cash_required != 0 && res2.GetCost() == 0) {
		/* It could happen we removed rail, thus gained money, and deleted something else.
		 * So make sure the signal buffer is empty even in this case */
		UpdateSignalsInBuffer();
		SetDParam(0, _additional_cash_required);
		return_dcpi(CommandCost(STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY));
	}

	/* update last build coordinate of company. */
	if (tile != 0) {
		Company *c = Company::GetIfValid(_current_company);
		if (c != nullptr) c->last_build_coordinate = tile;
	}

	SubtractMoneyFromCompany(res2);

	/* update signals if needed */
	UpdateSignalsInBuffer();

	return_dcpi(res2);
}
#undef return_dcpi


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
uint32 CommandCost::textref_stack[16];

/**
 * Activate usage of the NewGRF #TextRefStack for the error message.
 * @param grffile NewGRF that provides the #TextRefStack
 * @param num_registers number of entries to copy from the temporary NewGRF registers
 */
void CommandCost::UseTextRefStack(const GRFFile *grffile, uint num_registers)
{
	extern TemporaryStorageArray<int32, 0x110> _temp_store;

	assert(num_registers < lengthof(textref_stack));
	this->textref_stack_grffile = grffile;
	this->textref_stack_size = num_registers;
	for (uint i = 0; i < num_registers; i++) {
		textref_stack[i] = _temp_store.GetValue(0x100 + i);
	}
}
