/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file network_udp.cpp This file handles the UDP related communication.
 *
 * This is the GameServer <-> GameClient
 * communication before the game is being joined.
 */

#include "../stdafx.h"
#include "../date_func.h"
#include "../map_func.h"
#include "../debug.h"
#include "core/game_info.h"
#include "network_gamelist.h"
#include "network_internal.h"
#include "network_udp.h"
#include "network.h"
#include "../core/endian_func.hpp"
#include "../company_base.h"
#include "../thread.h"
#include "../rev.h"
#include "../newgrf_text.h"
#include "../strings_func.h"
#include "table/strings.h"
#include <mutex>

#include "core/udp.h"

#include "../safeguards.h"

static bool _network_udp_server;         ///< Is the UDP server started?
static uint16 _network_udp_broadcast;    ///< Timeout for the UDP broadcasts.

/** Some information about a socket, which exists before the actual socket has been created to provide locking and the likes. */
struct UDPSocket {
	const std::string name;                     ///< The name of the socket.
	std::mutex mutex;                           ///< Mutex for everything that (indirectly) touches the sockets within the handler.
	NetworkUDPSocketHandler *socket;            ///< The actual socket, which may be nullptr when not initialized yet.
	std::atomic<int> receive_iterations_locked; ///< The number of receive iterations the mutex was locked.

	UDPSocket(const std::string &name_) : name(name_), socket(nullptr) {}

	void CloseSocket()
	{
		std::lock_guard<std::mutex> lock(mutex);
		socket->CloseSocket();
		delete socket;
		socket = nullptr;
	}

	void ReceivePackets()
	{
		std::unique_lock<std::mutex> lock(mutex, std::defer_lock);
		if (!lock.try_lock()) {
			if (++receive_iterations_locked % 32 == 0) {
				Debug(net, 0, "{} background UDP loop processing appears to be blocked. Your OS may be low on UDP send buffers.", name);
			}
			return;
		}

		receive_iterations_locked.store(0);
		socket->ReceivePackets();
	}
};

static UDPSocket _udp_client("Client"); ///< udp client socket
static UDPSocket _udp_server("Server"); ///< udp server socket

/**
 * Helper function doing the actual work for querying the server.
 * @param connection_string The address of the server.
 * @param needs_mutex Whether we need to acquire locks when sending the packet or not.
 * @param manually Whether the address was entered manually.
 */
static void DoNetworkUDPQueryServer(const std::string &connection_string, bool needs_mutex, bool manually)
{
	/* Clear item in gamelist */
	NetworkGameList *item = new NetworkGameList(connection_string, manually);
	item->info.server_name = connection_string;
	NetworkGameListAddItemDelayed(item);

	std::unique_lock<std::mutex> lock(_udp_client.mutex, std::defer_lock);
	if (needs_mutex) lock.lock();
	/* Init the packet */
	NetworkAddress address = NetworkAddress(ParseConnectionString(connection_string, NETWORK_DEFAULT_PORT));
	Packet p(PACKET_UDP_CLIENT_FIND_SERVER);
	if (_udp_client.socket != nullptr) _udp_client.socket->SendPacket(&p, &address);
}

/**
 * Query a specific server.
 * @param connection_string The address of the server.
 * @param manually Whether the address was entered manually.
 */
void NetworkUDPQueryServer(const std::string &connection_string, bool manually)
{
	if (!StartNewThread(nullptr, "ottd:udp-query", &DoNetworkUDPQueryServer, std::move(connection_string), true, std::move(manually))) {
		DoNetworkUDPQueryServer(connection_string, true, manually);
	}
}

///*** Communication with clients (we are server) ***/

/** Helper class for handling all server side communication. */
class ServerNetworkUDPSocketHandler : public NetworkUDPSocketHandler {
protected:
	void Receive_CLIENT_FIND_SERVER(Packet *p, NetworkAddress *client_addr) override;
	void Receive_CLIENT_DETAIL_INFO(Packet *p, NetworkAddress *client_addr) override;
	void Receive_CLIENT_GET_NEWGRFS(Packet *p, NetworkAddress *client_addr) override;
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

	Packet packet(PACKET_UDP_SERVER_RESPONSE);
	SerializeNetworkGameInfo(&packet, GetCurrentNetworkServerGameInfo());

	/* Let the client know that we are here */
	this->SendPacket(&packet, client_addr);

	Debug(net, 7, "Queried from {}", client_addr->GetHostname());
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

