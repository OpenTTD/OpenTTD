#ifndef NETWORK_H
#define NETWORK_H

typedef struct NetworkGameInfo {
	char server_name[40];			// name of the game
	char server_revision[8];	// server game version
	byte server_lang;					// langid
	byte players_max;					// max players allowed on server
	byte players_on;					// current count of players on server
	uint16 game_date;					// current date
	char game_password[10];		// should fit ... 10 chars
	char map_name[40];				// map which is played ["random" for a randomized map]
	uint map_width;						// map width / 8
	uint map_height;					// map height / 8
	byte map_set;							// graphical set
} NetworkGameInfo;

//typedef struct NetworkGameList;

typedef struct NetworkGameList {
	NetworkGameInfo item;
	uint32 ip;
	uint16 port;
	struct NetworkGameList * _next;
} NetworkGameList;

NetworkGameInfo _network_game;
NetworkGameList * _network_game_list;

#endif /* NETWORK_H */
