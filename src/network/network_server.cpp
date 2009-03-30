/* $Id$ */

/** @file network_server.cpp Server part of the network protocol. */

#ifdef ENABLE_NETWORK

#include "../stdafx.h"
#include "../debug.h"
#include "../strings_func.h"
#include "network_internal.h"
#include "../vehicle_base.h"
#include "../date_func.h"
#include "network_server.h"
#include "network_udp.h"
#include "../console_func.h"
#include "../command_func.h"
#include "../saveload/saveload.h"
#include "../station_base.h"
#include "../genworld.h"
#include "../fileio_func.h"
#include "../string_func.h"
#include "../company_func.h"
#include "../company_gui.h"
#include "../settings_type.h"
#include "../window_func.h"

#include "table/strings.h"

/* This file handles all the server-commands */

static void NetworkHandleCommandQueue(NetworkClientSocket *cs);

/***********
 * Sending functions
 *   DEF_SERVER_SEND_COMMAND has parameter: NetworkClientSocket *cs
 ************/

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_CLIENT_INFO)(NetworkClientSocket *cs, NetworkClientInfo *ci)
{
	/*
	 * Packet: SERVER_CLIENT_INFO
	 * Function: Sends info about a client
	 * Data:
	 *    uint32:  The identifier of the client (always unique on a server. 1 = server, 0 is invalid)
	 *    uint8:  As which company the client is playing
	 *    String: The name of the client
	 */

	if (ci->client_id != INVALID_CLIENT_ID) {
		Packet *p = NetworkSend_Init(PACKET_SERVER_CLIENT_INFO);
		p->Send_uint32(ci->client_id);
		p->Send_uint8 (ci->client_playas);
		p->Send_string(ci->client_name);

		cs->Send_Packet(p);
	}
}

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_COMPANY_INFO)
{
	/*
	 * Packet: SERVER_COMPANY_INFO
	 * Function: Sends info about the companies
	 * Data:
	 */

	/* Fetch the latest version of the stats */
	NetworkCompanyStats company_stats[MAX_COMPANIES];
	NetworkPopulateCompanyStats(company_stats);

	/* Make a list of all clients per company */
	char clients[MAX_COMPANIES][NETWORK_CLIENTS_LENGTH];
	NetworkClientSocket *csi;
	memset(clients, 0, sizeof(clients));

	/* Add the local player (if not dedicated) */
	const NetworkClientInfo *ci = NetworkFindClientInfoFromClientID(CLIENT_ID_SERVER);
	if (ci != NULL && IsValidCompanyID(ci->client_playas)) {
		strecpy(clients[ci->client_playas], ci->client_name, lastof(clients[ci->client_playas]));
	}

	FOR_ALL_CLIENT_SOCKETS(csi) {
		char client_name[NETWORK_CLIENT_NAME_LENGTH];

		NetworkGetClientName(client_name, sizeof(client_name), csi);

		ci = csi->GetInfo();
		if (ci != NULL && IsValidCompanyID(ci->client_playas)) {
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
		p = NetworkSend_Init(PACKET_SERVER_COMPANY_INFO);

		p->Send_uint8 (NETWORK_COMPANY_INFO_VERSION);
		p->Send_bool  (true);
		cs->Send_CompanyInformation(p, company, &company_stats[company->index]);

		if (StrEmpty(clients[company->index])) {
			p->Send_string("<none>");
		} else {
			p->Send_string(clients[company->index]);
		}

		cs->Send_Packet(p);
	}

	p = NetworkSend_Init(PACKET_SERVER_COMPANY_INFO);

	p->Send_uint8 (NETWORK_COMPANY_INFO_VERSION);
	p->Send_bool  (false);

	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_ERROR)(NetworkClientSocket *cs, NetworkErrorCode error)
{
	/*
	 * Packet: SERVER_ERROR
	 * Function: The client made an error
	 * Data:
	 *    uint8:  ErrorID (see network_data.h, NetworkErrorCode)
	 */

	char str[100];
	Packet *p = NetworkSend_Init(PACKET_SERVER_ERROR);

	p->Send_uint8(error);
	cs->Send_Packet(p);

	StringID strid = GetNetworkErrorMsg(error);
	GetString(str, strid, lastof(str));

	/* Only send when the current client was in game */
	if (cs->status > STATUS_AUTH) {
		NetworkClientSocket *new_cs;
		char client_name[NETWORK_CLIENT_NAME_LENGTH];

		NetworkGetClientName(client_name, sizeof(client_name), cs);

		DEBUG(net, 1, "'%s' made an error and has been disconnected. Reason: '%s'", client_name, str);

		NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, client_name, NULL, strid);

		FOR_ALL_CLIENT_SOCKETS(new_cs) {
			if (new_cs->status > STATUS_AUTH && new_cs != cs) {
				/* Some errors we filter to a more general error. Clients don't have to know the real
				 *  reason a joining failed. */
				if (error == NETWORK_ERROR_NOT_AUTHORIZED || error == NETWORK_ERROR_NOT_EXPECTED || error == NETWORK_ERROR_WRONG_REVISION)
					error = NETWORK_ERROR_ILLEGAL_PACKET;

				SEND_COMMAND(PACKET_SERVER_ERROR_QUIT)(new_cs, cs->client_id, error);
			}
		}
	} else {
		DEBUG(net, 1, "Client %d made an error and has been disconnected. Reason: '%s'", cs->client_id, str);
	}

	cs->has_quit = true;

	/* Make sure the data get's there before we close the connection */
	cs->Send_Packets();

	/* The client made a mistake, so drop his connection now! */
	NetworkCloseClient(cs);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_CHECK_NEWGRFS)(NetworkClientSocket *cs)
{
	/*
	 * Packet: PACKET_SERVER_CHECK_NEWGRFS
	 * Function: Sends info about the used GRFs to the client
	 * Data:
	 *      uint8:  Amount of GRFs
	 *    And then for each GRF:
	 *      uint32: GRF ID
	 * 16 * uint8:  MD5 checksum of the GRF
	 */

	Packet *p = NetworkSend_Init(PACKET_SERVER_CHECK_NEWGRFS);
	const GRFConfig *c;
	uint grf_count = 0;

	for (c = _grfconfig; c != NULL; c = c->next) {
		if (!HasBit(c->flags, GCF_STATIC)) grf_count++;
	}

	p->Send_uint8 (grf_count);
	for (c = _grfconfig; c != NULL; c = c->next) {
		if (!HasBit(c->flags, GCF_STATIC)) cs->Send_GRFIdentifier(p, c);
	}

	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_NEED_PASSWORD)(NetworkClientSocket *cs, NetworkPasswordType type)
{
	/*
	 * Packet: SERVER_NEED_PASSWORD
	 * Function: Indication to the client that the server needs a password
	 * Data:
	 *    uint8:  Type of password
	 */

	/* Invalid packet when status is AUTH or higher */
	if (cs->status >= STATUS_AUTH) return;

	cs->status = STATUS_AUTHORIZING;

	Packet *p = NetworkSend_Init(PACKET_SERVER_NEED_PASSWORD);
	p->Send_uint8(type);
	p->Send_uint32(_settings_game.game_creation.generation_seed);
	p->Send_string(_settings_client.network.network_id);
	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_WELCOME)
{
	/*
	 * Packet: SERVER_WELCOME
	 * Function: The client is joined and ready to receive his map
	 * Data:
	 *    uint32:  Own Client identifier
	 */

	Packet *p;
	NetworkClientSocket *new_cs;

	/* Invalid packet when status is AUTH or higher */
	if (cs->status >= STATUS_AUTH) return;

	cs->status = STATUS_AUTH;
	_network_game_info.clients_on++;

	p = NetworkSend_Init(PACKET_SERVER_WELCOME);
	p->Send_uint32(cs->client_id);
	p->Send_uint32(_settings_game.game_creation.generation_seed);
	p->Send_string(_settings_client.network.network_id);
	cs->Send_Packet(p);

		/* Transmit info about all the active clients */
	FOR_ALL_CLIENT_SOCKETS(new_cs) {
		if (new_cs != cs && new_cs->status > STATUS_AUTH)
			SEND_COMMAND(PACKET_SERVER_CLIENT_INFO)(cs, new_cs->GetInfo());
	}
	/* Also send the info of the server */
	SEND_COMMAND(PACKET_SERVER_CLIENT_INFO)(cs, NetworkFindClientInfoFromClientID(CLIENT_ID_SERVER));
}

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_WAIT)
{
	/*
	 * Packet: PACKET_SERVER_WAIT
	 * Function: The client can not receive the map at the moment because
	 *             someone else is already receiving the map
	 * Data:
	 *    uint8:  Clients awaiting map
	 */
	int waiting = 0;
	NetworkClientSocket *new_cs;
	Packet *p;

	/* Count how many clients are waiting in the queue */
	FOR_ALL_CLIENT_SOCKETS(new_cs) {
		if (new_cs->status == STATUS_MAP_WAIT) waiting++;
	}

	p = NetworkSend_Init(PACKET_SERVER_WAIT);
	p->Send_uint8(waiting);
	cs->Send_Packet(p);
}

