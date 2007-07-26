/* $Id$ */

/**
 * @file network_gamelist.cpp This file handles the GameList
 * Also, it handles the request to a server for data about the server
 */

#ifdef ENABLE_NETWORK

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_config.h"
#include "../helpers.hpp"
#include "core/game.h"
#include "network_udp.h"
#include "network_gamelist.h"
#include "network_gui.h"

NetworkGameList *_network_game_list = NULL;

/** Add a new item to the linked gamelist. If the IP and Port match
 * return the existing item instead of adding it again
 * @param ip the IP-address (inet_addr) of the to-be added item
 * @param port the port the server is running on
 * @return a point to the newly added or already existing item */
NetworkGameList *NetworkGameListAddItem(uint32 ip, uint16 port)
{
	NetworkGameList *item, *prev_item;

	prev_item = NULL;
	for (item = _network_game_list; item != NULL; item = item->next) {
		if (item->ip == ip && item->port == port) return item;
		prev_item = item;
	}

	item = MallocT<NetworkGameList>(1);
	memset(item, 0, sizeof(*item));
	item->next = NULL;
	item->ip = ip;
	item->port = port;

	if (prev_item == NULL) {
		_network_game_list = item;
	} else {
		prev_item->next = item;
	}
	DEBUG(net, 4, "[gamelist] added server to list");

	UpdateNetworkGameWindow(false);

	return item;
}

/** Remove an item from the gamelist linked list
 * @param remove pointer to the item to be removed */
void NetworkGameListRemoveItem(NetworkGameList *remove)
{
	NetworkGameList *item, *prev_item;

	prev_item = NULL;
	for (item = _network_game_list; item != NULL; item = item->next) {
		if (remove == item) {
			if (prev_item == NULL) {
				_network_game_list = remove->next;
			} else {
				prev_item->next = remove->next;
			}

			/* Remove GRFConfig information */
			ClearGRFConfigList(&remove->info.grfconfig);
			free(remove);
			remove = NULL;

			DEBUG(net, 4, "[gamelist] removed server from list");
			UpdateNetworkGameWindow(false);
			return;
		}
		prev_item = item;
	}
}

enum {
	MAX_GAME_LIST_REQUERY_COUNT  =  5, ///< How often do we requery in number of times per server?
	REQUERY_EVERY_X_GAMELOOPS    = 60, ///< How often do we requery in time?
	REFRESH_GAMEINFO_X_REQUERIES = 50, ///< Refresh the game info itself after REFRESH_GAMEINFO_X_REQUERIES * REQUERY_EVERY_X_GAMELOOPS game loops
};

/** Requeries the (game) servers we have not gotten a reply from */
void NetworkGameListRequery()
{
	static uint8 requery_cnt = 0;

	if (++requery_cnt < REQUERY_EVERY_X_GAMELOOPS) return;
	requery_cnt = 0;

	struct in_addr ip;
	NetworkGameList *item;

	for (item = _network_game_list; item != NULL; item = item->next) {
		item->retries++;
		if (item->retries < REFRESH_GAMEINFO_X_REQUERIES && (item->online || item->retries >= MAX_GAME_LIST_REQUERY_COUNT)) continue;

		ip.s_addr = item->ip;

		/* item gets mostly zeroed by NetworkUDPQueryServer */
		uint8 retries = item->retries;
		NetworkUDPQueryServer(inet_ntoa(ip), item->port);
		item->retries = (retries >= REFRESH_GAMEINFO_X_REQUERIES) ? 0 : retries;
	}
}

#endif /* ENABLE_NETWORK */
