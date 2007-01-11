/* $Id$ */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "../../debug.h"
#include "../../macros.h"
#include "../../helpers.hpp"
#include "packet.h"
#include "udp.h"

/**
 * @file udp.c Basic functions to receive and send UDP packets.
 */

/**
 * Start listening on the given host and port.
 * @param udp       the place where the (references to the) UDP are stored
 * @param host      the host (ip) to listen on
 * @param port      the port to listen on
 * @param broadcast whether to allow broadcast sending/receiving
 * @return true if the listening succeeded
 */
bool NetworkUDPListen(SOCKET *udp, const uint32 host, const uint16 port, const bool broadcast)
{
	struct sockaddr_in sin;

	/* Make sure socket is closed */
	NetworkUDPClose(udp);

	*udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (*udp == INVALID_SOCKET) {
		DEBUG(net, 0, "[udp] failed to start UDP listener");
		return false;
	}

	/* set nonblocking mode for socket */
	{
		unsigned long blocking = 1;
#ifndef BEOS_NET_SERVER
		ioctlsocket(*udp, FIONBIO, &blocking);
#else
		setsockopt(*udp, SOL_SOCKET, SO_NONBLOCK, &blocking, NULL);
#endif
	}

	sin.sin_family = AF_INET;
	/* Listen on all IPs */
	sin.sin_addr.s_addr = host;
	sin.sin_port = htons(port);

	if (bind(*udp, (struct sockaddr*)&sin, sizeof(sin)) != 0) {
		DEBUG(net, 0, "[udp] bind failed on %s:%i", inet_ntoa(*(struct in_addr *)&host), port);
		return false;
	}

	if (broadcast) {
		/* Enable broadcast */
		unsigned long val = 1;
#ifndef BEOS_NET_SERVER // will work around this, some day; maybe.
		setsockopt(*udp, SOL_SOCKET, SO_BROADCAST, (char *) &val , sizeof(val));
#endif
	}

	DEBUG(net, 1, "[udp] listening on port %s:%d", inet_ntoa(*(struct in_addr *)&host), port);

	return true;
}

/**
 * Close the given UDP socket
 * @param udp the socket to close
 */
void NetworkUDPClose(SOCKET *udp)
{
	if (*udp == INVALID_SOCKET) return;

	closesocket(*udp);
	*udp = INVALID_SOCKET;
}


/**
 * Send a packet over UDP
 * @param udp  the socket to send over
 * @param p    the packet to send
 * @param recv the receiver (target) of the packet
 */
void NetworkSendUDP_Packet(const SOCKET udp, Packet *p, const struct sockaddr_in *recv)
{
	int res;

	NetworkSend_FillPacketSize(p);

	/* Send the buffer */
	res = sendto(udp, (const char*)p->buffer, p->size, 0, (struct sockaddr *)recv, sizeof(*recv));

	/* Check for any errors, but ignore it otherwise */
	if (res == -1) DEBUG(net, 1, "[udp] sendto failed with: %i", GET_LAST_ERROR());
}

/**
 * Receive a packet at UDP level
 * @param udp the socket to receive the packet on
 */
