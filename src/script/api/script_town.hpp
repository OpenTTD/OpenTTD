/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_town.hpp Everything to query towns. */

#ifndef SCRIPT_TOWN_HPP
#define SCRIPT_TOWN_HPP

#include "script_cargo.hpp"
#include "script_company.hpp"
#include "../../town_type.h"

/**
 * Class that handles all town related functions.
 * @api ai game
 */
class ScriptTown : public ScriptObject {
public:
	/**
	 * Actions that one can perform on a town.
	 */
	enum TownAction {
		/* Note: these values represent part of the in-game order of the _town_action_proc array */

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
		/* Note: these values represent part of the in-game TownLayout enum */
		ROAD_LAYOUT_ORIGINAL      = ::TL_ORIGINAL,     ///< Original algorithm (min. 1 distance between roads).
		ROAD_LAYOUT_BETTER_ROADS  = ::TL_BETTER_ROADS, ///< Extended original algorithm (min. 2 distance between roads).
		ROAD_LAYOUT_2x2           = ::TL_2X2_GRID,     ///< Geometric 2x2 grid algorithm
		ROAD_LAYOUT_3x3           = ::TL_3X3_GRID,     ///< Geometric 3x3 grid algorithm
		ROAD_LAYOUT_RANDOM        = ::TL_RANDOM,       ///< Random road layout

		/* Custom added value, only valid for this API */
		ROAD_LAYOUT_INVALID       = -1,                ///< The layout for invalid towns.
	};

	/**
	 * Possible town construction sizes.
	 */
	enum TownSize {
		TOWN_SIZE_SMALL   = ::TSZ_SMALL,  ///< Small town.
		TOWN_SIZE_MEDIUM  = ::TSZ_MEDIUM, ///< Medium town.
		TOWN_SIZE_LARGE   = ::TSZ_LARGE,  ///< Large town.

		TOWN_SIZE_INVALID = -1,  ///< Invalid town size.
	};

	/**
	 * Special values for SetGrowthRate.
	 */
	enum TownGrowth {
		TOWN_GROWTH_NONE   = 0xFFFF,  ///< Town does not grow at all.
		TOWN_GROWTH_NORMAL = 0x10000, ///< Use default town growth algorithm instead of custom growth rate.
	};

	/**
	 * Gets the number of towns.
	 * @return The number of towns.
	 */
	static SQInteger GetTownCount();

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
	static std::optional<std::string> GetName(TownID town_id);

	/**
	 * Rename a town.
	 * @param town_id The town to rename
	 * @param name The new name of the town. If null, or an empty string, is passed, the town name will be reset to the default name.
	 * @pre IsValidTown(town_id).
	 * @pre ScriptCompanyMode::IsDeity().
	 * @return True if the action succeeded.
	 * @api -ai
	 */
	static bool SetName(TownID town_id, Text *name);

	/**
	 * Set the custom text of a town, shown in the GUI.
	 * @param town_id The town to set the custom text of.
	 * @param text The text to set it to (can be either a raw string, or a ScriptText object). If null, or an empty string, is passed, the text will be removed.
	 * @pre IsValidTown(town_id).
	 * @pre ScriptCompanyMode::IsDeity().
	 * @return True if the action succeeded.
	 * @api -ai
	 */
	static bool SetText(TownID town_id, Text *text);

	/**
	 * Gets the number of inhabitants in the town.
	 * @param town_id The town to get the population of.
	 * @pre IsValidTown(town_id).
	 * @return The number of inhabitants.
	 */
	static SQInteger GetPopulation(TownID town_id);

	/**
	 * Gets the number of houses in the town.
	 * @param town_id The town to get the number of houses of.
	 * @pre IsValidTown(town_id).
	 * @return The number of houses.
	 */
	static SQInteger GetHouseCount(TownID town_id);

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
	 * @pre ScriptCargo::IsValidCargo(cargo_id).
	 * @return The last month's production of the given cargo for this town.
	 */
	static SQInteger GetLastMonthProduction(TownID town_id, CargoID cargo_id);

	/**
	 * Get the total amount of cargo supplied from a town last month.
	 * @param town_id The index of the town.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidTown(town_id).
	 * @pre ScriptCargo::IsValidCargo(cargo_id).
	 * @return The amount of cargo supplied for transport from this town last month.
	 */
	static SQInteger GetLastMonthSupplied(TownID town_id, CargoID cargo_id);

	/**
	 * Get the percentage of transported production of the given cargo at a town.
	 * @param town_id The index of the town.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidTown(town_id).
	 * @pre ScriptCargo::IsValidCargo(cargo_id).
	 * @return The percentage of given cargo transported from this town last month.
	 */
	static SQInteger GetLastMonthTransportedPercentage(TownID town_id, CargoID cargo_id);

