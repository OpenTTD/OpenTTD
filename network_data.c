/* $Id$ */

#ifdef ENABLE_NETWORK

#include "stdafx.h"
#include "debug.h"
#include "network_data.h"
#include "functions.h"
#include "string.h"
#include "table/strings.h"
#include "network_client.h"
#include "command.h"
#include "callback_table.h"
#include "variables.h"

// This files handles the send/receive of all packets

// Create a packet for sending
Packet *NetworkSend_Init(PacketType type)
{
	Packet *packet = malloc(sizeof(Packet));
	// An error is inplace here, because it simply means we ran out of memory.
	if (packet == NULL) error("Failed to allocate Packet");

	// Skip the size so we can write that in before sending the packet
	packet->size = sizeof(packet->size);
	packet->buffer[packet->size++] = type;
	packet->pos = 0;

	return packet;
}

// The next couple of functions make sure we can send
//  uint8, uint16, uint32 and uint64 endian-safe
//  over the network. The order it uses is:
//
//  1 2 3 4
//

void NetworkSend_uint8(Packet *packet, uint8 data)
{
	assert(packet->size < sizeof(packet->buffer) - sizeof(data));
	packet->buffer[packet->size++] = data;
}

void NetworkSend_uint16(Packet *packet, uint16 data)
{
	assert(packet->size < sizeof(packet->buffer) - sizeof(data));
	packet->buffer[packet->size++] = GB(data, 0, 8);
	packet->buffer[packet->size++] = GB(data, 8, 8);
}

void NetworkSend_uint32(Packet *packet, uint32 data)
{
	assert(packet->size < sizeof(packet->buffer) - sizeof(data));
	packet->buffer[packet->size++] = GB(data,  0, 8);
	packet->buffer[packet->size++] = GB(data,  8, 8);
	packet->buffer[packet->size++] = GB(data, 16, 8);
	packet->buffer[packet->size++] = GB(data, 24, 8);
}

void NetworkSend_uint64(Packet *packet, uint64 data)
{
	assert(packet->size < sizeof(packet->buffer) - sizeof(data));
	packet->buffer[packet->size++] = GB(data,  0, 8);
	packet->buffer[packet->size++] = GB(data,  8, 8);
	packet->buffer[packet->size++] = GB(data, 16, 8);
	packet->buffer[packet->size++] = GB(data, 24, 8);
	packet->buffer[packet->size++] = GB(data, 32, 8);
	packet->buffer[packet->size++] = GB(data, 40, 8);
	packet->buffer[packet->size++] = GB(data, 48, 8);
	packet->buffer[packet->size++] = GB(data, 56, 8);
}

// Sends a string over the network. It sends out
//  the string + '\0'. No size-byte or something.
void NetworkSend_string(Packet *packet, const char* data)
{
	assert(data != NULL);
	assert(packet->size < sizeof(packet->buffer) - strlen(data) - 1);
	while ((packet->buffer[packet->size++] = *data++) != '\0') {}
}

// If PacketSize changes of size, you have to change the 2 packet->size
//   lines below matching the size of packet->size/PacketSize!
// (line 'packet->buffer[0] = packet->size & 0xFF;'  and below)
assert_compile(sizeof(PacketSize) == 2);

// This function puts the packet in the send-queue and it is send
//  as soon as possible
// (that is: the next tick, or maybe one tick later if the
//   OS-network-buffer is full)
void NetworkSend_Packet(Packet *packet, NetworkClientState *cs)
{
	Packet *p;
	assert(packet != NULL);

	packet->pos = 0;
	packet->next = NULL;

	packet->buffer[0] = GB(packet->size, 0, 8);
	packet->buffer[1] = GB(packet->size, 8, 8);

	// Locate last packet buffered for the client
	p = cs->packet_queue;
	if (p == NULL) {
		// No packets yet
		cs->packet_queue = packet;
	} else {
		// Skip to the last packet
		while (p->next != NULL) p = p->next;
		p->next = packet;
	}
}

