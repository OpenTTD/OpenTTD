#include "stdafx.h"
#include "network_data.h"

#ifdef ENABLE_NETWORK

#include "table/strings.h"
#include "network_client.h"
#include "network_gamelist.h"
#include "command.h"
#include "gfx.h"
#include "window.h"
#include "settings.h"


// This file handles all the client-commands


extern const char _openttd_revision[];

// So we don't make too much typos ;)
#define MY_CLIENT DEREF_CLIENT(0)

static uint32 last_ack_frame;

void NetworkRecvPatchSettings(Packet *p);

// **********
// Sending functions
//   DEF_CLIENT_SEND_COMMAND has no parameters
// **********

DEF_CLIENT_SEND_COMMAND(PACKET_CLIENT_COMPANY_INFO)
{
	//
	// Packet: CLIENT_COMPANY_INFO
	// Function: Request company-info (in detail)
	// Data:
	//    <none>
	//
	Packet *p;
	_network_join_status = NETWORK_JOIN_STATUS_GETTING_COMPANY_INFO;
	InvalidateWindow(WC_NETWORK_STATUS_WINDOW, 0);

	p = NetworkSend_Init(PACKET_CLIENT_COMPANY_INFO);
	NetworkSend_Packet(p, MY_CLIENT);
}

DEF_CLIENT_SEND_COMMAND(PACKET_CLIENT_JOIN)
{
	//
	// Packet: CLIENT_JOIN
	// Function: Try to join the server
	// Data:
	//    String: OpenTTD Revision (norev000 if no revision)
	//    String: Player Name (max NETWORK_NAME_LENGTH)
	//    uint8:  Play as Player id (1..MAX_PLAYERS)
	//    uint8:  Language ID
	//    String: Unique id to find the player back in server-listing
	//

	Packet *p;
	_network_join_status = NETWORK_JOIN_STATUS_AUTHORIZING;
	InvalidateWindow(WC_NETWORK_STATUS_WINDOW, 0);

	p = NetworkSend_Init(PACKET_CLIENT_JOIN);
	NetworkSend_string(p, _openttd_revision);
	NetworkSend_string(p, _network_player_name); // Player name
	NetworkSend_uint8(p, _network_playas); // PlayAs
	NetworkSend_uint8(p, NETLANG_ANY); // Language
	NetworkSend_string(p, _network_unique_id);
	NetworkSend_Packet(p, MY_CLIENT);
}

DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_PASSWORD)(NetworkPasswordType type, const char *password)
{
	//
	// Packet: CLIENT_PASSWORD
	// Function: Send a password to the server to authorize
	// Data:
	//    uint8:  NetworkPasswordType
	//    String: Password
	//
	Packet *p = NetworkSend_Init(PACKET_CLIENT_PASSWORD);
	NetworkSend_uint8(p, type);
	NetworkSend_string(p, password);
	NetworkSend_Packet(p, MY_CLIENT);
}

DEF_CLIENT_SEND_COMMAND(PACKET_CLIENT_GETMAP)
{
	//
	// Packet: CLIENT_GETMAP
	// Function: Request the map from the server
	// Data:
	//    <none>
	//

	Packet *p = NetworkSend_Init(PACKET_CLIENT_GETMAP);
	NetworkSend_Packet(p, MY_CLIENT);
}

DEF_CLIENT_SEND_COMMAND(PACKET_CLIENT_MAP_OK)
{
	//
	// Packet: CLIENT_MAP_OK
	// Function: Tell the server that we are done receiving/loading the map
	// Data:
	//    <none>
	//

	Packet *p = NetworkSend_Init(PACKET_CLIENT_MAP_OK);
	NetworkSend_Packet(p, MY_CLIENT);
}

DEF_CLIENT_SEND_COMMAND(PACKET_CLIENT_ACK)
{
	//
	// Packet: CLIENT_ACK
	// Function: Tell the server we are done with this frame
	// Data:
	//    uint32: current FrameCounter of the client
	//

	Packet *p = NetworkSend_Init(PACKET_CLIENT_ACK);

	NetworkSend_uint32(p, _frame_counter);
	NetworkSend_Packet(p, MY_CLIENT);
}