	/**
	 * Get the total amount of cargo effects received by a town last month.
	 * @param town_id The index of the town.
	 * @param towneffect_id The index of the cargo.
	 * @pre IsValidTown(town_id).
	 * @pre ScriptCargo::IsValidTownEffect(cargo_id).
	 * @return The amount of cargo received by this town last month for this cargo effect.
	 */
	static SQInteger GetLastMonthReceived(TownID town_id, ScriptCargo::TownEffect towneffect_id);

	/**
	 * Set the goal of a cargo for this town.
	 * @param town_id The index of the town.
	 * @param towneffect_id The index of the towneffect.
	 * @param goal The new goal.
	 *             The value will be clamped to 0 .. MAX(uint32_t).
	 * @pre IsValidTown(town_id).
	 * @pre ScriptCargo::IsValidTownEffect(towneffect_id).
	 * @pre ScriptCompanyMode::IsDeity().
	 * @return True if the action succeeded.
	 * @api -ai
	 */
	static bool SetCargoGoal(TownID town_id, ScriptCargo::TownEffect towneffect_id, SQInteger goal);

	/**
	 * Get the amount of cargo that needs to be delivered (per TownEffect) for a
	 *  town to grow. All goals need to be reached before a town will grow.
	 * @param town_id The index of the town.
	 * @param towneffect_id The index of the towneffect.
	 * @pre IsValidTown(town_id).
	 * @pre ScriptCargo::IsValidTownEffect(towneffect_id).
	 * @return The goal of the cargo.
	 * @note Goals can change over time. For example with a changing snowline, or
	 *  with a growing town.
	 */
	static SQInteger GetCargoGoal(TownID town_id, ScriptCargo::TownEffect towneffect_id);

	/**
	 * Set the amount of days between town growth.
	 * @param town_id The index of the town.
	 * @param days_between_town_growth The amount of days between town growth, TOWN_GROWTH_NONE or TOWN_GROWTH_NORMAL.
	 * @pre IsValidTown(town_id).
	 * @pre days_between_town_growth <= 880 || days_between_town_growth == TOWN_GROWTH_NONE || days_between_town_growth == TOWN_GROWTH_NORMAL.
	 * @return True if the action succeeded.
	 * @note Even when setting a growth rate, towns only grow when the conditions for growth (SetCargoCoal) are met,
	 *       and the game settings (economy.town_growth_rate) allow town growth at all.
	 * @note When changing the growth rate, the relative progress is preserved and scaled to the new rate.
	 * @api -ai
	 */
	static bool SetGrowthRate(TownID town_id, SQInteger days_between_town_growth);

	/**
	 * Get the amount of days between town growth.
	 * @param town_id The index of the town.
	 * @pre IsValidTown(town_id).
	 * @return Amount of days between town growth, or TOWN_GROWTH_NONE.
	 * @note This function does not indicate when it will grow next. It only tells you the time between growths.
	 */
	static SQInteger GetGrowthRate(TownID town_id);

	/**
	 * Get the manhattan distance from the tile to the ScriptTown::GetLocation()
	 *  of the town.
	 * @param town_id The town to get the distance to.
	 * @param tile The tile to get the distance to.
	 * @pre IsValidTown(town_id).
	 * @return The distance between town and tile.
	 */
	static SQInteger GetDistanceManhattanToTile(TownID town_id, TileIndex tile);

	/**
	 * Get the square distance from the tile to the ScriptTown::GetLocation()
	 *  of the town.
	 * @param town_id The town to get the distance to.
	 * @param tile The tile to get the distance to.
	 * @pre IsValidTown(town_id).
	 * @return The distance between town and tile.
	 */
	static SQInteger GetDistanceSquareToTile(TownID town_id, TileIndex tile);

	/**
	 * Find out if this tile is within the rating influence of a town.
	 *  If a station sign would be on this tile, the servicing quality of the station would
	 *  influence the rating of the town.
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
	 * @game @pre ScriptCompanyMode::IsValid().
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
	static SQInteger GetRoadReworkDuration(TownID town_id);

	/**
	 * Find out how long new buildings are still being funded in a town.
	 * @param town_id The town to check.
	 * @pre IsValidTown(town_id).
	 * @return The number of months building construction is still funded.
	 *         The value 0 means that there is currently no funding.
	 */
	static SQInteger GetFundBuildingsDuration(TownID town_id);

	/**
	 * Find out which company currently has the exclusive rights of this town.
	 * @param town_id The town to check.
	 * @pre IsValidTown(town_id).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return The company that has the exclusive rights. The value
	 *         ScriptCompany::COMPANY_INVALID means that there are currently no
	 *         exclusive rights given out to anyone.
	 */
	static ScriptCompany::CompanyID GetExclusiveRightsCompany(TownID town_id);

