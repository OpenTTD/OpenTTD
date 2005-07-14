#include "stdafx.h"
#include "debug.h"
#include "string.h"
#include "strings.h"
#include "map.h"
#include "network_data.h"
#include "command.h"

#if defined(WITH_REV)
	extern const char _openttd_revision[];
#elif defined(WITH_REV_HACK)
	#define WITH_REV
	const char _openttd_revision[] = WITH_REV_HACK;
#else
	const char _openttd_revision[] = NOREV_STRING;
#endif


#ifdef ENABLE_NETWORK

#include "table/strings.h"
#include "network_client.h"
#include "network_server.h"
#include "network_udp.h"
#include "network_gamelist.h"
#include "console.h" /* IConsoleCmdExec */
#include <stdarg.h> /* va_list */
#include "md5.h"

#ifdef __MORPHOS__
// the library base is required here
struct Library *SocketBase = NULL;
#endif

// The listen socket for the server
static SOCKET _listensocket;

// Network copy of patches, so the patches of a client are not fucked up
//  after he joined a server
static Patches network_tmp_patches;

// The amount of clients connected
static byte _network_clients_connected = 0;
// The index counter for new clients (is never decreased)
static uint16 _network_client_index = NETWORK_SERVER_INDEX + 1;

/* Some externs / forwards */
extern void ShowJoinStatusWindow(void);
extern void StateGameLoop(void);

// Function that looks up the CI for a given client-index
NetworkClientInfo *NetworkFindClientInfoFromIndex(uint16 client_index)
{
	NetworkClientInfo *ci;

	for (ci = _network_client_info; ci != &_network_client_info[MAX_CLIENT_INFO]; ci++)
		if (ci->client_index == client_index)
			return ci;

	return NULL;
}

// Function that looks up the CS for a given client-index
NetworkClientState *NetworkFindClientStateFromIndex(uint16 client_index)
{
	NetworkClientState *cs;

	for (cs = _clients; cs != &_clients[MAX_CLIENT_INFO]; cs++)
		if (cs->index == client_index)
			return cs;

	return NULL;
}

// NetworkGetClientName is a server-safe function to get the name of the client
//  if the user did not send it yet, Client #<no> is used.
void NetworkGetClientName(char *client_name, size_t size, const NetworkClientState *cs)
{
	NetworkClientInfo *ci = DEREF_CLIENT_INFO(cs);
	if (ci->client_name[0] == '\0')
		snprintf(client_name, size, "Client #%d", cs->index);
	else
		snprintf(client_name, size, "%s", ci->client_name);
}

// This puts a text-message to the console, or in the future, the chat-box,
//  (to keep it all a bit more general)
// If 'self_send' is true, this is the client who is sending the message
void CDECL NetworkTextMessage(NetworkAction action, uint16 color, bool self_send, const char *name, const char *str, ...)
{
	char buf[1024];
	va_list va;
	const int duration = 10; // Game days the messages stay visible
	char message[1024];
	char temp[1024];

	va_start(va, str);
	vsprintf(buf, str, va);
	va_end(va);

	switch (action) {
		case NETWORK_ACTION_JOIN:
			GetString(temp, STR_NETWORK_CLIENT_JOINED);
			snprintf(message, sizeof(message), "*** %s %s", name, temp);
			break;
		case NETWORK_ACTION_LEAVE:
			GetString(temp, STR_NETWORK_ERR_LEFT);
			snprintf(message, sizeof(message), "*** %s %s (%s)", name, temp, buf);
			break;
		case NETWORK_ACTION_GIVE_MONEY:
			if (self_send) {
				SetDParamStr(0, name);
				SetDParam(1, atoi(buf));
				GetString(temp, STR_NETWORK_GAVE_MONEY_AWAY);
				snprintf(message, sizeof(message), "*** %s", temp);
			} else {
				SetDParam(0, atoi(buf));
				GetString(temp, STR_NETWORK_GIVE_MONEY);
				snprintf(message, sizeof(message), "*** %s %s", name, temp);
			}
			break;
		case NETWORK_ACTION_CHAT_PLAYER:
			if (self_send) {
				SetDParamStr(0, name);
				GetString(temp, STR_NETWORK_CHAT_TO_COMPANY);
				snprintf(message, sizeof(message), "%s %s", temp, buf);
			} else {
				SetDParamStr(0, name);
				GetString(temp, STR_NETWORK_CHAT_COMPANY);
				snprintf(message, sizeof(message), "%s %s", temp, buf);
			}
			break;
		case NETWORK_ACTION_CHAT_CLIENT:
			if (self_send) {
				SetDParamStr(0, name);
				GetString(temp, STR_NETWORK_CHAT_TO_CLIENT);
				snprintf(message, sizeof(message), "%s %s", temp, buf);
			} else {
				SetDParamStr(0, name);
				GetString(temp, STR_NETWORK_CHAT_CLIENT);
				snprintf(message, sizeof(message), "%s %s", temp, buf);
			}
			break;
		case NETWORK_ACTION_NAME_CHANGE:
			GetString(temp, STR_NETWORK_NAME_CHANGE);
			snprintf(message, sizeof(message), "*** %s %s %s", name, temp, buf);
			break;
		default:
			SetDParamStr(0, name);
			GetString(temp, STR_NETWORK_CHAT_ALL);
			snprintf(message, sizeof(message), "%s %s", temp, buf);
			break;
	}

	IConsolePrintF(color, "%s", message);
	AddTextMessage(color, duration, "%s", message);
}

