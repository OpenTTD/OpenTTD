/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_command.cpp Command handling over network connections. */

#ifdef ENABLE_NETWORK

#include "../stdafx.h"
#include "network_admin.h"
#include "network_client.h"
#include "network_server.h"
#include "../command_func.h"
#include "../company_func.h"
#include "../settings_type.h"

/** Table with all the callbacks we'll use for conversion*/
static CommandCallback * const _callback_table[] = {
	/* 0x00 */ NULL,
	/* 0x01 */ CcBuildPrimaryVehicle,
	/* 0x02 */ CcBuildAirport,
	/* 0x03 */ CcBuildBridge,
	/* 0x04 */ CcBuildCanal,
	/* 0x05 */ CcBuildDocks,
	/* 0x06 */ CcFoundTown,
	/* 0x07 */ CcBuildRoadTunnel,
	/* 0x08 */ CcBuildRailTunnel,
	/* 0x09 */ CcBuildWagon,
	/* 0x0A */ CcRoadDepot,
	/* 0x0B */ CcRailDepot,
	/* 0x0C */ CcPlaceSign,
	/* 0x0D */ CcPlaySound10,
	/* 0x0E */ CcPlaySound1D,
	/* 0x0F */ CcPlaySound1E,
	/* 0x10 */ CcStation,
	/* 0x11 */ CcTerraform,
#ifdef ENABLE_AI
	/* 0x12 */ CcAI,
#else
	/* 0x12 */ NULL,
#endif /* ENABLE_AI */
	/* 0x13 */ CcCloneVehicle,
	/* 0x14 */ CcGiveMoney,
	/* 0x15 */ CcCreateGroup,
	/* 0x16 */ CcFoundRandomTown,
	/* 0x17 */ CcRoadStop,
	/* 0x18 */ CcBuildIndustry,
	/* 0x19 */ CcStartStopVehicle,
};

/**
 * Append a CommandPacket at the end of the queue.
 * @param p The packet to append to the queue.
 * @note A new instance of the CommandPacket will be made.
 */
