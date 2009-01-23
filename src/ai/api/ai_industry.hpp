/* $Id$ */

/** @file ai_industry.hpp Everything to query and build industries. */

#ifndef AI_INDUSTRY_HPP
#define AI_INDUSTRY_HPP

#include "ai_object.hpp"

/**
 * Class that handles all industry related functions.
 */
class AIIndustry : public AIObject {
public:
	static const char *GetClassName() { return "AIIndustry"; }

	/**
	 * Gets the maximum industry index; there are no valid industries with a
	 *   higher index.
	 * @return The maximum industry index.
	 * @post Return value is always non-negative.
	 */
	static IndustryID GetMaxIndustryID();

	/**
	 * Gets the number of industries. This is different than GetMaxIndustryID()
	 *   because of the way OpenTTD works internally.
	 * @return The number of industries.
	 * @post Return value is always non-negative.
	 */
	static int32 GetIndustryCount();

	/**
	 * Checks whether the given industry index is valid.
	 * @param industry_id The index to check.
	 * @return True if and only if the industry is valid.
	 */
	static bool IsValidIndustry(IndustryID industry_id);

	/**
	 * Get the name of the industry.
	 * @param industry_id The industry to get the name of.
	 * @pre IsValidIndustry(industry_id).
	 * @return The name of the industry.
	 */
	static char *GetName(IndustryID industry_id);

	/**
	 * Gets the production of a cargo of the industry.
	 * WARNING This function does not neccessarily return useful values for newindustries.
	 *         An industry produces at least the returned amount per month,
	 *         but the function can also return 0, when the industry produces lots of cargo.
	 *         GetLastMonthProduction() is more robust.
	 * @param industry_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidIndustry(industry_id).
	 * @pre AICargo::IsValidCargo(cargo_id).
	 * @return The production of the cargo for this industry, or -1 if
	 *   this industry doesn't produce this cargo.
	 */
	static int32 GetProduction(IndustryID industry_id, CargoID cargo_id);

	/**
	 * See if an industry accepts a certain cargo.
	 * @param industry_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidIndustry(industry_id).
	 * @pre AICargo::IsValidCargo(cargo_id).
	 * @return True if and only if the industry accepts the cargo.
	 */
	static bool IsCargoAccepted(IndustryID industry_id, CargoID cargo_id);

	/**
	 * Get the amount of cargo stockpiled for processing.
	 * @param industry_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidIndustry(industry_id).
	 * @pre AICargo::IsValidCargo(cargo_id).
	 * @return The amount of cargo that is waiting for processing.
	 */
	static int32 GetStockpiledCargo(IndustryID industry_id, CargoID cargo_id);

	/**
	 * Get the total last month's production of the given cargo at an industry.
	 * @param industry_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidIndustry(industry_id).
	 * @pre AICargo::IsValidCargo(cargo_id).
	 * @return The last month's production of the given cargo for this industry.
	 */
	static int32 GetLastMonthProduction(IndustryID industry_id, CargoID cargo_id);

	/**
	 * Get the total amount of cargo transported from an industry last month.
	 * @param industry_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidIndustry(industry_id).
	 * @pre AICargo::IsValidCargo(cargo_id).
	 * @return The amount of given cargo transported from this industry last month.
	 */
	static int32 GetLastMonthTransported(IndustryID industry_id, CargoID cargo_id);

	/**
	 * Gets the location of the industry.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return The location of the industry.
	 */
	static TileIndex GetLocation(IndustryID industry_id);

	/**
	 * Get the number of stations around an industry.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return The number of stations around an industry.
	 */
	static int32 GetAmountOfStationsAround(IndustryID industry_id);

	/**
	 * Get the manhattan distance from the tile to the AIIndustry::GetLocation()
	 *  of the industry.
	 * @param industry_id The industry to get the distance to.
	 * @param tile The tile to get the distance to.
	 * @pre IsValidIndustry(industry_id).
	 * @pre AIMap::IsValidTile(tile).
	 * @return The distance between industry and tile.
	 */
	static int32 GetDistanceManhattanToTile(IndustryID industry_id, TileIndex tile);

	/**
	 * Get the square distance from the tile to the AIIndustry::GetLocation()
	 *  of the industry.
	 * @param industry_id The industry to get the distance to.
	 * @param tile The tile to get the distance to.
	 * @pre IsValidIndustry(industry_id).
	 * @pre AIMap::IsValidTile(tile).
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
	 * Does this industry have a heliport and dock?
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return True when the industry has a heliport and dock.
	 */
	static bool HasHeliportAndDock(IndustryID industry_id);

	/**
	 * Gets the location of the industry's heliport/dock.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @pre HasHeliportAndDock(industry_id).
	 * @return The location of the industry's heliport/dock.
	 */
	static TileIndex GetHeliportAndDockLocation(IndustryID industry_id);

	/**
	 * Get the IndustryType of the industry.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return The IndustryType of the industry.
	 */
	static IndustryType GetIndustryType(IndustryID industry_id);
};

#endif /* AI_INDUSTRY_HPP */
