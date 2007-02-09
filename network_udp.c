/* $Id$ */

#ifdef ENABLE_NETWORK

#include "stdafx.h"
#include "debug.h"
#include "string.h"
#include "network_data.h"
#include "date.h"
#include "map.h"
#include "network_gamelist.h"
#include "network_udp.h"
#include "variables.h"
#include "newgrf_config.h"

//
// This file handles all the LAN-stuff
// Stuff like:
//   - UDP search over the network
//

typedef enum {
	PACKET_UDP_CLIENT_FIND_SERVER,
	PACKET_UDP_SERVER_RESPONSE,
	PACKET_UDP_CLIENT_DETAIL_INFO,
	PACKET_UDP_SERVER_DETAIL_INFO,   // Is not used in OpenTTD itself, only for external querying
	PACKET_UDP_SERVER_REGISTER,      // Packet to register itself to the master server
	PACKET_UDP_MASTER_ACK_REGISTER,  // Packet indicating registration has succedeed
	PACKET_UDP_CLIENT_GET_LIST,      // Request for serverlist from master server
	PACKET_UDP_MASTER_RESPONSE_LIST, // Response from master server with server ip's + port's
	PACKET_UDP_SERVER_UNREGISTER,    // Request to be removed from the server-list
	PACKET_UDP_CLIENT_GET_NEWGRFS,   // Requests the name for a list of GRFs (GRF_ID and MD5)
	PACKET_UDP_SERVER_NEWGRFS,       // Sends the list of NewGRF's requested.
	PACKET_UDP_END
} PacketUDPType;

enum {
	ADVERTISE_NORMAL_INTERVAL = 30000, // interval between advertising in ticks (15 minutes)
	ADVERTISE_RETRY_INTERVAL  =   300, // readvertise when no response after this many ticks (9 seconds)
	ADVERTISE_RETRY_TIMES     =     3  // give up readvertising after this much failed retries
};

#define DEF_UDP_RECEIVE_COMMAND(type) void NetworkPacketReceive_ ## type ## _command(Packet *p, struct sockaddr_in *client_addr)
static void NetworkSendUDP_Packet(SOCKET udp, Packet* p, struct sockaddr_in* recv);

static NetworkClientState _udp_cs;

DEF_UDP_RECEIVE_COMMAND(PACKET_UDP_CLIENT_FIND_SERVER)
{
	Packet *packet;
	// Just a fail-safe.. should never happen
	if (!_network_udp_server)
		return;

	packet = NetworkSend_Init(PACKET_UDP_SERVER_RESPONSE);

	// Update some game_info
	_network_game_info.game_date = _date;
	_network_game_info.map_width = MapSizeX();
	_network_game_info.map_height = MapSizeY();
	_network_game_info.map_set = _opt.landscape;

	NetworkSend_uint8 (packet, NETWORK_GAME_INFO_VERSION);

	/* NETWORK_GAME_INFO_VERSION = 4 */
	{
		/* Only send the GRF Identification (GRF_ID and MD5 checksum) of
		 * the GRFs that are needed, i.e. the ones that the server has
		 * selected in the NewGRF GUI and not the ones that are used due
		 * to the fact that they are in [newgrf-static] in openttd.cfg */
		const GRFConfig *c;
		uint i = 0;

		/* Count number of GRFs to send information about */
		for (c = _grfconfig; c != NULL; c = c->next) {
			if (!HASBIT(c->flags, GCF_STATIC)) i++;
		}
		NetworkSend_uint8 (packet, i); // Send number of GRFs

		/* Send actual GRF Identifications */
		for (c = _grfconfig; c != NULL; c = c->next) {
			if (!HASBIT(c->flags, GCF_STATIC)) NetworkSend_GRFIdentifier(packet, c);
		}
	}

	/* NETWORK_GAME_INFO_VERSION = 3 */
	NetworkSend_uint32(packet, _network_game_info.game_date);
	NetworkSend_uint32(packet, _network_game_info.start_date);

	/* NETWORK_GAME_INFO_VERSION = 2 */
	NetworkSend_uint8 (packet, _network_game_info.companies_max);
	NetworkSend_uint8 (packet, ActivePlayerCount());
	NetworkSend_uint8 (packet, _network_game_info.spectators_max);

	/* NETWORK_GAME_INFO_VERSION = 1 */
	NetworkSend_string(packet, _network_game_info.server_name);
	NetworkSend_string(packet, _network_game_info.server_revision);
	NetworkSend_uint8 (packet, _network_game_info.server_lang);
	NetworkSend_uint8 (packet, _network_game_info.use_password);
	NetworkSend_uint8 (packet, _network_game_info.clients_max);
	NetworkSend_uint8 (packet, _network_game_info.clients_on);
	NetworkSend_uint8 (packet, NetworkSpectatorCount());
	NetworkSend_string(packet, _network_game_info.map_name);
	NetworkSend_uint16(packet, _network_game_info.map_width);
	NetworkSend_uint16(packet, _network_game_info.map_height);
	NetworkSend_uint8 (packet, _network_game_info.map_set);
	NetworkSend_uint8 (packet, _network_game_info.dedicated);

	// Let the client know that we are here
	NetworkSendUDP_Packet(_udp_server_socket, packet, client_addr);

	free(packet);

	DEBUG(net, 2)("[NET][UDP] Queried from %s", inet_ntoa(client_addr->sin_addr));
}

