/* $Id$ */

/** @file network_client.cpp Client part of the network protocol. */

#ifdef ENABLE_NETWORK

#include "../stdafx.h"
#include "../debug.h"
#include "../openttd.h"
#include "../gfx_func.h"
#include "network_internal.h"
#include "network_gui.h"
#include "../saveload/saveload.h"
#include "../command_func.h"
#include "../console_func.h"
#include "../fileio_func.h"
#include "../md5.h"
#include "../strings_func.h"
#include "../window_func.h"
#include "../string_func.h"
#include "../company_func.h"
#include "../company_base.h"
#include "../company_gui.h"
#include "../settings_type.h"
#include "../rev.h"

#include "table/strings.h"

/* This file handles all the client-commands */


/* So we don't make too much typos ;) */
#define MY_CLIENT GetNetworkClientSocket(0)

static uint32 last_ack_frame;

/** One bit of 'entropy' used to generate a salt for the company passwords. */
static uint32 _password_game_seed;
/** The other bit of 'entropy' used to generate a salt for the company passwords. */
static char _password_server_unique_id[NETWORK_UNIQUE_ID_LENGTH];

/** Maximum number of companies of the currently joined server. */
static uint8 _network_server_max_companies;
/** Maximum number of spectators of the currently joined server. */
static uint8 _network_server_max_spectators;

/** Make sure the unique ID length is the same as a md5 hash. */
assert_compile(NETWORK_UNIQUE_ID_LENGTH == 16 * 2 + 1);

/**
 * Generates a hashed password for the company name.
 * @param password the password to 'encrypt'.
 * @return the hashed password.
 */
static const char *GenerateCompanyPasswordHash(const char *password)
{
	if (StrEmpty(password)) return password;

	char salted_password[NETWORK_UNIQUE_ID_LENGTH];

	memset(salted_password, 0, sizeof(salted_password));
	snprintf(salted_password, sizeof(salted_password), "%s", password);
	/* Add the game seed and the server's unique ID as the salt. */
	for (uint i = 0; i < NETWORK_UNIQUE_ID_LENGTH - 1; i++) salted_password[i] ^= _password_server_unique_id[i] ^ (_password_game_seed >> i);

	Md5 checksum;
	uint8 digest[16];
	static char hashed_password[NETWORK_UNIQUE_ID_LENGTH];

	/* Generate the MD5 hash */
	checksum.Append((const uint8*)salted_password, sizeof(salted_password) - 1);
	checksum.Finish(digest);

	for (int di = 0; di < 16; di++) sprintf(hashed_password + di * 2, "%02x", digest[di]);
	hashed_password[lengthof(hashed_password) - 1] = '\0';

	return hashed_password;
}

/**
 * Hash the current company password; used when the server 'company' sets his/her password.
 */
void HashCurrentCompanyPassword(const char *password)
{
	_password_game_seed = _settings_game.game_creation.generation_seed;
	strecpy(_password_server_unique_id, _settings_client.network.network_id, lastof(_password_server_unique_id));

	const char *new_pw = GenerateCompanyPasswordHash(password);
	strecpy(_network_company_states[_local_company].password, new_pw, lastof(_network_company_states[_local_company].password));

	if (_network_server) {
		NetworkServerUpdateCompanyPassworded(_local_company, !StrEmpty(_network_company_states[_local_company].password));
	}
}


/***********
 * Sending functions
 *   DEF_CLIENT_SEND_COMMAND has no parameters
 ************/

DEF_CLIENT_SEND_COMMAND(PACKET_CLIENT_COMPANY_INFO)
{
	/*
	 * Packet: CLIENT_COMPANY_INFO
	 * Function: Request company-info (in detail)
	 * Data:
	 *    <none>
	 */
	Packet *p;
	_network_join_status = NETWORK_JOIN_STATUS_GETTING_COMPANY_INFO;
	InvalidateWindow(WC_NETWORK_STATUS_WINDOW, 0);

	p = NetworkSend_Init(PACKET_CLIENT_COMPANY_INFO);
	MY_CLIENT->Send_Packet(p);
}

DEF_CLIENT_SEND_COMMAND(PACKET_CLIENT_JOIN)
{
	/*
	 * Packet: CLIENT_JOIN
	 * Function: Try to join the server
	 * Data:
	 *    String: OpenTTD Revision (norev000 if no revision)
	 *    String: Client Name (max NETWORK_NAME_LENGTH)
	 *    uint8:  Play as Company id (1..MAX_COMPANIES)
	 *    uint8:  Language ID
	 *    String: Unique id to find the client back in server-listing
	 */

	Packet *p;
	_network_join_status = NETWORK_JOIN_STATUS_AUTHORIZING;
	InvalidateWindow(WC_NETWORK_STATUS_WINDOW, 0);

	p = NetworkSend_Init(PACKET_CLIENT_JOIN);
	p->Send_string(_openttd_revision);
	p->Send_string(_settings_client.network.client_name); // Client name
	p->Send_uint8 (_network_playas);      // PlayAs
	p->Send_uint8 (NETLANG_ANY);          // Language
	p->Send_string(_settings_client.network.network_id);
	MY_CLIENT->Send_Packet(p);
}

