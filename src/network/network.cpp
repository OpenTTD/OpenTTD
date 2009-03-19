/* $Id$ */

/** @file network.cpp Base functions for networking support. */

#include "../stdafx.h"
#include "../company_type.h"

#ifdef ENABLE_NETWORK

#include "../openttd.h"
#include "../strings_func.h"
#include "../command_func.h"
#include "../variables.h"
#include "../date_func.h"
#include "network_internal.h"
#include "network_client.h"
#include "network_server.h"
#include "network_content.h"
#include "network_udp.h"
#include "network_gamelist.h"
#include "core/udp.h"
#include "core/host.h"
#include "network_gui.h"
#include "../console_func.h"
#include "../md5.h"
#include "../core/random_func.hpp"
#include "../window_func.h"
#include "../string_func.h"
#include "../company_func.h"
#include "../company_base.h"
#include "../settings_type.h"
#include "../landscape_type.h"
#include "../rev.h"
#include "../core/alloc_func.hpp"
#ifdef DEBUG_DUMP_COMMANDS
	#include "../fileio_func.h"
#endif /* DEBUG_DUMP_COMMANDS */
#include "table/strings.h"
#include "../oldpool_func.h"

DECLARE_POSTFIX_INCREMENT(ClientID);

typedef ClientIndex NetworkClientInfoID;
DEFINE_OLD_POOL_GENERIC(NetworkClientInfo, NetworkClientInfo);

bool _networking;         ///< are we in networking mode?
bool _network_server;     ///< network-server is active
bool _network_available;  ///< is network mode available?
bool _network_dedicated;  ///< are we a dedicated server?
bool _is_network_server;  ///< Does this client wants to be a network-server?
NetworkServerGameInfo _network_game_info;
NetworkCompanyState *_network_company_states = NULL;
ClientID _network_own_client_id;
ClientID _redirect_console_to_client;
bool _network_need_advertise;
uint32 _network_last_advertise_frame;
uint8 _network_reconnect;
char *_network_host_list[10];
char *_network_ban_list[25];
uint32 _frame_counter_server; // The frame_counter of the server, if in network-mode
uint32 _frame_counter_max; // To where we may go with our clients
uint32 _frame_counter;
uint32 _last_sync_frame; // Used in the server to store the last time a sync packet was sent to clients.
uint32 _broadcast_list[MAX_INTERFACES + 1];
uint32 _network_server_bind_ip;
uint32 _sync_seed_1, _sync_seed_2;
uint32 _sync_frame;
bool _network_first_time;
bool _network_udp_server;
uint16 _network_udp_broadcast;
uint8 _network_advertise_retries;
CompanyMask _network_company_passworded; ///< Bitmask of the password status of all companies.

/* Check whether NETWORK_NUM_LANDSCAPES is still in sync with NUM_LANDSCAPE */
assert_compile((int)NETWORK_NUM_LANDSCAPES == (int)NUM_LANDSCAPE);
assert_compile((int)NETWORK_COMPANY_NAME_LENGTH == MAX_LENGTH_COMPANY_NAME_BYTES);

extern NetworkUDPSocketHandler *_udp_client_socket; ///< udp client socket
extern NetworkUDPSocketHandler *_udp_server_socket; ///< udp server socket
extern NetworkUDPSocketHandler *_udp_master_socket; ///< udp master socket

/* The listen socket for the server */
static SOCKET _listensocket;

/* The amount of clients connected */
static byte _network_clients_connected = 0;
/* The identifier counter for new clients (is never decreased) */
static ClientID _network_client_id = CLIENT_ID_FIRST;

/* Some externs / forwards */
extern void StateGameLoop();

/**
 * Return the CI given it's raw index
 * @param index the index to search for
 * @return return a pointer to the corresponding NetworkClientInfo struct
 */
NetworkClientInfo *NetworkFindClientInfoFromIndex(ClientIndex index)
{
	return IsValidNetworkClientInfoIndex(index) ? GetNetworkClientInfo(index) : NULL;
}

/**
 * Return the CI given it's client-identifier
 * @param client_id the ClientID to search for
 * @return return a pointer to the corresponding NetworkClientInfo struct or NULL when not found
 */
NetworkClientInfo *NetworkFindClientInfoFromClientID(ClientID client_id)
{
	NetworkClientInfo *ci;

	FOR_ALL_CLIENT_INFOS(ci) {
		if (ci->client_id == client_id) return ci;
	}

	return NULL;
}

/**
 * Return the CI for a given IP
 * @param ip IP of the client we are looking for. This must be in string-format
 * @return return a pointer to the corresponding NetworkClientInfo struct or NULL when not found
 */
NetworkClientInfo *NetworkFindClientInfoFromIP(const char *ip)
{
	NetworkClientInfo *ci;
	uint32 ip_number = inet_addr(ip);

	FOR_ALL_CLIENT_INFOS(ci) {
		if (ci->client_ip == ip_number) return ci;
	}

	return NULL;
}

/**
 * Return the client state given it's client-identifier
 * @param client_id the ClientID to search for
 * @return return a pointer to the corresponding NetworkClientSocket struct or NULL when not found
 */
