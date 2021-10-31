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
#include "misc/endian_buffer.hpp"
#include "tile_map.h"

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

CommandCost DoCommandPInternal(Commands cmd, StringID err_message, CommandCallback *callback, bool my_cmd, bool estimate_only, bool network_command, TileIndex tile, uint32 p1, uint32 p2, const std::string &text);

void NetworkSendCommand(Commands cmd, StringID err_message, CommandCallback *callback, CompanyID company, TileIndex tile, uint32 p1, uint32 p2, const std::string &text);
void NetworkSendCommand(Commands cmd, StringID err_message, CommandCallback *callback, CompanyID company, TileIndex location, const CommandDataBuffer &cmd_data);

extern Money _additional_cash_required;

bool IsValidCommand(Commands cmd);
CommandFlags GetCommandFlags(Commands cmd);
const char *GetCommandName(Commands cmd);
Money GetAvailableMoneyForCommand();
bool IsCommandAllowedWhilePaused(Commands cmd);

template <Commands Tcmd>
constexpr CommandFlags GetCommandFlags()
{
	return CommandTraits<Tcmd>::flags;
}

/**
 * Extracts the DC flags needed for DoCommand from the flags returned by GetCommandFlags
 * @param cmd_flags Flags from GetCommandFlags
 * @return flags for DoCommand
 */
static constexpr inline DoCommandFlag CommandFlagsToDCFlags(CommandFlags cmd_flags)
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


template<Commands TCmd, typename T> struct CommandHelper;

class CommandHelperBase {
protected:
	static void InternalDoBefore(bool top_level, bool test);
	static void InternalDoAfter(CommandCost &res, DoCommandFlag flags, bool top_level, bool test);
	static std::tuple<bool, bool, bool> InternalPostBefore(Commands cmd, CommandFlags flags, TileIndex tile, StringID err_message, bool network_command);
	static void InternalPostResult(const CommandCost &res, TileIndex tile, bool estimate_only, bool only_sending, StringID err_message, bool my_cmd);
};

/**
 * Templated wrapper that exposes the command parameter arguments
 * for the various Command::Do/Post calls.
 * @tparam Tcmd The command-id to execute.
 * @tparam Targs The command parameter types.
 */
template <Commands Tcmd, typename... Targs>
struct CommandHelper<Tcmd, CommandCost(*)(DoCommandFlag, Targs...)> : protected CommandHelperBase {
public:
	/**
	 * This function executes a given command with the parameters from the #CommandProc parameter list.
	 * Depending on the flags parameter it executes or tests a command.
	 *
	 * @note This function is to be called from the StateGameLoop or from within the execution of a Command.
	 * This function must not be called from the context of a "player" (real person, AI, game script).
	 * Use ::Post for commands directly triggered by "players".
	 *
	 * @param flags Flags for the command and how to execute the command
	 * @param args Parameters for the command
	 * @see CommandProc
	 * @return the cost
	 */
	static CommandCost Do(DoCommandFlag flags, Targs... args)
	{
		if constexpr (std::is_same_v<TileIndex, std::tuple_element_t<0, std::tuple<Targs...>>>) {
			/* Do not even think about executing out-of-bounds tile-commands. */
			TileIndex tile = std::get<0>(std::make_tuple(args...));
			if (tile != 0 && (tile >= MapSize() || (!IsValidTile(tile) && (flags & DC_ALL_TILES) == 0))) return CMD_ERROR;
		}

		RecursiveCommandCounter counter{};

		/* Only execute the test call if it's toplevel, or we're not execing. */
		if (counter.IsTopLevel() || !(flags & DC_EXEC)) {
			InternalDoBefore(counter.IsTopLevel(), true);
			CommandCost res = CommandTraits<Tcmd>::proc(flags & ~DC_EXEC, args...);
			InternalDoAfter(res, flags, counter.IsTopLevel(), true); // Can modify res.

			if (res.Failed() || !(flags & DC_EXEC)) return res;
		}

		/* Execute the command here. All cost-relevant functions set the expenses type
		 * themselves to the cost object at some point. */
		InternalDoBefore(counter.IsTopLevel(), false);
		CommandCost res = CommandTraits<Tcmd>::proc(flags, args...);
		InternalDoAfter(res, flags, counter.IsTopLevel(), false);

		return res;
	}

