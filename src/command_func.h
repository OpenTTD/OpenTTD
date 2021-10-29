/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file command_func.h Functions related to commands. */

#ifndef COMMAND_FUNC_H
#define COMMAND_FUNC_H

#include "command_type.h"
#include "company_type.h"
#include <vector>

/**
 * Define a default return value for a failed command.
 *
 * This variable contains a CommandCost object with is declared as "failed".
 * Other functions just need to return this error if there is an error,
 * which doesn't need to specific by a StringID.
 */
static const CommandCost CMD_ERROR = CommandCost(INVALID_STRING_ID);

/**
 * Returns from a function with a specific StringID as error.
 *
 * This macro is used to return from a function. The parameter contains the
 * StringID which will be returned.
 *
 * @param errcode The StringID to return
 */
#define return_cmd_error(errcode) return CommandCost(errcode);

/** Storage buffer for serialized command data. */
typedef std::vector<byte> CommandDataBuffer;

CommandCost DoCommand(DoCommandFlag flags, Commands cmd, TileIndex tile, uint32 p1, uint32 p2, const std::string &text = {});

bool DoCommandP(Commands cmd, StringID err_message, CommandCallback *callback, TileIndex tile, uint32 p1, uint32 p2, const std::string &text = {});
bool DoCommandP(Commands cmd, StringID err_message, TileIndex tile, uint32 p1, uint32 p2, const std::string &text = {});
bool DoCommandP(Commands cmd, CommandCallback *callback, TileIndex tile, uint32 p1, uint32 p2, const std::string &text = {});
bool DoCommandP(Commands cmd, TileIndex tile, uint32 p1, uint32 p2, const std::string &text = {});

bool InjectNetworkCommand(Commands cmd, StringID err_message, CommandCallback *callback, bool my_cmd, TileIndex tile, uint32 p1, uint32 p2, const std::string &text);

CommandCost DoCommandPInternal(Commands cmd, StringID err_message, CommandCallback *callback, bool my_cmd, bool estimate_only, bool network_command, TileIndex tile, uint32 p1, uint32 p2, const std::string &text);

void NetworkSendCommand(Commands cmd, StringID err_message, CommandCallback *callback, CompanyID company, TileIndex tile, uint32 p1, uint32 p2, const std::string &text);
void NetworkSendCommand(Commands cmd, StringID err_message, CommandCallback *callback, CompanyID company, TileIndex location, const CommandDataBuffer &cmd_data);

extern Money _additional_cash_required;

bool IsValidCommand(Commands cmd);
CommandFlags GetCommandFlags(Commands cmd);
const char *GetCommandName(Commands cmd);
Money GetAvailableMoneyForCommand();
bool IsCommandAllowedWhilePaused(Commands cmd);

/**
 * Extracts the DC flags needed for DoCommand from the flags returned by GetCommandFlags
 * @param cmd_flags Flags from GetCommandFlags
 * @return flags for DoCommand
 */
static inline DoCommandFlag CommandFlagsToDCFlags(CommandFlags cmd_flags)
{
	DoCommandFlag flags = DC_NONE;
	if (cmd_flags & CMD_NO_WATER) flags |= DC_NO_WATER;
	if (cmd_flags & CMD_AUTO) flags |= DC_AUTO;
	if (cmd_flags & CMD_ALL_TILES) flags |= DC_ALL_TILES;
	return flags;
}

/** Helper class to keep track of command nesting level. */
struct RecursiveCommandCounter {
	RecursiveCommandCounter() noexcept { _counter++; }
	~RecursiveCommandCounter() noexcept { _counter--; }

	/** Are we in the top-level command execution? */
	bool IsTopLevel() const { return _counter == 1; }
private:
	static int _counter;
};

#endif /* COMMAND_FUNC_H */
