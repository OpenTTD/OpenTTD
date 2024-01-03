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
#include "network/network_type.h"
#include "company_type.h"
#include "company_func.h"
#include "core/backup_type.hpp"
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

void NetworkSendCommand(Commands cmd, StringID err_message, CommandCallback *callback, CompanyID company, const CommandDataBuffer &cmd_data);

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

#if defined(__GNUC__) && !defined(__clang__)
/*
 * We cast specialized function pointers to a generic one, but don't use the
 * converted value to call the function, which is safe, except that GCC
 * helpfully thinks it is not.
 *
 * "Any pointer to function can be converted to a pointer to a different function type.
 * Calling the function through a pointer to a different function type is undefined,
 * but converting such pointer back to pointer to the original function type yields
 * the pointer to the original function." */
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wcast-function-type"
#	define SILENCE_GCC_FUNCTION_POINTER_CAST
#endif

template<Commands TCmd, typename T, bool THasTile> struct CommandHelper;

class CommandHelperBase {
protected:
	static void InternalDoBefore(bool top_level, bool test);
	static void InternalDoAfter(CommandCost &res, DoCommandFlag flags, bool top_level, bool test);
	static std::tuple<bool, bool, bool> InternalPostBefore(Commands cmd, CommandFlags flags, TileIndex tile, StringID err_message, bool network_command);
	static void InternalPostResult(const CommandCost &res, TileIndex tile, bool estimate_only, bool only_sending, StringID err_message, bool my_cmd);
	static bool InternalExecutePrepTest(CommandFlags cmd_flags, TileIndex tile, Backup<CompanyID> &cur_company);
	static std::tuple<bool, bool, bool> InternalExecuteValidateTestAndPrepExec(CommandCost &res, CommandFlags cmd_flags, bool estimate_only, bool network_command, Backup<CompanyID> &cur_company);
	static CommandCost InternalExecuteProcessResult(Commands cmd, CommandFlags cmd_flags, const CommandCost &res_test, const CommandCost &res_exec, Money extra_cash, TileIndex tile, Backup<CompanyID> &cur_company);
	static void LogCommandExecution(Commands cmd, StringID err_message, const CommandDataBuffer &args, bool failed);
};

/**
 * Templated wrapper that exposes the command parameter arguments
 * for the various Command::Do/Post calls.
 * @tparam Tcmd The command-id to execute.
 * @tparam Tret Return type of the command.
 * @tparam Targs The command parameter types.
 */
template <Commands Tcmd, typename Tret, typename... Targs>
struct CommandHelper<Tcmd, Tret(*)(DoCommandFlag, Targs...), true> : protected CommandHelperBase {
private:
	/** Extract the \c CommandCost from a command proc result. */
	static inline CommandCost &ExtractCommandCost(Tret &ret)
	{
		if constexpr (std::is_same_v<Tret, CommandCost>) {
			return ret;
		} else {
			return std::get<0>(ret);
		}
	}

	/** Make a command proc result from a \c CommandCost. */
	static inline Tret MakeResult(const CommandCost &cost)
	{
		Tret ret{};
		ExtractCommandCost(ret) = cost;
		return ret;
	}

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
	static Tret Do(DoCommandFlag flags, Targs... args)
	{
		if constexpr (std::is_same_v<TileIndex, std::tuple_element_t<0, std::tuple<Targs...>>>) {
			/* Do not even think about executing out-of-bounds tile-commands. */
			TileIndex tile = std::get<0>(std::make_tuple(args...));
			if (tile != 0 && (tile >= Map::Size() || (!IsValidTile(tile) && (flags & DC_ALL_TILES) == 0))) return MakeResult(CMD_ERROR);
		}

		RecursiveCommandCounter counter{};

		/* Only execute the test call if it's toplevel, or we're not execing. */
		if (counter.IsTopLevel() || !(flags & DC_EXEC)) {
			InternalDoBefore(counter.IsTopLevel(), true);
			Tret res = CommandTraits<Tcmd>::proc(flags & ~DC_EXEC, args...);
			InternalDoAfter(ExtractCommandCost(res), flags, counter.IsTopLevel(), true); // Can modify res.

			if (ExtractCommandCost(res).Failed() || !(flags & DC_EXEC)) return res;
		}

		/* Execute the command here. All cost-relevant functions set the expenses type
		 * themselves to the cost object at some point. */
		InternalDoBefore(counter.IsTopLevel(), false);
		Tret res = CommandTraits<Tcmd>::proc(flags, args...);
		InternalDoAfter(ExtractCommandCost(res), flags, counter.IsTopLevel(), false);

		return res;
	}

