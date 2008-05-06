/* $Id$ */

/** @file network_gamelist.h Handling of the list of games. */

#ifndef NETWORK_GAMELIST_H
#define NETWORK_GAMELIST_H

/** Structure with information shown in the game list (GUI) */
struct NetworkGameList {
	NetworkGameInfo info;  ///< The game information of this server
	uint32 ip;             ///< The IP of the game server
	uint16 port;           ///< The port of the game server
	bool online;           ///< False if the server did not respond (default status)
	bool manually;         ///< True if the server was added manually
	uint8 retries;         ///< Number of retries (to stop requerying)
	NetworkGameList *next; ///< Next pointer to make a linked game list
};

/** Game list of this client */
extern NetworkGameList *_network_game_list;

NetworkGameList *NetworkGameListAddItem(uint32 ip, uint16 port);
void NetworkGameListRemoveItem(NetworkGameList *remove);
void NetworkGameListRequery();

#endif /* NETWORK_GAMELIST_H */