DEF_UDP_RECEIVE_COMMAND(PACKET_UDP_SERVER_RESPONSE)
{
	extern const char _openttd_revision[];
	NetworkGameList *item;
	byte game_info_version;

	// Just a fail-safe.. should never happen
	if (_network_udp_server)
		return;

	game_info_version = NetworkRecv_uint8(&_udp_cs, p);

	if (_udp_cs.has_quit) return;

	DEBUG(net, 6)("[NET][UDP] Server response from %s:%d", inet_ntoa(client_addr->sin_addr),ntohs(client_addr->sin_port));

	// Find next item
	item = NetworkGameListAddItem(inet_addr(inet_ntoa(client_addr->sin_addr)), ntohs(client_addr->sin_port));

	item->info.compatible = true;
	/* Please observer the order. In the order in which packets are sent
	 * they are to be received */
	switch (game_info_version) {
		case 4: {
			GRFConfig *c, **dst = &item->info.grfconfig;
			const GRFConfig *f;
			uint i;
			uint num_grfs = NetworkRecv_uint8(&_udp_cs, p);

			for (i = 0; i < num_grfs; i++) {
				c = calloc(1, sizeof(*c));
				NetworkRecv_GRFIdentifier(&_udp_cs, p, c);

				/* Find the matching GRF file */
				f = FindGRFConfig(c->grfid, c->md5sum);
				if (f == NULL) {
					/* Don't know the GRF, so mark game incompatible and the (possibly)
					 * already resolved name for this GRF (another server has sent the
					 * name of the GRF already */
					item->info.compatible = false;
					c->name     = FindUnknownGRFName(c->grfid, c->md5sum, true);
					SETBIT(c->flags, GCF_NOT_FOUND);
				} else {
					c->filename = f->filename;
					c->name     = f->name;
					c->info     = f->info;
				}
				SETBIT(c->flags, GCF_COPY);

				/* Append GRFConfig to the list */
				*dst = c;
				dst = &c->next;
			}
		} /* Fallthrough */
		case 3:
			item->info.game_date     = NetworkRecv_uint32(&_udp_cs, p);
			item->info.start_date    = NetworkRecv_uint32(&_udp_cs, p);
			/* Fallthrough */
		case 2:
			item->info.companies_max = NetworkRecv_uint8(&_udp_cs, p);
			item->info.companies_on = NetworkRecv_uint8(&_udp_cs, p);
			item->info.spectators_max = NetworkRecv_uint8(&_udp_cs, p);
			/* Fallthrough */
		case 1:
			NetworkRecv_string(&_udp_cs, p, item->info.server_name, sizeof(item->info.server_name));
			NetworkRecv_string(&_udp_cs, p, item->info.server_revision, sizeof(item->info.server_revision));
			item->info.server_lang   = NetworkRecv_uint8(&_udp_cs, p);
			item->info.use_password  = NetworkRecv_uint8(&_udp_cs, p);
			item->info.clients_max   = NetworkRecv_uint8(&_udp_cs, p);
			item->info.clients_on    = NetworkRecv_uint8(&_udp_cs, p);
			item->info.spectators_on = NetworkRecv_uint8(&_udp_cs, p);
			if (game_info_version < 3) { // 16 bits dates got scrapped and are read earlier
				item->info.game_date     = NetworkRecv_uint16(&_udp_cs, p) + DAYS_TILL_ORIGINAL_BASE_YEAR;
				item->info.start_date    = NetworkRecv_uint16(&_udp_cs, p) + DAYS_TILL_ORIGINAL_BASE_YEAR;
			}
			NetworkRecv_string(&_udp_cs, p, item->info.map_name, sizeof(item->info.map_name));
			item->info.map_width     = NetworkRecv_uint16(&_udp_cs, p);
			item->info.map_height    = NetworkRecv_uint16(&_udp_cs, p);
			item->info.map_set       = NetworkRecv_uint8(&_udp_cs, p);
			item->info.dedicated     = NetworkRecv_uint8(&_udp_cs, p);

			if (item->info.server_lang >= NETWORK_NUM_LANGUAGES) item->info.server_lang = 0;
			if (item->info.map_set >= NUM_LANDSCAPE ) item->info.map_set = 0;

			if (item->info.hostname[0] == '\0')
				snprintf(item->info.hostname, sizeof(item->info.hostname), "%s", inet_ntoa(client_addr->sin_addr));

			/* Check if we are allowed on this server based on the revision-match */
			item->info.version_compatible =
				strcmp(item->info.server_revision, _openttd_revision) == 0 ||
				strcmp(item->info.server_revision, NOREV_STRING) == 0;
			item->info.compatible &= item->info.version_compatible; // Already contains match for GRFs
			break;
	}

	{
		/* Checks whether there needs to be a request for names of GRFs and makes
		 * the request if necessary. GRFs that need to be requested are the GRFs
		 * that do not exist on the clients system and we do not have the name
		 * resolved of, i.e. the name is still UNKNOWN_GRF_NAME_PLACEHOLDER.
		 * The in_request array and in_request_count are used so there is no need
		 * to do a second loop over the GRF list, which can be relatively expensive
		 * due to the string comparisons. */
		const GRFConfig *in_request[NETWORK_MAX_GRF_COUNT];
		const GRFConfig *c;
		uint in_request_count = 0;
		struct sockaddr_in out_addr;

		for (c = item->info.grfconfig; c != NULL; c = c->next) {
			if (!HASBIT(c->flags, GCF_NOT_FOUND) || strcmp(c->name, UNKNOWN_GRF_NAME_PLACEHOLDER) != 0) continue;
			in_request[in_request_count] = c;
			in_request_count++;
		}

		if (in_request_count > 0) {
			/* There are 'unknown' GRFs, now send a request for them */
			uint i;
			Packet *packet = NetworkSend_Init(PACKET_UDP_CLIENT_GET_NEWGRFS);

			NetworkSend_uint8 (packet, in_request_count);
			for (i = 0; i < in_request_count; i++) {
				NetworkSend_GRFIdentifier(packet, in_request[i]);
			}

			out_addr.sin_family      = AF_INET;
			out_addr.sin_port        = htons(item->port);
			out_addr.sin_addr.s_addr = item->ip;
			NetworkSendUDP_Packet(_udp_client_socket, packet, &out_addr);
			free(packet);
		}
	}

	item->online = true;

	UpdateNetworkGameWindow(false);
}

