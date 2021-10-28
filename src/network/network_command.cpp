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
#include "../goal_cmd.h"
#include "../group_cmd.h"
#include "../industry_cmd.h"
#include "../landscape_cmd.h"
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
#include <array>

#include "../safeguards.h"

/** Table with all the callbacks we'll use for conversion*/
static CommandCallback * const _callback_table[] = {
	/* 0x00 */ nullptr,
	/* 0x01 */ CcBuildPrimaryVehicle,
	/* 0x02 */ CcBuildAirport,
	/* 0x03 */ CcBuildBridge,
	/* 0x04 */ CcPlaySound_CONSTRUCTION_WATER,
	/* 0x05 */ CcBuildDocks,
	/* 0x06 */ CcFoundTown,
	/* 0x07 */ CcBuildRoadTunnel,
	/* 0x08 */ CcBuildRailTunnel,
	/* 0x09 */ CcBuildWagon,
	/* 0x0A */ CcRoadDepot,
	/* 0x0B */ CcRailDepot,
	/* 0x0C */ CcPlaceSign,
	/* 0x0D */ CcPlaySound_EXPLOSION,
	/* 0x0E */ CcPlaySound_CONSTRUCTION_OTHER,
	/* 0x0F */ CcPlaySound_CONSTRUCTION_RAIL,
	/* 0x10 */ CcStation,
	/* 0x11 */ CcTerraform,
	/* 0x12 */ CcAI,
	/* 0x13 */ CcCloneVehicle,
	/* 0x14 */ nullptr,
	/* 0x15 */ CcCreateGroup,
	/* 0x16 */ CcFoundRandomTown,
	/* 0x17 */ CcRoadStop,
	/* 0x18 */ CcBuildIndustry,
	/* 0x19 */ CcStartStopVehicle,
	/* 0x1A */ CcGame,
	/* 0x1B */ CcAddVehicleNewGroup,
};

/* Helpers to generate the command dispatch table from the command traits. */

template <Commands Tcmd> static CommandDataBuffer SanitizeCmdStrings(const CommandDataBuffer &data);
template <Commands Tcmd> static void UnpackNetworkCommand(const CommandPacket *cp);
struct CommandDispatch {
	CommandDataBuffer(*Sanitize)(const CommandDataBuffer &);
	void (*Unpack)(const CommandPacket *);
};

template<typename T, T... i>
inline constexpr auto MakeDispatchTable(std::integer_sequence<T, i...>) noexcept
{
	return std::array<CommandDispatch, sizeof...(i)>{{ { &SanitizeCmdStrings<static_cast<Commands>(i)>, &UnpackNetworkCommand<static_cast<Commands>(i)> }... }};
}
static constexpr auto _cmd_dispatch = MakeDispatchTable(std::make_integer_sequence<std::underlying_type_t<Commands>, CMD_END>{});


/**
 * Append a CommandPacket at the end of the queue.
 * @param p The packet to append to the queue.
 * @note A new instance of the CommandPacket will be made.
 */
void CommandQueue::Append(CommandPacket *p)
{
	CommandPacket *add = new CommandPacket();
	*add = *p;
	add->next = nullptr;
	if (this->first == nullptr) {
		this->first = add;
	} else {
		this->last->next = add;
	}
	this->last = add;
	this->count++;
}

/**
 * Return the first item in the queue and remove it from the queue.
 * @param ignore_paused Whether to ignore commands that may not be executed while paused.
 * @return the first item in the queue.
 */
CommandPacket *CommandQueue::Pop(bool ignore_paused)
{
	CommandPacket **prev = &this->first;
	CommandPacket *ret = this->first;
	CommandPacket *prev_item = nullptr;
	if (ignore_paused && _pause_mode != PM_UNPAUSED) {
		while (ret != nullptr && !IsCommandAllowedWhilePaused(ret->cmd)) {
			prev_item = ret;
			prev = &ret->next;
			ret = ret->next;
		}
	}
	if (ret != nullptr) {
		if (ret == this->last) this->last = prev_item;
		*prev = ret->next;
		this->count--;
	}
	return ret;
}

