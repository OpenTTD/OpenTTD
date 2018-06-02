/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_infrastructure.cpp Implementation of ScriptInfrastructure. */

#include "../../stdafx.h"
#include "script_infrastructure.hpp"
#include "../../company_base.h"
#include "../../rail.h"
#include "../../road_func.h"
#include "../../water.h"
#include "../../station_func.h"

#include "../../safeguards.h"


/* static */ uint32 ScriptInfrastructure::GetRailPieceCount(ScriptCompany::CompanyID company, ScriptRail::RailType railtype)
{
	company = ScriptCompany::ResolveCompanyID(company);
	if (company == ScriptCompany::COMPANY_INVALID || (::RailType)railtype >= RAILTYPE_END) return 0;

	return ::Company::Get((::CompanyID)company)->infrastructure.rail[railtype];
}

/* static */ uint32 ScriptInfrastructure::GetRoadPieceCount(ScriptCompany::CompanyID company, ScriptRoad::RoadType roadtype)
{
	company = ScriptCompany::ResolveCompanyID(company);
	if (company == ScriptCompany::COMPANY_INVALID || (::RoadType)roadtype >= ROADTYPE_END) return 0;

	return ::Company::Get((::CompanyID)company)->infrastructure.GetRoadTotal((::RoadType)roadtype); // TODO
}

/* static */ uint32 ScriptInfrastructure::GetInfrastructurePieceCount(ScriptCompany::CompanyID company, Infrastructure infra_type)
{
	company = ScriptCompany::ResolveCompanyID(company);
	if (company == ScriptCompany::COMPANY_INVALID) return 0;

	::Company *c = ::Company::Get((::CompanyID)company);
	switch (infra_type) {
		case INFRASTRUCTURE_RAIL: {
			uint32 count = 0;
			for (::RailType rt = ::RAILTYPE_BEGIN; rt != ::RAILTYPE_END; rt++) {
				count += c->infrastructure.rail[rt];
			}
			return count;
		}

		case INFRASTRUCTURE_SIGNALS:
			return c->infrastructure.signal;

		case INFRASTRUCTURE_ROAD: {
			uint32 count = 0;
			for (::RoadType rt = ::ROADTYPE_BEGIN; rt != ::ROADTYPE_END; rt++) {
				count += c->infrastructure.GetRoadTotal(rt);
			}
			return count;
		}

		case INFRASTRUCTURE_CANAL:
			return c->infrastructure.water;

		case INFRASTRUCTURE_STATION:
			return c->infrastructure.station;

		case INFRASTRUCTURE_AIRPORT:
			return c->infrastructure.airport;

		default:
			return 0;
	}
}

/* static */ Money ScriptInfrastructure::GetMonthlyRailCosts(ScriptCompany::CompanyID company, ScriptRail::RailType railtype)
{
	company = ScriptCompany::ResolveCompanyID(company);
	if (company == ScriptCompany::COMPANY_INVALID || (::RailType)railtype >= RAILTYPE_END || !_settings_game.economy.infrastructure_maintenance) return 0;

	const ::Company *c = ::Company::Get((::CompanyID)company);
	return ::RailMaintenanceCost((::RailType)railtype, c->infrastructure.rail[railtype], c->infrastructure.GetRailTotal());
}

/* static */ Money ScriptInfrastructure::GetMonthlyRoadCosts(ScriptCompany::CompanyID company, ScriptRoad::RoadType roadtype)
{
	company = ScriptCompany::ResolveCompanyID(company);
	if (company == ScriptCompany::COMPANY_INVALID || (::RoadType)roadtype >= ROADTYPE_END || !_settings_game.economy.infrastructure_maintenance) return 0;

	const ::Company *c = ::Company::Get((::CompanyID)company);
	uint32 total_count = c->infrastructure.GetRoadTotal((::RoadType)roadtype);
	Money cost;
	for (RoadSubType rst = ROADSUBTYPE_BEGIN; rst < ROADSUBTYPE_END; rst++) {
		if (c->infrastructure.road[(::RoadType)roadtype][rst] != 0) {
			cost += RoadMaintenanceCost(RoadTypeIdentifier((::RoadType)roadtype, rst), c->infrastructure.road[(::RoadType)roadtype][rst], total_count);
		}
	}

	return cost;
}

/* static */ Money ScriptInfrastructure::GetMonthlyInfrastructureCosts(ScriptCompany::CompanyID company, Infrastructure infra_type)
{
	company = ScriptCompany::ResolveCompanyID(company);
	if (company == ScriptCompany::COMPANY_INVALID || !_settings_game.economy.infrastructure_maintenance) return 0;

	::Company *c = ::Company::Get((::CompanyID)company);
	switch (infra_type) {
		case INFRASTRUCTURE_RAIL: {
			Money cost;
			uint32 rail_total = c->infrastructure.GetRailTotal();
			for (::RailType rt = ::RAILTYPE_BEGIN; rt != ::RAILTYPE_END; rt++) {
				cost += RailMaintenanceCost(rt, c->infrastructure.rail[rt], rail_total);
			}
			return cost;
		}

		case INFRASTRUCTURE_SIGNALS:
			return SignalMaintenanceCost(c->infrastructure.signal);

		case INFRASTRUCTURE_ROAD: {
			Money cost;
			for (::RoadType rt = ::ROADTYPE_BEGIN; rt != ::ROADTYPE_END; rt++) {
				uint32 total_count = c->infrastructure.GetRoadTotal(rt);
				for (RoadSubType rst = ROADSUBTYPE_BEGIN; rst < ROADSUBTYPE_END; rst++) {
					if (c->infrastructure.road[rt][rst] != 0) {
						cost += RoadMaintenanceCost(RoadTypeIdentifier(rt, rst), c->infrastructure.road[rt][rst], total_count);
					}
				}
			}
			return cost;
		}

		case INFRASTRUCTURE_CANAL:
			return CanalMaintenanceCost(c->infrastructure.water);

		case INFRASTRUCTURE_STATION:
			return StationMaintenanceCost(c->infrastructure.station);

		case INFRASTRUCTURE_AIRPORT:
			return AirportMaintenanceCost(c->index);

		default:
			return 0;
	}
}