DEF_CLIENT_SEND_COMMAND(PACKET_CLIENT_NEWGRFS_CHECKED)
{
	/*
	 * Packet: CLIENT_NEWGRFS_CHECKED
	 * Function: Tell the server that we have the required GRFs
	 * Data:
	 */

	Packet *p = NetworkSend_Init(PACKET_CLIENT_NEWGRFS_CHECKED);
	MY_CLIENT->Send_Packet(p);
}

DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_PASSWORD)(NetworkPasswordType type, const char *password)
{
	/*
	 * Packet: CLIENT_PASSWORD
	 * Function: Send a password to the server to authorize
	 * Data:
	 *    uint8:  NetworkPasswordType
	 *    String: Password
	 */
	Packet *p = NetworkSend_Init(PACKET_CLIENT_PASSWORD);
	p->Send_uint8 (type);
	p->Send_string(type == NETWORK_GAME_PASSWORD ? password : GenerateCompanyPasswordHash(password));
	MY_CLIENT->Send_Packet(p);
}

DEF_CLIENT_SEND_COMMAND(PACKET_CLIENT_GETMAP)
{
	/*
	 * Packet: CLIENT_GETMAP
	 * Function: Request the map from the server
	 * Data:
	 *    <none>
	 */

	Packet *p = NetworkSend_Init(PACKET_CLIENT_GETMAP);
	MY_CLIENT->Send_Packet(p);
}

DEF_CLIENT_SEND_COMMAND(PACKET_CLIENT_MAP_OK)
{
	/*
	 * Packet: CLIENT_MAP_OK
	 * Function: Tell the server that we are done receiving/loading the map
	 * Data:
	 *    <none>
	 */

	Packet *p = NetworkSend_Init(PACKET_CLIENT_MAP_OK);
	MY_CLIENT->Send_Packet(p);
}

DEF_CLIENT_SEND_COMMAND(PACKET_CLIENT_ACK)
{
	/*
	 * Packet: CLIENT_ACK
	 * Function: Tell the server we are done with this frame
	 * Data:
	 *    uint32: current FrameCounter of the client
	 */

	Packet *p = NetworkSend_Init(PACKET_CLIENT_ACK);

	p->Send_uint32(_frame_counter);
	MY_CLIENT->Send_Packet(p);
}

/* Send a command packet to the server */
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_COMMAND)(const CommandPacket *cp)
{
	/*
	 * Packet: CLIENT_COMMAND
	 * Function: Send a DoCommand to the Server
	 * Data:
	 *    uint8:  CompanyID (0..MAX_COMPANIES-1)
	 *    uint32: CommandID (see command.h)
	 *    uint32: P1 (free variables used in DoCommand)
	 *    uint32: P2
	 *    uint32: Tile
	 *    string: text
	 *    uint8:  CallBackID (see callback_table.c)
	 */

	Packet *p = NetworkSend_Init(PACKET_CLIENT_COMMAND);
	MY_CLIENT->Send_Command(p, cp);

	MY_CLIENT->Send_Packet(p);
}

/* Send a chat-packet over the network */
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_CHAT)(NetworkAction action, DestType type, int dest, const char *msg, int64 data)
{
	/*
	 * Packet: CLIENT_CHAT
	 * Function: Send a chat-packet to the serve
	 * Data:
	 *    uint8:  ActionID (see network_data.h, NetworkAction)
	 *    uint8:  Destination Type (see network_data.h, DestType);
	 *    uint32: Destination CompanyID/Client-identifier
	 *    String: Message (max NETWORK_CHAT_LENGTH)
	 *    uint64: Some arbitrary number
	 */

	Packet *p = NetworkSend_Init(PACKET_CLIENT_CHAT);

	p->Send_uint8 (action);
	p->Send_uint8 (type);
	p->Send_uint32(dest);
	p->Send_string(msg);
	p->Send_uint64(data);

	MY_CLIENT->Send_Packet(p);
}

/* Send an error-packet over the network */
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_ERROR)(NetworkErrorCode errorno)
{
	/*
	 * Packet: CLIENT_ERROR
	 * Function: The client made an error and is quiting the game
	 * Data:
	 *    uint8:  ErrorID (see network_data.h, NetworkErrorCode)
	 */
	Packet *p = NetworkSend_Init(PACKET_CLIENT_ERROR);

	p->Send_uint8(errorno);
	MY_CLIENT->Send_Packet(p);
}

DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_SET_PASSWORD)(const char *password)
{
	/*
	 * Packet: PACKET_CLIENT_SET_PASSWORD
	 * Function: Set the password for the clients current company
	 * Data:
	 *    String: Password
	 */
	Packet *p = NetworkSend_Init(PACKET_CLIENT_SET_PASSWORD);

	p->Send_string(GenerateCompanyPasswordHash(password));
	MY_CLIENT->Send_Packet(p);
}

DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_SET_NAME)(const char *name)
{
	/*
	 * Packet: PACKET_CLIENT_SET_NAME
	 * Function: Gives the client a new name
	 * Data:
	 *    String: Name
	 */
	Packet *p = NetworkSend_Init(PACKET_CLIENT_SET_NAME);

	p->Send_string(name);
	MY_CLIENT->Send_Packet(p);
}

/* Send an quit-packet over the network */
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_QUIT)()
{
	/*
	 * Packet: CLIENT_QUIT
	 * Function: The client is quiting the game
	 * Data:
	 */
	Packet *p = NetworkSend_Init(PACKET_CLIENT_QUIT);

	MY_CLIENT->Send_Packet(p);
}

DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_RCON)(const char *pass, const char *command)
{
	Packet *p = NetworkSend_Init(PACKET_CLIENT_RCON);
	p->Send_string(pass);
	p->Send_string(command);
	MY_CLIENT->Send_Packet(p);
}

DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_MOVE)(CompanyID company, const char *pass)
{
	Packet *p = NetworkSend_Init(PACKET_CLIENT_MOVE);
	p->Send_uint8(company);
	p->Send_string(GenerateCompanyPasswordHash(pass));
	MY_CLIENT->Send_Packet(p);
}


/***********
 * Receiving functions
 *   DEF_CLIENT_RECEIVE_COMMAND has parameter: Packet *p
 ************/

extern bool SafeSaveOrLoad(const char *filename, int mode, GameMode newgm, Subdirectory subdir);
extern StringID _switch_mode_errorstr;

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_FULL)
{
	/* We try to join a server which is full */
	_switch_mode_errorstr = STR_NETWORK_ERR_SERVER_FULL;
	DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);

	return NETWORK_RECV_STATUS_SERVER_FULL;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_BANNED)
{
	/* We try to join a server where we are banned */
	_switch_mode_errorstr = STR_NETWORK_ERR_SERVER_BANNED;
	DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);

	return NETWORK_RECV_STATUS_SERVER_BANNED;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_COMPANY_INFO)
{
	byte company_info_version = p->Recv_uint8();

	if (!MY_CLIENT->has_quit && company_info_version == NETWORK_COMPANY_INFO_VERSION) {
		/* We have received all data... (there are no more packets coming) */
		if (!p->Recv_bool()) return NETWORK_RECV_STATUS_CLOSE_QUERY;

		CompanyID current = (Owner)p->Recv_uint8();
		if (current >= MAX_COMPANIES) return NETWORK_RECV_STATUS_CLOSE_QUERY;

		NetworkCompanyInfo *company_info = GetLobbyCompanyInfo(current);
		if (company_info == NULL) return NETWORK_RECV_STATUS_CLOSE_QUERY;

		p->Recv_string(company_info->company_name, sizeof(company_info->company_name));
		company_info->inaugurated_year = p->Recv_uint32();
		company_info->company_value    = p->Recv_uint64();
		company_info->money            = p->Recv_uint64();
		company_info->income           = p->Recv_uint64();
		company_info->performance      = p->Recv_uint16();
		company_info->use_password     = p->Recv_bool();
		for (int i = 0; i < NETWORK_VEHICLE_TYPES; i++)
			company_info->num_vehicle[i] = p->Recv_uint16();
		for (int i = 0; i < NETWORK_STATION_TYPES; i++)
			company_info->num_station[i] = p->Recv_uint16();

		p->Recv_string(company_info->clients, sizeof(company_info->clients));

		InvalidateWindow(WC_NETWORK_WINDOW, 0);

		return NETWORK_RECV_STATUS_OKAY;
	}

	return NETWORK_RECV_STATUS_CLOSE_QUERY;
}

/* This packet contains info about the client (playas and name)
 *  as client we save this in NetworkClientInfo, linked via 'client_id'
 *  which is always an unique number on a server. */
DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_CLIENT_INFO)
{
	NetworkClientInfo *ci;
	ClientID client_id = (ClientID)p->Recv_uint32();
	CompanyID playas = (CompanyID)p->Recv_uint8();
	char name[NETWORK_NAME_LENGTH];

	p->Recv_string(name, sizeof(name));

	if (MY_CLIENT->has_quit) return NETWORK_RECV_STATUS_CONN_LOST;

	/* Do we receive a change of data? Most likely we changed playas */
	if (client_id == _network_own_client_id) _network_playas = playas;

	ci = NetworkFindClientInfoFromClientID(client_id);
	if (ci != NULL) {
		if (playas == ci->client_playas && strcmp(name, ci->client_name) != 0) {
			/* Client name changed, display the change */
			NetworkTextMessage(NETWORK_ACTION_NAME_CHANGE, CC_DEFAULT, false, ci->client_name, name);
		} else if (playas != ci->client_playas) {
			/* The client changed from client-player..
			 * Do not display that for now */
		}

		ci->client_playas = playas;
		strecpy(ci->client_name, name, lastof(ci->client_name));

		InvalidateWindow(WC_CLIENT_LIST, 0);

		return NETWORK_RECV_STATUS_OKAY;
	}

	/* We don't have this client_id yet, find an empty client_id, and put the data there */
	ci = new NetworkClientInfo(client_id);
	ci->client_playas = playas;
	if (client_id == _network_own_client_id) MY_CLIENT->SetInfo(ci);

	strecpy(ci->client_name, name, lastof(ci->client_name));

	InvalidateWindow(WC_CLIENT_LIST, 0);

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_ERROR)
{
	NetworkErrorCode error = (NetworkErrorCode)p->Recv_uint8();

	switch (error) {
		/* We made an error in the protocol, and our connection is closed.... */
		case NETWORK_ERROR_NOT_AUTHORIZED:
		case NETWORK_ERROR_NOT_EXPECTED:
		case NETWORK_ERROR_COMPANY_MISMATCH:
			_switch_mode_errorstr = STR_NETWORK_ERR_SERVER_ERROR;
			break;
		case NETWORK_ERROR_FULL:
			_switch_mode_errorstr = STR_NETWORK_ERR_SERVER_FULL;
			break;
		case NETWORK_ERROR_WRONG_REVISION:
			_switch_mode_errorstr = STR_NETWORK_ERR_WRONG_REVISION;
			break;
		case NETWORK_ERROR_WRONG_PASSWORD:
			_switch_mode_errorstr = STR_NETWORK_ERR_WRONG_PASSWORD;
			break;
		case NETWORK_ERROR_KICKED:
			_switch_mode_errorstr = STR_NETWORK_ERR_KICKED;
			break;
		case NETWORK_ERROR_CHEATER:
			_switch_mode_errorstr = STR_NETWORK_ERR_CHEATER;
			break;
		default:
			_switch_mode_errorstr = STR_NETWORK_ERR_LOSTCONNECTION;
	}

	DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);

	return NETWORK_RECV_STATUS_SERVER_ERROR;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_CHECK_NEWGRFS)
{
	uint grf_count = p->Recv_uint8();
	NetworkRecvStatus ret = NETWORK_RECV_STATUS_OKAY;

	/* Check all GRFs */
	for (; grf_count > 0; grf_count--) {
		GRFConfig c;
		MY_CLIENT->Recv_GRFIdentifier(p, &c);

		/* Check whether we know this GRF */
		const GRFConfig *f = FindGRFConfig(c.grfid, c.md5sum);
		if (f == NULL) {
			/* We do not know this GRF, bail out of initialization */
			char buf[sizeof(c.md5sum) * 2 + 1];
			md5sumToString(buf, lastof(buf), c.md5sum);
			DEBUG(grf, 0, "NewGRF %08X not found; checksum %s", BSWAP32(c.grfid), buf);
			ret = NETWORK_RECV_STATUS_NEWGRF_MISMATCH;
		}
	}

	if (ret == NETWORK_RECV_STATUS_OKAY) {
		/* Start receiving the map */
		SEND_COMMAND(PACKET_CLIENT_NEWGRFS_CHECKED)();
	} else {
		/* NewGRF mismatch, bail out */
		_switch_mode_errorstr = STR_NETWORK_ERR_NEWGRF_MISMATCH;
	}
	return ret;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_NEED_PASSWORD)
{
	NetworkPasswordType type = (NetworkPasswordType)p->Recv_uint8();

	switch (type) {
		case NETWORK_COMPANY_PASSWORD:
			/* Initialize the password hash salting variables. */
			_password_game_seed = p->Recv_uint32();
			p->Recv_string(_password_server_unique_id, sizeof(_password_server_unique_id));
			if (MY_CLIENT->has_quit) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

		case NETWORK_GAME_PASSWORD:
			ShowNetworkNeedPassword(type);
			return NETWORK_RECV_STATUS_OKAY;

		default: return NETWORK_RECV_STATUS_MALFORMED_PACKET;
	}
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_WELCOME)
{
	_network_own_client_id = (ClientID)p->Recv_uint32();

	/* Initialize the password hash salting variables, even if they were previously. */
	_password_game_seed = p->Recv_uint32();
	p->Recv_string(_password_server_unique_id, sizeof(_password_server_unique_id));

	/* Start receiving the map */
	SEND_COMMAND(PACKET_CLIENT_GETMAP)();
	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_WAIT)
{
	_network_join_status = NETWORK_JOIN_STATUS_WAITING;
	_network_join_waiting = p->Recv_uint8();
	InvalidateWindow(WC_NETWORK_STATUS_WINDOW, 0);

	/* We are put on hold for receiving the map.. we need GUI for this ;) */
	DEBUG(net, 1, "The server is currently busy sending the map to someone else, please wait..." );
	DEBUG(net, 1, "There are %d clients in front of you", _network_join_waiting);

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_MAP)
{
	static FILE *file_pointer;

	byte maptype;

	maptype = p->Recv_uint8();

	if (MY_CLIENT->has_quit) return NETWORK_RECV_STATUS_CONN_LOST;

	/* First packet, init some stuff */
	if (maptype == MAP_PACKET_START) {
		file_pointer = FioFOpenFile("network_client.tmp", "wb", AUTOSAVE_DIR);;
		if (file_pointer == NULL) {
			_switch_mode_errorstr = STR_NETWORK_ERR_SAVEGAMEERROR;
			return NETWORK_RECV_STATUS_SAVEGAME;
		}

		_frame_counter = _frame_counter_server = _frame_counter_max = p->Recv_uint32();

		_network_join_bytes = 0;
		_network_join_bytes_total = p->Recv_uint32();

		/* If the network connection has been closed due to loss of connection
		 * or when _network_join_kbytes_total is 0, the join status window will
		 * do a division by zero. When the connection is lost, we just return
		 * that. If kbytes_total is 0, the packet must be malformed as a
		 * savegame less than 1 kilobyte is practically impossible. */
		if (MY_CLIENT->has_quit) return NETWORK_RECV_STATUS_CONN_LOST;
		if (_network_join_bytes_total == 0) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

		_network_join_status = NETWORK_JOIN_STATUS_DOWNLOADING;
		InvalidateWindow(WC_NETWORK_STATUS_WINDOW, 0);

		/* The first packet does not contain any more data */
		return NETWORK_RECV_STATUS_OKAY;
	}

	if (maptype == MAP_PACKET_NORMAL) {
		/* We are still receiving data, put it to the file */
		if (fwrite(p->buffer + p->pos, 1, p->size - p->pos, file_pointer) != (size_t)(p->size - p->pos)) {
			_switch_mode_errorstr = STR_NETWORK_ERR_SAVEGAMEERROR;
			return NETWORK_RECV_STATUS_SAVEGAME;
		}

		_network_join_bytes = ftell(file_pointer);
		InvalidateWindow(WC_NETWORK_STATUS_WINDOW, 0);
	}

	/* Check if this was the last packet */
	if (maptype == MAP_PACKET_END) {
		fclose(file_pointer);

		_network_join_status = NETWORK_JOIN_STATUS_PROCESSING;
		InvalidateWindow(WC_NETWORK_STATUS_WINDOW, 0);

		/* The map is done downloading, load it */
		if (!SafeSaveOrLoad("network_client.tmp", SL_LOAD, GM_NORMAL, AUTOSAVE_DIR)) {
			DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
			_switch_mode_errorstr = STR_NETWORK_ERR_SAVEGAMEERROR;
			return NETWORK_RECV_STATUS_SAVEGAME;
		}
		/* If the savegame has successfully loaded, ALL windows have been removed,
		 * only toolbar/statusbar and gamefield are visible */

		/* Say we received the map and loaded it correctly! */
		SEND_COMMAND(PACKET_CLIENT_MAP_OK)();

		/* New company/spectator (invalid company) or company we want to join is not active
		 * Switch local company to spectator and await the server's judgement */
		if (_network_playas == COMPANY_NEW_COMPANY || !IsValidCompanyID(_network_playas)) {
			SetLocalCompany(COMPANY_SPECTATOR);

			if (_network_playas != COMPANY_SPECTATOR) {
				/* We have arrived and ready to start playing; send a command to make a new company;
				 * the server will give us a client-id and let us in */
				_network_join_status = NETWORK_JOIN_STATUS_REGISTERING;
				ShowJoinStatusWindow();
				NetworkSend_Command(0, 0, 0, CMD_COMPANY_CTRL, NULL, NULL);
			}
		} else {
			/* take control over an existing company */
			SetLocalCompany(_network_playas);
		}
	}

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_FRAME)
{
	_frame_counter_server = p->Recv_uint32();
	_frame_counter_max = p->Recv_uint32();
#ifdef ENABLE_NETWORK_SYNC_EVERY_FRAME
	/* Test if the server supports this option
	 *  and if we are at the frame the server is */
	if (p->pos < p->size) {
		_sync_frame = _frame_counter_server;
		_sync_seed_1 = p->Recv_uint32();
#ifdef NETWORK_SEND_DOUBLE_SEED
		_sync_seed_2 = p->Recv_uint32();
#endif
	}
#endif
	DEBUG(net, 5, "Received FRAME %d", _frame_counter_server);

	/* Let the server know that we received this frame correctly
	 *  We do this only once per day, to save some bandwidth ;) */
	if (!_network_first_time && last_ack_frame < _frame_counter) {
		last_ack_frame = _frame_counter + DAY_TICKS;
		DEBUG(net, 4, "Sent ACK at %d", _frame_counter);
		SEND_COMMAND(PACKET_CLIENT_ACK)();
	}

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_SYNC)
{
	_sync_frame = p->Recv_uint32();
	_sync_seed_1 = p->Recv_uint32();
#ifdef NETWORK_SEND_DOUBLE_SEED
	_sync_seed_2 = p->Recv_uint32();
#endif

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_COMMAND)
{
	CommandPacket cp;
	const char *err = MY_CLIENT->Recv_Command(p, &cp);
	cp.frame    = p->Recv_uint32();
	cp.my_cmd   = p->Recv_bool();
	cp.next     = NULL;

	if (err != NULL) {
		IConsolePrintF(CC_ERROR, "WARNING: %s from server, dropping...", err);
		return NETWORK_RECV_STATUS_MALFORMED_PACKET;
	}

	/* The server did send us this command..
	 *  queue it in our own queue, so we can handle it in the upcoming frame! */
	NetworkAddCommandQueue(cp);

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_CHAT)
{
	char name[NETWORK_NAME_LENGTH], msg[NETWORK_CHAT_LENGTH];
	const NetworkClientInfo *ci = NULL, *ci_to;

	NetworkAction action = (NetworkAction)p->Recv_uint8();
	ClientID client_id = (ClientID)p->Recv_uint32();
	bool self_send = p->Recv_bool();
	p->Recv_string(msg, NETWORK_CHAT_LENGTH);
	int64 data = p->Recv_uint64();

	ci_to = NetworkFindClientInfoFromClientID(client_id);
	if (ci_to == NULL) return NETWORK_RECV_STATUS_OKAY;

	/* Did we initiate the action locally? */
	if (self_send) {
		switch (action) {
			case NETWORK_ACTION_CHAT_CLIENT:
				/* For speaking to client we need the client-name */
				snprintf(name, sizeof(name), "%s", ci_to->client_name);
				ci = NetworkFindClientInfoFromClientID(_network_own_client_id);
				break;

			/* For speaking to company or giving money, we need the company-name */
			case NETWORK_ACTION_GIVE_MONEY:
				if (!IsValidCompanyID(ci_to->client_playas)) return NETWORK_RECV_STATUS_OKAY;
				/* fallthrough */
			case NETWORK_ACTION_CHAT_COMPANY: {
				StringID str = IsValidCompanyID(ci_to->client_playas) ? STR_COMPANY_NAME : STR_NETWORK_SPECTATORS;
				SetDParam(0, ci_to->client_playas);

				GetString(name, str, lastof(name));
				ci = NetworkFindClientInfoFromClientID(_network_own_client_id);
			} break;

			default: return NETWORK_RECV_STATUS_MALFORMED_PACKET;
		}
	} else {
		/* Display message from somebody else */
		snprintf(name, sizeof(name), "%s", ci_to->client_name);
		ci = ci_to;
	}

	if (ci != NULL)
		NetworkTextMessage(action, (ConsoleColour)GetDrawStringCompanyColour(ci->client_playas), self_send, name, msg, data);
	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_ERROR_QUIT)
{
	ClientID client_id = (ClientID)p->Recv_uint32();

	NetworkClientInfo *ci = NetworkFindClientInfoFromClientID(client_id);
	if (ci != NULL) {
		NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, ci->client_name, NULL, GetNetworkErrorMsg((NetworkErrorCode)p->Recv_uint8()));
		delete ci;
	}

	InvalidateWindow(WC_CLIENT_LIST, 0);

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_QUIT)
{
	NetworkClientInfo *ci;

	ClientID client_id = (ClientID)p->Recv_uint32();

	ci = NetworkFindClientInfoFromClientID(client_id);
	if (ci != NULL) {
		NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, ci->client_name, NULL, STR_NETWORK_CLIENT_LEAVING);
		delete ci;
	} else {
		DEBUG(net, 0, "Unknown client (%d) is leaving the game", client_id);
	}

	InvalidateWindow(WC_CLIENT_LIST, 0);

	/* If we come here it means we could not locate the client.. strange :s */
	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_JOIN)
{
	ClientID client_id = (ClientID)p->Recv_uint32();

	NetworkClientInfo *ci = NetworkFindClientInfoFromClientID(client_id);
	if (ci != NULL)
		NetworkTextMessage(NETWORK_ACTION_JOIN, CC_DEFAULT, false, ci->client_name);

	InvalidateWindow(WC_CLIENT_LIST, 0);

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_SHUTDOWN)
{
	_switch_mode_errorstr = STR_NETWORK_SERVER_SHUTDOWN;

	return NETWORK_RECV_STATUS_SERVER_ERROR;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_NEWGAME)
{
	/* To trottle the reconnects a bit, every clients waits
	 *  his _local_company value before reconnecting
	 * COMPANY_SPECTATOR is currently 255, so to avoid long wait periods
	 *  set the max to 10. */
	_network_reconnect = min(_local_company + 1, 10);
	_switch_mode_errorstr = STR_NETWORK_SERVER_REBOOT;

	return NETWORK_RECV_STATUS_SERVER_ERROR;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_RCON)
{
	char rcon_out[NETWORK_RCONCOMMAND_LENGTH];

	ConsoleColour colour_code = (ConsoleColour)p->Recv_uint16();
	p->Recv_string(rcon_out, sizeof(rcon_out));

	IConsolePrint(colour_code, rcon_out);

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_MOVE)
{
	/* Nothing more in this packet... */
	ClientID client_id   = (ClientID)p->Recv_uint32();
	CompanyID company_id = (CompanyID)p->Recv_uint8();

	if (client_id == 0) {
		/* definitely an invalid client id, debug message and do nothing. */
		DEBUG(net, 0, "[move] received invalid client index = 0");
		return NETWORK_RECV_STATUS_MALFORMED_PACKET;
	}

	const NetworkClientInfo *ci = NetworkFindClientInfoFromClientID(client_id);
	/* Just make sure we do not try to use a client_index that does not exist */
	if (ci == NULL) return NETWORK_RECV_STATUS_OKAY;

	/* if not valid player, force spectator, else check player exists */
	if (!IsValidCompanyID(company_id)) company_id = COMPANY_SPECTATOR;

	if (client_id == _network_own_client_id) {
		_network_playas = company_id;
		SetLocalCompany(company_id);
	}

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_CONFIG_UPDATE)
{
	_network_server_max_companies = p->Recv_uint8();
	_network_server_max_spectators = p->Recv_uint8();

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_COMPANY_UPDATE)
{
	_network_company_passworded = p->Recv_uint16();
	InvalidateWindowClasses(WC_COMPANY);

	return NETWORK_RECV_STATUS_OKAY;
}


