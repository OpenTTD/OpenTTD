/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_object.hpp Main object, on which all objects depend. */

#ifndef SCRIPT_OBJECT_HPP
#define SCRIPT_OBJECT_HPP

#include "../../misc/countedptr.hpp"
#include "../../road_type.h"
#include "../../rail_type.h"
#include "../../string_func.h"
#include "../../command_func.h"
#include "../../core/random_func.hpp"

#include "script_types.hpp"
#include "script_log_types.hpp"
#include "../script_suspend.hpp"
#include "../squirrel.hpp"

#include <utility>

/**
 * The callback function for Mode-classes.
 */
typedef bool (ScriptModeProc)();

/**
 * The callback function for Async Mode-classes.
 */
typedef bool (ScriptAsyncModeProc)();

/**
 * Uper-parent object of all API classes. You should never use this class in
 *   your script, as it doesn't publish any public functions. It is used
 *   internally to have a common place to handle general things, like internal
 *   command processing, and command-validation checks.
 * @api none
 */
class ScriptObject : public SimpleCountedObject {
friend class ScriptInstance;
friend class ScriptController;
friend class TestScriptController;
protected:
	/**
	 * A class that handles the current active instance. By instantiating it at
	 *  the beginning of a function with the current active instance, it remains
	 *  active till the scope of the variable closes. It then automatically
	 *  reverts to the active instance it was before instantiating.
	 */
	class ActiveInstance {
	friend class ScriptObject;
	public:
		ActiveInstance(ScriptInstance *instance);
		~ActiveInstance();
	private:
		ScriptInstance *last_active;    ///< The active instance before we go instantiated.
		ScriptAllocatorScope alc_scope; ///< Keep the correct allocator for the script instance activated

		static ScriptInstance *active;  ///< The global current active instance.
	};

public:
	/**
	 * Store the latest result of a DoCommand per company.
	 * @param res The result of the last command.
	 */
	static void SetLastCommandRes(bool res);

	/**
	 * Store the extra data return by the last DoCommand.
	 * @param data Extra data return by the command.
	 */
	static void SetLastCommandResData(CommandDataBuffer data);

	/**
	 * Get the currently active instance.
	 * @return The instance.
	 */
	static class ScriptInstance *GetActiveInstance();

	/**
	 * Get a reference of the randomizer that brings this script random values.
	 * @param owner The owner/script to get the randomizer for. This defaults to ScriptObject::GetRootCompany()
	 */
	static Randomizer &GetRandomizer(Owner owner = ScriptObject::GetRootCompany());

	/**
	 * Initialize/reset the script random states. The state of the scripts are
	 * based on the current _random seed, but _random does not get changed.
	 */
	static void InitializeRandomizers();

protected:
	template<Commands TCmd, typename T> struct ScriptDoCommandHelper;

	/**
	 * Templated wrapper that exposes the command parameter arguments
	 * on the various DoCommand calls.
	 * @tparam Tcmd The command-id to execute.
	 * @tparam Tret Return type of the command.
	 * @tparam Targs The command parameter types.
	 */
	template <Commands Tcmd, typename Tret, typename... Targs>
	struct ScriptDoCommandHelper<Tcmd, Tret(*)(DoCommandFlag, Targs...)> {
		static bool Do(Script_SuspendCallbackProc *callback, Targs... args)
		{
			return Execute(callback, std::forward_as_tuple(args...));
		}

		static bool Do(Targs... args)
		{
			return Execute(nullptr, std::forward_as_tuple(args...));
		}

	private:
		static bool Execute(Script_SuspendCallbackProc *callback, std::tuple<Targs...> args);
	};

	template <Commands Tcmd>
	using Command = ScriptDoCommandHelper<Tcmd, typename ::CommandTraits<Tcmd>::ProcType>;

	/**
	 * Store the latest command executed by the script.
	 */
	static void SetLastCommand(const CommandDataBuffer &data, Commands cmd);

	/**
	 * Check if it's the latest command executed by the script.
	 */
	static bool CheckLastCommand(const CommandDataBuffer &data, Commands cmd);

	/**
	 * Sets the DoCommand costs counter to a value.
	 */
	static void SetDoCommandCosts(Money value);

	/**
	 * Increase the current value of the DoCommand costs counter.
	 */
	static void IncreaseDoCommandCosts(Money value);

	/**
	 * Get the current DoCommand costs counter.
	 */
	static Money GetDoCommandCosts();

