/* $Id$ */

/**
 * @file network_udp.cpp This file handles the UDP related communication.
 *
 * This is the GameServer <-> MasterServer and GameServer <-> GameClient
 * communication before the game is being joined.
 */

#ifdef ENABLE_NETWORK

#include "../stdafx.h"
#include "../date_func.h"
#include "../map_func.h"
#include "network_gamelist.h"
#include "network_internal.h"
#include "network_udp.h"
#include "core/host.h"
#include "../core/endian_func.hpp"
#include "../core/alloc_func.hpp"
#include "../string_func.h"
#include "../company_base.h"
#include "../settings_type.h"
#include "../thread.h"
#include "../rev.h"

#include "core/udp.h"

ThreadMutex *_network_udp_mutex = ThreadMutex::New();

enum {
	ADVERTISE_NORMAL_INTERVAL = 30000, // interval between advertising in ticks (15 minutes)
	ADVERTISE_RETRY_INTERVAL  =   300, // readvertise when no response after this many ticks (9 seconds)
	ADVERTISE_RETRY_TIMES     =     3  // give up readvertising after this much failed retries
};

NetworkUDPSocketHandler *_udp_client_socket = NULL; ///< udp client socket
NetworkUDPSocketHandler *_udp_server_socket = NULL; ///< udp server socket
NetworkUDPSocketHandler *_udp_master_socket = NULL; ///< udp master socket

///*** Communication with the masterserver ***/

class MasterNetworkUDPSocketHandler : public NetworkUDPSocketHandler {
protected:
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_MASTER_ACK_REGISTER);
public:
	virtual ~MasterNetworkUDPSocketHandler() {}
};

DEF_UDP_RECEIVE_COMMAND(Master, PACKET_UDP_MASTER_ACK_REGISTER)
{
	_network_advertise_retries = 0;
	DEBUG(net, 2, "[udp] advertising on master server successful");

	/* We are advertised, but we don't want to! */
	if (!_settings_client.network.server_advertise) NetworkUDPRemoveAdvertise();
}

///*** Communication with clients (we are server) ***/

class ServerNetworkUDPSocketHandler : public NetworkUDPSocketHandler {
protected:
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_CLIENT_FIND_SERVER);
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_CLIENT_DETAIL_INFO);
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_CLIENT_GET_NEWGRFS);
public:
	virtual ~ServerNetworkUDPSocketHandler() {}
};

DEF_UDP_RECEIVE_COMMAND(Server, PACKET_UDP_CLIENT_FIND_SERVER)
{
	/* Just a fail-safe.. should never happen */
	if (!_network_udp_server) {
		return;
	}

	NetworkGameInfo ngi;

	/* Update some game_info */
	ngi.clients_on     = _network_game_info.clients_on;
	ngi.start_date     = _network_game_info.start_date;

	ngi.server_lang    = _settings_client.network.server_lang;
	ngi.use_password   = !StrEmpty(_settings_client.network.server_password);
	ngi.clients_max    = _settings_client.network.max_clients;
	ngi.companies_on   = ActiveCompanyCount();
	ngi.companies_max  = _settings_client.network.max_companies;
	ngi.spectators_on  = NetworkSpectatorCount();
	ngi.spectators_max = _settings_client.network.max_spectators;
	ngi.game_date      = _date;
	ngi.map_width      = MapSizeX();
	ngi.map_height     = MapSizeY();
	ngi.map_set        = _settings_game.game_creation.landscape;
	ngi.dedicated      = _network_dedicated;
	ngi.grfconfig      = _grfconfig;

	strecpy(ngi.map_name, _network_game_info.map_name, lastof(ngi.map_name));
	strecpy(ngi.server_name, _settings_client.network.server_name, lastof(ngi.server_name));
	strecpy(ngi.server_revision, _openttd_revision, lastof(ngi.server_revision));

	Packet packet(PACKET_UDP_SERVER_RESPONSE);
	this->Send_NetworkGameInfo(&packet, &ngi);

	/* Let the client know that we are here */
	this->SendPacket(&packet, client_addr);

	DEBUG(net, 2, "[udp] queried from '%s'", inet_ntoa(client_addr->sin_addr));
}