void CommandQueue::Append(CommandPacket *p)
{
	CommandPacket *add = MallocT<CommandPacket>(1);
	*add = *p;
	add->next = NULL;
	if (this->first == NULL) {
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
	if (ignore_paused && _pause_mode != PM_UNPAUSED) {
		while (ret != NULL && !IsCommandAllowedWhilePaused(ret->cmd)) {
			prev = &ret->next;
			ret = ret->next;
		}
	}
	if (ret != NULL) {
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

	for (CommandPacket *p = this->first; p != NULL; p = p->next) {
		if (IsCommandAllowedWhilePaused(p->cmd)) return p;
	}
	return NULL;
}

/** Free everything that is in the queue. */
void CommandQueue::Free()
{
	CommandPacket *cp;
	while ((cp = this->Pop()) != NULL) {
		free(cp);
	}
	assert(this->count == 0);
}

/** Local queue of packets waiting for handling. */
static CommandQueue _local_wait_queue;
/** Local queue of packets waiting for execution. */
static CommandQueue _local_execution_queue;

/**
 * Prepare a DoCommand to be send over the network
 * @param tile The tile to perform a command on (see #CommandProc)
 * @param p1 Additional data for the command (see #CommandProc)
 * @param p2 Additional data for the command (see #CommandProc)
 * @param cmd The command to execute (a CMD_* value)
 * @param callback A callback function to call after the command is finished
 * @param text The text to pass
 * @param company The company that wants to send the command
 */
void NetworkSendCommand(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, CommandCallback *callback, const char *text, CompanyID company)
{
	assert((cmd & CMD_FLAGS_MASK) == 0);

	CommandPacket c;
	c.company  = company;
	c.tile     = tile;
	c.p1       = p1;
	c.p2       = p2;
	c.cmd      = cmd;
	c.callback = callback;

	strecpy(c.text, (text != NULL) ? text : "", lastof(c.text));

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
	for (CommandPacket *p = _local_execution_queue.Peek(); p != NULL; p = p->next) {
		CommandPacket c = *p;
		c.callback = 0;
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
	while ((cp = queue.Peek()) != NULL) {
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
		cp->cmd |= CMD_NETWORK_COMMAND;
		DoCommandP(cp, cp->my_cmd);

		queue.Pop();
		free(cp);
	}

	/* Local company may have changed, so we should not restore the old value */
	_current_company = _local_company;
}

/**
 * Free the local command queue.
 */
void NetworkFreeLocalCommandQueue()
{
	_local_execution_queue.Free();
}

/**
 * "Send" a particular CommandPacket to all clients.
 * @param cp    The command that has to be distributed.
 * @param owner The client that owns the command,
 */
static void DistributeCommandPacket(CommandPacket cp, const NetworkClientSocket *owner)
{
	CommandCallback *callback = cp.callback;
	cp.frame = _frame_counter_max + 1;

	NetworkClientSocket *cs;
	FOR_ALL_CLIENT_SOCKETS(cs) {
		if (cs->status >= NetworkClientSocket::STATUS_MAP) {
			/* Callbacks are only send back to the client who sent them in the
			 *  first place. This filters that out. */
			cp.callback = (cs != owner) ? NULL : callback;
			cp.my_cmd = (cs == owner);
			cs->outgoing_queue.Append(&cp);
		}
	}

	cp.callback = (cs != owner) ? NULL : callback;
	cp.my_cmd = (cs == owner);
	_local_execution_queue.Append(&cp);
}

/**
 * "Send" a particular CommandQueue to all clients.
 * @param queue The queue of commands that has to be distributed.
 * @param owner The client that owns the commands,
 */
static void DistributeQueue(CommandQueue *queue, const NetworkClientSocket *owner)
{
	int to_go = _settings_client.network.commands_per_frame;

	CommandPacket *cp;
	while (--to_go >= 0 && (cp = queue->Pop(true)) != NULL) {
		DistributeCommandPacket(*cp, owner);
		NetworkAdminCmdLogging(owner, cp);
		free(cp);
	}
}

void NetworkDistributeCommands()
{
	/* First send the server's commands. */
	DistributeQueue(&_local_wait_queue, NULL);

	/* Then send the queues of the others. */
	NetworkClientSocket *cs;
	FOR_ALL_CLIENT_SOCKETS(cs) {
		DistributeQueue(&cs->incoming_queue, cs);
	}
}

/**
 * Receives a command from the network.
 * @param p the packet to read from.
 * @param cp the struct to write the data to.
 * @return an error message. When NULL there has been no error.
 */
const char *NetworkGameSocketHandler::ReceiveCommand(Packet *p, CommandPacket *cp)
{
	cp->company = (CompanyID)p->Recv_uint8();
	cp->cmd     = p->Recv_uint32();
	cp->p1      = p->Recv_uint32();
	cp->p2      = p->Recv_uint32();
	cp->tile    = p->Recv_uint32();
	p->Recv_string(cp->text, lengthof(cp->text));

	byte callback = p->Recv_uint8();

	if (!IsValidCommand(cp->cmd))               return "invalid command";
	if (GetCommandFlags(cp->cmd) & CMD_OFFLINE) return "offline only command";
	if ((cp->cmd & CMD_FLAGS_MASK) != 0)        return "invalid command flag";
	if (callback > lengthof(_callback_table))   return "invalid callback";

	cp->callback = _callback_table[callback];
	return NULL;
}

/**
 * Sends a command over the network.
 * @param p the packet to send it in.
 * @param cp the packet to actually send.
 */
void NetworkGameSocketHandler::SendCommand(Packet *p, const CommandPacket *cp)
{
	p->Send_uint8 (cp->company);
	p->Send_uint32(cp->cmd);
	p->Send_uint32(cp->p1);
	p->Send_uint32(cp->p2);
	p->Send_uint32(cp->tile);
	p->Send_string(cp->text);

	byte callback = 0;
	while (callback < lengthof(_callback_table) && _callback_table[callback] != cp->callback) {
		callback++;
	}

	if (callback == lengthof(_callback_table)) {
		DEBUG(net, 0, "Unknown callback. (Pointer: %p) No callback sent", cp->callback);
		callback = 0; // _callback_table[0] == NULL
	}
	p->Send_uint8 (callback);
}

#endif /* ENABLE_NETWORK */
