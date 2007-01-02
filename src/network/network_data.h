/* $Id$ */

#ifndef NETWORK_DATA_H
#define NETWORK_DATA_H

// Is the network enabled?
#ifdef ENABLE_NETWORK

#include "../openttd.h"
#include "network.h"
#include "core/os_abstraction.h"
#include "core/config.h"
#include "core/packet.h"

#define MAX_TEXT_MSG_LEN 1024 /* long long long long sentences :-) */

// The client-info-server-index is always 1
#define NETWORK_SERVER_INDEX 1
#define NETWORK_EMPTY_INDEX 0

typedef struct CommandPacket {
	struct CommandPacket *next;
	PlayerID player; /// player that is executing the command
	uint32 cmd;    /// command being executed
	uint32 p1;     /// parameter p1
	uint32 p2;     /// parameter p2
	TileIndex tile; /// tile command being executed on
	char text[80];
	uint32 frame;  /// the frame in which this packet is executed
	byte callback; /// any callback function executed upon successful completion of the command
} CommandPacket;

typedef enum {
	STATUS_INACTIVE,
	STATUS_AUTH, // This means that the client is authorized
	STATUS_MAP_WAIT, // This means that the client is put on hold because someone else is getting the map
	STATUS_MAP,
	STATUS_DONE_MAP,
	STATUS_PRE_ACTIVE,
	STATUS_ACTIVE,
} ClientStatus;

typedef enum {
	MAP_PACKET_START,
	MAP_PACKET_NORMAL,
	MAP_PACKET_END,
} MapPacket;

typedef enum {
	NETWORK_RECV_STATUS_OKAY,
	NETWORK_RECV_STATUS_DESYNC,
	NETWORK_RECV_STATUS_SAVEGAME,
	NETWORK_RECV_STATUS_CONN_LOST,
	NETWORK_RECV_STATUS_MALFORMED_PACKET,
	NETWORK_RECV_STATUS_SERVER_ERROR, // The server told us we made an error
	NETWORK_RECV_STATUS_SERVER_FULL,
	NETWORK_RECV_STATUS_SERVER_BANNED,
	NETWORK_RECV_STATUS_CLOSE_QUERY, // Done quering the server
} NetworkRecvStatus;

typedef enum {
	NETWORK_ERROR_GENERAL, // Try to use thisone like never

	// Signals from clients
	NETWORK_ERROR_DESYNC,
	NETWORK_ERROR_SAVEGAME_FAILED,
	NETWORK_ERROR_CONNECTION_LOST,
	NETWORK_ERROR_ILLEGAL_PACKET,

	// Signals from servers
	NETWORK_ERROR_NOT_AUTHORIZED,
	NETWORK_ERROR_NOT_EXPECTED,
	NETWORK_ERROR_WRONG_REVISION,
	NETWORK_ERROR_NAME_IN_USE,
	NETWORK_ERROR_WRONG_PASSWORD,
	NETWORK_ERROR_PLAYER_MISMATCH, // Happens in CLIENT_COMMAND
	NETWORK_ERROR_KICKED,
	NETWORK_ERROR_CHEATER,
	NETWORK_ERROR_FULL,
} NetworkErrorCode;

// Actions that can be used for NetworkTextMessage
typedef enum {
	NETWORK_ACTION_JOIN,
	NETWORK_ACTION_LEAVE,
	NETWORK_ACTION_SERVER_MESSAGE,
	NETWORK_ACTION_CHAT,
	NETWORK_ACTION_CHAT_COMPANY,
	NETWORK_ACTION_CHAT_CLIENT,
	NETWORK_ACTION_GIVE_MONEY,
	NETWORK_ACTION_NAME_CHANGE,
} NetworkAction;

typedef enum {
	NETWORK_GAME_PASSWORD,
	NETWORK_COMPANY_PASSWORD,
} NetworkPasswordType;

// To keep the clients all together
struct NetworkClientState { // Typedeffed in network_core/packet.h
	SOCKET socket;
	uint16 index;
	uint32 last_frame;
	uint32 last_frame_server;
	byte lag_test; // This byte is used for lag-testing the client

	ClientStatus status;
	bool writable; // is client ready to write to?
	bool has_quit;

	Packet *packet_queue; // Packets that are awaiting delivery
	Packet *packet_recv; // Partially received packet

	CommandPacket *command_queue; // The command-queue awaiting delivery
};

typedef enum {
	DESTTYPE_BROADCAST, ///< Send message/notice to all players (All)
	DESTTYPE_TEAM,    ///< Send message/notice to everyone playing the same company (Team)
	DESTTYPE_CLIENT,    ///< Send message/notice to only a certain player (Private)
} DestType;

CommandPacket *_local_command_queue;

SOCKET _udp_client_socket; // udp client socket
SOCKET _udp_server_socket; // udp server socket
SOCKET _udp_master_socket; // udp master socket

// Here we keep track of the clients
//  (and the client uses [0] for his own communication)
NetworkClientState _clients[MAX_CLIENTS];
#define DEREF_CLIENT(i) (&_clients[i])
// This returns the NetworkClientInfo from a NetworkClientState
#define DEREF_CLIENT_INFO(cs) (&_network_client_info[cs - _clients])

// Macros to make life a bit more easier
#define DEF_CLIENT_RECEIVE_COMMAND(type) NetworkRecvStatus NetworkPacketReceive_ ## type ## _command(Packet *p)
#define DEF_CLIENT_SEND_COMMAND(type) void NetworkPacketSend_ ## type ## _command(void)
#define DEF_CLIENT_SEND_COMMAND_PARAM(type) void NetworkPacketSend_ ## type ## _command
#define DEF_SERVER_RECEIVE_COMMAND(type) void NetworkPacketReceive_ ## type ## _command(NetworkClientState *cs, Packet *p)
#define DEF_SERVER_SEND_COMMAND(type) void NetworkPacketSend_ ## type ## _command(NetworkClientState *cs)
#define DEF_SERVER_SEND_COMMAND_PARAM(type) void NetworkPacketSend_ ## type ## _command

#define SEND_COMMAND(type) NetworkPacketSend_ ## type ## _command
#define RECEIVE_COMMAND(type) NetworkPacketReceive_ ## type ## _command

#define FOR_ALL_CLIENTS(cs) for (cs = _clients; cs != endof(_clients) && cs->socket != INVALID_SOCKET; cs++)
#define FOR_ALL_ACTIVE_CLIENT_INFOS(ci) for (ci = _network_client_info; ci != endof(_network_client_info); ci++) if (ci->client_index != NETWORK_EMPTY_INDEX)

void NetworkExecuteCommand(CommandPacket *cp);
void NetworkAddCommandQueue(NetworkClientState *cs, CommandPacket *cp);

// from network.c
void NetworkCloseClient(NetworkClientState *cs);
void CDECL NetworkTextMessage(NetworkAction action, uint16 color, bool self_send, const char *name, const char *str, ...);
void NetworkGetClientName(char *clientname, size_t size, const NetworkClientState *cs);
uint NetworkCalculateLag(const NetworkClientState *cs);
byte NetworkGetCurrentLanguageIndex(void);
NetworkClientInfo *NetworkFindClientInfoFromIndex(uint16 client_index);
NetworkClientInfo *NetworkFindClientInfoFromIP(const char *ip);
NetworkClientState *NetworkFindClientStateFromIndex(uint16 client_index);
unsigned long NetworkResolveHost(const char *hostname);
char* GetNetworkErrorMsg(char* buf, NetworkErrorCode err, const char* last);

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_DATA_H */