	/**
	 * Shortcut for the long Post when not using a callback.
	 * @param err_message Message prefix to show on error
	 * @param args Parameters for the command
	 */
	static inline bool Post(StringID err_message, Targs... args) { return Post<CommandCallback>(err_message, nullptr, std::forward<Targs>(args)...); }
	/**
	 * Shortcut for the long Post when not using an error message.
	 * @param callback A callback function to call after the command is finished
	 * @param args Parameters for the command
	 */
	template <typename Tcallback>
	static inline bool Post(Tcallback *callback, Targs... args) { return Post((StringID)0, callback, std::forward<Targs>(args)...); }
	/**
	 * Shortcut for the long Post when not using a callback or an error message.
	 * @param args Parameters for the command
	 */
	static inline bool Post(Targs... args) { return Post<CommandCallback>((StringID)0, nullptr, std::forward<Targs>(args)...); }

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
	template <typename Tcallback>
	static bool Post(StringID err_message, Tcallback *callback, Targs... args)
	{
		return InternalPost(err_message, callback, true, false, std::forward_as_tuple(args...));
	}

	/**
	 * Execute a command coming from the network.
	 * @param err_message Message prefix to show on error
	 * @param callback A callback function to call after the command is finished
	 * @param my_cmd indicator if the command is from a company or server (to display error messages for a user)
	 * @param args Parameters for the command
	 * @return \c true if the command succeeded, else \c false.
	 */
	template <typename Tcallback>
	static bool PostFromNet(StringID err_message, Tcallback *callback, bool my_cmd, std::tuple<Targs...> args)
	{
		return InternalPost(err_message, callback, my_cmd, true, std::move(args));
	}

	/**
	 * Prepare a command to be send over the network
	 * @param cmd The command to execute (a CMD_* value)
	 * @param err_message Message prefix to show on error
	 * @param company The company that wants to send the command
	 * @param args Parameters for the command
	 */
	static void SendNet(StringID err_message, CompanyID company, Targs... args)
	{
		auto args_tuple = std::forward_as_tuple(args...);

		::NetworkSendCommand(Tcmd, err_message, nullptr, company, EndianBufferWriter<CommandDataBuffer>::FromValue(args_tuple));
	}

	/**
	 * Top-level network safe command execution without safety checks.
	 * @param err_message Message prefix to show on error
	 * @param callback A callback function to call after the command is finished
	 * @param my_cmd indicator if the command is from a company or server (to display error messages for a user)
	 * @param estimate_only whether to give only the estimate or also execute the command
	 * @param location Tile location for user feedback.
	 * @param args Parameters for the command
	 * @return the command cost of this function.
	 */
	template <typename Tcallback>
	static Tret Unsafe(StringID err_message, Tcallback *callback, bool my_cmd, bool estimate_only, TileIndex location, std::tuple<Targs...> args)
	{
		return Execute(err_message, reinterpret_cast<CommandCallback *>(callback), my_cmd, estimate_only, false, location, std::move(args));
	}

protected:
	/** Helper to process a single ClientID argument. */
	template <class T>
	static inline void SetClientIdHelper([[maybe_unused]] T &data)
	{
		if constexpr (std::is_same_v<ClientID, T>) {
			if (data == INVALID_CLIENT_ID) data = CLIENT_ID_SERVER;
		}
	}

	/** Set all invalid ClientID's to the proper value. */
	template<class Ttuple, size_t... Tindices>
	static inline void SetClientIds(Ttuple &values, std::index_sequence<Tindices...>)
	{
		((SetClientIdHelper(std::get<Tindices>(values))), ...);
	}

	/** Remove the first element of a tuple. */
	template <template <typename...> typename Tt, typename T1, typename... Ts>
	static inline Tt<Ts...> RemoveFirstTupleElement(const Tt<T1, Ts...> &tuple)
	{
		return std::apply([](auto &&, const auto&... args) { return std::tie(args...); }, tuple);
	}

	template <typename Tcallback>
	static bool InternalPost(StringID err_message, Tcallback *callback, bool my_cmd, bool network_command, std::tuple<Targs...> args)
	{
		/* Where to show the message? */
		TileIndex tile{};
		if constexpr (std::is_same_v<TileIndex, std::tuple_element_t<0, decltype(args)>>) {
			tile = std::get<0>(args);
		}

		return InternalPost(err_message, callback, my_cmd, network_command, tile, std::move(args));
	}

