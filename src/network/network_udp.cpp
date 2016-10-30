/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

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
#include "../debug.h"
#include "network_gamelist.h"
#include "network_internal.h"
#include "network_udp.h"
#include "network.h"
#include "../core/endian_func.hpp"
#include "../company_base.h"
#include "../thread/thread.h"
#include "../rev.h"
#include "../newgrf_text.h"
#include "../strings_func.h"
#include "table/strings.h"

#include "core/udp.h"

#include "../safeguards.h"

/** Mutex for all out threaded udp resolution and such. */
static ThreadMutex *_network_udp_mutex = ThreadMutex::New();

/** Session key to register ourselves to the master server */
static uint64 _session_key = 0;

static const uint32 ADVERTISE_NORMAL_INTERVAL = 15 * 60 * 1000; ///< interval between advertising in ms (15 minutes)
static const uint32 ADVERTISE_RETRY_INTERVAL  =      10 * 1000; ///< re-advertise when no response after this many ms (10 seconds)
static const uint32 ADVERTISE_RETRY_TIMES     =              3; ///< give up re-advertising after this much failed retries

NetworkUDPSocketHandler *_udp_client_socket = NULL; ///< udp client socket
NetworkUDPSocketHandler *_udp_server_socket = NULL; ///< udp server socket
NetworkUDPSocketHandler *_udp_master_socket = NULL; ///< udp master socket

/** Simpler wrapper struct for NetworkUDPQueryServerThread */
struct NetworkUDPQueryServerInfo : NetworkAddress {
	bool manually; ///< Did we connect manually or not?

	/**
	 * Create the structure.
	 * @param address The address of the server to query.
	 * @param manually Whether the address was entered manually.
	 */
	NetworkUDPQueryServerInfo(const NetworkAddress &address, bool manually) :
		NetworkAddress(address),
		manually(manually)
	{
	}
};

/**
 * Helper function doing the actual work for querying the server.
 * @param address The address of the server.
 * @param needs_mutex Whether we need to acquire locks when sending the packet or not.
 * @param manually Whether the address was entered manually.
 */
static void NetworkUDPQueryServer(NetworkAddress *address, bool needs_mutex, bool manually)
{
	/* Clear item in gamelist */
	NetworkGameList *item = CallocT<NetworkGameList>(1);
	address->GetAddressAsString(item->info.server_name, lastof(item->info.server_name));
	strecpy(item->info.hostname, address->GetHostname(), lastof(item->info.hostname));
	item->address = *address;
	item->manually = manually;
	NetworkGameListAddItemDelayed(item);

	if (needs_mutex) _network_udp_mutex->BeginCritical();
	/* Init the packet */
	Packet p(PACKET_UDP_CLIENT_FIND_SERVER);
	if (_udp_client_socket != NULL) _udp_client_socket->SendPacket(&p, address);
	if (needs_mutex) _network_udp_mutex->EndCritical();
}

/**
 * Threaded part for resolving the IP of a server and querying it.
 * @param pntr the NetworkUDPQueryServerInfo.
 */
static void NetworkUDPQueryServerThread(void *pntr)
{
	NetworkUDPQueryServerInfo *info = (NetworkUDPQueryServerInfo*)pntr;
	NetworkUDPQueryServer(info, true, info->manually);

	delete info;
}

/**
 * Query a specific server.
 * @param address The address of the server.
 * @param manually Whether the address was entered manually.
 */
void NetworkUDPQueryServer(NetworkAddress address, bool manually)
{
	NetworkUDPQueryServerInfo *info = new NetworkUDPQueryServerInfo(address, manually);
	if (address.IsResolved() || !ThreadObject::New(NetworkUDPQueryServerThread, info, NULL, "ottd:udp-query")) {
		NetworkUDPQueryServerThread(info);
	}
}

///*** Communication with the masterserver ***/

/** Helper class for connecting to the master server. */
class MasterNetworkUDPSocketHandler : public NetworkUDPSocketHandler {
protected:
	virtual void Receive_MASTER_ACK_REGISTER(Packet *p, NetworkAddress *client_addr);
	virtual void Receive_MASTER_SESSION_KEY(Packet *p, NetworkAddress *client_addr);
public:
	/**
	 * Create the socket.
	 * @param addresses The addresses to bind on.
	 */
	MasterNetworkUDPSocketHandler(NetworkAddressList *addresses) : NetworkUDPSocketHandler(addresses) {}
	virtual ~MasterNetworkUDPSocketHandler() {}
};

