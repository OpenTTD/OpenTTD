#include "stdafx.h"
#include "network_data.h"

#ifdef ENABLE_NETWORK

#include "network_gamelist.h"

extern void UpdateNetworkGameWindow(bool unselect);

//
// This file handles all the LAN-stuff
// Stuff like:
//   - UDP search over the network
//

typedef enum {
	PACKET_UDP_FIND_SERVER,
	PACKET_UDP_SERVER_RESPONSE,
	PACKET_UDP_END
} PacketUDPType;

static SOCKET _udp_server_socket; // udp server socket

#define DEF_UDP_RECEIVE_COMMAND(type) void NetworkPacketReceive_ ## type ## _command(Packet *p, struct sockaddr_in *client_addr)
void NetworkSendUDP_Packet(Packet *p, struct sockaddr_in *recv);

DEF_UDP_RECEIVE_COMMAND(PACKET_UDP_FIND_SERVER)
{
	Packet *packet;
	// Just a fail-safe.. should never happen
	if (!_network_udp_server)
		return;

	packet = NetworkSend_Init(PACKET_UDP_SERVER_RESPONSE);

	// Update some game_info
	_network_game_info.game_date = _date;
	_network_game_info.map_set = _opt.landscape;

	NetworkSend_uint8 (packet, NETWORK_GAME_INFO_VERSION);
	NetworkSend_string(packet, _network_game_info.server_name);
	NetworkSend_string(packet, _network_game_info.server_revision);
	NetworkSend_uint8 (packet, _network_game_info.server_lang);
	NetworkSend_uint8 (packet, _network_game_info.use_password);
	NetworkSend_uint8 (packet, _network_game_info.clients_max);
	NetworkSend_uint8 (packet, _network_game_info.clients_on);
	NetworkSend_uint8 (packet, _network_game_info.spectators_on);
	NetworkSend_uint16(packet, _network_game_info.game_date);
	NetworkSend_uint16(packet, _network_game_info.start_date);
	NetworkSend_string(packet, _network_game_info.map_name);
	NetworkSend_uint16(packet, _network_game_info.map_width);
	NetworkSend_uint16(packet, _network_game_info.map_height);
	NetworkSend_uint8 (packet, _network_game_info.map_set);
	NetworkSend_uint8 (packet, _network_game_info.dedicated);

	// Let the client know that we are here
	NetworkSendUDP_Packet(packet, client_addr);

	free(packet);

	DEBUG(net, 2)("[NET][UDP] Queried from %s", inet_ntoa(client_addr->sin_addr));
}

DEF_UDP_RECEIVE_COMMAND(PACKET_UDP_SERVER_RESPONSE)
{
	NetworkGameList *item;
	byte game_info_version;

	// Just a fail-safe.. should never happen
	if (_network_udp_server)
		return;

	game_info_version = NetworkRecv_uint8(p);

	// Find next item
	item = NetworkGameListAddItem(inet_addr(inet_ntoa(client_addr->sin_addr)), ntohs(client_addr->sin_port));

	if (game_info_version == 1) {
		NetworkRecv_string(p, item->info.server_name, sizeof(item->info.server_name));
		NetworkRecv_string(p, item->info.server_revision, sizeof(item->info.server_revision));
		item->info.server_lang = NetworkRecv_uint8(p);
		item->info.use_password = NetworkRecv_uint8(p);
		item->info.clients_max = NetworkRecv_uint8(p);
		item->info.clients_on = NetworkRecv_uint8(p);
		item->info.spectators_on = NetworkRecv_uint8(p);
		item->info.game_date = NetworkRecv_uint16(p);
		item->info.start_date = NetworkRecv_uint16(p);
		NetworkRecv_string(p, item->info.map_name, sizeof(item->info.map_name));
		item->info.map_width = NetworkRecv_uint16(p);
		item->info.map_height = NetworkRecv_uint16(p);
		item->info.map_set = NetworkRecv_uint8(p);
		item->info.dedicated = NetworkRecv_uint8(p);

		if (item->info.hostname[0] == '\0')
			snprintf(item->info.hostname, sizeof(item->info.hostname), "%s", inet_ntoa(client_addr->sin_addr));
	}

	item->online = true;

	UpdateNetworkGameWindow(false);
}