/* This sends the map to the client */
DEF_SERVER_SEND_COMMAND(PACKET_SERVER_MAP)
{
	/*
	 * Packet: SERVER_MAP
	 * Function: Sends the map to the client, or a part of it (it is splitted in
	 *   a lot of multiple packets)
	 * Data:
	 *    uint8:  packet-type (MAP_PACKET_START, MAP_PACKET_NORMAL and MAP_PACKET_END)
	 *  if MAP_PACKET_START:
	 *    uint32: The current FrameCounter
	 *  if MAP_PACKET_NORMAL:
	 *    piece of the map (till max-size of packet)
	 *  if MAP_PACKET_END:
	 *    nothing
	 */

	static FILE *file_pointer;
	static uint sent_packets; // How many packets we did send succecfully last time

	if (cs->status < STATUS_AUTH) {
		/* Illegal call, return error and ignore the packet */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_AUTHORIZED);
		return;
	}

	if (cs->status == STATUS_AUTH) {
		const char *filename = "network_server.tmp";
		Packet *p;

		/* Make a dump of the current game */
		if (SaveOrLoad(filename, SL_SAVE, AUTOSAVE_DIR) != SL_OK) usererror("network savedump failed");

		file_pointer = FioFOpenFile(filename, "rb", AUTOSAVE_DIR);
		fseek(file_pointer, 0, SEEK_END);

		if (ftell(file_pointer) == 0) usererror("network savedump failed - zero sized savegame?");

		/* Now send the _frame_counter and how many packets are coming */
		p = NetworkSend_Init(PACKET_SERVER_MAP);
		p->Send_uint8 (MAP_PACKET_START);
		p->Send_uint32(_frame_counter);
		p->Send_uint32(ftell(file_pointer));
		cs->Send_Packet(p);

		fseek(file_pointer, 0, SEEK_SET);

		sent_packets = 4; // We start with trying 4 packets

		cs->status = STATUS_MAP;
		/* Mark the start of download */
		cs->last_frame = _frame_counter;
		cs->last_frame_server = _frame_counter;
	}

	if (cs->status == STATUS_MAP) {
		uint i;
		int res;
		for (i = 0; i < sent_packets; i++) {
			Packet *p = NetworkSend_Init(PACKET_SERVER_MAP);
			p->Send_uint8(MAP_PACKET_NORMAL);
			res = (int)fread(p->buffer + p->size, 1, SEND_MTU - p->size, file_pointer);

			if (ferror(file_pointer)) usererror("Error reading temporary network savegame!");

			p->size += res;
			cs->Send_Packet(p);
			if (feof(file_pointer)) {
				/* Done reading! */
				Packet *p = NetworkSend_Init(PACKET_SERVER_MAP);
				p->Send_uint8(MAP_PACKET_END);
				cs->Send_Packet(p);

				/* Set the status to DONE_MAP, no we will wait for the client
				 *  to send it is ready (maybe that happens like never ;)) */
				cs->status = STATUS_DONE_MAP;
				fclose(file_pointer);

				{
					NetworkClientSocket *new_cs;
					bool new_map_client = false;
					/* Check if there is a client waiting for receiving the map
					 *  and start sending him the map */
					FOR_ALL_CLIENT_SOCKETS(new_cs) {
						if (new_cs->status == STATUS_MAP_WAIT) {
							/* Check if we already have a new client to send the map to */
							if (!new_map_client) {
								/* If not, this client will get the map */
								new_cs->status = STATUS_AUTH;
								new_map_client = true;
								SEND_COMMAND(PACKET_SERVER_MAP)(new_cs);
							} else {
								/* Else, send the other clients how many clients are in front of them */
								SEND_COMMAND(PACKET_SERVER_WAIT)(new_cs);
							}
						}
					}
				}

				/* There is no more data, so break the for */
				break;
			}
		}

		/* Send all packets (forced) and check if we have send it all */
		cs->Send_Packets();
		if (cs->IsPacketQueueEmpty()) {
			/* All are sent, increase the sent_packets */
			sent_packets *= 2;
		} else {
			/* Not everything is sent, decrease the sent_packets */
			if (sent_packets > 1) sent_packets /= 2;
		}
	}
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_JOIN)(NetworkClientSocket *cs, ClientID client_id)
{
	/*
	 * Packet: SERVER_JOIN
	 * Function: A client is joined (all active clients receive this after a
	 *     PACKET_CLIENT_MAP_OK) Mostly what directly follows is a
	 *     PACKET_SERVER_CLIENT_INFO
	 * Data:
	 *    uint32:  Client-identifier
	 */

	Packet *p = NetworkSend_Init(PACKET_SERVER_JOIN);

	p->Send_uint32(client_id);

	cs->Send_Packet(p);
}