/**
 * Return the first item in the queue, but don't remove it.
 * @param ignore_paused Whether to ignore commands that may not be executed while paused.
 * @return the first item in the queue.
 */
CommandPacket *CommandQueue::Peek(bool ignore_paused)
{
	if (!ignore_paused || _pause_mode == PM_UNPAUSED) return this->first;

	for (CommandPacket *p = this->first; p != nullptr; p = p->next) {
		if (IsCommandAllowedWhilePaused(p->cmd)) return p;
	}
	return nullptr;
}

/** Free everything that is in the queue. */
void CommandQueue::Free()
{
	CommandPacket *cp;
	while ((cp = this->Pop()) != nullptr) {
		delete cp;
	}
	assert(this->count == 0);
}

/** Local queue of packets waiting for handling. */
static CommandQueue _local_wait_queue;
/** Local queue of packets waiting for execution. */
static CommandQueue _local_execution_queue;

/**
 * Prepare a DoCommand to be send over the network
 * @param cmd The command to execute (a CMD_* value)
 * @param err_message Message prefix to show on error
 * @param callback A callback function to call after the command is finished
 * @param company The company that wants to send the command
 * @param tile The tile to perform a command on (see #CommandProc)
 * @param p1 Additional data for the command (see #CommandProc)
 * @param p2 Additional data for the command (see #CommandProc)
 * @param text The text to pass
 */
void NetworkSendCommand(Commands cmd, StringID err_message, CommandCallback *callback, CompanyID company, TileIndex tile, uint32 p1, uint32 p2, const std::string &text)
{
	auto data = EndianBufferWriter<CommandDataBuffer>::FromValue(std::make_tuple(tile, p1, p2, text));
	NetworkSendCommand(cmd, err_message, callback, company, tile, data);
}

/**
 * Prepare a DoCommand to be send over the network
 * @param cmd The command to execute (a CMD_* value)
 * @param err_message Message prefix to show on error
 * @param callback A callback function to call after the command is finished
 * @param company The company that wants to send the command
 * @param location Location of the command (e.g. for error message position)
 * @param cmd_data The command proc arguments.
 */
void NetworkSendCommand(Commands cmd, StringID err_message, CommandCallback *callback, CompanyID company, TileIndex location, const CommandDataBuffer &cmd_data)
{
	CommandPacket c;
	c.company  = company;
	c.cmd      = cmd;
	c.err_msg  = err_message;
	c.callback = callback;
	c.tile     = location;
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

		_local_wait_queue.Append(&c);
		return;
	}

	c.frame = 0; // The client can't tell which frame, so just make it 0

	/* Clients send their command to the server and forget all about the packet */
	MyClient::SendCommand(&c);
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
	for (CommandPacket *p = _local_execution_queue.Peek(); p != nullptr; p = p->next) {
		CommandPacket c = *p;
		c.callback = nullptr;
		cs->outgoing_queue.Append(&c);
	}
}

/**
 * Execute all commands on the local command queue that ought to be executed this frame.
 */
void NetworkExecuteLocalCommandQueue()
{
	assert(IsLocalCompany());

	CommandQueue &queue = (_network_server ? _local_execution_queue : ClientNetworkGameSocketHandler::my_client->incoming_queue);

	CommandPacket *cp;
	while ((cp = queue.Peek()) != nullptr) {
		/* The queue is always in order, which means
		 * that the first element will be executed first. */
		if (_frame_counter < cp->frame) break;

		if (_frame_counter > cp->frame) {
			/* If we reach here, it means for whatever reason, we've already executed
			 * past the command we need to execute. */
			error("[net] Trying to execute a packet in the past!");
		}

		/* We can execute this command */
		_current_company = cp->company;
		_cmd_dispatch[cp->cmd].Unpack(cp);

		queue.Pop();
		delete cp;
	}

	/* Local company may have changed, so we should not restore the old value */
	_current_company = _local_company;
}

/**
 * Free the local command queues.
 */