	if (!packet.CanWriteToPacket(Company::GetNumItems() * (MIN_CI_SIZE + NETWORK_COMPANY_NAME_LENGTH))) {
		/* Assume we can at least put the company information in the packets. */
		assert(packet.CanWriteToPacket(Company::GetNumItems() * MIN_CI_SIZE));

		/* At this moment the company names might not fit in the
		 * packet. Check whether that is really the case. */

		for (;;) {
			size_t required = 0;
			for (const Company *company : Company::Iterate()) {
				char company_name[NETWORK_COMPANY_NAME_LENGTH];
				SetDParam(0, company->index);
				GetString(company_name, STR_COMPANY_NAME, company_name + max_cname_length - 1);
				required += MIN_CI_SIZE;
				required += strlen(company_name);
			}
			if (packet.CanWriteToPacket(required)) break;

			/* Try again, with slightly shorter strings. */
			assert(max_cname_length > 0);
			max_cname_length--;
		}
	}

	/* Go through all the companies */
	for (const Company *company : Company::Iterate()) {
		/* Send the information */
		this->SendCompanyInformation(&packet, company, &company_stats[company->index], max_cname_length);
	}

	this->SendPacket(&packet, client_addr);
}

/**
 * A client has requested the names of some NewGRFs.
 *
 * Replying this can be tricky as we have a limit of UDP_MTU bytes
 * in the reply packet and we can send up to 100 bytes per NewGRF
 * (GRF ID, MD5sum and NETWORK_GRF_NAME_LENGTH bytes for the name).
 * As UDP_MTU is _much_ less than 100 * NETWORK_MAX_GRF_COUNT, it
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

	Debug(net, 7, "NewGRF data request from {}", client_addr->GetAddressAsString());

	num_grfs = p->Recv_uint8 ();
	if (num_grfs > NETWORK_MAX_GRF_COUNT) return;

	for (i = 0; i < num_grfs; i++) {
		GRFIdentifier c;
		const GRFConfig *f;

		DeserializeGRFIdentifier(p, &c);

		/* Find the matching GRF file */
		f = FindGRFConfig(c.grfid, FGCM_EXACT, c.md5sum);
		if (f == nullptr) continue; // The GRF is unknown to this server

		/* If the reply might exceed the size of the packet, only reply
		 * the current list and do not send the other data.
		 * The name could be an empty string, if so take the filename. */
		packet_len += sizeof(c.grfid) + sizeof(c.md5sum) +
				std::min(strlen(f->GetName()) + 1, (size_t)NETWORK_GRF_NAME_LENGTH);
		if (packet_len > UDP_MTU - 4) { // 4 is 3 byte header + grf count in reply
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
		SerializeGRFIdentifier(&packet, &in_reply[i]->ident);
		packet.Send_string(name);
	}

	this->SendPacket(&packet, client_addr);
}

///*** Communication with servers (we are client) ***/

/** Helper class for handling all client side communication. */
class ClientNetworkUDPSocketHandler : public NetworkUDPSocketHandler {
protected:
	void Receive_SERVER_RESPONSE(Packet *p, NetworkAddress *client_addr) override;
	void Receive_MASTER_RESPONSE_LIST(Packet *p, NetworkAddress *client_addr) override;
	void Receive_SERVER_NEWGRFS(Packet *p, NetworkAddress *client_addr) override;
public:
	virtual ~ClientNetworkUDPSocketHandler() {}
};

void ClientNetworkUDPSocketHandler::Receive_SERVER_RESPONSE(Packet *p, NetworkAddress *client_addr)
{
	NetworkGameList *item;

	/* Just a fail-safe.. should never happen */
	if (_network_udp_server) return;

	Debug(net, 3, "Server response from {}", client_addr->GetAddressAsString());

	/* Find next item */
	item = NetworkGameListAddItem(client_addr->GetAddressAsString(false));

	/* Clear any existing GRFConfig chain. */
	ClearGRFConfigList(&item->info.grfconfig);
	/* Retrieve the NetworkGameInfo from the packet. */
	DeserializeNetworkGameInfo(p, &item->info);
	/* Check for compatability with the client. */
	CheckGameCompatibility(item->info);
	/* Ensure we consider the server online. */
	item->online = true;
	/* Make sure this entry never expires. */
	item->version = INT32_MAX;

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

		for (c = item->info.grfconfig; c != nullptr; c = c->next) {
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
				SerializeGRFIdentifier(&packet, &in_request[i]->ident);
			}

			NetworkAddress address = NetworkAddress(ParseConnectionString(item->connection_string, NETWORK_DEFAULT_PORT));
			this->SendPacket(&packet, &address);
		}
	}

	if (client_addr->GetAddress()->ss_family == AF_INET6) {
		item->info.server_name.append(" (IPv6)");
	}

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

			DoNetworkUDPQueryServer(addr.GetAddressAsString(false), false, false);
		}
	}
}