DEF_UDP_RECEIVE_COMMAND(PACKET_UDP_CLIENT_DETAIL_INFO)
{
	NetworkClientState *cs;
	NetworkClientInfo *ci;
	Packet *packet;
	Player *player;
	byte current = 0;
	int i;

	// Just a fail-safe.. should never happen
	if (!_network_udp_server) return;

	packet = NetworkSend_Init(PACKET_UDP_SERVER_DETAIL_INFO);

	/* Send the amount of active companies */
	NetworkSend_uint8 (packet, NETWORK_COMPANY_INFO_VERSION);
	NetworkSend_uint8 (packet, ActivePlayerCount());

	/* Fetch the latest version of everything */
	NetworkPopulateCompanyInfo();

	/* Go through all the players */
	FOR_ALL_PLAYERS(player) {
		/* Skip non-active players */
		if (!player->is_active) continue;

		current++;

		/* Send the information */
		NetworkSend_uint8(packet, current);

		NetworkSend_string(packet, _network_player_info[player->index].company_name);
		NetworkSend_uint32(packet, _network_player_info[player->index].inaugurated_year);
		NetworkSend_uint64(packet, _network_player_info[player->index].company_value);
		NetworkSend_uint64(packet, _network_player_info[player->index].money);
		NetworkSend_uint64(packet, _network_player_info[player->index].income);
		NetworkSend_uint16(packet, _network_player_info[player->index].performance);

		/* Send 1 if there is a passord for the company else send 0 */
		if (_network_player_info[player->index].password[0] != '\0') {
			NetworkSend_uint8(packet, 1);
		} else {
			NetworkSend_uint8(packet, 0);
		}

		for (i = 0; i < NETWORK_VEHICLE_TYPES; i++)
			NetworkSend_uint16(packet, _network_player_info[player->index].num_vehicle[i]);

		for (i = 0; i < NETWORK_STATION_TYPES; i++)
			NetworkSend_uint16(packet, _network_player_info[player->index].num_station[i]);

		/* Find the clients that are connected to this player */
		FOR_ALL_CLIENTS(cs) {
			ci = DEREF_CLIENT_INFO(cs);
			if (ci->client_playas == player->index) {
				/* The uint8 == 1 indicates that a client is following */
				NetworkSend_uint8(packet, 1);
				NetworkSend_string(packet, ci->client_name);
				NetworkSend_string(packet, ci->unique_id);
				NetworkSend_uint32(packet, ci->join_date);
			}
		}
		/* Also check for the server itself */
		ci = NetworkFindClientInfoFromIndex(NETWORK_SERVER_INDEX);
		if (ci->client_playas == player->index) {
			/* The uint8 == 1 indicates that a client is following */
			NetworkSend_uint8(packet, 1);
			NetworkSend_string(packet, ci->client_name);
			NetworkSend_string(packet, ci->unique_id);
			NetworkSend_uint32(packet, ci->join_date);
		}

		/* Indicates end of client list */
		NetworkSend_uint8(packet, 0);
	}

	/* And check if we have any spectators */
	FOR_ALL_CLIENTS(cs) {
		ci = DEREF_CLIENT_INFO(cs);
		if (!IsValidPlayer(ci->client_playas)) {
			/* The uint8 == 1 indicates that a client is following */
			NetworkSend_uint8(packet, 1);
			NetworkSend_string(packet, ci->client_name);
			NetworkSend_string(packet, ci->unique_id);
			NetworkSend_uint32(packet, ci->join_date);
		}
	}

	/* Also check for the server itself */
	ci = NetworkFindClientInfoFromIndex(NETWORK_SERVER_INDEX);
	if (!IsValidPlayer(ci->client_playas)) {
		/* The uint8 == 1 indicates that a client is following */
		NetworkSend_uint8(packet, 1);
		NetworkSend_string(packet, ci->client_name);
		NetworkSend_string(packet, ci->unique_id);
		NetworkSend_uint32(packet, ci->join_date);
	}

	/* Indicates end of client list */
	NetworkSend_uint8(packet, 0);

	NetworkSendUDP_Packet(_udp_server_socket, packet, client_addr);

	free(packet);
}

