/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_bridge.hpp Everything to query and build bridges. */

#ifndef SCRIPT_BRIDGE_HPP
#define SCRIPT_BRIDGE_HPP

#include "script_vehicle.hpp"

/**
 * Class that handles all bridge related functions.
 * @api ai game
 */
class ScriptBridge : public ScriptObject {
public:
	/**
	 * All bridge related error messages.
	 */
	enum ErrorMessages {
		/** Base for bridge related errors */
		ERR_BRIDGE_BASE = ScriptError::ERR_CAT_BRIDGE << ScriptError::ERR_CAT_BIT_SIZE,

		/**
		 * The bridge you want to build is not available yet,
		 * or it is not available for the requested length.
		 */
		ERR_BRIDGE_TYPE_UNAVAILABLE,         // [STR_ERROR_CAN_T_BUILD_BRIDGE_HERE]

		/** One (or more) of the bridge head(s) ends in water. */
		ERR_BRIDGE_CANNOT_END_IN_WATER,      // [STR_ERROR_ENDS_OF_BRIDGE_MUST_BOTH]

		/** The bride heads need to be on the same height */
		ERR_BRIDGE_HEADS_NOT_ON_SAME_HEIGHT, // [STR_ERROR_BRIDGEHEADS_NOT_SAME_HEIGHT]
	};

	/**
	 * Checks whether the given bridge type is valid.
	 * @param bridge_id The bridge to check.
	 * @return True if and only if the bridge type is valid.
	 */
	static bool IsValidBridge(BridgeID bridge_id);

	/**
	 * Checks whether the given tile is actually a bridge start or end tile.
	 * @param tile The tile to check.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @return True if and only if the tile is the beginning or end of a bridge.
	 */
	static bool IsBridgeTile(TileIndex tile);

	/**
	 * Get the BridgeID of a bridge at a given tile.
	 * @param tile The tile to get the BridgeID from.
	 * @pre IsBridgeTile(tile).
	 * @return The BridgeID from the bridge at tile 'tile'.
	 */
	static BridgeID GetBridgeID(TileIndex tile);

	/**
	 * Get the name of a bridge.
	 * @param bridge_id The bridge to get the name of.
	 * @param vehicle_type The vehicle-type of bridge to get the name of.
	 * @pre IsValidBridge(bridge_id).
	 * @pre vehicle_type == ScriptVehicle::VT_ROAD || vehicle_type == ScriptVehicle::VT_RAIL || vehicle_type == ScriptVehicle::VT_WATER
	 * @return The name the bridge has.
	 */
	static std::optional<std::string> GetName(BridgeID bridge_id, ScriptVehicle::VehicleType vehicle_type);

	/**
	 * Get the maximum speed of a bridge.
	 * @param bridge_id The bridge to get the maximum speed of.
	 * @pre IsValidBridge(bridge_id).
	 * @return The maximum speed the bridge has.
	 * @note The speed is in OpenTTD's internal speed unit.
	 *       This is mph / 1.6, which is roughly km/h.
	 *       To get km/h multiply this number by 1.00584.
	 */
	static SQInteger GetMaxSpeed(BridgeID bridge_id);

	/**
	 * Get the new cost of a bridge, excluding the road and/or rail.
	 * @param bridge_id The bridge to get the new cost of.
	 * @param length The length of the bridge.
	 *               The value will be clamped to 0 .. MAX(int32_t).
	 * @pre IsValidBridge(bridge_id).
	 * @return The new cost the bridge has.
	 */
	static Money GetPrice(BridgeID bridge_id, SQInteger length);

	/**
	 * Get the maximum length of a bridge.
	 * @param bridge_id The bridge to get the maximum length of.
	 * @pre IsValidBridge(bridge_id).
	 * @returns The maximum length the bridge has.
	 */
	static SQInteger GetMaxLength(BridgeID bridge_id);

	/**
	 * Get the minimum length of a bridge.
	 * @param bridge_id The bridge to get the minimum length of.
	 * @pre IsValidBridge(bridge_id).
	 * @returns The minimum length the bridge has.
	 */
	static SQInteger GetMinLength(BridgeID bridge_id);

	/**
	 * Internal function to help BuildBridge in case of road.
	 * @api -all
	 */
	static bool _BuildBridgeRoad1();

	/**
	 * Internal function to help BuildBridge in case of road.
	 * @api -all
	 */
	static bool _BuildBridgeRoad2();

	/**
	 * Build a bridge from one tile to the other.
	 * As an extra for road, this functions builds two half-pieces of road on
	 *  each end of the bridge, making it easier for you to connect it to your
	 *  network.
	 * @param vehicle_type The vehicle-type of bridge to build.
	 * @param bridge_id The bridge-type to build.
	 * @param start Where to start the bridge.
	 * @param end Where to end the bridge.
	 * @pre ScriptMap::IsValidTile(start).
	 * @pre ScriptMap::IsValidTile(end).
	 * @pre 'start' and 'end' are in a straight line, i.e.
	 *  ScriptMap::GetTileX(start) == ScriptMap::GetTileX(end) or
	 *  ScriptMap::GetTileY(start) == ScriptMap::GetTileY(end).
	 * @pre vehicle_type == ScriptVehicle::VT_WATER ||
	 *   (vehicle_type == ScriptVehicle::VT_ROAD && ScriptRoad::IsRoadTypeAvailable(ScriptRoad::GetCurrentRoadType())) ||
	 *   (vehicle_type == ScriptVehicle::VT_RAIL && ScriptRail::IsRailTypeAvailable(ScriptRail::GetCurrentRailType())).
	 * @game @pre ScriptCompanyMode::IsValid() || vehicle_type == ScriptVehicle::VT_ROAD.
	 * @exception ScriptError::ERR_ALREADY_BUILT
	 * @exception ScriptError::ERR_AREA_NOT_CLEAR
	 * @exception ScriptError::ERR_LAND_SLOPED_WRONG
	 * @exception ScriptError::ERR_VEHICLE_IN_THE_WAY
	 * @exception ScriptBridge::ERR_BRIDGE_TYPE_UNAVAILABLE
	 * @exception ScriptBridge::ERR_BRIDGE_CANNOT_END_IN_WATER
	 * @exception ScriptBridge::ERR_BRIDGE_HEADS_NOT_ON_SAME_HEIGHT
	 * @return Whether the bridge has been/can be build or not.
	 * @game @note Building a bridge as deity (ScriptCompanyMode::IsDeity()) results in a bridge owned by towns.
	 * @note No matter if the road pieces were build or not, if building the
	 *  bridge succeeded, this function returns true.
	 */
	static bool BuildBridge(ScriptVehicle::VehicleType vehicle_type, BridgeID bridge_id, TileIndex start, TileIndex end);

	/**
	 * Removes a bridge, by executing it on either the start or end tile.
	 * @param tile An end or start tile of the bridge.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @return Whether the bridge has been/can be removed or not.
	 */
	static bool RemoveBridge(TileIndex tile);

	/**
	 * Get the tile that is on the other end of a bridge starting at tile.
	 * @param tile The tile that is an end of a bridge.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @pre IsBridgeTile(tile).
	 * @return The TileIndex that is the other end of the bridge.
	 */
	static TileIndex GetOtherBridgeEnd(TileIndex tile);
};

#endif /* SCRIPT_BRIDGE_HPP */
