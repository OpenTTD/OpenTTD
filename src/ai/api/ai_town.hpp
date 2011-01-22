/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_town.hpp Everything to query towns. */

#ifndef AI_TOWN_HPP
#define AI_TOWN_HPP

#include "ai_company.hpp"

/**
 * Class that handles all town related functions.
 */
class AITown : public AIObject {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AITown"; }

	/**
	 * Actions that one can perform on a town.
	 */
	enum TownAction {
		/* Values are important, as they represent the internal state of the game. */

		/**
		 * The cargo ratings temporary gains 25% of rating (in
		 * absolute percentage, so 10% becomes 35%, with a max of 99%)
		 * for all stations within 10 tiles.
		 */
		TOWN_ACTION_ADVERTISE_SMALL  = 0,

		/**
		 * The cargo ratings temporary gains 44% of rating (in
		 * absolute percentage, so 10% becomes 54%, with a max of 99%)
		 * for all stations within 15 tiles.
		 */
		TOWN_ACTION_ADVERTISE_MEDIUM = 1,

		/**
		 * The cargo ratings temporary gains 63% of rating (in
		 * absolute percentage, so 10% becomes 73%, with a max of 99%)
		 * for all stations within 20 tiles.
		 */
		TOWN_ACTION_ADVERTISE_LARGE  = 2,

		/**
		 * Rebuild the roads of this town for 6 months.
		 */
		TOWN_ACTION_ROAD_REBUILD     = 3,

		/**
		 * Build a statue in this town.
		 */
		TOWN_ACTION_BUILD_STATUE     = 4,

		/**
		 * Fund the creation of extra buildings for 3 months.
		 */
		TOWN_ACTION_FUND_BUILDINGS   = 5,

		/**
		 * Buy exclusive rights for this town for 12 months.
		 */
		TOWN_ACTION_BUY_RIGHTS       = 6,

		/**
		 * Bribe the town in order to get a higher rating.
		 */
		TOWN_ACTION_BRIBE            = 7,
	};

	/**
	 * Different ratings one could have in a town.
	 */
	enum TownRating {
		TOWN_RATING_NONE,         ///< The company got no rating in the town.
		TOWN_RATING_APPALLING,    ///< The company got an appalling rating in the town .
		TOWN_RATING_VERY_POOR,    ///< The company got an very poor rating in the town.
		TOWN_RATING_POOR,         ///< The company got an poor rating in the town.
		TOWN_RATING_MEDIOCRE,     ///< The company got an mediocre rating in the town.
		TOWN_RATING_GOOD,         ///< The company got an good rating in the town.
		TOWN_RATING_VERY_GOOD,    ///< The company got an very good rating in the town.
		TOWN_RATING_EXCELLENT,    ///< The company got an excellent rating in the town.
		TOWN_RATING_OUTSTANDING,  ///< The company got an outstanding rating in the town.
		TOWN_RATING_INVALID = -1, ///< The town rating for invalid towns/companies.
	};

	/**
	 * Possible layouts for the roads in a town.
	 */
	enum RoadLayout {
		/* Order IS important, as it matches an in-game value */
		ROAD_LAYOUT_ORIGINAL,     ///< Original algorithm (min. 1 distance between roads).
		ROAD_LAYOUT_BETTER_ROADS, ///< Extended original algorithm (min. 2 distance between roads).
		ROAD_LAYOUT_2x2,          ///< Geometric 2x2 grid algorithm
		ROAD_LAYOUT_3x3,          ///< Geometric 3x3 grid algorithm
		ROAD_LAYOUT_INVALID = -1, ///< The layout for invalid towns.
	};

	/**
	 * Gets the number of towns.
	 * @return The number of towns.
	 * @post Return value is always non-negative.
	 */
	static int32 GetTownCount();

	/**
	 * Checks whether the given town index is valid.
	 * @param town_id The index to check.
	 * @return True if and only if the town is valid.
	 */
	static bool IsValidTown(TownID town_id);

	/**
	 * Get the name of the town.
	 * @param town_id The town to get the name of.
	 * @pre IsValidTown(town_id).
	 * @return The name of the town.
	 */
	static char *GetName(TownID town_id);

	/**
	 * Gets the number of inhabitants in the town.
	 * @param town_id The town to get the population of.
	 * @pre IsValidTown(town_id).
	 * @return The number of inhabitants.
	 * @post Return value is always non-negative.
	 */
	static int32 GetPopulation(TownID town_id);

	/**
	 * Gets the number of houses in the town.
	 * @param town_id The town to get the number of houses of.
	 * @pre IsValidTown(town_id).
	 * @return The number of houses.
	 * @post Return value is always non-negative.
	 */
	static int32 GetHouseCount(TownID town_id);

	/**
	 * Gets the location of the town.
	 * @param town_id The town to get the location of.
	 * @pre IsValidTown(town_id).
	 * @return The location of the town.
	 */
	static TileIndex GetLocation(TownID town_id);

	/**
	 * Get the total last month's production of the given cargo at a town.
	 * @param town_id The index of the town.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidTown(town_id).
	 * @pre AICargo::IsValidCargo(cargo_id).
	 * @pre AICargo::GetTownEffect(cargo_id) == TE_PASSENGERS || AICargo::GetTownEffect(cargo_id) == TE_MAIL.
	 * @return The last month's production of the given cargo for this town.
	 * @post Return value is always non-negative.
	 */
	static int32 GetLastMonthProduction(TownID town_id, CargoID cargo_id);

