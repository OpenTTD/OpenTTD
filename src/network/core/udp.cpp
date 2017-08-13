/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file core/udp.cpp Basic functions to receive and send UDP packets.
 */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "../../date_func.h"
#include "../../debug.h"
#include "udp.h"

#include "../../safeguards.h"

/**
 * Create an UDP socket but don't listen yet.
 * @param bind the addresses to bind to.
 */
NetworkUDPSocketHandler::NetworkUDPSocketHandler(NetworkAddressList *bind)
{
	if (bind != NULL) {
		for (NetworkAddress *addr = bind->Begin(); addr != bind->End(); addr++) {
			*this->bind.Append() = *addr;
		}
	} else {
		/* As hostname NULL and port 0/NULL don't go well when
		 * resolving it we need to add an address for each of
		 * the address families we support. */
		*this->bind.Append() = NetworkAddress(NULL, 0, AF_INET);
		*this->bind.Append() = NetworkAddress(NULL, 0, AF_INET6);
	}
}


/**
 * Start listening on the given host and port.
 * @return true if at least one port is listening
 */
bool NetworkUDPSocketHandler::Listen()
{
	/* Make sure socket is closed */
	this->Close();

	for (NetworkAddress *addr = this->bind.Begin(); addr != this->bind.End(); addr++) {
		addr->Listen(SOCK_DGRAM, &this->sockets);
	}

	return this->sockets.Length() != 0;
}

/**
 * Close the given UDP socket
 */
void NetworkUDPSocketHandler::Close()
{
	for (SocketList::iterator s = this->sockets.Begin(); s != this->sockets.End(); s++) {
		closesocket(s->second);
	}
	this->sockets.Clear();
}

