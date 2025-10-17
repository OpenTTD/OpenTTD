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


/* static */ SQInteger ScriptInfrastructure::GetRailPieceCount(ScriptCompany::CompanyID company, ScriptRail::RailType railtype)
{
	company = ScriptCompany::ResolveCompanyID(company);
	if (company == ScriptCompany::COMPANY_INVALID || (::RailType)railtype >= RAILTYPE_END) return 0;

	return ::Company::Get(ScriptCompany::FromScriptCompanyID(company))->infrastructure.GetRailCount(static_cast<::RailType>(railtype));
}

/* static */ SQInteger ScriptInfrastructure::GetRoadPieceCount(ScriptCompany::CompanyID company, ScriptRoad::RoadType roadtype)
{
	company = ScriptCompany::ResolveCompanyID(company);
	if (company == ScriptCompany::COMPANY_INVALID || (::RoadType)roadtype >= ROADTYPE_END) return 0;

	return ::Company::Get(ScriptCompany::FromScriptCompanyID(company))->infrastructure.GetRoadCount(static_cast<::RoadType>(roadtype));
}

/* static */ SQInteger ScriptInfrastructure::GetInfrastructurePieceCount(ScriptCompany::CompanyID company, Infrastructure infra_type)
{
	company = ScriptCompany::ResolveCompanyID(company);
	if (company == ScriptCompany::COMPANY_INVALID) return 0;

	const ::Company *c = ::Company::Get(ScriptCompany::FromScriptCompanyID(company));
	switch (infra_type) {
		case INFRASTRUCTURE_RAIL:
			return c->infrastructure.GetRailTotal();

		case INFRASTRUCTURE_SIGNALS:
			return c->infrastructure.signal;

		case INFRASTRUCTURE_ROAD:
			return c->infrastructure.GetRoadTotal() + c->infrastructure.GetTramTotal();

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
	::RailType rt = static_cast<::RailType>(railtype);
	company = ScriptCompany::ResolveCompanyID(company);
	if (company == ScriptCompany::COMPANY_INVALID || rt >= RAILTYPE_END || !_settings_game.economy.infrastructure_maintenance) return 0;

	const ::Company *c = ::Company::Get(ScriptCompany::FromScriptCompanyID(company));
	return ::RailMaintenanceCost(rt, c->infrastructure.GetRailCount(rt), c->infrastructure.GetRailTotal());
}

/* static */ Money ScriptInfrastructure::GetMonthlyRoadCosts(ScriptCompany::CompanyID company, ScriptRoad::RoadType roadtype)
{
	::RoadType rt = static_cast<::RoadType>(roadtype);
	company = ScriptCompany::ResolveCompanyID(company);
	if (company == ScriptCompany::COMPANY_INVALID || rt >= ROADTYPE_END || !_settings_game.economy.infrastructure_maintenance) return 0;

	const ::Company *c = ::Company::Get(ScriptCompany::FromScriptCompanyID(company));
	return ::RoadMaintenanceCost(rt, c->infrastructure.GetRoadCount(rt), RoadTypeIsRoad(rt) ? c->infrastructure.GetRoadTotal() : c->infrastructure.GetTramTotal());
}

/* static */ Money ScriptInfrastructure::GetMonthlyInfrastructureCosts(ScriptCompany::CompanyID company, Infrastructure infra_type)
{
	company = ScriptCompany::ResolveCompanyID(company);
	if (company == ScriptCompany::COMPANY_INVALID || !_settings_game.economy.infrastructure_maintenance) return 0;

	const ::Company *c = ::Company::Get(ScriptCompany::FromScriptCompanyID(company));
	switch (infra_type) {
		case INFRASTRUCTURE_RAIL: {
			Money cost;
			uint32_t rail_total = c->infrastructure.GetRailTotal();
			for (const auto &[railtype, count] : c->infrastructure.rail) {
				cost += RailMaintenanceCost(railtype, count, rail_total);
			}
			return cost;
		}

		case INFRASTRUCTURE_SIGNALS:
			return SignalMaintenanceCost(c->infrastructure.signal);

		case INFRASTRUCTURE_ROAD: {
			Money cost;
			uint32_t road_total = c->infrastructure.GetRoadTotal();
			uint32_t tram_total = c->infrastructure.GetTramTotal();
			for (const auto &[roadtype, count] : c->infrastructure.road) {
				cost += RoadMaintenanceCost(roadtype, count, RoadTypeIsRoad(roadtype) ? road_total : tram_total);
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