void MasterNetworkUDPSocketHandler::Receive_MASTER_ACK_REGISTER(Packet *p, NetworkAddress *client_addr)
{
	_network_advertise_retries = 0;
	DEBUG(net, 2, "[udp] advertising on master server successful (%s)", NetworkAddress::AddressFamilyAsString(client_addr->GetAddress()->ss_family));

	/* We are advertised, but we don't want to! */
	if (!_settings_client.network.server_advertise) NetworkUDPRemoveAdvertise(false);
}

void MasterNetworkUDPSocketHandler::Receive_MASTER_SESSION_KEY(Packet *p, NetworkAddress *client_addr)
{
	_session_key = p->Recv_uint64();
	DEBUG(net, 2, "[udp] received new session key from master server (%s)", NetworkAddress::AddressFamilyAsString(client_addr->GetAddress()->ss_family));
}

///*** Communication with clients (we are server) ***/

/** Helper class for handling all server side communication. */
class ServerNetworkUDPSocketHandler : public NetworkUDPSocketHandler {
protected:
	virtual void Receive_CLIENT_FIND_SERVER(Packet *p, NetworkAddress *client_addr);
	virtual void Receive_CLIENT_DETAIL_INFO(Packet *p, NetworkAddress *client_addr);
	virtual void Receive_CLIENT_GET_NEWGRFS(Packet *p, NetworkAddress *client_addr);
public:
	/**
	 * Create the socket.
	 * @param addresses The addresses to bind on.
	 */
	ServerNetworkUDPSocketHandler(NetworkAddressList *addresses) : NetworkUDPSocketHandler(addresses) {}
	virtual ~ServerNetworkUDPSocketHandler() {}
};

void ServerNetworkUDPSocketHandler::Receive_CLIENT_FIND_SERVER(Packet *p, NetworkAddress *client_addr)
{
	/* Just a fail-safe.. should never happen */
	if (!_network_udp_server) {
		return;
	}

	NetworkGameInfo ngi;

	/* Update some game_info */
	ngi.clients_on     = _network_game_info.clients_on;
	ngi.start_date     = ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1);

	ngi.server_lang    = _settings_client.network.server_lang;
	ngi.use_password   = !StrEmpty(_settings_client.network.server_password);
	ngi.clients_max    = _settings_client.network.max_clients;
	ngi.companies_on   = (byte)Company::GetNumItems();
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
	this->SendNetworkGameInfo(&packet, &ngi);

	/* Let the client know that we are here */
	this->SendPacket(&packet, client_addr);

	DEBUG(net, 2, "[udp] queried from %s", client_addr->GetHostname());
}