DEF_SERVER_SEND_COMMAND(PACKET_SERVER_FRAME)
{
	/*
	 * Packet: SERVER_FRAME
	 * Function: Sends the current frame-counter to the client
	 * Data:
	 *    uint32: Frame Counter
	 *    uint32: Frame Counter Max (how far may the client walk before the server?)
	 *    [uint32: general-seed-1]
	 *    [uint32: general-seed-2]
	 *      (last two depends on compile-settings, and are not default settings)
	 */

	Packet *p = NetworkSend_Init(PACKET_SERVER_FRAME);
	p->Send_uint32(_frame_counter);
	p->Send_uint32(_frame_counter_max);
#ifdef ENABLE_NETWORK_SYNC_EVERY_FRAME
	p->Send_uint32(_sync_seed_1);
#ifdef NETWORK_SEND_DOUBLE_SEED
	p->Send_uint32(_sync_seed_2);
#endif
#endif
	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_SYNC)
{
	/*
	 * Packet: SERVER_SYNC
	 * Function: Sends a sync-check to the client
	 * Data:
	 *    uint32: Frame Counter
	 *    uint32: General-seed-1
	 *    [uint32: general-seed-2]
	 *      (last one depends on compile-settings, and are not default settings)
	 */

	Packet *p = NetworkSend_Init(PACKET_SERVER_SYNC);
	p->Send_uint32(_frame_counter);
	p->Send_uint32(_sync_seed_1);

#ifdef NETWORK_SEND_DOUBLE_SEED
	p->Send_uint32(_sync_seed_2);
#endif
	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_COMMAND)(NetworkClientSocket *cs, const CommandPacket *cp)
{
	/*
	 * Packet: SERVER_COMMAND
	 * Function: Sends a DoCommand to the client
	 * Data:
	 *    uint8:  CompanyID (0..MAX_COMPANIES-1)
	 *    uint32: CommandID (see command.h)
	 *    uint32: P1 (free variables used in DoCommand)
	 *    uint32: P2
	 *    uint32: Tile
	 *    string: text
	 *    uint8:  CallBackID (see callback_table.c)
	 *    uint32: Frame of execution
	 */

	Packet *p = NetworkSend_Init(PACKET_SERVER_COMMAND);

	cs->Send_Command(p, cp);
	p->Send_uint32(cp->frame);
	p->Send_bool  (cp->my_cmd);

	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_CHAT)(NetworkClientSocket *cs, NetworkAction action, ClientID client_id, bool self_send, const char *msg, int64 data)
{
	/*
	 * Packet: SERVER_CHAT
	 * Function: Sends a chat-packet to the client
	 * Data:
	 *    uint8:  ActionID (see network_data.h, NetworkAction)
	 *    uint32: Client-identifier
	 *    String: Message (max NETWORK_CHAT_LENGTH)
	 *    uint64: Arbitrary data
	 */

	Packet *p = NetworkSend_Init(PACKET_SERVER_CHAT);

	p->Send_uint8 (action);
	p->Send_uint32(client_id);
	p->Send_bool  (self_send);
	p->Send_string(msg);
	p->Send_uint64(data);

	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_ERROR_QUIT)(NetworkClientSocket *cs, ClientID client_id, NetworkErrorCode errorno)
{
	/*
	 * Packet: SERVER_ERROR_QUIT
	 * Function: One of the clients made an error and is quiting the game
	 *      This packet informs the other clients of that.
	 * Data:
	 *    uint32:  Client-identifier
	 *    uint8:  ErrorID (see network_data.h, NetworkErrorCode)
	 */

	Packet *p = NetworkSend_Init(PACKET_SERVER_ERROR_QUIT);

	p->Send_uint32(client_id);
	p->Send_uint8 (errorno);

	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_QUIT)(NetworkClientSocket *cs, ClientID client_id)
{
	/*
	 * Packet: SERVER_ERROR_QUIT
	 * Function: A client left the game, and this packets informs the other clients
	 *      of that.
	 * Data:
	 *    uint32:  Client-identifier
	 */

	Packet *p = NetworkSend_Init(PACKET_SERVER_QUIT);

	p->Send_uint32(client_id);

	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_SHUTDOWN)
{
	/*
	 * Packet: SERVER_SHUTDOWN
	 * Function: Let the clients know that the server is closing
	 * Data:
	 *     <none>
	 */

	Packet *p = NetworkSend_Init(PACKET_SERVER_SHUTDOWN);
	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_NEWGAME)
{
	/*
	 * Packet: PACKET_SERVER_NEWGAME
	 * Function: Let the clients know that the server is loading a new map
	 * Data:
	 *     <none>
	 */

	Packet *p = NetworkSend_Init(PACKET_SERVER_NEWGAME);
	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_RCON)(NetworkClientSocket *cs, uint16 colour, const char *command)
{
	Packet *p = NetworkSend_Init(PACKET_SERVER_RCON);

	p->Send_uint16(colour);
	p->Send_string(command);
	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_MOVE)(NetworkClientSocket *cs, ClientID client_id, CompanyID company_id)
{
	Packet *p = NetworkSend_Init(PACKET_SERVER_MOVE);

	p->Send_uint32(client_id);
	p->Send_uint8(company_id);
	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_COMPANY_UPDATE)(NetworkClientSocket *cs)
{
	Packet *p = NetworkSend_Init(PACKET_SERVER_COMPANY_UPDATE);

	p->Send_uint16(_network_company_passworded);
	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_CONFIG_UPDATE)
{
	Packet *p = NetworkSend_Init(PACKET_SERVER_CONFIG_UPDATE);

	p->Send_uint8(_settings_client.network.max_companies);
	p->Send_uint8(_settings_client.network.max_spectators);
	cs->Send_Packet(p);
}