// Calculate the frame-lag of a client
uint NetworkCalculateLag(const NetworkClientState *cs)
{
	int lag = cs->last_frame_server - cs->last_frame;
	// This client has missed his ACK packet after 1 DAY_TICKS..
	//  so we increase his lag for every frame that passes!
	// The packet can be out by a max of _net_frame_freq
	if (cs->last_frame_server + DAY_TICKS + _network_frame_freq < _frame_counter)
		lag += _frame_counter - (cs->last_frame_server + DAY_TICKS + _network_frame_freq);

	return lag;
}


// There was a non-recoverable error, drop back to the main menu with a nice
//  error
static void NetworkError(StringID error_string)
{
	_switch_mode = SM_MENU;
	_switch_mode_errorstr = error_string;
}

static void ClientStartError(const char *error)
{
	DEBUG(net, 0)("[NET] Client could not start network: %s",error);
	NetworkError(STR_NETWORK_ERR_CLIENT_START);
}

static void ServerStartError(const char *error)
{
	DEBUG(net, 0)("[NET] Server could not start network: %s",error);
	NetworkError(STR_NETWORK_ERR_SERVER_START);
}

static void NetworkClientError(byte res, NetworkClientState *cs) {
	// First, send a CLIENT_ERROR to the server, so he knows we are
	//  disconnection (and why!)
	NetworkErrorCode errorno;

	// We just want to close the connection..
	if (res == NETWORK_RECV_STATUS_CLOSE_QUERY) {
		cs->quited = true;
		NetworkCloseClient(cs);
		_networking = false;

		DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
		return;
	}

	switch(res) {
		case NETWORK_RECV_STATUS_DESYNC: errorno = NETWORK_ERROR_DESYNC; break;
		case NETWORK_RECV_STATUS_SAVEGAME: errorno = NETWORK_ERROR_SAVEGAME_FAILED; break;
		default: errorno = NETWORK_ERROR_GENERAL;
	}
	// This means we fucked up and the server closed the connection
	if (res != NETWORK_RECV_STATUS_SERVER_ERROR && res != NETWORK_RECV_STATUS_SERVER_FULL &&
			res != NETWORK_RECV_STATUS_SERVER_BANNED) {
		SEND_COMMAND(PACKET_CLIENT_ERROR)(errorno);

		// Dequeue all commands before closing the socket
		NetworkSend_Packets(DEREF_CLIENT(0));
	}

	_switch_mode = SM_MENU;
	NetworkCloseClient(cs);
	_networking = false;
}

// Find all IP-aliases for this host
static void NetworkFindIPs(void)
{
	int i, last;

#if defined(BEOS_NET_SERVER) /* doesn't have neither getifaddrs or net/if.h */
	/* Based on Andrew Bachmann's netstat+.c. Big thanks to him! */
	int _netstat(int fd, char **output, int verbose);

	int seek_past_header(char **pos, const char *header) {
		char *new_pos = strstr(*pos, header);
		if (new_pos == 0) {
			return B_ERROR;
		}
		*pos += strlen(header) + new_pos - *pos + 1;
		return B_OK;
	}

	int output_length;
	char *output_pointer = NULL;
	char **output;
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	i = 0;

	// If something fails, make sure the list is empty
	_network_ip_list[0] = 0;

	if (sock < 0) {
		DEBUG(net, 0)("Error creating socket!");
		return;
	}

	output_length = _netstat(sock, &output_pointer, 1);
	if (output_length < 0) {
		DEBUG(net, 0)("Error running _netstat!");
		return;
	}

	output = &output_pointer;
	if (seek_past_header(output, "IP Interfaces:") == B_OK) {
		for (;;) {
			uint32 n, fields, read;
			uint8 i1, i2, i3, i4, j1, j2, j3, j4;
			struct in_addr inaddr;
			fields = sscanf(*output, "%u: %hhu.%hhu.%hhu.%hhu, netmask %hhu.%hhu.%hhu.%hhu%n",
												&n, &i1,&i2,&i3,&i4, &j1,&j2,&j3,&j4, &read);
			read += 1;
			if (fields != 9) {
				break;
			}
			inaddr.s_addr = htonl((uint32)i1 << 24 | (uint32)i2 << 16 | (uint32)i3 << 8 | (uint32)i4);
			if (inaddr.s_addr != 0) {
				_network_ip_list[i] = inaddr.s_addr;
				i++;
			}
			if (read < 0) {
				break;
			}
			*output += read;
		}
		/* XXX - Using either one of these crashes openttd heavily? - wber */
		/*free(output_pointer);*/
		/*free(output);*/
		closesocket(sock);
	}
#elif defined(HAVE_GETIFADDRS)
	struct ifaddrs *ifap, *ifa;

	// If something fails, make sure the list is empty
	_network_ip_list[0] = 0;

	if (getifaddrs(&ifap) != 0)
		return;

	i = 0;
	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
			continue;
		_network_ip_list[i] = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
		i++;
	}
	freeifaddrs(ifap);

#else /* not HAVE_GETIFADDRS */

	unsigned long len = 0;
	SOCKET sock;
	IFREQ ifo[MAX_INTERFACES];

