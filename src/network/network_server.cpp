/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_server.cpp Server part of the network protocol. */

#ifdef ENABLE_NETWORK

#include "../stdafx.h"
#include "../debug.h"
#include "../strings_func.h"
#include "../date_func.h"
#include "network_admin.h"
#include "network_server.h"
#include "network_udp.h"
#include "network.h"
#include "network_base.h"
#include "../console_func.h"
#include "../company_base.h"
#include "../command_func.h"
#include "../saveload/saveload.h"
#include "../saveload/saveload_filter.h"
#include "../station_base.h"
#include "../genworld.h"
#include "../fileio_func.h"
#include "../company_func.h"
#include "../company_gui.h"
#include "../window_func.h"
#include "../roadveh.h"
#include "../order_backup.h"
#include "../core/pool_func.hpp"
#include "../core/random_func.hpp"
#include "../rev.h"

#include "table/strings.h"

/* This file handles all the server-commands */

DECLARE_POSTFIX_INCREMENT(ClientID)
/** The identifier counter for new clients (is never decreased) */
static ClientID _network_client_id = CLIENT_ID_FIRST;

/** Make very sure the preconditions given in network_type.h are actually followed */
assert_compile(MAX_CLIENT_SLOTS > MAX_CLIENTS);
assert_compile(NetworkClientSocketPool::MAX_SIZE == MAX_CLIENT_SLOTS);

NetworkClientSocketPool _networkclientsocket_pool("NetworkClientSocket");
INSTANTIATE_POOL_METHODS(NetworkClientSocket)

/** Instantiate the listen sockets. */
template SocketList TCPListenHandler<ServerNetworkGameSocketHandler, PACKET_SERVER_FULL, PACKET_SERVER_BANNED>::sockets;

/** Writing a savegame directly to a number of packets. */
struct PacketWriter : SaveFilter {
	ServerNetworkGameSocketHandler *cs; ///< Socket we are associated with.
	Packet *current;                    ///< The packet we're currently writing to.
	size_t total_size;                  ///< Total size of the compressed savegame.

	/**
	 * Create the packet writer.
	 * @param cs The socket handler we're making the packets for.
	 */
	PacketWriter(ServerNetworkGameSocketHandler *cs) : SaveFilter(NULL), cs(cs), current(NULL), total_size(0)
	{
		this->cs->savegame_mutex = ThreadMutex::New();
	}

	/** Make sure everything is cleaned up. */
	~PacketWriter()
	{

		/* Prevent double frees. */
		if (this->cs != NULL) {
			if (this->cs->savegame_mutex != NULL) this->cs->savegame_mutex->BeginCritical();
			this->cs->savegame = NULL;
			if (this->cs->savegame_mutex != NULL) this->cs->savegame_mutex->EndCritical();

			delete this->cs->savegame_mutex;
			this->cs->savegame_mutex = NULL;
		}

		delete this->current;
	}

	/** Append the current packet to the queue. */
	void AppendQueue()
	{
		if (this->current == NULL) return;

		Packet **p = &this->cs->savegame_packets;
		while (*p != NULL) {
			p = &(*p)->next;
		}
		*p = this->current;

		this->current = NULL;
	}

	/* virtual */ void Write(byte *buf, size_t size)
	{
		if (this->cs == NULL) return;

		if (this->current == NULL) this->current = new Packet(PACKET_SERVER_MAP_DATA);

		if (this->cs->savegame_mutex != NULL) this->cs->savegame_mutex->BeginCritical();

		byte *bufe = buf + size;
		while (buf != bufe) {
			size_t to_write = min(SEND_MTU - this->current->size, bufe - buf);
			memcpy(this->current->buffer + this->current->size, buf, to_write);
			this->current->size += (PacketSize)to_write;
			buf += to_write;

			if (this->current->size == SEND_MTU) {
				this->AppendQueue();
				if (buf != bufe) this->current = new Packet(PACKET_SERVER_MAP_DATA);
			}
		}

		if (this->cs->savegame_mutex != NULL) this->cs->savegame_mutex->EndCritical();

		this->total_size += size;
	}

	/* virtual */ void Finish()
	{
		if (this->cs == NULL) return;

		if (this->cs->savegame_mutex != NULL) this->cs->savegame_mutex->BeginCritical();

		/* Make sure the last packet is flushed. */
		this->AppendQueue();

		/* Add a packet stating that this is the end to the queue. */
		this->current = new Packet(PACKET_SERVER_MAP_DONE);
		this->AppendQueue();

		/* Fast-track the size to the client. */
		Packet *p = new Packet(PACKET_SERVER_MAP_SIZE);
		p->Send_uint32((uint32)this->total_size);
		this->cs->NetworkTCPSocketHandler::SendPacket(p);

		if (this->cs->savegame_mutex != NULL) this->cs->savegame_mutex->EndCritical();
	}
};


/**
 * Create a new socket for the server side of the game connection.
 * @param s The socket to connect with.
 */
ServerNetworkGameSocketHandler::ServerNetworkGameSocketHandler(SOCKET s) : NetworkGameSocketHandler(s)
{
	this->status = STATUS_INACTIVE;
	this->client_id = _network_client_id++;
	this->receive_limit = _settings_client.network.bytes_per_frame_burst;
	NetworkClientInfo *ci = new NetworkClientInfo(this->client_id);
	this->SetInfo(ci);
	ci->client_playas = COMPANY_INACTIVE_CLIENT;
	ci->join_date = _date;
}

/**
 * Clear everything related to this client.
 */
ServerNetworkGameSocketHandler::~ServerNetworkGameSocketHandler()
{
	if (_redirect_console_to_client == this->client_id) _redirect_console_to_client = INVALID_CLIENT_ID;
	OrderBackup::ResetUser(this->client_id);

	if (this->savegame_mutex != NULL) this->savegame_mutex->BeginCritical();
	delete this->savegame_packets;
	if (this->savegame != NULL) this->savegame->cs = NULL;

	if (this->savegame_mutex != NULL) this->savegame_mutex->EndCritical();
	delete this->savegame_mutex;
}

Packet *ServerNetworkGameSocketHandler::ReceivePacket()
{
	/* Only allow receiving when we have some buffer free; this value
	 * can go negative, but eventually it will become positive again. */
	if (this->receive_limit <= 0) return NULL;

	/* We can receive a packet, so try that and if needed account for
	 * the amount of received data. */
	Packet *p = this->NetworkTCPSocketHandler::ReceivePacket();
	if (p != NULL) this->receive_limit -= p->size;
	return p;
}

void ServerNetworkGameSocketHandler::SendPacket(Packet *packet)
{
	if (this->savegame_mutex != NULL) this->savegame_mutex->BeginCritical();
	this->NetworkTCPSocketHandler::SendPacket(packet);
	if (this->savegame_mutex != NULL) this->savegame_mutex->EndCritical();
}

NetworkRecvStatus ServerNetworkGameSocketHandler::CloseConnection(NetworkRecvStatus status)
{
	assert(status != NETWORK_RECV_STATUS_OKAY);
	/*
	 * Sending a message just before leaving the game calls cs->SendPackets.
	 * This might invoke this function, which means that when we close the
	 * connection after cs->SendPackets we will close an already closed
	 * connection. This handles that case gracefully without having to make
	 * that code any more complex or more aware of the validity of the socket.
	 */
	if (this->sock == INVALID_SOCKET) return status;

	if (status != NETWORK_RECV_STATUS_CONN_LOST && !this->HasClientQuit() && this->status >= STATUS_AUTHORIZED) {
		/* We did not receive a leave message from this client... */
		char client_name[NETWORK_CLIENT_NAME_LENGTH];
		NetworkClientSocket *new_cs;

		this->GetClientName(client_name, sizeof(client_name));

		NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, client_name, NULL, STR_NETWORK_ERROR_CLIENT_CONNECTION_LOST);

		/* Inform other clients of this... strange leaving ;) */
		FOR_ALL_CLIENT_SOCKETS(new_cs) {
			if (new_cs->status > STATUS_AUTHORIZED && this != new_cs) {
				new_cs->SendErrorQuit(this->client_id, NETWORK_ERROR_CONNECTION_LOST);
			}
		}
	}

	DEBUG(net, 1, "Closed client connection %d", this->client_id);

	/* We just lost one client :( */
	if (this->status >= STATUS_AUTHORIZED) _network_game_info.clients_on--;
	extern byte _network_clients_connected;
	_network_clients_connected--;

	SetWindowDirty(WC_CLIENT_LIST, 0);

	this->SendPackets(true);

	delete this->GetInfo();
	delete this;

	return status;
}

/**
 * Whether an connection is allowed or not at this moment.
 * @return true if the connection is allowed.
 */
/* static */ bool ServerNetworkGameSocketHandler::AllowConnection()
{
	extern byte _network_clients_connected;
	return _network_clients_connected < MAX_CLIENTS && _network_game_info.clients_on < _settings_client.network.max_clients;
}

