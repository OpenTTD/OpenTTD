/* $Id$ */

/** @file network_internal.h Variables and function used internally. */

#ifndef NETWORK_INTERNAL_H
#define NETWORK_INTERNAL_H

#ifdef ENABLE_NETWORK

#include "network.h"
#include "network_func.h"
#include "core/os_abstraction.h"
#include "core/core.h"
#include "core/config.h"
#include "core/packet.h"
#include "core/tcp.h"

/**
 * If this line is enable, every frame will have a sync test
 *  this is not needed in normal games. Normal is like 1 sync in 100
 *  frames. You can enable this if you have a lot of desyncs on a certain
 *  game.
 * Remember: both client and server have to be compiled with this
 *  option enabled to make it to work. If one of the two has it disabled
 *  nothing will happen.
 */
//#define ENABLE_NETWORK_SYNC_EVERY_FRAME

/**
 * In theory sending 1 of the 2 seeds is enough to check for desyncs
 *   so in theory, this next define can be left off.
 */
//#define NETWORK_SEND_DOUBLE_SEED

#define MAX_TEXT_MSG_LEN 1024 /* long long long long sentences :-) */

enum MapPacket {
	MAP_PACKET_START,
	MAP_PACKET_NORMAL,
	MAP_PACKET_END,
};


enum NetworkJoinStatus {
	NETWORK_JOIN_STATUS_CONNECTING,
	NETWORK_JOIN_STATUS_AUTHORIZING,
	NETWORK_JOIN_STATUS_WAITING,
	NETWORK_JOIN_STATUS_DOWNLOADING,
	NETWORK_JOIN_STATUS_PROCESSING,
	NETWORK_JOIN_STATUS_REGISTERING,

	NETWORK_JOIN_STATUS_GETTING_COMPANY_INFO,
};

/** Language ids for server_lang and client_lang. Do NOT modify the order. */
enum NetworkLanguage {
	NETLANG_ANY = 0,
	NETLANG_ENGLISH,
	NETLANG_GERMAN,
	NETLANG_FRENCH,
	NETLANG_BRAZILIAN,
	NETLANG_BULGARIAN,
	NETLANG_CHINESE,
	NETLANG_CZECH,
	NETLANG_DANISH,
	NETLANG_DUTCH,
	NETLANG_ESPERANTO,
	NETLANG_FINNISH,
	NETLANG_HUNGARIAN,
	NETLANG_ICELANDIC,
	NETLANG_ITALIAN,
	NETLANG_JAPANESE,
	NETLANG_KOREAN,
	NETLANG_LITHUANIAN,
	NETLANG_NORWEGIAN,
	NETLANG_POLISH,
	NETLANG_PORTUGUESE,
	NETLANG_ROMANIAN,
	NETLANG_RUSSIAN,
	NETLANG_SLOVAK,
	NETLANG_SLOVENIAN,
	NETLANG_SPANISH,
	NETLANG_SWEDISH,
	NETLANG_TURKISH,
	NETLANG_UKRAINIAN,
	NETLANG_AFRIKAANS,
	NETLANG_CROATIAN,
	NETLANG_CATALAN,
	NETLANG_ESTONIAN,
	NETLANG_GALICIAN,
	NETLANG_GREEK,
	NETLANG_LATVIAN,
	NETLANG_COUNT
};

#define VARDEF extern

extern NetworkPlayerInfo _network_player_info[MAX_PLAYERS];

extern uint32 _frame_counter_server; // The frame_counter of the server, if in network-mode
extern uint32 _frame_counter_max; // To where we may go with our clients

extern uint32 _last_sync_frame; // Used in the server to store the last time a sync packet was sent to clients.

// networking settings
extern uint32 _broadcast_list[MAX_INTERFACES + 1];

extern uint32 _network_server_bind_ip;

extern uint32 _sync_seed_1, _sync_seed_2;
extern uint32 _sync_frame;
extern bool _network_first_time;
// Vars needed for the join-GUI
extern NetworkJoinStatus _network_join_status;
extern uint8 _network_join_waiting;
extern uint16 _network_join_kbytes;
extern uint16 _network_join_kbytes_total;

extern uint32 _network_last_host_ip;
extern uint8 _network_reconnect;

extern bool _network_udp_server;
extern uint16 _network_udp_broadcast;

extern uint8 _network_advertise_retries;

// following externs are instantiated at network.cpp
extern CommandPacket *_local_command_queue;

// Here we keep track of the clients
//  (and the client uses [0] for his own communication)
extern NetworkTCPSocketHandler _clients[MAX_CLIENTS];

void NetworkTCPQueryServer(const char* host, unsigned short port);

void NetworkAddServer(const char *b);
void NetworkRebuildHostList();
void UpdateNetworkGameWindow(bool unselect);

bool IsNetworkCompatibleVersion(const char *version);

void NetworkExecuteCommand(CommandPacket *cp);
void NetworkAddCommandQueue(NetworkTCPSocketHandler *cs, CommandPacket *cp);

// from network.c
void NetworkCloseClient(NetworkTCPSocketHandler *cs);
void CDECL NetworkTextMessage(NetworkAction action, ConsoleColour color, bool self_send, const char *name, const char *str, ...);
void NetworkGetClientName(char *clientname, size_t size, const NetworkTCPSocketHandler *cs);
uint NetworkCalculateLag(const NetworkTCPSocketHandler *cs);
byte NetworkGetCurrentLanguageIndex();
NetworkTCPSocketHandler *NetworkFindClientStateFromIndex(uint16 client_index);
unsigned long NetworkResolveHost(const char *hostname);
char* GetNetworkErrorMsg(char* buf, NetworkErrorCode err, const char* last);
bool NetworkFindName(char new_name[NETWORK_CLIENT_NAME_LENGTH]);

#define DEREF_CLIENT(i) (&_clients[i])
// This returns the NetworkClientInfo from a NetworkClientState
#define DEREF_CLIENT_INFO(cs) (&_network_client_info[cs - _clients])

// Macros to make life a bit more easier
#define DEF_CLIENT_RECEIVE_COMMAND(type) NetworkRecvStatus NetworkPacketReceive_ ## type ## _command(Packet *p)
#define DEF_CLIENT_SEND_COMMAND(type) void NetworkPacketSend_ ## type ## _command()
#define DEF_CLIENT_SEND_COMMAND_PARAM(type) void NetworkPacketSend_ ## type ## _command
#define DEF_SERVER_RECEIVE_COMMAND(type) void NetworkPacketReceive_ ## type ## _command(NetworkTCPSocketHandler *cs, Packet *p)
#define DEF_SERVER_SEND_COMMAND(type) void NetworkPacketSend_ ## type ## _command(NetworkTCPSocketHandler *cs)
#define DEF_SERVER_SEND_COMMAND_PARAM(type) void NetworkPacketSend_ ## type ## _command

#define SEND_COMMAND(type) NetworkPacketSend_ ## type ## _command
#define RECEIVE_COMMAND(type) NetworkPacketReceive_ ## type ## _command

#define FOR_ALL_CLIENTS(cs) for (cs = _clients; cs != endof(_clients) && cs->IsConnected(); cs++)

#endif /* ENABLE_NETWORK */
#endif /* NETWORK_INTERNAL_H */