/* The layout for the receive-functions by the client */
typedef NetworkRecvStatus NetworkClientPacket(Packet *p);

/* This array matches PacketType. At an incoming
 *  packet it is matches against this array
 *  and that way the right function to handle that
 *  packet is found. */
static NetworkClientPacket * const _network_client_packet[] = {
	RECEIVE_COMMAND(PACKET_SERVER_FULL),
	RECEIVE_COMMAND(PACKET_SERVER_BANNED),
	NULL, // PACKET_CLIENT_JOIN,
	RECEIVE_COMMAND(PACKET_SERVER_ERROR),
	NULL, // PACKET_CLIENT_COMPANY_INFO,
	RECEIVE_COMMAND(PACKET_SERVER_COMPANY_INFO),
	RECEIVE_COMMAND(PACKET_SERVER_CLIENT_INFO),
	RECEIVE_COMMAND(PACKET_SERVER_NEED_PASSWORD),
	NULL, // PACKET_CLIENT_PASSWORD,
	RECEIVE_COMMAND(PACKET_SERVER_WELCOME),
	NULL, // PACKET_CLIENT_GETMAP,
	RECEIVE_COMMAND(PACKET_SERVER_WAIT),
	RECEIVE_COMMAND(PACKET_SERVER_MAP),
	NULL, // PACKET_CLIENT_MAP_OK,
	RECEIVE_COMMAND(PACKET_SERVER_JOIN),
	RECEIVE_COMMAND(PACKET_SERVER_FRAME),
	RECEIVE_COMMAND(PACKET_SERVER_SYNC),
	NULL, // PACKET_CLIENT_ACK,
	NULL, // PACKET_CLIENT_COMMAND,
	RECEIVE_COMMAND(PACKET_SERVER_COMMAND),
	NULL, // PACKET_CLIENT_CHAT,
	RECEIVE_COMMAND(PACKET_SERVER_CHAT),
	NULL, // PACKET_CLIENT_SET_PASSWORD,
	NULL, // PACKET_CLIENT_SET_NAME,
	NULL, // PACKET_CLIENT_QUIT,
	NULL, // PACKET_CLIENT_ERROR,
	RECEIVE_COMMAND(PACKET_SERVER_QUIT),
	RECEIVE_COMMAND(PACKET_SERVER_ERROR_QUIT),
	RECEIVE_COMMAND(PACKET_SERVER_SHUTDOWN),
	RECEIVE_COMMAND(PACKET_SERVER_NEWGAME),
	RECEIVE_COMMAND(PACKET_SERVER_RCON),
	NULL, // PACKET_CLIENT_RCON,
	RECEIVE_COMMAND(PACKET_SERVER_CHECK_NEWGRFS),
	NULL, // PACKET_CLIENT_NEWGRFS_CHECKED,
	RECEIVE_COMMAND(PACKET_SERVER_MOVE),
	NULL, // PACKET_CLIENT_MOVE
	RECEIVE_COMMAND(PACKET_SERVER_COMPANY_UPDATE),
	RECEIVE_COMMAND(PACKET_SERVER_CONFIG_UPDATE),
};