// Send a command packet to the server
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_COMMAND)(CommandPacket *cp)
{
	//
	// Packet: CLIENT_COMMAND
	// Function: Send a DoCommand to the Server
	// Data:
	//    uint8:  PlayerID (0..MAX_PLAYERS-1)
	//    uint32: CommandID (see command.h)
	//    uint32: P1 (free variables used in DoCommand)
	//    uint32: P2
	//    uint32: Tile
	//    uint32: decode_params
	//      10 times the last one (lengthof(cp->dp))
	//    uint8:  CallBackID (see callback_table.c)
	//

	int i;
	char *dparam_char;
	Packet *p = NetworkSend_Init(PACKET_CLIENT_COMMAND);

	NetworkSend_uint8(p, cp->player);
	NetworkSend_uint32(p, cp->cmd);
	NetworkSend_uint32(p, cp->p1);
	NetworkSend_uint32(p, cp->p2);
	NetworkSend_uint32(p, (uint32)cp->tile);
	/* We are going to send them byte by byte, because dparam is misused
	    for chars (if it is used), and else we have a BigEndian / LittleEndian
	    problem.. we should fix the misuse of dparam... -- TrueLight */
	dparam_char = (char *)&cp->dp[0];
	for (i = 0; i < lengthof(cp->dp) * 4; i++) {
		NetworkSend_uint8(p, *dparam_char);
		dparam_char++;
	}
	NetworkSend_uint8(p, cp->callback);

	NetworkSend_Packet(p, MY_CLIENT);
}

// Send a chat-packet over the network
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_CHAT)(NetworkAction action, DestType desttype, int dest, const char *msg)
{
	//
	// Packet: CLIENT_CHAT
	// Function: Send a chat-packet to the serve
	// Data:
	//    uint8:  ActionID (see network_data.h, NetworkAction)
	//    uint8:  Destination Type (see network_data.h, DestType);
	//    uint8:  Destination Player (1..MAX_PLAYERS)
	//    String: Message (max MAX_TEXT_MSG_LEN)
	//

	Packet *p = NetworkSend_Init(PACKET_CLIENT_CHAT);

	NetworkSend_uint8(p, action);
	NetworkSend_uint8(p, desttype);
	NetworkSend_uint8(p, dest);
	NetworkSend_string(p, msg);
	NetworkSend_Packet(p, MY_CLIENT);
}

// Send an error-packet over the network
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_ERROR)(NetworkErrorCode errorno)
{
	//
	// Packet: CLIENT_ERROR
	// Function: The client made an error and is quiting the game
	// Data:
	//    uint8:  ErrorID (see network_data.h, NetworkErrorCode)
	//
	Packet *p = NetworkSend_Init(PACKET_CLIENT_ERROR);

	NetworkSend_uint8(p, errorno);
	NetworkSend_Packet(p, MY_CLIENT);
}

DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_SET_PASSWORD)(const char *password)
{
	//
	// Packet: PACKET_CLIENT_SET_PASSWORD
	// Function: Set the password for the clients current company
	// Data:
	//    String: Password
	//
	Packet *p = NetworkSend_Init(PACKET_CLIENT_SET_PASSWORD);

	NetworkSend_string(p, password);
	NetworkSend_Packet(p, MY_CLIENT);
}

DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_SET_NAME)(const char *name)
{
	//
	// Packet: PACKET_CLIENT_SET_NAME
	// Function: Gives the player a new name
	// Data:
	//    String: Name
	//
	Packet *p = NetworkSend_Init(PACKET_CLIENT_SET_NAME);

	NetworkSend_string(p, name);
	NetworkSend_Packet(p, MY_CLIENT);
}

// Send an quit-packet over the network
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_QUIT)(const char *leavemsg)
{
	//
	// Packet: CLIENT_QUIT
	// Function: The client is quiting the game
	// Data:
	//    String: leave-message
	//
	Packet *p = NetworkSend_Init(PACKET_CLIENT_QUIT);

	NetworkSend_string(p, leavemsg);
	NetworkSend_Packet(p, MY_CLIENT);
}


// **********
// Receiving functions
//   DEF_CLIENT_RECEIVE_COMMAND has parameter: Packet *p
// **********