#ifndef WIN32
	struct ifconf if_conf;
#endif

	// If something fails, make sure the list is empty
	_network_ip_list[0] = 0;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) return;

#ifdef WIN32
	// On windows it is easy
	memset(&ifo[0], 0, sizeof(ifo));
	if ((WSAIoctl(sock, SIO_GET_INTERFACE_LIST, NULL, 0, &ifo[0], sizeof(ifo), &len, NULL, NULL)) != 0) {
		closesocket(sock);
		return;
	}
#else
	// On linux a bit harder
	if_conf.ifc_len = (sizeof (struct ifreq)) * MAX_INTERFACES;
	if_conf.ifc_buf = (char *)&ifo[0];
	if ((ioctl(sock, SIOCGIFCONF, &if_conf)) == -1) {
		closesocket(sock);
		return;
	}
	len = if_conf.ifc_len;
#endif /* WIN32 */

	// Now walk through all IPs and list them
	for (i = 0; i < (int)(len / sizeof(IFREQ)); i++) {
		// Request IP for this interface
#ifdef WIN32
		_network_ip_list[i] = *(&ifo[i].iiAddress.AddressIn.sin_addr.s_addr);
#else
		if ((ioctl(sock, SIOCGIFADDR, &ifo[i])) != 0) {
			closesocket(sock);
			return;
		}

		_network_ip_list[i] = ((struct sockaddr_in *)&ifo[i].ifr_addr)->sin_addr.s_addr;
#endif
	}

	closesocket(sock);

#endif /* not HAVE_GETIFADDRS */

	_network_ip_list[i] = 0;
	last = i - 1;

	DEBUG(net, 3)("Detected IPs:");
	// Now display to the debug all the detected ips
	i = 0;
	while (_network_ip_list[i] != 0) {
		// Also check for non-used ips (127.0.0.1)
		if (_network_ip_list[i] == inet_addr("127.0.0.1")) {
			// If there is an ip after thisone, put him in here
			if (last > i)
				_network_ip_list[i] = _network_ip_list[last];
			// Clear the last ip
			_network_ip_list[last] = 0;
			// And we have 1 ip less
			last--;
			continue;
		}

		DEBUG(net, 3)(" %d) %s", i, inet_ntoa(*(struct in_addr *)&_network_ip_list[i]));//inet_ntoa(inaddr));
		i++;
	}
}

// Resolve a hostname to a inet_addr
unsigned long NetworkResolveHost(const char *hostname)
{
	in_addr_t ip;

	// First try: is it an ip address?
	ip = inet_addr(hostname);

	// If not try to resolve the name
	if (ip == INADDR_NONE) {
		struct hostent *he = gethostbyname(hostname);
		if (he == NULL) {
			DEBUG(net, 0) ("[NET] Cannot resolve %s", hostname);
		} else {
			struct in_addr addr = *(struct in_addr *)he->h_addr_list[0];
			DEBUG(net, 1) ("[NET] Resolved %s to %s", hostname, inet_ntoa(addr));
			ip = addr.s_addr;
		}
	}
	return ip;
}

// Converts a string to ip/port/player
//  Format: IP#player:port
//
// connection_string will be re-terminated to seperate out the hostname, and player and port will
// be set to the player and port strings given by the user, inside the memory area originally
// occupied by connection_string.
void ParseConnectionString(const char **player, const char **port, char *connection_string)
{
	char *p;
	for (p = connection_string; *p != '\0'; p++) {
		if (*p == '#') {
			*player = p + 1;
			*p = '\0';
		} else if (*p == ':') {
			*port = p + 1;
			*p = '\0';
		}
	}
}

// Creates a new client from a socket
//   Used both by the server and the client
static NetworkClientState *NetworkAllocClient(SOCKET s)
{
	NetworkClientState *cs;
	NetworkClientInfo *ci;
	byte client_no;

	client_no = 0;

	if (_network_server) {
		// Can we handle a new client?
		if (_network_clients_connected >= MAX_CLIENTS)
			return NULL;

		if (_network_game_info.clients_on >= _network_game_info.clients_max)
			return NULL;

		// Register the login
		client_no = _network_clients_connected++;
	}

	cs = &_clients[client_no];
	memset(cs, 0, sizeof(*cs));
	cs->socket = s;
	cs->last_frame = 0;
	cs->quited = false;

	cs->last_frame = _frame_counter;
	cs->last_frame_server = _frame_counter;

	if (_network_server) {
		ci = DEREF_CLIENT_INFO(cs);
		memset(ci, 0, sizeof(*ci));

		cs->index = _network_client_index++;
		ci->client_index = cs->index;
		ci->join_date = _date;

		InvalidateWindow(WC_CLIENT_LIST, 0);
	}

	return cs;
}