	/**
	 * Find out how long the town is under influence of the exclusive rights.
	 * @param town_id The town to check.
	 * @pre IsValidTown(town_id).
	 * @return The number of months the exclusive rights hold.
	 *         The value 0 means that there are currently no exclusive rights
	 *         given out to anyone.
	 */
	static SQInteger GetExclusiveRightsDuration(TownID town_id);

	/**
	 * Find out if an action can currently be performed on the town.
	 * @param town_id The town to perform the action on.
	 * @param town_action The action to perform on the town.
	 * @pre IsValidTown(town_id).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return True if and only if the action can performed.
	 */
	static bool IsActionAvailable(TownID town_id, TownAction town_action);

	/**
	 * Perform a town action on this town.
	 * @param town_id The town to perform the action on.
	 * @param town_action The action to perform on the town.
	 * @pre IsValidTown(town_id).
	 * @pre IsActionAvailable(town_id, town_action).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return True if the action succeeded.
	 */
	static bool PerformTownAction(TownID town_id, TownAction town_action);

	/**
	 * Expand the town.
	 * @param town_id The town to expand.
	 * @param houses The amount of houses to grow the town with.
	 *               The value will be clamped to 0 .. MAX(uint32_t).
	 * @pre IsValidTown(town_id).
	 * @pre houses > 0.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @return True if the action succeeded.
	 * @api -ai
	 */
	static bool ExpandTown(TownID town_id, SQInteger houses);

	/**
	 * Found a new town.
	 * @param tile The location of the new town.
	 * @param size The town size of the new town.
	 * @param city True if the new town should be a city.
	 * @param layout The town layout of the new town.
	 * @param name The name of the new town. Pass null, or an empty string, to use a random town name.
	 * @game @pre ScriptCompanyMode::IsDeity() || ScriptSettings.GetValue("economy.found_town") != 0.
	 * @ai @pre ScriptSettings.GetValue("economy.found_town") != 0.
	 * @game @pre ScriptCompanyMode::IsDeity() || size != TOWN_SIZE_LARGE.
	 * @ai @pre size != TOWN_SIZE_LARGE.
	 * @pre size != TOWN_SIZE_INVALID.
	 * @pre layout != ROAD_LAYOUT_INVALID.
	 * @return True if the action succeeded.
	 * @game @note Companies are restricted by the advanced setting that controls if funding towns is allowed or not. If custom road layout is forbidden and there is a company mode in scope (ScriptCompanyMode::IsValid()), the layout parameter will be ignored.
	 * @ai @note AIs are restricted by the advanced setting that controls if funding towns is allowed or not. If custom road layout is forbidden, the layout parameter will be ignored.
	 */
	static bool FoundTown(TileIndex tile, TownSize size, bool city, RoadLayout layout, Text *name);

	/**
	 * Get the rating of a company within a town.
	 * @param town_id The town to get the rating for.
	 * @param company_id The company to get the rating for.
	 * @pre IsValidTown(town_id).
	 * @pre ScriptCompany.ResolveCompanyID(company) != ScriptCompany::COMPANY_INVALID.
	 * @return The rating as shown to humans.
	 */
	static TownRating GetRating(TownID town_id, ScriptCompany::CompanyID company_id);

	/**
	 * Get the accurate rating of a company within a town.
	 * @param town_id The town to get the rating for.
	 * @param company_id The company to get the rating for.
	 * @pre IsValidTown(town_id).
	 * @pre ScriptCompany.ResolveCompanyID(company) != ScriptCompany::COMPANY_INVALID.
	 * @return The rating as a number between -1000 (worst) and 1000 (best).
	 * @api -ai
	 */
	static SQInteger GetDetailedRating(TownID town_id, ScriptCompany::CompanyID company_id);

	/**
	 * Change the rating of a company within a town.
	 * @param town_id The town to change the rating in.
	 * @param company_id The company to change the rating for.
	 * @param delta How much to change rating by (range -1000 to +1000).
	 * @return True if the rating was changed.
	 * @pre IsValidTown(town_id).
	 * @pre ScriptCompany.ResolveCompanyID(company) != ScriptCompany::COMPANY_INVALID.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @api -ai
	 */
	static bool ChangeRating(TownID town_id, ScriptCompany::CompanyID company_id, SQInteger delta);

	/**
	 * Get the maximum level of noise that still can be added by airports
	 *  before the town start to refuse building a new airport.
	 * @param town_id The town to get the allowed noise from.
	 * @return The noise that still can be added.
	 */
	static SQInteger GetAllowedNoise(TownID town_id);

	/**
	 * Get the road layout for a town.
	 * @param town_id The town to get the road layout from.
	 * @return The RoadLayout for the town.
	 */
	static RoadLayout GetRoadLayout(TownID town_id);
};

#endif /* SCRIPT_TOWN_HPP */