DEF_UDP_RECEIVE_COMMAND(Server, PACKET_UDP_CLIENT_DETAIL_INFO)
{
	/* Just a fail-safe.. should never happen */
	if (!_network_udp_server) return;

	Packet packet(PACKET_UDP_SERVER_DETAIL_INFO);

	/* Send the amount of active companies */
	packet.Send_uint8 (NETWORK_COMPANY_INFO_VERSION);
	packet.Send_uint8 (ActiveCompanyCount());

	/* Fetch the latest version of the stats */
	NetworkCompanyStats company_stats[MAX_COMPANIES];
	NetworkPopulateCompanyStats(company_stats);

	Company *company;
	/* Go through all the companies */
	FOR_ALL_COMPANIES(company) {
		/* Send the information */
		this->Send_CompanyInformation(&packet, company, &company_stats[company->index]);
	}

	this->SendPacket(&packet, client_addr);
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
DEF_UDP_RECEIVE_COMMAND(Server, PACKET_UDP_CLIENT_GET_NEWGRFS)
{
	uint8 num_grfs;
	uint i;

	const GRFConfig *in_reply[NETWORK_MAX_GRF_COUNT];
	uint8 in_reply_count = 0;
	size_t packet_len = 0;

	DEBUG(net, 6, "[udp] newgrf data request from %s:%d", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));

	num_grfs = p->Recv_uint8 ();
	if (num_grfs > NETWORK_MAX_GRF_COUNT) return;

	for (i = 0; i < num_grfs; i++) {
		GRFConfig c;
		const GRFConfig *f;

		this->Recv_GRFIdentifier(p, &c);

		/* Find the matching GRF file */
		f = FindGRFConfig(c.grfid, c.md5sum);
		if (f == NULL) continue; // The GRF is unknown to this server

		/* If the reply might exceed the size of the packet, only reply
		 * the current list and do not send the other data.
		 * The name could be an empty string, if so take the filename. */
		packet_len += sizeof(c.grfid) + sizeof(c.md5sum) +
				min(strlen((f->name != NULL && !StrEmpty(f->name)) ? f->name : f->filename) + 1, (size_t)NETWORK_GRF_NAME_LENGTH);
		if (packet_len > SEND_MTU - 4) { // 4 is 3 byte header + grf count in reply
			break;
		}
		in_reply[in_reply_count] = f;
		in_reply_count++;
	}

	if (in_reply_count == 0) return;

	Packet packet(PACKET_UDP_SERVER_NEWGRFS);
	packet.Send_uint8(in_reply_count);
	for (i = 0; i < in_reply_count; i++) {
		char name[NETWORK_GRF_NAME_LENGTH];

		/* The name could be an empty string, if so take the filename */
		strecpy(name, (in_reply[i]->name != NULL && !StrEmpty(in_reply[i]->name)) ?
				in_reply[i]->name : in_reply[i]->filename, lastof(name));
		this->Send_GRFIdentifier(&packet, in_reply[i]);
		packet.Send_string(name);
	}

	this->SendPacket(&packet, client_addr);
}

///*** Communication with servers (we are client) ***/

class ClientNetworkUDPSocketHandler : public NetworkUDPSocketHandler {
protected:
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_SERVER_RESPONSE);
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_MASTER_RESPONSE_LIST);
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_SERVER_NEWGRFS);
	virtual void HandleIncomingNetworkGameInfoGRFConfig(GRFConfig *config);
public:
	virtual ~ClientNetworkUDPSocketHandler() {}
};

