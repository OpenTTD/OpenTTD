/* $Id$ */

/**
 * @file core/udp.cpp Basic functions to receive and send UDP packets.
 */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "../../debug.h"
#include "../../core/bitmath_func.hpp"
#include "../../core/math_func.hpp"
#include "../../core/alloc_func.hpp"
#include "../../date_func.h"
#include "packet.h"
#include "udp.h"

/**
 * Start listening on the given host and port.
 * @param address the host to listen on
 * @param broadcast whether to allow broadcast sending/receiving
 * @return true if the listening succeeded
 */
bool NetworkUDPSocketHandler::Listen(NetworkAddress address, bool broadcast)
{
	/* Make sure socket is closed */
	this->Close();

	this->sock = address.Listen(AF_INET, SOCK_DGRAM);

	if (broadcast) {
		/* Enable broadcast */
		unsigned long val = 1;
#ifndef BEOS_NET_SERVER /* will work around this, some day; maybe. */
		setsockopt(this->sock, SOL_SOCKET, SO_BROADCAST, (char *) &val, sizeof(val));
#endif
	}

	DEBUG(net, 1, "[udp] listening on port %s", address.GetAddressAsString());

	return true;
}

/**
 * Close the given UDP socket
 */
void NetworkUDPSocketHandler::Close()
{
	if (!this->IsConnected()) return;

	closesocket(this->sock);
	this->sock = INVALID_SOCKET;
}

NetworkRecvStatus NetworkUDPSocketHandler::CloseConnection()
{
	this->has_quit = true;
	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Send a packet over UDP
 * @param p    the packet to send
 * @param recv the receiver (target) of the packet
 */
void NetworkUDPSocketHandler::SendPacket(Packet *p, NetworkAddress *recv)
{
	int res;

	p->PrepareToSend();

	/* Send the buffer */
	res = sendto(this->sock, (const char*)p->buffer, p->size, 0, (struct sockaddr *)recv->GetAddress(), recv->GetAddressLength());

	/* Check for any errors, but ignore it otherwise */
	if (res == -1) DEBUG(net, 1, "[udp] sendto failed with: %i", GET_LAST_ERROR());
}

/**
 * Receive a packet at UDP level
 */
void NetworkUDPSocketHandler::ReceivePackets()
{
	struct sockaddr_storage client_addr;
	memset(&client_addr, 0, sizeof(client_addr));

	socklen_t client_len;
	int nbytes;
	Packet p(this);
	int packet_len;

	if (!this->IsConnected()) return;

	packet_len = sizeof(p.buffer);
	client_len = sizeof(client_addr);

	/* Try to receive anything */
	SetNonBlocking(this->sock); // Some OSes seem to lose the non-blocking status of the socket
	nbytes = recvfrom(this->sock, (char*)p.buffer, packet_len, 0, (struct sockaddr *)&client_addr, &client_len);

	/* We got some bytes for the base header of the packet. */
	if (nbytes > 2) {
		NetworkAddress address(client_addr, client_len);
		p.PrepareToRead();

		/* If the size does not match the packet must be corrupted.
		 * Otherwise it will be marked as corrupted later on. */
		if (nbytes != p.size) {
			DEBUG(net, 1, "received a packet with mismatching size from %s", address.GetAddressAsString());
			return;
		}

		/* Handle the packet */
		this->HandleUDPPacket(&p, &address);
	}
}


/**
 * Serializes the NetworkGameInfo struct to the packet
 * @param p    the packet to write the data to
 * @param info the NetworkGameInfo struct to serialize
 */
void NetworkUDPSocketHandler::Send_NetworkGameInfo(Packet *p, const NetworkGameInfo *info)
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
			if (!HasBit(c->flags, GCF_STATIC)) this->Send_GRFIdentifier(p, c);
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
void NetworkUDPSocketHandler::Recv_NetworkGameInfo(Packet *p, NetworkGameInfo *info)
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
				GRFConfig *c = CallocT<GRFConfig>(1);
				this->Recv_GRFIdentifier(p, c);
				this->HandleIncomingNetworkGameInfoGRFConfig(c);

				/* Append GRFConfig to the list */
				*dst = c;
				dst = &c->next;
			}
		} // Fallthrough
		case 3:
			info->game_date      = Clamp(p->Recv_uint32(), 0, MAX_DATE);
			info->start_date     = Clamp(p->Recv_uint32(), 0, MAX_DATE);
			/* Fallthrough */
		case 2:
			info->companies_max  = p->Recv_uint8 ();
			info->companies_on   = p->Recv_uint8 ();
			info->spectators_max = p->Recv_uint8 ();
			/* Fallthrough */
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
 * Defines a simple (switch) case for each network packet
 * @param type the packet type to create the case for
 */
