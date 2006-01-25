/* $Id$ */

#include "stdafx.h"
#include "debug.h"
#include "network_data.h"

#ifdef ENABLE_NETWORK

//
// This file handles the GameList
// Also, it handles the request to a server for data about the server

extern void UpdateNetworkGameWindow(bool unselect);


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

	if (prev_item == NULL) {_network_game_list = item;}
	else {prev_item->next = item;}
	DEBUG(net, 4) ("[NET][GameList] Added server to list");

	UpdateNetworkGameWindow(false);

	return item;
}

void NetworkGameListRemoveItem(NetworkGameList *remove)
{
	NetworkGameList *item, *prev_item;

	prev_item = NULL;
	for (item = _network_game_list; item != NULL; item = item->next) {
		if (remove == item) {
			if (prev_item == NULL) {_network_game_list = remove->next;}
			else {prev_item->next = remove->next;}

			free(remove);
			DEBUG(net, 4) ("[NET][GameList] Removed server from list");
			UpdateNetworkGameWindow(false);
			return;
		}
		prev_item = item;
	}
}

#endif /* ENABLE_NETWORK */