/** The return of the client's request of the names of some NewGRFs */
void ClientNetworkUDPSocketHandler::Receive_SERVER_NEWGRFS(Packet *p, NetworkAddress *client_addr)
{
	uint8 num_grfs;
	uint i;

	Debug(net, 7, "NewGRF data reply from {}", client_addr->GetAddressAsString());

	num_grfs = p->Recv_uint8 ();
	if (num_grfs > NETWORK_MAX_GRF_COUNT) return;

	for (i = 0; i < num_grfs; i++) {
		GRFIdentifier c;

		DeserializeGRFIdentifier(p, &c);
		std::string name = p->Recv_string(NETWORK_GRF_NAME_LENGTH);

		/* An empty name is not possible under normal circumstances
		 * and causes problems when showing the NewGRF list. */
		if (name.empty()) continue;

		/* Try to find the GRFTextWrapper for the name of this GRF ID and MD5sum tuple.
		 * If it exists and not resolved yet, then name of the fake GRF is
		 * overwritten with the name from the reply. */
		GRFTextWrapper unknown_name = FindUnknownGRFName(c.grfid, c.md5sum, false);
		if (unknown_name && strcmp(GetGRFStringFromGRFText(unknown_name), UNKNOWN_GRF_NAME_PLACEHOLDER) == 0) {
			AddGRFTextToList(unknown_name, name);
		}
	}
}

/** Broadcast to all ips */
static void NetworkUDPBroadCast(NetworkUDPSocketHandler *socket)
{
	for (NetworkAddress &addr : _broadcast_list) {
		Packet p(PACKET_UDP_CLIENT_FIND_SERVER);

		Debug(net, 5, "Broadcasting to {}", addr.GetHostname());

		socket->SendPacket(&p, &addr, true, true);
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

	std::lock_guard<std::mutex> lock(_udp_client.mutex);
	_udp_client.socket->SendPacket(&p, &out_addr, true);

	Debug(net, 6, "Master server queried at {}", out_addr.GetAddressAsString());
}

/** Find all servers */
void NetworkUDPSearchGame()
{
	/* We are still searching.. */
	if (_network_udp_broadcast > 0) return;

	Debug(net, 3, "Searching server");

	NetworkUDPBroadCast(_udp_client.socket);
	_network_udp_broadcast = 300; // Stay searching for 300 ticks
}

/** Initialize the whole UDP bit. */
void NetworkUDPInitialize()
{
	/* If not closed, then do it. */
	if (_udp_server.socket != nullptr) NetworkUDPClose();

	Debug(net, 3, "Initializing UDP listeners");
	assert(_udp_client.socket == nullptr && _udp_server.socket == nullptr);

	std::scoped_lock lock(_udp_client.mutex, _udp_server.mutex);

	_udp_client.socket = new ClientNetworkUDPSocketHandler();

	NetworkAddressList server;
	GetBindAddresses(&server, _settings_client.network.server_port);
	_udp_server.socket = new ServerNetworkUDPSocketHandler(&server);

	_network_udp_server = false;
	_network_udp_broadcast = 0;
}

/** Start the listening of the UDP server component. */
void NetworkUDPServerListen()
{
	std::lock_guard<std::mutex> lock(_udp_server.mutex);
	_network_udp_server = _udp_server.socket->Listen();
}

/** Close all UDP related stuff. */
void NetworkUDPClose()
{
	_udp_client.CloseSocket();
	_udp_server.CloseSocket();

	_network_udp_server = false;
	_network_udp_broadcast = 0;
	Debug(net, 5, "Closed UDP listeners");
}

/** Receive the UDP packets. */
void NetworkBackgroundUDPLoop()
{
	if (_network_udp_server) {
		_udp_server.ReceivePackets();
	} else {
		_udp_client.ReceivePackets();
		if (_network_udp_broadcast > 0) _network_udp_broadcast--;
	}
}