// Close a connection
void NetworkCloseClient(NetworkClientState *cs)
{
	NetworkClientInfo *ci;
	// Socket is already dead
	if (cs->socket == INVALID_SOCKET) {
		cs->quited = true;
		return;
	}

	DEBUG(net, 1) ("[NET] Closed client connection");

	if (!cs->quited && _network_server && cs->status > STATUS_INACTIVE) {
		// We did not receive a leave message from this client...
		NetworkErrorCode errorno = NETWORK_ERROR_CONNECTION_LOST;
		char str[100];
		char client_name[NETWORK_NAME_LENGTH];
		NetworkClientState *new_cs;

		NetworkGetClientName(client_name, sizeof(client_name), cs);

		GetString(str, STR_NETWORK_ERR_CLIENT_GENERAL + errorno);

		NetworkTextMessage(NETWORK_ACTION_LEAVE, 1, false, client_name, str);

		// Inform other clients of this... strange leaving ;)
		FOR_ALL_CLIENTS(new_cs) {
			if (new_cs->status > STATUS_AUTH && cs != new_cs) {
				SEND_COMMAND(PACKET_SERVER_ERROR_QUIT)(new_cs, cs->index, errorno);
			}
		}
	}

	/* When the client was PRE_ACTIVE, the server was in pause mode, so unpause */
	if (cs->status == STATUS_PRE_ACTIVE && _network_pause_on_join) {
		DoCommandP(0, 0, 0, NULL, CMD_PAUSE);
		NetworkServer_HandleChat(NETWORK_ACTION_CHAT, DESTTYPE_BROADCAST, 0, "Game unpaused", NETWORK_SERVER_INDEX);
	}

	closesocket(cs->socket);
	cs->writable = false;
	cs->quited = true;

	// Free all pending and partially received packets
	while (cs->packet_queue != NULL) {
		Packet *p = cs->packet_queue->next;
		free(cs->packet_queue);
		cs->packet_queue = p;
	}
	free(cs->packet_recv);
	cs->packet_recv = NULL;

	while (cs->command_queue != NULL) {
		CommandPacket *p = cs->command_queue->next;
		free(cs->command_queue);
		cs->command_queue = p;
	}

	// Close the gap in the client-list
	ci = DEREF_CLIENT_INFO(cs);

	if (_network_server) {
		// We just lost one client :(
		if (cs->status > STATUS_INACTIVE)
			_network_game_info.clients_on--;
		_network_clients_connected--;

		while ((cs + 1) != DEREF_CLIENT(MAX_CLIENTS) && (cs + 1)->socket != INVALID_SOCKET) {
			*cs = *(cs + 1);
			*ci = *(ci + 1);
			cs++;
			ci++;
		}

		InvalidateWindow(WC_CLIENT_LIST, 0);
	}

	// Reset the status of the last socket
	cs->socket = INVALID_SOCKET;
	cs->status = STATUS_INACTIVE;
	cs->index = NETWORK_EMPTY_INDEX;
	ci->client_index = NETWORK_EMPTY_INDEX;
}

// A client wants to connect to a server
static bool NetworkConnect(const char *hostname, int port)
{
	SOCKET s;
	struct sockaddr_in sin;

	DEBUG(net, 1) ("[NET] Connecting to %s %d", hostname, port);

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == INVALID_SOCKET) {
		ClientStartError("socket() failed");
		return false;
	}

	if (!SetNoDelay(s))
		DEBUG(net, 1)("[NET] Setting TCP_NODELAY failed");

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = NetworkResolveHost(hostname);
	sin.sin_port = htons(port);
	_network_last_host_ip = sin.sin_addr.s_addr;

	if (connect(s, (struct sockaddr*) &sin, sizeof(sin)) != 0) {
		// We failed to connect for which reason what so ever
		return false;
	}

	if (!SetNonBlocking(s))
		DEBUG(net, 0)("[NET] Setting non-blocking failed"); // XXX should this be an error?

	// in client mode, only the first client field is used. it's pointing to the server.
	NetworkAllocClient(s);

	ShowJoinStatusWindow();

	memcpy(&network_tmp_patches, &_patches, sizeof(_patches));

	return true;
}

// For the server, to accept new clients
static void NetworkAcceptClients(void)
{
	struct sockaddr_in sin;
	SOCKET s;
	NetworkClientState *cs;
	uint i;
	bool banned;

	// Should never ever happen.. is it possible??
	assert(_listensocket != INVALID_SOCKET);

	for (;;) {
		socklen_t sin_len;

		sin_len = sizeof(sin);
		s = accept(_listensocket, (struct sockaddr*)&sin, &sin_len);
		if (s == INVALID_SOCKET) return;

		SetNonBlocking(s); // XXX error handling?

		DEBUG(net, 1) ("[NET] Client connected from %s on frame %d", inet_ntoa(sin.sin_addr), _frame_counter);

		SetNoDelay(s); // XXX error handling?

		/* Check if the client is banned */
		banned = false;
		for (i = 0; i < lengthof(_network_ban_list); i++) {
			if (_network_ban_list[i] == NULL)
				continue;

			if (sin.sin_addr.s_addr == inet_addr(_network_ban_list[i])) {
				Packet *p = NetworkSend_Init(PACKET_SERVER_BANNED);

				DEBUG(net, 1)("[NET] Banned ip tried to join (%s), refused", _network_ban_list[i]);

				p->buffer[0] = p->size & 0xFF;
				p->buffer[1] = p->size >> 8;

				send(s, p->buffer, p->size, 0);
				closesocket(s);

				free(p);

				banned = true;
				break;
			}
		}
		/* If this client is banned, continue with next client */
		if (banned)
			continue;

		cs = NetworkAllocClient(s);
		if (cs == NULL) {
			// no more clients allowed?
			// Send to the client that we are full!
			Packet *p = NetworkSend_Init(PACKET_SERVER_FULL);

			p->buffer[0] = p->size & 0xFF;
			p->buffer[1] = p->size >> 8;

			send(s, p->buffer, p->size, 0);
			closesocket(s);

			free(p);

			continue;
		}

		// a new client has connected. We set him at inactive for now
		//  maybe he is only requesting server-info. Till he has sent a PACKET_CLIENT_MAP_OK
		//  the client stays inactive
		cs->status = STATUS_INACTIVE;

		{
			// Save the IP of the client
			NetworkClientInfo *ci;
			ci = DEREF_CLIENT_INFO(cs);
			ci->client_ip = sin.sin_addr.s_addr;
		}
	}
}