	template <typename Tcallback>
	static bool InternalPost(StringID err_message, Tcallback *callback, bool my_cmd, bool network_command, TileIndex tile, std::tuple<Targs...> args)
	{
		/* Do not even think about executing out-of-bounds tile-commands. */
		if (tile != 0 && (tile >= Map::Size() || (!IsValidTile(tile) && (GetCommandFlags<Tcmd>() & CMD_ALL_TILES) == 0))) return false;

		auto [err, estimate_only, only_sending] = InternalPostBefore(Tcmd, GetCommandFlags<Tcmd>(), tile, err_message, network_command);
		if (err) return false;

		/* Only set client IDs when the command does not come from the network. */
		if (!network_command && GetCommandFlags<Tcmd>() & CMD_CLIENT_ID) SetClientIds(args, std::index_sequence_for<Targs...>{});

		Tret res = Execute(err_message, reinterpret_cast<CommandCallback *>(callback), my_cmd, estimate_only, network_command, tile, args);
		InternalPostResult(ExtractCommandCost(res), tile, estimate_only, only_sending, err_message, my_cmd);

		if (!estimate_only && !only_sending && callback != nullptr) {
			if constexpr (std::is_same_v<Tcallback, CommandCallback>) {
				/* Callback that doesn't need any command arguments. */
				callback(Tcmd, ExtractCommandCost(res), tile);
			} else if constexpr (std::is_same_v<Tcallback, CommandCallbackData>) {
				/* Generic callback that takes packed arguments as a buffer. */
				if constexpr (std::is_same_v<Tret, CommandCost>) {
					callback(Tcmd, ExtractCommandCost(res), EndianBufferWriter<CommandDataBuffer>::FromValue(args), {});
				} else {
					callback(Tcmd, ExtractCommandCost(res), EndianBufferWriter<CommandDataBuffer>::FromValue(args), EndianBufferWriter<CommandDataBuffer>::FromValue(RemoveFirstTupleElement(res)));
				}
			} else if constexpr (!std::is_same_v<Tret, CommandCost> && std::is_same_v<Tcallback *, typename CommandTraits<Tcmd>::RetCallbackProc>) {
				std::apply(callback, std::tuple_cat(std::make_tuple(Tcmd), res));
			} else {
				/* Callback with arguments. We assume that the tile is only interesting if it actually is in the command arguments. */
				if constexpr (std::is_same_v<Tret, CommandCost>) {
					std::apply(callback, std::tuple_cat(std::make_tuple(Tcmd, res), args));
				} else {
					std::apply(callback, std::tuple_cat(std::make_tuple(Tcmd), res, args));
				}
			}
		}

		return ExtractCommandCost(res).Succeeded();
	}

	/** Helper to process a single ClientID argument. */
	template <class T>
	static inline bool ClientIdIsSet([[maybe_unused]] T &data)
	{
		if constexpr (std::is_same_v<ClientID, T>) {
			return data != INVALID_CLIENT_ID;
		} else {
			return true;
		}
	}

	/** Check if all ClientID arguments are set to valid values. */
	template<class Ttuple, size_t... Tindices>
	static inline bool AllClientIdsSet(Ttuple &values, std::index_sequence<Tindices...>)
	{
		return (ClientIdIsSet(std::get<Tindices>(values)) && ...);
	}

	template<class Ttuple>
	static inline Money ExtractAdditionalMoney([[maybe_unused]] Ttuple &values)
	{
		if constexpr (std::is_same_v<std::tuple_element_t<1, Tret>, Money>) {
			return std::get<1>(values);
		} else {
			return {};
		}
	}