// Functions to help NetworkRecv_Packet/NetworkSend_Packet a bit
//  A socket can make errors. When that happens
//  this handles what to do.
// For clients: close connection and drop back to main-menu
// For servers: close connection and that is it
static NetworkRecvStatus CloseConnection(NetworkClientState *cs)
{
	NetworkCloseClient(cs);

	// Clients drop back to the main menu
	if (!_network_server && _networking) {
		_switch_mode = SM_MENU;
		_networking = false;
		_switch_mode_errorstr = STR_NETWORK_ERR_LOSTCONNECTION;

		return NETWORK_RECV_STATUS_CONN_LOST;
	}

	return NETWORK_RECV_STATUS_OKAY;
}

// Sends all the buffered packets out for this client
//  it stops when:
//   1) all packets are send (queue is empty)
//   2) the OS reports back that it can not send any more
//        data right now (full network-buffer, it happens ;))
//   3) sending took too long
bool NetworkSend_Packets(NetworkClientState *cs)
{
	ssize_t res;
	Packet *p;

	// We can not write to this socket!!
	if (!cs->writable) return false;
	if (cs->socket == INVALID_SOCKET) return false;

	p = cs->packet_queue;
	while (p != NULL) {
		res = send(cs->socket, p->buffer + p->pos, p->size - p->pos, 0);
		if (res == -1) {
			int err = GET_LAST_ERROR();
			if (err != EWOULDBLOCK) { // Something went wrong.. close client!
				DEBUG(net, 0, "send failed with error %d", err);
				CloseConnection(cs);
				return false;
			}
			return true;
		}
		if (res == 0) {
			// Client/server has left us :(
			CloseConnection(cs);
			return false;
		}

		p->pos += res;

		// Is this packet sent?
		if (p->pos == p->size) {
			// Go to the next packet
			cs->packet_queue = p->next;
			free(p);
			p = cs->packet_queue;
		} else {
			return true;
		}
	}

	return true;
}


// Receiving commands
// Again, the next couple of functions are endian-safe
//  see the comment around NetworkSend_uint8 for more info.
uint8 NetworkRecv_uint8(NetworkClientState *cs, Packet *packet)
{
	/* Don't allow reading from a closed socket */
	if (cs->has_quit) return 0;

	/* Check if variable is within packet-size */
	if (packet->pos + 1 > packet->size) {
		CloseConnection(cs);
		return 0;
	}

	return packet->buffer[packet->pos++];
}

uint16 NetworkRecv_uint16(NetworkClientState *cs, Packet *packet)
{
	uint16 n;

	/* Don't allow reading from a closed socket */
	if (cs->has_quit) return 0;

	/* Check if variable is within packet-size */
	if (packet->pos + 2 > packet->size) {
		CloseConnection(cs);
		return 0;
	}

	n  = (uint16)packet->buffer[packet->pos++];
	n += (uint16)packet->buffer[packet->pos++] << 8;
	return n;
}

uint32 NetworkRecv_uint32(NetworkClientState *cs, Packet *packet)
{
	uint32 n;

	/* Don't allow reading from a closed socket */
	if (cs->has_quit) return 0;

	/* Check if variable is within packet-size */
	if (packet->pos + 4 > packet->size) {
		CloseConnection(cs);
		return 0;
	}

	n  = (uint32)packet->buffer[packet->pos++];
	n += (uint32)packet->buffer[packet->pos++] << 8;
	n += (uint32)packet->buffer[packet->pos++] << 16;
	n += (uint32)packet->buffer[packet->pos++] << 24;
	return n;
}

uint64 NetworkRecv_uint64(NetworkClientState *cs, Packet *packet)
{
	uint64 n;

	/* Don't allow reading from a closed socket */
	if (cs->has_quit) return 0;

	/* Check if variable is within packet-size */
	if (packet->pos + 8 > packet->size) {
		CloseConnection(cs);
		return 0;
	}

	n  = (uint64)packet->buffer[packet->pos++];
	n += (uint64)packet->buffer[packet->pos++] << 8;
	n += (uint64)packet->buffer[packet->pos++] << 16;
	n += (uint64)packet->buffer[packet->pos++] << 24;
	n += (uint64)packet->buffer[packet->pos++] << 32;
	n += (uint64)packet->buffer[packet->pos++] << 40;
	n += (uint64)packet->buffer[packet->pos++] << 48;
	n += (uint64)packet->buffer[packet->pos++] << 56;
	return n;
}

