/* $Id$ */

/**
 * @file game.h Information about a game that is sent between a
 *              game server, game client and masterserver.
 */

#ifndef NETWORK_CORE_GAME_H
#define NETWORK_CORE_GAME_H

#ifdef ENABLE_NETWORK

#include "config.h"
#include "../../date.h"
#include "../../newgrf_config.h"

/**
 * This is the struct used by both client and server
 * some fields will be empty on the client (like game_password) by default
 * and only filled with data a player enters.
 */
typedef struct NetworkGameInfo {
	byte game_info_version;                         ///< Version of the game info
	char server_name[NETWORK_NAME_LENGTH];          ///< Server name
	char hostname[NETWORK_HOSTNAME_LENGTH];         ///< Hostname of the server (if any)
	char server_revision[NETWORK_REVISION_LENGTH];  ///< The version number the server is using (e.g.: 'r304' or 0.5.0)
	bool version_compatible;                        ///< Can we connect to this server or not? (based on server_revision)
	bool compatible;                                ///< Can we connect to this server or not? (based on server_revision _and_ grf_match
	byte server_lang;                               ///< Language of the server (we should make a nice table for this)
	bool use_password;                              ///< Is this server passworded?
	char server_password[NETWORK_PASSWORD_LENGTH];  ///< On the server: the game password, on the client: != "" if server has password
	byte clients_max;                               ///< Max clients allowed on server
	byte clients_on;                                ///< Current count of clients on server
	byte companies_max;                             ///< Max companies allowed on server
	byte companies_on;                              ///< How many started companies do we have
	byte spectators_max;                            ///< Max spectators allowed on server
	byte spectators_on;                             ///< How many spectators do we have?
	Date game_date;                                 ///< Current date
	Date start_date;                                ///< When the game started
	char map_name[NETWORK_NAME_LENGTH];             ///< Map which is played ["random" for a randomized map]
	uint16 map_width;                               ///< Map width
	uint16 map_height;                              ///< Map height
	byte map_set;                                   ///< Graphical set
	bool dedicated;                                 ///< Is this a dedicated server?
	char rcon_password[NETWORK_PASSWORD_LENGTH];    ///< RCon password for the server. "" if rcon is disabled
	struct GRFConfig *grfconfig;                    ///< List of NewGRF files used
} NetworkGameInfo;

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_GAME_H */