void NetworkUDPReceive(const SOCKET udp)
{
	struct sockaddr_in client_addr;
	socklen_t client_len;
	int nbytes;
	Packet p;
	int packet_len;

	packet_len = sizeof(p.buffer);
	client_len = sizeof(client_addr);

	/* Try to receive anything */
	nbytes = recvfrom(udp, (char*)p.buffer, packet_len, 0, (struct sockaddr *)&client_addr, &client_len);

	/* We got some bytes for the base header of the packet. */
	if (nbytes > 2) {
		NetworkRecv_ReadPacketSize(&p);

		/* If the size does not match the packet must be corrupted.
		 * Otherwise it will be marked as corrupted later on. */
		if (nbytes != p.size) {
			DEBUG(net, 1, "received a packet with mismatching size from %s:%d",
					inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

			return;
		}

		/* Put the position on the right place */
		p.pos = 2;
		p.next = NULL;

		/* Handle the packet */
		NetworkHandleUDPPacket(udp, &p, &client_addr);
	}
}


/**
 * Serializes the GRFIdentifier (GRF ID and MD5 checksum) to the packet
 * @param p the packet to write the data to
 * @param c the configuration to write the GRF ID and MD5 checksum from
 */
void NetworkSend_GRFIdentifier(Packet *p, const GRFConfig *c)
{
	uint j;
	NetworkSend_uint32(p, c->grfid);
	for (j = 0; j < sizeof(c->md5sum); j++) {
		NetworkSend_uint8 (p, c->md5sum[j]);
	}
}

/**
 * Deserializes the GRFIdentifier (GRF ID and MD5 checksum) from the packet
 * @param cs the client state (for closing connect on out-of-bounds reading etc)
 * @param p  the packet to read the data from
 * @param c  the configuration to write the GRF ID and MD5 checksum to
 */
void NetworkRecv_GRFIdentifier(NetworkClientState *cs, Packet *p, GRFConfig *c)
{
	uint j;
	c->grfid = NetworkRecv_uint32(cs, p);
	for (j = 0; j < sizeof(c->md5sum); j++) {
		c->md5sum[j] = NetworkRecv_uint8(cs, p);
	}
}


/**
 * Serializes the NetworkGameInfo struct to the packet
 * @param p    the packet to write the data to
 * @param info the NetworkGameInfo struct to serialize
 */
void NetworkSend_NetworkGameInfo(Packet *p, const NetworkGameInfo *info)
{
	NetworkSend_uint8 (p, NETWORK_GAME_INFO_VERSION);

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
			if (!HASBIT(c->flags, GCF_STATIC)) count++;
		}
		NetworkSend_uint8 (p, count); // Send number of GRFs

		/* Send actual GRF Identifications */
		for (c = info->grfconfig; c != NULL; c = c->next) {
			if (!HASBIT(c->flags, GCF_STATIC)) NetworkSend_GRFIdentifier(p, c);
		}
	}

	/* NETWORK_GAME_INFO_VERSION = 3 */
	NetworkSend_uint32(p, info->game_date);
	NetworkSend_uint32(p, info->start_date);

	/* NETWORK_GAME_INFO_VERSION = 2 */
	NetworkSend_uint8 (p, info->companies_max);
	NetworkSend_uint8 (p, info->companies_on);
	NetworkSend_uint8 (p, info->spectators_max);

	/* NETWORK_GAME_INFO_VERSION = 1 */
	NetworkSend_string(p, info->server_name);
	NetworkSend_string(p, info->server_revision);
	NetworkSend_uint8 (p, info->server_lang);
	NetworkSend_uint8 (p, info->use_password);
	NetworkSend_uint8 (p, info->clients_max);
	NetworkSend_uint8 (p, info->clients_on);
	NetworkSend_uint8 (p, info->spectators_on);
	NetworkSend_string(p, info->map_name);
	NetworkSend_uint16(p, info->map_width);
	NetworkSend_uint16(p, info->map_height);
	NetworkSend_uint8 (p, info->map_set);
	NetworkSend_uint8 (p, info->dedicated);
}

/**
 * Deserializes the NetworkGameInfo struct from the packet
 * @param cs   the client state (for closing connect on out-of-bounds reading etc)
 * @param p    the packet to read the data from
 * @param info the NetworkGameInfo to deserialize into
 */
void NetworkRecv_NetworkGameInfo(NetworkClientState *cs, Packet *p, NetworkGameInfo *info)
{
	info->game_info_version = NetworkRecv_uint8(cs, p);

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
			uint num_grfs = NetworkRecv_uint8(cs, p);

			for (i = 0; i < num_grfs; i++) {
				GRFConfig *c = CallocT<GRFConfig>(1);
				NetworkRecv_GRFIdentifier(cs, p, c);
				HandleIncomingNetworkGameInfoGRFConfig(c);

				/* Append GRFConfig to the list */
				*dst = c;
				dst = &c->next;
			}
		} /* Fallthrough */
		case 3:
			info->game_date      = NetworkRecv_uint32(cs, p);
			info->start_date     = NetworkRecv_uint32(cs, p);
			/* Fallthrough */
		case 2:
			info->companies_max  = NetworkRecv_uint8 (cs, p);
			info->companies_on   = NetworkRecv_uint8 (cs, p);
			info->spectators_max = NetworkRecv_uint8 (cs, p);
			/* Fallthrough */
		case 1:
			NetworkRecv_string(cs, p, info->server_name,     sizeof(info->server_name));
			NetworkRecv_string(cs, p, info->server_revision, sizeof(info->server_revision));
			info->server_lang    = NetworkRecv_uint8 (cs, p);
			info->use_password   = NetworkRecv_uint8 (cs, p);
			info->clients_max    = NetworkRecv_uint8 (cs, p);
			info->clients_on     = NetworkRecv_uint8 (cs, p);
			info->spectators_on  = NetworkRecv_uint8 (cs, p);
			if (info->game_info_version < 3) { // 16 bits dates got scrapped and are read earlier
				info->game_date    = NetworkRecv_uint16(cs, p) + DAYS_TILL_ORIGINAL_BASE_YEAR;
				info->start_date   = NetworkRecv_uint16(cs, p) + DAYS_TILL_ORIGINAL_BASE_YEAR;
			}
			NetworkRecv_string(cs, p, info->map_name, sizeof(info->map_name));
			info->map_width      = NetworkRecv_uint16(cs, p);
			info->map_height     = NetworkRecv_uint16(cs, p);
			info->map_set        = NetworkRecv_uint8 (cs, p);
			info->dedicated      = (NetworkRecv_uint8 (cs, p) != 0);
	}
}

#endif /* ENABLE_NETWORK */