NetworkClientSocket *NetworkFindClientStateFromClientID(ClientID client_id)
{
	NetworkClientSocket *cs;

	FOR_ALL_CLIENT_SOCKETS(cs) {
		if (cs->client_id == client_id) return cs;
	}

	return NULL;
}

/* NetworkGetClientName is a server-safe function to get the name of the client
 *  if the user did not send it yet, Client #<no> is used. */
void NetworkGetClientName(char *client_name, size_t size, const NetworkClientSocket *cs)
{
	const NetworkClientInfo *ci = cs->GetInfo();

	if (StrEmpty(ci->client_name)) {
		snprintf(client_name, size, "Client #%4d", cs->client_id);
	} else {
		ttd_strlcpy(client_name, ci->client_name, size);
	}
}

byte NetworkSpectatorCount()
{
	const NetworkClientInfo *ci;
	byte count = 0;

	FOR_ALL_CLIENT_INFOS(ci) {
		if (ci->client_playas == COMPANY_SPECTATOR) count++;
	}

	/* Don't count a dedicated server as spectator */
	if (_network_dedicated) count--;

	return count;
}

/**
 * Check if the company we want to join requires a password.
 * @param company_id id of the company we want to check the 'passworded' flag for.
 * @return true if the company requires a password.
 */
bool NetworkCompanyIsPassworded(CompanyID company_id)
{
	return HasBit(_network_company_passworded, company_id);
}

/* This puts a text-message to the console, or in the future, the chat-box,
 *  (to keep it all a bit more general)
 * If 'self_send' is true, this is the client who is sending the message */
void NetworkTextMessage(NetworkAction action, ConsoleColour colour, bool self_send, const char *name, const char *str, int64 data)
{
	const int duration = 10; // Game days the messages stay visible

	StringID strid;
	switch (action) {
		case NETWORK_ACTION_SERVER_MESSAGE:
			/* Ignore invalid messages */
			if (data >= NETWORK_SERVER_MESSAGE_END) return;

			strid = STR_NETWORK_SERVER_MESSAGE;
			colour = CC_DEFAULT;
			data = STR_NETWORK_SERVER_MESSAGE_GAME_PAUSED_PLAYERS + data;
			break;
		case NETWORK_ACTION_COMPANY_SPECTATOR:
			colour = CC_DEFAULT;
			strid = STR_NETWORK_CLIENT_COMPANY_SPECTATE;
			break;
		case NETWORK_ACTION_COMPANY_JOIN:
			colour = CC_DEFAULT;
			strid = STR_NETWORK_CLIENT_COMPANY_JOIN;
			break;
		case NETWORK_ACTION_COMPANY_NEW:
			colour = CC_DEFAULT;
			strid = STR_NETWORK_CLIENT_COMPANY_NEW;
			break;
		case NETWORK_ACTION_JOIN:           strid = STR_NETWORK_CLIENT_JOINED; break;
		case NETWORK_ACTION_LEAVE:          strid = STR_NETWORK_CLIENT_LEFT; break;
		case NETWORK_ACTION_NAME_CHANGE:    strid = STR_NETWORK_NAME_CHANGE; break;
		case NETWORK_ACTION_GIVE_MONEY:     strid = self_send ? STR_NETWORK_GAVE_MONEY_AWAY : STR_NETWORK_GIVE_MONEY;   break;
		case NETWORK_ACTION_CHAT_COMPANY:   strid = self_send ? STR_NETWORK_CHAT_TO_COMPANY : STR_NETWORK_CHAT_COMPANY; break;
		case NETWORK_ACTION_CHAT_CLIENT:    strid = self_send ? STR_NETWORK_CHAT_TO_CLIENT  : STR_NETWORK_CHAT_CLIENT;  break;
		default:                            strid = STR_NETWORK_CHAT_ALL; break;
	}

	char message[1024];
	SetDParamStr(0, name);
	SetDParamStr(1, str);
	SetDParam(2, data);
	GetString(message, strid, lastof(message));

	DEBUG(desync, 1, "msg: %d; %d; %s\n", _date, _date_fract, message);
	IConsolePrintF(colour, "%s", message);
	NetworkAddChatMessage((TextColour)colour, duration, "%s", message);
}

/* Calculate the frame-lag of a client */
uint NetworkCalculateLag(const NetworkClientSocket *cs)
{
	int lag = cs->last_frame_server - cs->last_frame;
	/* This client has missed his ACK packet after 1 DAY_TICKS..
	 *  so we increase his lag for every frame that passes!
	 * The packet can be out by a max of _net_frame_freq */
	if (cs->last_frame_server + DAY_TICKS + _settings_client.network.frame_freq < _frame_counter)
		lag += _frame_counter - (cs->last_frame_server + DAY_TICKS + _settings_client.network.frame_freq);

	return lag;
}


/* There was a non-recoverable error, drop back to the main menu with a nice
 *  error */
static void NetworkError(StringID error_string)
{
	_switch_mode = SM_MENU;
	extern StringID _switch_mode_errorstr;
	_switch_mode_errorstr = error_string;
}