/* If this fails, check the array above with network_data.h */
assert_compile(lengthof(_network_client_packet) == PACKET_END);

/* Is called after a client is connected to the server */
void NetworkClient_Connected()
{
	/* Set the frame-counter to 0 so nothing happens till we are ready */
	_frame_counter = 0;
	_frame_counter_server = 0;
	last_ack_frame = 0;
	/* Request the game-info */
	SEND_COMMAND(PACKET_CLIENT_JOIN)();
}

/* Reads the packets from the socket-stream, if available */
NetworkRecvStatus NetworkClient_ReadPackets(NetworkClientSocket *cs)
{
	Packet *p;
	NetworkRecvStatus res = NETWORK_RECV_STATUS_OKAY;

	while (res == NETWORK_RECV_STATUS_OKAY && (p = cs->Recv_Packet(&res)) != NULL) {
		byte type = p->Recv_uint8();
		if (type < PACKET_END && _network_client_packet[type] != NULL && !MY_CLIENT->has_quit) {
			res = _network_client_packet[type](p);
		} else {
			res = NETWORK_RECV_STATUS_MALFORMED_PACKET;
			DEBUG(net, 0, "[client] received invalid packet type %d", type);
		}

		delete p;
	}

	return res;
}

void NetworkClientSendRcon(const char *password, const char *command)
{
	SEND_COMMAND(PACKET_CLIENT_RCON)(password, command);
}

