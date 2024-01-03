/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road.cpp Generic road related functions. */

#include "stdafx.h"
#include "rail_map.h"
#include "road_map.h"
#include "water_map.h"
#include "genworld.h"
#include "company_func.h"
#include "company_base.h"
#include "engine_base.h"
#include "timer/timer_game_calendar.h"
#include "landscape.h"
#include "road.h"
#include "road_func.h"
#include "roadveh.h"

#include "safeguards.h"

/**
 * Return if the tile is a valid tile for a crossing.
 *
 * @param tile the current tile
 * @param ax the axis of the road over the rail
 * @return true if it is a valid tile
 */
static bool IsPossibleCrossing(const TileIndex tile, Axis ax)
{
	return (IsTileType(tile, MP_RAILWAY) &&
		GetRailTileType(tile) == RAIL_TILE_NORMAL &&
		GetTrackBits(tile) == (ax == AXIS_X ? TRACK_BIT_Y : TRACK_BIT_X) &&
		GetFoundationSlope(tile) == SLOPE_FLAT);
}

/**
 * Clean up unnecessary RoadBits of a planned tile.
 * @param tile current tile
 * @param org_rb planned RoadBits
 * @return optimised RoadBits
 */
RoadBits CleanUpRoadBits(const TileIndex tile, RoadBits org_rb)
{
	if (!IsValidTile(tile)) return ROAD_NONE;
	for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
		const TileIndex neighbor_tile = TileAddByDiagDir(tile, dir);

		/* Get the Roadbit pointing to the neighbor_tile */
		const RoadBits target_rb = DiagDirToRoadBits(dir);

		/* If the roadbit is in the current plan */
		if (org_rb & target_rb) {
			bool connective = false;
			const RoadBits mirrored_rb = MirrorRoadBits(target_rb);

			if (IsValidTile(neighbor_tile)) {
				switch (GetTileType(neighbor_tile)) {
					/* Always connective ones */
					case MP_CLEAR: case MP_TREES:
						connective = true;
						break;

					/* The conditionally connective ones */
					case MP_TUNNELBRIDGE:
					case MP_STATION:
					case MP_ROAD:
						if (IsNormalRoadTile(neighbor_tile)) {
							/* Always connective */
							connective = true;
						} else {
							const RoadBits neighbor_rb = GetAnyRoadBits(neighbor_tile, RTT_ROAD) | GetAnyRoadBits(neighbor_tile, RTT_TRAM);

							/* Accept only connective tiles */
							connective = (neighbor_rb & mirrored_rb) != ROAD_NONE;
						}
						break;

					case MP_RAILWAY:
						connective = IsPossibleCrossing(neighbor_tile, DiagDirToAxis(dir));
						break;

					case MP_WATER:
						/* Check for real water tile */
						connective = !IsWater(neighbor_tile);
						break;

					/* The definitely not connective ones */
					default: break;
				}
			}

			/* If the neighbor tile is inconnective, remove the planned road connection to it */
			if (!connective) org_rb ^= target_rb;
		}
	}

	return org_rb;
}

/**
 * Finds out, whether given company has a given RoadType available for construction.
 * @param company ID of company
 * @param roadtypet RoadType to test
 * @return true if company has the requested RoadType available
 */
bool HasRoadTypeAvail(const CompanyID company, RoadType roadtype)
{
	if (company == OWNER_DEITY || company == OWNER_TOWN || _game_mode == GM_EDITOR || _generating_world) {
		const RoadTypeInfo *rti = GetRoadTypeInfo(roadtype);
		if (rti->label == 0) return false;

		/* Not yet introduced at this date. */
		if (IsInsideMM(rti->introduction_date, 0, CalendarTime::MAX_DATE.base()) && rti->introduction_date > TimerGameCalendar::date) return false;

		/*
		 * Do not allow building hidden road types, except when a town may build it.
		 * The GS under deity mode, as well as anybody in the editor builds roads that are
		 * owned by towns. So if a town may build it, it should be buildable by them too.
		 */
		return (rti->flags & ROTFB_HIDDEN) == 0 || (rti->flags & ROTFB_TOWN_BUILD) != 0;
	} else {
		const Company *c = Company::GetIfValid(company);
		if (c == nullptr) return false;
		return HasBit(c->avail_roadtypes & ~_roadtypes_hidden_mask, roadtype);
	}
}

static RoadTypes GetMaskForRoadTramType(RoadTramType rtt)
{
	return rtt == RTT_TRAM ? _roadtypes_type : ~_roadtypes_type;
}

/**
 * Test if any buildable RoadType is available for a company.
 * @param company the company in question
 * @return true if company has any RoadTypes available
 */
bool HasAnyRoadTypesAvail(CompanyID company, RoadTramType rtt)
{
	return (Company::Get(company)->avail_roadtypes & ~_roadtypes_hidden_mask & GetMaskForRoadTramType(rtt)) != ROADTYPES_NONE;
}

