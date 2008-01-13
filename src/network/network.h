/* $Id$ */

#ifndef NETWORK_H
#define NETWORK_H

#ifdef ENABLE_NETWORK

#include "../player_type.h"
#include "../economy_type.h"
#include "core/config.h"
#include "core/game.h"

// If this line is enable, every frame will have a sync test
//  this is not needed in normal games. Normal is like 1 sync in 100
//  frames. You can enable this if you have a lot of desyncs on a certain
//  game.
// Remember: both client and server have to be compiled with this
//  option enabled to make it to work. If one of the two has it disabled
//  nothing will happen.
//#define ENABLE_NETWORK_SYNC_EVERY_FRAME

// In theory sending 1 of the 2 seeds is enough to check for desyncs
//   so in theory, this next define can be left off.
//#define NETWORK_SEND_DOUBLE_SEED

// How many clients can we have? Like.. MAX_PLAYERS - 1 is the amount of
//  players that can really play.. so.. a max of 4 spectators.. gives us..
//  MAX_PLAYERS + 3
#define MAX_CLIENTS (MAX_PLAYERS + 3)


// Do not change this next line. It should _ALWAYS_ be MAX_CLIENTS + 1
#define MAX_CLIENT_INFO (MAX_CLIENTS + 1)

#define MAX_INTERFACES 9


// How many vehicle/station types we put over the network
#define NETWORK_VEHICLE_TYPES 5
#define NETWORK_STATION_TYPES 5

struct NetworkPlayerInfo {
	char company_name[NETWORK_NAME_LENGTH];         // Company name
	char password[NETWORK_PASSWORD_LENGTH];         // The password for the player
	Year inaugurated_year;                          // What year the company started in
	Money company_value;                            // The company value
	Money money;                                    // The amount of money the company has
	Money income;                                   // How much did the company earned last year
	uint16 performance;                             // What was his performance last month?
	bool use_password;                              // Is there a password
	uint16 num_vehicle[NETWORK_VEHICLE_TYPES];      // How many vehicles are there of this type?
	uint16 num_station[NETWORK_STATION_TYPES];      // How many stations are there of this type?
	char players[NETWORK_PLAYERS_LENGTH];           // The players that control this company (Name1, name2, ..)
	uint16 months_empty;                            // How many months the company is empty
};

struct NetworkClientInfo {
	uint16 client_index;                            // Index of the client (same as ClientState->index)
	char client_name[NETWORK_CLIENT_NAME_LENGTH];   // Name of the client
	byte client_lang;                               // The language of the client
	PlayerID client_playas;                         // As which player is this client playing (PlayerID)
	uint32 client_ip;                               // IP-address of the client (so he can be banned)
	Date join_date;                                 // Gamedate the player has joined
	char unique_id[NETWORK_UNIQUE_ID_LENGTH];       // Every play sends an unique id so we can indentify him
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

/* Language ids for server_lang and client_lang. Do NOT modify the order. */
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
	NETLANG_COUNT
};

VARDEF NetworkGameInfo _network_game_info;
VARDEF NetworkPlayerInfo _network_player_info[MAX_PLAYERS];
VARDEF NetworkClientInfo _network_client_info[MAX_CLIENT_INFO];

VARDEF char _network_player_name[NETWORK_CLIENT_NAME_LENGTH];
VARDEF char _network_default_ip[NETWORK_HOSTNAME_LENGTH];

VARDEF uint16 _network_own_client_index;
VARDEF char _network_unique_id[NETWORK_UNIQUE_ID_LENGTH]; // Our own unique ID

VARDEF uint32 _frame_counter_server; // The frame_counter of the server, if in network-mode
VARDEF uint32 _frame_counter_max; // To where we may go with our clients

VARDEF uint32 _last_sync_frame; // Used in the server to store the last time a sync packet was sent to clients.

// networking settings
VARDEF uint32 _broadcast_list[MAX_INTERFACES + 1];

VARDEF uint16 _network_server_port;
/* We use bind_ip and bind_ip_host, where bind_ip_host is the readable form of
    bind_ip_host, and bind_ip the numeric value, because we want a nice number
    in the openttd.cfg, but we wants to use the uint32 internally.. */