/**
 * Notify the server of this client wanting to be moved to another company.
 * @param company_id id of the company the client wishes to be moved to.
 * @param pass the password, is only checked on the server end if a password is needed.
 * @return void
 */
void NetworkClientRequestMove(CompanyID company_id, const char *pass)
{
	SEND_COMMAND(PACKET_CLIENT_MOVE)(company_id, pass);
}

void NetworkUpdateClientName()
{
	NetworkClientInfo *ci = NetworkFindClientInfoFromClientID(_network_own_client_id);

	if (ci == NULL) return;

	/* Don't change the name if it is the same as the old name */
	if (strcmp(ci->client_name, _settings_client.network.client_name) != 0) {
		if (!_network_server) {
			SEND_COMMAND(PACKET_CLIENT_SET_NAME)(_settings_client.network.client_name);
		} else {
			if (NetworkFindName(_settings_client.network.client_name)) {
				NetworkTextMessage(NETWORK_ACTION_NAME_CHANGE, CC_DEFAULT, false, ci->client_name, _settings_client.network.client_name);
				strecpy(ci->client_name, _settings_client.network.client_name, lastof(ci->client_name));
				NetworkUpdateClientInfo(CLIENT_ID_SERVER);
			}
		}
	}
}

void NetworkClientSendChat(NetworkAction action, DestType type, int dest, const char *msg, int64 data)
{
	SEND_COMMAND(PACKET_CLIENT_CHAT)(action, type, dest, msg, data);
}