extern bool SafeSaveOrLoad(const char *filename, int mode, int newgm);

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_FULL)
{
	// We try to join a server which is full
	_switch_mode_errorstr = STR_NETWORK_ERR_SERVER_FULL;
	DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);

	return NETWORK_RECV_STATUS_SERVER_FULL;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_COMPANY_INFO)
{
	byte company_info_version;
	int i;

	company_info_version = NetworkRecv_uint8(p);

	if (company_info_version == 1) {
		byte total;
		byte current;

		total = NetworkRecv_uint8(p);

		// There is no data at all..
		if (total == 0)
			return NETWORK_RECV_STATUS_CLOSE_QUERY;

		current = NetworkRecv_uint8(p);
		if (current >= MAX_PLAYERS)
			return NETWORK_RECV_STATUS_CLOSE_QUERY;

		_network_lobby_company_count++;

		NetworkRecv_string(p, _network_player_info[current].company_name, sizeof(_network_player_info[current].company_name));
		_network_player_info[current].inaugurated_year = NetworkRecv_uint8(p);
		_network_player_info[current].company_value = NetworkRecv_uint64(p);
		_network_player_info[current].money = NetworkRecv_uint64(p);
		_network_player_info[current].income = NetworkRecv_uint64(p);
		_network_player_info[current].performance = NetworkRecv_uint16(p);
		for (i = 0; i < NETWORK_VEHICLE_TYPES; i++)
			_network_player_info[current].num_vehicle[i] = NetworkRecv_uint16(p);
		for (i = 0; i < NETWORK_STATION_TYPES; i++)
			_network_player_info[current].num_station[i] = NetworkRecv_uint16(p);

		NetworkRecv_string(p, _network_player_info[current].players, sizeof(_network_player_info[current].players));

		InvalidateWindow(WC_NETWORK_WINDOW, 0);

		return NETWORK_RECV_STATUS_OKAY;
	}

	return NETWORK_RECV_STATUS_CLOSE_QUERY;
}

