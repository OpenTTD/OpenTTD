/* $Id$ */

#ifndef NETWORK_H
#define NETWORK_H

#define NOREV_STRING "norev000"

#ifdef ENABLE_NETWORK

#include "../player.h"
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

typedef struct NetworkPlayerInfo {
	char company_name[NETWORK_NAME_LENGTH];         // Company name
	char password[NETWORK_PASSWORD_LENGTH];         // The password for the player
	Year inaugurated_year;                          // What year the company started in
	int64 company_value;                            // The company value
	int64 money;                                    // The amount of money the company has
	int64 income;                                   // How much did the company earned last year
	uint16 performance;                             // What was his performance last month?
	byte use_password;                              // 0: No password 1: There is a password
	uint16 num_vehicle[NETWORK_VEHICLE_TYPES];      // How many vehicles are there of this type?
	uint16 num_station[NETWORK_STATION_TYPES];      // How many stations are there of this type?
	char players[NETWORK_PLAYERS_LENGTH];           // The players that control this company (Name1, name2, ..)
	uint16 months_empty;                            // How many months the company is empty
} NetworkPlayerInfo;

typedef struct NetworkClientInfo {
	uint16 client_index;                            // Index of the client (same as ClientState->index)
	char client_name[NETWORK_CLIENT_NAME_LENGTH];   // Name of the client
	byte client_lang;                               // The language of the client
	PlayerID client_playas;                         // As which player is this client playing (PlayerID)
	uint32 client_ip;                               // IP-address of the client (so he can be banned)
	Date join_date;                                 // Gamedate the player has joined
	char unique_id[NETWORK_NAME_LENGTH];            // Every play sends an unique id so we can indentify him
} NetworkClientInfo;

typedef struct NetworkGameList {
	NetworkGameInfo info;
	uint32 ip;
	uint16 port;
	bool online;                                    // False if the server did not respond (default status)
	bool manually;                                  // True if the server was added manually
	uint8 retries;
	struct NetworkGameList *next;
} NetworkGameList;

typedef enum {
	NETWORK_JOIN_STATUS_CONNECTING,
	NETWORK_JOIN_STATUS_AUTHORIZING,
	NETWORK_JOIN_STATUS_WAITING,
	NETWORK_JOIN_STATUS_DOWNLOADING,
	NETWORK_JOIN_STATUS_PROCESSING,
	NETWORK_JOIN_STATUS_REGISTERING,

	NETWORK_JOIN_STATUS_GETTING_COMPANY_INFO,
} NetworkJoinStatus;

// language ids for server_lang and client_lang
typedef enum {
	NETLANG_ANY     = 0,
	NETLANG_ENGLISH = 1,
	NETLANG_GERMAN  = 2,
	NETLANG_FRENCH  = 3,
} NetworkLanguage;

VARDEF NetworkGameList *_network_game_list;

VARDEF NetworkGameInfo _network_game_info;
VARDEF NetworkPlayerInfo _network_player_info[MAX_PLAYERS];
VARDEF NetworkClientInfo _network_client_info[MAX_CLIENT_INFO];

VARDEF char _network_player_name[NETWORK_CLIENT_NAME_LENGTH];
VARDEF char _network_default_ip[NETWORK_HOSTNAME_LENGTH];

VARDEF uint16 _network_own_client_index;
VARDEF char _network_unique_id[NETWORK_NAME_LENGTH]; // Our own unique ID

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

NetworkGameList *NetworkQueryServer(const char* host, unsigned short port, bool game_info);

byte NetworkSpectatorCount(void);

VARDEF char *_network_host_list[10];
VARDEF char *_network_ban_list[25];

void ParseConnectionString(const char **player, const char **port, char *connection_string);
void NetworkUpdateClientInfo(uint16 client_index);
void NetworkAddServer(const char *b);
void NetworkRebuildHostList(void);
bool NetworkChangeCompanyPassword(byte argc, char *argv[]);
void NetworkPopulateCompanyInfo(void);
void UpdateNetworkGameWindow(bool unselect);
void CheckMinPlayers(void);

void NetworkStartUp(void);
void NetworkUDPCloseAll();
void NetworkShutDown(void);
void NetworkGameLoop(void);
void NetworkUDPGameLoop(void);
bool NetworkServerStart(void);
bool NetworkClientConnectGame(const char *host, uint16 port);
void NetworkReboot(void);
void NetworkDisconnect(void);

VARDEF bool _network_server;     ///< network-server is active
VARDEF bool _network_available;  ///< is network mode available?
VARDEF bool _network_dedicated;  ///< are we a dedicated server?
VARDEF bool _network_advertise;  ///< is the server advertising to the master server?

#else /* ENABLE_NETWORK */
/* Network function stubs when networking is disabled */

static inline void NetworkStartUp(void) {}
static inline void NetworkShutDown(void) {}

#define _networking 0
#define _network_server 0
#define _network_available 0
#define _network_dedicated 0
#define _network_advertise 0

#endif /* ENABLE_NETWORK */

/* Thss variable must always be registered! */
VARDEF PlayerID _network_playas; ///< an id to play as.. (see players.h:Players)

#endif /* NETWORK_H */