/***********
 * Receiving functions
 *   DEF_SERVER_RECEIVE_COMMAND has parameter: NetworkClientSocket *cs, Packet *p
 ************/

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_COMPANY_INFO)
{
	SEND_COMMAND(PACKET_SERVER_COMPANY_INFO)(cs);
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_NEWGRFS_CHECKED)
{
	if (cs->status != STATUS_INACTIVE) {
		/* Illegal call, return error and ignore the packet */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
		return;
	}

	NetworkClientInfo *ci = cs->GetInfo();

	/* We now want a password from the client else we do not allow him in! */
	if (!StrEmpty(_settings_client.network.server_password)) {
		SEND_COMMAND(PACKET_SERVER_NEED_PASSWORD)(cs, NETWORK_GAME_PASSWORD);
	} else {
		if (IsValidCompanyID(ci->client_playas) && !StrEmpty(_network_company_states[ci->client_playas].password)) {
			SEND_COMMAND(PACKET_SERVER_NEED_PASSWORD)(cs, NETWORK_COMPANY_PASSWORD);
		} else {
			SEND_COMMAND(PACKET_SERVER_WELCOME)(cs);
		}
	}
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_JOIN)
{
	if (cs->status != STATUS_INACTIVE) {
		/* Illegal call, return error and ignore the packet */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
		return;
	}

	char name[NETWORK_CLIENT_NAME_LENGTH];
	char unique_id[NETWORK_UNIQUE_ID_LENGTH];
	NetworkClientInfo *ci;
	CompanyID playas;
	NetworkLanguage client_lang;
	char client_revision[NETWORK_REVISION_LENGTH];

	p->Recv_string(client_revision, sizeof(client_revision));

	/* Check if the client has revision control enabled */
	if (!IsNetworkCompatibleVersion(client_revision)) {
		/* Different revisions!! */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_WRONG_REVISION);
		return;
	}

	p->Recv_string(name, sizeof(name));
	playas = (Owner)p->Recv_uint8();
	client_lang = (NetworkLanguage)p->Recv_uint8();
	p->Recv_string(unique_id, sizeof(unique_id));

	if (cs->has_quit) return;

	/* join another company does not affect these values */
	switch (playas) {
		case COMPANY_NEW_COMPANY: // New company
			if (ActiveCompanyCount() >= _settings_client.network.max_companies) {
				SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_FULL);
				return;
			}
			break;
		case COMPANY_SPECTATOR: // Spectator
			if (NetworkSpectatorCount() >= _settings_client.network.max_spectators) {
				SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_FULL);
				return;
			}
			break;
		default: // Join another company (companies 1-8 (index 0-7))
			if (!IsValidCompanyID(playas) || !IsHumanCompany(playas)) {
				SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_COMPANY_MISMATCH);
				return;
			}
			break;
	}

	/* We need a valid name.. make it Player */
	if (StrEmpty(name)) strecpy(name, "Player", lastof(name));

	if (!NetworkFindName(name)) { // Change name if duplicate
		/* We could not create a name for this client */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NAME_IN_USE);
		return;
	}

	ci = cs->GetInfo();

	strecpy(ci->client_name, name, lastof(ci->client_name));
	strecpy(ci->unique_id, unique_id, lastof(ci->unique_id));
	ci->client_playas = playas;
	ci->client_lang = client_lang;

	/* Make sure companies to which people try to join are not autocleaned */
	if (IsValidCompanyID(playas)) _network_company_states[playas].months_empty = 0;

	if (_grfconfig == NULL) {
		RECEIVE_COMMAND(PACKET_CLIENT_NEWGRFS_CHECKED)(cs, NULL);
	} else {
		SEND_COMMAND(PACKET_SERVER_CHECK_NEWGRFS)(cs);
	}
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_PASSWORD)
{
	NetworkPasswordType type;
	char password[NETWORK_PASSWORD_LENGTH];
	const NetworkClientInfo *ci;

	type = (NetworkPasswordType)p->Recv_uint8();
	p->Recv_string(password, sizeof(password));

	if (cs->status == STATUS_AUTHORIZING && type == NETWORK_GAME_PASSWORD) {
		/* Check game-password */
		if (strcmp(password, _settings_client.network.server_password) != 0) {
			/* Password is invalid */
			SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_WRONG_PASSWORD);
			return;
		}

		ci = cs->GetInfo();

		if (IsValidCompanyID(ci->client_playas) && !StrEmpty(_network_company_states[ci->client_playas].password)) {
			SEND_COMMAND(PACKET_SERVER_NEED_PASSWORD)(cs, NETWORK_COMPANY_PASSWORD);
			return;
		}

		/* Valid password, allow user */
		SEND_COMMAND(PACKET_SERVER_WELCOME)(cs);
		return;
	} else if (cs->status == STATUS_AUTHORIZING && type == NETWORK_COMPANY_PASSWORD) {
		ci = cs->GetInfo();

		if (strcmp(password, _network_company_states[ci->client_playas].password) != 0) {
			/* Password is invalid */
			SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_WRONG_PASSWORD);
			return;
		}

		SEND_COMMAND(PACKET_SERVER_WELCOME)(cs);
		return;
	}


	SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
	return;
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_GETMAP)
{
	NetworkClientSocket *new_cs;

	/* The client was never joined.. so this is impossible, right?
	 *  Ignore the packet, give the client a warning, and close his connection */
	if (cs->status < STATUS_AUTH || cs->has_quit) {
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_AUTHORIZED);
		return;
	}

	/* Check if someone else is receiving the map */
	FOR_ALL_CLIENT_SOCKETS(new_cs) {
		if (new_cs->status == STATUS_MAP) {
			/* Tell the new client to wait */
			cs->status = STATUS_MAP_WAIT;
			SEND_COMMAND(PACKET_SERVER_WAIT)(cs);
			return;
		}
	}

	/* We receive a request to upload the map.. give it to the client! */
	SEND_COMMAND(PACKET_SERVER_MAP)(cs);
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_MAP_OK)
{
	/* Client has the map, now start syncing */
	if (cs->status == STATUS_DONE_MAP && !cs->has_quit) {
		char client_name[NETWORK_CLIENT_NAME_LENGTH];
		NetworkClientSocket *new_cs;

		NetworkGetClientName(client_name, sizeof(client_name), cs);

		NetworkTextMessage(NETWORK_ACTION_JOIN, CC_DEFAULT, false, client_name);

		/* Mark the client as pre-active, and wait for an ACK
		 *  so we know he is done loading and in sync with us */
		cs->status = STATUS_PRE_ACTIVE;
		NetworkHandleCommandQueue(cs);
		SEND_COMMAND(PACKET_SERVER_FRAME)(cs);
		SEND_COMMAND(PACKET_SERVER_SYNC)(cs);

		/* This is the frame the client receives
		 *  we need it later on to make sure the client is not too slow */
		cs->last_frame = _frame_counter;
		cs->last_frame_server = _frame_counter;

		FOR_ALL_CLIENT_SOCKETS(new_cs) {
			if (new_cs->status > STATUS_AUTH) {
				SEND_COMMAND(PACKET_SERVER_CLIENT_INFO)(new_cs, cs->GetInfo());
				SEND_COMMAND(PACKET_SERVER_JOIN)(new_cs, cs->client_id);
			}
		}

		if (_settings_client.network.pause_on_join) {
			/* Now pause the game till the client is in sync */
			DoCommandP(0, 1, 0, CMD_PAUSE);

			NetworkServerSendChat(NETWORK_ACTION_SERVER_MESSAGE, DESTTYPE_BROADCAST, 0, "", CLIENT_ID_SERVER, NETWORK_SERVER_MESSAGE_GAME_PAUSED_CONNECT);
		}

		/* also update the new client with our max values */
		SEND_COMMAND(PACKET_SERVER_CONFIG_UPDATE)(cs);

		/* quickly update the syncing client with company details */
		SEND_COMMAND(PACKET_SERVER_COMPANY_UPDATE)(cs);
	} else {
		/* Wrong status for this packet, give a warning to client, and close connection */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
	}
}

/** The client has done a command and wants us to handle it
 * @param *cs the connected client that has sent the command
 * @param *p the packet in which the command was sent
 */
DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND)
{
	NetworkClientSocket *new_cs;

	/* The client was never joined.. so this is impossible, right?
	 *  Ignore the packet, give the client a warning, and close his connection */
	if (cs->status < STATUS_DONE_MAP || cs->has_quit) {
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
		return;
	}

	CommandPacket cp;
	const char *err = cs->Recv_Command(p, &cp);

	if (cs->has_quit) return;

	const NetworkClientInfo *ci = cs->GetInfo();

	if (err != NULL) {
		IConsolePrintF(CC_ERROR, "WARNING: %s from client %d (IP: %s).", err, ci->client_id, GetClientIP(ci));
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
		return;
	}


	if (GetCommandFlags(cp.cmd) & CMD_SERVER && ci->client_id != CLIENT_ID_SERVER) {
		IConsolePrintF(CC_ERROR, "WARNING: server only command from client %d (IP: %s), kicking...", ci->client_id, GetClientIP(ci));
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_KICKED);
		return;
	}

	if ((GetCommandFlags(cp.cmd) & CMD_SPECTATOR) == 0 && !IsValidCompanyID(cp.company) && ci->client_id != CLIENT_ID_SERVER) {
		IConsolePrintF(CC_ERROR, "WARNING: spectator issueing command from client %d (IP: %s), kicking...", ci->client_id, GetClientIP(ci));
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_KICKED);
		return;
	}

	/** Only CMD_COMPANY_CTRL is always allowed, for the rest, playas needs
	 * to match the company in the packet. If it doesn't, the client has done
	 * something pretty naughty (or a bug), and will be kicked
	 */
	if (!(cp.cmd == CMD_COMPANY_CTRL && cp.p1 == 0 && ci->client_playas == COMPANY_NEW_COMPANY) && ci->client_playas != cp.company) {
		IConsolePrintF(CC_ERROR, "WARNING: client %d (IP: %s) tried to execute a command as company %d, kicking...",
		               ci->client_playas + 1, GetClientIP(ci), cp.company + 1);
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_COMPANY_MISMATCH);
		return;
	}

	/** @todo CMD_COMPANY_CTRL with p1 = 0 announces a new company to the server. To give the
	 * company the correct ID, the server injects p2 and executes the command. Any other p1
	 * is prohibited. Pretty ugly and should be redone together with its function.
	 * @see CmdCompanyCtrl()
	 */
	if (cp.cmd == CMD_COMPANY_CTRL) {
		if (cp.p1 != 0) {
			SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_CHEATER);
			return;
		}

		/* Check if we are full - else it's possible for spectators to send a CMD_COMPANY_CTRL and the company is created regardless of max_companies! */
		if (ActiveCompanyCount() >= _settings_client.network.max_companies) {
			NetworkServerSendChat(NETWORK_ACTION_SERVER_MESSAGE, DESTTYPE_CLIENT, ci->client_id, "cannot create new company, server full", CLIENT_ID_SERVER);
			return;
		}

		/* XXX - Execute the command as a valid company. Normally this would be done by a
		 * spectator, but that is not allowed any commands. So do an impersonation. The drawback
		 * of this is that the first company's last_built_tile is also updated... */
		cp.company = OWNER_BEGIN;
		cp.p2 = cs->client_id;
	}

	/* The frame can be executed in the same frame as the next frame-packet
	 *  That frame just before that frame is saved in _frame_counter_max */
	cp.frame = _frame_counter_max + 1;
	cp.next  = NULL;

	CommandCallback *callback = cp.callback;

	/* Queue the command for the clients (are send at the end of the frame
	 *   if they can handle it ;)) */
	FOR_ALL_CLIENT_SOCKETS(new_cs) {
		if (new_cs->status >= STATUS_MAP) {
			/* Callbacks are only send back to the client who sent them in the
			 *  first place. This filters that out. */
			cp.callback = (new_cs != cs) ? NULL : callback;
			cp.my_cmd = (new_cs == cs);
			NetworkAddCommandQueue(cp, new_cs);
		}
	}

	cp.callback = NULL;
	cp.my_cmd = false;
	NetworkAddCommandQueue(cp);
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_ERROR)
{
	/* This packets means a client noticed an error and is reporting this
	 *  to us. Display the error and report it to the other clients */
	NetworkClientSocket *new_cs;
	char str[100];
	char client_name[NETWORK_CLIENT_NAME_LENGTH];
	NetworkErrorCode errorno = (NetworkErrorCode)p->Recv_uint8();

	/* The client was never joined.. thank the client for the packet, but ignore it */
	if (cs->status < STATUS_DONE_MAP || cs->has_quit) {
		cs->has_quit = true;
		return;
	}

	NetworkGetClientName(client_name, sizeof(client_name), cs);

	StringID strid = GetNetworkErrorMsg(errorno);
	GetString(str, strid, lastof(str));

	DEBUG(net, 2, "'%s' reported an error and is closing its connection (%s)", client_name, str);

	NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, client_name, NULL, strid);

	FOR_ALL_CLIENT_SOCKETS(new_cs) {
		if (new_cs->status > STATUS_AUTH) {
			SEND_COMMAND(PACKET_SERVER_ERROR_QUIT)(new_cs, cs->client_id, errorno);
		}
	}

	cs->has_quit = true;
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_QUIT)
{
	/* The client wants to leave. Display this and report it to the other
	 *  clients. */
	NetworkClientSocket *new_cs;
	char client_name[NETWORK_CLIENT_NAME_LENGTH];

	/* The client was never joined.. thank the client for the packet, but ignore it */
	if (cs->status < STATUS_DONE_MAP || cs->has_quit) {
		cs->has_quit = true;
		return;
	}

	NetworkGetClientName(client_name, sizeof(client_name), cs);

	NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, client_name, NULL, STR_NETWORK_CLIENT_LEAVING);

	FOR_ALL_CLIENT_SOCKETS(new_cs) {
		if (new_cs->status > STATUS_AUTH) {
			SEND_COMMAND(PACKET_SERVER_QUIT)(new_cs, cs->client_id);
		}
	}

	cs->has_quit = true;
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_ACK)
{
	if (cs->status < STATUS_AUTH) {
		/* Illegal call, return error and ignore the packet */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_AUTHORIZED);
		return;
	}

	uint32 frame = p->Recv_uint32();

	/* The client is trying to catch up with the server */
	if (cs->status == STATUS_PRE_ACTIVE) {
		/* The client is not yet catched up? */
		if (frame + DAY_TICKS < _frame_counter) return;

		/* Now he is! Unpause the game */
		cs->status = STATUS_ACTIVE;

		if (_settings_client.network.pause_on_join) {
			DoCommandP(0, 0, 0, CMD_PAUSE);
			NetworkServerSendChat(NETWORK_ACTION_SERVER_MESSAGE, DESTTYPE_BROADCAST, 0, "", CLIENT_ID_SERVER, NETWORK_SERVER_MESSAGE_GAME_UNPAUSED_CONNECT);
		}

		/* Execute script for, e.g. MOTD */
		IConsoleCmdExec("exec scripts/on_server_connect.scr 0");
	}

	/* The client received the frame, make note of it */
	cs->last_frame = frame;
	/* With those 2 values we can calculate the lag realtime */
	cs->last_frame_server = _frame_counter;
}



