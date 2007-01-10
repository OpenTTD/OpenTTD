/* $Id$ */

#ifdef ENABLE_NETWORK

#include "../stdafx.h"
#include "../debug.h"
#include "network_data.h"
#include "../string.h"
#include "network_client.h"
#include "../command.h"
#include "../callback_table.h"
#include "../helpers.hpp"

// Add a command to the local command queue
void NetworkAddCommandQueue(NetworkClientState *cs, CommandPacket *cp)
{
	CommandPacket* new_cp;
	MallocT(&new_cp, 1);

	*new_cp = *cp;

	if (cs->command_queue == NULL) {
		cs->command_queue = new_cp;
	} else {
		CommandPacket *c = cs->command_queue;
		while (c->next != NULL) c = c->next;
		c->next = new_cp;
	}
}

// Prepare a DoCommand to be send over the network
void NetworkSend_Command(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, CommandCallback *callback)
{
	CommandPacket *c;
	MallocT(&c, 1);
	byte temp_callback;

	c->player = _local_player;
	c->next = NULL;
	c->tile = tile;
	c->p1 = p1;
	c->p2 = p2;
	c->cmd = cmd;
	c->callback = 0;

	temp_callback = 0;

	while (temp_callback < _callback_table_count && _callback_table[temp_callback] != callback)
		temp_callback++;
	if (temp_callback == _callback_table_count) {
		DEBUG(net, 0, "Unknown callback. (Pointer: %p) No callback sent", callback);
		temp_callback = 0; /* _callback_table[0] == NULL */
	}

	if (_network_server) {
		// We are the server, so set the command to be executed next possible frame
		c->frame = _frame_counter_max + 1;
	} else {
		c->frame = 0; // The client can't tell which frame, so just make it 0
	}

	ttd_strlcpy(c->text, (_cmd_text != NULL) ? _cmd_text : "", lengthof(c->text));

	if (_network_server) {
		// If we are the server, we queue the command in our 'special' queue.
		//   In theory, we could execute the command right away, but then the
		//   client on the server can do everything 1 tick faster than others.
		//   So to keep the game fair, we delay the command with 1 tick
		//   which gives about the same speed as most clients.
		NetworkClientState *cs;

		// And we queue it for delivery to the clients
		FOR_ALL_CLIENTS(cs) {
			if (cs->status > STATUS_AUTH) NetworkAddCommandQueue(cs, c);
		}

		// Only the server gets the callback, because clients should not get them
		c->callback = temp_callback;
		if (_local_command_queue == NULL) {
			_local_command_queue = c;
		} else {
			// Find last packet
			CommandPacket *cp = _local_command_queue;
			while (cp->next != NULL) cp = cp->next;
			cp->next = c;
		}

		return;
	}

	// Clients send their command to the server and forget all about the packet
	c->callback = temp_callback;
	SEND_COMMAND(PACKET_CLIENT_COMMAND)(c);
}

// Execute a DoCommand we received from the network
void NetworkExecuteCommand(CommandPacket *cp)
{
	_current_player = cp->player;
	_cmd_text = cp->text;
	/* cp->callback is unsigned. so we don't need to do lower bounds checking. */
	if (cp->callback > _callback_table_count) {
		DEBUG(net, 0, "Received out-of-bounds callback (%d)", cp->callback);
		cp->callback = 0;
	}
	DoCommandP(cp->tile, cp->p1, cp->p2, _callback_table[cp->callback], cp->cmd | CMD_NETWORK_COMMAND);
}

#endif /* ENABLE_NETWORK */
