/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_infrastructure.hpp Everything to query a company's infrastructure. */

#ifndef SCRIPT_INFRASTRUCTURE_HPP
#define SCRIPT_INFRASTRUCTURE_HPP

#include "script_road.hpp"
#include "script_rail.hpp"

/**
 * Class that handles all company infrastructure related functions.
 * @api ai game
 */
class ScriptInfrastructure : public ScriptObject {
public:
	/** Infrastructure categories. */
	enum Infrastructure {
		INFRASTRUCTURE_RAIL,    ///< Rail infrastructure.
		INFRASTRUCTURE_SIGNALS, ///< Signal infrastructure.
		INFRASTRUCTURE_ROAD,    ///< Road infrastructure.
		INFRASTRUCTURE_CANAL,   ///< Canal infrastructure.
		INFRASTRUCTURE_STATION, ///< Station infrastructure.
		INFRASTRUCTURE_AIRPORT, ///< Airport infrastructure.
	};

	/**
	 * Return the number of rail pieces of a specific rail type for a company.
	 * @param company The company to get the count for.
	 * @param railtype Rail type to get the count of.
	 * @return Count for the rail type.
	 */
	static SQInteger GetRailPieceCount(ScriptCompany::CompanyID company, ScriptRail::RailType railtype);

	/**
	 * Return the number of road pieces of a specific road type for a company.
	 * @param company The company to get the count for.
	 * @param roadtype Road type to get the count of.
	 * @return Count for the road type.
	 */
	static SQInteger GetRoadPieceCount(ScriptCompany::CompanyID company, ScriptRoad::RoadType roadtype);

	/**
	 * Return the number of pieces of an infrastructure category for a company.
	 * @param company The company to get the count for.
	 * @param infra_type Infrastructure category to get the cost of.
	 * @return Count for the wanted category.
	 * @note #INFRASTRUCTURE_RAIL and #INFRASTRUCTURE_ROAD return the total count for all rail/road types.
	 */
	static SQInteger GetInfrastructurePieceCount(ScriptCompany::CompanyID company, Infrastructure infra_type);

	/**
	 * Return the monthly maintenance costs of a specific rail type for a company.
	 * @param company The company to get the monthly cost for.
	 * @param railtype Rail type to get the cost of.
	 * @return Maintenance cost for the rail type per economy-month.
	 * @see \ref ScriptEconomyTime
	 */
	static Money GetMonthlyRailCosts(ScriptCompany::CompanyID company, ScriptRail::RailType railtype);

	/**
	 * Return the monthly maintenance costs of a specific road type for a company.
	 * @param company The company to get the monthly cost for.
	 * @param roadtype Road type to get the cost of.
	 * @return Maintenance cost for the road type per economy-month.
	 * @see \ref ScriptEconomyTime
	 */
	static Money GetMonthlyRoadCosts(ScriptCompany::CompanyID company, ScriptRoad::RoadType roadtype);

	/**
	 * Return the monthly maintenance costs of an infrastructure category for a company.
	 * @param company The company to get the monthly cost for.
	 * @param infra_type Infrastructure category to get the cost of.
	 * @return Maintenance cost for the wanted category per economy-month.
	 * @note #INFRASTRUCTURE_RAIL and #INFRASTRUCTURE_ROAD return the total cost for all rail/road types.
	 * @see \ref ScriptEconomyTime
	 */
	static Money GetMonthlyInfrastructureCosts(ScriptCompany::CompanyID company, Infrastructure infra_type);
};

#endif /* SCRIPT_INFRASTRUCTURE_HPP */