void NetworkServerSendChat(NetworkAction action, DestType desttype, int dest, const char *msg, ClientID from_id, int64 data)
{
	NetworkClientSocket *cs;
	const NetworkClientInfo *ci, *ci_own, *ci_to;

	switch (desttype) {
	case DESTTYPE_CLIENT:
		/* Are we sending to the server? */
		if ((ClientID)dest == CLIENT_ID_SERVER) {
			ci = NetworkFindClientInfoFromClientID(from_id);
			/* Display the text locally, and that is it */
			if (ci != NULL)
				NetworkTextMessage(action, (ConsoleColour)GetDrawStringCompanyColour(ci->client_playas), false, ci->client_name, msg, data);
		} else {
			/* Else find the client to send the message to */
			FOR_ALL_CLIENT_SOCKETS(cs) {
				if (cs->client_id == (ClientID)dest) {
					SEND_COMMAND(PACKET_SERVER_CHAT)(cs, action, from_id, false, msg, data);
					break;
				}
			}
		}

		/* Display the message locally (so you know you have sent it) */
		if (from_id != (ClientID)dest) {
			if (from_id == CLIENT_ID_SERVER) {
				ci = NetworkFindClientInfoFromClientID(from_id);
				ci_to = NetworkFindClientInfoFromClientID((ClientID)dest);
				if (ci != NULL && ci_to != NULL)
					NetworkTextMessage(action, (ConsoleColour)GetDrawStringCompanyColour(ci->client_playas), true, ci_to->client_name, msg, data);
			} else {
				FOR_ALL_CLIENT_SOCKETS(cs) {
					if (cs->client_id == from_id) {
						SEND_COMMAND(PACKET_SERVER_CHAT)(cs, action, (ClientID)dest, true, msg, data);
						break;
					}
				}
			}
		}
		break;
	case DESTTYPE_TEAM: {
		bool show_local = true; // If this is false, the message is already displayed
														/* on the client who did sent it.
		 * Find all clients that belong to this company */
		ci_to = NULL;
		FOR_ALL_CLIENT_SOCKETS(cs) {
			ci = cs->GetInfo();
			if (ci->client_playas == (CompanyID)dest) {
				SEND_COMMAND(PACKET_SERVER_CHAT)(cs, action, from_id, false, msg, data);
				if (cs->client_id == from_id) show_local = false;
				ci_to = ci; // Remember a client that is in the company for company-name
			}
		}

		ci = NetworkFindClientInfoFromClientID(from_id);
		ci_own = NetworkFindClientInfoFromClientID(CLIENT_ID_SERVER);
		if (ci != NULL && ci_own != NULL && ci_own->client_playas == dest) {
			NetworkTextMessage(action, (ConsoleColour)GetDrawStringCompanyColour(ci->client_playas), false, ci->client_name, msg, data);
			if (from_id == CLIENT_ID_SERVER) show_local = false;
			ci_to = ci_own;
		}

		/* There is no such client */
		if (ci_to == NULL) break;

		/* Display the message locally (so you know you have sent it) */
		if (ci != NULL && show_local) {
			if (from_id == CLIENT_ID_SERVER) {
				char name[NETWORK_NAME_LENGTH];
				StringID str = IsValidCompanyID(ci_to->client_playas) ? STR_COMPANY_NAME : STR_NETWORK_SPECTATORS;
				SetDParam(0, ci_to->client_playas);
				GetString(name, str, lastof(name));
				NetworkTextMessage(action, (ConsoleColour)GetDrawStringCompanyColour(ci_own->client_playas), true, name, msg, data);
			} else {
				FOR_ALL_CLIENT_SOCKETS(cs) {
					if (cs->client_id == from_id) {
						SEND_COMMAND(PACKET_SERVER_CHAT)(cs, action, ci_to->client_id, true, msg, data);
					}
				}
			}
		}
		}
		break;
	default:
		DEBUG(net, 0, "[server] received unknown chat destination type %d. Doing broadcast instead", desttype);
		/* fall-through to next case */
	case DESTTYPE_BROADCAST:
		FOR_ALL_CLIENT_SOCKETS(cs) {
			SEND_COMMAND(PACKET_SERVER_CHAT)(cs, action, from_id, false, msg, data);
		}
		ci = NetworkFindClientInfoFromClientID(from_id);
		if (ci != NULL)
			NetworkTextMessage(action, (ConsoleColour)GetDrawStringCompanyColour(ci->client_playas), false, ci->client_name, msg, data);
		break;
	}
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_CHAT)
{
	if (cs->status < STATUS_AUTH) {
		/* Illegal call, return error and ignore the packet */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_AUTHORIZED);
		return;
	}

	NetworkAction action = (NetworkAction)p->Recv_uint8();
	DestType desttype = (DestType)p->Recv_uint8();
	int dest = p->Recv_uint32();
	char msg[NETWORK_CHAT_LENGTH];

	p->Recv_string(msg, NETWORK_CHAT_LENGTH);
	int64 data = p->Recv_uint64();

	const NetworkClientInfo *ci = cs->GetInfo();
	switch (action) {
		case NETWORK_ACTION_GIVE_MONEY:
			if (!IsValidCompanyID(ci->client_playas)) break;
			/* Fall-through */
		case NETWORK_ACTION_CHAT:
		case NETWORK_ACTION_CHAT_CLIENT:
		case NETWORK_ACTION_CHAT_COMPANY:
			NetworkServerSendChat(action, desttype, dest, msg, cs->client_id, data);
			break;
		default:
			IConsolePrintF(CC_ERROR, "WARNING: invalid chat action from client %d (IP: %s).", ci->client_id, GetClientIP(ci));
			SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
			break;
	}
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_SET_PASSWORD)
{
	if (cs->status != STATUS_ACTIVE) {
		/* Illegal call, return error and ignore the packet */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
		return;
	}

	char password[NETWORK_PASSWORD_LENGTH];
	const NetworkClientInfo *ci;

	p->Recv_string(password, sizeof(password));
	ci = cs->GetInfo();

	if (IsValidCompanyID(ci->client_playas)) {
		strecpy(_network_company_states[ci->client_playas].password, password, lastof(_network_company_states[ci->client_playas].password));
		NetworkServerUpdateCompanyPassworded(ci->client_playas, !StrEmpty(_network_company_states[ci->client_playas].password));
	}
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_SET_NAME)
{
	if (cs->status != STATUS_ACTIVE) {
		/* Illegal call, return error and ignore the packet */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
		return;
	}

	char client_name[NETWORK_CLIENT_NAME_LENGTH];
	NetworkClientInfo *ci;

	p->Recv_string(client_name, sizeof(client_name));
	ci = cs->GetInfo();

	if (cs->has_quit) return;

	if (ci != NULL) {
		/* Display change */
		if (NetworkFindName(client_name)) {
			NetworkTextMessage(NETWORK_ACTION_NAME_CHANGE, CC_DEFAULT, false, ci->client_name, client_name);
			strecpy(ci->client_name, client_name, lastof(ci->client_name));
			NetworkUpdateClientInfo(ci->client_id);
		}
	}
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_RCON)
{
	char pass[NETWORK_PASSWORD_LENGTH];
	char command[NETWORK_RCONCOMMAND_LENGTH];

	if (StrEmpty(_settings_client.network.rcon_password)) return;

	p->Recv_string(pass, sizeof(pass));
	p->Recv_string(command, sizeof(command));

	if (strcmp(pass, _settings_client.network.rcon_password) != 0) {
		DEBUG(net, 0, "[rcon] wrong password from client-id %d", cs->client_id);
		return;
	}

	DEBUG(net, 0, "[rcon] client-id %d executed: '%s'", cs->client_id, command);

	_redirect_console_to_client = cs->client_id;
	IConsoleCmdExec(command);
	_redirect_console_to_client = INVALID_CLIENT_ID;
	return;
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_MOVE)
{
	CompanyID company_id = (Owner)p->Recv_uint8();

	/* Check if the company is valid */
	if (!IsValidCompanyID(company_id) && company_id != COMPANY_SPECTATOR) return;

	/* Check if we require a password for this company */
	if (company_id != COMPANY_SPECTATOR && !StrEmpty(_network_company_states[company_id].password)) {
		/* we need a password from the client - should be in this packet */
		char password[NETWORK_PASSWORD_LENGTH];
		p->Recv_string(password, sizeof(password));

		/* Incorrect password sent, return! */
		if (strcmp(password, _network_company_states[company_id].password) != 0) {
			DEBUG(net, 2, "[move] wrong password from client-id #%d for company #%d", cs->client_id, company_id + 1);
			return;
		}
	}

	/* if we get here we can move the client */
	NetworkServerDoMove(cs->client_id, company_id);
}