NetworkRecvStatus NetworkUDPSocketHandler::CloseConnection(bool error)
{
	NetworkSocketHandler::CloseConnection(error);
	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Send a packet over UDP
 * @param p    the packet to send
 * @param recv the receiver (target) of the packet
 * @param all  send the packet using all sockets that can send it
 * @param broadcast whether to send a broadcast message
 */
void NetworkUDPSocketHandler::SendPacket(Packet *p, NetworkAddress *recv, bool all, bool broadcast)
{
	if (this->sockets.Length() == 0) this->Listen();

	for (SocketList::iterator s = this->sockets.Begin(); s != this->sockets.End(); s++) {
		/* Make a local copy because if we resolve it we cannot
		 * easily unresolve it so we can resolve it later again. */
		NetworkAddress send(*recv);

		/* Not the same type */
		if (!send.IsFamily(s->first.GetAddress()->ss_family)) continue;

		p->PrepareToSend();

#ifndef BEOS_NET_SERVER /* will work around this, some day; maybe. */
		if (broadcast) {
			/* Enable broadcast */
			unsigned long val = 1;
			if (setsockopt(s->second, SOL_SOCKET, SO_BROADCAST, (char *) &val, sizeof(val)) < 0) {
				DEBUG(net, 1, "[udp] setting broadcast failed with: %i", GET_LAST_ERROR());
			}
		}
#endif

		/* Send the buffer */
		int res = sendto(s->second, (const char*)p->buffer, p->size, 0, (const struct sockaddr *)send.GetAddress(), send.GetAddressLength());
		DEBUG(net, 7, "[udp] sendto(%s)", send.GetAddressAsString());

		/* Check for any errors, but ignore it otherwise */
		if (res == -1) DEBUG(net, 1, "[udp] sendto(%s) failed with: %i", send.GetAddressAsString(), GET_LAST_ERROR());

		if (!all) break;
	}
}

/**
 * Receive a packet at UDP level
 */
void NetworkUDPSocketHandler::ReceivePackets()
{
	for (SocketList::iterator s = this->sockets.Begin(); s != this->sockets.End(); s++) {
		for (int i = 0; i < 1000; i++) { // Do not infinitely loop when DoSing with UDP
			struct sockaddr_storage client_addr;
			memset(&client_addr, 0, sizeof(client_addr));

			Packet p(this);
			socklen_t client_len = sizeof(client_addr);

			/* Try to receive anything */
			SetNonBlocking(s->second); // Some OSes seem to lose the non-blocking status of the socket
			int nbytes = recvfrom(s->second, (char*)p.buffer, SEND_MTU, 0, (struct sockaddr *)&client_addr, &client_len);

			/* Did we get the bytes for the base header of the packet? */
			if (nbytes <= 0) break;    // No data, i.e. no packet
			if (nbytes <= 2) continue; // Invalid data; try next packet

			NetworkAddress address(client_addr, client_len);
			p.PrepareToRead();

			/* If the size does not match the packet must be corrupted.
			 * Otherwise it will be marked as corrupted later on. */
			if (nbytes != p.size) {
				DEBUG(net, 1, "received a packet with mismatching size from %s", address.GetAddressAsString());
				continue;
			}

			/* Handle the packet */
			this->HandleUDPPacket(&p, &address);
		}
	}
}


/**
 * Serializes the NetworkGameInfo struct to the packet
 * @param p    the packet to write the data to
 * @param info the NetworkGameInfo struct to serialize
 */
void NetworkUDPSocketHandler::SendNetworkGameInfo(Packet *p, const NetworkGameInfo *info)
{
	p->Send_uint8 (NETWORK_GAME_INFO_VERSION);

	/*
	 *              Please observe the order.
	 * The parts must be read in the same order as they are sent!
	 */

	/* Update the documentation in udp.h on changes
	 * to the NetworkGameInfo wire-protocol! */

	/* NETWORK_GAME_INFO_VERSION = 4 */
	{
		/* Only send the GRF Identification (GRF_ID and MD5 checksum) of
		 * the GRFs that are needed, i.e. the ones that the server has
		 * selected in the NewGRF GUI and not the ones that are used due
		 * to the fact that they are in [newgrf-static] in openttd.cfg */
		const GRFConfig *c;
		uint count = 0;

		/* Count number of GRFs to send information about */
		for (c = info->grfconfig; c != NULL; c = c->next) {
			if (!HasBit(c->flags, GCF_STATIC)) count++;
		}
		p->Send_uint8 (count); // Send number of GRFs

		/* Send actual GRF Identifications */
		for (c = info->grfconfig; c != NULL; c = c->next) {
			if (!HasBit(c->flags, GCF_STATIC)) this->SendGRFIdentifier(p, &c->ident);
		}
	}

	/* NETWORK_GAME_INFO_VERSION = 3 */
	p->Send_uint32(info->game_date);
	p->Send_uint32(info->start_date);

	/* NETWORK_GAME_INFO_VERSION = 2 */
	p->Send_uint8 (info->companies_max);
	p->Send_uint8 (info->companies_on);
	p->Send_uint8 (info->spectators_max);

	/* NETWORK_GAME_INFO_VERSION = 1 */
	p->Send_string(info->server_name);
	p->Send_string(info->server_revision);
	p->Send_uint8 (info->server_lang);
	p->Send_bool  (info->use_password);
	p->Send_uint8 (info->clients_max);
	p->Send_uint8 (info->clients_on);
	p->Send_uint8 (info->spectators_on);
	p->Send_string(info->map_name);
	p->Send_uint16(info->map_width);
	p->Send_uint16(info->map_height);
	p->Send_uint8 (info->map_set);
	p->Send_bool  (info->dedicated);
}

/**
 * Deserializes the NetworkGameInfo struct from the packet
 * @param p    the packet to read the data from
 * @param info the NetworkGameInfo to deserialize into
 */
void NetworkUDPSocketHandler::ReceiveNetworkGameInfo(Packet *p, NetworkGameInfo *info)
{
	static const Date MAX_DATE = ConvertYMDToDate(MAX_YEAR, 11, 31); // December is month 11

	info->game_info_version = p->Recv_uint8();

	/*
	 *              Please observe the order.
	 * The parts must be read in the same order as they are sent!
	 */

	/* Update the documentation in udp.h on changes
	 * to the NetworkGameInfo wire-protocol! */

	switch (info->game_info_version) {
		case 4: {
			GRFConfig **dst = &info->grfconfig;
			uint i;
			uint num_grfs = p->Recv_uint8();

			/* Broken/bad data. It cannot have that many NewGRFs. */
			if (num_grfs > NETWORK_MAX_GRF_COUNT) return;

			for (i = 0; i < num_grfs; i++) {
				GRFConfig *c = new GRFConfig();
				this->ReceiveGRFIdentifier(p, &c->ident);
				this->HandleIncomingNetworkGameInfoGRFConfig(c);

				/* Append GRFConfig to the list */
				*dst = c;
				dst = &c->next;
			}
		}
		FALLTHROUGH;

		case 3:
			info->game_date      = Clamp(p->Recv_uint32(), 0, MAX_DATE);
			info->start_date     = Clamp(p->Recv_uint32(), 0, MAX_DATE);
			FALLTHROUGH;

		case 2:
			info->companies_max  = p->Recv_uint8 ();
			info->companies_on   = p->Recv_uint8 ();
			info->spectators_max = p->Recv_uint8 ();
			FALLTHROUGH;

		case 1:
			p->Recv_string(info->server_name,     sizeof(info->server_name));
			p->Recv_string(info->server_revision, sizeof(info->server_revision));
			info->server_lang    = p->Recv_uint8 ();
			info->use_password   = p->Recv_bool  ();
			info->clients_max    = p->Recv_uint8 ();
			info->clients_on     = p->Recv_uint8 ();
			info->spectators_on  = p->Recv_uint8 ();
			if (info->game_info_version < 3) { // 16 bits dates got scrapped and are read earlier
				info->game_date    = p->Recv_uint16() + DAYS_TILL_ORIGINAL_BASE_YEAR;
				info->start_date   = p->Recv_uint16() + DAYS_TILL_ORIGINAL_BASE_YEAR;
			}
			p->Recv_string(info->map_name, sizeof(info->map_name));
			info->map_width      = p->Recv_uint16();
			info->map_height     = p->Recv_uint16();
			info->map_set        = p->Recv_uint8 ();
			info->dedicated      = p->Recv_bool  ();

			if (info->server_lang >= NETWORK_NUM_LANGUAGES)  info->server_lang = 0;
			if (info->map_set     >= NETWORK_NUM_LANDSCAPES) info->map_set     = 0;
	}
}

/**
 * Handle an incoming packets by sending it to the correct function.
 * @param p the received packet
 * @param client_addr the sender of the packet
 */
void NetworkUDPSocketHandler::HandleUDPPacket(Packet *p, NetworkAddress *client_addr)
{
	PacketUDPType type;

	/* New packet == new client, which has not quit yet */
	this->Reopen();

	type = (PacketUDPType)p->Recv_uint8();

	switch (this->HasClientQuit() ? PACKET_UDP_END : type) {
		case PACKET_UDP_CLIENT_FIND_SERVER:   this->Receive_CLIENT_FIND_SERVER(p, client_addr);   break;
		case PACKET_UDP_SERVER_RESPONSE:      this->Receive_SERVER_RESPONSE(p, client_addr);      break;
		case PACKET_UDP_CLIENT_DETAIL_INFO:   this->Receive_CLIENT_DETAIL_INFO(p, client_addr);   break;
		case PACKET_UDP_SERVER_DETAIL_INFO:   this->Receive_SERVER_DETAIL_INFO(p, client_addr);   break;
		case PACKET_UDP_SERVER_REGISTER:      this->Receive_SERVER_REGISTER(p, client_addr);      break;
		case PACKET_UDP_MASTER_ACK_REGISTER:  this->Receive_MASTER_ACK_REGISTER(p, client_addr);  break;
		case PACKET_UDP_CLIENT_GET_LIST:      this->Receive_CLIENT_GET_LIST(p, client_addr);      break;
		case PACKET_UDP_MASTER_RESPONSE_LIST: this->Receive_MASTER_RESPONSE_LIST(p, client_addr); break;
		case PACKET_UDP_SERVER_UNREGISTER:    this->Receive_SERVER_UNREGISTER(p, client_addr);    break;
		case PACKET_UDP_CLIENT_GET_NEWGRFS:   this->Receive_CLIENT_GET_NEWGRFS(p, client_addr);   break;
		case PACKET_UDP_SERVER_NEWGRFS:       this->Receive_SERVER_NEWGRFS(p, client_addr);       break;
		case PACKET_UDP_MASTER_SESSION_KEY:   this->Receive_MASTER_SESSION_KEY(p, client_addr);   break;

		default:
			if (this->HasClientQuit()) {
				DEBUG(net, 0, "[udp] received invalid packet type %d from %s", type, client_addr->GetAddressAsString());
			} else {
				DEBUG(net, 0, "[udp] received illegal packet from %s", client_addr->GetAddressAsString());
			}
			break;
	}
}

/**
 * Helper for logging receiving invalid packets.
 * @param type The received packet type.
 * @param client_addr The address we received the packet from.
 */
void NetworkUDPSocketHandler::ReceiveInvalidPacket(PacketUDPType type, NetworkAddress *client_addr)
{
	DEBUG(net, 0, "[udp] received packet type %d on wrong port from %s", type, client_addr->GetAddressAsString());
}

void NetworkUDPSocketHandler::Receive_CLIENT_FIND_SERVER(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_CLIENT_FIND_SERVER, client_addr); }
void NetworkUDPSocketHandler::Receive_SERVER_RESPONSE(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_SERVER_RESPONSE, client_addr); }
void NetworkUDPSocketHandler::Receive_CLIENT_DETAIL_INFO(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_CLIENT_DETAIL_INFO, client_addr); }
void NetworkUDPSocketHandler::Receive_SERVER_DETAIL_INFO(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_SERVER_DETAIL_INFO, client_addr); }
void NetworkUDPSocketHandler::Receive_SERVER_REGISTER(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_SERVER_REGISTER, client_addr); }
void NetworkUDPSocketHandler::Receive_MASTER_ACK_REGISTER(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_MASTER_ACK_REGISTER, client_addr); }
void NetworkUDPSocketHandler::Receive_CLIENT_GET_LIST(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_CLIENT_GET_LIST, client_addr); }
void NetworkUDPSocketHandler::Receive_MASTER_RESPONSE_LIST(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_MASTER_RESPONSE_LIST, client_addr); }
void NetworkUDPSocketHandler::Receive_SERVER_UNREGISTER(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_SERVER_UNREGISTER, client_addr); }
void NetworkUDPSocketHandler::Receive_CLIENT_GET_NEWGRFS(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_CLIENT_GET_NEWGRFS, client_addr); }
void NetworkUDPSocketHandler::Receive_SERVER_NEWGRFS(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_SERVER_NEWGRFS, client_addr); }
void NetworkUDPSocketHandler::Receive_MASTER_SESSION_KEY(Packet *p, NetworkAddress *client_addr) { this->ReceiveInvalidPacket(PACKET_UDP_MASTER_SESSION_KEY, client_addr); }

#endif /* ENABLE_NETWORK */