DEF_UDP_RECEIVE_COMMAND(PACKET_UDP_MASTER_RESPONSE_LIST)
{
	int i;
	struct in_addr ip;
	uint16 port;
	uint8 ver;

	/* packet begins with the protocol version (uint8)
	 * then an uint16 which indicates how many
	 * ip:port pairs are in this packet, after that
	 * an uint32 (ip) and an uint16 (port) for each pair
	 */

	ver = NetworkRecv_uint8(&_udp_cs, p);

	if (_udp_cs.has_quit) return;

	if (ver == 1) {
		for (i = NetworkRecv_uint16(&_udp_cs, p); i != 0 ; i--) {
			ip.s_addr = TO_LE32(NetworkRecv_uint32(&_udp_cs, p));
			port = NetworkRecv_uint16(&_udp_cs, p);
			NetworkUDPQueryServer(inet_ntoa(ip), port);
		}
	}
}

DEF_UDP_RECEIVE_COMMAND(PACKET_UDP_MASTER_ACK_REGISTER)
{
	_network_advertise_retries = 0;
	DEBUG(net, 2)("[NET][UDP] We are advertised on the master-server!");

	if (!_network_advertise) {
		/* We are advertised, but we don't want to! */
		NetworkUDPRemoveAdvertise();
	}
}

