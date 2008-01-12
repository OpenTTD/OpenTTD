/* $Id$ */

#ifdef ENABLE_NETWORK

#include "../stdafx.h"
#include "../debug.h"
#include "network_data.h"
#include "network_client.h"
#include "../command_func.h"
#include "../callback_table.h"
#include "../core/alloc_func.hpp"
#include "../string_func.h"
#include "../date_func.h"
#include "../player_func.h"

// Add a command to the local command queue
void NetworkAddCommandQueue(NetworkTCPSocketHandler *cs, CommandPacket *cp)
{
	CommandPacket* new_cp = MallocT<CommandPacket>(1);

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
	CommandPacket c;

	c.player = _local_player;
	c.next   = NULL;
	c.tile   = tile;
	c.p1     = p1;
	c.p2     = p2;
	c.cmd    = cmd;

	c.callback = 0;
	while (c.callback < _callback_table_count && _callback_table[c.callback] != callback) {
		c.callback++;
	}

	if (c.callback == _callback_table_count) {
		DEBUG(net, 0, "Unknown callback. (Pointer: %p) No callback sent", callback);
		c.callback = 0; // _callback_table[0] == NULL
	}

	ttd_strlcpy(c.text, (_cmd_text != NULL) ? _cmd_text : "", lengthof(c.text));

	if (_network_server) {
		/* If we are the server, we queue the command in our 'special' queue.
		 *   In theory, we could execute the command right away, but then the
		 *   client on the server can do everything 1 tick faster than others.
		 *   So to keep the game fair, we delay the command with 1 tick
		 *   which gives about the same speed as most clients.
		 */
		c.frame = _frame_counter_max + 1;

		CommandPacket *new_cp = MallocT<CommandPacket>(1);
		*new_cp = c;
		new_cp->my_cmd = true;
		if (_local_command_queue == NULL) {
			_local_command_queue = new_cp;
		} else {
			/* Find last packet */
			CommandPacket *cp = _local_command_queue;
			while (cp->next != NULL) cp = cp->next;
			cp->next = new_cp;
		}

		/* Only the local client (in this case, the server) gets the callback */
		c.callback = 0;
		/* And we queue it for delivery to the clients */
		NetworkTCPSocketHandler *cs;
		FOR_ALL_CLIENTS(cs) {
			if (cs->status > STATUS_AUTH) NetworkAddCommandQueue(cs, &c);
		}
		return;
	}

	c.frame = 0; // The client can't tell which frame, so just make it 0

	/* Clients send their command to the server and forget all about the packet */
	SEND_COMMAND(PACKET_CLIENT_COMMAND)(&c);
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

	DebugDumpCommands("ddc:cmd:%d;%d;%d;%d;%d;%d;%d;%s\n", _date, _date_fract, (int)cp->player, cp->tile, cp->p1, cp->p2, cp->cmd, cp->text);

	DoCommandP(cp->tile, cp->p1, cp->p2, _callback_table[cp->callback], cp->cmd | CMD_NETWORK_COMMAND, cp->my_cmd);
}

#endif /* ENABLE_NETWORK */
