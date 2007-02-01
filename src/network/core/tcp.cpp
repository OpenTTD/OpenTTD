/* $Id$ */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "../../debug.h"
#include "../../openttd.h"
#include "../../variables.h"
#include "table/strings.h"
#include "../../functions.h"

#include "../network_data.h"
#include "packet.h"
#include "tcp.h"
#include "../../helpers.hpp"

/**
 * @file tcp.cpp Basic functions to receive and send TCP packets.
 */

/** Very ugly temporary hack !!! */
void NetworkTCPSocketHandler::Initialize()
{
	this->sock              = INVALID_SOCKET;

	this->index             = 0;
	this->last_frame        = 0;
	this->last_frame_server = 0;
	this->lag_test          = 0;

	this->status            = STATUS_INACTIVE;
	this->has_quit          = false;
	this->writable          = false;

	this->packet_queue      = NULL;
	this->packet_recv       = NULL;

	this->command_queue     = NULL;
}

/**
 * Functions to help NetworkRecv_Packet/NetworkSend_Packet a bit
 *  A socket can make errors. When that happens this handles what to do.
 * For clients: close connection and drop back to main-menu
 * For servers: close connection and that is it
 * @return the new status
 * TODO: needs to be splitted when using client and server socket packets
 */
NetworkRecvStatus NetworkTCPSocketHandler::CloseConnection()
{
	NetworkCloseClient(this);

	/* Clients drop back to the main menu */
	if (!_network_server && _networking) {
		_switch_mode = SM_MENU;
		_networking = false;
		_switch_mode_errorstr = STR_NETWORK_ERR_LOSTCONNECTION;

		return NETWORK_RECV_STATUS_CONN_LOST;
	}

	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * This function puts the packet in the send-queue and it is send as
 * soon as possible. This is the next tick, or maybe one tick later
 * if the OS-network-buffer is full)
 * @param packet the packet to send
 * @param cs     the client to send to
 */
void NetworkSend_Packet(Packet *packet, NetworkTCPSocketHandler *cs)
{
	Packet *p;
	assert(packet != NULL);

	packet->PrepareToSend();

	/* Locate last packet buffered for the client */
	p = cs->packet_queue;
	if (p == NULL) {
		/* No packets yet */
		cs->packet_queue = packet;
	} else {
		/* Skip to the last packet */
		while (p->next != NULL) p = p->next;
		p->next = packet;
	}
}

/**
 * Sends all the buffered packets out for this client. It stops when:
 *   1) all packets are send (queue is empty)
 *   2) the OS reports back that it can not send any more
 *      data right now (full network-buffer, it happens ;))
 *   3) sending took too long
 * @param cs the client to send the packets for
 */
bool NetworkSend_Packets(NetworkTCPSocketHandler *cs)
{
	ssize_t res;
	Packet *p;

	/* We can not write to this socket!! */
	if (!cs->writable) return false;
	if (!cs->IsConnected()) return false;

	p = cs->packet_queue;
	while (p != NULL) {
		res = send(cs->sock, (const char*)p->buffer + p->pos, p->size - p->pos, 0);
		if (res == -1) {
			int err = GET_LAST_ERROR();
			if (err != EWOULDBLOCK) {
				/* Something went wrong.. close client! */
				DEBUG(net, 0, "send failed with error %d", err);
				cs->CloseConnection();
				return false;
			}
			return true;
		}
		if (res == 0) {
			/* Client/server has left us :( */
			cs->CloseConnection();
			return false;
		}

		p->pos += res;

		/* Is this packet sent? */
		if (p->pos == p->size) {
			/* Go to the next packet */
			cs->packet_queue = p->next;
			delete p;
			p = cs->packet_queue;
		} else {
			return true;
		}
	}

	return true;
}

/**
 * Receives a packet for the given client
 * @param cs     the client to (try to) receive a packet for
 * @param status the variable to store the status into
 * @return the received packet (or NULL when it didn't receive one)
 */
Packet *NetworkRecv_Packet(NetworkTCPSocketHandler *cs, NetworkRecvStatus *status)
{
	ssize_t res;
	Packet *p;

	*status = NETWORK_RECV_STATUS_OKAY;

	if (!cs->IsConnected()) return NULL;

	if (cs->packet_recv == NULL) {
		cs->packet_recv = new Packet(cs);
		if (cs->packet_recv == NULL) error("Failed to allocate packet");
	}

	p = cs->packet_recv;

	/* Read packet size */
	if (p->pos < sizeof(PacketSize)) {
		while (p->pos < sizeof(PacketSize)) {
		/* Read the size of the packet */
			res = recv(cs->sock, (char*)p->buffer + p->pos, sizeof(PacketSize) - p->pos, 0);
			if (res == -1) {
				int err = GET_LAST_ERROR();
				if (err != EWOULDBLOCK) {
					/* Something went wrong... (104 is connection reset by peer) */
					if (err != 104) DEBUG(net, 0, "recv failed with error %d", err);
					*status = cs->CloseConnection();
					return NULL;
				}
				/* Connection would block, so stop for now */
				return NULL;
			}
			if (res == 0) {
				/* Client/server has left */
				*status = cs->CloseConnection();
				return NULL;
			}
			p->pos += res;
		}

		/* Read the packet size from the received packet */
		p->ReadRawPacketSize();

		if (p->size > SEND_MTU) {
			*status = cs->CloseConnection();
			return NULL;
		}
	}

	/* Read rest of packet */
	while (p->pos < p->size) {
		res = recv(cs->sock, (char*)p->buffer + p->pos, p->size - p->pos, 0);
		if (res == -1) {
			int err = GET_LAST_ERROR();
			if (err != EWOULDBLOCK) {
				/* Something went wrong... (104 is connection reset by peer) */
				if (err != 104) DEBUG(net, 0, "recv failed with error %d", err);
				*status = cs->CloseConnection();
				return NULL;
			}
			/* Connection would block */
			return NULL;
		}
		if (res == 0) {
			/* Client/server has left */
			*status = cs->CloseConnection();
			return NULL;
		}

		p->pos += res;
	}

	/* Prepare for receiving a new packet */
	cs->packet_recv = NULL;

	p->PrepareToRead();
	return p;
}

#endif /* ENABLE_NETWORK */
