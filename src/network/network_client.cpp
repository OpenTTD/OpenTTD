/* $Id$ */

#ifdef ENABLE_NETWORK

#include "../stdafx.h"
#include "../debug.h"
#include "../string.h"
#include "../strings.h"
#include "network_data.h"
#include "core/tcp.h"
#include "../date.h"
#include "table/strings.h"
#include "../functions.h"
#include "network_client.h"
#include "network_gamelist.h"
#include "network_gui.h"
#include "../saveload.h"
#include "../command.h"
#include "../window.h"
#include "../console.h"
#include "../variables.h"
#include "../ai/ai.h"
#include "../helpers.hpp"

// This file handles all the client-commands


// So we don't make too much typos ;)
#define MY_CLIENT DEREF_CLIENT(0)

static uint32 last_ack_frame;

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
	MY_CLIENT->Send_Packet(p);
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

	extern const char _openttd_revision[];
	Packet *p;
	_network_join_status = NETWORK_JOIN_STATUS_AUTHORIZING;
	InvalidateWindow(WC_NETWORK_STATUS_WINDOW, 0);

	p = NetworkSend_Init(PACKET_CLIENT_JOIN);
	p->Send_string(_openttd_revision);
	p->Send_string(_network_player_name); // Player name
	p->Send_uint8 (_network_playas);      // PlayAs
	p->Send_uint8 (NETLANG_ANY);          // Language
	p->Send_string(_network_unique_id);
	MY_CLIENT->Send_Packet(p);
}

DEF_CLIENT_SEND_COMMAND(PACKET_CLIENT_NEWGRFS_CHECKED)
{
	//
	// Packet: CLIENT_NEWGRFS_CHECKED
	// Function: Tell the server that we have the required GRFs
	// Data:
	//

	Packet *p = NetworkSend_Init(PACKET_CLIENT_NEWGRFS_CHECKED);
	MY_CLIENT->Send_Packet(p);
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
	p->Send_uint8 (type);
	p->Send_string(password);
	MY_CLIENT->Send_Packet(p);
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
	MY_CLIENT->Send_Packet(p);
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
	MY_CLIENT->Send_Packet(p);
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

	p->Send_uint32(_frame_counter);
	MY_CLIENT->Send_Packet(p);
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
	//    string: text
	//    uint8:  CallBackID (see callback_table.c)
	//

	Packet *p = NetworkSend_Init(PACKET_CLIENT_COMMAND);

	p->Send_uint8 (cp->player);
	p->Send_uint32(cp->cmd);
	p->Send_uint32(cp->p1);
	p->Send_uint32(cp->p2);
	p->Send_uint32((uint32)cp->tile);
	p->Send_string(cp->text);
	p->Send_uint8 (cp->callback);

	MY_CLIENT->Send_Packet(p);
}

// Send a chat-packet over the network
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_CHAT)(NetworkAction action, DestType type, int dest, const char *msg)
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

	p->Send_uint8 (action);
	p->Send_uint8 (type);
	p->Send_uint8 (dest);
	p->Send_string(msg);
	MY_CLIENT->Send_Packet(p);
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

	p->Send_uint8(errorno);
	MY_CLIENT->Send_Packet(p);
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

	p->Send_string(password);
	MY_CLIENT->Send_Packet(p);
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

	p->Send_string(name);
	MY_CLIENT->Send_Packet(p);
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

	p->Send_string(leavemsg);
	MY_CLIENT->Send_Packet(p);
}

DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_RCON)(const char *pass, const char *command)
{
	Packet *p = NetworkSend_Init(PACKET_CLIENT_RCON);
	p->Send_string(pass);
	p->Send_string(command);
	MY_CLIENT->Send_Packet(p);
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

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_BANNED)
{
	// We try to join a server where we are banned
	_switch_mode_errorstr = STR_NETWORK_ERR_SERVER_BANNED;
	DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);

	return NETWORK_RECV_STATUS_SERVER_BANNED;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_COMPANY_INFO)
{
	byte company_info_version;
	int i;

	company_info_version = p->Recv_uint8();

	if (!MY_CLIENT->has_quit && company_info_version == NETWORK_COMPANY_INFO_VERSION) {
		byte total;
		PlayerID current;

		total = p->Recv_uint8();

		// There is no data at all..
		if (total == 0) return NETWORK_RECV_STATUS_CLOSE_QUERY;

		current = (Owner)p->Recv_uint8();
		if (!IsValidPlayer(current)) return NETWORK_RECV_STATUS_CLOSE_QUERY;

		p->Recv_string(_network_player_info[current].company_name, sizeof(_network_player_info[current].company_name));
		_network_player_info[current].inaugurated_year = p->Recv_uint32();
		_network_player_info[current].company_value    = p->Recv_uint64();
		_network_player_info[current].money            = p->Recv_uint64();
		_network_player_info[current].income           = p->Recv_uint64();
		_network_player_info[current].performance      = p->Recv_uint16();
		_network_player_info[current].use_password     = p->Recv_bool();
		for (i = 0; i < NETWORK_VEHICLE_TYPES; i++)
			_network_player_info[current].num_vehicle[i] = p->Recv_uint16();
		for (i = 0; i < NETWORK_STATION_TYPES; i++)
			_network_player_info[current].num_station[i] = p->Recv_uint16();

		p->Recv_string(_network_player_info[current].players, sizeof(_network_player_info[current].players));

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
	uint16 index = p->Recv_uint16();
	PlayerID playas = (Owner)p->Recv_uint8();
	char name[NETWORK_NAME_LENGTH];
	char unique_id[NETWORK_NAME_LENGTH];

	p->Recv_string(name, sizeof(name));
	p->Recv_string(unique_id, sizeof(unique_id));

	if (MY_CLIENT->has_quit) return NETWORK_RECV_STATUS_CONN_LOST;

	/* Do we receive a change of data? Most likely we changed playas */
	if (index == _network_own_client_index) _network_playas = playas;

	ci = NetworkFindClientInfoFromIndex(index);
	if (ci != NULL) {
		if (playas == ci->client_playas && strcmp(name, ci->client_name) != 0) {
			// Client name changed, display the change
			NetworkTextMessage(NETWORK_ACTION_NAME_CHANGE, 1, false, ci->client_name, "%s", name);
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
	NetworkErrorCode error = (NetworkErrorCode)p->Recv_uint8();

	switch (error) {
		/* We made an error in the protocol, and our connection is closed.... */
		case NETWORK_ERROR_NOT_AUTHORIZED:
		case NETWORK_ERROR_NOT_EXPECTED:
		case NETWORK_ERROR_PLAYER_MISMATCH:
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
		case NETWORK_GAME_PASSWORD:
		case NETWORK_COMPANY_PASSWORD:
			ShowNetworkNeedPassword(type);
			return NETWORK_RECV_STATUS_OKAY;

		default: return NETWORK_RECV_STATUS_MALFORMED_PACKET;
	}
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_WELCOME)
{
	_network_own_client_index = p->Recv_uint16();

	// Start receiving the map
	SEND_COMMAND(PACKET_CLIENT_GETMAP)();
	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_WAIT)
{
	_network_join_status = NETWORK_JOIN_STATUS_WAITING;
	_network_join_waiting = p->Recv_uint8();
	InvalidateWindow(WC_NETWORK_STATUS_WINDOW, 0);

	// We are put on hold for receiving the map.. we need GUI for this ;)
	DEBUG(net, 1, "The server is currently busy sending the map to someone else, please wait..." );
	DEBUG(net, 1, "There are %d clients in front of you", _network_join_waiting);

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_MAP)
{
	static char filename[256];
	static FILE *file_pointer;

	byte maptype;

	maptype = p->Recv_uint8();

	if (MY_CLIENT->has_quit) return NETWORK_RECV_STATUS_CONN_LOST;

	// First packet, init some stuff
	if (maptype == MAP_PACKET_START) {
		// The name for the temp-map
		snprintf(filename, lengthof(filename), "%s%snetwork_client.tmp",  _paths.autosave_dir, PATHSEP);

		file_pointer = fopen(filename, "wb");
		if (file_pointer == NULL) {
			_switch_mode_errorstr = STR_NETWORK_ERR_SAVEGAMEERROR;
			return NETWORK_RECV_STATUS_SAVEGAME;
		}

		_frame_counter = _frame_counter_server = _frame_counter_max = p->Recv_uint32();

		_network_join_kbytes = 0;
		_network_join_kbytes_total = p->Recv_uint32() / 1024;

		/* If the network connection has been closed due to loss of connection
		 * or when _network_join_kbytes_total is 0, the join status window will
		 * do a division by zero. When the connection is lost, we just return
		 * that. If kbytes_total is 0, the packet must be malformed as a
		 * savegame less than 1 kilobyte is practically impossible. */
		if (MY_CLIENT->has_quit) return NETWORK_RECV_STATUS_CONN_LOST;
		if (_network_join_kbytes_total == 0) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

		_network_join_status = NETWORK_JOIN_STATUS_DOWNLOADING;
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

	// Check if this was the last packet
	if (maptype == MAP_PACKET_END) {
		fclose(file_pointer);

		_network_join_status = NETWORK_JOIN_STATUS_PROCESSING;
		InvalidateWindow(WC_NETWORK_STATUS_WINDOW, 0);

		/* The map is done downloading, load it */
		if (!SafeSaveOrLoad(filename, SL_LOAD, GM_NORMAL)) {
			DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
			_switch_mode_errorstr = STR_NETWORK_ERR_SAVEGAMEERROR;
			return NETWORK_RECV_STATUS_SAVEGAME;
		}
		/* If the savegame has successfully loaded, ALL windows have been removed,
		 * only toolbar/statusbar and gamefield are visible */

		_opt_ptr = &_opt; // during a network game you are always in-game

		// Say we received the map and loaded it correctly!
		SEND_COMMAND(PACKET_CLIENT_MAP_OK)();

		/* New company/spectator (invalid player) or company we want to join is not active
		 * Switch local player to spectator and await the server's judgement */
		if (_network_playas == PLAYER_NEW_COMPANY || !IsValidPlayer(_network_playas) ||
				!GetPlayer(_network_playas)->is_active) {

			SetLocalPlayer(PLAYER_SPECTATOR);

			if (_network_playas != PLAYER_SPECTATOR) {
				/* We have arrived and ready to start playing; send a command to make a new player;
				 * the server will give us a client-id and let us in */
				_network_join_status = NETWORK_JOIN_STATUS_REGISTERING;
				ShowJoinStatusWindow();
				NetworkSend_Command(0, 0, 0, CMD_PLAYER_CTRL, NULL);
			}
		} else {
			// take control over an existing company
			SetLocalPlayer(_network_playas);
		}
	}

	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_FRAME)
{
	_frame_counter_server = p->Recv_uint32();
	_frame_counter_max = p->Recv_uint32();
#ifdef ENABLE_NETWORK_SYNC_EVERY_FRAME
	// Test if the server supports this option
	//  and if we are at the frame the server is
	if (p->pos < p->size) {
		_sync_frame = _frame_counter_server;
		_sync_seed_1 = p->Recv_uint32();
#ifdef NETWORK_SEND_DOUBLE_SEED
		_sync_seed_2 = p->Recv_uint32();
#endif
	}
#endif
	DEBUG(net, 5, "Received FRAME %d", _frame_counter_server);

	// Let the server know that we received this frame correctly
	//  We do this only once per day, to save some bandwidth ;)
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
	CommandPacket *cp = MallocT<CommandPacket>(1);
	cp->player   = (PlayerID)p->Recv_uint8();
	cp->cmd      = p->Recv_uint32();
	cp->p1       = p->Recv_uint32();
	cp->p2       = p->Recv_uint32();
	cp->tile     = p->Recv_uint32();
	p->Recv_string(cp->text, sizeof(cp->text));
	cp->callback = p->Recv_uint8();
	cp->frame    = p->Recv_uint32();
	cp->next     = NULL;

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
	char name[NETWORK_NAME_LENGTH], msg[MAX_TEXT_MSG_LEN];
	const NetworkClientInfo *ci = NULL, *ci_to;

	NetworkAction action = (NetworkAction)p->Recv_uint8();
	uint16 index = p->Recv_uint16();
	bool self_send = p->Recv_bool();
	p->Recv_string(msg, MAX_TEXT_MSG_LEN);

	ci_to = NetworkFindClientInfoFromIndex(index);
	if (ci_to == NULL) return NETWORK_RECV_STATUS_OKAY;

	/* Did we initiate the action locally? */
	if (self_send) {
		switch (action) {
			case NETWORK_ACTION_CHAT_CLIENT:
				/* For speaking to client we need the client-name */
				snprintf(name, sizeof(name), "%s", ci_to->client_name);
				ci = NetworkFindClientInfoFromIndex(_network_own_client_index);
				break;

			/* For speaking to company or giving money, we need the player-name */
			case NETWORK_ACTION_GIVE_MONEY:
				if (!IsValidPlayer(ci_to->client_playas)) return NETWORK_RECV_STATUS_OKAY;
				/* fallthrough */
			case NETWORK_ACTION_CHAT_COMPANY: {
				StringID str = IsValidPlayer(ci_to->client_playas) ? GetPlayer(ci_to->client_playas)->name_1 : (uint16)STR_NETWORK_SPECTATORS;

				GetString(name, str, lastof(name));
				ci = NetworkFindClientInfoFromIndex(_network_own_client_index);
			} break;

			default: NOT_REACHED(); break;
		}
	} else {
		/* Display message from somebody else */
		snprintf(name, sizeof(name), "%s", ci_to->client_name);
		ci = ci_to;
	}

	if (ci != NULL)
		NetworkTextMessage(action, GetDrawStringPlayerColor(ci->client_playas), self_send, name, "%s", msg);
	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_ERROR_QUIT)
{
	char str[100];
	uint16 index;
	NetworkClientInfo *ci;

	index = p->Recv_uint16();
	GetNetworkErrorMsg(str, (NetworkErrorCode)p->Recv_uint8(), lastof(str));

	ci = NetworkFindClientInfoFromIndex(index);
	if (ci != NULL) {
		NetworkTextMessage(NETWORK_ACTION_LEAVE, 1, false, ci->client_name, "%s", str);

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

	index = p->Recv_uint16();
	p->Recv_string(str, lengthof(str));

	ci = NetworkFindClientInfoFromIndex(index);
	if (ci != NULL) {
		NetworkTextMessage(NETWORK_ACTION_LEAVE, 1, false, ci->client_name, "%s", str);

		// The client is gone, give the NetworkClientInfo free
		ci->client_index = NETWORK_EMPTY_INDEX;
	} else {
		DEBUG(net, 0, "Unknown client (%d) is leaving the game", index);
	}

	InvalidateWindow(WC_CLIENT_LIST, 0);

	// If we come here it means we could not locate the client.. strange :s
	return NETWORK_RECV_STATUS_OKAY;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_JOIN)
{
	uint16 index;
	NetworkClientInfo *ci;

	index = p->Recv_uint16();

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
	// PLAYER_SPECTATOR is currently 255, so to avoid long wait periods
	//  set the max to 10.
	_network_reconnect = min(_local_player + 1, 10);
	_switch_mode_errorstr = STR_NETWORK_SERVER_REBOOT;

	return NETWORK_RECV_STATUS_SERVER_ERROR;
}

DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_RCON)
{
	char rcon_out[NETWORK_RCONCOMMAND_LENGTH];
	uint16 color_code;

	color_code = p->Recv_uint16();
	p->Recv_string(rcon_out, sizeof(rcon_out));

	IConsolePrint(color_code, rcon_out);

	return NETWORK_RECV_STATUS_OKAY;
}



// The layout for the receive-functions by the client
typedef NetworkRecvStatus NetworkClientPacket(Packet *p);

// This array matches PacketType. At an incoming
//  packet it is matches against this array
//  and that way the right function to handle that
//  packet is found.
static NetworkClientPacket* const _network_client_packet[] = {
	RECEIVE_COMMAND(PACKET_SERVER_FULL),
	RECEIVE_COMMAND(PACKET_SERVER_BANNED),
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
	RECEIVE_COMMAND(PACKET_SERVER_RCON),
	NULL, /*PACKET_CLIENT_RCON,*/
	RECEIVE_COMMAND(PACKET_SERVER_CHECK_NEWGRFS),
	NULL, /*PACKET_CLIENT_NEWGRFS_CHECKED,*/
};

// If this fails, check the array above with network_data.h
assert_compile(lengthof(_network_client_packet) == PACKET_END);

// Is called after a client is connected to the server
void NetworkClient_Connected()
{
	// Set the frame-counter to 0 so nothing happens till we are ready
	_frame_counter = 0;
	_frame_counter_server = 0;
	last_ack_frame = 0;
	// Request the game-info
	SEND_COMMAND(PACKET_CLIENT_JOIN)();
}

// Reads the packets from the socket-stream, if available
NetworkRecvStatus NetworkClient_ReadPackets(NetworkTCPSocketHandler *cs)
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

#endif /* ENABLE_NETWORK */