// Reads a string till it finds a '\0' in the stream
void NetworkRecv_string(NetworkClientState *cs, Packet *p, char *buffer, size_t size)
{
	PacketSize pos;
	char *bufp = buffer;

	/* Don't allow reading from a closed socket */
	if (cs->has_quit) return;

	pos = p->pos;
	while (--size > 0 && pos < p->size && (*buffer++ = p->buffer[pos++]) != '\0') {}

	if (size == 0 || pos == p->size) {
		*buffer = '\0';
		// If size was sooner to zero then the string in the stream
		//  skip till the \0, so the packet can be read out correctly for the rest
		while (pos < p->size && p->buffer[pos] != '\0') pos++;
		pos++;
	}
	p->pos = pos;

	str_validate(bufp);
}

// If PacketSize changes of size, you have to change the 2 packet->size
//   lines below matching the size of packet->size/PacketSize!
// (the line: 'p->size = (uint16)p->buffer[0];' and below)
assert_compile(sizeof(PacketSize) == 2);

Packet *NetworkRecv_Packet(NetworkClientState *cs, NetworkRecvStatus *status)
{
	ssize_t res;
	Packet *p;

	*status = NETWORK_RECV_STATUS_OKAY;

	if (cs->socket == INVALID_SOCKET) return NULL;

	if (cs->packet_recv == NULL) {
		cs->packet_recv = malloc(sizeof(Packet));
		if (cs->packet_recv == NULL) error("Failed to allocate packet");
		// Set pos to zero!
		cs->packet_recv->pos = 0;
		cs->packet_recv->size = 0; // Can be ommited, just for safety reasons
	}

	p = cs->packet_recv;

	// Read packet size
	if (p->pos < sizeof(PacketSize)) {
		while (p->pos < sizeof(PacketSize)) {
			// Read the size of the packet
			res = recv(cs->socket, p->buffer + p->pos, sizeof(PacketSize) - p->pos, 0);
			if (res == -1) {
				int err = GET_LAST_ERROR();
				if (err != EWOULDBLOCK) {
					/* Something went wrong... (104 is connection reset by peer) */
					if (err != 104) DEBUG(net, 0, "recv failed with error %d", err);
					*status = CloseConnection(cs);
					return NULL;
				}
				// Connection would block, so stop for now
				return NULL;
			}
			if (res == 0) {
				// Client/server has left
				*status = CloseConnection(cs);
				return NULL;
			}
			p->pos += res;
		}

		p->size = (uint16)p->buffer[0];
		p->size += (uint16)p->buffer[1] << 8;

		if (p->size > SEND_MTU) {
			*status = CloseConnection(cs);
			return NULL;
		}
	}

	// Read rest of packet
	while (p->pos < p->size) {
		res = recv(cs->socket, p->buffer + p->pos, p->size - p->pos, 0);
		if (res == -1) {
			int err = GET_LAST_ERROR();
			if (err != EWOULDBLOCK) {
				/* Something went wrong... (104 is connection reset by peer) */
				if (err != 104) DEBUG(net, 0, "recv failed with error %d", err);
				*status = CloseConnection(cs);
				return NULL;
			}
			// Connection would block
			return NULL;
		}
		if (res == 0) {
			// Client/server has left
			*status = CloseConnection(cs);
			return NULL;
		}

		p->pos += res;
	}

	// We have a complete packet, return it!
	p->pos = 2;
	p->next = NULL; // Should not be needed, but who knows...

	// Prepare for receiving a new packet
	cs->packet_recv = NULL;

	return p;
}

// Add a command to the local command queue
void NetworkAddCommandQueue(NetworkClientState *cs, CommandPacket *cp)
{
	CommandPacket* new_cp = malloc(sizeof(*new_cp));

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
	CommandPacket *c = malloc(sizeof(CommandPacket));
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