	/**
	 * Set the DoCommand last error.
	 */
	static void SetLastError(ScriptErrorType last_error);

	/**
	 * Get the DoCommand last error.
	 */
	static ScriptErrorType GetLastError();

	/**
	 * Set the road type.
	 */
	static void SetRoadType(RoadType road_type);

	/**
	 * Get the road type.
	 */
	static RoadType GetRoadType();

	/**
	 * Set the rail type.
	 */
	static void SetRailType(RailType rail_type);

	/**
	 * Get the rail type.
	 */
	static RailType GetRailType();

	/**
	 * Set the current mode of your script to this proc.
	 */
	static void SetDoCommandMode(ScriptModeProc *proc, ScriptObject *instance);

	/**
	 * Get the current mode your script is currently under.
	 */
	static ScriptModeProc *GetDoCommandMode();

	/**
	 * Get the instance of the current mode your script is currently under.
	 */
	static ScriptObject *GetDoCommandModeInstance();

	/**
	 * Set the current async mode of your script to this proc.
	 */
	static void SetDoCommandAsyncMode(ScriptAsyncModeProc *proc, ScriptObject *instance);

	/**
	 * Get the current async mode your script is currently under.
	 */
	static ScriptModeProc *GetDoCommandAsyncMode();

	/**
	 * Get the instance of the current async mode your script is currently under.
	 */
	static ScriptObject *GetDoCommandAsyncModeInstance();

	/**
	 * Set the delay of the DoCommand.
	 */
	static void SetDoCommandDelay(uint ticks);

	/**
	 * Get the delay of the DoCommand.
	 */
	static uint GetDoCommandDelay();

	/**
	 * Get the latest result of a DoCommand.
	 */
	static bool GetLastCommandRes();

	/**
	 * Get the extra return data from the last DoCommand.
	 */
	static const CommandDataBuffer &GetLastCommandResData();

	/**
	 * Store a allow_do_command per company.
	 * @param allow The new allow.
	 */
	static void SetAllowDoCommand(bool allow);

	/**
	 * Get the internal value of allow_do_command. This can differ
	 * from CanSuspend() if the reason we are not allowed
	 * to execute a DoCommand is in squirrel and not the API.
	 * In that case use this function to restore the previous value.
	 * @return True iff DoCommands are allowed in the current scope.
	 */
	static bool GetAllowDoCommand();

	/**
	 * Set the current company to execute commands for or request
	 *  information about.
	 * @param company The new company.
	 */
	static void SetCompany(CompanyID company);

	/**
	 * Get the current company we are executing commands for or
	 *  requesting information about.
	 * @return The current company.
	 */
	static CompanyID GetCompany();

	/**
	 * Get the root company, the company that the script really
	 *  runs under / for.
	 * @return The root company.
	 */
	static CompanyID GetRootCompany();

	/**
	 * Set the cost of the last command.
	 */
	static void SetLastCost(Money last_cost);

	/**
	 * Get the cost of the last command.
	 */
	static Money GetLastCost();

	/**
	 * Set a variable that can be used by callback functions to pass information.
	 */
	static void SetCallbackVariable(int index, int value);

	/**
	 * Get the variable that is used by callback functions to pass information.
	 */
	static int GetCallbackVariable(int index);

	/**
	 * Can we suspend the script at this moment?
	 */
	static bool CanSuspend();

	/**
	 * Get the pointer to store event data in.
	 */
	static void *&GetEventPointer();

	/**
	 * Get the pointer to store log message in.
	 */
	static ScriptLogTypes::LogData &GetLogData();

	/**
	 * Get an allocated string with all control codes stripped off.
	 */
	static std::string GetString(StringID string);

private:
	/* Helper functions for DoCommand. */
	static std::tuple<bool, bool, bool, bool> DoCommandPrep();
	static bool DoCommandProcessResult(const CommandCost &res, Script_SuspendCallbackProc *callback, bool estimate_only, bool asynchronous);
	static CommandCallbackData *GetDoCommandCallback();
	static Randomizer random_states[OWNER_END]; ///< Random states for each of the scripts (game script uses OWNER_DEITY)
};

namespace ScriptObjectInternal {
	/** Validate a single string argument coming from network. */
	template <class T>
	static inline void SanitizeSingleStringHelper(T &data)
	{
		if constexpr (std::is_same_v<std::string, T>) {
			/* The string must be valid, i.e. not contain special codes. Since some
			 * can be made with GSText, make sure the control codes are removed. */
			data = ::StrMakeValid(data, SVS_NONE);
		}
	}