/* The layout for the receive-functions by the server */
typedef void NetworkServerPacket(NetworkClientSocket *cs, Packet *p);


/* This array matches PacketType. At an incoming
 *  packet it is matches against this array
 *  and that way the right function to handle that
 *  packet is found. */
static NetworkServerPacket * const _network_server_packet[] = {
	NULL, // PACKET_SERVER_FULL,
	NULL, // PACKET_SERVER_BANNED,
	RECEIVE_COMMAND(PACKET_CLIENT_JOIN),
	NULL, // PACKET_SERVER_ERROR,
	RECEIVE_COMMAND(PACKET_CLIENT_COMPANY_INFO),
	NULL, // PACKET_SERVER_COMPANY_INFO,
	NULL, // PACKET_SERVER_CLIENT_INFO,
	NULL, // PACKET_SERVER_NEED_PASSWORD,
	RECEIVE_COMMAND(PACKET_CLIENT_PASSWORD),
	NULL, // PACKET_SERVER_WELCOME,
	RECEIVE_COMMAND(PACKET_CLIENT_GETMAP),
	NULL, // PACKET_SERVER_WAIT,
	NULL, // PACKET_SERVER_MAP,
	RECEIVE_COMMAND(PACKET_CLIENT_MAP_OK),
	NULL, // PACKET_SERVER_JOIN,
	NULL, // PACKET_SERVER_FRAME,
	NULL, // PACKET_SERVER_SYNC,
	RECEIVE_COMMAND(PACKET_CLIENT_ACK),
	RECEIVE_COMMAND(PACKET_CLIENT_COMMAND),
	NULL, // PACKET_SERVER_COMMAND,
	RECEIVE_COMMAND(PACKET_CLIENT_CHAT),
	NULL, // PACKET_SERVER_CHAT,
	RECEIVE_COMMAND(PACKET_CLIENT_SET_PASSWORD),
	RECEIVE_COMMAND(PACKET_CLIENT_SET_NAME),
	RECEIVE_COMMAND(PACKET_CLIENT_QUIT),
	RECEIVE_COMMAND(PACKET_CLIENT_ERROR),
	NULL, // PACKET_SERVER_QUIT,
	NULL, // PACKET_SERVER_ERROR_QUIT,
	NULL, // PACKET_SERVER_SHUTDOWN,
	NULL, // PACKET_SERVER_NEWGAME,
	NULL, // PACKET_SERVER_RCON,
	RECEIVE_COMMAND(PACKET_CLIENT_RCON),
	NULL, // PACKET_CLIENT_CHECK_NEWGRFS,
	RECEIVE_COMMAND(PACKET_CLIENT_NEWGRFS_CHECKED),
	NULL, // PACKET_SERVER_MOVE,
	RECEIVE_COMMAND(PACKET_CLIENT_MOVE),
	NULL, // PACKET_SERVER_COMPANY_UPDATE,
	NULL, // PACKET_SERVER_CONFIG_UPDATE,
};

/* If this fails, check the array above with network_data.h */
assert_compile(lengthof(_network_server_packet) == PACKET_END);

void NetworkSocketHandler::Send_CompanyInformation(Packet *p, const Company *c, const NetworkCompanyStats *stats)
{
	/* Grab the company name */
	char company_name[NETWORK_COMPANY_NAME_LENGTH];
	SetDParam(0, c->index);
	GetString(company_name, STR_COMPANY_NAME, lastof(company_name));

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

	for (int i = 0; i < NETWORK_VEHICLE_TYPES; i++) {
		p->Send_uint16(stats->num_vehicle[i]);
	}

	for (int i = 0; i < NETWORK_STATION_TYPES; i++) {
		p->Send_uint16(stats->num_station[i]);
	}
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
		if (!IsValidCompanyID(v->owner) || !v->IsPrimaryVehicle()) continue;
		byte type = 0;
		switch (v->type) {
			case VEH_TRAIN: type = 0; break;
			case VEH_ROAD: type = (v->cargo_type != CT_PASSENGERS) ? 1 : 2; break;
			case VEH_AIRCRAFT: type = 3; break;
			case VEH_SHIP: type = 4; break;
			default: continue;
		}
		stats[v->owner].num_vehicle[type]++;
	}

	/* Go through all stations and count the types of stations */
	FOR_ALL_STATIONS(s) {
		if (IsValidCompanyID(s->owner)) {
			NetworkCompanyStats *npi = &stats[s->owner];

			if (s->facilities & FACIL_TRAIN)      npi->num_station[0]++;
			if (s->facilities & FACIL_TRUCK_STOP) npi->num_station[1]++;
			if (s->facilities & FACIL_BUS_STOP)   npi->num_station[2]++;
			if (s->facilities & FACIL_AIRPORT)    npi->num_station[3]++;
			if (s->facilities & FACIL_DOCK)       npi->num_station[4]++;
		}
	}
}

