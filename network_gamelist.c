/* $Id$ */

#ifdef ENABLE_NETWORK

#include "stdafx.h"
#include "debug.h"
#include "network_data.h"
#include "newgrf_config.h"

// This file handles the GameList
// Also, it handles the request to a server for data about the server

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

	item = malloc(sizeof(*item));
	memset(item, 0, sizeof(*item));
	item->next = NULL;
	item->ip = ip;
	item->port = port;

	if (prev_item == NULL) {
		_network_game_list = item;
	} else {
		prev_item->next = item;
	}
	DEBUG(net, 4) ("[NET][GameList] Added server to list");

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
			ClearGRFConfigList(remove->info.grfconfig);

			free(remove);
			DEBUG(net, 4) ("[NET][GameList] Removed server from list");
			UpdateNetworkGameWindow(false);
			return;
		}
		prev_item = item;
	}
}

#endif /* ENABLE_NETWORK */