// Set up the listen socket for the server
static bool NetworkListen(void)
{
	SOCKET ls;
	struct sockaddr_in sin;
	int port;

	port = _network_server_port;

	DEBUG(net, 1) ("[NET] Listening on %s:%d", _network_server_bind_ip_host, port);

	ls = socket(AF_INET, SOCK_STREAM, 0);
	if (ls == INVALID_SOCKET) {
		ServerStartError("socket() on listen socket failed");
		return false;
	}

	{ // reuse the socket
		int reuse = 1;
		// The (const char*) cast is needed for windows!!
		if (setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) == -1) {
			ServerStartError("setsockopt() on listen socket failed");
			return false;
		}
	}

	if (!SetNonBlocking(ls))
		DEBUG(net, 0)("[NET] Setting non-blocking failed"); // XXX should this be an error?

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = _network_server_bind_ip;
	sin.sin_port = htons(port);

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

// Close all current connections
static void NetworkClose(void)
{
	NetworkClientState *cs;

	FOR_ALL_CLIENTS(cs) {
		if (!_network_server) {
			SEND_COMMAND(PACKET_CLIENT_QUIT)("leaving");
			NetworkSend_Packets(cs);
		}
		NetworkCloseClient(cs);
	}

	if (_network_server) {
		// We are a server, also close the listensocket
		closesocket(_listensocket);
		_listensocket = INVALID_SOCKET;
		DEBUG(net, 1) ("[NET] Closed listener");
		NetworkUDPClose();
	}
}

// Inits the network (cleans sockets and stuff)
static void NetworkInitialize(void)
{
	NetworkClientState *cs;

	_local_command_queue = NULL;

	// Clean all client-sockets
	memset(_clients, 0, sizeof(_clients));
	for (cs = _clients; cs != &_clients[MAX_CLIENTS]; cs++) {
		cs->socket = INVALID_SOCKET;
		cs->status = STATUS_INACTIVE;
		cs->command_queue = NULL;
	}

	// Clean the client_info memory
	memset(_network_client_info, 0, sizeof(_network_client_info));
	memset(_network_player_info, 0, sizeof(_network_player_info));
	_network_lobby_company_count = 0;

	_sync_frame = 0;
	_network_first_time = true;

	_network_reconnect = 0;

	InitPlayerRandoms();

	NetworkUDPInitialize();
}

// Query a server to fetch his game-info
//  If game_info is true, only the gameinfo is fetched,
//   else only the client_info is fetched
NetworkGameList *NetworkQueryServer(const char* host, unsigned short port, bool game_info)
{
	if (!_network_available) return NULL;

	NetworkDisconnect();

	if (game_info) {
		return NetworkUDPQueryServer(host, port);
	}

	NetworkInitialize();

	_network_server = false;

	// Try to connect
	_networking = NetworkConnect(host, port);

//	ttd_strlcpy(_network_last_host, host, sizeof(_network_last_host));
//	_network_last_port = port;

	// We are connected
	if (_networking) {
		SEND_COMMAND(PACKET_CLIENT_COMPANY_INFO)();
		return NULL;
	}

	// No networking, close everything down again
	NetworkDisconnect();
	return NULL;
}

/* Validates an address entered as a string and adds the server to
 * the list. If you use this functions, the games will be marked
 * as manually added. */
void NetworkAddServer(const char *b)
{
	if (*b != '\0') {
		NetworkGameList *item;
		const char *port = NULL;
		const char *player = NULL;
		char host[NETWORK_HOSTNAME_LENGTH];
		uint16 rport;

		ttd_strlcpy(host, b, lengthof(host));

		ttd_strlcpy(_network_default_ip, b, lengthof(_network_default_ip));
		rport = NETWORK_DEFAULT_PORT;

		ParseConnectionString(&player, &port, host);

		if (player != NULL) _network_playas = atoi(player);
		if (port != NULL) rport = atoi(port);

		item = NetworkQueryServer(host, rport, true);
		item->manually = true;
	}
}

/* Generates the list of manually added hosts from NetworkGameList and
 * dumps them into the array _network_host_list. This array is needed
 * by the function that generates the config file. */
void NetworkRebuildHostList(void)
{
	uint i = 0;
	NetworkGameList *item = _network_game_list;
	while (item != NULL && i != lengthof(_network_host_list)) {
		if (item->manually)
			_network_host_list[i++] = str_fmt("%s:%i", item->info.hostname, item->port);
		item = item->next;
	}

	for (; i < lengthof(_network_host_list); i++) {
		_network_host_list[i] = strdup("");
	}
}

