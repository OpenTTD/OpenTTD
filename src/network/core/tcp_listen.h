/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_listen.h Basic functions to listen for TCP connections.
 */

#ifndef NETWORK_CORE_TCP_LISTEN_H
#define NETWORK_CORE_TCP_LISTEN_H

#include "tcp.h"
#include "../network.h"
#include "../../core/pool_type.hpp"
#include "../../debug.h"
#include "table/strings.h"

#ifdef ENABLE_NETWORK

/**
 * Template for TCP listeners.
 * @param Tsocket      The class we create sockets for.
 * @param Tfull_packet The packet type to return when we don't allow more sockets.
 * @param Tban_packet  The packet type to return when the client is banned.
 */
template <class Tsocket, PacketType Tfull_packet, PacketType Tban_packet>
class TCPListenHandler {
	/** List of sockets we listen on. */
	static SocketList sockets;

public:
	/**
	 * Accepts clients from the sockets.
	 * @param ls Socket to accept clients from.
	 */
	static void AcceptClient(SOCKET ls)
	{
		for (;;) {
			struct sockaddr_storage sin;
			memset(&sin, 0, sizeof(sin));
			socklen_t sin_len = sizeof(sin);
			SOCKET s = accept(ls, (struct sockaddr*)&sin, &sin_len);
			if (s == INVALID_SOCKET) return;

			SetNonBlocking(s); // XXX error handling?

			NetworkAddress address(sin, sin_len);
			DEBUG(net, 1, "[%s] Client connected from %s on frame %d", Tsocket::GetName(), address.GetHostname(), _frame_counter);

			SetNoDelay(s); // XXX error handling?

			/* Check if the client is banned */
			bool banned = false;
			for (char **iter = _network_ban_list.Begin(); iter != _network_ban_list.End(); iter++) {
				banned = address.IsInNetmask(*iter);
				if (banned) {
					Packet p(Tban_packet);
					p.PrepareToSend();

					DEBUG(net, 1, "[%s] Banned ip tried to join (%s), refused", Tsocket::GetName(), *iter);

					send(s, (const char*)p.buffer, p.size, 0);
					closesocket(s);
					break;
				}
			}
			/* If this client is banned, continue with next client */
			if (banned) continue;

			/* Can we handle a new client? */
			if (!Tsocket::AllowConnection()) {
				/* no more clients allowed?
				 * Send to the client that we are full! */
				Packet p(Tfull_packet);
				p.PrepareToSend();

				send(s, (const char*)p.buffer, p.size, 0);
				closesocket(s);

				continue;
			}

			Tsocket::AcceptConnection(s, address);
		}
	}

	/**
	 * Handle the receiving of packets.
	 * @return true if everything went okay.
	 */
	static bool Receive()
	{
		fd_set read_fd, write_fd;
		struct timeval tv;

		FD_ZERO(&read_fd);
		FD_ZERO(&write_fd);


		Tsocket *cs;
		FOR_ALL_ITEMS_FROM(Tsocket, idx, cs, 0) {
			FD_SET(cs->sock, &read_fd);
			FD_SET(cs->sock, &write_fd);
		}

		/* take care of listener port */
		for (SocketList::iterator s = sockets.Begin(); s != sockets.End(); s++) {
			FD_SET(s->second, &read_fd);
		}

		tv.tv_sec = tv.tv_usec = 0; // don't block at all.
#if !defined(__MORPHOS__) && !defined(__AMIGA__)
		select(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv);
#else
		WaitSelect(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv, NULL);
#endif

		/* accept clients.. */
		for (SocketList::iterator s = sockets.Begin(); s != sockets.End(); s++) {
			if (FD_ISSET(s->second, &read_fd)) AcceptClient(s->second);
		}

		/* read stuff from clients */
		FOR_ALL_ITEMS_FROM(Tsocket, idx, cs, 0) {
			cs->writable = !!FD_ISSET(cs->sock, &write_fd);
			if (FD_ISSET(cs->sock, &read_fd)) {
				cs->ReceivePackets();
			}
		}
		return _networking;
	}

	/**
	 * Listen on a particular port.
	 * @param port The port to listen on.
	 * @return true if listening succeeded.
	 */
	static bool Listen(uint16 port)
	{
		assert(sockets.Length() == 0);

		NetworkAddressList addresses;
		GetBindAddresses(&addresses, port);

		for (NetworkAddress *address = addresses.Begin(); address != addresses.End(); address++) {
			address->Listen(SOCK_STREAM, &sockets);
		}

		if (sockets.Length() == 0) {
			DEBUG(net, 0, "[server] could not start network: could not create listening socket");
			NetworkError(STR_NETWORK_ERROR_SERVER_START);
			return false;
		}

		return true;
	}

	/** Close the sockets we're listening on. */
	static void CloseListeners()
	{
		for (SocketList::iterator s = sockets.Begin(); s != sockets.End(); s++) {
			closesocket(s->second);
		}
		sockets.Clear();
		DEBUG(net, 1, "[%s] closed listeners", Tsocket::GetName());
	}
};

template <class Tsocket, PacketType Tfull_packet, PacketType Tban_packet> SocketList TCPListenHandler<Tsocket, Tfull_packet, Tban_packet>::sockets;

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_TCP_LISTEN_H */