VARDEF uint32 _network_server_bind_ip;
VARDEF char _network_server_bind_ip_host[NETWORK_HOSTNAME_LENGTH];
VARDEF bool _is_network_server; // Does this client wants to be a network-server?
VARDEF char _network_server_name[NETWORK_NAME_LENGTH];
VARDEF char _network_server_password[NETWORK_PASSWORD_LENGTH];
VARDEF char _network_rcon_password[NETWORK_PASSWORD_LENGTH];
VARDEF char _network_default_company_pass[NETWORK_PASSWORD_LENGTH];

VARDEF uint16 _network_max_join_time;             ///< Time a client can max take to join
VARDEF bool _network_pause_on_join;               ///< Pause the game when a client tries to join (more chance of succeeding join)

VARDEF uint16 _redirect_console_to_client;

VARDEF uint16 _network_sync_freq;
VARDEF uint8 _network_frame_freq;

VARDEF uint32 _sync_seed_1, _sync_seed_2;
VARDEF uint32 _sync_frame;
VARDEF bool _network_first_time;
// Vars needed for the join-GUI
VARDEF NetworkJoinStatus _network_join_status;
VARDEF uint8 _network_join_waiting;
VARDEF uint16 _network_join_kbytes;
VARDEF uint16 _network_join_kbytes_total;

VARDEF char _network_last_host[NETWORK_HOSTNAME_LENGTH];
VARDEF short _network_last_port;
VARDEF uint32 _network_last_host_ip;
VARDEF uint8 _network_reconnect;

VARDEF bool _network_udp_server;
VARDEF uint16 _network_udp_broadcast;

VARDEF byte _network_lan_internet;

VARDEF bool _network_need_advertise;
VARDEF uint32 _network_last_advertise_frame;
VARDEF uint8 _network_advertise_retries;

VARDEF bool _network_autoclean_companies;
VARDEF uint8 _network_autoclean_unprotected; // Remove a company after X months
VARDEF uint8 _network_autoclean_protected;   // Unprotect a company after X months

VARDEF Year _network_restart_game_year;      // If this year is reached, the server automaticly restarts
VARDEF uint8 _network_min_players;           // Minimum number of players for game to unpause

void NetworkTCPQueryServer(const char* host, unsigned short port);

byte NetworkSpectatorCount();

VARDEF char *_network_host_list[10];
VARDEF char *_network_ban_list[25];

void ParseConnectionString(const char **player, const char **port, char *connection_string);
void NetworkUpdateClientInfo(uint16 client_index);
void NetworkAddServer(const char *b);
void NetworkRebuildHostList();
bool NetworkChangeCompanyPassword(byte argc, char *argv[]);
void NetworkPopulateCompanyInfo();
void UpdateNetworkGameWindow(bool unselect);
void CheckMinPlayers();
void NetworkStartDebugLog(const char *hostname, uint16 port);

void NetworkStartUp();
void NetworkUDPCloseAll();
void NetworkShutDown();
void NetworkGameLoop();
void NetworkUDPGameLoop();
bool NetworkServerStart();
bool NetworkClientConnectGame(const char *host, uint16 port);
void NetworkReboot();
void NetworkDisconnect();

bool IsNetworkCompatibleVersion(const char *version);

extern bool _networking;         ///< are we in networking mode?
VARDEF bool _network_server;     ///< network-server is active
VARDEF bool _network_available;  ///< is network mode available?
VARDEF bool _network_dedicated;  ///< are we a dedicated server?
VARDEF bool _network_advertise;  ///< is the server advertising to the master server?
extern bool _network_reload_cfg; ///< will we reload the entire config for the next game?

#else /* ENABLE_NETWORK */
/* Network function stubs when networking is disabled */

static inline void NetworkStartUp() {}
static inline void NetworkShutDown() {}

#define _networking 0
#define _network_server 0
#define _network_available 0
#define _network_dedicated 0
#define _network_advertise 0

#endif /* ENABLE_NETWORK */

/* This variable must always be registered! */
VARDEF PlayerID _network_playas; ///< an id to play as.. (see players.h:Players)

#endif /* NETWORK_H */
