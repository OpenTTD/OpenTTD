/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_tunnel.hpp Everything to query and build tunnels. */

#ifndef SCRIPT_TUNNEL_HPP
#define SCRIPT_TUNNEL_HPP

#include "script_vehicle.hpp"

/**
 * Class that handles all tunnel related functions.
 * @api ai game
 */
class ScriptTunnel : public ScriptObject {
public:
	/**
	 * All tunnel related errors.
	 */
	enum ErrorMessages {

		/** Base for bridge related errors */
		ERR_TUNNEL_BASE = ScriptError::ERR_CAT_TUNNEL << ScriptError::ERR_CAT_BIT_SIZE,

		/** Can't build tunnels on water */
		ERR_TUNNEL_CANNOT_BUILD_ON_WATER,            // [STR_ERROR_CAN_T_BUILD_ON_WATER]

		/** The start tile must slope either North, South, West or East */
		ERR_TUNNEL_START_SITE_UNSUITABLE,            // [STR_ERROR_SITE_UNSUITABLE_FOR_TUNNEL]

		/** Another tunnel is in the way */
		ERR_TUNNEL_ANOTHER_TUNNEL_IN_THE_WAY,        // [STR_ERROR_ANOTHER_TUNNEL_IN_THE_WAY]

		/** Unable to excavate land at the end to create the tunnel's exit */
		ERR_TUNNEL_END_SITE_UNSUITABLE,              // [STR_ERROR_UNABLE_TO_EXCAVATE_LAND]
	};

	/**
	 * Check whether the tile is an entrance to a tunnel.
	 * @param tile The tile to check.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @return True if and only if the tile is the beginning or end of a tunnel.
	 */
	static bool IsTunnelTile(TileIndex tile);

	/**
	 * Get the tile that exits on the other end of a (would be) tunnel starting
	 *  at tile. If there is no 'simple' inclined slope at the start tile,
	 *  this function will return ScriptMap::TILE_INVALID.
	 * @param tile The tile that is an entrance to a tunnel or the tile where you may want to build a tunnel.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @return The TileIndex that is the other end of the (would be) tunnel, or
	 *  ScriptMap::TILE_INVALID if no other end was found (can't build tunnel).
	 * @note Even if this function returns a valid tile, that is no guarantee
	 *  that building a tunnel will succeed. Use BuildTunnel in ScriptTestMode to
	 *  check whether a tunnel can actually be build.
	 */
	static TileIndex GetOtherTunnelEnd(TileIndex tile);

	/**
	 * Internal function to help BuildTunnel in case of road.
	 * @api -all
	 */
	static bool _BuildTunnelRoad1();

	/**
	 * Internal function to help BuildTunnel in case of road.
	 * @api -all
	 */
	static bool _BuildTunnelRoad2();

	/**
	 * Builds a tunnel starting at start. The direction of the tunnel depends
	 *  on the slope of the start tile. Tunnels can be created for either
	 *  rails or roads; use the appropriate ScriptVehicle::VehicleType.
	 * As an extra for road, this functions builds two half-pieces of road on
	 *  each end of the tunnel, making it easier for you to connect it to your
	 *  network.
	 * @param start Where to start the tunnel.
	 * @param vehicle_type The vehicle-type of tunnel to build.
	 * @pre ScriptMap::IsValidTile(start).
	 * @pre (vehicle_type == ScriptVehicle::VT_ROAD && ScriptRoad::IsRoadTypeAvailable(ScriptRoad::GetCurrentRoadType())) ||
	 *      (vehicle_type == ScriptVehicle::VT_RAIL && ScriptRail::IsRailTypeAvailable(ScriptRail::GetCurrentRailType())).
	 * @game @pre Outside CompanyMode: vehicle_type == ScriptVehicle::VT_ROAD.
	 * @exception ScriptError::ERR_AREA_NOT_CLEAR
	 * @exception ScriptTunnel::ERR_TUNNEL_CANNOT_BUILD_ON_WATER
	 * @exception ScriptTunnel::ERR_TUNNEL_START_SITE_UNSUITABLE
	 * @exception ScriptTunnel::ERR_TUNNEL_ANOTHER_TUNNEL_IN_THE_WAY
	 * @exception ScriptTunnel::ERR_TUNNEL_END_SITE_UNSUITABLE
	 * @return Whether the tunnel has been/can be build or not.
	 * @note The slope of a tile can be determined by ScriptTile::GetSlope(TileIndex).
	 * @note No matter if the road pieces were build or not, if building the
	 *  tunnel succeeded, this function returns true.
	 * @game @note Building a bridge (without CompanyMode) results in a bridge owned by towns.
	 */
	static bool BuildTunnel(ScriptVehicle::VehicleType vehicle_type, TileIndex start);

	/**
	 * Remove the tunnel whose entrance is located at tile.
	 * @param tile The tile that is an entrance to a tunnel.
	 * @pre ScriptMap::IsValidTile(tile) && IsTunnelTile(tile).
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @return Whether the tunnel has been/can be removed or not.
	 */
	static bool RemoveTunnel(TileIndex tile);
};

#endif /* SCRIPT_TUNNEL_HPP */
