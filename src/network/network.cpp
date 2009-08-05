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
#include "../core/pool_func.hpp"
#ifdef DEBUG_DUMP_COMMANDS
	#include "../fileio_func.h"
#endif /* DEBUG_DUMP_COMMANDS */
#include "table/strings.h"

DECLARE_POSTFIX_INCREMENT(ClientID);

assert_compile(NetworkClientInfoPool::MAX_SIZE == NetworkClientSocketPool::MAX_SIZE);

NetworkClientInfoPool _networkclientinfo_pool("NetworkClientInfo");
INSTANTIATE_POOL_METHODS(NetworkClientInfo)

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
StringList _network_bind_list;
StringList _network_host_list;
StringList _network_ban_list;
uint32 _frame_counter_server; // The frame_counter of the server, if in network-mode
uint32 _frame_counter_max; // To where we may go with our clients
uint32 _frame_counter;
uint32 _last_sync_frame; // Used in the server to store the last time a sync packet was sent to clients.
NetworkAddressList _broadcast_list;
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
static SocketList _listensockets;

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
	return NetworkClientInfo::GetIfValid(index);
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
	NetworkAddress address(ip);

	if (address.GetAddressLength() == 0) return NULL;

	FOR_ALL_CLIENT_INFOS(ci) {
		if (ci->client_address == address) return ci;
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
			strid = STR_NETWORK_MESSAGE_CLIENT_COMPANY_SPECTATE;
			break;
		case NETWORK_ACTION_COMPANY_JOIN:
			colour = CC_DEFAULT;
			strid = STR_NETWORK_MESSAGE_CLIENT_COMPANY_JOIN;
			break;
		case NETWORK_ACTION_COMPANY_NEW:
			colour = CC_DEFAULT;
			strid = STR_NETWORK_MESSAGE_CLIENT_COMPANY_NEW;
			break;
		case NETWORK_ACTION_JOIN:           strid = STR_NETWORK_MESSAGE_CLIENT_JOINED; break;
		case NETWORK_ACTION_LEAVE:          strid = STR_NETWORK_MESSAGE_CLIENT_LEFT; break;
		case NETWORK_ACTION_NAME_CHANGE:    strid = STR_NETWORK_MESSAGE_NAME_CHANGE; break;
		case NETWORK_ACTION_GIVE_MONEY:     strid = self_send ? STR_NETWORK_MESSAGE_GAVE_MONEY_AWAY : STR_NETWORK_MESSAGE_GIVE_MONEY;   break;
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
	NetworkError(STR_NETWORK_ERROR_SERVER_START);
}

static void NetworkClientError(NetworkRecvStatus res, NetworkClientSocket *cs)
{
	/* First, send a CLIENT_ERROR to the server, so he knows we are
	 *  disconnection (and why!) */
	NetworkErrorCode errorno;

	/* We just want to close the connection.. */
	if (res == NETWORK_RECV_STATUS_CLOSE_QUERY) {
		cs->NetworkSocketHandler::CloseConnection();
		NetworkCloseClient(cs, true);
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
	NetworkCloseClient(cs, true);
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
		STR_NETWORK_ERROR_CLIENT_GENERAL,
		STR_NETWORK_ERROR_CLIENT_DESYNC,
		STR_NETWORK_ERROR_CLIENT_SAVEGAME,
		STR_NETWORK_ERROR_CLIENT_CONNECTION_LOST,
		STR_NETWORK_ERROR_CLIENT_PROTOCOL_ERROR,
		STR_NETWORK_ERROR_CLIENT_NEWGRF_MISMATCH,
		STR_NETWORK_ERROR_CLIENT_NOT_AUTHORIZED,
		STR_NETWORK_ERROR_CLIENT_NOT_EXPECTED,
		STR_NETWORK_ERROR_CLIENT_WRONG_REVISION,
		STR_NETWORK_ERROR_CLIENT_NAME_IN_USE,
		STR_NETWORK_ERROR_CLIENT_WRONG_PASSWORD,
		STR_NETWORK_ERROR_CLIENT_COMPANY_MISMATCH,
		STR_NETWORK_ERROR_CLIENT_KICKED,
		STR_NETWORK_ERROR_CLIENT_CHEATER,
		STR_NETWORK_ERROR_CLIENT_SERVER_FULL
	};

	if (err >= (ptrdiff_t)lengthof(network_error_strings)) err = NETWORK_ERROR_GENERAL;

	return network_error_strings[err];
}

