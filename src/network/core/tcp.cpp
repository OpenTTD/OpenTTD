/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp.cpp Basic functions to receive and send TCP packets.
 */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "../../debug.h"

#include "tcp.h"

NetworkTCPSocketHandler::NetworkTCPSocketHandler(SOCKET s) :
		NetworkSocketHandler(),
		packet_queue(NULL), packet_recv(NULL),
		sock(s), writable(false)
{
}

NetworkTCPSocketHandler::~NetworkTCPSocketHandler()
{
	this->CloseConnection();

	if (this->sock != INVALID_SOCKET) closesocket(this->sock);
	this->sock = INVALID_SOCKET;
}

NetworkRecvStatus NetworkTCPSocketHandler::CloseConnection(bool error)
{
	this->writable = false;
	NetworkSocketHandler::CloseConnection(error);

	/* Free all pending and partially received packets */
	while (this->packet_queue != NULL) {
		Packet *p = this->packet_queue->next;
		delete this->packet_queue;
		this->packet_queue = p;
	}
	delete this->packet_recv;
	this->packet_recv = NULL;

	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * This function puts the packet in the send-queue and it is send as
 * soon as possible. This is the next tick, or maybe one tick later
 * if the OS-network-buffer is full)
 * @param packet the packet to send
 */
void NetworkTCPSocketHandler::SendPacket(Packet *packet)
{
	Packet *p;
	assert(packet != NULL);

	packet->PrepareToSend();

	/* Reallocate the packet as in 99+% of the times we send at most 25 bytes and
	 * keeping the other 1400+ bytes wastes memory, especially when someone tries
	 * to do a denial of service attack! */
	packet->buffer = ReallocT(packet->buffer, packet->size);

	/* Locate last packet buffered for the client */
	p = this->packet_queue;
	if (p == NULL) {
		/* No packets yet */
		this->packet_queue = packet;
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
 * @param closing_down Whether we are closing down the connection.
 * @return \c true if a (part of a) packet could be sent and
 *         the connection is not closed yet.
 */
SendPacketsState NetworkTCPSocketHandler::SendPackets(bool closing_down)
{
	ssize_t res;
	Packet *p;

	/* We can not write to this socket!! */
	if (!this->writable) return SPS_NONE_SENT;
	if (!this->IsConnected()) return SPS_CLOSED;

	p = this->packet_queue;
	while (p != NULL) {
		res = send(this->sock, (const char*)p->buffer + p->pos, p->size - p->pos, 0);
		if (res == -1) {
			int err = GET_LAST_ERROR();
			if (err != EWOULDBLOCK) {
				/* Something went wrong.. close client! */
				if (!closing_down) {
					DEBUG(net, 0, "send failed with error %d", err);
					this->CloseConnection();
				}
				return SPS_CLOSED;
			}
			return SPS_PARTLY_SENT;
		}
		if (res == 0) {
			/* Client/server has left us :( */
			if (!closing_down) this->CloseConnection();
			return SPS_CLOSED;
		}

		p->pos += res;

		/* Is this packet sent? */
		if (p->pos == p->size) {
			/* Go to the next packet */
			this->packet_queue = p->next;
			delete p;
			p = this->packet_queue;
		} else {
			return SPS_PARTLY_SENT;
		}
	}

	return SPS_ALL_SENT;
}

/**
 * Receives a packet for the given client
 * @param status the variable to store the status into
 * @return the received packet (or NULL when it didn't receive one)
 */
Packet *NetworkTCPSocketHandler::ReceivePacket()
{
	ssize_t res;

	if (!this->IsConnected()) return NULL;

	if (this->packet_recv == NULL) {
		this->packet_recv = new Packet(this);
	}

	Packet *p = this->packet_recv;

	/* Read packet size */
	if (p->pos < sizeof(PacketSize)) {
		while (p->pos < sizeof(PacketSize)) {
		/* Read the size of the packet */
			res = recv(this->sock, (char*)p->buffer + p->pos, sizeof(PacketSize) - p->pos, 0);
			if (res == -1) {
				int err = GET_LAST_ERROR();
				if (err != EWOULDBLOCK) {
					/* Something went wrong... (104 is connection reset by peer) */
					if (err != 104) DEBUG(net, 0, "recv failed with error %d", err);
					this->CloseConnection();
					return NULL;
				}
				/* Connection would block, so stop for now */
				return NULL;
			}
			if (res == 0) {
				/* Client/server has left */
				this->CloseConnection();
				return NULL;
			}
			p->pos += res;
		}

		/* Read the packet size from the received packet */
		p->ReadRawPacketSize();

		if (p->size > SEND_MTU) {
			this->CloseConnection();
			return NULL;
		}
	}

	/* Read rest of packet */
	while (p->pos < p->size) {
		res = recv(this->sock, (char*)p->buffer + p->pos, p->size - p->pos, 0);
		if (res == -1) {
			int err = GET_LAST_ERROR();
			if (err != EWOULDBLOCK) {
				/* Something went wrong... (104 is connection reset by peer) */
				if (err != 104) DEBUG(net, 0, "recv failed with error %d", err);
				this->CloseConnection();
				return NULL;
			}
			/* Connection would block */
			return NULL;
		}
		if (res == 0) {
			/* Client/server has left */
			this->CloseConnection();
			return NULL;
		}

		p->pos += res;
	}

	/* Prepare for receiving a new packet */
	this->packet_recv = NULL;

	p->PrepareToRead();
	return p;
}

/**
 * Check whether this socket can send or receive something.
 * @return \c true when there is something to receive.
 * @note Sets #writeable if more data can be sent.
 */
bool NetworkTCPSocketHandler::CanSendReceive()
{
	fd_set read_fd, write_fd;
	struct timeval tv;

	FD_ZERO(&read_fd);
	FD_ZERO(&write_fd);

	FD_SET(this->sock, &read_fd);
	FD_SET(this->sock, &write_fd);

	tv.tv_sec = tv.tv_usec = 0; // don't block at all.
#if !defined(__MORPHOS__) && !defined(__AMIGA__)
	select(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv);
#else
	WaitSelect(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv, NULL);
#endif

	this->writable = !!FD_ISSET(this->sock, &write_fd);
	return FD_ISSET(this->sock, &read_fd) != 0;
}

#endif /* ENABLE_NETWORK */
