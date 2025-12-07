/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rail.cpp Implementation of rail specific functions. */

#include "stdafx.h"
#include "station_map.h"
#include "tunnelbridge_map.h"
#include "timer/timer_game_calendar.h"
#include "company_func.h"
#include "company_base.h"
#include "engine_base.h"

#include "table/track_data.h"

#include "safeguards.h"

/**
 * Get the RailType for this RailTypeInfo.
 * @return RailType in static RailTypeInfo definitions.
 */
RailType RailTypeInfo::Index() const
{
	extern RailTypeInfo _railtypes[RAILTYPE_END];
	size_t index = this - _railtypes;
	assert(index < RAILTYPE_END);
	return static_cast<RailType>(index);
}

/**
 * Return the rail type of tile, or INVALID_RAILTYPE if this is no rail tile.
 */
RailType GetTileRailType(Tile tile)
{
	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			return GetRailType(tile);

		case MP_ROAD:
			/* rail/road crossing */
			if (IsLevelCrossing(tile)) return GetRailType(tile);
			break;

		case MP_STATION:
			if (HasStationRail(tile)) return GetRailType(tile);
			break;

		case MP_TUNNELBRIDGE:
			if (GetTunnelBridgeTransportType(tile) == TRANSPORT_RAIL) return GetRailType(tile);
			break;

		default:
			break;
	}
	return INVALID_RAILTYPE;
}

/**
 * Finds out if a company has a certain buildable railtype available.
 * @param company the company in question
 * @param railtype requested RailType
 * @return true if company has requested RailType available
 */
bool HasRailTypeAvail(const CompanyID company, const RailType railtype)
{
	return !_railtypes_hidden_mask.Test(railtype) && Company::Get(company)->avail_railtypes.Test(railtype);
}

/**
 * Test if any buildable railtype is available for a company.
 * @param company the company in question
 * @return true if company has any RailTypes available
 */
bool HasAnyRailTypesAvail(const CompanyID company)
{
	RailTypes avail = Company::Get(company)->avail_railtypes;
	avail.Reset(_railtypes_hidden_mask);
	return avail.Any();
}

/**
 * Validate functions for rail building.
 * @param rail the railtype to check.
 * @return true if the current company may build the rail.
 */
bool ValParamRailType(const RailType rail)
{
	return rail < RAILTYPE_END && HasRailTypeAvail(_current_company, rail);
}

/**
 * Add the rail types that are to be introduced at the given date.
 * @param current The currently available railtypes.
 * @param date    The date for the introduction comparisons.
 * @return The rail types that should be available when date
 *         introduced rail types are taken into account as well.
 */
RailTypes AddDateIntroducedRailTypes(RailTypes current, TimerGameCalendar::Date date)
{
	RailTypes rts = current;

	for (RailType rt = RAILTYPE_BEGIN; rt != RAILTYPE_END; rt++) {
		const RailTypeInfo *rti = GetRailTypeInfo(rt);
		/* Unused rail type. */
		if (rti->label == 0) continue;

		/* Not date introduced. */
		if (!IsInsideMM(rti->introduction_date, 0, CalendarTime::MAX_DATE.base())) continue;

		/* Not yet introduced at this date. */
		if (rti->introduction_date > date) continue;

		/* Have we introduced all required railtypes? */
		RailTypes required = rti->introduction_required_railtypes;
		if (!rts.All(required)) continue;

		rts.Set(rti->introduces_railtypes);
	}

	/* When we added railtypes we need to run this method again; the added
	 * railtypes might enable more rail types to become introduced. */
	return rts == current ? rts : AddDateIntroducedRailTypes(rts, date);
}

/**
 * Get the rail types the given company can build.
 * @param company the company to get the rail types for.
 * @param introduces If true, include rail types introduced by other rail types
 * @return the rail types.
 */
RailTypes GetCompanyRailTypes(CompanyID company, bool introduces)
{
	RailTypes rts{};

	for (const Engine *e : Engine::IterateType(VEH_TRAIN)) {
		const EngineInfo *ei = &e->info;

		if (ei->climates.Test(_settings_game.game_creation.landscape) &&
				(e->company_avail.Test(company) || TimerGameCalendar::date >= e->intro_date + CalendarTime::DAYS_IN_YEAR)) {
			const RailVehicleInfo *rvi = &e->VehInfo<RailVehicleInfo>();

			if (rvi->railveh_type != RAILVEH_WAGON) {
				assert(rvi->railtypes.Any());
				if (introduces) {
					rts.Set(GetAllIntroducesRailTypes(rvi->railtypes));
				} else {
					rts.Set(rvi->railtypes);
				}
			}
		}
	}

	if (introduces) return AddDateIntroducedRailTypes(rts, TimerGameCalendar::date);
	return rts;
}

/**
 * Get list of rail types, regardless of company availability.
 * @param introduces If true, include rail types introduced by other rail types
 * @return the rail types.
 */
RailTypes GetRailTypes(bool introduces)
{
	RailTypes rts{};

	for (const Engine *e : Engine::IterateType(VEH_TRAIN)) {
		const EngineInfo *ei = &e->info;
		if (!ei->climates.Test(_settings_game.game_creation.landscape)) continue;

		const RailVehicleInfo *rvi = &e->VehInfo<RailVehicleInfo>();
		if (rvi->railveh_type != RAILVEH_WAGON) {
			assert(rvi->railtypes.Any());
			if (introduces) {
				rts.Set(GetAllIntroducesRailTypes(rvi->railtypes));
			} else {
				rts.Set(rvi->railtypes);
			}
		}
	}

	if (introduces) return AddDateIntroducedRailTypes(rts, CalendarTime::MAX_DATE);
	return rts;
}

/**
 * Get the rail type for a given label.
 * @param label the railtype label.
 * @param allow_alternate_labels Search in the alternate label lists as well.
 * @return the railtype.
 */
RailType GetRailTypeByLabel(RailTypeLabel label, bool allow_alternate_labels)
{
	extern RailTypeInfo _railtypes[RAILTYPE_END];
	if (label == 0) return INVALID_RAILTYPE;

	auto it = std::ranges::find(_railtypes, label, &RailTypeInfo::label);
	if (it == std::end(_railtypes) && allow_alternate_labels) {
		/* Test if any rail type defines the label as an alternate. */
		it = std::ranges::find_if(_railtypes, [label](const RailTypeInfo &rti) { return rti.alternate_labels.contains(label); });
	}

	if (it != std::end(_railtypes)) return it->Index();

	/* No matching label was found, so it is invalid */
	return INVALID_RAILTYPE;
}
