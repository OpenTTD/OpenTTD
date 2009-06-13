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
 * The game information that is not generated on-the-fly and has to
 * be sent to the clients.
 */
struct NetworkServerGameInfo {
	char map_name[NETWORK_NAME_LENGTH];             ///< Map which is played ["random" for a randomized map]
	byte clients_on;                                ///< Current count of clients on server
};

/**
 * The game information that is sent from the server to the clients.
 */
struct NetworkGameInfo : NetworkServerGameInfo {
	GRFConfig *grfconfig;                           ///< List of NewGRF files used
	Date start_date;                                ///< When the game started
	Date game_date;                                 ///< Current date
	uint16 map_width;                               ///< Map width
	uint16 map_height;                              ///< Map height
	char server_name[NETWORK_NAME_LENGTH];          ///< Server name
	char hostname[NETWORK_HOSTNAME_LENGTH];         ///< Hostname of the server (if any)
	char server_revision[NETWORK_REVISION_LENGTH];  ///< The version number the server is using (e.g.: 'r304' or 0.5.0)
	bool dedicated;                                 ///< Is this a dedicated server?
	bool version_compatible;                        ///< Can we connect to this server or not? (based on server_revision)
	bool compatible;                                ///< Can we connect to this server or not? (based on server_revision _and_ grf_match
	bool use_password;                              ///< Is this server passworded?
	byte game_info_version;                         ///< Version of the game info
	byte server_lang;                               ///< Language of the server (we should make a nice table for this)
	byte clients_max;                               ///< Max clients allowed on server
	byte companies_on;                              ///< How many started companies do we have
	byte companies_max;                             ///< Max companies allowed on server
	byte spectators_on;                             ///< How many spectators do we have?
	byte spectators_max;                            ///< Max spectators allowed on server
	byte map_set;                                   ///< Graphical set
};

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_GAME_H */