void NetworkClientSetPassword(const char *password)
{
	SEND_COMMAND(PACKET_CLIENT_SET_PASSWORD)(password);
}

/**
 * Tell whether the client has team members where he/she can chat to.
 * @param cio client to check members of.
 * @return true if there is at least one team member.
 */
bool NetworkClientPreferTeamChat(const NetworkClientInfo *cio)
{
	/* Only companies actually playing can speak to team. Eg spectators cannot */
	if (!_settings_client.gui.prefer_teamchat || !IsValidCompanyID(cio->client_playas)) return false;

	const NetworkClientInfo *ci;
	FOR_ALL_CLIENT_INFOS(ci) {
		if (ci->client_playas == cio->client_playas && ci != cio) return true;
	}

	return false;
}

/**
 * Check if max_companies has been reached on the server (local check only).
 * @return true if the max value has been reached or exceeded, false otherwise.
 */
bool NetworkMaxCompaniesReached()
{
	return ActiveCompanyCount() >= (_network_server ? _settings_client.network.max_companies : _network_server_max_companies);
}

/**
 * Check if max_spectatos has been reached on the server (local check only).
 * @return true if the max value has been reached or exceeded, false otherwise.
 */
bool NetworkMaxSpectatorsReached()
{
	return NetworkSpectatorCount() >= (_network_server ? _settings_client.network.max_spectators : _network_server_max_spectators);
}

/**
 * Print all the clients to the console
 */
void NetworkPrintClients()
{
	const NetworkClientInfo *ci;
	FOR_ALL_CLIENT_INFOS(ci) {
		IConsolePrintF(CC_INFO, "Client #%1d  name: '%s'  company: %1d  IP: %s",
				ci->client_id,
				ci->client_name,
				ci->client_playas + (IsValidCompanyID(ci->client_playas) ? 1 : 0),
				GetClientIP(ci));
	}
}

#endif /* ENABLE_NETWORK */