// The layout for the receive-functions by UDP
typedef void NetworkUDPPacket(Packet *p, struct sockaddr_in *client_addr);

static NetworkUDPPacket* const _network_udp_packet[] = {
	RECEIVE_COMMAND(PACKET_UDP_FIND_SERVER),
	RECEIVE_COMMAND(PACKET_UDP_SERVER_RESPONSE),
};

// If this fails, check the array above with network_data.h
assert_compile(lengthof(_network_udp_packet) == PACKET_UDP_END);


void NetworkHandleUDPPacket(Packet *p, struct sockaddr_in *client_addr)
{
	byte type;

	type = NetworkRecv_uint8(p);

	if (type < PACKET_UDP_END && _network_udp_packet[type] != NULL) {
		_network_udp_packet[type](p, client_addr);
	}	else {
		DEBUG(net, 0)("[NET][UDP] Received invalid packet type %d", type);
	}
}


// Send a packet over UDP
void NetworkSendUDP_Packet(Packet *p, struct sockaddr_in *recv)
{
	SOCKET udp;
	int res;

	// Find the correct socket
	if (_network_udp_server)
		udp = _udp_server_socket;
	else
		udp = _udp_client_socket;

	// Put the length in the buffer
	p->buffer[0] = p->size & 0xFF;
	p->buffer[1] = p->size >> 8;

	// Send the buffer
	res = sendto(udp, p->buffer, p->size, 0, (struct sockaddr *)recv, sizeof(*recv));

	// Check for any errors, but ignore it for the rest
	if (res == -1) {
		DEBUG(net, 1)("[NET][UDP] Send error: %i", GET_LAST_ERROR());
	}
}

// Start UDP listener
bool NetworkUDPListen(uint32 host, uint16 port)
{
	struct sockaddr_in sin;
	SOCKET udp;

	// Make sure sockets are closed
	if (_network_udp_server)
		closesocket(_udp_server_socket);
	else
		closesocket(_udp_client_socket);

	udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (udp == INVALID_SOCKET) {
		DEBUG(net, 1)("[NET][UDP] Failed to start UDP support");
		return false;
	}

	// set nonblocking mode for socket
	{
		unsigned long blocking = 1;
		ioctlsocket(udp, FIONBIO, &blocking);
	}

	sin.sin_family = AF_INET;
	// Listen on all IPs
	sin.sin_addr.s_addr = host;
	sin.sin_port = htons(port);

	if (bind(udp, (struct sockaddr*)&sin, sizeof(sin)) != 0) {
		DEBUG(net, 1) ("[NET][UDP] error: bind failed on %s:%i", inet_ntoa(*(struct in_addr *)&host), port);
		return false;
	}

	// enable broadcasting
	// allow reusing
	{
		unsigned long val = 1;
		setsockopt(udp, SOL_SOCKET, SO_BROADCAST, (char *) &val , sizeof(val));
		val = 1;
		setsockopt(udp, SOL_SOCKET, SO_REUSEADDR, (char *) &val , sizeof(val));
	}

	if (_network_udp_server)
		_udp_server_socket = udp;
	else
		_udp_client_socket = udp;

	DEBUG(net, 1)("[NET][UDP] Listening on port %s:%d", inet_ntoa(*(struct in_addr *)&host), port);

	return true;
}

// Close UDP connection
void NetworkUDPClose(void)
{
	DEBUG(net, 1) ("[NET][UDP] Closed listener");

	if (_network_udp_server) {
		closesocket(_udp_server_socket);
		_udp_server_socket = INVALID_SOCKET;
		_network_udp_server = false;
		_network_udp_broadcast = 0;
	} else {
		closesocket(_udp_client_socket);
		_udp_client_socket = INVALID_SOCKET;
		_network_udp_broadcast = 0;
	}
}

