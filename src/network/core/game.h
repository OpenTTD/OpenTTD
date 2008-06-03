/* $Id$ */

/**
 * @file game.h Information about a game that is sent between a
 *              game server, game client and masterserver.
 */

#ifndef NETWORK_CORE_GAME_H
#define NETWORK_CORE_GAME_H

#ifdef ENABLE_NETWORK

#include "config.h"
#include "../../newgrf_config.h"
#include "../../date_type.h"

/**
 * This is the struct used by both client and server
 * some fields will be empty on the client (like game_password) by default
 * and only filled with data a player enters.
 */
struct NetworkServerGameInfo {
	byte clients_on;                                ///< Current count of clients on server
	Date start_date;                                ///< When the game started
	char map_name[NETWORK_NAME_LENGTH];             ///< Map which is played ["random" for a randomized map]
};

struct NetworkGameInfo : NetworkServerGameInfo {
	byte game_info_version;                         ///< Version of the game info
	char server_name[NETWORK_NAME_LENGTH];          ///< Server name
	char hostname[NETWORK_HOSTNAME_LENGTH];         ///< Hostname of the server (if any)
	char server_revision[NETWORK_REVISION_LENGTH];  ///< The version number the server is using (e.g.: 'r304' or 0.5.0)
	bool version_compatible;                        ///< Can we connect to this server or not? (based on server_revision)
	bool compatible;                                ///< Can we connect to this server or not? (based on server_revision _and_ grf_match
	byte server_lang;                               ///< Language of the server (we should make a nice table for this)
	bool use_password;                              ///< Is this server passworded?
	byte clients_max;                               ///< Max clients allowed on server
	byte companies_on;                              ///< How many started companies do we have
	byte companies_max;                             ///< Max companies allowed on server
	byte spectators_on;                             ///< How many spectators do we have?
	byte spectators_max;                            ///< Max spectators allowed on server
	Date game_date;                                 ///< Current date
	uint16 map_width;                               ///< Map width
	uint16 map_height;                              ///< Map height
	byte map_set;                                   ///< Graphical set
	bool dedicated;                                 ///< Is this a dedicated server?
	GRFConfig *grfconfig;                           ///< List of NewGRF files used
};

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_GAME_H */