	static Tret Execute(StringID err_message, CommandCallback *callback, bool, bool estimate_only, bool network_command, TileIndex tile, std::tuple<Targs...> args)
	{
		/* Prevent recursion; it gives a mess over the network */
		RecursiveCommandCounter counter{};
		assert(counter.IsTopLevel());

		/* Command flags are used internally */
		constexpr CommandFlags cmd_flags = GetCommandFlags<Tcmd>();

		if constexpr ((cmd_flags & CMD_CLIENT_ID) != 0) {
			/* Make sure arguments are properly set to a ClientID also when processing external commands. */
			assert(AllClientIdsSet(args, std::index_sequence_for<Targs...>{}));
		}

		Backup<CompanyID> cur_company(_current_company, FILE_LINE);
		if (!InternalExecutePrepTest(cmd_flags, tile, cur_company)) {
			cur_company.Trash();
			return MakeResult(CMD_ERROR);
		}

		/* Test the command. */
		DoCommandFlag flags = CommandFlagsToDCFlags(cmd_flags);
		Tret res = std::apply(CommandTraits<Tcmd>::proc, std::tuple_cat(std::make_tuple(flags), args));

		auto [exit_test, desync_log, send_net] = InternalExecuteValidateTestAndPrepExec(ExtractCommandCost(res), cmd_flags, estimate_only, network_command, cur_company);
		if (exit_test) {
			if (desync_log) LogCommandExecution(Tcmd, err_message, EndianBufferWriter<CommandDataBuffer>::FromValue(args), true);
			cur_company.Restore();
			return res;
		}

		/* If we are in network, and the command is not from the network
		 * send it to the command-queue and abort execution. */
		if (send_net) {
			::NetworkSendCommand(Tcmd, err_message, callback, _current_company, EndianBufferWriter<CommandDataBuffer>::FromValue(args));
			cur_company.Restore();

			/* Don't return anything special here; no error, no costs.
			 * This way it's not handled by DoCommand and only the
			 * actual execution of the command causes messages. Also
			 * reset the storages as we've not executed the command. */
			return {};
		}

		if (desync_log) LogCommandExecution(Tcmd, err_message, EndianBufferWriter<CommandDataBuffer>::FromValue(args), false);

		/* Actually try and execute the command. */
		Tret res2 = std::apply(CommandTraits<Tcmd>::proc, std::tuple_cat(std::make_tuple(flags | DC_EXEC), args));

		/* Convention: If the second result element is of type Money,
		 * this is the additional cash required for the command. */
		Money additional_money{};
		if constexpr (!std::is_same_v<Tret, CommandCost>) { // No short-circuiting for 'if constexpr'.
			additional_money = ExtractAdditionalMoney(res2);
		}

		if constexpr (std::is_same_v<Tret, CommandCost>) {
			return InternalExecuteProcessResult(Tcmd, cmd_flags, res, res2, additional_money, tile, cur_company);
		} else {
			std::get<0>(res2) = InternalExecuteProcessResult(Tcmd, cmd_flags, ExtractCommandCost(res), ExtractCommandCost(res2), additional_money, tile, cur_company);
			return res2;
		}
	}
};

/**
 * Overload for #CommandHelper that exposes additional \c Post variants
 * for commands that don't take a TileIndex themselves.
 * @tparam Tcmd The command-id to execute.
 * @tparam Tret Return type of the command.
 * @tparam Targs The command parameter types.
 */
template <Commands Tcmd, typename Tret, typename... Targs>
struct CommandHelper<Tcmd, Tret(*)(DoCommandFlag, Targs...), false> : CommandHelper<Tcmd, Tret(*)(DoCommandFlag, Targs...), true>
{
	/* Do not allow Post without explicit location. */
	static inline bool Post(StringID err_message, Targs... args) = delete;
	template <typename Tcallback>
	static inline bool Post(Tcallback *callback, Targs... args) = delete;
	static inline bool Post(Targs... args) = delete;
	template <typename Tcallback>
	static bool Post(StringID err_message, Tcallback *callback, Targs... args) = delete;

	/**
	 * Shortcut for Post when not using a callback.
	 * @param err_message Message prefix to show on error
	 * @param location Tile location for user feedback.
	 * @param args Parameters for the command
	 */
	static inline bool Post(StringID err_message, TileIndex location, Targs... args) { return Post<CommandCallback>(err_message, nullptr, location, std::forward<Targs>(args)...); }
	/**
	 * Shortcut for Post when not using an error message.
	 * @param callback A callback function to call after the command is finished
	 * @param location Tile location for user feedback.
	 * @param args Parameters for the command
	 */
	template <typename Tcallback>
	static inline bool Post(Tcallback *callback, TileIndex location, Targs... args) { return Post((StringID)0, callback, location, std::forward<Targs>(args)...); }
	/**
	 * Shortcut for Post when not using a callback or an error message.
	 * @param location Tile location for user feedback.
	 * @param args Parameters for the command*
	 */
	static inline bool Post(TileIndex location, Targs... args) { return Post<CommandCallback>((StringID)0, nullptr, location, std::forward<Targs>(args)...); }

	/**
	 * Post variant that takes a TileIndex (for error window location and text effects) for
	 * commands that don't take a TileIndex by themselves.
	 * @param err_message Message prefix to show on error
	 * @param callback A callback function to call after the command is finished
	 * @param args Parameters for the command
	 * @return \c true if the command succeeded, else \c false.
	 */
	template <typename Tcallback>
	static inline bool Post(StringID err_message, Tcallback *callback, TileIndex location, Targs... args)
	{
		return CommandHelper<Tcmd, Tret(*)(DoCommandFlag, Targs...), true>::InternalPost(err_message, callback, true, false, location, std::forward_as_tuple(args...));
	}
};

#ifdef SILENCE_GCC_FUNCTION_POINTER_CAST
#	pragma GCC diagnostic pop
#endif

template <Commands Tcmd>
using Command = CommandHelper<Tcmd, typename CommandTraits<Tcmd>::ProcType, (GetCommandFlags<Tcmd>() & CMD_LOCATION) == 0>;

#endif /* COMMAND_FUNC_H */