// Receive something on UDP level
void NetworkUDPReceive(void)
{
	struct sockaddr_in client_addr;
#ifndef __MORPHOS__
	int client_len;
#else
	LONG client_len; // for some reason we need a 'LONG' under MorphOS
#endif
	int nbytes;
	static Packet *p = NULL;
	int packet_len;
	SOCKET udp;

	if (_network_udp_server)
		udp = _udp_server_socket;
	else
		udp = _udp_client_socket;

	// If p is NULL, malloc him.. this prevents unneeded mallocs
	if (p == NULL)
		p = malloc(sizeof(Packet));

	packet_len = sizeof(p->buffer);
	client_len = sizeof(client_addr);

	// Try to receive anything
	nbytes = recvfrom(udp, p->buffer, packet_len, 0, (struct sockaddr *)&client_addr, &client_len);

	// We got some bytes.. just asume we receive the whole packet
	if (nbytes > 0) {
		// Get the size of the buffer
		p->size = (uint16)p->buffer[0];
		p->size += (uint16)p->buffer[1] << 8;
		// Put the position on the right place
		p->pos = 2;
		p->next = NULL;

		// Handle the packet
		NetworkHandleUDPPacket(p, &client_addr);

		// Free the packet
		free(p);
		p = NULL;
	}
}

// Broadcast to all ips
void NetworkUDPBroadCast(void)
{
	int i;
	struct sockaddr_in out_addr;
	byte *bcptr;
	uint32 bcaddr;
	Packet *p;

	// Init the packet
	p = NetworkSend_Init(PACKET_UDP_FIND_SERVER);

	// Go through all the ips on this pc
	i = 0;
	while (_network_ip_list[i] != 0) {
		bcaddr = _network_ip_list[i];
		bcptr = (byte *)&bcaddr;
		// Make the address a broadcast address
		bcptr[3] = 255;

		DEBUG(net, 6)("[NET][UDP] Broadcasting to %s", inet_ntoa(*(struct in_addr *)&bcaddr));

		out_addr.sin_family = AF_INET;
		out_addr.sin_port = htons(_network_server_port);
		out_addr.sin_addr.s_addr = bcaddr;

		NetworkSendUDP_Packet(p, &out_addr);

		i++;
	}

	free(p);
}

// Find all servers
void NetworkUDPSearchGame(void)
{
	// We are still searching..
	if (_network_udp_broadcast > 0)
		return;

	// No UDP-socket yet..
	if (_udp_client_socket == INVALID_SOCKET)
		if (!NetworkUDPListen(0, 0))
			return;

	DEBUG(net, 0)("[NET][UDP] Searching server");

	NetworkUDPBroadCast();
	_network_udp_broadcast = 300; // Stay searching for 300 ticks
}

void NetworkUDPQueryServer(const byte* host, unsigned short port)
{
	struct sockaddr_in out_addr;
	Packet *p;
	NetworkGameList *item;
	char hostname[NETWORK_HOSTNAME_LENGTH];

	// No UDP-socket yet..
	if (_udp_client_socket == INVALID_SOCKET)
		if (!NetworkUDPListen(0, 0))
			return;

	ttd_strlcpy(hostname, host, sizeof(hostname));

	out_addr.sin_family = AF_INET;
	out_addr.sin_port = htons(port);
	out_addr.sin_addr.s_addr = NetworkResolveHost(host);

	// Clear item in gamelist
	item = NetworkGameListAddItem(inet_addr(inet_ntoa(out_addr.sin_addr)), ntohs(out_addr.sin_port));
	memset(&item->info, 0, sizeof(item->info));
	snprintf(item->info.server_name, sizeof(item->info.server_name), "%s", hostname);
	snprintf(item->info.hostname, sizeof(item->info.hostname), "%s", hostname);
	item->online = false;

	// Init the packet
	p = NetworkSend_Init(PACKET_UDP_FIND_SERVER);

	NetworkSendUDP_Packet(p, &out_addr);

	free(p);

	UpdateNetworkGameWindow(false);
}

void NetworkUDPInitialize(void)
{
	_udp_client_socket = INVALID_SOCKET;
	_udp_server_socket = INVALID_SOCKET;

	_network_udp_server = false;
	_network_udp_broadcast = 0;
}

#endif /* ENABLE_NETWORK */