static void ServerStartError(const char *error)
{
	DEBUG(net, 0, "[server] could not start network: %s",error);
	NetworkError(STR_NETWORK_ERR_SERVER_START);
}

static void NetworkClientError(NetworkRecvStatus res, NetworkClientSocket *cs)
{
	/* First, send a CLIENT_ERROR to the server, so he knows we are
	 *  disconnection (and why!) */
	NetworkErrorCode errorno;

	/* We just want to close the connection.. */
	if (res == NETWORK_RECV_STATUS_CLOSE_QUERY) {
		cs->has_quit = true;
		NetworkCloseClient(cs);
		_networking = false;

		DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
		return;
	}

	switch (res) {
		case NETWORK_RECV_STATUS_DESYNC:          errorno = NETWORK_ERROR_DESYNC; break;
		case NETWORK_RECV_STATUS_SAVEGAME:        errorno = NETWORK_ERROR_SAVEGAME_FAILED; break;
		case NETWORK_RECV_STATUS_NEWGRF_MISMATCH: errorno = NETWORK_ERROR_NEWGRF_MISMATCH; break;
		default:                                  errorno = NETWORK_ERROR_GENERAL; break;
	}
	/* This means we fucked up and the server closed the connection */
	if (res != NETWORK_RECV_STATUS_SERVER_ERROR && res != NETWORK_RECV_STATUS_SERVER_FULL &&
			res != NETWORK_RECV_STATUS_SERVER_BANNED) {
		SEND_COMMAND(PACKET_CLIENT_ERROR)(errorno);
	}

	_switch_mode = SM_MENU;
	NetworkCloseClient(cs);
	_networking = false;
}

/**
 * Retrieve the string id of an internal error number
 * @param err NetworkErrorCode
 * @return the StringID
 */
StringID GetNetworkErrorMsg(NetworkErrorCode err)
{
	/* List of possible network errors, used by
	 * PACKET_SERVER_ERROR and PACKET_CLIENT_ERROR */
	static const StringID network_error_strings[] = {
		STR_NETWORK_ERR_CLIENT_GENERAL,
		STR_NETWORK_ERR_CLIENT_DESYNC,
		STR_NETWORK_ERR_CLIENT_SAVEGAME,
		STR_NETWORK_ERR_CLIENT_CONNECTION_LOST,
		STR_NETWORK_ERR_CLIENT_PROTOCOL_ERROR,
		STR_NETWORK_ERR_CLIENT_NEWGRF_MISMATCH,
		STR_NETWORK_ERR_CLIENT_NOT_AUTHORIZED,
		STR_NETWORK_ERR_CLIENT_NOT_EXPECTED,
		STR_NETWORK_ERR_CLIENT_WRONG_REVISION,
		STR_NETWORK_ERR_CLIENT_NAME_IN_USE,
		STR_NETWORK_ERR_CLIENT_WRONG_PASSWORD,
		STR_NETWORK_ERR_CLIENT_COMPANY_MISMATCH,
		STR_NETWORK_ERR_CLIENT_KICKED,
		STR_NETWORK_ERR_CLIENT_CHEATER,
		STR_NETWORK_ERR_CLIENT_SERVER_FULL
	};

	if (err >= (ptrdiff_t)lengthof(network_error_strings)) err = NETWORK_ERROR_GENERAL;

	return network_error_strings[err];
}

/* Count the number of active clients connected */
static uint NetworkCountActiveClients()
{
	const NetworkClientInfo *ci;
	uint count = 0;

	FOR_ALL_CLIENT_INFOS(ci) {
		if (IsValidCompanyID(ci->client_playas)) count++;
	}

	return count;
}

static bool _min_active_clients_paused = false;

/* Check if the minimum number of active clients has been reached and pause or unpause the game as appropriate */
static void CheckMinActiveClients()
{
	if (!_network_dedicated) return;

	if (NetworkCountActiveClients() < _settings_client.network.min_active_clients) {
		if (_min_active_clients_paused) return;

		_min_active_clients_paused = true;
		DoCommandP(0, 1, 0, CMD_PAUSE);
		NetworkServerSendChat(NETWORK_ACTION_SERVER_MESSAGE, DESTTYPE_BROADCAST, 0, "", CLIENT_ID_SERVER, NETWORK_SERVER_MESSAGE_GAME_PAUSED_PLAYERS);
	} else {
		if (!_min_active_clients_paused) return;

		_min_active_clients_paused = false;
		DoCommandP(0, 0, 0, CMD_PAUSE);
		NetworkServerSendChat(NETWORK_ACTION_SERVER_MESSAGE, DESTTYPE_BROADCAST, 0, "", CLIENT_ID_SERVER, NETWORK_SERVER_MESSAGE_GAME_UNPAUSED_PLAYERS);
	}
}

/** Converts a string to ip/port/company
 *  Format: IP#company:port
 *
 * connection_string will be re-terminated to seperate out the hostname, and company and port will
 * be set to the company and port strings given by the user, inside the memory area originally
 * occupied by connection_string. */