/**
 * A client has requested the names of some NewGRFs.
 *
 * Replying this can be tricky as we have a limit of SEND_MTU bytes
 * in the reply packet and we can send up to 100 bytes per NewGRF
 * (GRF ID, MD5sum and NETWORK_GRF_NAME_LENGTH bytes for the name).
 * As SEND_MTU is _much_ less than 100 * NETWORK_MAX_GRF_COUNT, it
 * could be that a packet overflows. To stop this we only reply
 * with the first N NewGRFs so that if the first N + 1 NewGRFs
 * would be sent, the packet overflows.
 * in_reply and in_reply_count are used to keep a list of GRFs to
 * send in the reply.
 */
DEF_UDP_RECEIVE_COMMAND(PACKET_UDP_CLIENT_GET_NEWGRFS)
{
	uint8 num_grfs;
	uint i;

	const GRFConfig *in_reply[NETWORK_MAX_GRF_COUNT];
	Packet *packet;
	uint8 in_reply_count = 0;
	uint packet_len = 0;

	/* Just a fail-safe.. should never happen */
	if (_udp_cs.has_quit) return;

	DEBUG(net, 6)("[NET][UDP] NewGRF data request from %s:%d", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));

	num_grfs = NetworkRecv_uint8 (&_udp_cs, p);
	if (num_grfs > NETWORK_MAX_GRF_COUNT) return;

	for (i = 0; i < num_grfs; i++) {
		GRFConfig c;
		const GRFConfig *f;

		NetworkRecv_GRFIdentifier(&_udp_cs, p, &c);

		/* Find the matching GRF file */
		f = FindGRFConfig(c.grfid, c.md5sum);
		if (f == NULL) continue; // The GRF is unknown to this server

		/* If the reply might exceed the size of the packet, only reply
		 * the current list and do not send the other data.
		 * The name could be an empty string, if so take the filename. */
		packet_len += sizeof(c.grfid) + sizeof(c.md5sum) +
				min(strlen((f->name != NULL && strlen(f->name) > 0) ? f->name : f->filename) + 1, NETWORK_GRF_NAME_LENGTH);
		if (packet_len > SEND_MTU - 4) { // 4 is 3 byte header + grf count in reply
			break;
		}
		in_reply[in_reply_count] = f;
		in_reply_count++;
	}

	if (in_reply_count == 0) return;

	packet = NetworkSend_Init(PACKET_UDP_SERVER_NEWGRFS);
	NetworkSend_uint8 (packet, in_reply_count);
	for (i = 0; i < in_reply_count; i++) {
		char name[NETWORK_GRF_NAME_LENGTH];

		/* The name could be an empty string, if so take the filename */
		ttd_strlcpy(name, (in_reply[i]->name != NULL && strlen(in_reply[i]->name) > 0) ?
				in_reply[i]->name : in_reply[i]->filename, sizeof(name));
	 	NetworkSend_GRFIdentifier(packet, in_reply[i]);
		NetworkSend_string(packet, name);
	}

	NetworkSendUDP_Packet(_udp_server_socket, packet, client_addr);
	free(packet);
}

