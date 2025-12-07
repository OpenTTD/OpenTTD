/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file network_gamelist.cpp This file handles the GameList
 * Also, it handles the request to a server for data about the server
 */

#include "../stdafx.h"

#include "../window_func.h"
#include "network_internal.h"
#include "network_gamelist.h"

#include "../safeguards.h"

std::vector<std::unique_ptr<NetworkGame>> _network_game_list; ///< Game list of this client.
int _network_game_list_version = 0; ///< Current version of all items in the list.

/**
 * Add a new item to the linked gamelist. If the IP and Port match
 * return the existing item instead of adding it again
 * @param connection_string the address of the to-be added item
 * @return a point to the newly added or already existing item
 */
NetworkGame *NetworkGameListAddItem(std::string_view connection_string)
{
	/* Parse the connection string to ensure the default port is there. */
	const std::string resolved_connection_string = ServerAddress::Parse(connection_string, NETWORK_DEFAULT_PORT).connection_string;

	/* Check if it's already added. */
	auto it = std::ranges::find(_network_game_list, resolved_connection_string, &NetworkGame::connection_string);
	if (it != std::end(_network_game_list)) return it->get();

	auto &item = _network_game_list.emplace_back(std::make_unique<NetworkGame>(resolved_connection_string));
	item->info.gamescript_version = -1;
	item->version = _network_game_list_version;

	UpdateNetworkGameWindow();

	return item.get();
}

/**
 * Remove an item from the gamelist linked list
 * @param remove pointer to the item to be removed
 */
void NetworkGameListRemoveItem(NetworkGame *remove)
{
	auto it = std::ranges::find_if(_network_game_list, [&remove](const auto &item) { return item.get() == remove; });
	if (it != std::end(_network_game_list)) {
		_network_game_list.erase(it);

		NetworkRebuildHostList();
		UpdateNetworkGameWindow();
	}
}

/**
 * Remove all servers that have not recently been updated.
 * Call this after you received all the servers from the Game Coordinator, so
 * the ones that are no longer listed are removed.
 */
void NetworkGameListRemoveExpired()
{
	auto it = std::remove_if(std::begin(_network_game_list), std::end(_network_game_list), [](const auto &item) { return !item->manually && item->version < _network_game_list_version; });
	_network_game_list.erase(it, std::end(_network_game_list));

	UpdateNetworkGameWindow();
}

/**
 * Rebuild the GRFConfig's of the servers in the game list as we did
 * a rescan and might have found new NewGRFs.
 */
void NetworkAfterNewGRFScan()
{
	for (const auto &item : _network_game_list) {
		/* Reset compatibility state */
		item->info.compatible = item->info.version_compatible;

		for (auto &c : item->info.grfconfig) {
			assert(c->flags.Test(GRFConfigFlag::Copy));

			const GRFConfig *f = FindGRFConfig(c->ident.grfid, FGCM_EXACT, &c->ident.md5sum);
			if (f == nullptr) {
				/* Don't know the GRF (anymore), so mark game incompatible. */
				c->status = GCS_NOT_FOUND;

				/* If we miss a file, we're obviously incompatible. */
				item->info.compatible = false;
			} else {
				c->filename = f->filename;
				c->name = f->name;
				c->info = f->info;
				c->status = GCS_UNKNOWN;
			}
		}
	}

	InvalidateWindowClassesData(WC_NETWORK_WINDOW);
}