/**
 * Counts the number of active clients connected.
 * It has to be in STATUS_ACTIVE and not a spectator
 * @return number of active clients
 */
static uint NetworkCountActiveClients()
{
	const NetworkClientSocket *cs;
	uint count = 0;

	FOR_ALL_CLIENT_SOCKETS(cs) {
		if (cs->status != STATUS_ACTIVE) continue;
		if (!Company::IsValidID(cs->GetInfo()->client_playas)) continue;
		count++;
	}

	return count;
}

/* Check if the minimum number of active clients has been reached and pause or unpause the game as appropriate */
static void CheckMinActiveClients()
{
	if (!_network_dedicated || _settings_client.network.min_active_clients == 0) return;

	if (NetworkCountActiveClients() < _settings_client.network.min_active_clients) {
		if ((_pause_mode & PM_PAUSED_NORMAL) != 0) return;

		DoCommandP(0, PM_PAUSED_NORMAL, 1, CMD_PAUSE);
		NetworkServerSendChat(NETWORK_ACTION_SERVER_MESSAGE, DESTTYPE_BROADCAST, 0, "", CLIENT_ID_SERVER, NETWORK_SERVER_MESSAGE_GAME_PAUSED_PLAYERS);
	} else {
		if ((_pause_mode & PM_PAUSED_NORMAL) == 0) return;

		DoCommandP(0, PM_PAUSED_NORMAL, 0, CMD_PAUSE);
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
	bool ipv6 = (strchr(connection_string, ':') != strrchr(connection_string, ':'));
	char *p;
	for (p = connection_string; *p != '\0'; p++) {
		switch (*p) {
			case '[':
				ipv6 = true;
				break;

			case ']':
				ipv6 = false;
				break;

			case '#':
				*company = p + 1;
				*p = '\0';
				break;

			case ':':
				if (ipv6) break;
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
void NetworkCloseClient(NetworkClientSocket *cs, bool error)
{
	/*
	 * Sending a message just before leaving the game calls cs->Send_Packets.
	 * This might invoke this function, which means that when we close the
	 * connection after cs->Send_Packets we will close an already closed
	 * connection. This handles that case gracefully without having to make
	 * that code any more complex or more aware of the validity of the socket.
	 */
	if (cs->sock == INVALID_SOCKET) return;

	if (error && !cs->HasClientQuit() && _network_server && cs->status > STATUS_INACTIVE) {
		/* We did not receive a leave message from this client... */
		char client_name[NETWORK_CLIENT_NAME_LENGTH];
		NetworkClientSocket *new_cs;

		NetworkGetClientName(client_name, sizeof(client_name), cs);

		NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, client_name, NULL, STR_NETWORK_ERROR_CLIENT_CONNECTION_LOST);

		/* Inform other clients of this... strange leaving ;) */
		FOR_ALL_CLIENT_SOCKETS(new_cs) {
			if (new_cs->status > STATUS_AUTH && cs != new_cs) {
				SEND_COMMAND(PACKET_SERVER_ERROR_QUIT)(new_cs, cs->client_id, NETWORK_ERROR_CONNECTION_LOST);
			}
		}
	}

	DEBUG(net, 1, "Closed client connection %d", cs->client_id);

	/* When the client was PRE_ACTIVE, the server was in pause mode, so unpause */
	if (cs->status == STATUS_PRE_ACTIVE && (_pause_mode & PM_PAUSED_JOIN)) {
		DoCommandP(0, PM_PAUSED_JOIN, 0, CMD_PAUSE);
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
static void NetworkAcceptClients(SOCKET ls)
{
	for (;;) {
		struct sockaddr_storage sin;
		memset(&sin, 0, sizeof(sin));
		socklen_t sin_len = sizeof(sin);
		SOCKET s = accept(ls, (struct sockaddr*)&sin, &sin_len);
		if (s == INVALID_SOCKET) return;

		SetNonBlocking(s); // XXX error handling?

		NetworkAddress address(sin, sin_len);
		DEBUG(net, 1, "Client connected from %s on frame %d", address.GetHostname(), _frame_counter);

		SetNoDelay(s); // XXX error handling?

		/* Check if the client is banned */
		bool banned = false;
		for (char **iter = _network_ban_list.Begin(); iter != _network_ban_list.End(); iter++) {
			banned = address.IsInNetmask(*iter);
			if (banned) {
				Packet p(PACKET_SERVER_BANNED);
				p.PrepareToSend();

				DEBUG(net, 1, "Banned ip tried to join (%s), refused", *iter);

				send(s, (const char*)p.buffer, p.size, 0);
				closesocket(s);
				break;
			}
		}
		/* If this client is banned, continue with next client */
		if (banned) continue;

		NetworkClientSocket *cs = NetworkAllocClient(s);
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

		cs->GetInfo()->client_address = address; // Save the IP of the client
	}
}

/* Set up the listen socket for the server */
static bool NetworkListen()
{
	assert(_listensockets.Length() == 0);

	NetworkAddressList addresses;
	GetBindAddresses(&addresses, _settings_client.network.server_port);

	for (NetworkAddress *address = addresses.Begin(); address != addresses.End(); address++) {
		address->Listen(SOCK_STREAM, &_listensockets);
	}

	if (_listensockets.Length() == 0) {
		ServerStartError("Could not create listening socket");
		return false;
	}

	return true;
}

/** Resets both pools used for network clients */
static void InitializeNetworkPools()
{
	_networkclientsocket_pool.CleanPool();
	_networkclientinfo_pool.CleanPool();
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
		NetworkCloseClient(cs, false);
	}

	if (_network_server) {
		/* We are a server, also close the listensocket */
		for (SocketList::iterator s = _listensockets.Begin(); s != _listensockets.End(); s++) {
			closesocket(s->second);
		}
		_listensockets.Clear();
		DEBUG(net, 1, "[tcp] closed listeners");
	}

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
	NetworkUDPInitialize();

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

/**
 * Get the addresses to bind to.
 * @param addresses the list to write to.
 * @param port the port to bind to.
 */
void GetBindAddresses(NetworkAddressList *addresses, uint16 port)
{
	for (char **iter = _network_bind_list.Begin(); iter != _network_bind_list.End(); iter++) {
		*addresses->Append() = NetworkAddress(*iter, port);
	}

	/* No address, so bind to everything. */
	if (addresses->Length() == 0) {
		*addresses->Append() = NetworkAddress("", port);
	}
}

/* Generates the list of manually added hosts from NetworkGameList and
 * dumps them into the array _network_host_list. This array is needed
 * by the function that generates the config file. */
void NetworkRebuildHostList()
{
	_network_host_list.Clear();

	for (NetworkGameList *item = _network_game_list; item != NULL; item = item->next) {
		if (item->manually) *_network_host_list.Append() = strdup(item->address.GetAddressAsString(false));
	}
}

/** Non blocking connection create to actually connect to servers */
class TCPClientConnecter : TCPConnecter {
public:
	TCPClientConnecter(const NetworkAddress &address) : TCPConnecter(address) {}

	virtual void OnFailure()
	{
		NetworkError(STR_NETWORK_ERROR_NOCONNECTION);
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
void NetworkClientConnectGame(NetworkAddress address, CompanyID join_as, const char *join_server_password, const char *join_company_password)
{
	if (!_network_available) return;

	if (address.GetPort() == 0) return;

	strecpy(_settings_client.network.last_host, address.GetHostname(), lastof(_settings_client.network.last_host));
	_settings_client.network.last_port = address.GetPort();
	_network_join_as = join_as;
	_network_join_server_password = join_server_password;
	_network_join_company_password = join_company_password;

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

	NetworkClientInfo *ci = new NetworkClientInfo(CLIENT_ID_SERVER);
	ci->client_playas = _network_dedicated ? COMPANY_SPECTATOR : _local_company;
	/* Give the server a valid IP; banning it is pointless anyways */
	sockaddr_in sock;
	memset(&sock, 0, sizeof(sock));
	sock.sin_family = AF_INET;
	ci->client_address = NetworkAddress((sockaddr*)&sock, sizeof(sock));

	strecpy(ci->client_name, _settings_client.network.client_name, lastof(ci->client_name));
	strecpy(ci->unique_id, _settings_client.network.network_id, lastof(ci->unique_id));
}

bool NetworkServerStart()
{
	if (!_network_available) return false;

	/* Call the pre-scripts */
	IConsoleCmdExec("exec scripts/pre_server.scr 0");
	if (_network_dedicated) IConsoleCmdExec("exec scripts/pre_dedicated.scr 0");

	NetworkDisconnect();
	NetworkInitialize();
	if (!NetworkListen()) return false;

	/* Try to start UDP-server */
	_network_udp_server = _udp_server_socket->Listen();

	_network_company_states = CallocT<NetworkCompanyState>(MAX_COMPANIES);
	_network_server = true;
	_networking = true;
	_frame_counter = 0;
	_frame_counter_server = 0;
	_frame_counter_max = 0;
	_last_sync_frame = 0;
	_network_own_client_id = CLIENT_ID_SERVER;

	_network_clients_connected = 0;

	NetworkInitGameInfo();

	/* execute server initialization script */
	IConsoleCmdExec("exec scripts/on_server.scr 0");
	/* if the server is dedicated ... add some other script */
	if (_network_dedicated) IConsoleCmdExec("exec scripts/on_dedicated.scr 0");

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

/**
 * We want to disconnect from the host/clients.
 * @param blocking whether to wait till everything has been closed
 */
void NetworkDisconnect(bool blocking)
{
	if (_network_server) {
		NetworkClientSocket *cs;
		FOR_ALL_CLIENT_SOCKETS(cs) {
			SEND_COMMAND(PACKET_SERVER_SHUTDOWN)(cs);
			cs->Send_Packets();
		}
	}

	if (_settings_client.network.server_advertise) NetworkUDPRemoveAdvertise(blocking);

	DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);

	NetworkClose();
}

/* Receives something from the network */
static bool NetworkReceive()
{
	NetworkClientSocket *cs;
	fd_set read_fd, write_fd;
	struct timeval tv;

	FD_ZERO(&read_fd);
	FD_ZERO(&write_fd);

	FOR_ALL_CLIENT_SOCKETS(cs) {
		FD_SET(cs->sock, &read_fd);
		FD_SET(cs->sock, &write_fd);
	}

	/* take care of listener port */
	for (SocketList::iterator s = _listensockets.Begin(); s != _listensockets.End(); s++) {
		FD_SET(s->second, &read_fd);
	}

	tv.tv_sec = tv.tv_usec = 0; // don't block at all.
#if !defined(__MORPHOS__) && !defined(__AMIGA__)
	int n = select(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv);
#else
	int n = WaitSelect(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv, NULL);
#endif
	if (n == -1 && !_network_server) NetworkError(STR_NETWORK_ERROR_LOSTCONNECTION);

	/* accept clients.. */
	for (SocketList::iterator s = _listensockets.Begin(); s != _listensockets.End(); s++) {
		if (FD_ISSET(s->second, &read_fd)) NetworkAcceptClients(s->second);
	}

	/* read stuff from clients */
	FOR_ALL_CLIENT_SOCKETS(cs) {
		cs->writable = !!FD_ISSET(cs->sock, &write_fd);
		if (FD_ISSET(cs->sock, &read_fd)) {
			if (_network_server) {
				NetworkServer_ReadPackets(cs);
			} else {
				NetworkRecvStatus res;

				/* The client already was quiting! */
				if (cs->HasClientQuit()) return false;

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
				NetworkError(STR_NETWORK_ERROR_DESYNC);
				DEBUG(desync, 1, "sync_err: %d; %d\n", _date, _date_fract);
				DEBUG(net, 0, "Sync error detected!");
				NetworkClientError(NETWORK_RECV_STATUS_DESYNC, NetworkClientSocket::Get(0));
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

	for (di = 0; di < 16; ++di) {
		sprintf(hex_output + di * 2, "%02x", digest[di]);
	}

	/* _network_unique_id is our id */
	snprintf(_settings_client.network.network_id, sizeof(_settings_client.network.network_id), "%s", hex_output);
}

void NetworkStartDebugLog(NetworkAddress address)
{
	extern SOCKET _debug_socket;  // Comes from debug.c

	DEBUG(net, 0, "Redirecting DEBUG() to %s:%d", address.GetHostname(), address.GetPort());

	SOCKET s = address.Connect();
	if (s == INVALID_SOCKET) {
		DEBUG(net, 0, "Failed to open socket for redirection DEBUG()");
		return;
	}

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

	/* Generate an unique id when there is none yet */
	if (StrEmpty(_settings_client.network.network_id)) NetworkGenerateUniqueId();

	memset(&_network_game_info, 0, sizeof(_network_game_info));

	NetworkInitialize();
	DEBUG(net, 3, "[core] network online, multiplayer available");
	NetworkFindBroadcastIPs(&_broadcast_list);
}

/** This shuts the network down */
void NetworkShutDown()
{
	NetworkDisconnect(true);
	NetworkUDPClose();

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