#define UDP_COMMAND(type) case type: this->NetworkPacketReceive_ ## type ## _command(p, client_addr); break;

/**
 * Handle an incoming packets by sending it to the correct function.
 * @param p the received packet
 * @param client_addr the sender of the packet
 */
void NetworkUDPSocketHandler::HandleUDPPacket(Packet *p, NetworkAddress *client_addr)
{
	PacketUDPType type;

	/* New packet == new client, which has not quit yet */
	this->has_quit = false;

	type = (PacketUDPType)p->Recv_uint8();

	switch (this->HasClientQuit() ? PACKET_UDP_END : type) {
		UDP_COMMAND(PACKET_UDP_CLIENT_FIND_SERVER);
		UDP_COMMAND(PACKET_UDP_SERVER_RESPONSE);
		UDP_COMMAND(PACKET_UDP_CLIENT_DETAIL_INFO);
		UDP_COMMAND(PACKET_UDP_SERVER_DETAIL_INFO);
		UDP_COMMAND(PACKET_UDP_SERVER_REGISTER);
		UDP_COMMAND(PACKET_UDP_MASTER_ACK_REGISTER);
		UDP_COMMAND(PACKET_UDP_CLIENT_GET_LIST);
		UDP_COMMAND(PACKET_UDP_MASTER_RESPONSE_LIST);
		UDP_COMMAND(PACKET_UDP_SERVER_UNREGISTER);
		UDP_COMMAND(PACKET_UDP_CLIENT_GET_NEWGRFS);
		UDP_COMMAND(PACKET_UDP_SERVER_NEWGRFS);

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
 * Create stub implementations for all receive commands that only
 * show a warning that the given command is not available for the
 * socket where the packet came from.
 * @param type the packet type to create the stub for
 */
#define DEFINE_UNAVAILABLE_UDP_RECEIVE_COMMAND(type) \
void NetworkUDPSocketHandler::NetworkPacketReceive_## type ##_command(\
		Packet *p, NetworkAddress *client_addr) { \
	DEBUG(net, 0, "[udp] received packet type %d on wrong port from %s", \
			type, client_addr->GetAddressAsString()); \
}

DEFINE_UNAVAILABLE_UDP_RECEIVE_COMMAND(PACKET_UDP_CLIENT_FIND_SERVER);
DEFINE_UNAVAILABLE_UDP_RECEIVE_COMMAND(PACKET_UDP_SERVER_RESPONSE);
DEFINE_UNAVAILABLE_UDP_RECEIVE_COMMAND(PACKET_UDP_CLIENT_DETAIL_INFO);
DEFINE_UNAVAILABLE_UDP_RECEIVE_COMMAND(PACKET_UDP_SERVER_DETAIL_INFO);
DEFINE_UNAVAILABLE_UDP_RECEIVE_COMMAND(PACKET_UDP_SERVER_REGISTER);
DEFINE_UNAVAILABLE_UDP_RECEIVE_COMMAND(PACKET_UDP_MASTER_ACK_REGISTER);
DEFINE_UNAVAILABLE_UDP_RECEIVE_COMMAND(PACKET_UDP_CLIENT_GET_LIST);
DEFINE_UNAVAILABLE_UDP_RECEIVE_COMMAND(PACKET_UDP_MASTER_RESPONSE_LIST);
DEFINE_UNAVAILABLE_UDP_RECEIVE_COMMAND(PACKET_UDP_SERVER_UNREGISTER);
DEFINE_UNAVAILABLE_UDP_RECEIVE_COMMAND(PACKET_UDP_CLIENT_GET_NEWGRFS);
DEFINE_UNAVAILABLE_UDP_RECEIVE_COMMAND(PACKET_UDP_SERVER_NEWGRFS);

#endif /* ENABLE_NETWORK */