DEF_UDP_RECEIVE_COMMAND(Client, PACKET_UDP_SERVER_RESPONSE)
{
	NetworkGameList *item;

	/* Just a fail-safe.. should never happen */
	if (_network_udp_server) return;

	DEBUG(net, 4, "[udp] server response from %s:%d", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));

	/* Find next item */
	item = NetworkGameListAddItem(inet_addr(inet_ntoa(client_addr->sin_addr)), ntohs(client_addr->sin_port));

	this->Recv_NetworkGameInfo(p, &item->info);

	item->info.compatible = true;
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
			if (c->status == GCS_NOT_FOUND) item->info.compatible = false;
			if (c->status != GCS_NOT_FOUND || strcmp(c->name, UNKNOWN_GRF_NAME_PLACEHOLDER) != 0) continue;
			in_request[in_request_count] = c;
			in_request_count++;
		}

		if (in_request_count > 0) {
			/* There are 'unknown' GRFs, now send a request for them */
			uint i;
			Packet packet(PACKET_UDP_CLIENT_GET_NEWGRFS);

			packet.Send_uint8(in_request_count);
			for (i = 0; i < in_request_count; i++) {
				this->Send_GRFIdentifier(&packet, in_request[i]);
			}

			out_addr.sin_family      = AF_INET;
			out_addr.sin_port        = htons(item->port);
			out_addr.sin_addr.s_addr = item->ip;
			this->SendPacket(&packet, &out_addr);
		}
	}

	if (item->info.hostname[0] == '\0')
		snprintf(item->info.hostname, sizeof(item->info.hostname), "%s", inet_ntoa(client_addr->sin_addr));

	/* Check if we are allowed on this server based on the revision-match */
	item->info.version_compatible = IsNetworkCompatibleVersion(item->info.server_revision);
	item->info.compatible &= item->info.version_compatible; // Already contains match for GRFs

	item->online = true;

	UpdateNetworkGameWindow(false);
}

DEF_UDP_RECEIVE_COMMAND(Client, PACKET_UDP_MASTER_RESPONSE_LIST)
{
	/* packet begins with the protocol version (uint8)
	 * then an uint16 which indicates how many
	 * ip:port pairs are in this packet, after that
	 * an uint32 (ip) and an uint16 (port) for each pair
	 */

	uint8 ver = p->Recv_uint8();

	if (ver == 1) {
		for (int i = p->Recv_uint16(); i != 0 ; i--) {
			uint32 ip = TO_LE32(p->Recv_uint32());
			uint16 port = p->Recv_uint16();

			/* Somehow we reached the end of the packet */
			if (this->HasClientQuit()) return;
			NetworkUDPQueryServer(NetworkAddress(ip, port));
		}
	}
}

/** The return of the client's request of the names of some NewGRFs */
DEF_UDP_RECEIVE_COMMAND(Client, PACKET_UDP_SERVER_NEWGRFS)
{
	uint8 num_grfs;
	uint i;

	DEBUG(net, 6, "[udp] newgrf data reply from %s:%d", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));

	num_grfs = p->Recv_uint8 ();
	if (num_grfs > NETWORK_MAX_GRF_COUNT) return;

	for (i = 0; i < num_grfs; i++) {
		char *unknown_name;
		char name[NETWORK_GRF_NAME_LENGTH];
		GRFConfig c;

		this->Recv_GRFIdentifier(p, &c);
		p->Recv_string(name, sizeof(name));

		/* An empty name is not possible under normal circumstances
		 * and causes problems when showing the NewGRF list. */
		if (StrEmpty(name)) continue;

		/* Finds the fake GRFConfig for the just read GRF ID and MD5sum tuple.
		 * If it exists and not resolved yet, then name of the fake GRF is
		 * overwritten with the name from the reply. */
		unknown_name = FindUnknownGRFName(c.grfid, c.md5sum, false);
		if (unknown_name != NULL && strcmp(unknown_name, UNKNOWN_GRF_NAME_PLACEHOLDER) == 0) {
			ttd_strlcpy(unknown_name, name, NETWORK_GRF_NAME_LENGTH);
		}
	}
}

void ClientNetworkUDPSocketHandler::HandleIncomingNetworkGameInfoGRFConfig(GRFConfig *config)
{
	/* Find the matching GRF file */
	const GRFConfig *f = FindGRFConfig(config->grfid, config->md5sum);
	if (f == NULL) {
		/* Don't know the GRF, so mark game incompatible and the (possibly)
		 * already resolved name for this GRF (another server has sent the
		 * name of the GRF already */
		config->name   = FindUnknownGRFName(config->grfid, config->md5sum, true);
		config->status = GCS_NOT_FOUND;
	} else {
		config->filename  = f->filename;
		config->name      = f->name;
		config->info      = f->info;
	}
	SetBit(config->flags, GCF_COPY);
}

/* Close UDP connection */
void NetworkUDPCloseAll()
{
	DEBUG(net, 1, "[udp] closed listeners");

	_network_udp_mutex->BeginCritical();
	_udp_server_socket->Close();
	_udp_master_socket->Close();
	_udp_client_socket->Close();
	_network_udp_mutex->EndCritical();

	_network_udp_server = false;
	_network_udp_broadcast = 0;
}