void ParseConnectionString(const char **company, const char **port, char *connection_string)
{
	char *p;
	for (p = connection_string; *p != '\0'; p++) {
		switch (*p) {
			case '#':
				*company = p + 1;
				*p = '\0';
				break;
			case ':':
				*port = p + 1;
				*p = '\0';
				break;
		}
	}
}

/* Creates a new client from a socket
 *   Used both by the server and the client */
static NetworkClientSocket *NetworkAllocClient(SOCKET s)
{
	if (_network_server) {
		/* Can we handle a new client? */
		if (_network_clients_connected >= MAX_CLIENTS) return NULL;
		if (_network_game_info.clients_on >= _settings_client.network.max_clients) return NULL;

		/* Register the login */
		_network_clients_connected++;
	}

	NetworkClientSocket *cs = new NetworkClientSocket(INVALID_CLIENT_ID);
	cs->sock = s;
	cs->last_frame = _frame_counter;
	cs->last_frame_server = _frame_counter;

	if (_network_server) {
		cs->client_id = _network_client_id++;
		NetworkClientInfo *ci = new NetworkClientInfo(cs->client_id);
		cs->SetInfo(ci);
		ci->client_playas = COMPANY_INACTIVE_CLIENT;
		ci->join_date = _date;

		InvalidateWindow(WC_CLIENT_LIST, 0);
	}

	return cs;
}

/* Close a connection */
void NetworkCloseClient(NetworkClientSocket *cs)
{
	/*
	 * Sending a message just before leaving the game calls cs->Send_Packets.
	 * This might invoke this function, which means that when we close the
	 * connection after cs->Send_Packets we will close an already closed
	 * connection. This handles that case gracefully without having to make
	 * that code any more complex or more aware of the validity of the socket.
	 */
	if (cs->sock == INVALID_SOCKET) return;

	DEBUG(net, 1, "Closed client connection %d", cs->client_id);

	if (!cs->has_quit && _network_server && cs->status > STATUS_INACTIVE) {
		/* We did not receive a leave message from this client... */
		char client_name[NETWORK_CLIENT_NAME_LENGTH];
		NetworkClientSocket *new_cs;

		NetworkGetClientName(client_name, sizeof(client_name), cs);

		NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, client_name, NULL, STR_NETWORK_ERR_CLIENT_CONNECTION_LOST);

		/* Inform other clients of this... strange leaving ;) */
		FOR_ALL_CLIENT_SOCKETS(new_cs) {
			if (new_cs->status > STATUS_AUTH && cs != new_cs) {
				SEND_COMMAND(PACKET_SERVER_ERROR_QUIT)(new_cs, cs->client_id, NETWORK_ERROR_CONNECTION_LOST);
			}
		}
	}

	/* When the client was PRE_ACTIVE, the server was in pause mode, so unpause */
	if (cs->status == STATUS_PRE_ACTIVE && _settings_client.network.pause_on_join) {
		DoCommandP(0, 0, 0, CMD_PAUSE);
		NetworkServerSendChat(NETWORK_ACTION_SERVER_MESSAGE, DESTTYPE_BROADCAST, 0, "", CLIENT_ID_SERVER, NETWORK_SERVER_MESSAGE_GAME_UNPAUSED_CONNECT_FAIL);
	}

	if (_network_server) {
		/* We just lost one client :( */
		if (cs->status >= STATUS_AUTH) _network_game_info.clients_on--;
		_network_clients_connected--;

		InvalidateWindow(WC_CLIENT_LIST, 0);
	}

	delete cs->GetInfo();
	delete cs;
}

/* For the server, to accept new clients */
static void NetworkAcceptClients()
{
	struct sockaddr_in sin;
	NetworkClientSocket *cs;
	uint i;
	bool banned;

	/* Should never ever happen.. is it possible?? */
	assert(_listensocket != INVALID_SOCKET);

	for (;;) {
		socklen_t sin_len = sizeof(sin);
		SOCKET s = accept(_listensocket, (struct sockaddr*)&sin, &sin_len);
		if (s == INVALID_SOCKET) return;

		SetNonBlocking(s); // XXX error handling?

		DEBUG(net, 1, "Client connected from %s on frame %d", inet_ntoa(sin.sin_addr), _frame_counter);

		SetNoDelay(s); // XXX error handling?

		/* Check if the client is banned */
		banned = false;
		for (i = 0; i < lengthof(_network_ban_list); i++) {
			if (_network_ban_list[i] == NULL) continue;

			/* Check for CIDR separator */
			char *chr_cidr = strchr(_network_ban_list[i], '/');
			if (chr_cidr != NULL) {
				int cidr = atoi(chr_cidr + 1);

				/* Invalid CIDR, treat as single host */
				if (cidr <= 0 || cidr > 32) cidr = 32;

				/* Remove and then replace the / so that inet_addr() works on the IP portion */
				*chr_cidr = '\0';
				uint32 ban_ip = inet_addr(_network_ban_list[i]);
				*chr_cidr = '/';

				/* Convert CIDR to mask in network format */
				uint32 mask = htonl(-(1 << (32 - cidr)));
				if ((sin.sin_addr.s_addr & mask) == (ban_ip & mask)) banned = true;
			} else {
				/* No CIDR used, so just perform a simple IP test */
				if (sin.sin_addr.s_addr == inet_addr(_network_ban_list[i])) banned = true;
			}

			if (banned) {
				Packet p(PACKET_SERVER_BANNED);
				p.PrepareToSend();

				DEBUG(net, 1, "Banned ip tried to join (%s), refused", _network_ban_list[i]);

				send(s, (const char*)p.buffer, p.size, 0);
				closesocket(s);
				break;
			}
		}
		/* If this client is banned, continue with next client */
		if (banned) continue;

		cs = NetworkAllocClient(s);
		if (cs == NULL) {
			/* no more clients allowed?
			 * Send to the client that we are full! */
			Packet p(PACKET_SERVER_FULL);
			p.PrepareToSend();

			send(s, (const char*)p.buffer, p.size, 0);
			closesocket(s);

			continue;
		}

		/* a new client has connected. We set him at inactive for now
		 *  maybe he is only requesting server-info. Till he has sent a PACKET_CLIENT_MAP_OK
		 *  the client stays inactive */
		cs->status = STATUS_INACTIVE;

		cs->GetInfo()->client_ip = sin.sin_addr.s_addr; // Save the IP of the client
	}
}

