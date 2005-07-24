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
	NetworkGameList *item;

	item = _network_game_list;
	if (item != NULL) {
		while (item->next != NULL) {
			if (item->ip == ip && item->port == port)
				return item;
			item = item->next;
		}

		if (item->ip == ip && item->port == port)
			return item;

		item->next = malloc(sizeof(*item));
		item = item->next;
	} else {
		item = malloc(sizeof(*item));
		_network_game_list = item;
	}

	DEBUG(net, 4) ("[NET][GameList] Added server to list");

	memset(item, 0, sizeof(*item));

	item->next = NULL;
	item->ip = ip;
	item->port = port;
	_network_game_count++;

	UpdateNetworkGameWindow(false);

	return item;
}

void NetworkGameListRemoveItem(NetworkGameList *remove)
{
	NetworkGameList *item;

	item = _network_game_list;

	// examine head of the list
	if ( remove == _network_game_list ) {
		_network_game_list = remove->next;
		free(remove);
		DEBUG(net, 4) ("[NET][GameList] Removed server from list");
		return;
	}

	// examine each item
	while ( item->next != NULL ) {
		if ( item->next == remove )
		{
			item->next = remove->next;
			free(remove);
			DEBUG(net, 4) ("[NET][GameList] Removed server from list");
			return;
		}
		item = item->next;
	}
}

#endif /* ENABLE_NETWORK */