// Used by clients, to connect to a server
bool NetworkClientConnectGame(const char* host, unsigned short port)
{
	if (!_network_available) return false;

	if (port == 0) return false;

	ttd_strlcpy(_network_last_host, host, sizeof(_network_last_host));
	_network_last_port = port;

	NetworkDisconnect();
	NetworkUDPClose();
	NetworkInitialize();

	// Try to connect
	_networking = NetworkConnect(host, port);

	// We are connected
	if (_networking) {
		IConsoleCmdExec("exec scripts/on_client.scr 0");
		NetworkClient_Connected();
	} else {
		// Connecting failed
		NetworkError(STR_NETWORK_ERR_NOCONNECTION);
	}

	return _networking;
}

static void NetworkInitGameInfo(void)
{
	NetworkClientInfo *ci;

	ttd_strlcpy(_network_game_info.server_name, _network_server_name, sizeof(_network_game_info.server_name));
	ttd_strlcpy(_network_game_info.server_password, _network_server_password, sizeof(_network_server_password));
	ttd_strlcpy(_network_game_info.rcon_password, _network_rcon_password, sizeof(_network_rcon_password));
	if (_network_game_info.server_name[0] == '\0')
		snprintf(_network_game_info.server_name, sizeof(_network_game_info.server_name), "Unnamed Server");

	// The server is a client too ;)
	if (_network_dedicated) {
		_network_game_info.clients_on = 0;
		_network_game_info.dedicated = true;
	} else {
		_network_game_info.clients_on = 1;
		_network_game_info.dedicated = false;
	}
	ttd_strlcpy(_network_game_info.server_revision, _openttd_revision, sizeof(_network_game_info.server_revision));
	_network_game_info.spectators_on = 0;
	_network_game_info.game_date = _date;
	_network_game_info.start_date = ConvertIntDate(_patches.starting_date);
	_network_game_info.map_width = MapSizeX();
	_network_game_info.map_height = MapSizeY();
	_network_game_info.map_set = _opt.landscape;

	_network_game_info.use_password = (_network_server_password[0] != '\0');

	// We use _network_client_info[MAX_CLIENT_INFO - 1] to store the server-data in it
	//  The index is NETWORK_SERVER_INDEX ( = 1)
	ci = &_network_client_info[MAX_CLIENT_INFO - 1];
	memset(ci, 0, sizeof(*ci));

	ci->client_index = NETWORK_SERVER_INDEX;
	if (_network_dedicated)
		ci->client_playas = OWNER_SPECTATOR;
	else
		ci->client_playas = _local_player + 1;
	ttd_strlcpy(ci->client_name, _network_player_name, sizeof(ci->client_name));
	ttd_strlcpy(ci->unique_id, _network_unique_id, sizeof(ci->unique_id));
}

bool NetworkServerStart(void)
{
	if (!_network_available) return false;

	/* Call the pre-scripts */
	IConsoleCmdExec("exec scripts/pre_server.scr 0");
	if (_network_dedicated) IConsoleCmdExec("exec scripts/pre_dedicated.scr 0");

	NetworkInitialize();
	if (!NetworkListen())
		return false;

	// Try to start UDP-server
	_network_udp_server = true;
	_network_udp_server = NetworkUDPListen(&_udp_server_socket, _network_server_bind_ip, _network_server_port, false);

	_network_server = true;
	_networking = true;
	_frame_counter = 0;
	_frame_counter_server = 0;
	_frame_counter_max = 0;
	_network_own_client_index = NETWORK_SERVER_INDEX;

	if (!_network_dedicated)
		_network_playas = 1;

	_network_clients_connected = 0;

	NetworkInitGameInfo();

	// execute server initialization script
	IConsoleCmdExec("exec scripts/on_server.scr 0");
	// if the server is dedicated ... add some other script
	if (_network_dedicated) IConsoleCmdExec("exec scripts/on_dedicated.scr 0");

	/* Try to register us to the master server */
	_network_last_advertise_date = 0;
	NetworkUDPAdvertise();
	return true;
}

// The server is rebooting...
// The only difference with NetworkDisconnect, is the packets that is sent
void NetworkReboot(void)
{
	if (_network_server) {
		NetworkClientState *cs;
		FOR_ALL_CLIENTS(cs) {
			SEND_COMMAND(PACKET_SERVER_NEWGAME)(cs);
			NetworkSend_Packets(cs);
		}
	}

	NetworkClose();

	// Free all queued commands
	while (_local_command_queue != NULL) {
		CommandPacket *p = _local_command_queue;
		_local_command_queue = _local_command_queue->next;
		free(p);
	}

	_networking = false;
	_network_server = false;
}

// We want to disconnect from the host/clients
void NetworkDisconnect(void)
{
	if (_network_server) {
		NetworkClientState *cs;
		FOR_ALL_CLIENTS(cs) {
			SEND_COMMAND(PACKET_SERVER_SHUTDOWN)(cs);
			NetworkSend_Packets(cs);
		}
	}

	if (_network_advertise)
		NetworkUDPRemoveAdvertise();

	DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);

	NetworkClose();

	// Free all queued commands
	while (_local_command_queue != NULL) {
		CommandPacket *p = _local_command_queue;
		_local_command_queue = _local_command_queue->next;
		free(p);
	}

	if (_networking && !_network_server) {
		memcpy(&_patches, &network_tmp_patches, sizeof(_patches));
	}

	_networking = false;
	_network_server = false;
}

