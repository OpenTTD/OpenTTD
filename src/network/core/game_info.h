/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file game_info.h Convert NetworkGameInfo to Packet and back.
 */

#ifndef NETWORK_CORE_GAME_INFO_H
#define NETWORK_CORE_GAME_INFO_H

#include "config.h"
#include "core.h"
#include "../../newgrf_config.h"
#include "../../timer/timer_game_calendar.h"

#include <unordered_map>

/*
 * NetworkGameInfo has several revisions which we still need to support on the
 * wire. The table below shows the version and size for each field of the
 * serialized NetworkGameInfo.
 *
 * Version: Bytes:  Description:
 *   all      1       the version of this packet's structure
 *
 *   6+       1       type of storage for the NewGRFs below:
 *                      0 = NewGRF ID and MD5 checksum.
 *                          Used as default for version 5 and below, and for
 *                          later game updates to the Game Coordinator.
 *                      1 = NewGRF ID, MD5 checksum and name.
 *                          Used for direct requests and the first game
 *                          update to Game Coordinator.
 *                      2 = Index in NewGRF lookup table.
 *                          Used for sending server listing from the Game
 *                          Coordinator to the clients.
 *
 *   5+       4       version number of the Game Script (-1 is case none is selected).
 *   5+       var     string with the name of the Game Script.
 *
 *   4+       1       number of GRFs attached (n).
 *   4+       n * var identifiers for GRF files. Consists of:
 *                    Note: the 'vN' refers to packet version and 'type'
 *                    refers to the v6+ type of storage for the NewGRFs.
 *                     - 4 byte variable with the GRF ID.
 *                       For v4, v5, and v6+ in case of type 0 and/or type 1.
 *                     - 16 bytes with the MD5 checksum of the GRF.
 *                       For v4, v5, and v6+ in case of type 0 and/or type 1.
 *                     - string with name of NewGRF.
 *                       For v6+ in case of type 1.
 *                     - 4 byte lookup table index.
 *                       For v6+ in case of type 2.
 *
 *   3+       4       current game date in days since 1-1-0 (DMY)
 *   3+       4       game introduction date in days since 1-1-0 (DMY)
 *
 *   2+       1       maximum number of companies allowed on the server
 *   2+       1       number of companies on the server
 *   2+       1       maximum number of spectators allowed on the server
 *
 *   1+       var     string with the name of the server
 *   1+       var     string with the revision of the server
 *   1 - 5    1       the language run on the server
 *                    (0 = any, 1 = English, 2 = German, 3 = French)
 *   1+       1       whether the server uses a password (0 = no, 1 = yes)
 *   1+       1       maximum number of clients allowed on the server
 *   1+       1       number of clients on the server
 *   1+       1       number of spectators on the server
 *   1 & 2    2       current game date in days since 1-1-1920 (DMY)
 *   1 & 2    2       game introduction date in days since 1-1-1920 (DMY)
 *   1 - 5    var     string with the name of the map
 *   1+       2       width of the map in tiles
 *   1+       2       height of the map in tiles
 *   1+       1       type of map:
 *                    (0 = temperate, 1 = arctic, 2 = desert, 3 = toyland)
 *   1+       1       whether the server is dedicated (0 = no, 1 = yes)
 */

/** The different types/ways a NewGRF can be serialized in the GameInfo since version 6. */
enum NewGRFSerializationType {
	NST_GRFID_MD5      = 0, ///< Unique GRF ID and MD5 checksum.
	NST_GRFID_MD5_NAME = 1, ///< Unique GRF ID, MD5 checksum and name.
	NST_LOOKUP_ID      = 2, ///< Unique ID into a lookup table that is sent before.
	NST_END                 ///< The end of the list (period).
};

/**
 * The game information that is sent from the server to the client.
 */
struct NetworkServerGameInfo {
	GRFConfig *grfconfig;        ///< List of NewGRF files used
	TimerGameCalendar::Date start_date; ///< When the game started
	TimerGameCalendar::Date game_date;  ///< Current date
	uint16_t map_width;            ///< Map width
	uint16_t map_height;           ///< Map height
	std::string server_name;     ///< Server name
	std::string server_revision; ///< The version number the server is using (e.g.: 'r304' or 0.5.0)
	bool dedicated;              ///< Is this a dedicated server?
	bool use_password;           ///< Is this server passworded?
	byte clients_on;             ///< Current count of clients on server
	byte clients_max;            ///< Max clients allowed on server
	byte companies_on;           ///< How many started companies do we have
	byte companies_max;          ///< Max companies allowed on server
	byte spectators_on;          ///< How many spectators do we have?
	byte landscape;              ///< The used landscape
	int gamescript_version;      ///< Version of the gamescript.
	std::string gamescript_name; ///< Name of the gamescript.
};

/**
 * The game information that is sent from the server to the clients
 * with extra information only required at the client side.
 */
struct NetworkGameInfo : NetworkServerGameInfo {
	bool version_compatible;                        ///< Can we connect to this server or not? (based on server_revision)
	bool compatible;                                ///< Can we connect to this server or not? (based on server_revision _and_ grf_match
};

/**
 * Container to hold the GRF identifier (GRF ID + MD5 checksum) and the name
 * associated with that NewGRF.
 */
struct NamedGRFIdentifier {
	GRFIdentifier ident; ///< The unique identifier of the NewGRF.
	std::string name;    ///< The name of the NewGRF.
};
/** Lookup table for the GameInfo in case of #NST_LOOKUP_ID. */
typedef std::unordered_map<uint32_t, NamedGRFIdentifier> GameInfoNewGRFLookupTable;

extern NetworkServerGameInfo _network_game_info;

std::string_view GetNetworkRevisionString();
bool IsNetworkCompatibleVersion(std::string_view other);
void CheckGameCompatibility(NetworkGameInfo &ngi);

void FillStaticNetworkServerGameInfo();
const NetworkServerGameInfo *GetCurrentNetworkServerGameInfo();

void DeserializeGRFIdentifier(Packet *p, GRFIdentifier *grf);
void DeserializeGRFIdentifierWithName(Packet *p, NamedGRFIdentifier *grf);
void SerializeGRFIdentifier(Packet *p, const GRFIdentifier *grf);

void DeserializeNetworkGameInfo(Packet *p, NetworkGameInfo *info, const GameInfoNewGRFLookupTable *newgrf_lookup_table = nullptr);
void SerializeNetworkGameInfo(Packet *p, const NetworkServerGameInfo *info, bool send_newgrf_names = true);

#endif /* NETWORK_CORE_GAME_INFO_H */