	/**
	 * Get the total amount of cargo transported from a town last month.
	 * @param town_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidTown(town_id).
	 * @pre AICargo::IsValidCargo(cargo_id).
	 * @pre AICargo::GetTownEffect(cargo_id) == TE_PASSENGERS || AICargo::GetTownEffect(cargo_id) == TE_MAIL.
	 * @return The amount of given cargo transported from this town last month.
	 * @post Return value is always non-negative.
	 */
	static int32 GetLastMonthTransported(TownID town_id, CargoID cargo_id);

	/**
	 * Get the percentage of transported production of the given cargo at a town.
	 * @param town_id The index of the town.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidTown(town_id).
	 * @pre AICargo::IsValidCargo(cargo_id).
	 * @pre AICargo::GetTownEffect(cargo_id) == TE_PASSENGERS || AICargo::GetTownEffect(cargo_id) == TE_MAIL.
	 * @return The percentage of given cargo transported from this town last month.
	 * @post Return value is always non-negative.
	 */
	static int32 GetLastMonthTransportedPercentage(TownID town_id, CargoID cargo_id);

	/**
	 * Get the manhattan distance from the tile to the AITown::GetLocation()
	 *  of the town.
	 * @param town_id The town to get the distance to.
	 * @param tile The tile to get the distance to.
	 * @pre IsValidTown(town_id).
	 * @return The distance between town and tile.
	 */
	static int32 GetDistanceManhattanToTile(TownID town_id, TileIndex tile);

	/**
	 * Get the square distance from the tile to the AITown::GetLocation()
	 *  of the town.
	 * @param town_id The town to get the distance to.
	 * @param tile The tile to get the distance to.
	 * @pre IsValidTown(town_id).
	 * @return The distance between town and tile.
	 */
	static int32 GetDistanceSquareToTile(TownID town_id, TileIndex tile);

	/**
	 * Find out if this tile is within the rating influence of a town.
	 *  Stations on this tile influence the rating of the town.
	 * @param town_id The town to check.
	 * @param tile The tile to check.
	 * @pre IsValidTown(town_id).
	 * @return True if the tile is within the rating influence of the town.
	 */
	static bool IsWithinTownInfluence(TownID town_id, TileIndex tile);

	/**
	 * Find out if this town has a statue for the current company.
	 * @param town_id The town to check.
	 * @pre IsValidTown(town_id).
	 * @return True if the town has a statue.
	 */
	static bool HasStatue(TownID town_id);

	/**
	 * Find out if the town is a city.
	 * @param town_id The town to check.
	 * @pre IsValidTown(town_id).
	 * @return True if the town is a city.
	 */
	static bool IsCity(TownID town_id);

	/**
	 * Find out how long the town is undergoing road reconstructions.
	 * @param town_id The town to check.
	 * @pre IsValidTown(town_id).
	 * @return The number of months the road reworks are still going to take.
	 *         The value 0 means that there are currently no road reworks.
	 */
	static int GetRoadReworkDuration(TownID town_id);

	/**
	 * Find out which company currently has the exclusive rights of this town.
	 * @param town_id The town to check.
	 * @pre IsValidTown(town_id).
	 * @return The company that has the exclusive rights. The value
	 *         AICompany::COMPANY_INVALID means that there are currently no
	 *         exclusive rights given out to anyone.
	 */
	static AICompany::CompanyID GetExclusiveRightsCompany(TownID town_id);

	/**
	 * Find out how long the town is under influence of the exclusive rights.
	 * @param town_id The town to check.
	 * @pre IsValidTown(town_id).
	 * @return The number of months the exclusive rights hold.
	 *         The value 0 means that there are currently no exclusive rights
	 *         given out to anyone.
	 */
	static int32 GetExclusiveRightsDuration(TownID town_id);

	/**
	 * Find out if an action can currently be performed on the town.
	 * @param town_id The town to perform the action on.
	 * @param town_action The action to perform on the town.
	 * @pre IsValidTown(town_id).
	 * @return True if and only if the action can performed.
	 */
	static bool IsActionAvailable(TownID town_id, TownAction town_action);

	/**
	 * Perform a town action on this town.
	 * @param town_id The town to perform the action on.
	 * @param town_action The action to perform on the town.
	 * @pre IsValidTown(town_id).
	 * @pre IsActionAvailable(town_id, town_action).
	 * @return True if the action succeeded.
	 */
	static bool PerformTownAction(TownID town_id, TownAction town_action);

	/**
	 * Get the rating of a company within a town.
	 * @param town_id The town to get the rating for.
	 * @param company_id The company to get the rating for.
	 * @pre IsValidTown(town_id).
	 * @pre AICompany.ResolveCompanyID(company) != AICompany::COMPANY_INVALID.
	 * @return The rating as shown to humans.
	 */
	static TownRating GetRating(TownID town_id, AICompany::CompanyID company_id);

	/**
	 * Get the maximum level of noise that still can be added by airports
	 *  before the town start to refuse building a new airport.
	 * @param town_id The town to get the allowed noise from.
	 * @return The noise that still can be added.
	 */
	static int GetAllowedNoise(TownID town_id);

	/**
	 * Get the road layout for a town.
	 * @param town_id The town to get the road layout from.
	 * @return The RoadLayout for the town.
	 */
	static RoadLayout GetRoadLayout(TownID town_id);
};

#endif /* AI_TOWN_HPP */