// Receives something from the network
static bool NetworkReceive(void)
{
	NetworkClientState *cs;
	int n;
	fd_set read_fd, write_fd;
	struct timeval tv;

	FD_ZERO(&read_fd);
	FD_ZERO(&write_fd);

	FOR_ALL_CLIENTS(cs) {
		FD_SET(cs->socket, &read_fd);
		FD_SET(cs->socket, &write_fd);
	}

	// take care of listener port
	if (_network_server) {
		FD_SET(_listensocket, &read_fd);
	}

	tv.tv_sec = tv.tv_usec = 0; // don't block at all.
#if !defined(__MORPHOS__) && !defined(__AMIGA__)
	n = select(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv);
#else
	n = WaitSelect(FD_SETSIZE, &read_fd, &write_fd, NULL, &tv, NULL);
#endif
	if (n == -1 && !_network_server) NetworkError(STR_NETWORK_ERR_LOSTCONNECTION);

	// accept clients..
	if (_network_server && FD_ISSET(_listensocket, &read_fd))
		NetworkAcceptClients();

	// read stuff from clients
	FOR_ALL_CLIENTS(cs) {
		cs->writable = !!FD_ISSET(cs->socket, &write_fd);
		if (FD_ISSET(cs->socket, &read_fd)) {
			if (_network_server)
				NetworkServer_ReadPackets(cs);
			else {
				byte res;
				// The client already was quiting!
				if (cs->quited) return false;
				if ((res = NetworkClient_ReadPackets(cs)) != NETWORK_RECV_STATUS_OKAY) {
					// The client made an error of which we can not recover
					//   close the client and drop back to main menu

					NetworkClientError(res, cs);
					return false;
				}
			}
		}
	}
	return true;
}

// This sends all buffered commands (if possible)
static void NetworkSend(void)
{
	NetworkClientState *cs;
	FOR_ALL_CLIENTS(cs) {
		if (cs->writable) {
			NetworkSend_Packets(cs);

			if (cs->status == STATUS_MAP) {
				// This client is in the middle of a map-send, call the function for that
				SEND_COMMAND(PACKET_SERVER_MAP)(cs);
			}
		}
	}
}

// Handle the local-command-queue
static void NetworkHandleLocalQueue(void)
{
	if (_local_command_queue != NULL) {
		CommandPacket *cp;
		CommandPacket *cp_prev;

		cp = _local_command_queue;
		cp_prev = NULL;

		while (cp != NULL) {
			if (_frame_counter > cp->frame) {
				// We can execute this command
				NetworkExecuteCommand(cp);

				if (cp_prev != NULL) {
					cp_prev->next = cp->next;
					free(cp);
					cp = cp_prev->next;
				} else {
					// This means we are at our first packet
					_local_command_queue = cp->next;
					free(cp);
					cp = _local_command_queue;
				}

			} else {
				// Command is in the future, skip to next
				//  (commands don't have to be in order in the queue!!)
				cp_prev = cp;
				cp = cp->next;
			}
		}
	}
}