/* Set up the listen socket for the server */
static bool NetworkListen()
{
	SOCKET ls;
	struct sockaddr_in sin;

	DEBUG(net, 1, "Listening on %s:%d", _settings_client.network.server_bind_ip, _settings_client.network.server_port);

	ls = socket(AF_INET, SOCK_STREAM, 0);
	if (ls == INVALID_SOCKET) {
		ServerStartError("socket() on listen socket failed");
		return false;
	}

	{ // reuse the socket
		int reuse = 1;
		/* The (const char*) cast is needed for windows!! */
		if (setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) == -1) {
			ServerStartError("setsockopt() on listen socket failed");
			return false;
		}
	}

	if (!SetNonBlocking(ls)) DEBUG(net, 0, "Setting non-blocking mode failed"); // XXX should this be an error?

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = _network_server_bind_ip;
	sin.sin_port = htons(_settings_client.network.server_port);

	if (bind(ls, (struct sockaddr*)&sin, sizeof(sin)) != 0) {
		ServerStartError("bind() failed");
		return false;
	}

	if (listen(ls, 1) != 0) {
		ServerStartError("listen() failed");
		return false;
	}

	_listensocket = ls;

	return true;
}

/** Resets both pools used for network clients */
static void InitializeNetworkPools()
{
	_NetworkClientSocket_pool.CleanPool();
	_NetworkClientSocket_pool.AddBlockToPool();
	_NetworkClientInfo_pool.CleanPool();
	_NetworkClientInfo_pool.AddBlockToPool();
}

/* Close all current connections */
static void NetworkClose()
{
	NetworkClientSocket *cs;

	FOR_ALL_CLIENT_SOCKETS(cs) {
		if (!_network_server) {
			SEND_COMMAND(PACKET_CLIENT_QUIT)();
			cs->Send_Packets();
		}
		NetworkCloseClient(cs);
	}

	if (_network_server) {
		/* We are a server, also close the listensocket */
		closesocket(_listensocket);
		_listensocket = INVALID_SOCKET;
		DEBUG(net, 1, "Closed listener");
	}
	NetworkUDPCloseAll();

	TCPConnecter::KillAll();

	_networking = false;
	_network_server = false;

	NetworkFreeLocalCommandQueue();

	free(_network_company_states);
	_network_company_states = NULL;

	InitializeNetworkPools();
}

/* Inits the network (cleans sockets and stuff) */
static void NetworkInitialize()
{
	InitializeNetworkPools();

	_sync_frame = 0;
	_network_first_time = true;

	_network_reconnect = 0;
}

/** Non blocking connection create to query servers */
class TCPQueryConnecter : TCPConnecter {
public:
	TCPQueryConnecter(const NetworkAddress &address) : TCPConnecter(address) {}

	virtual void OnFailure()
	{
		NetworkDisconnect();
	}

	virtual void OnConnect(SOCKET s)
	{
		_networking = true;
		NetworkAllocClient(s);
		SEND_COMMAND(PACKET_CLIENT_COMPANY_INFO)();
	}
};

/* Query a server to fetch his game-info
 *  If game_info is true, only the gameinfo is fetched,
 *   else only the client_info is fetched */
void NetworkTCPQueryServer(NetworkAddress address)
{
	if (!_network_available) return;

	NetworkDisconnect();
	NetworkInitialize();

	new TCPQueryConnecter(address);
}

/* Validates an address entered as a string and adds the server to
 * the list. If you use this function, the games will be marked
 * as manually added. */
void NetworkAddServer(const char *b)
{
	if (*b != '\0') {
		const char *port = NULL;
		const char *company = NULL;
		char host[NETWORK_HOSTNAME_LENGTH];
		uint16 rport;

		strecpy(host, b, lastof(host));

		strecpy(_settings_client.network.connect_to_ip, b, lastof(_settings_client.network.connect_to_ip));
		rport = NETWORK_DEFAULT_PORT;

		ParseConnectionString(&company, &port, host);
		if (port != NULL) rport = atoi(port);

		NetworkUDPQueryServer(NetworkAddress(host, rport), true);
	}
}