/** The return of the client's request of the names of some NewGRFs */
DEF_UDP_RECEIVE_COMMAND(PACKET_UDP_SERVER_NEWGRFS)
{
	uint8 num_grfs;
	uint i;

	/* Just a fail-safe.. should never happen */
	if (_udp_cs.has_quit) return;

	DEBUG(net, 6)("[NET][UDP] NewGRF data reply from %s:%d", inet_ntoa(client_addr->sin_addr),ntohs(client_addr->sin_port));

	num_grfs = NetworkRecv_uint8 (&_udp_cs, p);
	if (num_grfs > NETWORK_MAX_GRF_COUNT) return;

	for (i = 0; i < num_grfs; i++) {
		char *unknown_name;
		char name[NETWORK_GRF_NAME_LENGTH];
		GRFConfig c;

		NetworkRecv_GRFIdentifier(&_udp_cs, p, &c);
		NetworkRecv_string(&_udp_cs, p, name, sizeof(name));

		/* An empty name is not possible under normal circumstances
		 * and causes problems when showing the NewGRF list. */
		if (strlen(name) == 0) continue;

		/* Finds the fake GRFConfig for the just read GRF ID and MD5sum tuple.
		 * If it exists and not resolved yet, then name of the fake GRF is
		 * overwritten with the name from the reply. */
		unknown_name = FindUnknownGRFName(c.grfid, c.md5sum, false);
		if (unknown_name != NULL && strcmp(unknown_name, UNKNOWN_GRF_NAME_PLACEHOLDER) == 0) {
			ttd_strlcpy(unknown_name, name, NETWORK_GRF_NAME_LENGTH);
		}
	}
}


// The layout for the receive-functions by UDP
typedef void NetworkUDPPacket(Packet *p, struct sockaddr_in *client_addr);

static NetworkUDPPacket* const _network_udp_packet[] = {
	RECEIVE_COMMAND(PACKET_UDP_CLIENT_FIND_SERVER),
	RECEIVE_COMMAND(PACKET_UDP_SERVER_RESPONSE),
	RECEIVE_COMMAND(PACKET_UDP_CLIENT_DETAIL_INFO),
	NULL,
	NULL,
	RECEIVE_COMMAND(PACKET_UDP_MASTER_ACK_REGISTER),
	NULL,
	RECEIVE_COMMAND(PACKET_UDP_MASTER_RESPONSE_LIST),
	NULL,
	RECEIVE_COMMAND(PACKET_UDP_CLIENT_GET_NEWGRFS),
	RECEIVE_COMMAND(PACKET_UDP_SERVER_NEWGRFS),
};


// If this fails, check the array above with network_data.h
assert_compile(lengthof(_network_udp_packet) == PACKET_UDP_END);