/**
 * Validate functions for rail building.
 * @param roadtype road type to check.
 * @return true if the current company may build the road.
 */
bool ValParamRoadType(RoadType roadtype)
{
	return roadtype < ROADTYPE_END && HasRoadTypeAvail(_current_company, roadtype);
}

/**
 * Add the road types that are to be introduced at the given date.
 * @param rt      Roadtype
 * @param current The currently available roadtypes.
 * @param date    The date for the introduction comparisons.
 * @return The road types that should be available when date
 *         introduced road types are taken into account as well.
 */
RoadTypes AddDateIntroducedRoadTypes(RoadTypes current, TimerGameCalendar::Date date)
{
	RoadTypes rts = current;

	for (RoadType rt = ROADTYPE_BEGIN; rt != ROADTYPE_END; rt++) {
		const RoadTypeInfo *rti = GetRoadTypeInfo(rt);
		/* Unused road type. */
		if (rti->label == 0) continue;

		/* Not date introduced. */
		if (!IsInsideMM(rti->introduction_date, 0, CalendarTime::MAX_DATE.base())) continue;

		/* Not yet introduced at this date. */
		if (rti->introduction_date > date) continue;

		/* Have we introduced all required roadtypes? */
		RoadTypes required = rti->introduction_required_roadtypes;
		if ((rts & required) != required) continue;

		rts |= rti->introduces_roadtypes;
	}

	/* When we added roadtypes we need to run this method again; the added
	 * roadtypes might enable more rail types to become introduced. */
	return rts == current ? rts : AddDateIntroducedRoadTypes(rts, date);
}

/**
 * Get the road types the given company can build.
 * @param company the company to get the road types for.
 * @param introduces If true, include road types introduced by other road types
 * @return the road types.
 */
RoadTypes GetCompanyRoadTypes(CompanyID company, bool introduces)
{
	RoadTypes rts = ROADTYPES_NONE;

	for (const Engine *e : Engine::IterateType(VEH_ROAD)) {
		const EngineInfo *ei = &e->info;

		if (HasBit(ei->climates, _settings_game.game_creation.landscape) &&
				(HasBit(e->company_avail, company) || TimerGameCalendar::date >= e->intro_date + CalendarTime::DAYS_IN_YEAR)) {
			const RoadVehicleInfo *rvi = &e->u.road;
			assert(rvi->roadtype < ROADTYPE_END);
			if (introduces) {
				rts |= GetRoadTypeInfo(rvi->roadtype)->introduces_roadtypes;
			} else {
				SetBit(rts, rvi->roadtype);
			}
		}
	}

	if (introduces) return AddDateIntroducedRoadTypes(rts, TimerGameCalendar::date);
	return rts;
}

/**
 * Get list of road types, regardless of company availability.
 * @param introduces If true, include road types introduced by other road types
 * @return the road types.
 */
RoadTypes GetRoadTypes(bool introduces)
{
	RoadTypes rts = ROADTYPES_NONE;

	for (const Engine *e : Engine::IterateType(VEH_ROAD)) {
		const EngineInfo *ei = &e->info;
		if (!HasBit(ei->climates, _settings_game.game_creation.landscape)) continue;

		const RoadVehicleInfo *rvi = &e->u.road;
		assert(rvi->roadtype < ROADTYPE_END);
		if (introduces) {
			rts |= GetRoadTypeInfo(rvi->roadtype)->introduces_roadtypes;
		} else {
			SetBit(rts, rvi->roadtype);
		}
	}

	if (introduces) return AddDateIntroducedRoadTypes(rts, CalendarTime::MAX_DATE);
	return rts;
}

/**
 * Get the road type for a given label.
 * @param label the roadtype label.
 * @param allow_alternate_labels Search in the alternate label lists as well.
 * @return the roadtype.
 */
RoadType GetRoadTypeByLabel(RoadTypeLabel label, bool allow_alternate_labels)
{
	/* Loop through each road type until the label is found */
	for (RoadType r = ROADTYPE_BEGIN; r != ROADTYPE_END; r++) {
		const RoadTypeInfo *rti = GetRoadTypeInfo(r);
		if (rti->label == label) return r;
	}

	if (allow_alternate_labels) {
		/* Test if any road type defines the label as an alternate. */
		for (RoadType r = ROADTYPE_BEGIN; r != ROADTYPE_END; r++) {
			const RoadTypeInfo *rti = GetRoadTypeInfo(r);
			if (std::find(rti->alternate_labels.begin(), rti->alternate_labels.end(), label) != rti->alternate_labels.end()) return r;
		}
	}

	/* No matching label was found, so it is invalid */
	return INVALID_ROADTYPE;
}
