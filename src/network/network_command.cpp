/* $Id$ */

/** @file network_command.cpp Command handling over network connections. */

#ifdef ENABLE_NETWORK

#include "../stdafx.h"
#include "../debug.h"
#include "network_internal.h"
#include "network_client.h"
#include "../command_func.h"
#include "../callback_table.h"
#include "../core/alloc_func.hpp"
#include "../string_func.h"
#include "../company_func.h"

/** Local queue of packets */
static CommandPacket *_local_command_queue = NULL;

/**
 * Add a command to the local or client socket command queue,
 * based on the socket.
 * @param cp the command packet to add
 * @param cs the socket to send to (NULL = locally)
 */
void NetworkAddCommandQueue(CommandPacket cp, NetworkClientSocket *cs)
{
	CommandPacket *new_cp = MallocT<CommandPacket>(1);
	*new_cp = cp;

	CommandPacket **begin = (cs == NULL ? &_local_command_queue : &cs->command_queue);

	if (*begin == NULL) {
		*begin = new_cp;
	} else {
		CommandPacket *c = *begin;
		while (c->next != NULL) c = c->next;
		c->next = new_cp;
	}
}

/**
 * Prepare a DoCommand to be send over the network
 * @param tile The tile to perform a command on (see #CommandProc)
 * @param p1 Additional data for the command (see #CommandProc)
 * @param p2 Additional data for the command (see #CommandProc)
 * @param cmd The command to execute (a CMD_* value)
 * @param callback A callback function to call after the command is finished
 * @param text The text to pass
 */
void NetworkSend_Command(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, CommandCallback *callback, const char *text)
{
	assert((cmd & CMD_FLAGS_MASK) == 0);

	CommandPacket c;
	c.company  = _local_company;
	c.next     = NULL;
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

		NetworkAddCommandQueue(c);

		/* Only the local client (in this case, the server) gets the callback */
		c.callback = 0;
		/* And we queue it for delivery to the clients */
		NetworkClientSocket *cs;
		FOR_ALL_CLIENT_SOCKETS(cs) {
			if (cs->status > STATUS_MAP_WAIT) NetworkAddCommandQueue(c, cs);
		}
		return;
	}

	c.frame = 0; // The client can't tell which frame, so just make it 0

	/* Clients send their command to the server and forget all about the packet */
	SEND_COMMAND(PACKET_CLIENT_COMMAND)(&c);
}

/**
 * Execute all commands on the local command queue that ought to be executed this frame.
 */
void NetworkExecuteLocalCommandQueue()
{
	while (_local_command_queue != NULL) {

		/* The queue is always in order, which means
		 * that the first element will be executed first. */
		if (_frame_counter < _local_command_queue->frame) break;

		if (_frame_counter > _local_command_queue->frame) {
			/* If we reach here, it means for whatever reason, we've already executed
			 * past the command we need to execute. */
			error("[net] Trying to execute a packet in the past!");
		}

		CommandPacket *cp = _local_command_queue;

		/* We can execute this command */
		_current_company = cp->company;
		cp->cmd |= CMD_NETWORK_COMMAND;
		DoCommandP(cp, cp->my_cmd);

		_local_command_queue = _local_command_queue->next;
		free(cp);
	}
}

/**
 * Free the local command queue.
 */
void NetworkFreeLocalCommandQueue()
{
	/* Free all queued commands */
	while (_local_command_queue != NULL) {
		CommandPacket *p = _local_command_queue;
		_local_command_queue = _local_command_queue->next;
		free(p);
	}
}

/**
 * Receives a command from the network.
 * @param p the packet to read from.
 * @param cp the struct to write the data to.
 * @return an error message. When NULL there has been no error.
 */
const char *NetworkClientSocket::Recv_Command(Packet *p, CommandPacket *cp)
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
	if (callback > _callback_table_count)       return "invalid callback";

	cp->callback = _callback_table[callback];
	return NULL;
}

/**
 * Sends a command over the network.
 * @param p the packet to send it in.
 * @param cp the packet to actually send.
 */
void NetworkClientSocket::Send_Command(Packet *p, const CommandPacket *cp)
{
	p->Send_uint8 (cp->company);
	p->Send_uint32(cp->cmd);
	p->Send_uint32(cp->p1);
	p->Send_uint32(cp->p2);
	p->Send_uint32(cp->tile);
	p->Send_string(cp->text);

	byte callback = 0;
	while (callback < _callback_table_count && _callback_table[callback] != cp->callback) {
		callback++;
	}

	if (callback == _callback_table_count) {
		DEBUG(net, 0, "Unknown callback. (Pointer: %p) No callback sent", callback);
		callback = 0; // _callback_table[0] == NULL
	}
	p->Send_uint8 (callback);
}

#endif /* ENABLE_NETWORK */