	/** Helper function to perform validation on command data strings. */
	template<class Ttuple, size_t... Tindices>
	static inline void SanitizeStringsHelper(Ttuple &values, std::index_sequence<Tindices...>)
	{
		((SanitizeSingleStringHelper(std::get<Tindices>(values))), ...);
	}

	/** Helper to process a single ClientID argument. */
	template <class T>
	static inline void SetClientIdHelper(T &data)
	{
		if constexpr (std::is_same_v<ClientID, T>) {
			if (data == INVALID_CLIENT_ID) data = (ClientID)UINT32_MAX;
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
}

template <Commands Tcmd, typename Tret, typename... Targs>
bool ScriptObject::ScriptDoCommandHelper<Tcmd, Tret(*)(DoCommandFlag, Targs...)>::Execute(Script_SuspendCallbackProc *callback, std::tuple<Targs...> args)
{
	auto [err, estimate_only, asynchronous, networking] = ScriptObject::DoCommandPrep();
	if (err) return false;

	if ((::GetCommandFlags<Tcmd>() & CMD_STR_CTRL) == 0) {
		ScriptObjectInternal::SanitizeStringsHelper(args, std::index_sequence_for<Targs...>{});
	}

	TileIndex tile{};
	if constexpr (std::is_same_v<TileIndex, std::tuple_element_t<0, decltype(args)>>) {
		tile = std::get<0>(args);
	}

	/* Do not even think about executing out-of-bounds tile-commands. */
	if (tile != 0 && (tile >= Map::Size() || (!IsValidTile(tile) && (GetCommandFlags<Tcmd>() & CMD_ALL_TILES) == 0))) return false;

	/* Only set ClientID parameters when the command does not come from the network. */
	if constexpr ((::GetCommandFlags<Tcmd>() & CMD_CLIENT_ID) != 0) ScriptObjectInternal::SetClientIds(args, std::index_sequence_for<Targs...>{});

	/* Store the command for command callback validation. */
	if (!estimate_only && networking) ScriptObject::SetLastCommand(EndianBufferWriter<CommandDataBuffer>::FromValue(args), Tcmd);

	/* Try to perform the command. */
	Tret res = ::Command<Tcmd>::Unsafe((StringID)0, networking ? ScriptObject::GetDoCommandCallback() : nullptr, false, estimate_only, tile, args);

	if constexpr (std::is_same_v<Tret, CommandCost>) {
		return ScriptObject::DoCommandProcessResult(res, callback, estimate_only, asynchronous);
	} else {
		ScriptObject::SetLastCommandResData(EndianBufferWriter<CommandDataBuffer>::FromValue(ScriptObjectInternal::RemoveFirstTupleElement(res)));
		return ScriptObject::DoCommandProcessResult(std::get<0>(res), callback, estimate_only, asynchronous);
	}
}

/**
 * Internally used class to automate the ScriptObject reference counting.
 * @api -all
 */
template <typename T>
class ScriptObjectRef {
private:
	T *data; ///< The reference counted object.
public:
	/**
	 * Create the reference counter for the given ScriptObject instance.
	 * @param data The underlying object.
	 */
	ScriptObjectRef(T *data) : data(data)
	{
		this->data->AddRef();
	}

	/* No copy constructor. */
	ScriptObjectRef(const ScriptObjectRef<T> &ref) = delete;

	/* Move constructor. */
	ScriptObjectRef(ScriptObjectRef<T> &&ref) noexcept : data(std::exchange(ref.data, nullptr))
	{
	}

	/* No copy assignment. */
	ScriptObjectRef& operator=(const ScriptObjectRef<T> &other) = delete;

	/* Move assignment. */
	ScriptObjectRef& operator=(ScriptObjectRef<T> &&other) noexcept
	{
		std::swap(this->data, other.data);
		return *this;
	}

	/**
	 * Release the reference counted object.
	 */
	~ScriptObjectRef()
	{
		if (this->data != nullptr) this->data->Release();
	}

	/**
	 * Dereferencing this reference returns a reference to the reference
	 * counted object
	 * @return Reference to the underlying object.
	 */
	T &operator*()
	{
		return *this->data;
	}

	/**
	 * The arrow operator on this reference returns the reference counted object.
	 * @return Pointer to the underlying object.
	 */
	T *operator->()
	{
		return this->data;
	}
};

#endif /* SCRIPT_OBJECT_HPP */
