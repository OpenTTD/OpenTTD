/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_bridge.hpp Everything to query and build bridges. */

#ifndef AI_BRIDGE_HPP
#define AI_BRIDGE_HPP

#include "ai_vehicle.hpp"

/**
 * Class that handles all bridge related functions.
 */
class AIBridge : public AIObject {
public:
	/**
	 * All bridge related error messages.
	 */
	enum ErrorMessages {
		/** Base for bridge related errors */
		ERR_BRIDGE_BASE = AIError::ERR_CAT_BRIDGE << AIError::ERR_CAT_BIT_SIZE,

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

	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIBridge"; }

	/**
	 * Checks whether the given bridge type is valid.
	 * @param bridge_id The bridge to check.
	 * @return True if and only if the bridge type is valid.
	 */
	static bool IsValidBridge(BridgeID bridge_id);

	/**
	 * Checks whether the given tile is actually a bridge start or end tile.
	 * @param tile The tile to check.
	 * @pre AIMap::IsValidTile(tile).
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
	 * @pre IsValidBridge(bridge_id).
	 * @return The name the bridge has.
	 */
	static char *GetName(BridgeID bridge_id);

	/**
	 * Get the maximum speed of a bridge.
	 * @param bridge_id The bridge to get the maximum speed of.
	 * @pre IsValidBridge(bridge_id).
	 * @return The maximum speed the bridge has.
	 * @note The speed is in OpenTTD's internal speed unit.
	 *       This is mph / 1.6, which is roughly km/h.
	 *       To get km/h multiply this number by 1.00584.
	 */
	static int32 GetMaxSpeed(BridgeID bridge_id);

	/**
	 * Get the new cost of a bridge, excluding the road and/or rail.
	 * @param bridge_id The bridge to get the new cost of.
	 * @param length The length of the bridge.
	 * @pre IsValidBridge(bridge_id).
	 * @return The new cost the bridge has.
	 */
	static Money GetPrice(BridgeID bridge_id, uint length);

	/**
	 * Get the maximum length of a bridge.
	 * @param bridge_id The bridge to get the maximum length of.
	 * @pre IsValidBridge(bridge_id).
	 * @returns The maximum length the bridge has.
	 */
	static int32 GetMaxLength(BridgeID bridge_id);

	/**
	 * Get the minimum length of a bridge.
	 * @param bridge_id The bridge to get the minimum length of.
	 * @pre IsValidBridge(bridge_id).
	 * @returns The minimum length the bridge has.
	 */
	static int32 GetMinLength(BridgeID bridge_id);

#ifndef DOXYGEN_SKIP
	/**
	 * Internal function to help BuildBridge in case of road.
	 */
	static bool _BuildBridgeRoad1();

	/**
	 * Internal function to help BuildBridge in case of road.
	 */
	static bool _BuildBridgeRoad2();
#endif

	/**
	 * Build a bridge from one tile to the other.
	 * As an extra for road, this functions builds two half-pieces of road on
	 *  each end of the bridge, making it easier for you to connect it to your
	 *  network.
	 * @param vehicle_type The vehicle-type of bridge to build.
	 * @param bridge_id The bridge-type to build.
	 * @param start Where to start the bridge.
	 * @param end Where to end the bridge.
	 * @pre AIMap::IsValidTile(start).
	 * @pre AIMap::IsValidTile(end).
	 * @pre 'start' and 'end' are in a straight line, i.e.
	 *  AIMap::GetTileX(start) == AIMap::GetTileX(end) or
	 *  AIMap::GetTileY(start) == AIMap::GetTileY(end).
	 * @pre vehicle_type == AIVehicle::VT_ROAD || vehicle_type == AIVehicle::VT_WATER ||
	 *   (vehicle_type == AIVehicle::VT_RAIL && AIRail::IsRailTypeAvailable(AIRail::GetCurrentRailType())).
	 * @exception AIError::ERR_ALREADY_BUILT
	 * @exception AIError::ERR_AREA_NOT_CLEAR
	 * @exception AIError::ERR_LAND_SLOPED_WRONG
	 * @exception AIError::ERR_VEHICLE_IN_THE_WAY
	 * @exception AIBridge::ERR_BRIDGE_TYPE_UNAVAILABLE
	 * @exception AIBridge::ERR_BRIDGE_CANNOT_END_IN_WATER
	 * @exception AIBridge::ERR_BRIDGE_HEADS_NOT_ON_SAME_HEIGHT
	 * @return Whether the bridge has been/can be build or not.
	 * @note No matter if the road pieces were build or not, if building the
	 *  bridge succeeded, this function returns true.
	 */
	static bool BuildBridge(AIVehicle::VehicleType vehicle_type, BridgeID bridge_id, TileIndex start, TileIndex end);

	/**
	 * Removes a bridge, by executing it on either the start or end tile.
	 * @param tile An end or start tile of the bridge.
	 * @pre AIMap::IsValidTile(tile).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @return Whether the bridge has been/can be removed or not.
	 */
	static bool RemoveBridge(TileIndex tile);

	/**
	 * Get the tile that is on the other end of a bridge starting at tile.
	 * @param tile The tile that is an end of a bridge.
	 * @pre AIMap::IsValidTile(tile).
	 * @pre IsBridgeTile(tile).
	 * @return The TileIndex that is the other end of the bridge.
	 */
	static TileIndex GetOtherBridgeEnd(TileIndex tile);
};

#endif /* AI_BRIDGE_HPP */
