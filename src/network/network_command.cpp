/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_command.cpp Command handling over network connections. */

#include "../stdafx.h"
#include "network_admin.h"
#include "network_client.h"
#include "network_server.h"
#include "../command_func.h"
#include "../company_func.h"
#include "../settings_type.h"
#include "../airport_cmd.h"
#include "../aircraft_cmd.h"
#include "../autoreplace_cmd.h"
#include "../company_cmd.h"
#include "../depot_cmd.h"
#include "../dock_cmd.h"
#include "../economy_cmd.h"
#include "../engine_cmd.h"
#include "../error_func.h"
#include "../goal_cmd.h"
#include "../group_cmd.h"
#include "../industry_cmd.h"
#include "../landscape_cmd.h"
#include "../league_cmd.h"
#include "../misc_cmd.h"
#include "../news_cmd.h"
#include "../object_cmd.h"
#include "../order_cmd.h"
#include "../rail_cmd.h"
#include "../road_cmd.h"
#include "../roadveh_cmd.h"
#include "../settings_cmd.h"
#include "../signs_cmd.h"
#include "../station_cmd.h"
#include "../story_cmd.h"
#include "../subsidy_cmd.h"
#include "../terraform_cmd.h"
#include "../timetable_cmd.h"
#include "../town_cmd.h"
#include "../train_cmd.h"
#include "../tree_cmd.h"
#include "../tunnelbridge_cmd.h"
#include "../vehicle_cmd.h"
#include "../viewport_cmd.h"
#include "../water_cmd.h"
#include "../waypoint_cmd.h"
#include "../script/script_cmd.h"

#include "../safeguards.h"

/** Typed list of all possible callbacks. */
static constexpr auto _callback_tuple = std::make_tuple(
	(CommandCallback *)nullptr, // Make sure this is actually a pointer-to-function.
	&CcBuildPrimaryVehicle,
	&CcBuildAirport,
	&CcBuildBridge,
	&CcPlaySound_CONSTRUCTION_WATER,
	&CcBuildDocks,
	&CcFoundTown,
	&CcBuildRoadTunnel,
	&CcBuildRailTunnel,
	&CcBuildWagon,
	&CcRoadDepot,
	&CcRailDepot,
	&CcPlaceSign,
	&CcPlaySound_EXPLOSION,
	&CcPlaySound_CONSTRUCTION_OTHER,
	&CcPlaySound_CONSTRUCTION_RAIL,
	&CcStation,
	&CcTerraform,
	&CcAI,
	&CcCloneVehicle,
	&CcCreateGroup,
	&CcFoundRandomTown,
	&CcRoadStop,
	&CcBuildIndustry,
	&CcStartStopVehicle,
	&CcGame,
	&CcAddVehicleNewGroup
);

#ifdef SILENCE_GCC_FUNCTION_POINTER_CAST
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
#endif

/* Helpers to generate the callback table from the callback list. */

inline constexpr size_t _callback_tuple_size = std::tuple_size_v<decltype(_callback_tuple)>;

template <size_t... i>
inline auto MakeCallbackTable(std::index_sequence<i...>) noexcept
{
	return std::array<CommandCallback *, sizeof...(i)>{{ reinterpret_cast<CommandCallback *>(reinterpret_cast<void(*)()>(std::get<i>(_callback_tuple)))... }}; // MingW64 fails linking when casting a pointer to its own type. To work around, cast it to some other type first.
}

/** Type-erased table of callbacks. */
static auto _callback_table = MakeCallbackTable(std::make_index_sequence<_callback_tuple_size>{});

template <typename T> struct CallbackArgsHelper;
template <typename... Targs>
struct CallbackArgsHelper<void(*const)(Commands, const CommandCost &, Targs...)> {
	using Args = std::tuple<std::decay_t<Targs>...>;
};


/* Helpers to generate the command dispatch table from the command traits. */

template <Commands Tcmd> static CommandDataBuffer SanitizeCmdStrings(const CommandDataBuffer &data);
template <Commands Tcmd, size_t cb> static void UnpackNetworkCommand(const CommandPacket &cp);
template <Commands Tcmd> static void NetworkReplaceCommandClientId(CommandPacket &cp, ClientID client_id);
using UnpackNetworkCommandProc = void (*)(const CommandPacket &);
using UnpackDispatchT = std::array<UnpackNetworkCommandProc, _callback_tuple_size>;
struct CommandDispatch {
	CommandDataBuffer(*Sanitize)(const CommandDataBuffer &);
	void (*ReplaceClientId)(CommandPacket &, ClientID);
	UnpackDispatchT Unpack;
};