static void NetworkHandleUDPPacket(Packet* p, struct sockaddr_in* client_addr)
{
	byte type;

	/* Fake a client, so we can see when there is an illegal packet */
	_udp_cs.socket = INVALID_SOCKET;
	_udp_cs.has_quit = false;

	type = NetworkRecv_uint8(&_udp_cs, p);

	if (type < PACKET_UDP_END && _network_udp_packet[type] != NULL && !_udp_cs.has_quit) {
		_network_udp_packet[type](p, client_addr);
	} else {
		if (!_udp_cs.has_quit) {
			DEBUG(net, 0)("[NET][UDP] Received invalid packet type %d from %s:%d",
					type, inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
		} else {
			DEBUG(net, 0)("[NET][UDP] Received illegal packet from %s:%d",
					inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
		}
	}
}


// Send a packet over UDP
static void NetworkSendUDP_Packet(SOCKET udp, Packet* p, struct sockaddr_in* recv)
{
	int res;

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
bool NetworkUDPListen(SOCKET *udp, uint32 host, uint16 port, bool broadcast)
{
	struct sockaddr_in sin;

	// Make sure socket is closed
	closesocket(*udp);

	*udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (*udp == INVALID_SOCKET) {
		DEBUG(net, 1)("[NET][UDP] Failed to start UDP support");
		return false;
	}

	// set nonblocking mode for socket
	{
		unsigned long blocking = 1;
#ifndef BEOS_NET_SERVER
		ioctlsocket(*udp, FIONBIO, &blocking);
#else
		setsockopt(*udp, SOL_SOCKET, SO_NONBLOCK, &blocking, NULL);
#endif
	}

	sin.sin_family = AF_INET;
	// Listen on all IPs
	sin.sin_addr.s_addr = host;
	sin.sin_port = htons(port);

	if (bind(*udp, (struct sockaddr*)&sin, sizeof(sin)) != 0) {
		DEBUG(net, 1) ("[NET][UDP] error: bind failed on %s:%i", inet_ntoa(*(struct in_addr *)&host), port);
		return false;
	}

	if (broadcast) {
		/* Enable broadcast */
		unsigned long val = 1;
#ifndef BEOS_NET_SERVER // will work around this, some day; maybe.
		setsockopt(*udp, SOL_SOCKET, SO_BROADCAST, (char *) &val , sizeof(val));
#endif
	}

	DEBUG(net, 1)("[NET][UDP] Listening on port %s:%d", inet_ntoa(*(struct in_addr *)&host), port);

	return true;
}

// Close UDP connection
void NetworkUDPClose(void)
{
	DEBUG(net, 1) ("[NET][UDP] Closed listeners");

	if (_network_udp_server) {
		if (_udp_server_socket != INVALID_SOCKET) {
			closesocket(_udp_server_socket);
			_udp_server_socket = INVALID_SOCKET;
		}

		if (_udp_master_socket != INVALID_SOCKET) {
			closesocket(_udp_master_socket);
			_udp_master_socket = INVALID_SOCKET;
		}

		_network_udp_server = false;
		_network_udp_broadcast = 0;
	} else {
		if (_udp_client_socket != INVALID_SOCKET) {
			closesocket(_udp_client_socket);
			_udp_client_socket = INVALID_SOCKET;
		}
		_network_udp_broadcast = 0;
	}
}

// Receive something on UDP level
void NetworkUDPReceive(SOCKET udp)
{
	struct sockaddr_in client_addr;
	socklen_t client_len;
	int nbytes;
	static Packet *p = NULL;
	int packet_len;

	// If p is NULL, malloc him.. this prevents unneeded mallocs
	if (p == NULL) p = malloc(sizeof(*p));

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
static void NetworkUDPBroadCast(SOCKET udp)
{
	Packet* p = NetworkSend_Init(PACKET_UDP_CLIENT_FIND_SERVER);
	uint i;

	for (i = 0; _broadcast_list[i] != 0; i++) {
		struct sockaddr_in out_addr;

		out_addr.sin_family = AF_INET;
		out_addr.sin_port = htons(_network_server_port);
		out_addr.sin_addr.s_addr = _broadcast_list[i];

		DEBUG(net, 6)("[NET][UDP] Broadcasting to %s", inet_ntoa(out_addr.sin_addr));

		NetworkSendUDP_Packet(udp, p, &out_addr);
	}

	free(p);
}


// Request the the server-list from the master server
void NetworkUDPQueryMasterServer(void)
{
	struct sockaddr_in out_addr;
	Packet *p;

	if (_udp_client_socket == INVALID_SOCKET)
		if (!NetworkUDPListen(&_udp_client_socket, 0, 0, true))
			return;

	p = NetworkSend_Init(PACKET_UDP_CLIENT_GET_LIST);

	out_addr.sin_family = AF_INET;
	out_addr.sin_port = htons(NETWORK_MASTER_SERVER_PORT);
	out_addr.sin_addr.s_addr = NetworkResolveHost(NETWORK_MASTER_SERVER_HOST);

	// packet only contains protocol version
	NetworkSend_uint8(p, NETWORK_MASTER_SERVER_VERSION);

	NetworkSendUDP_Packet(_udp_client_socket, p, &out_addr);

	DEBUG(net, 2)("[NET][UDP] Queried Master Server at %s:%d", inet_ntoa(out_addr.sin_addr),ntohs(out_addr.sin_port));

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
		if (!NetworkUDPListen(&_udp_client_socket, 0, 0, true))
			return;

	DEBUG(net, 2)("[NET][UDP] Searching server");

	NetworkUDPBroadCast(_udp_client_socket);
	_network_udp_broadcast = 300; // Stay searching for 300 ticks
}

NetworkGameList *NetworkUDPQueryServer(const char* host, unsigned short port)
{
	struct sockaddr_in out_addr;
	Packet *p;
	NetworkGameList *item;

	// No UDP-socket yet..
	if (_udp_client_socket == INVALID_SOCKET)
		if (!NetworkUDPListen(&_udp_client_socket, 0, 0, true))
			return NULL;

	out_addr.sin_family = AF_INET;
	out_addr.sin_port = htons(port);
	out_addr.sin_addr.s_addr = NetworkResolveHost(host);

	// Clear item in gamelist
	item = NetworkGameListAddItem(inet_addr(inet_ntoa(out_addr.sin_addr)), ntohs(out_addr.sin_port));
	memset(&item->info, 0, sizeof(item->info));
	ttd_strlcpy(item->info.server_name, host, lengthof(item->info.server_name));
	ttd_strlcpy(item->info.hostname, host, lengthof(item->info.hostname));
	item->online = false;

	// Init the packet
	p = NetworkSend_Init(PACKET_UDP_CLIENT_FIND_SERVER);

	NetworkSendUDP_Packet(_udp_client_socket, p, &out_addr);

	free(p);

	UpdateNetworkGameWindow(false);
	return item;
}

/* Remove our advertise from the master-server */
void NetworkUDPRemoveAdvertise(void)
{
	struct sockaddr_in out_addr;
	Packet *p;

	/* Check if we are advertising */
	if (!_networking || !_network_server || !_network_udp_server)
		return;

	/* check for socket */
	if (_udp_master_socket == INVALID_SOCKET)
		if (!NetworkUDPListen(&_udp_master_socket, _network_server_bind_ip, 0, false))
			return;

	DEBUG(net, 2)("[NET][UDP] Removing advertise..");

	/* Find somewhere to send */
	out_addr.sin_family = AF_INET;
	out_addr.sin_port = htons(NETWORK_MASTER_SERVER_PORT);
	out_addr.sin_addr.s_addr = NetworkResolveHost(NETWORK_MASTER_SERVER_HOST);

	/* Send the packet */
	p = NetworkSend_Init(PACKET_UDP_SERVER_UNREGISTER);
	/* Packet is: Version, server_port */
	NetworkSend_uint8(p, NETWORK_MASTER_SERVER_VERSION);
	NetworkSend_uint16(p, _network_server_port);
	NetworkSendUDP_Packet(_udp_master_socket, p, &out_addr);

	free(p);
}

/* Register us to the master server
     This function checks if it needs to send an advertise */
void NetworkUDPAdvertise(void)
{
	struct sockaddr_in out_addr;
	Packet *p;

	/* Check if we should send an advertise */
	if (!_networking || !_network_server || !_network_udp_server || !_network_advertise)
		return;

	/* check for socket */
	if (_udp_master_socket == INVALID_SOCKET)
		if (!NetworkUDPListen(&_udp_master_socket, _network_server_bind_ip, 0, false))
			return;

	if (_network_need_advertise) {
		_network_need_advertise = false;
		_network_advertise_retries = ADVERTISE_RETRY_TIMES;
	} else {
		/* Only send once every ADVERTISE_NORMAL_INTERVAL ticks */
		if (_network_advertise_retries == 0) {
			if ((_network_last_advertise_frame + ADVERTISE_NORMAL_INTERVAL) > _frame_counter)
				return;
			_network_advertise_retries = ADVERTISE_RETRY_TIMES;
		}

		if ((_network_last_advertise_frame + ADVERTISE_RETRY_INTERVAL) > _frame_counter)
			return;
	}

	_network_advertise_retries--;
	_network_last_advertise_frame = _frame_counter;

	/* Find somewhere to send */
	out_addr.sin_family = AF_INET;
	out_addr.sin_port = htons(NETWORK_MASTER_SERVER_PORT);
	out_addr.sin_addr.s_addr = NetworkResolveHost(NETWORK_MASTER_SERVER_HOST);

	DEBUG(net, 1)("[NET][UDP] Advertising to master server");

	/* Send the packet */
	p = NetworkSend_Init(PACKET_UDP_SERVER_REGISTER);
	/* Packet is: WELCOME_MESSAGE, Version, server_port */
	NetworkSend_string(p, NETWORK_MASTER_SERVER_WELCOME_MESSAGE);
	NetworkSend_uint8(p, NETWORK_MASTER_SERVER_VERSION);
	NetworkSend_uint16(p, _network_server_port);
	NetworkSendUDP_Packet(_udp_master_socket, p, &out_addr);

	free(p);
}

void NetworkUDPInitialize(void)
{
	_udp_client_socket = INVALID_SOCKET;
	_udp_server_socket = INVALID_SOCKET;
	_udp_master_socket = INVALID_SOCKET;

	_network_udp_server = false;
	_network_udp_broadcast = 0;
}

#endif /* ENABLE_NETWORK */