// This packet contains info about the client (playas and name)
//  as client we save this in NetworkClientInfo, linked via 'index'
//  which is always an unique number on a server.
DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_CLIENT_INFO)
{
	NetworkClientInfo *ci;
	uint16 index = NetworkRecv_uint16(p);
	byte playas = NetworkRecv_uint8(p);
	char name[NETWORK_NAME_LENGTH];
	char unique_id[NETWORK_NAME_LENGTH];

	NetworkRecv_string(p, name, sizeof(name));
	NetworkRecv_string(p, unique_id, sizeof(unique_id));

	/* Do we receive a change of data? Most likely we changed playas */
	if (index == _network_own_client_index)
		_network_playas = playas;

	ci = NetworkFindClientInfoFromIndex(index);
	if (ci != NULL) {
		if (playas == ci->client_playas && strcmp(name, ci->client_name) != 0) {
			// Client name changed, display the change
			NetworkTextMessage(NETWORK_ACTION_NAME_CHANGE, 1, false, ci->client_name, name);
		} else if (playas != ci->client_playas) {
			// The player changed from client-player..
			// Do not display that for now
		}

		ci->client_playas = playas;
		ttd_strlcpy(ci->client_name, name, sizeof(ci->client_name));

		InvalidateWindow(WC_CLIENT_LIST, 0);

		return NETWORK_RECV_STATUS_OKAY;
	}

	// We don't have this index yet, find an empty index, and put the data there
	ci = NetworkFindClientInfoFromIndex(NETWORK_EMPTY_INDEX);
	if (ci != NULL) {
		ci->client_index = index;
		ci->client_playas = playas;

		ttd_strlcpy(ci->client_name, name, sizeof(ci->client_name));
		ttd_strlcpy(ci->unique_id, unique_id, sizeof(ci->unique_id));

		InvalidateWindow(WC_CLIENT_LIST, 0);

		return NETWORK_RECV_STATUS_OKAY;
	}

	// Here the program should never ever come.....
	return NETWORK_RECV_STATUS_MALFORMED_PACKET;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_ERROR)
{
	NetworkErrorCode error = NetworkRecv_uint8(p);

	if (error == NETWORK_ERROR_NOT_AUTHORIZED || error == NETWORK_ERROR_NOT_EXPECTED ||
			error == NETWORK_ERROR_PLAYER_MISMATCH) {
		// We made an error in the protocol, and our connection is closed.... :(
		_switch_mode_errorstr = STR_NETWORK_ERR_SERVER_ERROR;
	} else if (error == NETWORK_ERROR_WRONG_REVISION) {
		// Wrong revision :(
		_switch_mode_errorstr = STR_NETWORK_ERR_WRONG_REVISION;
	} else if (error == NETWORK_ERROR_WRONG_PASSWORD) {
		// Wrong password
		_switch_mode_errorstr = STR_NETWORK_ERR_WRONG_PASSWORD;
	} else if (error == NETWORK_ERROR_KICKED) {
		_switch_mode_errorstr = STR_NETWORK_ERR_KICKED;
	} else if (error == NETWORK_ERROR_CHEATER) {
		_switch_mode_errorstr = STR_NETWORK_ERR_CHEATER;
	}

	DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);

	return NETWORK_RECV_STATUS_SERVER_ERROR;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_NEED_PASSWORD)
{
	NetworkPasswordType type;
	type = NetworkRecv_uint8(p);

	if (type == NETWORK_GAME_PASSWORD) {
		ShowNetworkNeedGamePassword();
		return NETWORK_RECV_STATUS_OKAY;
	} else if (type == NETWORK_COMPANY_PASSWORD) {
		ShowNetworkNeedCompanyPassword();
		return NETWORK_RECV_STATUS_OKAY;
	}

	return NETWORK_RECV_STATUS_MALFORMED_PACKET;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_WELCOME)
{
	_network_own_client_index = NetworkRecv_uint16(p);

	// Start receiving the map
	SEND_COMMAND(PACKET_CLIENT_GETMAP)();
	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_WAIT)
{
	_network_join_status = NETWORK_JOIN_STATUS_WAITING;
	_network_join_waiting = NetworkRecv_uint8(p);
	InvalidateWindow(WC_NETWORK_STATUS_WINDOW, 0);

	// We are put on hold for receiving the map.. we need GUI for this ;)
	DEBUG(net, 1)("[NET] The server is currently busy sending the map to someone else.. please hold..." );
	DEBUG(net, 1)("[NET]  There are %d clients in front of you", _network_join_waiting);

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_MAP)
{
	static char filename[256];
	static FILE *file_pointer;

	byte maptype;

	maptype = NetworkRecv_uint8(p);

	// First packet, init some stuff
	if (maptype == MAP_PACKET_START) {
		// The name for the temp-map
		sprintf(filename, "%s%snetwork_client.tmp",  _path.autosave_dir, PATHSEP);

		file_pointer = fopen(filename, "wb");
		if (file_pointer == NULL) {
			_switch_mode_errorstr = STR_NETWORK_ERR_SAVEGAMEERROR;
			return NETWORK_RECV_STATUS_SAVEGAME;
		}

		_frame_counter = _frame_counter_server = _frame_counter_max = NetworkRecv_uint32(p);

		_network_join_status = NETWORK_JOIN_STATUS_DOWNLOADING;
		_network_join_kbytes = 0;
		_network_join_kbytes_total = NetworkRecv_uint32(p) / 1024;
		InvalidateWindow(WC_NETWORK_STATUS_WINDOW, 0);

		// The first packet does not contain any more data
		return NETWORK_RECV_STATUS_OKAY;
	}

	if (maptype == MAP_PACKET_NORMAL) {
		// We are still receiving data, put it to the file
		fwrite(p->buffer + p->pos, 1, p->size - p->pos, file_pointer);

		_network_join_kbytes = ftell(file_pointer) / 1024;
		InvalidateWindow(WC_NETWORK_STATUS_WINDOW, 0);
	}

	if (maptype == MAP_PACKET_PATCH) {
		NetworkRecvPatchSettings(p);
	}

	// Check if this was the last packet
	if (maptype == MAP_PACKET_END) {
		// We also get, very nice, the player_seeds in this packet
		int i;
		for (i = 0; i < MAX_PLAYERS; i++) {
			_player_seeds[i][0] = NetworkRecv_uint32(p);
			_player_seeds[i][1] = NetworkRecv_uint32(p);
		}

		fclose(file_pointer);

		_network_join_status = NETWORK_JOIN_STATUS_PROCESSING;
		InvalidateWindow(WC_NETWORK_STATUS_WINDOW, 0);

		// The map is done downloading, load it
		// Load the map
		if (!SafeSaveOrLoad(filename, SL_LOAD, GM_NORMAL)) {
			DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
			_switch_mode_errorstr = STR_NETWORK_ERR_SAVEGAMEERROR;
			return NETWORK_RECV_STATUS_SAVEGAME;
		}
		_opt_mod_ptr = &_opt;

		// Say we received the map and loaded it correctly!
		SEND_COMMAND(PACKET_CLIENT_MAP_OK)();

		if (_network_playas == 0 || _network_playas > MAX_PLAYERS ||
				!DEREF_PLAYER(_network_playas - 1)->is_active) {

			if (_network_playas == OWNER_SPECTATOR) {
				// The client wants to be a spectator..
				_local_player = OWNER_SPECTATOR;
				DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
			} else {
				// send a command to make a new player
				_local_player = 0;
				NetworkSend_Command(0, 0, 0, CMD_PLAYER_CTRL, NULL);
				_local_player = OWNER_SPECTATOR;
			}
		} else {
			// take control over an existing company
			_local_player = _network_playas - 1;
			DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
		}
	}

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_FRAME)
{
	_frame_counter_server = NetworkRecv_uint32(p);
	_frame_counter_max = NetworkRecv_uint32(p);
#ifdef ENABLE_NETWORK_SYNC_EVERY_FRAME
	// Test if the server supports this option
	//  and if we are at the frame the server is
	if (p->pos < p->size) {
		_sync_frame = _frame_counter_server;
		_sync_seed_1 = NetworkRecv_uint32(p);
#ifdef NETWORK_SEND_DOUBLE_SEED
		_sync_seed_2 = NetworkRecv_uint32(p);
#endif
	}
#endif
	DEBUG(net, 7)("[NET] Received FRAME %d",_frame_counter_server);

	// Let the server know that we received this frame correctly
	//  We do this only once per day, to save some bandwidth ;)
	if (!_network_first_time && last_ack_frame < _frame_counter) {
		last_ack_frame = _frame_counter + DAY_TICKS;
		DEBUG(net,6)("[NET] Sent ACK at %d", _frame_counter);
		SEND_COMMAND(PACKET_CLIENT_ACK)();
	}

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_SYNC)
{
	_sync_frame = NetworkRecv_uint32(p);
	_sync_seed_1 = NetworkRecv_uint32(p);
#ifdef NETWORK_SEND_DOUBLE_SEED
	_sync_seed_2 = NetworkRecv_uint32(p);
#endif

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_COMMAND)
{
	int i;
	char *dparam_char;
	CommandPacket *cp = malloc(sizeof(CommandPacket));
	cp->player = NetworkRecv_uint8(p);
	cp->cmd = NetworkRecv_uint32(p);
	cp->p1 = NetworkRecv_uint32(p);
	cp->p2 = NetworkRecv_uint32(p);
	cp->tile = NetworkRecv_uint32(p);
	/* We are going to send them byte by byte, because dparam is misused
	    for chars (if it is used), and else we have a BigEndian / LittleEndian
	    problem.. we should fix the misuse of dparam... -- TrueLight */
	dparam_char = (char *)&cp->dp[0];
	for (i = 0; i < lengthof(cp->dp) * 4; i++) {
		*dparam_char = NetworkRecv_uint8(p);
		dparam_char++;
	}
	cp->callback = NetworkRecv_uint8(p);
	cp->frame = NetworkRecv_uint32(p);
	cp->next = NULL;

	// The server did send us this command..
	//  queue it in our own queue, so we can handle it in the upcoming frame!

	if (_local_command_queue == NULL) {
		_local_command_queue = cp;
	} else {
		// Find last packet
		CommandPacket *c = _local_command_queue;
		while (c->next != NULL) c = c->next;
		c->next = cp;
	}

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_CHAT)
{
	NetworkAction action = NetworkRecv_uint8(p);
	char msg[MAX_TEXT_MSG_LEN];
	NetworkClientInfo *ci = NULL, *ci_to;
	uint16 index;
	char name[NETWORK_NAME_LENGTH];
	bool self_send;

	index = NetworkRecv_uint16(p);
	self_send = NetworkRecv_uint8(p);
	NetworkRecv_string(p, msg, MAX_TEXT_MSG_LEN);

	ci_to = NetworkFindClientInfoFromIndex(index);
	if (ci_to == NULL) return NETWORK_RECV_STATUS_OKAY;

	/* Do we display the action locally? */
	if (self_send) {
		switch (action) {
			case NETWORK_ACTION_CHAT_CLIENT:
				/* For speak to client we need the client-name */
				snprintf(name, sizeof(name), "%s", ci_to->client_name);
				ci = NetworkFindClientInfoFromIndex(_network_own_client_index);
				break;
			case NETWORK_ACTION_CHAT_PLAYER:
			case NETWORK_ACTION_GIVE_MONEY:
				/* For speak to player or give money, we need the player-name */
				if (ci_to->client_playas > MAX_PLAYERS)
					return NETWORK_RECV_STATUS_OKAY; // This should never happen
				GetString(name, DEREF_PLAYER(ci_to->client_playas-1)->name_1);
				ci = NetworkFindClientInfoFromIndex(_network_own_client_index);
				break;
			default:
				/* This should never happen */
				NOT_REACHED();
				break;
		}
	} else {
		/* Display message from somebody else */
		snprintf(name, sizeof(name), "%s", ci_to->client_name);
		ci = ci_to;
	}

	if (ci != NULL)
		NetworkTextMessage(action, GetDrawStringPlayerColor(ci->client_playas-1), self_send, name, "%s", msg);
	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_ERROR_QUIT)
{
	int errorno;
	char str[100];
	uint16 index;
	NetworkClientInfo *ci;

	index = NetworkRecv_uint16(p);
	errorno = NetworkRecv_uint8(p);

	GetString(str, STR_NETWORK_ERR_CLIENT_GENERAL + errorno);

	ci = NetworkFindClientInfoFromIndex(index);
	if (ci != NULL) {
		NetworkTextMessage(NETWORK_ACTION_LEAVE, 1, false, ci->client_name, str);

		// The client is gone, give the NetworkClientInfo free
		ci->client_index = NETWORK_EMPTY_INDEX;
	}

	InvalidateWindow(WC_CLIENT_LIST, 0);

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_QUIT)
{
	char str[100];
	uint16 index;
	NetworkClientInfo *ci;

	index = NetworkRecv_uint16(p);
	NetworkRecv_string(p, str, 100);

	ci = NetworkFindClientInfoFromIndex(index);
	if (ci != NULL) {
		NetworkTextMessage(NETWORK_ACTION_LEAVE, 1, false, ci->client_name, str);

		// The client is gone, give the NetworkClientInfo free
		ci->client_index = NETWORK_EMPTY_INDEX;
	} else {
		DEBUG(net, 0)("[NET] Error - unknown client (%d) is leaving the game", index);
	}

	InvalidateWindow(WC_CLIENT_LIST, 0);

	// If we come here it means we could not locate the client.. strange :s
	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_JOIN)
{
	uint16 index;
	NetworkClientInfo *ci;

	index = NetworkRecv_uint16(p);

	ci = NetworkFindClientInfoFromIndex(index);
	if (ci != NULL)
		NetworkTextMessage(NETWORK_ACTION_JOIN, 1, false, ci->client_name, "");

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
	// To trottle the reconnects a bit, every clients waits
	//  his _local_player value before reconnecting
	// OWNER_SPECTATOR is currently 255, so to avoid long wait periods
	//  set the max to 10.
	_network_reconnect = min(_local_player + 1, 10);
	_switch_mode_errorstr = STR_NETWORK_SERVER_REBOOT;

	return NETWORK_RECV_STATUS_SERVER_ERROR;
}