/* Generates the list of manually added hosts from NetworkGameList and
 * dumps them into the array _network_host_list. This array is needed
 * by the function that generates the config file. */
void NetworkRebuildHostList()
{
	uint i = 0;
	const NetworkGameList *item = _network_game_list;
	while (item != NULL && i != lengthof(_network_host_list)) {
		if (item->manually) {
			free(_network_host_list[i]);
			_network_host_list[i++] = str_fmt("%s:%i", item->info.hostname, item->port);
		}
		item = item->next;
	}

	for (; i < lengthof(_network_host_list); i++) {
		free(_network_host_list[i]);
		_network_host_list[i] = NULL;
	}
}

/** Non blocking connection create to actually connect to servers */
class TCPClientConnecter : TCPConnecter {
public:
	TCPClientConnecter(const NetworkAddress &address) : TCPConnecter(address) {}

	virtual void OnFailure()
	{
		NetworkError(STR_NETWORK_ERR_NOCONNECTION);
	}

	virtual void OnConnect(SOCKET s)
	{
		_networking = true;
		NetworkAllocClient(s);
		IConsoleCmdExec("exec scripts/on_client.scr 0");
		NetworkClient_Connected();
	}
};


/* Used by clients, to connect to a server */
void NetworkClientConnectGame(NetworkAddress address)
{
	if (!_network_available) return;

	if (address.GetPort() == 0) return;

	strecpy(_settings_client.network.last_host, address.GetHostname(), lastof(_settings_client.network.last_host));
	_settings_client.network.last_port = address.GetPort();

	NetworkDisconnect();
	NetworkInitialize();

	_network_join_status = NETWORK_JOIN_STATUS_CONNECTING;
	ShowJoinStatusWindow();

	new TCPClientConnecter(address);
}

static void NetworkInitGameInfo()
{
	if (StrEmpty(_settings_client.network.server_name)) {
		snprintf(_settings_client.network.server_name, sizeof(_settings_client.network.server_name), "Unnamed Server");
	}

	/* The server is a client too */
	_network_game_info.clients_on = _network_dedicated ? 0 : 1;
	_network_game_info.start_date = ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1);

	NetworkClientInfo *ci = new NetworkClientInfo(CLIENT_ID_SERVER);
	ci->client_playas = _network_dedicated ? COMPANY_SPECTATOR : _local_company;

	strecpy(ci->client_name, _settings_client.network.client_name, lastof(ci->client_name));
	strecpy(ci->unique_id, _settings_client.network.network_id, lastof(ci->unique_id));
}

bool NetworkServerStart()
{
	if (!_network_available) return false;

	/* Call the pre-scripts */
	IConsoleCmdExec("exec scripts/pre_server.scr 0");
	if (_network_dedicated) IConsoleCmdExec("exec scripts/pre_dedicated.scr 0");

	NetworkInitialize();
	if (!NetworkListen()) return false;

	/* Try to start UDP-server */
	_network_udp_server = true;
	_network_udp_server = _udp_server_socket->Listen(_network_server_bind_ip, _settings_client.network.server_port, false);

	_network_company_states = CallocT<NetworkCompanyState>(MAX_COMPANIES);
	_network_server = true;
	_networking = true;
	_frame_counter = 0;
	_frame_counter_server = 0;
	_frame_counter_max = 0;
	_last_sync_frame = 0;
	_network_own_client_id = CLIENT_ID_SERVER;

	/* Non-dedicated server will always be company #1 */
	if (!_network_dedicated) _network_playas = COMPANY_FIRST;

	_network_clients_connected = 0;

	NetworkInitGameInfo();

	/* execute server initialization script */
	IConsoleCmdExec("exec scripts/on_server.scr 0");
	/* if the server is dedicated ... add some other script */
	if (_network_dedicated) IConsoleCmdExec("exec scripts/on_dedicated.scr 0");

	_min_active_clients_paused = false;

	/* Try to register us to the master server */
	_network_last_advertise_frame = 0;
	_network_need_advertise = true;
	NetworkUDPAdvertise();
	return true;
}

/* The server is rebooting...
 * The only difference with NetworkDisconnect, is the packets that is sent */
void NetworkReboot()
{
	if (_network_server) {
		NetworkClientSocket *cs;
		FOR_ALL_CLIENT_SOCKETS(cs) {
			SEND_COMMAND(PACKET_SERVER_NEWGAME)(cs);
			cs->Send_Packets();
		}
	}

	NetworkClose();
}

/* We want to disconnect from the host/clients */
void NetworkDisconnect()
{
	if (_network_server) {
		NetworkClientSocket *cs;
		FOR_ALL_CLIENT_SOCKETS(cs) {
			SEND_COMMAND(PACKET_SERVER_SHUTDOWN)(cs);
			cs->Send_Packets();
		}
	}

	if (_settings_client.network.server_advertise) NetworkUDPRemoveAdvertise();

	DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);

	NetworkClose();
}