void NetworkFreeLocalCommandQueue()
{
	_local_wait_queue.Free();
	_local_execution_queue.Free();
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
			cs->outgoing_queue.Append(&cp);
		}
	}

	cp.callback = (nullptr != owner) ? nullptr : callback;
	cp.my_cmd = (nullptr == owner);
	_local_execution_queue.Append(&cp);
}

/**
 * "Send" a particular CommandQueue to all clients.
 * @param queue The queue of commands that has to be distributed.
 * @param owner The client that owns the commands,
 */
static void DistributeQueue(CommandQueue *queue, const NetworkClientSocket *owner)
{
#ifdef DEBUG_DUMP_COMMANDS
	/* When replaying we do not want this limitation. */
	int to_go = UINT16_MAX;
#else
	int to_go = _settings_client.network.commands_per_frame;
#endif

	CommandPacket *cp;
	while (--to_go >= 0 && (cp = queue->Pop(true)) != nullptr) {
		DistributeCommandPacket(*cp, owner);
		NetworkAdminCmdLogging(owner, cp);
		delete cp;
	}
}

/** Distribute the commands of ourself and the clients. */
void NetworkDistributeCommands()
{
	/* First send the server's commands. */
	DistributeQueue(&_local_wait_queue, nullptr);

	/* Then send the queues of the others. */
	for (NetworkClientSocket *cs : NetworkClientSocket::Iterate()) {
		DistributeQueue(&cs->incoming_queue, cs);
	}
}

/**
 * Receives a command from the network.
 * @param p the packet to read from.
 * @param cp the struct to write the data to.
 * @return an error message. When nullptr there has been no error.
 */
const char *NetworkGameSocketHandler::ReceiveCommand(Packet *p, CommandPacket *cp)
{
	cp->company = (CompanyID)p->Recv_uint8();
	cp->cmd     = static_cast<Commands>(p->Recv_uint16());
	if (!IsValidCommand(cp->cmd))               return "invalid command";
	if (GetCommandFlags(cp->cmd) & CMD_OFFLINE) return "single-player only command";
	cp->err_msg = p->Recv_uint16();
	cp->tile    = p->Recv_uint32();
	cp->data    = _cmd_dispatch[cp->cmd].Sanitize(p->Recv_buffer());

	byte callback = p->Recv_uint8();
	if (callback >= lengthof(_callback_table))  return "invalid callback";

	cp->callback = _callback_table[callback];
	return nullptr;
}

/**
 * Sends a command over the network.
 * @param p the packet to send it in.
 * @param cp the packet to actually send.
 */
void NetworkGameSocketHandler::SendCommand(Packet *p, const CommandPacket *cp)
{
	p->Send_uint8(cp->company);
	p->Send_uint16(cp->cmd);
	p->Send_uint16(cp->err_msg);
	p->Send_uint32(cp->tile);
	p->Send_buffer(cp->data);

	byte callback = 0;
	while (callback < lengthof(_callback_table) && _callback_table[callback] != cp->callback) {
		callback++;
	}

	if (callback == lengthof(_callback_table)) {
		Debug(net, 0, "Unknown callback for command; no callback sent (command: {})", cp->cmd);
		callback = 0; // _callback_table[0] == nullptr
	}
	p->Send_uint8 (callback);
}

/**
 * Insert a client ID into the command data in a command packet.
 * @param cp Command packet to modify.
 * @param client_id Client id to insert.
 */
void NetworkReplaceCommandClientId(CommandPacket &cp, ClientID client_id)
{
	/* Unpack command parameters. */
	auto params = EndianBufferReader::ToValue<std::tuple<TileIndex, uint32, uint32, std::string>>(cp.data);

	/* Insert client id. */
	std::get<2>(params) = client_id;

	/* Repack command parameters. */
	cp.data = EndianBufferWriter<CommandDataBuffer>::FromValue(params);
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

template <Commands Tcmd>
void UnpackNetworkCommand(const CommandPacket *cp)
{
	auto args = EndianBufferReader::ToValue<typename CommandTraits<Tcmd>::Args>(cp->data);
	std::apply(&InjectNetworkCommand, std::tuple_cat(std::make_tuple(Tcmd, cp->err_msg, cp->callback, cp->my_cmd), args));
}
