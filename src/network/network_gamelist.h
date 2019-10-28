/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_gamelist.h Handling of the list of games. */

#ifndef NETWORK_GAMELIST_H
#define NETWORK_GAMELIST_H

#include "core/address.h"
#include "network_type.h"

/** Structure with information shown in the game list (GUI) */
struct NetworkGameList {
	NetworkGameInfo info;   ///< The game information of this server
	NetworkAddress address; ///< The connection info of the game server
	bool online;            ///< False if the server did not respond (default status)
	bool manually;          ///< True if the server was added manually
	uint8 retries;          ///< Number of retries (to stop requerying)
	NetworkGameList *next;  ///< Next pointer to make a linked game list
};

/** Game list of this client */
extern NetworkGameList *_network_game_list;

void NetworkGameListAddItemDelayed(NetworkGameList *item);
NetworkGameList *NetworkGameListAddItem(NetworkAddress address);
void NetworkGameListRemoveItem(NetworkGameList *remove);
void NetworkGameListRequery();

#endif /* NETWORK_GAMELIST_H */