template <Commands Tcmd, size_t Tcb>
constexpr UnpackNetworkCommandProc MakeUnpackNetworkCommandCallback() noexcept
{
	/* Check if the callback matches with the command arguments. If not, don't generate an Unpack proc. */
	using Tcallback = std::tuple_element_t<Tcb, decltype(_callback_tuple)>;
	if constexpr (std::is_same_v<Tcallback, CommandCallback * const> || // Callback type is CommandCallback.
			std::is_same_v<Tcallback, CommandCallbackData * const> || // Callback type is CommandCallbackData.
			std::is_same_v<typename CommandTraits<Tcmd>::CbArgs, typename CallbackArgsHelper<Tcallback>::Args> || // Callback proc takes all command return values and parameters.
			(!std::is_void_v<typename CommandTraits<Tcmd>::RetTypes> && std::is_same_v<typename CallbackArgsHelper<typename CommandTraits<Tcmd>::RetCallbackProc const>::Args, typename CallbackArgsHelper<Tcallback>::Args>)) { // Callback return is more than CommandCost and the proc takes all return values.
		return &UnpackNetworkCommand<Tcmd, Tcb>;
	} else {
		return nullptr;
	}
}

template <Commands Tcmd, size_t... i>
constexpr UnpackDispatchT MakeUnpackNetworkCommand(std::index_sequence<i...>) noexcept
{
	return UnpackDispatchT{{ MakeUnpackNetworkCommandCallback<Tcmd, i>()...}};
}

template <typename T, T... i, size_t... j>
inline constexpr auto MakeDispatchTable(std::integer_sequence<T, i...>, std::index_sequence<j...>) noexcept
{
	return std::array<CommandDispatch, sizeof...(i)>{{ { &SanitizeCmdStrings<static_cast<Commands>(i)>, &NetworkReplaceCommandClientId<static_cast<Commands>(i)>, MakeUnpackNetworkCommand<static_cast<Commands>(i)>(std::make_index_sequence<_callback_tuple_size>{}) }... }};
}
/** Command dispatch table. */
static constexpr auto _cmd_dispatch = MakeDispatchTable(std::make_integer_sequence<std::underlying_type_t<Commands>, CMD_END>{}, std::make_index_sequence<_callback_tuple_size>{});

#ifdef SILENCE_GCC_FUNCTION_POINTER_CAST
#	pragma GCC diagnostic pop
#endif

/** Local queue of packets waiting for handling. */
static CommandQueue _local_wait_queue;
/** Local queue of packets waiting for execution. */
static CommandQueue _local_execution_queue;


/**
 * Find the callback index of a callback pointer.
 * @param callback Address of callback to search for.
 * @return Callback index or std::numeric_limits<size_t>::max() if the function wasn't found in the callback list.
 */
static size_t FindCallbackIndex(CommandCallback *callback)
{
	if (auto it = std::find(std::cbegin(_callback_table), std::cend(_callback_table), callback); it != std::cend(_callback_table)) {
		return static_cast<size_t>(std::distance(std::cbegin(_callback_table), it));
	}

	return std::numeric_limits<size_t>::max();
}

/**
 * Prepare a DoCommand to be send over the network
 * @param cmd The command to execute (a CMD_* value)
 * @param err_message Message prefix to show on error
 * @param callback A callback function to call after the command is finished
 * @param company The company that wants to send the command
 * @param cmd_data The command proc arguments.
 */
void NetworkSendCommand(Commands cmd, StringID err_message, CommandCallback *callback, CompanyID company, const CommandDataBuffer &cmd_data)
{
	CommandPacket c;
	c.company  = company;
	c.cmd      = cmd;
	c.err_msg  = err_message;
	c.callback = callback;
	c.data     = cmd_data;

	if (_network_server) {
		/* If we are the server, we queue the command in our 'special' queue.
		 *   In theory, we could execute the command right away, but then the
		 *   client on the server can do everything 1 tick faster than others.
		 *   So to keep the game fair, we delay the command with 1 tick
		 *   which gives about the same speed as most clients.
		 */
		c.frame = _frame_counter_max + 1;
		c.my_cmd = true;

		_local_wait_queue.push_back(c);
		return;
	}

	c.frame = 0; // The client can't tell which frame, so just make it 0

	/* Clients send their command to the server and forget all about the packet */
	MyClient::SendCommand(c);
}

/**
 * Sync our local command queue to the command queue of the given
 * socket. This is needed for the case where we receive a command
 * before saving the game for a joining client, but without the
 * execution of those commands. Not syncing those commands means
 * that the client will never get them and as such will be in a
 * desynced state from the time it started with joining.
 * @param cs The client to sync the queue to.
 */
void NetworkSyncCommandQueue(NetworkClientSocket *cs)
{
	for (auto &p : _local_execution_queue) {
		CommandPacket &c = cs->outgoing_queue.emplace_back(p);
		c.callback = nullptr;
	}
}

