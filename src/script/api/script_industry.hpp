/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_industry.hpp Everything to query and build industries. */

#ifndef SCRIPT_INDUSTRY_HPP
#define SCRIPT_INDUSTRY_HPP

#include "script_object.hpp"

/**
 * Class that handles all industry related functions.
 * @api ai game
 */
class ScriptIndustry : public ScriptObject {
public:
	/** Ways for an industry to accept a cargo. */
	enum CargoAcceptState {
		CAS_NOT_ACCEPTED, ///< The CargoID is not accepted by this industry.
		CAS_ACCEPTED,     ///< The industry currently accepts this CargoID.
		CAS_TEMP_REFUSED, ///< The industry temporarily refuses to accept this CargoID but may do so again in the future.
	};

	/**
	 * Gets the number of industries.
	 * @return The number of industries.
	 * @note The maximum valid IndustryID can be higher than the value returned.
	 */
	static int32 GetIndustryCount();

	/**
	 * Checks whether the given industry index is valid.
	 * @param industry_id The index to check.
	 * @return True if and only if the industry is valid.
	 */
	static bool IsValidIndustry(IndustryID industry_id);

	/**
	 * Get the IndustryID of a tile, if there is an industry.
	 * @param tile The tile to find the IndustryID of.
	 * @return IndustryID of the industry.
	 * @post Use IsValidIndustry() to see if the industry is valid.
	 * @note GetIndustryID will return an invalid IndustryID for the
	 *   station tile of industries with a dock/heliport.
	 */
	static IndustryID GetIndustryID(TileIndex tile);

	/**
	 * Get the name of the industry.
	 * @param industry_id The industry to get the name of.
	 * @pre IsValidIndustry(industry_id).
	 * @return The name of the industry.
	 */
	static char *GetName(IndustryID industry_id);

	/**
	 * See whether an industry currently accepts a certain cargo.
	 * @param industry_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptCargo::IsValidCargo(cargo_id).
	 * @return Whether the industry accepts, temporarily refuses or never accepts this cargo.
	 */
	static CargoAcceptState IsCargoAccepted(IndustryID industry_id, CargoID cargo_id);

	/**
	 * Get the amount of cargo stockpiled for processing.
	 * @param industry_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptCargo::IsValidCargo(cargo_id).
	 * @return The amount of cargo that is waiting for processing.
	 */
	static int32 GetStockpiledCargo(IndustryID industry_id, CargoID cargo_id);

	/**
	 * Get the total last month's production of the given cargo at an industry.
	 * @param industry_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptCargo::IsValidCargo(cargo_id).
	 * @return The last month's production of the given cargo for this industry.
	 */
	static int32 GetLastMonthProduction(IndustryID industry_id, CargoID cargo_id);

	/**
	 * Get the total amount of cargo transported from an industry last month.
	 * @param industry_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptCargo::IsValidCargo(cargo_id).
	 * @return The amount of given cargo transported from this industry last month.
	 */
	static int32 GetLastMonthTransported(IndustryID industry_id, CargoID cargo_id);

	/**
	 * Get the percentage of cargo transported from an industry last month.
	 * @param industry_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptCargo::IsValidCargo(cargo_id).
	 * @return The percentage of given cargo transported from this industry last month.
	 */
	static int32 GetLastMonthTransportedPercentage(IndustryID industry_id, CargoID cargo_id);

	/**
	 * Gets the location of the industry.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return The location of the industry.
	 */
	static TileIndex GetLocation(IndustryID industry_id);

	/**
	 * Get the number of stations around an industry. All stations that can
	 * service the industry are counted, your own stations but also your
	 * opponents stations.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return The number of stations around an industry.
	 */
	static int32 GetAmountOfStationsAround(IndustryID industry_id);

	/**
	 * Get the manhattan distance from the tile to the ScriptIndustry::GetLocation()
	 *  of the industry.
	 * @param industry_id The industry to get the distance to.
	 * @param tile The tile to get the distance to.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptMap::IsValidTile(tile).
	 * @return The distance between industry and tile.
	 */
	static int32 GetDistanceManhattanToTile(IndustryID industry_id, TileIndex tile);

	/**
	 * Get the square distance from the tile to the ScriptIndustry::GetLocation()
	 *  of the industry.
	 * @param industry_id The industry to get the distance to.
	 * @param tile The tile to get the distance to.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptMap::IsValidTile(tile).
	 * @return The distance between industry and tile.
	 */
	static int32 GetDistanceSquareToTile(IndustryID industry_id, TileIndex tile);

	/**
	 * Is this industry built on water.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return True when the industry is built on water.
	 */
	static bool IsBuiltOnWater(IndustryID industry_id);

	/**
	 * Does this industry have a heliport?
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return True when the industry has a heliport.
	 */
	static bool HasHeliport(IndustryID industry_id);

	/**
	 * Gets the location of the industry's heliport.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @pre HasHeliport(industry_id).
	 * @return The location of the industry's heliport.
	 */
	static TileIndex GetHeliportLocation(IndustryID industry_id);

	/**
	 * Does this industry have a dock?
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return True when the industry has a dock.
	 */
	static bool HasDock(IndustryID industry_id);

	/**
	 * Gets the location of the industry's dock.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @pre HasDock(industry_id).
	 * @return The location of the industry's dock.
	 */
	static TileIndex GetDockLocation(IndustryID industry_id);

	/**
	 * Get the IndustryType of the industry.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return The IndustryType of the industry.
	 */
	static IndustryType GetIndustryType(IndustryID industry_id);
};

#endif /* SCRIPT_INDUSTRY_HPP */