/* Receives something from the network */
static bool NetworkReceive()
{
	NetworkClientSocket *cs;
	int n;
	fd_set read_fd, write_fd;
	struct timeval tv;

	FD_ZERO(&read_fd);
	FD_ZERO(&write_fd);

	FOR_ALL_CLIENT_SOCKETS(cs) {
		FD_SET(cs->sock, &read_fd);
		FD_SET(cs->sock, &write_fd);
	}

	/* take care of listener port */
	if (_network_server) FD_SET(_listensocket, &read_fd);

	tv.tv_sec = tv.tv_usec = 0; // don't block at all.
#if !defined(__MORPHOS__) && !defined(__AMIGA__)
	n = select(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv);
#else
	n = WaitSelect(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv, NULL);
#endif
	if (n == -1 && !_network_server) NetworkError(STR_NETWORK_ERR_LOSTCONNECTION);

	/* accept clients.. */
	if (_network_server && FD_ISSET(_listensocket, &read_fd)) NetworkAcceptClients();

	/* read stuff from clients */
	FOR_ALL_CLIENT_SOCKETS(cs) {
		cs->writable = !!FD_ISSET(cs->sock, &write_fd);
		if (FD_ISSET(cs->sock, &read_fd)) {
			if (_network_server) {
				NetworkServer_ReadPackets(cs);
			} else {
				NetworkRecvStatus res;

				/* The client already was quiting! */
				if (cs->has_quit) return false;

				res = NetworkClient_ReadPackets(cs);
				if (res != NETWORK_RECV_STATUS_OKAY) {
					/* The client made an error of which we can not recover
					 *   close the client and drop back to main menu */
					NetworkClientError(res, cs);
					return false;
				}
			}
		}
	}
	return true;
}

/* This sends all buffered commands (if possible) */
static void NetworkSend()
{
	NetworkClientSocket *cs;
	FOR_ALL_CLIENT_SOCKETS(cs) {
		if (cs->writable) {
			cs->Send_Packets();

			if (cs->status == STATUS_MAP) {
				/* This client is in the middle of a map-send, call the function for that */
				SEND_COMMAND(PACKET_SERVER_MAP)(cs);
			}
		}
	}
}