/**
 * Execute all commands on the local command queue that ought to be executed this frame.
 */
void NetworkExecuteLocalCommandQueue()
{
	assert(IsLocalCompany());

	CommandQueue &queue = (_network_server ? _local_execution_queue : ClientNetworkGameSocketHandler::my_client->incoming_queue);

	auto cp = queue.begin();
	for (; cp != queue.end(); cp++) {
		/* The queue is always in order, which means
		 * that the first element will be executed first. */
		if (_frame_counter < cp->frame) break;

		if (_frame_counter > cp->frame) {
			/* If we reach here, it means for whatever reason, we've already executed
			 * past the command we need to execute. */
			FatalError("[net] Trying to execute a packet in the past!");
		}

		/* We can execute this command */
		_current_company = cp->company;
		size_t cb_index = FindCallbackIndex(cp->callback);
		assert(cb_index < _callback_tuple_size);
		assert(_cmd_dispatch[cp->cmd].Unpack[cb_index] != nullptr);
		_cmd_dispatch[cp->cmd].Unpack[cb_index](*cp);
	}
	queue.erase(queue.begin(), cp);

	/* Local company may have changed, so we should not restore the old value */
	_current_company = _local_company;
}

/**
 * Free the local command queues.
 */
void NetworkFreeLocalCommandQueue()
{
	_local_wait_queue.clear();
	_local_execution_queue.clear();
}

/**
 * "Send" a particular CommandPacket to all clients.
 * @param cp    The command that has to be distributed.
 * @param owner The client that owns the command,
 */
static void DistributeCommandPacket(CommandPacket &cp, const NetworkClientSocket *owner)
{
	CommandCallback *callback = cp.callback;
	cp.frame = _frame_counter_max + 1;

	for (NetworkClientSocket *cs : NetworkClientSocket::Iterate()) {
		if (cs->status >= NetworkClientSocket::STATUS_MAP) {
			/* Callbacks are only send back to the client who sent them in the
			 *  first place. This filters that out. */
			cp.callback = (cs != owner) ? nullptr : callback;
			cp.my_cmd = (cs == owner);
			cs->outgoing_queue.push_back(cp);
		}
	}

	cp.callback = (nullptr != owner) ? nullptr : callback;
	cp.my_cmd = (nullptr == owner);
	_local_execution_queue.push_back(cp);
}

/**
 * "Send" a particular CommandQueue to all clients.
 * @param queue The queue of commands that has to be distributed.
 * @param owner The client that owns the commands,
 */
static void DistributeQueue(CommandQueue &queue, const NetworkClientSocket *owner)
{
#ifdef DEBUG_DUMP_COMMANDS
	/* When replaying we do not want this limitation. */
	int to_go = UINT16_MAX;
#else
	int to_go = _settings_client.network.commands_per_frame;
	if (owner == nullptr) {
		/* This is the server, use the commands_per_frame_server setting if higher */
		to_go = std::max<int>(to_go, _settings_client.network.commands_per_frame_server);
	}
#endif

	/* Not technically the most performant way, but consider clients rarely click more than once per tick. */
	for (auto cp = queue.begin(); cp != queue.end(); /* removing some items */) {
		/* Do not distribute commands when paused and the command is not allowed while paused. */
		if (_pause_mode != PM_UNPAUSED && !IsCommandAllowedWhilePaused(cp->cmd)) {
			++cp;
			continue;
		}

		/* Limit the number of commands per client per tick. */
		if (--to_go < 0) break;

		DistributeCommandPacket(*cp, owner);
		NetworkAdminCmdLogging(owner, *cp);
		cp = queue.erase(cp);
	}
}

/** Distribute the commands of ourself and the clients. */
void NetworkDistributeCommands()
{
	/* First send the server's commands. */
	DistributeQueue(_local_wait_queue, nullptr);

	/* Then send the queues of the others. */
	for (NetworkClientSocket *cs : NetworkClientSocket::Iterate()) {
		DistributeQueue(cs->incoming_queue, cs);
	}
}

/**
 * Receives a command from the network.
 * @param p the packet to read from.
 * @param cp the struct to write the data to.
 * @return an error message. When nullptr there has been no error.
 */
const char *NetworkGameSocketHandler::ReceiveCommand(Packet &p, CommandPacket &cp)
{
	cp.company = (CompanyID)p.Recv_uint8();
	cp.cmd     = static_cast<Commands>(p.Recv_uint16());
	if (!IsValidCommand(cp.cmd))               return "invalid command";
	if (GetCommandFlags(cp.cmd) & CMD_OFFLINE) return "single-player only command";
	cp.err_msg = p.Recv_uint16();
	cp.data    = _cmd_dispatch[cp.cmd].Sanitize(p.Recv_buffer());

	uint8_t callback = p.Recv_uint8();
	if (callback >= _callback_table.size() || _cmd_dispatch[cp.cmd].Unpack[callback] == nullptr)  return "invalid callback";

	cp.callback = _callback_table[callback];
	return nullptr;
}