/* Broadcast to all ips */
static void NetworkUDPBroadCast(NetworkUDPSocketHandler *socket)
{
	uint i;

	for (i = 0; _broadcast_list[i] != 0; i++) {
		Packet p(PACKET_UDP_CLIENT_FIND_SERVER);
		struct sockaddr_in out_addr;

		out_addr.sin_family = AF_INET;
		out_addr.sin_port = htons(_settings_client.network.server_port);
		out_addr.sin_addr.s_addr = _broadcast_list[i];

		DEBUG(net, 4, "[udp] broadcasting to %s", inet_ntoa(out_addr.sin_addr));

		socket->SendPacket(&p, &out_addr);
	}
}


/* Request the the server-list from the master server */
void NetworkUDPQueryMasterServer()
{
	struct sockaddr_in out_addr;

	if (!_udp_client_socket->IsConnected()) {
		if (!_udp_client_socket->Listen(0, 0, true)) return;
	}

	Packet p(PACKET_UDP_CLIENT_GET_LIST);

	out_addr.sin_family = AF_INET;
	out_addr.sin_port = htons(NETWORK_MASTER_SERVER_PORT);
	out_addr.sin_addr.s_addr = NetworkResolveHost(NETWORK_MASTER_SERVER_HOST);

	/* packet only contains protocol version */
	p.Send_uint8(NETWORK_MASTER_SERVER_VERSION);

	_udp_client_socket->SendPacket(&p, &out_addr);

	DEBUG(net, 2, "[udp] master server queried at %s:%d", inet_ntoa(out_addr.sin_addr), ntohs(out_addr.sin_port));
}

/* Find all servers */
void NetworkUDPSearchGame()
{
	/* We are still searching.. */
	if (_network_udp_broadcast > 0) return;

	/* No UDP-socket yet.. */
	if (!_udp_client_socket->IsConnected()) {
		if (!_udp_client_socket->Listen(0, 0, true)) return;
	}

	DEBUG(net, 0, "[udp] searching server");

	NetworkUDPBroadCast(_udp_client_socket);
	_network_udp_broadcast = 300; // Stay searching for 300 ticks
}

/** Simpler wrapper struct for NetworkUDPQueryServerThread */
struct NetworkUDPQueryServerInfo : NetworkAddress {
	bool manually; ///< Did we connect manually or not?
	NetworkUDPQueryServerInfo(const NetworkAddress &address, bool manually) :
		NetworkAddress(address),
		manually(manually)
	{
	}
};

/**
 * Threaded part for resolving the IP of a server and querying it.
 * @param pntr the NetworkUDPQueryServerInfo.
 */
void NetworkUDPQueryServerThread(void *pntr)
{
	NetworkUDPQueryServerInfo *info = (NetworkUDPQueryServerInfo*)pntr;

	struct sockaddr_in out_addr;
	out_addr.sin_family = AF_INET;
	out_addr.sin_port = htons(info->GetPort());
	out_addr.sin_addr.s_addr = info->GetIP();

	/* Clear item in gamelist */
	NetworkGameList *item = CallocT<NetworkGameList>(1);
	item->ip = info->GetIP();
	item->port = info->GetPort();
	strecpy(item->info.server_name, info->GetHostname(), lastof(item->info.server_name));
	strecpy(item->info.hostname, info->GetHostname(), lastof(item->info.hostname));
	item->manually = info->manually;
	NetworkGameListAddItemDelayed(item);

	_network_udp_mutex->BeginCritical();
	/* Init the packet */
	Packet p(PACKET_UDP_CLIENT_FIND_SERVER);
	if (_udp_client_socket != NULL) _udp_client_socket->SendPacket(&p, &out_addr);
	_network_udp_mutex->EndCritical();

	delete info;
}

void NetworkUDPQueryServer(NetworkAddress address, bool manually)
{
	/* No UDP-socket yet.. */
	if (!_udp_client_socket->IsConnected()) {
		if (!_udp_client_socket->Listen(0, 0, true)) return;
	}

	NetworkUDPQueryServerInfo *info = new NetworkUDPQueryServerInfo(address, manually);
	if (address.IsResolved() || !ThreadObject::New(NetworkUDPQueryServerThread, info)) {
		NetworkUDPQueryServerThread(info);
	}
}