/* Send a packet to all clients with updated info about this client_id */
void NetworkUpdateClientInfo(ClientID client_id)
{
	NetworkClientSocket *cs;
	NetworkClientInfo *ci = NetworkFindClientInfoFromClientID(client_id);

	if (ci == NULL) return;

	FOR_ALL_CLIENT_SOCKETS(cs) {
		SEND_COMMAND(PACKET_SERVER_CLIENT_INFO)(cs, ci);
	}
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
		if (IsValidCompanyID(ci->client_playas)) clients_in_company[ci->client_playas] = true;
	}

	if (!_network_dedicated) {
		ci = NetworkFindClientInfoFromClientID(CLIENT_ID_SERVER);
		if (IsValidCompanyID(ci->client_playas)) clients_in_company[ci->client_playas] = true;
	}

	if (_settings_client.network.autoclean_novehicles != 0) {
		memset(vehicles_in_company, 0, sizeof(vehicles_in_company));

		const Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (!IsValidCompanyID(v->owner) || !v->IsPrimaryVehicle()) continue;
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
				DoCommandP(0, 2, c->index, CMD_COMPANY_CTRL);
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
				DoCommandP(0, 2, c->index, CMD_COMPANY_CTRL);
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

/* Reads a packet from the stream */
bool NetworkServer_ReadPackets(NetworkClientSocket *cs)
{
	Packet *p;
	NetworkRecvStatus res;
	while ((p = cs->Recv_Packet(&res)) != NULL) {
		byte type = p->Recv_uint8();
		if (type < PACKET_END && _network_server_packet[type] != NULL && !cs->has_quit) {
			_network_server_packet[type](cs, p);
		} else {
			DEBUG(net, 0, "[server] received invalid packet type %d", type);
		}
		delete p;
	}

	return true;
}

/* Handle the local command-queue */
static void NetworkHandleCommandQueue(NetworkClientSocket *cs)
{
	CommandPacket *cp;

	while ( (cp = cs->command_queue) != NULL) {
		SEND_COMMAND(PACKET_SERVER_COMMAND)(cs, cp);

		cs->command_queue = cp->next;
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
		/* Check if the speed of the client is what we can expect from a client */
		if (cs->status == STATUS_ACTIVE) {
			/* 1 lag-point per day */
			int lag = NetworkCalculateLag(cs) / DAY_TICKS;
			if (lag > 0) {
				if (lag > 3) {
					/* Client did still not report in after 4 game-day, drop him
					 *  (that is, the 3 of above, + 1 before any lag is counted) */
					IConsolePrintF(CC_ERROR,"Client #%d is dropped because the client did not respond for more than 4 game-days", cs->client_id);
					NetworkCloseClient(cs);
					continue;
				}

				/* Report once per time we detect the lag */
				if (cs->lag_test == 0) {
					IConsolePrintF(CC_WARNING,"[%d] Client #%d is slow, try increasing *net_frame_freq to a higher value!", _frame_counter, cs->client_id);
					cs->lag_test = 1;
				}
			} else {
				cs->lag_test = 0;
			}
		} else if (cs->status == STATUS_PRE_ACTIVE) {
			int lag = NetworkCalculateLag(cs);
			if (lag > _settings_client.network.max_join_time) {
				IConsolePrintF(CC_ERROR,"Client #%d is dropped because it took longer than %d ticks for him to join", cs->client_id, _settings_client.network.max_join_time);
				NetworkCloseClient(cs);
			}
		} else if (cs->status == STATUS_INACTIVE) {
			int lag = NetworkCalculateLag(cs);
			if (lag > 4 * DAY_TICKS) {
				IConsolePrintF(CC_ERROR,"Client #%d is dropped because it took longer than %d ticks to start the joining process", cs->client_id, 4 * DAY_TICKS);
				NetworkCloseClient(cs);
			}
		}

		if (cs->status >= STATUS_PRE_ACTIVE) {
			/* Check if we can send command, and if we have anything in the queue */
			NetworkHandleCommandQueue(cs);

			/* Send an updated _frame_counter_max to the client */
			if (send_frame) SEND_COMMAND(PACKET_SERVER_FRAME)(cs);

#ifndef ENABLE_NETWORK_SYNC_EVERY_FRAME
			/* Send a sync-check packet */
			if (send_sync) SEND_COMMAND(PACKET_SERVER_SYNC)(cs);
#endif
		}
	}

	/* See if we need to advertise */
	NetworkUDPAdvertise();
}

void NetworkServerYearlyLoop()
{
	NetworkCheckRestartMap();
}

void NetworkServerMonthlyLoop()
{
	NetworkAutoCleanCompanies();
}

void NetworkServerChangeOwner(Owner current_owner, Owner new_owner)
{
	/* The server has to handle all administrative issues, for example
	 * updating and notifying all clients of what has happened */
	NetworkClientInfo *ci = NetworkFindClientInfoFromClientID(CLIENT_ID_SERVER);

	/* The server has just changed from owner */
	if (current_owner == ci->client_playas) {
		ci->client_playas = new_owner;
		NetworkUpdateClientInfo(CLIENT_ID_SERVER);
	}

	/* Find all clients that were in control of this company, and mark them as new_owner */
	FOR_ALL_CLIENT_INFOS(ci) {
		if (current_owner == ci->client_playas) {
			ci->client_playas = new_owner;
			NetworkUpdateClientInfo(ci->client_id);
		}
	}
}

const char *GetClientIP(const NetworkClientInfo *ci)
{
	struct in_addr addr;

	addr.s_addr = ci->client_ip;
	return inet_ntoa(addr);
}

void NetworkServerShowStatusToConsole()
{
	static const char * const stat_str[] = {
		"inactive",
		"authorizing",
		"authorized",
		"waiting",
		"loading map",
		"map done",
		"ready",
		"active"
	};

	NetworkClientSocket *cs;
	FOR_ALL_CLIENT_SOCKETS(cs) {
		int lag = NetworkCalculateLag(cs);
		const NetworkClientInfo *ci = cs->GetInfo();
		const char *status;

		status = (cs->status < (ptrdiff_t)lengthof(stat_str) ? stat_str[cs->status] : "unknown");
		IConsolePrintF(CC_INFO, "Client #%1d  name: '%s'  status: '%s'  frame-lag: %3d  company: %1d  IP: %s  unique-id: '%s'",
			cs->client_id, ci->client_name, status, lag,
			ci->client_playas + (IsValidCompanyID(ci->client_playas) ? 1 : 0),
			GetClientIP(ci), ci->unique_id);
	}
}

/**
 * Send Config Update
 */
void NetworkServerSendConfigUpdate()
{
	NetworkClientSocket *cs;

	FOR_ALL_CLIENT_SOCKETS(cs) {
		SEND_COMMAND(PACKET_SERVER_CONFIG_UPDATE)(cs);
	}
}

void NetworkServerUpdateCompanyPassworded(CompanyID company_id, bool passworded)
{
	if (NetworkCompanyIsPassworded(company_id) == passworded) return;

	SB(_network_company_passworded, company_id, 1, !!passworded);
	InvalidateWindowClasses(WC_COMPANY);

	NetworkClientSocket *cs;
	FOR_ALL_CLIENT_SOCKETS(cs) {
		SEND_COMMAND(PACKET_SERVER_COMPANY_UPDATE)(cs);
	}
}

/**
 * Handle the tid-bits of moving a client from one company to another.
 * @param client_id id of the client we want to move.
 * @param company_id id of the company we want to move the client to.
 * @return void
 **/
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
		SEND_COMMAND(PACKET_SERVER_MOVE)(NetworkFindClientStateFromClientID(client_id), client_id, company_id);
	}

	/* announce the client's move */
	NetworkUpdateClientInfo(client_id);

	NetworkAction action = (company_id == COMPANY_SPECTATOR) ? NETWORK_ACTION_COMPANY_SPECTATOR : NETWORK_ACTION_COMPANY_JOIN;
	NetworkServerSendChat(action, DESTTYPE_BROADCAST, 0, "", client_id, company_id + 1);
}

void NetworkServerSendRcon(ClientID client_id, ConsoleColour colour_code, const char *string)
{
	SEND_COMMAND(PACKET_SERVER_RCON)(NetworkFindClientStateFromClientID(client_id), colour_code, string);
}

void NetworkServerSendError(ClientID client_id, NetworkErrorCode error)
{
	SEND_COMMAND(PACKET_SERVER_ERROR)(NetworkFindClientStateFromClientID(client_id), error);
}

void NetworkServerKickClient(ClientID client_id)
{
	if (client_id == CLIENT_ID_SERVER) return;
	NetworkServerSendError(client_id, NETWORK_ERROR_KICKED);
}

void NetworkServerBanIP(const char *banip)
{
	const NetworkClientInfo *ci;
	uint32 ip_number = inet_addr(banip);

	/* There can be multiple clients with the same IP, kick them all */
	FOR_ALL_CLIENT_INFOS(ci) {
		if (ci->client_ip == ip_number) {
			NetworkServerKickClient(ci->client_id);
		}
	}

	/* Add user to ban-list */
	for (uint index = 0; index < lengthof(_network_ban_list); index++) {
		if (_network_ban_list[index] == NULL) {
			_network_ban_list[index] = strdup(banip);
			break;
		}
	}
}

bool NetworkCompanyHasClients(CompanyID company)
{
	const NetworkClientInfo *ci;
	FOR_ALL_CLIENT_INFOS(ci) {
		if (ci->client_playas == company) return true;
	}
	return false;
}

#endif /* ENABLE_NETWORK */