static bool NetworkDoClientLoop(void)
{
	_frame_counter++;

	NetworkHandleLocalQueue();

	StateGameLoop();

	// Check if we are in sync!
	if (_sync_frame != 0) {
		if (_sync_frame == _frame_counter) {
#ifdef NETWORK_SEND_DOUBLE_SEED
			if (_sync_seed_1 != _random_seeds[0][0] || _sync_seed_2 != _random_seeds[0][1]) {
#else
			if (_sync_seed_1 != _random_seeds[0][0]) {
#endif
				NetworkError(STR_NETWORK_ERR_DESYNC);
				DEBUG(net, 0)("[NET] Sync error detected!");
				NetworkClientError(NETWORK_RECV_STATUS_DESYNC, DEREF_CLIENT(0));
				return false;
			}

			// If this is the first time we have a sync-frame, we
			//   need to let the server know that we are ready and at the same
			//   frame as he is.. so we can start playing!
			if (_network_first_time) {
				_network_first_time = false;
				SEND_COMMAND(PACKET_CLIENT_ACK)();
			}

			_sync_frame = 0;
		} else if (_sync_frame < _frame_counter) {
			DEBUG(net, 1)("[NET] Missed frame for sync-test (%d / %d)", _sync_frame, _frame_counter);
			_sync_frame = 0;
		}
	}

	return true;
}

// We have to do some UDP checking
void NetworkUDPGameLoop(void)
{
	if (_network_udp_server) {
		NetworkUDPReceive(_udp_server_socket);
		if (_udp_master_socket != INVALID_SOCKET) {
			NetworkUDPReceive(_udp_master_socket);
		}
	}
	else if (_udp_client_socket != INVALID_SOCKET) {
		NetworkUDPReceive(_udp_client_socket);
		if (_network_udp_broadcast > 0)
			_network_udp_broadcast--;
	}
}

// The main loop called from ttd.c
//  Here we also have to do StateGameLoop if needed!
void NetworkGameLoop(void)
{
	if (!_networking) return;

	if (!NetworkReceive()) return;

	if (_network_server) {
		// We first increase the _frame_counter
		_frame_counter++;

		NetworkHandleLocalQueue();

		// Then we make the frame
		StateGameLoop();

		_sync_seed_1 = _random_seeds[0][0];
#ifdef NETWORK_SEND_DOUBLE_SEED
		_sync_seed_2 = _random_seeds[0][1];
#endif

		NetworkServer_Tick();
	} else {
		// Client

		// Make sure we are at the frame were the server is (quick-frames)
		if (_frame_counter_server > _frame_counter) {
			while (_frame_counter_server > _frame_counter) {
				if (!NetworkDoClientLoop()) break;
			}
		} else {
			// Else, keep on going till _frame_counter_max
			if (_frame_counter_max > _frame_counter) {
				NetworkDoClientLoop();
			}
		}
	}

	NetworkSend();
}

static void NetworkGenerateUniqueId(void)
{
	md5_state_t state;
	md5_byte_t digest[16];
	char hex_output[16*2 + 1];
	char coding_string[NETWORK_NAME_LENGTH];
	int di;

	snprintf(coding_string, sizeof(coding_string), "%d%s", (uint)Random(), "OpenTTD Unique ID");

	/* Generate the MD5 hash */
	md5_init(&state);
	md5_append(&state, coding_string, strlen(coding_string));
	md5_finish(&state, digest);

	for (di = 0; di < 16; ++di)
		sprintf(hex_output + di * 2, "%02x", digest[di]);

	/* _network_unique_id is our id */
	snprintf(_network_unique_id, sizeof(_network_unique_id), "%s", hex_output);
}

// This tries to launch the network for a given OS
void NetworkStartUp(void)
{
	DEBUG(net, 3) ("[NET][Core] Starting network...");

	#if defined(__MORPHOS__) || defined(__AMIGA__)
	/*
	 *  IMPORTANT NOTE: SocketBase needs to be initialized before we use _any_
	 *  network related function, else: crash.
	 */
	{
		DEBUG(misc,3) ("[NET][Core] Loading bsd socket library");
		if (!(SocketBase = OpenLibrary("bsdsocket.library", 4))) {
			DEBUG(net, 0) ("[NET][Core] Error: couldn't open bsdsocket.library version 4. Network not available.");
			_network_available = false;
			return;
		}

		#if defined(__AMIGA__)
		// for usleep() implementation (only required for legacy AmigaOS builds)
		if ( (TimerPort = CreateMsgPort()) ) {
			if ( (TimerRequest = (struct timerequest *) CreateIORequest(TimerPort, sizeof(struct timerequest))) ) {
				if ( OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *) TimerRequest, 0) == 0 ) {
					if ( !(TimerBase = TimerRequest->tr_node.io_Device) ) {
						// free ressources...
						DEBUG(net, 0) ("[NET][Core] Error: couldn't initialize timer. Network not available.");
						_network_available = false;
						return;
					}
				}
			}
		}
		#endif // __AMIGA__
	}
	#endif // __MORPHOS__ / __AMIGA__

    // Network is available
	_network_available = true;
	_network_dedicated = false;
	_network_last_advertise_date = 0;
	_network_advertise_retries = 0;

	/* Load the ip from the openttd.cfg */
	_network_server_bind_ip = inet_addr(_network_server_bind_ip_host);
	/* And put the data back in it in case it was an invalid ip */
	snprintf(_network_server_bind_ip_host, sizeof(_network_server_bind_ip_host), "%s", inet_ntoa(*(struct in_addr *)&_network_server_bind_ip));

	/* Generate an unique id when there is none yet */
	if (_network_unique_id[0] == '\0')
		NetworkGenerateUniqueId();

	memset(&_network_game_info, 0, sizeof(_network_game_info));

	/* XXX - Hard number here, because the strings can currently handle no more
	    than 10 clients -- TrueLight */
	_network_game_info.clients_max = 10;

	// Let's load the network in windows
	#if defined(WIN32)
	{
		WSADATA wsa;
		DEBUG(net, 3) ("[NET][Core] Loading windows socket library");
		if (WSAStartup(MAKEWORD(2,0), &wsa) != 0) {
			DEBUG(net, 0) ("[NET][Core] Error: WSAStartup failed. Network not available.");
			_network_available = false;
			return;
		}
	}
	#endif // WIN32

	NetworkInitialize();
	DEBUG(net, 3) ("[NET][Core] Network online. Multiplayer available.");
	NetworkFindIPs();
}

// This shuts the network down
void NetworkShutDown(void)
{
	DEBUG(net, 3) ("[NET][Core] Shutting down the network.");

	_network_available = false;

	#if defined(__MORPHOS__) || defined(__AMIGA__)
	{
		// free allocated ressources
		#if defined(__AMIGA__)
			if (TimerBase)    { CloseDevice((struct IORequest *) TimerRequest); }
			if (TimerRequest) { DeleteIORequest(TimerRequest); }
			if (TimerPort)    { DeleteMsgPort(TimerPort); }
		#endif

		if (SocketBase) {
			CloseLibrary(SocketBase);
		}
	}
	#endif

	#if defined(WIN32)
	{
		WSACleanup();
	}
	#endif
}
#else

void ParseConnectionString(const char **player, const char **port, char *connection_string) {}
void NetworkUpdateClientInfo(uint16 client_index) {}

#endif /* ENABLE_NETWORK */