void ServerNetworkUDPSocketHandler::Receive_CLIENT_DETAIL_INFO(Packet *p, NetworkAddress *client_addr)
{
	/* Just a fail-safe.. should never happen */
	if (!_network_udp_server) return;

	Packet packet(PACKET_UDP_SERVER_DETAIL_INFO);

	/* Send the amount of active companies */
	packet.Send_uint8 (NETWORK_COMPANY_INFO_VERSION);
	packet.Send_uint8 ((uint8)Company::GetNumItems());

	/* Fetch the latest version of the stats */
	NetworkCompanyStats company_stats[MAX_COMPANIES];
	NetworkPopulateCompanyStats(company_stats);

	/* The minimum company information "blob" size. */
	static const uint MIN_CI_SIZE = 54;
	uint max_cname_length = NETWORK_COMPANY_NAME_LENGTH;

	if (Company::GetNumItems() * (MIN_CI_SIZE + NETWORK_COMPANY_NAME_LENGTH) >= (uint)SEND_MTU - packet.size) {
		/* Assume we can at least put the company information in the packets. */
		assert(Company::GetNumItems() * MIN_CI_SIZE < (uint)SEND_MTU - packet.size);

		/* At this moment the company names might not fit in the
		 * packet. Check whether that is really the case. */

		for (;;) {
			int free = SEND_MTU - packet.size;
			Company *company;
			FOR_ALL_COMPANIES(company) {
				char company_name[NETWORK_COMPANY_NAME_LENGTH];
				SetDParam(0, company->index);
				GetString(company_name, STR_COMPANY_NAME, company_name + max_cname_length - 1);
				free -= MIN_CI_SIZE;
				free -= (int)strlen(company_name);
			}
			if (free >= 0) break;

			/* Try again, with slightly shorter strings. */
			assert(max_cname_length > 0);
			max_cname_length--;
		}
	}

	Company *company;
	/* Go through all the companies */
	FOR_ALL_COMPANIES(company) {
		/* Send the information */
		this->SendCompanyInformation(&packet, company, &company_stats[company->index], max_cname_length);
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
void ServerNetworkUDPSocketHandler::Receive_CLIENT_GET_NEWGRFS(Packet *p, NetworkAddress *client_addr)
{
	uint8 num_grfs;
	uint i;

	const GRFConfig *in_reply[NETWORK_MAX_GRF_COUNT];
	uint8 in_reply_count = 0;
	size_t packet_len = 0;

	DEBUG(net, 6, "[udp] newgrf data request from %s", client_addr->GetAddressAsString());

	num_grfs = p->Recv_uint8 ();
	if (num_grfs > NETWORK_MAX_GRF_COUNT) return;

	for (i = 0; i < num_grfs; i++) {
		GRFIdentifier c;
		const GRFConfig *f;

		this->ReceiveGRFIdentifier(p, &c);

		/* Find the matching GRF file */
		f = FindGRFConfig(c.grfid, FGCM_EXACT, c.md5sum);
		if (f == NULL) continue; // The GRF is unknown to this server

		/* If the reply might exceed the size of the packet, only reply
		 * the current list and do not send the other data.
		 * The name could be an empty string, if so take the filename. */
		packet_len += sizeof(c.grfid) + sizeof(c.md5sum) +
				min(strlen(f->GetName()) + 1, (size_t)NETWORK_GRF_NAME_LENGTH);
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
		strecpy(name, in_reply[i]->GetName(), lastof(name));
		this->SendGRFIdentifier(&packet, &in_reply[i]->ident);
		packet.Send_string(name);
	}

	this->SendPacket(&packet, client_addr);
}

///*** Communication with servers (we are client) ***/

/** Helper class for handling all client side communication. */
class ClientNetworkUDPSocketHandler : public NetworkUDPSocketHandler {
protected:
	virtual void Receive_SERVER_RESPONSE(Packet *p, NetworkAddress *client_addr);
	virtual void Receive_MASTER_RESPONSE_LIST(Packet *p, NetworkAddress *client_addr);
	virtual void Receive_SERVER_NEWGRFS(Packet *p, NetworkAddress *client_addr);
	virtual void HandleIncomingNetworkGameInfoGRFConfig(GRFConfig *config);
public:
	virtual ~ClientNetworkUDPSocketHandler() {}
};

void ClientNetworkUDPSocketHandler::Receive_SERVER_RESPONSE(Packet *p, NetworkAddress *client_addr)
{
	NetworkGameList *item;

	/* Just a fail-safe.. should never happen */
	if (_network_udp_server) return;

	DEBUG(net, 4, "[udp] server response from %s", client_addr->GetAddressAsString());

	/* Find next item */
	item = NetworkGameListAddItem(*client_addr);

	ClearGRFConfigList(&item->info.grfconfig);
	this->ReceiveNetworkGameInfo(p, &item->info);

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

		for (c = item->info.grfconfig; c != NULL; c = c->next) {
			if (c->status == GCS_NOT_FOUND) item->info.compatible = false;
			if (c->status != GCS_NOT_FOUND || strcmp(c->GetName(), UNKNOWN_GRF_NAME_PLACEHOLDER) != 0) continue;
			in_request[in_request_count] = c;
			in_request_count++;
		}

		if (in_request_count > 0) {
			/* There are 'unknown' GRFs, now send a request for them */
			uint i;
			Packet packet(PACKET_UDP_CLIENT_GET_NEWGRFS);

			packet.Send_uint8(in_request_count);
			for (i = 0; i < in_request_count; i++) {
				this->SendGRFIdentifier(&packet, &in_request[i]->ident);
			}

			this->SendPacket(&packet, &item->address);
		}
	}

	if (item->info.hostname[0] == '\0') {
		seprintf(item->info.hostname, lastof(item->info.hostname), "%s", client_addr->GetHostname());
	}

	if (client_addr->GetAddress()->ss_family == AF_INET6) {
		strecat(item->info.server_name, " (IPv6)", lastof(item->info.server_name));
	}

	/* Check if we are allowed on this server based on the revision-match */
	item->info.version_compatible = IsNetworkCompatibleVersion(item->info.server_revision);
	item->info.compatible &= item->info.version_compatible; // Already contains match for GRFs

	item->online = true;

	UpdateNetworkGameWindow();
}

void ClientNetworkUDPSocketHandler::Receive_MASTER_RESPONSE_LIST(Packet *p, NetworkAddress *client_addr)
{
	/* packet begins with the protocol version (uint8)
	 * then an uint16 which indicates how many
	 * ip:port pairs are in this packet, after that
	 * an uint32 (ip) and an uint16 (port) for each pair.
	 */

	ServerListType type = (ServerListType)(p->Recv_uint8() - 1);

	if (type < SLT_END) {
		for (int i = p->Recv_uint16(); i != 0 ; i--) {
			sockaddr_storage addr_storage;
			memset(&addr_storage, 0, sizeof(addr_storage));

			if (type == SLT_IPv4) {
				addr_storage.ss_family = AF_INET;
				((sockaddr_in*)&addr_storage)->sin_addr.s_addr = TO_LE32(p->Recv_uint32());
			} else {
				assert(type == SLT_IPv6);
				addr_storage.ss_family = AF_INET6;
				byte *addr = (byte*)&((sockaddr_in6*)&addr_storage)->sin6_addr;
				for (uint i = 0; i < sizeof(in6_addr); i++) *addr++ = p->Recv_uint8();
			}
			NetworkAddress addr(addr_storage, type == SLT_IPv4 ? sizeof(sockaddr_in) : sizeof(sockaddr_in6));
			addr.SetPort(p->Recv_uint16());

			/* Somehow we reached the end of the packet */
			if (this->HasClientQuit()) return;

			NetworkUDPQueryServer(&addr, false, false);
		}
	}
}

/** The return of the client's request of the names of some NewGRFs */
void ClientNetworkUDPSocketHandler::Receive_SERVER_NEWGRFS(Packet *p, NetworkAddress *client_addr)
{
	uint8 num_grfs;
	uint i;

	DEBUG(net, 6, "[udp] newgrf data reply from %s", client_addr->GetAddressAsString());

	num_grfs = p->Recv_uint8 ();
	if (num_grfs > NETWORK_MAX_GRF_COUNT) return;

	for (i = 0; i < num_grfs; i++) {
		char name[NETWORK_GRF_NAME_LENGTH];
		GRFIdentifier c;

		this->ReceiveGRFIdentifier(p, &c);
		p->Recv_string(name, sizeof(name));

		/* An empty name is not possible under normal circumstances
		 * and causes problems when showing the NewGRF list. */
		if (StrEmpty(name)) continue;

		/* Try to find the GRFTextWrapper for the name of this GRF ID and MD5sum tuple.
		 * If it exists and not resolved yet, then name of the fake GRF is
		 * overwritten with the name from the reply. */
		GRFTextWrapper *unknown_name = FindUnknownGRFName(c.grfid, c.md5sum, false);
		if (unknown_name != NULL && strcmp(GetGRFStringFromGRFText(unknown_name->text), UNKNOWN_GRF_NAME_PLACEHOLDER) == 0) {
			AddGRFTextToList(&unknown_name->text, name);
		}
	}
}

void ClientNetworkUDPSocketHandler::HandleIncomingNetworkGameInfoGRFConfig(GRFConfig *config)
{
	/* Find the matching GRF file */
	const GRFConfig *f = FindGRFConfig(config->ident.grfid, FGCM_EXACT, config->ident.md5sum);
	if (f == NULL) {
		/* Don't know the GRF, so mark game incompatible and the (possibly)
		 * already resolved name for this GRF (another server has sent the
		 * name of the GRF already */
		config->name->Release();
		config->name = FindUnknownGRFName(config->ident.grfid, config->ident.md5sum, true);
		config->name->AddRef();
		config->status = GCS_NOT_FOUND;
	} else {
		config->filename = f->filename;
		config->name->Release();
		config->name = f->name;
		config->name->AddRef();
		config->info->Release();
		config->info = f->info;
		config->info->AddRef();
		config->url->Release();
		config->url = f->url;
		config->url->AddRef();
	}
	SetBit(config->flags, GCF_COPY);
}

/** Broadcast to all ips */
static void NetworkUDPBroadCast(NetworkUDPSocketHandler *socket)
{
	for (NetworkAddress *addr = _broadcast_list.Begin(); addr != _broadcast_list.End(); addr++) {
		Packet p(PACKET_UDP_CLIENT_FIND_SERVER);

		DEBUG(net, 4, "[udp] broadcasting to %s", addr->GetHostname());

		socket->SendPacket(&p, addr, true, true);
	}
}


/** Request the the server-list from the master server */
void NetworkUDPQueryMasterServer()
{
	Packet p(PACKET_UDP_CLIENT_GET_LIST);
	NetworkAddress out_addr(NETWORK_MASTER_SERVER_HOST, NETWORK_MASTER_SERVER_PORT);

	/* packet only contains protocol version */
	p.Send_uint8(NETWORK_MASTER_SERVER_VERSION);
	p.Send_uint8(SLT_AUTODETECT);

	_udp_client_socket->SendPacket(&p, &out_addr, true);

	DEBUG(net, 2, "[udp] master server queried at %s", out_addr.GetAddressAsString());
}

/** Find all servers */
void NetworkUDPSearchGame()
{
	/* We are still searching.. */
	if (_network_udp_broadcast > 0) return;

	DEBUG(net, 0, "[udp] searching server");

	NetworkUDPBroadCast(_udp_client_socket);
	_network_udp_broadcast = 300; // Stay searching for 300 ticks
}

/**
 * Thread entry point for de-advertising.
 * @param pntr unused.
 */
static void NetworkUDPRemoveAdvertiseThread(void *pntr)
{
	DEBUG(net, 1, "[udp] removing advertise from master server");

	/* Find somewhere to send */
	NetworkAddress out_addr(NETWORK_MASTER_SERVER_HOST, NETWORK_MASTER_SERVER_PORT);

	/* Send the packet */
	Packet p(PACKET_UDP_SERVER_UNREGISTER);
	/* Packet is: Version, server_port */
	p.Send_uint8 (NETWORK_MASTER_SERVER_VERSION);
	p.Send_uint16(_settings_client.network.server_port);

	_network_udp_mutex->BeginCritical();
	if (_udp_master_socket != NULL) _udp_master_socket->SendPacket(&p, &out_addr, true);
	_network_udp_mutex->EndCritical();
}

/**
 * Remove our advertise from the master-server.
 * @param blocking whether to wait until the removal has finished.
 */
void NetworkUDPRemoveAdvertise(bool blocking)
{
	/* Check if we are advertising */
	if (!_networking || !_network_server || !_network_udp_server) return;

	if (blocking || !ThreadObject::New(NetworkUDPRemoveAdvertiseThread, NULL, NULL, "ottd:udp-advert")) {
		NetworkUDPRemoveAdvertiseThread(NULL);
	}
}

/**
 * Thread entry point for advertising.
 * @param pntr unused.
 */
static void NetworkUDPAdvertiseThread(void *pntr)
{
	/* Find somewhere to send */
	NetworkAddress out_addr(NETWORK_MASTER_SERVER_HOST, NETWORK_MASTER_SERVER_PORT);

	DEBUG(net, 1, "[udp] advertising to master server");

	/* Add a bit more messaging when we cannot get a session key */
	static byte session_key_retries = 0;
	if (_session_key == 0 && session_key_retries++ == 2) {
		DEBUG(net, 0, "[udp] advertising to the master server is failing");
		DEBUG(net, 0, "[udp]   we are not receiving the session key from the server");
		DEBUG(net, 0, "[udp]   please allow udp packets from %s to you to be delivered", out_addr.GetAddressAsString(false));
		DEBUG(net, 0, "[udp]   please allow udp packets from you to %s to be delivered", out_addr.GetAddressAsString(false));
	}
	if (_session_key != 0 && _network_advertise_retries == 0) {
		DEBUG(net, 0, "[udp] advertising to the master server is failing");
		DEBUG(net, 0, "[udp]   we are not receiving the acknowledgement from the server");
		DEBUG(net, 0, "[udp]   this usually means that the master server cannot reach us");
		DEBUG(net, 0, "[udp]   please allow udp and tcp packets to port %u to be delivered", _settings_client.network.server_port);
		DEBUG(net, 0, "[udp]   please allow udp and tcp packets from port %u to be delivered", _settings_client.network.server_port);
	}

	/* Send the packet */
	Packet p(PACKET_UDP_SERVER_REGISTER);
	/* Packet is: WELCOME_MESSAGE, Version, server_port */
	p.Send_string(NETWORK_MASTER_SERVER_WELCOME_MESSAGE);
	p.Send_uint8 (NETWORK_MASTER_SERVER_VERSION);
	p.Send_uint16(_settings_client.network.server_port);
	p.Send_uint64(_session_key);

	_network_udp_mutex->BeginCritical();
	if (_udp_master_socket != NULL) _udp_master_socket->SendPacket(&p, &out_addr, true);
	_network_udp_mutex->EndCritical();
}

/**
 * Register us to the master server
 *   This function checks if it needs to send an advertise
 */
void NetworkUDPAdvertise()
{
	static uint32 _last_advertisement = 0; ///< The time of the last advertisement (used to check for wrapping of time)
	static uint32 _next_advertisement = 0; ///< The next time we should perform a normal advertisement.
	static uint32 _next_retry         = 0; ///< The next time we should perform a retry of an advertisement.

	/* Check if we should send an advertise */
	if (!_networking || !_network_server || !_network_udp_server || !_settings_client.network.server_advertise) return;

	if (_network_need_advertise || _realtime_tick < _last_advertisement) {
		/* Forced advertisement, or a wrapping of time in which case we determine the advertisement/retry times again. */
		_network_need_advertise = false;
		_network_advertise_retries = ADVERTISE_RETRY_TIMES;
	} else {
		/* Only send once every ADVERTISE_NORMAL_INTERVAL ticks */
		if (_network_advertise_retries == 0) {
			if (_realtime_tick <= _next_advertisement) return;

			_network_advertise_retries = ADVERTISE_RETRY_TIMES;
		} else {
			/* An actual retry. */
			if (_realtime_tick <= _next_retry) return;
		}
	}

	_network_advertise_retries--;
	_last_advertisement = _realtime_tick;
	_next_advertisement = _realtime_tick + ADVERTISE_NORMAL_INTERVAL;
	_next_retry         = _realtime_tick + ADVERTISE_RETRY_INTERVAL;

	/* Make sure we do not have an overflow when checking these; when time wraps, we simply force an advertisement. */
	if (_next_advertisement < _last_advertisement) _next_advertisement = UINT32_MAX;
	if (_next_retry         < _last_advertisement) _next_retry         = UINT32_MAX;

	if (!ThreadObject::New(NetworkUDPAdvertiseThread, NULL, NULL, "ottd:udp-advert")) {
		NetworkUDPAdvertiseThread(NULL);
	}
}

/** Initialize the whole UDP bit. */
void NetworkUDPInitialize()
{
	/* If not closed, then do it. */
	if (_udp_server_socket != NULL) NetworkUDPClose();

	DEBUG(net, 1, "[udp] initializing listeners");
	assert(_udp_client_socket == NULL && _udp_server_socket == NULL && _udp_master_socket == NULL);

	_network_udp_mutex->BeginCritical();

	_udp_client_socket = new ClientNetworkUDPSocketHandler();

	NetworkAddressList server;
	GetBindAddresses(&server, _settings_client.network.server_port);
	_udp_server_socket = new ServerNetworkUDPSocketHandler(&server);

	server.Clear();
	GetBindAddresses(&server, 0);
	_udp_master_socket = new MasterNetworkUDPSocketHandler(&server);

	_network_udp_server = false;
	_network_udp_broadcast = 0;
	_network_udp_mutex->EndCritical();
}

/** Close all UDP related stuff. */
void NetworkUDPClose()
{
	_network_udp_mutex->BeginCritical();
	_udp_server_socket->Close();
	_udp_master_socket->Close();
	_udp_client_socket->Close();
	delete _udp_client_socket;
	delete _udp_server_socket;
	delete _udp_master_socket;
	_udp_client_socket = NULL;
	_udp_server_socket = NULL;
	_udp_master_socket = NULL;
	_network_udp_mutex->EndCritical();

	_network_udp_server = false;
	_network_udp_broadcast = 0;
	DEBUG(net, 1, "[udp] closed listeners");
}

/** Receive the UDP packets. */
void NetworkBackgroundUDPLoop()
{
	_network_udp_mutex->BeginCritical();

	if (_network_udp_server) {
		_udp_server_socket->ReceivePackets();
		_udp_master_socket->ReceivePackets();
	} else {
		_udp_client_socket->ReceivePackets();
		if (_network_udp_broadcast > 0) _network_udp_broadcast--;
	}

	_network_udp_mutex->EndCritical();
}

#endif /* ENABLE_NETWORK */