// The layout for the receive-functions by the client
typedef NetworkRecvStatus NetworkClientPacket(Packet *p);

// This array matches PacketType. At an incoming
//  packet it is matches against this array
//  and that way the right function to handle that
//  packet is found.
static NetworkClientPacket* const _network_client_packet[] = {
	RECEIVE_COMMAND(PACKET_SERVER_FULL),
	NULL, /*PACKET_CLIENT_JOIN,*/
	RECEIVE_COMMAND(PACKET_SERVER_ERROR),
	NULL, /*PACKET_CLIENT_COMPANY_INFO,*/
	RECEIVE_COMMAND(PACKET_SERVER_COMPANY_INFO),
	RECEIVE_COMMAND(PACKET_SERVER_CLIENT_INFO),
	RECEIVE_COMMAND(PACKET_SERVER_NEED_PASSWORD),
	NULL, /*PACKET_CLIENT_PASSWORD,*/
	RECEIVE_COMMAND(PACKET_SERVER_WELCOME),
	NULL, /*PACKET_CLIENT_GETMAP,*/
	RECEIVE_COMMAND(PACKET_SERVER_WAIT),
	RECEIVE_COMMAND(PACKET_SERVER_MAP),
	NULL, /*PACKET_CLIENT_MAP_OK,*/
	RECEIVE_COMMAND(PACKET_SERVER_JOIN),
	RECEIVE_COMMAND(PACKET_SERVER_FRAME),
	RECEIVE_COMMAND(PACKET_SERVER_SYNC),
	NULL, /*PACKET_CLIENT_ACK,*/
	NULL, /*PACKET_CLIENT_COMMAND,*/
	RECEIVE_COMMAND(PACKET_SERVER_COMMAND),
	NULL, /*PACKET_CLIENT_CHAT,*/
	RECEIVE_COMMAND(PACKET_SERVER_CHAT),
	NULL, /*PACKET_CLIENT_SET_PASSWORD,*/
	NULL, /*PACKET_CLIENT_SET_NAME,*/
	NULL, /*PACKET_CLIENT_QUIT,*/
	NULL, /*PACKET_CLIENT_ERROR,*/
	RECEIVE_COMMAND(PACKET_SERVER_QUIT),
	RECEIVE_COMMAND(PACKET_SERVER_ERROR_QUIT),
	RECEIVE_COMMAND(PACKET_SERVER_SHUTDOWN),
	RECEIVE_COMMAND(PACKET_SERVER_NEWGAME),
};

