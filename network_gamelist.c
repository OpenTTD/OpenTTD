#include "stdafx.h"
#include "network_data.h"

#ifdef ENABLE_NETWORK

//
// This file handles the GameList
// Also, it handles the request to a server for data about the server

extern void UpdateNetworkGameWindow(bool unselect);

void NetworkGameListClear(void)
{
	NetworkGameList *item;
	NetworkGameList *next;

	item = _network_game_list;

	while (item != NULL) {
		next = item->next;
		free(item);
		item = next;
	}
	_network_game_list = NULL;
	_network_game_count = 0;

	UpdateNetworkGameWindow(true);

	DEBUG(net, 4)("[NET][GameList] Cleared list");
}

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

void NetworkGameListAddQueriedItem(const NetworkGameInfo *info, bool server_online)
{
	// We queried a server and now we are going to add it to the list
	NetworkGameList *item;

	item = NetworkGameListAddItem(_network_last_host_ip, _network_last_port);
	item->online = server_online;
	memcpy(&item->info, info, sizeof(NetworkGameInfo));
	ttd_strlcpy(item->info.hostname, _network_last_host, sizeof(item->info.hostname));

	UpdateNetworkGameWindow(false);
}

#endif /* ENABLE_NETWORK */