void NetworkUDPRemoveAdvertiseThread(void *pntr)
{
	DEBUG(net, 1, "[udp] removing advertise from master server");

	/* Find somewhere to send */
	struct sockaddr_in out_addr;
	out_addr.sin_family = AF_INET;
	out_addr.sin_port = htons(NETWORK_MASTER_SERVER_PORT);
	out_addr.sin_addr.s_addr = NetworkResolveHost(NETWORK_MASTER_SERVER_HOST);

	/* Send the packet */
	Packet p(PACKET_UDP_SERVER_UNREGISTER);
	/* Packet is: Version, server_port */
	p.Send_uint8 (NETWORK_MASTER_SERVER_VERSION);
	p.Send_uint16(_settings_client.network.server_port);

	_network_udp_mutex->BeginCritical();
	if (_udp_master_socket != NULL) _udp_master_socket->SendPacket(&p, &out_addr);
	_network_udp_mutex->EndCritical();
}

/* Remove our advertise from the master-server */
void NetworkUDPRemoveAdvertise()
{
	/* Check if we are advertising */
	if (!_networking || !_network_server || !_network_udp_server) return;

	/* check for socket */
	if (!_udp_master_socket->IsConnected()) {
		if (!_udp_master_socket->Listen(_network_server_bind_ip, 0, false)) return;
	}

	if (!ThreadObject::New(NetworkUDPRemoveAdvertiseThread, NULL)) {
		NetworkUDPRemoveAdvertiseThread(NULL);
	}
}

void NetworkUDPAdvertiseThread(void *pntr)
{
	/* Find somewhere to send */
	struct sockaddr_in out_addr;
	out_addr.sin_family = AF_INET;
	out_addr.sin_port = htons(NETWORK_MASTER_SERVER_PORT);
	out_addr.sin_addr.s_addr = NetworkResolveHost(NETWORK_MASTER_SERVER_HOST);

	DEBUG(net, 1, "[udp] advertising to master server");

	/* Send the packet */
	Packet p(PACKET_UDP_SERVER_REGISTER);
	/* Packet is: WELCOME_MESSAGE, Version, server_port */
	p.Send_string(NETWORK_MASTER_SERVER_WELCOME_MESSAGE);
	p.Send_uint8 (NETWORK_MASTER_SERVER_VERSION);
	p.Send_uint16(_settings_client.network.server_port);

	_network_udp_mutex->BeginCritical();
	if (_udp_master_socket != NULL) _udp_master_socket->SendPacket(&p, &out_addr);
	_network_udp_mutex->EndCritical();
}

/* Register us to the master server
     This function checks if it needs to send an advertise */
void NetworkUDPAdvertise()
{
	/* Check if we should send an advertise */
	if (!_networking || !_network_server || !_network_udp_server || !_settings_client.network.server_advertise)
		return;

	/* check for socket */
	if (!_udp_master_socket->IsConnected()) {
		if (!_udp_master_socket->Listen(_network_server_bind_ip, 0, false)) return;
	}

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

	if (!ThreadObject::New(NetworkUDPAdvertiseThread, NULL)) {
		NetworkUDPAdvertiseThread(NULL);
	}
}

void NetworkUDPInitialize()
{
	assert(_udp_client_socket == NULL && _udp_server_socket == NULL && _udp_master_socket == NULL);

	_network_udp_mutex->BeginCritical();
	_udp_client_socket = new ClientNetworkUDPSocketHandler();
	_udp_server_socket = new ServerNetworkUDPSocketHandler();
	_udp_master_socket = new MasterNetworkUDPSocketHandler();

	_network_udp_server = false;
	_network_udp_broadcast = 0;
	_network_udp_mutex->EndCritical();
}

void NetworkUDPShutdown()
{
	NetworkUDPCloseAll();

	_network_udp_mutex->BeginCritical();
	delete _udp_client_socket;
	delete _udp_server_socket;
	delete _udp_master_socket;
	_udp_client_socket = NULL;
	_udp_server_socket = NULL;
	_udp_master_socket = NULL;
	_network_udp_mutex->EndCritical();
}

#endif /* ENABLE_NETWORK */