	/**
	 * Shortcut for the long Post when not using a callback.
	 * @param err_message Message prefix to show on error
	 * @param args Parameters for the command
	 */
	static inline bool Post(StringID err_message, Targs... args) { return Post(err_message, nullptr, std::forward<Targs>(args)...); }
	/**
	 * Shortcut for the long Post when not using an error message.
	 * @param callback A callback function to call after the command is finished
	 * @param args Parameters for the command
	 */
	static inline bool Post(CommandCallback *callback, Targs... args) { return Post((StringID)0, callback, std::forward<Targs>(args)...); }
	/**
	 * Shortcut for the long Post when not using a callback or an error message.
	 * @param args Parameters for the command
	 */
	static inline bool Post(Targs... args) { return Post((StringID)0, nullptr, std::forward<Targs>(args)...); }

	/**
	 * Top-level network safe command execution for the current company.
	 * Must not be called recursively. The callback is called when the
	 * command succeeded or failed.
	 *
	 * @param err_message Message prefix to show on error
	 * @param callback A callback function to call after the command is finished
	 * @param args Parameters for the command
	 * @return \c true if the command succeeded, else \c false.
	 */
	static bool Post(StringID err_message, CommandCallback *callback, Targs... args)
	{
		return InternalPost(err_message, callback, true, false, std::forward_as_tuple(args...));
	}

	/**
	 * Execute a command coming from the network.
	 * @param err_message Message prefix to show on error
	 * @param callback A callback function to call after the command is finished
	 * @param my_cmd indicator if the command is from a company or server (to display error messages for a user)
	 * @param location Tile location for user feedback.
	 * @param args Parameters for the command
	 * @return \c true if the command succeeded, else \c false.
	 */
	static bool PostFromNet(StringID err_message, CommandCallback *callback, bool my_cmd, TileIndex location, std::tuple<Targs...> args)
	{
		return InternalPost(err_message, callback, my_cmd, true, location, std::move(args));
	}

protected:
	static bool InternalPost(StringID err_message, CommandCallback *callback, bool my_cmd, bool network_command, std::tuple<Targs...> args)
	{
		/* Where to show the message? */
		TileIndex tile{};
		if constexpr (std::is_same_v<TileIndex, std::tuple_element_t<0, decltype(args)>>) {
			tile = std::get<0>(args);
		}

		return InternalPost(err_message, callback, my_cmd, network_command, tile, std::move(args));
	}

	static bool InternalPost(StringID err_message, CommandCallback *callback, bool my_cmd, bool network_command, TileIndex tile, std::tuple<Targs...> args)
	{
		auto [err, estimate_only, only_sending] = InternalPostBefore(Tcmd, GetCommandFlags<Tcmd>(), tile, err_message, network_command);
		if (err) return false;

		/* Only set p2 when the command does not come from the network. */
		if (!network_command && GetCommandFlags<Tcmd>() & CMD_CLIENT_ID && std::get<2>(args) == 0) std::get<2>(args) = CLIENT_ID_SERVER;

		CommandCost res = std::apply(DoCommandPInternal, std::tuple_cat(std::make_tuple(Tcmd, err_message, callback, my_cmd, estimate_only, network_command), args));
		InternalPostResult(res, tile, estimate_only, only_sending, err_message, my_cmd);

		if (!estimate_only && !only_sending && callback != nullptr) {
			callback(Tcmd, res, tile, EndianBufferWriter<CommandDataBuffer>::FromValue(args));
		}

		return res.Succeeded();
	}
};

template <Commands Tcmd>
using Command = CommandHelper<Tcmd, typename CommandTraits<Tcmd>::ProcType>;

#endif /* COMMAND_FUNC_H */