/** Send the packets for the server sockets. */
/* static */ void ServerNetworkGameSocketHandler::Send()
{
	NetworkClientSocket *cs;
	FOR_ALL_CLIENT_SOCKETS(cs) {
		if (cs->writable) {
			if (cs->SendPackets() && cs->status == STATUS_MAP) {
				/* This client is in the middle of a map-send, call the function for that */
				cs->SendMap();
			}
		}
	}
}

static void NetworkHandleCommandQueue(NetworkClientSocket *cs);

/***********
 * Sending functions
 *   DEF_SERVER_SEND_COMMAND has parameter: NetworkClientSocket *cs
 ************/

NetworkRecvStatus ServerNetworkGameSocketHandler::SendClientInfo(NetworkClientInfo *ci)
{
	if (ci->client_id != INVALID_CLIENT_ID) {
		Packet *p = new Packet(PACKET_SERVER_CLIENT_INFO);
		p->Send_uint32(ci->client_id);
		p->Send_uint8 (ci->client_playas);
		p->Send_string(ci->client_name);

		this->SendPacket(p);
	}
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendCompanyInfo()
{
	/* Fetch the latest version of the stats */
	NetworkCompanyStats company_stats[MAX_COMPANIES];
	NetworkPopulateCompanyStats(company_stats);

	/* Make a list of all clients per company */
	char clients[MAX_COMPANIES][NETWORK_CLIENTS_LENGTH];
	NetworkClientSocket *csi;
	memset(clients, 0, sizeof(clients));

	/* Add the local player (if not dedicated) */
	const NetworkClientInfo *ci = NetworkFindClientInfoFromClientID(CLIENT_ID_SERVER);
	if (ci != NULL && Company::IsValidID(ci->client_playas)) {
		strecpy(clients[ci->client_playas], ci->client_name, lastof(clients[ci->client_playas]));
	}

	FOR_ALL_CLIENT_SOCKETS(csi) {
		char client_name[NETWORK_CLIENT_NAME_LENGTH];

		((ServerNetworkGameSocketHandler*)csi)->GetClientName(client_name, sizeof(client_name));

		ci = csi->GetInfo();
		if (ci != NULL && Company::IsValidID(ci->client_playas)) {
			if (!StrEmpty(clients[ci->client_playas])) {
				strecat(clients[ci->client_playas], ", ", lastof(clients[ci->client_playas]));
			}

			strecat(clients[ci->client_playas], client_name, lastof(clients[ci->client_playas]));
		}
	}

	/* Now send the data */

	Company *company;
	Packet *p;

	FOR_ALL_COMPANIES(company) {
		p = new Packet(PACKET_SERVER_COMPANY_INFO);

		p->Send_uint8 (NETWORK_COMPANY_INFO_VERSION);
		p->Send_bool  (true);
		this->SendCompanyInformation(p, company, &company_stats[company->index]);

		if (StrEmpty(clients[company->index])) {
			p->Send_string("<none>");
		} else {
			p->Send_string(clients[company->index]);
		}

		this->SendPacket(p);
	}

	p = new Packet(PACKET_SERVER_COMPANY_INFO);

	p->Send_uint8 (NETWORK_COMPANY_INFO_VERSION);
	p->Send_bool  (false);

	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendError(NetworkErrorCode error)
{
	char str[100];
	Packet *p = new Packet(PACKET_SERVER_ERROR);

	p->Send_uint8(error);
	this->SendPacket(p);

	StringID strid = GetNetworkErrorMsg(error);
	GetString(str, strid, lastof(str));

	/* Only send when the current client was in game */
	if (this->status > STATUS_AUTHORIZED) {
		NetworkClientSocket *new_cs;
		char client_name[NETWORK_CLIENT_NAME_LENGTH];

		this->GetClientName(client_name, sizeof(client_name));

		DEBUG(net, 1, "'%s' made an error and has been disconnected. Reason: '%s'", client_name, str);

		NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, client_name, NULL, strid);

		FOR_ALL_CLIENT_SOCKETS(new_cs) {
			if (new_cs->status > STATUS_AUTHORIZED && new_cs != this) {
				/* Some errors we filter to a more general error. Clients don't have to know the real
				 *  reason a joining failed. */
				if (error == NETWORK_ERROR_NOT_AUTHORIZED || error == NETWORK_ERROR_NOT_EXPECTED || error == NETWORK_ERROR_WRONG_REVISION) {
					error = NETWORK_ERROR_ILLEGAL_PACKET;
				}
				new_cs->SendErrorQuit(this->client_id, error);
			}
		}

		NetworkAdminClientError(this->client_id, error);
	} else {
		DEBUG(net, 1, "Client %d made an error and has been disconnected. Reason: '%s'", this->client_id, str);
	}

	/* The client made a mistake, so drop his connection now! */
	return this->CloseConnection(NETWORK_RECV_STATUS_SERVER_ERROR);
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendNewGRFCheck()
{
	Packet *p = new Packet(PACKET_SERVER_CHECK_NEWGRFS);
	const GRFConfig *c;
	uint grf_count = 0;

	for (c = _grfconfig; c != NULL; c = c->next) {
		if (!HasBit(c->flags, GCF_STATIC)) grf_count++;
	}

	p->Send_uint8 (grf_count);
	for (c = _grfconfig; c != NULL; c = c->next) {
		if (!HasBit(c->flags, GCF_STATIC)) this->SendGRFIdentifier(p, &c->ident);
	}

	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendNeedGamePassword()
{
	/* Invalid packet when status is STATUS_AUTH_GAME or higher */
	if (this->status >= STATUS_AUTH_GAME) return this->CloseConnection(NETWORK_RECV_STATUS_MALFORMED_PACKET);

	this->status = STATUS_AUTH_GAME;

	Packet *p = new Packet(PACKET_SERVER_NEED_GAME_PASSWORD);
	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendNeedCompanyPassword()
{
	/* Invalid packet when status is STATUS_AUTH_COMPANY or higher */
	if (this->status >= STATUS_AUTH_COMPANY) return this->CloseConnection(NETWORK_RECV_STATUS_MALFORMED_PACKET);

	this->status = STATUS_AUTH_COMPANY;

	Packet *p = new Packet(PACKET_SERVER_NEED_COMPANY_PASSWORD);
	p->Send_uint32(_settings_game.game_creation.generation_seed);
	p->Send_string(_settings_client.network.network_id);
	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendWelcome()
{
	Packet *p;
	NetworkClientSocket *new_cs;

	/* Invalid packet when status is AUTH or higher */
	if (this->status >= STATUS_AUTHORIZED) return this->CloseConnection(NETWORK_RECV_STATUS_MALFORMED_PACKET);

	this->status = STATUS_AUTHORIZED;
	_network_game_info.clients_on++;

	p = new Packet(PACKET_SERVER_WELCOME);
	p->Send_uint32(this->client_id);
	p->Send_uint32(_settings_game.game_creation.generation_seed);
	p->Send_string(_settings_client.network.network_id);
	this->SendPacket(p);

	/* Transmit info about all the active clients */
	FOR_ALL_CLIENT_SOCKETS(new_cs) {
		if (new_cs != this && new_cs->status > STATUS_AUTHORIZED) {
			this->SendClientInfo(new_cs->GetInfo());
		}
	}
	/* Also send the info of the server */
	return this->SendClientInfo(NetworkFindClientInfoFromClientID(CLIENT_ID_SERVER));
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendWait()
{
	int waiting = 0;
	NetworkClientSocket *new_cs;
	Packet *p;

	/* Count how many clients are waiting in the queue */
	FOR_ALL_CLIENT_SOCKETS(new_cs) {
		if (new_cs->status == STATUS_MAP_WAIT) waiting++;
	}

	p = new Packet(PACKET_SERVER_WAIT);
	p->Send_uint8(waiting);
	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

/* This sends the map to the client */
NetworkRecvStatus ServerNetworkGameSocketHandler::SendMap()
{
	static uint sent_packets; // How many packets we did send succecfully last time

	if (this->status < STATUS_AUTHORIZED) {
		/* Illegal call, return error and ignore the packet */
		return this->SendError(NETWORK_ERROR_NOT_AUTHORIZED);
	}

	if (this->status == STATUS_AUTHORIZED) {
		this->savegame = new PacketWriter(this);

		/* Now send the _frame_counter and how many packets are coming */
		Packet *p = new Packet(PACKET_SERVER_MAP_BEGIN);
		p->Send_uint32(_frame_counter);
		this->SendPacket(p);

		NetworkSyncCommandQueue(this);
		this->status = STATUS_MAP;
		/* Mark the start of download */
		this->last_frame = _frame_counter;
		this->last_frame_server = _frame_counter;

		sent_packets = 4; // We start with trying 4 packets

		/* Make a dump of the current game */
		if (SaveWithFilter(this->savegame, true) != SL_OK) usererror("network savedump failed");
	}

	if (this->status == STATUS_MAP) {
		if (this->savegame_mutex != NULL) this->savegame_mutex->BeginCritical();

		for (uint i = 0; i < sent_packets && this->savegame_packets != NULL; i++) {
			Packet *p = this->savegame_packets;
			bool last_packet = p->buffer[2] == PACKET_SERVER_MAP_DONE;

			/* Remove the packet from the savegame queue and put it in the real queue. */
			this->savegame_packets = p->next;
			p->next = NULL;
			this->NetworkTCPSocketHandler::SendPacket(p);

			if (last_packet) {
				/* Done reading! */

				/* Set the status to DONE_MAP, no we will wait for the client
				 *  to send it is ready (maybe that happens like never ;)) */
				this->status = STATUS_DONE_MAP;

				NetworkClientSocket *new_cs;
				bool new_map_client = false;
				/* Check if there is a client waiting for receiving the map
				 *  and start sending him the map */
				FOR_ALL_CLIENT_SOCKETS(new_cs) {
					if (new_cs->status == STATUS_MAP_WAIT) {
						/* Check if we already have a new client to send the map to */
						if (!new_map_client) {
							/* If not, this client will get the map */
							new_cs->status = STATUS_AUTHORIZED;
							new_map_client = true;
							new_cs->SendMap();
						} else {
							/* Else, send the other clients how many clients are in front of them */
							new_cs->SendWait();
						}
					}
				}

				/* There is no more data, so break the for */
				break;
			}
		}

		/* Send all packets (forced) and check if we have send it all */
		if (this->SendPackets() && this->IsPacketQueueEmpty()) {
			/* All are sent, increase the sent_packets */
			if (this->savegame_packets != NULL) sent_packets *= 2;
		} else {
			/* Not everything is sent, decrease the sent_packets */
			if (sent_packets > 1) sent_packets /= 2;
		}

		if (this->savegame_mutex != NULL) this->savegame_mutex->EndCritical();
	}
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendJoin(ClientID client_id)
{
	Packet *p = new Packet(PACKET_SERVER_JOIN);

	p->Send_uint32(client_id);

	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}


NetworkRecvStatus ServerNetworkGameSocketHandler::SendFrame()
{
	Packet *p = new Packet(PACKET_SERVER_FRAME);
	p->Send_uint32(_frame_counter);
	p->Send_uint32(_frame_counter_max);
#ifdef ENABLE_NETWORK_SYNC_EVERY_FRAME
	p->Send_uint32(_sync_seed_1);
#ifdef NETWORK_SEND_DOUBLE_SEED
	p->Send_uint32(_sync_seed_2);
#endif
#endif

	/* If token equals 0, we need to make a new token and send that. */
	if (this->last_token == 0) {
		this->last_token = InteractiveRandomRange(UINT8_MAX - 1) + 1;
		p->Send_uint8(this->last_token);
	}

	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendSync()
{
	Packet *p = new Packet(PACKET_SERVER_SYNC);
	p->Send_uint32(_frame_counter);
	p->Send_uint32(_sync_seed_1);

#ifdef NETWORK_SEND_DOUBLE_SEED
	p->Send_uint32(_sync_seed_2);
#endif
	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendCommand(const CommandPacket *cp)
{
	Packet *p = new Packet(PACKET_SERVER_COMMAND);

	this->NetworkGameSocketHandler::SendCommand(p, cp);
	p->Send_uint32(cp->frame);
	p->Send_bool  (cp->my_cmd);

	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendChat(NetworkAction action, ClientID client_id, bool self_send, const char *msg, int64 data)
{
	Packet *p = new Packet(PACKET_SERVER_CHAT);

	p->Send_uint8 (action);
	p->Send_uint32(client_id);
	p->Send_bool  (self_send);
	p->Send_string(msg);
	p->Send_uint64(data);

	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendErrorQuit(ClientID client_id, NetworkErrorCode errorno)
{
	Packet *p = new Packet(PACKET_SERVER_ERROR_QUIT);

	p->Send_uint32(client_id);
	p->Send_uint8 (errorno);

	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendQuit(ClientID client_id)
{
	Packet *p = new Packet(PACKET_SERVER_QUIT);

	p->Send_uint32(client_id);

	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendShutdown()
{
	Packet *p = new Packet(PACKET_SERVER_SHUTDOWN);
	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendNewGame()
{
	Packet *p = new Packet(PACKET_SERVER_NEWGAME);
	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendRConResult(uint16 colour, const char *command)
{
	Packet *p = new Packet(PACKET_SERVER_RCON);

	p->Send_uint16(colour);
	p->Send_string(command);
	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendMove(ClientID client_id, CompanyID company_id)
{
	Packet *p = new Packet(PACKET_SERVER_MOVE);

	p->Send_uint32(client_id);
	p->Send_uint8(company_id);
	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendCompanyUpdate()
{
	Packet *p = new Packet(PACKET_SERVER_COMPANY_UPDATE);

	p->Send_uint16(_network_company_passworded);
	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ServerNetworkGameSocketHandler::SendConfigUpdate()
{
	Packet *p = new Packet(PACKET_SERVER_CONFIG_UPDATE);

	p->Send_uint8(_settings_client.network.max_companies);
	p->Send_uint8(_settings_client.network.max_spectators);
	this->SendPacket(p);
	return NETWORK_RECV_STATUS_OKAY;
}

/***********
 * Receiving functions
 *   DEF_SERVER_RECEIVE_COMMAND has parameter: NetworkClientSocket *cs, Packet *p
 ************/

DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_COMPANY_INFO)
{
	return this->SendCompanyInfo();
}

DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_NEWGRFS_CHECKED)
{
	if (this->status != STATUS_NEWGRFS_CHECK) {
		/* Illegal call, return error and ignore the packet */
		return this->SendError(NETWORK_ERROR_NOT_EXPECTED);
	}

	NetworkClientInfo *ci = this->GetInfo();

	/* We now want a password from the client else we do not allow him in! */
	if (!StrEmpty(_settings_client.network.server_password)) {
		return this->SendNeedGamePassword();
	}

	if (Company::IsValidID(ci->client_playas) && !StrEmpty(_network_company_states[ci->client_playas].password)) {
		return this->SendNeedCompanyPassword();
	}

	return this->SendWelcome();
}

DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_JOIN)
{
	if (this->status != STATUS_INACTIVE) {
		/* Illegal call, return error and ignore the packet */
		return this->SendError(NETWORK_ERROR_NOT_EXPECTED);
	}

	char name[NETWORK_CLIENT_NAME_LENGTH];
	NetworkClientInfo *ci;
	CompanyID playas;
	NetworkLanguage client_lang;
	char client_revision[NETWORK_REVISION_LENGTH];

	p->Recv_string(client_revision, sizeof(client_revision));

	/* Check if the client has revision control enabled */
	if (!IsNetworkCompatibleVersion(client_revision)) {
		/* Different revisions!! */
		return this->SendError(NETWORK_ERROR_WRONG_REVISION);
	}

	p->Recv_string(name, sizeof(name));
	playas = (Owner)p->Recv_uint8();
	client_lang = (NetworkLanguage)p->Recv_uint8();

	if (this->HasClientQuit()) return NETWORK_RECV_STATUS_CONN_LOST;

	/* join another company does not affect these values */
	switch (playas) {
		case COMPANY_NEW_COMPANY: // New company
			if (Company::GetNumItems() >= _settings_client.network.max_companies) {
				return this->SendError(NETWORK_ERROR_FULL);
			}
			break;
		case COMPANY_SPECTATOR: // Spectator
			if (NetworkSpectatorCount() >= _settings_client.network.max_spectators) {
				return this->SendError(NETWORK_ERROR_FULL);
			}
			break;
		default: // Join another company (companies 1-8 (index 0-7))
			if (!Company::IsValidHumanID(playas)) {
				return this->SendError(NETWORK_ERROR_COMPANY_MISMATCH);
			}
			break;
	}

	/* We need a valid name.. make it Player */
	if (StrEmpty(name)) strecpy(name, "Player", lastof(name));

	if (!NetworkFindName(name)) { // Change name if duplicate
		/* We could not create a name for this client */
		return this->SendError(NETWORK_ERROR_NAME_IN_USE);
	}

	ci = this->GetInfo();

	strecpy(ci->client_name, name, lastof(ci->client_name));
	ci->client_playas = playas;
	ci->client_lang = client_lang;
	DEBUG(desync, 1, "client: %08x; %02x; %02x; %04x", _date, _date_fract, (int)ci->client_playas, ci->index);

	/* Make sure companies to which people try to join are not autocleaned */
	if (Company::IsValidID(playas)) _network_company_states[playas].months_empty = 0;

	this->status = STATUS_NEWGRFS_CHECK;

	if (_grfconfig == NULL) {
		/* Behave as if we received PACKET_CLIENT_NEWGRFS_CHECKED */
		return this->NetworkPacketReceive_PACKET_CLIENT_NEWGRFS_CHECKED_command(NULL);
	}

	return this->SendNewGRFCheck();
}

DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_GAME_PASSWORD)
{
	if (this->status != STATUS_AUTH_GAME) {
		return this->SendError(NETWORK_ERROR_NOT_EXPECTED);
	}

	char password[NETWORK_PASSWORD_LENGTH];
	p->Recv_string(password, sizeof(password));

	/* Check game password. Allow joining if we cleared the password meanwhile */
	if (!StrEmpty(_settings_client.network.server_password) &&
			strcmp(password, _settings_client.network.server_password) != 0) {
		/* Password is invalid */
		return this->SendError(NETWORK_ERROR_WRONG_PASSWORD);
	}

	const NetworkClientInfo *ci = this->GetInfo();
	if (Company::IsValidID(ci->client_playas) && !StrEmpty(_network_company_states[ci->client_playas].password)) {
		return this->SendNeedCompanyPassword();
	}

	/* Valid password, allow user */
	return this->SendWelcome();
}

DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_COMPANY_PASSWORD)
{
	if (this->status != STATUS_AUTH_COMPANY) {
		return this->SendError(NETWORK_ERROR_NOT_EXPECTED);
	}

	char password[NETWORK_PASSWORD_LENGTH];
	p->Recv_string(password, sizeof(password));

	/* Check company password. Allow joining if we cleared the password meanwhile.
	 * Also, check the company is still valid - client could be moved to spectators
	 * in the middle of the authorization process */
	CompanyID playas = this->GetInfo()->client_playas;
	if (Company::IsValidID(playas) && !StrEmpty(_network_company_states[playas].password) &&
			strcmp(password, _network_company_states[playas].password) != 0) {
		/* Password is invalid */
		return this->SendError(NETWORK_ERROR_WRONG_PASSWORD);
	}

	return this->SendWelcome();
}

DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_GETMAP)
{
	NetworkClientSocket *new_cs;

	/* Do an extra version match. We told the client our version already,
	 * lets confirm that the client isn't lieing to us.
	 * But only do it for stable releases because of those we are sure
	 * that everybody has the same NewGRF version. For trunk and the
	 * branches we make tarballs of the OpenTTDs compiled from tarball
	 * will have the lower bits set to 0. As such they would become
	 * incompatible, which we would like to prevent by this. */
	if (HasBit(_openttd_newgrf_version, 19)) {
		if (_openttd_newgrf_version != p->Recv_uint32()) {
			/* The version we get from the client differs, it must have the
			 * wrong version. The client must be wrong. */
			return this->SendError(NETWORK_ERROR_NOT_EXPECTED);
		}
	} else if (p->size != 3) {
		/* We received a packet from a version that claims to be stable.
		 * That shouldn't happen. The client must be wrong. */
		return this->SendError(NETWORK_ERROR_NOT_EXPECTED);
	}

	/* The client was never joined.. so this is impossible, right?
	 *  Ignore the packet, give the client a warning, and close his connection */
	if (this->status < STATUS_AUTHORIZED || this->HasClientQuit()) {
		return this->SendError(NETWORK_ERROR_NOT_AUTHORIZED);
	}

	/* Check if someone else is receiving the map */
	FOR_ALL_CLIENT_SOCKETS(new_cs) {
		if (new_cs->status == STATUS_MAP) {
			/* Tell the new client to wait */
			this->status = STATUS_MAP_WAIT;
			return this->SendWait();
		}
	}

	/* We receive a request to upload the map.. give it to the client! */
	return this->SendMap();
}

DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_MAP_OK)
{
	/* Client has the map, now start syncing */
	if (this->status == STATUS_DONE_MAP && !this->HasClientQuit()) {
		char client_name[NETWORK_CLIENT_NAME_LENGTH];
		NetworkClientSocket *new_cs;

		this->GetClientName(client_name, sizeof(client_name));

		NetworkTextMessage(NETWORK_ACTION_JOIN, CC_DEFAULT, false, client_name, NULL, this->client_id);

		/* Mark the client as pre-active, and wait for an ACK
		 *  so we know he is done loading and in sync with us */
		this->status = STATUS_PRE_ACTIVE;
		NetworkHandleCommandQueue(this);
		this->SendFrame();
		this->SendSync();

		/* This is the frame the client receives
		 *  we need it later on to make sure the client is not too slow */
		this->last_frame = _frame_counter;
		this->last_frame_server = _frame_counter;

		FOR_ALL_CLIENT_SOCKETS(new_cs) {
			if (new_cs->status > STATUS_AUTHORIZED) {
				new_cs->SendClientInfo(this->GetInfo());
				new_cs->SendJoin(this->client_id);
			}
		}

		NetworkAdminClientInfo(this->GetInfo(), true);

		/* also update the new client with our max values */
		this->SendConfigUpdate();

		/* quickly update the syncing client with company details */
		return this->SendCompanyUpdate();
	}

	/* Wrong status for this packet, give a warning to client, and close connection */
	return this->SendError(NETWORK_ERROR_NOT_EXPECTED);
}

/**
 * The client has done a command and wants us to handle it
 * @param p the packet in which the command was sent
 */
DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_COMMAND)
{
	/* The client was never joined.. so this is impossible, right?
	 *  Ignore the packet, give the client a warning, and close his connection */
	if (this->status < STATUS_DONE_MAP || this->HasClientQuit()) {
		return this->SendError(NETWORK_ERROR_NOT_EXPECTED);
	}

	if (this->incoming_queue.Count() >= _settings_client.network.max_commands_in_queue) {
		return this->SendError(NETWORK_ERROR_TOO_MANY_COMMANDS);
	}

	CommandPacket cp;
	const char *err = this->ReceiveCommand(p, &cp);

	if (this->HasClientQuit()) return NETWORK_RECV_STATUS_CONN_LOST;

	NetworkClientInfo *ci = this->GetInfo();

	if (err != NULL) {
		IConsolePrintF(CC_ERROR, "WARNING: %s from client %d (IP: %s).", err, ci->client_id, GetClientIP(ci));
		return this->SendError(NETWORK_ERROR_NOT_EXPECTED);
	}


	if ((GetCommandFlags(cp.cmd) & CMD_SERVER) && ci->client_id != CLIENT_ID_SERVER) {
		IConsolePrintF(CC_ERROR, "WARNING: server only command from: client %d (IP: %s), kicking...", ci->client_id, GetClientIP(ci));
		return this->SendError(NETWORK_ERROR_KICKED);
	}

	if ((GetCommandFlags(cp.cmd) & CMD_SPECTATOR) == 0 && !Company::IsValidID(cp.company) && ci->client_id != CLIENT_ID_SERVER) {
		IConsolePrintF(CC_ERROR, "WARNING: spectator issueing command from client %d (IP: %s), kicking...", ci->client_id, GetClientIP(ci));
		return this->SendError(NETWORK_ERROR_KICKED);
	}

	/**
	 * Only CMD_COMPANY_CTRL is always allowed, for the rest, playas needs
	 * to match the company in the packet. If it doesn't, the client has done
	 * something pretty naughty (or a bug), and will be kicked
	 */
	if (!(cp.cmd == CMD_COMPANY_CTRL && cp.p1 == 0 && ci->client_playas == COMPANY_NEW_COMPANY) && ci->client_playas != cp.company) {
		IConsolePrintF(CC_ERROR, "WARNING: client %d (IP: %s) tried to execute a command as company %d, kicking...",
		               ci->client_playas + 1, GetClientIP(ci), cp.company + 1);
		return this->SendError(NETWORK_ERROR_COMPANY_MISMATCH);
	}

	if (cp.cmd == CMD_COMPANY_CTRL) {
		if (cp.p1 != 0 || cp.company != COMPANY_SPECTATOR) {
			return this->SendError(NETWORK_ERROR_CHEATER);
		}

		/* Check if we are full - else it's possible for spectators to send a CMD_COMPANY_CTRL and the company is created regardless of max_companies! */
		if (Company::GetNumItems() >= _settings_client.network.max_companies) {
			NetworkServerSendChat(NETWORK_ACTION_SERVER_MESSAGE, DESTTYPE_CLIENT, ci->client_id, "cannot create new company, server full", CLIENT_ID_SERVER);
			return NETWORK_RECV_STATUS_OKAY;
		}
	}

	if (GetCommandFlags(cp.cmd) & CMD_CLIENT_ID) cp.p2 = this->client_id;

	this->incoming_queue.Append(&cp);
	return NETWORK_RECV_STATUS_OKAY;
}

DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_ERROR)
{
	/* This packets means a client noticed an error and is reporting this
	 *  to us. Display the error and report it to the other clients */
	NetworkClientSocket *new_cs;
	char str[100];
	char client_name[NETWORK_CLIENT_NAME_LENGTH];
	NetworkErrorCode errorno = (NetworkErrorCode)p->Recv_uint8();

	/* The client was never joined.. thank the client for the packet, but ignore it */
	if (this->status < STATUS_DONE_MAP || this->HasClientQuit()) {
		return this->CloseConnection(NETWORK_RECV_STATUS_CONN_LOST);
	}

	this->GetClientName(client_name, sizeof(client_name));

	StringID strid = GetNetworkErrorMsg(errorno);
	GetString(str, strid, lastof(str));

	DEBUG(net, 2, "'%s' reported an error and is closing its connection (%s)", client_name, str);

	NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, client_name, NULL, strid);

	FOR_ALL_CLIENT_SOCKETS(new_cs) {
		if (new_cs->status > STATUS_AUTHORIZED) {
			new_cs->SendErrorQuit(this->client_id, errorno);
		}
	}

	NetworkAdminClientError(this->client_id, errorno);

	return this->CloseConnection(NETWORK_RECV_STATUS_CONN_LOST);
}

DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_QUIT)
{
	/* The client wants to leave. Display this and report it to the other
	 *  clients. */
	NetworkClientSocket *new_cs;
	char client_name[NETWORK_CLIENT_NAME_LENGTH];

	/* The client was never joined.. thank the client for the packet, but ignore it */
	if (this->status < STATUS_DONE_MAP || this->HasClientQuit()) {
		return this->CloseConnection(NETWORK_RECV_STATUS_CONN_LOST);
	}

	this->GetClientName(client_name, sizeof(client_name));

	NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, client_name, NULL, STR_NETWORK_MESSAGE_CLIENT_LEAVING);

	FOR_ALL_CLIENT_SOCKETS(new_cs) {
		if (new_cs->status > STATUS_AUTHORIZED && new_cs != this) {
			new_cs->SendQuit(this->client_id);
		}
	}

	NetworkAdminClientQuit(this->client_id);

	return this->CloseConnection(NETWORK_RECV_STATUS_CONN_LOST);
}

DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_ACK)
{
	if (this->status < STATUS_AUTHORIZED) {
		/* Illegal call, return error and ignore the packet */
		return this->SendError(NETWORK_ERROR_NOT_AUTHORIZED);
	}

	uint32 frame = p->Recv_uint32();

	/* The client is trying to catch up with the server */
	if (this->status == STATUS_PRE_ACTIVE) {
		/* The client is not yet catched up? */
		if (frame + DAY_TICKS < _frame_counter) return NETWORK_RECV_STATUS_OKAY;

		/* Now he is! Unpause the game */
		this->status = STATUS_ACTIVE;
		this->last_token_frame = _frame_counter;

		/* Execute script for, e.g. MOTD */
		IConsoleCmdExec("exec scripts/on_server_connect.scr 0");
	}

	/* Get, and validate the token. */
	uint8 token = p->Recv_uint8();
	if (token == this->last_token) {
		/* We differentiate between last_token_frame and last_frame so the lag
		 * test uses the actual lag of the client instead of the lag for getting
		 * the token back and forth; after all, the token is only sent every
		 * time we receive a PACKET_CLIENT_ACK, after which we will send a new
		 * token to the client. If the lag would be one day, then we would not
		 * be sending the new token soon enough for the new daily scheduled
		 * PACKET_CLIENT_ACK. This would then register the lag of the client as
		 * two days, even when it's only a single day. */
		this->last_token_frame = _frame_counter;
		/* Request a new token. */
		this->last_token = 0;
	}

	/* The client received the frame, make note of it */
	this->last_frame = frame;
	/* With those 2 values we can calculate the lag realtime */
	this->last_frame_server = _frame_counter;
	return NETWORK_RECV_STATUS_OKAY;
}



void NetworkServerSendChat(NetworkAction action, DestType desttype, int dest, const char *msg, ClientID from_id, int64 data, bool from_admin)
{
	NetworkClientSocket *cs;
	const NetworkClientInfo *ci, *ci_own, *ci_to;

	switch (desttype) {
		case DESTTYPE_CLIENT:
			/* Are we sending to the server? */
			if ((ClientID)dest == CLIENT_ID_SERVER) {
				ci = NetworkFindClientInfoFromClientID(from_id);
				/* Display the text locally, and that is it */
				if (ci != NULL) {
					NetworkTextMessage(action, GetDrawStringCompanyColour(ci->client_playas), false, ci->client_name, msg, data);

					if (_settings_client.network.server_admin_chat) {
						NetworkAdminChat(action, desttype, from_id, msg, data, from_admin);
					}
				}
			} else {
				/* Else find the client to send the message to */
				FOR_ALL_CLIENT_SOCKETS(cs) {
					if (cs->client_id == (ClientID)dest) {
						cs->SendChat(action, from_id, false, msg, data);
						break;
					}
				}
			}

			/* Display the message locally (so you know you have sent it) */
			if (from_id != (ClientID)dest) {
				if (from_id == CLIENT_ID_SERVER) {
					ci = NetworkFindClientInfoFromClientID(from_id);
					ci_to = NetworkFindClientInfoFromClientID((ClientID)dest);
					if (ci != NULL && ci_to != NULL) {
						NetworkTextMessage(action, GetDrawStringCompanyColour(ci->client_playas), true, ci_to->client_name, msg, data);
					}
				} else {
					FOR_ALL_CLIENT_SOCKETS(cs) {
						if (cs->client_id == from_id) {
							cs->SendChat(action, (ClientID)dest, true, msg, data);
							break;
						}
					}
				}
			}
			break;
		case DESTTYPE_TEAM: {
			/* If this is false, the message is already displayed on the client who sent it. */
			bool show_local = true;
			/* Find all clients that belong to this company */
			ci_to = NULL;
			FOR_ALL_CLIENT_SOCKETS(cs) {
				ci = cs->GetInfo();
				if (ci->client_playas == (CompanyID)dest) {
					cs->SendChat(action, from_id, false, msg, data);
					if (cs->client_id == from_id) show_local = false;
					ci_to = ci; // Remember a client that is in the company for company-name
				}
			}

			/* if the server can read it, let the admin network read it, too. */
			if (_local_company == (CompanyID)dest && _settings_client.network.server_admin_chat) {
				NetworkAdminChat(action, desttype, from_id, msg, data, from_admin);
			}

			ci = NetworkFindClientInfoFromClientID(from_id);
			ci_own = NetworkFindClientInfoFromClientID(CLIENT_ID_SERVER);
			if (ci != NULL && ci_own != NULL && ci_own->client_playas == dest) {
				NetworkTextMessage(action, GetDrawStringCompanyColour(ci->client_playas), false, ci->client_name, msg, data);
				if (from_id == CLIENT_ID_SERVER) show_local = false;
				ci_to = ci_own;
			}

			/* There is no such client */
			if (ci_to == NULL) break;

			/* Display the message locally (so you know you have sent it) */
			if (ci != NULL && show_local) {
				if (from_id == CLIENT_ID_SERVER) {
					char name[NETWORK_NAME_LENGTH];
					StringID str = Company::IsValidID(ci_to->client_playas) ? STR_COMPANY_NAME : STR_NETWORK_SPECTATORS;
					SetDParam(0, ci_to->client_playas);
					GetString(name, str, lastof(name));
					NetworkTextMessage(action, GetDrawStringCompanyColour(ci_own->client_playas), true, name, msg, data);
				} else {
					FOR_ALL_CLIENT_SOCKETS(cs) {
						if (cs->client_id == from_id) {
							cs->SendChat(action, ci_to->client_id, true, msg, data);
						}
					}
				}
			}
			break;
		}
		default:
			DEBUG(net, 0, "[server] received unknown chat destination type %d. Doing broadcast instead", desttype);
			/* FALL THROUGH */
		case DESTTYPE_BROADCAST:
			FOR_ALL_CLIENT_SOCKETS(cs) {
				cs->SendChat(action, from_id, false, msg, data);
			}

			NetworkAdminChat(action, desttype, from_id, msg, data, from_admin);

			ci = NetworkFindClientInfoFromClientID(from_id);
			if (ci != NULL) {
				NetworkTextMessage(action, GetDrawStringCompanyColour(ci->client_playas), false, ci->client_name, msg, data);
			}
			break;
	}
}

DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_CHAT)
{
	if (this->status < STATUS_AUTHORIZED) {
		/* Illegal call, return error and ignore the packet */
		return this->SendError(NETWORK_ERROR_NOT_AUTHORIZED);
	}

	NetworkAction action = (NetworkAction)p->Recv_uint8();
	DestType desttype = (DestType)p->Recv_uint8();
	int dest = p->Recv_uint32();
	char msg[NETWORK_CHAT_LENGTH];

	p->Recv_string(msg, NETWORK_CHAT_LENGTH);
	int64 data = p->Recv_uint64();

	NetworkClientInfo *ci = this->GetInfo();
	switch (action) {
		case NETWORK_ACTION_GIVE_MONEY:
			if (!Company::IsValidID(ci->client_playas)) break;
			/* FALL THROUGH */
		case NETWORK_ACTION_CHAT:
		case NETWORK_ACTION_CHAT_CLIENT:
		case NETWORK_ACTION_CHAT_COMPANY:
			NetworkServerSendChat(action, desttype, dest, msg, this->client_id, data);
			break;
		default:
			IConsolePrintF(CC_ERROR, "WARNING: invalid chat action from client %d (IP: %s).", ci->client_id, GetClientIP(ci));
			return this->SendError(NETWORK_ERROR_NOT_EXPECTED);
	}
	return NETWORK_RECV_STATUS_OKAY;
}

DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_SET_PASSWORD)
{
	if (this->status != STATUS_ACTIVE) {
		/* Illegal call, return error and ignore the packet */
		return this->SendError(NETWORK_ERROR_NOT_EXPECTED);
	}

	char password[NETWORK_PASSWORD_LENGTH];
	const NetworkClientInfo *ci;

	p->Recv_string(password, sizeof(password));
	ci = this->GetInfo();

	if (Company::IsValidID(ci->client_playas)) {
		strecpy(_network_company_states[ci->client_playas].password, password, lastof(_network_company_states[ci->client_playas].password));
		NetworkServerUpdateCompanyPassworded(ci->client_playas, !StrEmpty(_network_company_states[ci->client_playas].password));
	}
	return NETWORK_RECV_STATUS_OKAY;
}

DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_SET_NAME)
{
	if (this->status != STATUS_ACTIVE) {
		/* Illegal call, return error and ignore the packet */
		return this->SendError(NETWORK_ERROR_NOT_EXPECTED);
	}

	char client_name[NETWORK_CLIENT_NAME_LENGTH];
	NetworkClientInfo *ci;

	p->Recv_string(client_name, sizeof(client_name));
	ci = this->GetInfo();

	if (this->HasClientQuit()) return NETWORK_RECV_STATUS_CONN_LOST;

	if (ci != NULL) {
		/* Display change */
		if (NetworkFindName(client_name)) {
			NetworkTextMessage(NETWORK_ACTION_NAME_CHANGE, CC_DEFAULT, false, ci->client_name, client_name);
			strecpy(ci->client_name, client_name, lastof(ci->client_name));
			NetworkUpdateClientInfo(ci->client_id);
		}
	}
	return NETWORK_RECV_STATUS_OKAY;
}

DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_RCON)
{
	if (this->status != STATUS_ACTIVE) return this->SendError(NETWORK_ERROR_NOT_EXPECTED);

	char pass[NETWORK_PASSWORD_LENGTH];
	char command[NETWORK_RCONCOMMAND_LENGTH];

	if (StrEmpty(_settings_client.network.rcon_password)) return NETWORK_RECV_STATUS_OKAY;

	p->Recv_string(pass, sizeof(pass));
	p->Recv_string(command, sizeof(command));

	if (strcmp(pass, _settings_client.network.rcon_password) != 0) {
		DEBUG(net, 0, "[rcon] wrong password from client-id %d", this->client_id);
		return NETWORK_RECV_STATUS_OKAY;
	}

	DEBUG(net, 0, "[rcon] client-id %d executed: '%s'", this->client_id, command);

	_redirect_console_to_client = this->client_id;
	IConsoleCmdExec(command);
	_redirect_console_to_client = INVALID_CLIENT_ID;
	return NETWORK_RECV_STATUS_OKAY;
}

DEF_GAME_RECEIVE_COMMAND(Server, PACKET_CLIENT_MOVE)
{
	if (this->status != STATUS_ACTIVE) return this->SendError(NETWORK_ERROR_NOT_EXPECTED);

	CompanyID company_id = (Owner)p->Recv_uint8();

	/* Check if the company is valid, we don't allow moving to AI companies */
	if (company_id != COMPANY_SPECTATOR && !Company::IsValidHumanID(company_id)) return NETWORK_RECV_STATUS_OKAY;

	/* Check if we require a password for this company */
	if (company_id != COMPANY_SPECTATOR && !StrEmpty(_network_company_states[company_id].password)) {
		/* we need a password from the client - should be in this packet */
		char password[NETWORK_PASSWORD_LENGTH];
		p->Recv_string(password, sizeof(password));

		/* Incorrect password sent, return! */
		if (strcmp(password, _network_company_states[company_id].password) != 0) {
			DEBUG(net, 2, "[move] wrong password from client-id #%d for company #%d", this->client_id, company_id + 1);
			return NETWORK_RECV_STATUS_OKAY;
		}
	}

	/* if we get here we can move the client */
	NetworkServerDoMove(this->client_id, company_id);
	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Package some generic company information into a packet.
 * @param p       The packet that will contain the data.
 * @param c       The company to put the of into the packet.
 * @param stats   The statistics to put in the packet.
 * @param max_len The maximum length of the company name.
 */
void NetworkSocketHandler::SendCompanyInformation(Packet *p, const Company *c, const NetworkCompanyStats *stats, uint max_len)
{
	/* Grab the company name */
	char company_name[NETWORK_COMPANY_NAME_LENGTH];
	SetDParam(0, c->index);

	assert(max_len <= lengthof(company_name));
	GetString(company_name, STR_COMPANY_NAME, company_name + max_len - 1);

	/* Get the income */
	Money income = 0;
	if (_cur_year - 1 == c->inaugurated_year) {
		/* The company is here just 1 year, so display [2], else display[1] */
		for (uint i = 0; i < lengthof(c->yearly_expenses[2]); i++) {
			income -= c->yearly_expenses[2][i];
		}
	} else {
		for (uint i = 0; i < lengthof(c->yearly_expenses[1]); i++) {
			income -= c->yearly_expenses[1][i];
		}
	}

	/* Send the information */
	p->Send_uint8 (c->index);
	p->Send_string(company_name);
	p->Send_uint32(c->inaugurated_year);
	p->Send_uint64(c->old_economy[0].company_value);
	p->Send_uint64(c->money);
	p->Send_uint64(income);
	p->Send_uint16(c->old_economy[0].performance_history);

	/* Send 1 if there is a passord for the company else send 0 */
	p->Send_bool  (!StrEmpty(_network_company_states[c->index].password));

	for (uint i = 0; i < NETWORK_VEH_END; i++) {
		p->Send_uint16(stats->num_vehicle[i]);
	}

	for (uint i = 0; i < NETWORK_VEH_END; i++) {
		p->Send_uint16(stats->num_station[i]);
	}

	p->Send_bool(c->is_ai);
}

/**
 * Populate the company stats.
 * @param stats the stats to update
 */
void NetworkPopulateCompanyStats(NetworkCompanyStats *stats)
{
	const Vehicle *v;
	const Station *s;

	memset(stats, 0, sizeof(*stats) * MAX_COMPANIES);

	/* Go through all vehicles and count the type of vehicles */
	FOR_ALL_VEHICLES(v) {
		if (!Company::IsValidID(v->owner) || !v->IsPrimaryVehicle()) continue;
		byte type = 0;
		switch (v->type) {
			case VEH_TRAIN: type = NETWORK_VEH_TRAIN; break;
			case VEH_ROAD: type = RoadVehicle::From(v)->IsBus() ? NETWORK_VEH_BUS : NETWORK_VEH_LORRY; break;
			case VEH_AIRCRAFT: type = NETWORK_VEH_PLANE; break;
			case VEH_SHIP: type = NETWORK_VEH_SHIP; break;
			default: continue;
		}
		stats[v->owner].num_vehicle[type]++;
	}

	/* Go through all stations and count the types of stations */
	FOR_ALL_STATIONS(s) {
		if (Company::IsValidID(s->owner)) {
			NetworkCompanyStats *npi = &stats[s->owner];

			if (s->facilities & FACIL_TRAIN)      npi->num_station[NETWORK_VEH_TRAIN]++;
			if (s->facilities & FACIL_TRUCK_STOP) npi->num_station[NETWORK_VEH_LORRY]++;
			if (s->facilities & FACIL_BUS_STOP)   npi->num_station[NETWORK_VEH_BUS]++;
			if (s->facilities & FACIL_AIRPORT)    npi->num_station[NETWORK_VEH_PLANE]++;
			if (s->facilities & FACIL_DOCK)       npi->num_station[NETWORK_VEH_SHIP]++;
		}
	}
}

/* Send a packet to all clients with updated info about this client_id */
void NetworkUpdateClientInfo(ClientID client_id)
{
	NetworkClientSocket *cs;
	NetworkClientInfo *ci = NetworkFindClientInfoFromClientID(client_id);

	if (ci == NULL) return;

	DEBUG(desync, 1, "client: %08x; %02x; %02x; %04x", _date, _date_fract, (int)ci->client_playas, client_id);

	FOR_ALL_CLIENT_SOCKETS(cs) {
		cs->SendClientInfo(ci);
	}

	NetworkAdminClientUpdate(ci);
}

/* Check if we want to restart the map */
static void NetworkCheckRestartMap()
{
	if (_settings_client.network.restart_game_year != 0 && _cur_year >= _settings_client.network.restart_game_year) {
		DEBUG(net, 0, "Auto-restarting map. Year %d reached", _cur_year);

		StartNewGameWithoutGUI(GENERATE_NEW_SEED);
	}
}

/* Check if the server has autoclean_companies activated
    Two things happen:
      1) If a company is not protected, it is closed after 1 year (for example)
      2) If a company is protected, protection is disabled after 3 years (for example)
           (and item 1. happens a year later) */
static void NetworkAutoCleanCompanies()
{
	const NetworkClientInfo *ci;
	const Company *c;
	bool clients_in_company[MAX_COMPANIES];
	int vehicles_in_company[MAX_COMPANIES];

	if (!_settings_client.network.autoclean_companies) return;

	memset(clients_in_company, 0, sizeof(clients_in_company));

	/* Detect the active companies */
	FOR_ALL_CLIENT_INFOS(ci) {
		if (Company::IsValidID(ci->client_playas)) clients_in_company[ci->client_playas] = true;
	}

	if (!_network_dedicated) {
		ci = NetworkFindClientInfoFromClientID(CLIENT_ID_SERVER);
		if (Company::IsValidID(ci->client_playas)) clients_in_company[ci->client_playas] = true;
	}

	if (_settings_client.network.autoclean_novehicles != 0) {
		memset(vehicles_in_company, 0, sizeof(vehicles_in_company));

		const Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (!Company::IsValidID(v->owner) || !v->IsPrimaryVehicle()) continue;
			vehicles_in_company[v->owner]++;
		}
	}

	/* Go through all the comapnies */
	FOR_ALL_COMPANIES(c) {
		/* Skip the non-active once */
		if (c->is_ai) continue;

		if (!clients_in_company[c->index]) {
			/* The company is empty for one month more */
			_network_company_states[c->index].months_empty++;

			/* Is the company empty for autoclean_unprotected-months, and is there no protection? */
			if (_settings_client.network.autoclean_unprotected != 0 && _network_company_states[c->index].months_empty > _settings_client.network.autoclean_unprotected && StrEmpty(_network_company_states[c->index].password)) {
				/* Shut the company down */
				DoCommandP(0, 2 | c->index << 16, 0, CMD_COMPANY_CTRL);
				NetworkAdminCompanyRemove(c->index, ADMIN_CRR_AUTOCLEAN);
				IConsolePrintF(CC_DEFAULT, "Auto-cleaned company #%d with no password", c->index + 1);
			}
			/* Is the company empty for autoclean_protected-months, and there is a protection? */
			if (_settings_client.network.autoclean_protected != 0 && _network_company_states[c->index].months_empty > _settings_client.network.autoclean_protected && !StrEmpty(_network_company_states[c->index].password)) {
				/* Unprotect the company */
				_network_company_states[c->index].password[0] = '\0';
				IConsolePrintF(CC_DEFAULT, "Auto-removed protection from company #%d", c->index + 1);
				_network_company_states[c->index].months_empty = 0;
				NetworkServerUpdateCompanyPassworded(c->index, false);
			}
			/* Is the company empty for autoclean_novehicles-months, and has no vehicles? */
			if (_settings_client.network.autoclean_novehicles != 0 && _network_company_states[c->index].months_empty > _settings_client.network.autoclean_novehicles && vehicles_in_company[c->index] == 0) {
				/* Shut the company down */
				DoCommandP(0, 2 | c->index << 16, 0, CMD_COMPANY_CTRL);
				NetworkAdminCompanyRemove(c->index, ADMIN_CRR_AUTOCLEAN);
				IConsolePrintF(CC_DEFAULT, "Auto-cleaned company #%d with no vehicles", c->index + 1);
			}
		} else {
			/* It is not empty, reset the date */
			_network_company_states[c->index].months_empty = 0;
		}
	}
}

/* This function changes new_name to a name that is unique (by adding #1 ...)
 *  and it returns true if that succeeded. */
bool NetworkFindName(char new_name[NETWORK_CLIENT_NAME_LENGTH])
{
	bool found_name = false;
	uint number = 0;
	char original_name[NETWORK_CLIENT_NAME_LENGTH];

	/* We use NETWORK_CLIENT_NAME_LENGTH in here, because new_name is really a pointer */
	ttd_strlcpy(original_name, new_name, NETWORK_CLIENT_NAME_LENGTH);

	while (!found_name) {
		const NetworkClientInfo *ci;

		found_name = true;
		FOR_ALL_CLIENT_INFOS(ci) {
			if (strcmp(ci->client_name, new_name) == 0) {
				/* Name already in use */
				found_name = false;
				break;
			}
		}
		/* Check if it is the same as the server-name */
		ci = NetworkFindClientInfoFromClientID(CLIENT_ID_SERVER);
		if (ci != NULL) {
			if (strcmp(ci->client_name, new_name) == 0) found_name = false; // name already in use
		}

		if (!found_name) {
			/* Try a new name (<name> #1, <name> #2, and so on) */

			/* Something's really wrong when there're more names than clients */
			if (number++ > MAX_CLIENTS) break;
			snprintf(new_name, NETWORK_CLIENT_NAME_LENGTH, "%s #%d", original_name, number);
		}
	}

	return found_name;
}

/**
 * Change the client name of the given client
 * @param client_id the client to change the name of
 * @param new_name the new name for the client
 * @return true iff the name was changed
 */
bool NetworkServerChangeClientName(ClientID client_id, const char *new_name)
{
	NetworkClientInfo *ci;
	/* Check if the name's already in use */
	FOR_ALL_CLIENT_INFOS(ci) {
		if (strcmp(ci->client_name, new_name) == 0) return false;
	}

	ci = NetworkFindClientInfoFromClientID(client_id);
	if (ci == NULL) return false;

	NetworkTextMessage(NETWORK_ACTION_NAME_CHANGE, CC_DEFAULT, true, ci->client_name, new_name);

	strecpy(ci->client_name, new_name, lastof(ci->client_name));

	NetworkUpdateClientInfo(client_id);
	return true;
}

/* Handle the local command-queue */
static void NetworkHandleCommandQueue(NetworkClientSocket *cs)
{
	CommandPacket *cp;
	while ((cp = cs->outgoing_queue.Pop()) != NULL) {
		cs->SendCommand(cp);
		free(cp);
	}
}

/* This is called every tick if this is a _network_server */
void NetworkServer_Tick(bool send_frame)
{
	NetworkClientSocket *cs;
#ifndef ENABLE_NETWORK_SYNC_EVERY_FRAME
	bool send_sync = false;
#endif

#ifndef ENABLE_NETWORK_SYNC_EVERY_FRAME
	if (_frame_counter >= _last_sync_frame + _settings_client.network.sync_freq) {
		_last_sync_frame = _frame_counter;
		send_sync = true;
	}
#endif

	/* Now we are done with the frame, inform the clients that they can
	 *  do their frame! */
	FOR_ALL_CLIENT_SOCKETS(cs) {
		/* We allow a number of bytes per frame, but only to the burst amount
		 * to be available for packet receiving at any particular time. */
		cs->receive_limit = min(cs->receive_limit + _settings_client.network.bytes_per_frame,
				_settings_client.network.bytes_per_frame_burst);

		/* Check if the speed of the client is what we can expect from a client */
		if (cs->status == NetworkClientSocket::STATUS_ACTIVE) {
			/* 1 lag-point per day */
			uint lag = NetworkCalculateLag(cs) / DAY_TICKS;
			if (lag > 0) {
				if (lag > 3) {
					/* Client did still not report in after 4 game-day, drop him
					 *  (that is, the 3 of above, + 1 before any lag is counted) */
					IConsolePrintF(CC_ERROR, cs->last_packet + 3 * DAY_TICKS * MILLISECONDS_PER_TICK > _realtime_tick ?
							/* A packet was received in the last three game days, so the client is likely lagging behind. */
								"Client #%d is dropped because the client's game state is more than 4 game-days behind" :
							/* No packet was received in the last three game days; sounds like a lost connection. */
								"Client #%d is dropped because the client did not respond for more than 4 game-days",
							cs->client_id);
					cs->CloseConnection(NETWORK_RECV_STATUS_SERVER_ERROR);
					continue;
				}

				/* Report once per time we detect the lag, and only when we
				 * received a packet in the last 2000 milliseconds. If we
				 * did not receive a packet, then the client is not just
				 * slow, but the connection is likely severed. Mentioning
				 * frame_freq is not useful in this case. */
				if (cs->lag_test == 0 && cs->last_packet + DAY_TICKS * MILLISECONDS_PER_TICK > _realtime_tick) {
					IConsolePrintF(CC_WARNING,"[%d] Client #%d is slow, try increasing [network.]frame_freq to a higher value!", _frame_counter, cs->client_id);
					cs->lag_test = 1;
				}
			} else {
				cs->lag_test = 0;
			}
			if (cs->last_frame_server - cs->last_token_frame >= 5 * DAY_TICKS) {
				/* This is a bad client! It didn't send the right token back. */
				IConsolePrintF(CC_ERROR, "Client #%d is dropped because it fails to send valid acks", cs->client_id);
				cs->CloseConnection(NETWORK_RECV_STATUS_SERVER_ERROR);
				continue;
			}
		} else if (cs->status == NetworkClientSocket::STATUS_PRE_ACTIVE) {
			uint lag = NetworkCalculateLag(cs);
			if (lag > _settings_client.network.max_join_time) {
				IConsolePrintF(CC_ERROR,"Client #%d is dropped because it took longer than %d ticks for him to join", cs->client_id, _settings_client.network.max_join_time);
				cs->CloseConnection(NETWORK_RECV_STATUS_SERVER_ERROR);
				continue;
			}
		} else if (cs->status == NetworkClientSocket::STATUS_INACTIVE) {
			uint lag = NetworkCalculateLag(cs);
			if (lag > 4 * DAY_TICKS) {
				IConsolePrintF(CC_ERROR,"Client #%d is dropped because it took longer than %d ticks to start the joining process", cs->client_id, 4 * DAY_TICKS);
				cs->CloseConnection(NETWORK_RECV_STATUS_SERVER_ERROR);
				continue;
			}
		}

		if (cs->status >= NetworkClientSocket::STATUS_PRE_ACTIVE) {
			/* Check if we can send command, and if we have anything in the queue */
			NetworkHandleCommandQueue(cs);

			/* Send an updated _frame_counter_max to the client */
			if (send_frame) cs->SendFrame();

#ifndef ENABLE_NETWORK_SYNC_EVERY_FRAME
			/* Send a sync-check packet */
			if (send_sync) cs->SendSync();
#endif
		}
	}

	/* See if we need to advertise */
	NetworkUDPAdvertise();
}

/** Yearly "callback". Called whenever the year changes. */
void NetworkServerYearlyLoop()
{
	NetworkCheckRestartMap();
	NetworkAdminUpdate(ADMIN_FREQUENCY_ANUALLY);
}

/** Monthly "callback". Called whenever the month changes. */
void NetworkServerMonthlyLoop()
{
	NetworkAutoCleanCompanies();
	NetworkAdminUpdate(ADMIN_FREQUENCY_MONTHLY);
	if ((_cur_month % 3) == 0) NetworkAdminUpdate(ADMIN_FREQUENCY_QUARTERLY);
}

/** Daily "callback". Called whenever the date changes. */
void NetworkServerDailyLoop()
{
	NetworkAdminUpdate(ADMIN_FREQUENCY_DAILY);
	if ((_date % 7) == 3) NetworkAdminUpdate(ADMIN_FREQUENCY_WEEKLY);
}

const char *GetClientIP(NetworkClientInfo *ci)
{
	return ci->client_address.GetHostname();
}

void NetworkServerShowStatusToConsole()
{
	static const char * const stat_str[] = {
		"inactive",
		"checking NewGRFs",
		"authorizing (server password)",
		"authorizing (company password)",
		"authorized",
		"waiting",
		"loading map",
		"map done",
		"ready",
		"active"
	};
	assert_compile(lengthof(stat_str) == NetworkClientSocket::STATUS_END);

	NetworkClientSocket *cs;
	FOR_ALL_CLIENT_SOCKETS(cs) {
		uint lag = NetworkCalculateLag(cs);
		NetworkClientInfo *ci = cs->GetInfo();
		const char *status;

		status = (cs->status < (ptrdiff_t)lengthof(stat_str) ? stat_str[cs->status] : "unknown");
		IConsolePrintF(CC_INFO, "Client #%1d  name: '%s'  status: '%s'  frame-lag: %3d  company: %1d  IP: %s",
			cs->client_id, ci->client_name, status, lag,
			ci->client_playas + (Company::IsValidID(ci->client_playas) ? 1 : 0),
			GetClientIP(ci));
	}
}

/**
 * Send Config Update
 */
void NetworkServerSendConfigUpdate()
{
	NetworkClientSocket *cs;

	FOR_ALL_CLIENT_SOCKETS(cs) {
		if (cs->status >= NetworkClientSocket::STATUS_PRE_ACTIVE) cs->SendConfigUpdate();
	}
}

void NetworkServerUpdateCompanyPassworded(CompanyID company_id, bool passworded)
{
	if (NetworkCompanyIsPassworded(company_id) == passworded) return;

	SB(_network_company_passworded, company_id, 1, !!passworded);
	SetWindowClassesDirty(WC_COMPANY);

	NetworkClientSocket *cs;
	FOR_ALL_CLIENT_SOCKETS(cs) {
		if (cs->status >= NetworkClientSocket::STATUS_PRE_ACTIVE) cs->SendCompanyUpdate();
	}

	NetworkAdminCompanyUpdate(Company::GetIfValid(company_id));
}

/**
 * Handle the tid-bits of moving a client from one company to another.
 * @param client_id id of the client we want to move.
 * @param company_id id of the company we want to move the client to.
 * @return void
 */
void NetworkServerDoMove(ClientID client_id, CompanyID company_id)
{
	/* Only allow non-dedicated servers and normal clients to be moved */
	if (client_id == CLIENT_ID_SERVER && _network_dedicated) return;

	NetworkClientInfo *ci = NetworkFindClientInfoFromClientID(client_id);

	/* No need to waste network resources if the client is in the company already! */
	if (ci->client_playas == company_id) return;

	ci->client_playas = company_id;

	if (client_id == CLIENT_ID_SERVER) {
		SetLocalCompany(company_id);
	} else {
		NetworkClientSocket *cs = NetworkFindClientStateFromClientID(client_id);
		/* When the company isn't authorized we can't move them yet. */
		if (cs->status < NetworkClientSocket::STATUS_AUTHORIZED) return;
		cs->SendMove(client_id, company_id);
	}

	/* announce the client's move */
	NetworkUpdateClientInfo(client_id);

	NetworkAction action = (company_id == COMPANY_SPECTATOR) ? NETWORK_ACTION_COMPANY_SPECTATOR : NETWORK_ACTION_COMPANY_JOIN;
	NetworkServerSendChat(action, DESTTYPE_BROADCAST, 0, "", client_id, company_id + 1);
}

void NetworkServerSendRcon(ClientID client_id, TextColour colour_code, const char *string)
{
	NetworkFindClientStateFromClientID(client_id)->SendRConResult(colour_code, string);
}

static void NetworkServerSendError(ClientID client_id, NetworkErrorCode error)
{
	NetworkFindClientStateFromClientID(client_id)->SendError(error);
}

void NetworkServerKickClient(ClientID client_id)
{
	if (client_id == CLIENT_ID_SERVER) return;
	NetworkServerSendError(client_id, NETWORK_ERROR_KICKED);
}

uint NetworkServerKickOrBanIP(const char *ip, bool ban)
{
	/* Add address to ban-list */
	if (ban) *_network_ban_list.Append() = strdup(ip);

	uint n = 0;

	/* There can be multiple clients with the same IP, kick them all */
	NetworkClientInfo *ci;
	FOR_ALL_CLIENT_INFOS(ci) {
		if (ci->client_id == CLIENT_ID_SERVER) continue;
		if (ci->client_address.IsInNetmask(const_cast<char *>(ip))) {
			NetworkServerKickClient(ci->client_id);
			n++;
		}
	}

	return n;
}

bool NetworkCompanyHasClients(CompanyID company)
{
	const NetworkClientInfo *ci;
	FOR_ALL_CLIENT_INFOS(ci) {
		if (ci->client_playas == company) return true;
	}
	return false;
}


/**
 * Get the name of the client, if the user did not send it yet, Client #<no> is used.
 * @param client_name The variable to write the name to.
 * @param size        The amount of bytes we can write.
 */
void ServerNetworkGameSocketHandler::GetClientName(char *client_name, size_t size) const
{
	const NetworkClientInfo *ci = this->GetInfo();

	if (StrEmpty(ci->client_name)) {
		snprintf(client_name, size, "Client #%4d", this->client_id);
	} else {
		ttd_strlcpy(client_name, ci->client_name, size);
	}
}

#endif /* ENABLE_NETWORK */