// If this fails, check the array above with network_data.h
assert_compile(lengthof(_network_client_packet) == PACKET_END);

extern const SettingDesc patch_settings[];

// This is a TEMPORARY solution to get the patch-settings
//  to the client. When the patch-settings are saved in the savegame
//  this should be removed!!
void NetworkRecvPatchSettings(Packet *p)
{
	const SettingDesc *item;

	item = patch_settings;

	while (item->name != NULL) {
		switch (item->flags) {
			case SDT_BOOL:
			case SDT_INT8:
			case SDT_UINT8:
				*(uint8 *)(item->ptr) = NetworkRecv_uint8(p);
				break;
			case SDT_INT16:
			case SDT_UINT16:
				*(uint16 *)(item->ptr) = NetworkRecv_uint16(p);
				break;
			case SDT_INT32:
			case SDT_UINT32:
				*(uint32 *)(item->ptr) = NetworkRecv_uint32(p);
				break;
		}
		item++;
	}
}

// Is called after a client is connected to the server
void NetworkClient_Connected(void)
{
	// Set the frame-counter to 0 so nothing happens till we are ready
	_frame_counter = 0;
	_frame_counter_server = 0;
	last_ack_frame = 0;
	// Request the game-info
	SEND_COMMAND(PACKET_CLIENT_JOIN)();
}

// Reads the packets from the socket-stream, if available
NetworkRecvStatus NetworkClient_ReadPackets(NetworkClientState *cs)
{
	Packet *p;
	NetworkRecvStatus res = NETWORK_RECV_STATUS_OKAY;

	while (res == NETWORK_RECV_STATUS_OKAY && (p = NetworkRecv_Packet(cs, &res)) != NULL) {
		byte type = NetworkRecv_uint8(p);
		if (type < PACKET_END && _network_client_packet[type] != NULL) {
			res = _network_client_packet[type](p);
		}	else {
			res = NETWORK_RECV_STATUS_MALFORMED_PACKET;
			DEBUG(net, 0)("[NET][client] Received invalid packet type %d", type);
		}

		free(p);
	}

	return res;
}

#endif /* ENABLE_NETWORK */