/**
 * Sends a command over the network.
 * @param p the packet to send it in.
 * @param cp the packet to actually send.
 */
void NetworkGameSocketHandler::SendCommand(Packet &p, const CommandPacket &cp)
{
	p.Send_uint8(cp.company);
	p.Send_uint16(cp.cmd);
	p.Send_uint16(cp.err_msg);
	p.Send_buffer(cp.data);

	size_t callback = FindCallbackIndex(cp.callback);
	if (callback > UINT8_MAX || _cmd_dispatch[cp.cmd].Unpack[callback] == nullptr) {
		Debug(net, 0, "Unknown callback for command; no callback sent (command: {})", cp.cmd);
		callback = 0; // _callback_table[0] == nullptr
	}
	p.Send_uint8 ((uint8_t)callback);
}

/** Helper to process a single ClientID argument. */
template <class T>
static inline void SetClientIdHelper(T &data, [[maybe_unused]] ClientID client_id)
{
	if constexpr (std::is_same_v<ClientID, T>) {
		data = client_id;
	}
}

/** Set all invalid ClientID's to the proper value. */
template<class Ttuple, size_t... Tindices>
static inline void SetClientIds(Ttuple &values, ClientID client_id, std::index_sequence<Tindices...>)
{
	((SetClientIdHelper(std::get<Tindices>(values), client_id)), ...);
}

template <Commands Tcmd>
static void NetworkReplaceCommandClientId(CommandPacket &cp, ClientID client_id)
{
	/* Unpack command parameters. */
	auto params = EndianBufferReader::ToValue<typename CommandTraits<Tcmd>::Args>(cp.data);

	/* Insert client id. */
	SetClientIds(params, client_id, std::make_index_sequence<std::tuple_size_v<decltype(params)>>{});

	/* Repack command parameters. */
	cp.data = EndianBufferWriter<CommandDataBuffer>::FromValue(params);
}

/**
 * Insert a client ID into the command data in a command packet.
 * @param cp Command packet to modify.
 * @param client_id Client id to insert.
 */
void NetworkReplaceCommandClientId(CommandPacket &cp, ClientID client_id)
{
	_cmd_dispatch[cp.cmd].ReplaceClientId(cp, client_id);
}


/** Validate a single string argument coming from network. */
template <class T>
static inline void SanitizeSingleStringHelper([[maybe_unused]] CommandFlags cmd_flags, T &data)
{
	if constexpr (std::is_same_v<std::string, T>) {
		data = StrMakeValid(data.substr(0, NETWORK_COMPANY_NAME_LENGTH), (!_network_server && cmd_flags & CMD_STR_CTRL) != 0 ? SVS_ALLOW_CONTROL_CODE | SVS_REPLACE_WITH_QUESTION_MARK : SVS_REPLACE_WITH_QUESTION_MARK);
	}
}

/** Helper function to perform validation on command data strings. */
template<class Ttuple, size_t... Tindices>
static inline void SanitizeStringsHelper(CommandFlags cmd_flags, Ttuple &values, std::index_sequence<Tindices...>)
{
	((SanitizeSingleStringHelper(cmd_flags, std::get<Tindices>(values))), ...);
}

/**
 * Validate and sanitize strings in command data.
 * @tparam Tcmd Command this data belongs to.
 * @param data Command data.
 * @return Sanitized command data.
 */
template <Commands Tcmd>
CommandDataBuffer SanitizeCmdStrings(const CommandDataBuffer &data)
{
	auto args = EndianBufferReader::ToValue<typename CommandTraits<Tcmd>::Args>(data);
	SanitizeStringsHelper(CommandTraits<Tcmd>::flags, args, std::make_index_sequence<std::tuple_size_v<typename CommandTraits<Tcmd>::Args>>{});
	return EndianBufferWriter<CommandDataBuffer>::FromValue(args);
}

/**
 * Unpack a generic command packet into its actual typed components.
 * @tparam Tcmd Command type to be unpacked.
 * @tparam Tcb Index into the callback list.
 * @param cp Command packet to unpack.
 */
template <Commands Tcmd, size_t Tcb>
void UnpackNetworkCommand(const CommandPacket &cp)
{
	auto args = EndianBufferReader::ToValue<typename CommandTraits<Tcmd>::Args>(cp.data);
	Command<Tcmd>::PostFromNet(cp.err_msg, std::get<Tcb>(_callback_tuple), cp.my_cmd, args);
}