static bool NetworkDoClientLoop()
{
	_frame_counter++;

	NetworkExecuteLocalCommandQueue();

	StateGameLoop();

	/* Check if we are in sync! */
	if (_sync_frame != 0) {
		if (_sync_frame == _frame_counter) {
#ifdef NETWORK_SEND_DOUBLE_SEED
			if (_sync_seed_1 != _random.state[0] || _sync_seed_2 != _random.state[1]) {
#else
			if (_sync_seed_1 != _random.state[0]) {
#endif
				NetworkError(STR_NETWORK_ERR_DESYNC);
				DEBUG(desync, 1, "sync_err: %d; %d\n", _date, _date_fract);
				DEBUG(net, 0, "Sync error detected!");
				NetworkClientError(NETWORK_RECV_STATUS_DESYNC, GetNetworkClientSocket(0));
				return false;
			}

			/* If this is the first time we have a sync-frame, we
			 *   need to let the server know that we are ready and at the same
			 *   frame as he is.. so we can start playing! */
			if (_network_first_time) {
				_network_first_time = false;
				SEND_COMMAND(PACKET_CLIENT_ACK)();
			}

			_sync_frame = 0;
		} else if (_sync_frame < _frame_counter) {
			DEBUG(net, 1, "Missed frame for sync-test (%d / %d)", _sync_frame, _frame_counter);
			_sync_frame = 0;
		}
	}

	return true;
}

/* We have to do some UDP checking */
void NetworkUDPGameLoop()
{
	_network_content_client.SendReceive();
	TCPConnecter::CheckCallbacks();

	if (_network_udp_server) {
		_udp_server_socket->ReceivePackets();
		_udp_master_socket->ReceivePackets();
	} else {
		_udp_client_socket->ReceivePackets();
		if (_network_udp_broadcast > 0) _network_udp_broadcast--;
		NetworkGameListRequery();
	}
}

/* The main loop called from ttd.c
 *  Here we also have to do StateGameLoop if needed! */
void NetworkGameLoop()
{
	if (!_networking) return;

	if (!NetworkReceive()) return;

	if (_network_server) {
#ifdef DEBUG_DUMP_COMMANDS
		static FILE *f = FioFOpenFile("commands.log", "rb", SAVE_DIR);
		static Date next_date = 0;
		static uint32 next_date_fract;
		static CommandPacket *cp = NULL;
		if (f == NULL && next_date == 0) {
			printf("Cannot open commands.log\n");
			next_date = 1;
		}

		while (f != NULL && !feof(f)) {
			if (cp != NULL && _date == next_date && _date_fract == next_date_fract) {
				_current_company = cp->company;
				DoCommandP(cp->tile, cp->p1, cp->p2, cp->cmd, NULL, cp->text);
				free(cp);
				cp = NULL;
			}

			if (cp != NULL) break;

			char buff[4096];
			if (fgets(buff, lengthof(buff), f) == NULL) break;
			if (strncmp(buff, "cmd: ", 8) != 0) continue;
			cp = MallocT<CommandPacket>(1);
			int company;
			sscanf(&buff[8], "%d; %d; %d; %d; %d; %d; %d; %s", &next_date, &next_date_fract, &company, &cp->tile, &cp->p1, &cp->p2, &cp->cmd, cp->text);
			cp->company = (CompanyID)company;
		}
#endif /* DEBUG_DUMP_COMMANDS */
		CheckMinActiveClients();

		bool send_frame = false;

		/* We first increase the _frame_counter */
		_frame_counter++;
		/* Update max-frame-counter */
		if (_frame_counter > _frame_counter_max) {
			_frame_counter_max = _frame_counter + _settings_client.network.frame_freq;
			send_frame = true;
		}

		NetworkExecuteLocalCommandQueue();

		/* Then we make the frame */
		StateGameLoop();

		_sync_seed_1 = _random.state[0];
#ifdef NETWORK_SEND_DOUBLE_SEED
		_sync_seed_2 = _random.state[1];
#endif

		NetworkServer_Tick(send_frame);
	} else {
		/* Client */

		/* Make sure we are at the frame were the server is (quick-frames) */
		if (_frame_counter_server > _frame_counter) {
			while (_frame_counter_server > _frame_counter) {
				if (!NetworkDoClientLoop()) break;
			}
		} else {
			/* Else, keep on going till _frame_counter_max */
			if (_frame_counter_max > _frame_counter) NetworkDoClientLoop();
		}
	}

	NetworkSend();
}

static void NetworkGenerateUniqueId()
{
	Md5 checksum;
	uint8 digest[16];
	char hex_output[16 * 2 + 1];
	char coding_string[NETWORK_NAME_LENGTH];
	int di;

	snprintf(coding_string, sizeof(coding_string), "%d%s", (uint)Random(), "OpenTTD Unique ID");

	/* Generate the MD5 hash */
	checksum.Append((const uint8*)coding_string, strlen(coding_string));
	checksum.Finish(digest);

	for (di = 0; di < 16; ++di)
		sprintf(hex_output + di * 2, "%02x", digest[di]);

	/* _network_unique_id is our id */
	snprintf(_settings_client.network.network_id, sizeof(_settings_client.network.network_id), "%s", hex_output);
}

void NetworkStartDebugLog(NetworkAddress address)
{
	extern SOCKET _debug_socket;  // Comes from debug.c
	SOCKET s;
	struct sockaddr_in sin;

	DEBUG(net, 0, "Redirecting DEBUG() to %s:%d", address.GetHostname(), address.GetPort());

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == INVALID_SOCKET) {
		DEBUG(net, 0, "Failed to open socket for redirection DEBUG()");
		return;
	}

	if (!SetNoDelay(s)) DEBUG(net, 1, "Setting TCP_NODELAY failed");

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = address.GetIP();
	sin.sin_port = htons(address.GetPort());

	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
		DEBUG(net, 0, "Failed to redirection DEBUG() to %s:%d", address.GetHostname(), address.GetPort());
		return;
	}

	if (!SetNonBlocking(s)) DEBUG(net, 0, "Setting non-blocking mode failed");
	_debug_socket = s;

	DEBUG(net, 0, "DEBUG() is now redirected");
}

/** This tries to launch the network for a given OS */
void NetworkStartUp()
{
	DEBUG(net, 3, "[core] starting network...");

	/* Network is available */
	_network_available = NetworkCoreInitialize();;
	_network_dedicated = false;
	_network_last_advertise_frame = 0;
	_network_need_advertise = true;
	_network_advertise_retries = 0;

	/* Load the ip from the openttd.cfg */
	_network_server_bind_ip = inet_addr(_settings_client.network.server_bind_ip);
	/* And put the data back in it in case it was an invalid ip */
	snprintf(_settings_client.network.server_bind_ip, sizeof(_settings_client.network.server_bind_ip), "%s", inet_ntoa(*(struct in_addr *)&_network_server_bind_ip));

	/* Generate an unique id when there is none yet */
	if (StrEmpty(_settings_client.network.network_id)) NetworkGenerateUniqueId();

	memset(&_network_game_info, 0, sizeof(_network_game_info));

	NetworkUDPInitialize();
	NetworkInitialize();
	DEBUG(net, 3, "[core] network online, multiplayer available");
	NetworkFindBroadcastIPs(_broadcast_list, MAX_INTERFACES);
}

/** This shuts the network down */
void NetworkShutDown()
{
	NetworkDisconnect();
	NetworkUDPShutdown();

	DEBUG(net, 3, "[core] shutting down network");

	_network_available = false;

	NetworkCoreShutdown();
}

/**
 * Checks whether the given version string is compatible with our version.
 * @param other the version string to compare to
 */
bool IsNetworkCompatibleVersion(const char *other)
{
	return strncmp(_openttd_revision, other, NETWORK_REVISION_LENGTH - 1) == 0;
}

#endif /* ENABLE_NETWORK */

/* NOTE: this variable needs to be always available */
CompanyID _network_playas;
