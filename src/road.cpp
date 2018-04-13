/* $Id$ */

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
#include "date_func.h"
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
 * Clean up unnecessary RoadBits of a planed tile.
 * @param tile current tile
 * @param org_rb planed RoadBits
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
							const RoadBits neighbor_rb = GetAnyRoadBits(neighbor_tile, ROADTYPE_ROAD) | GetAnyRoadBits(neighbor_tile, ROADTYPE_TRAM); // TODO

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

			/* If the neighbor tile is inconnective, remove the planed road connection to it */
			if (!connective) org_rb ^= target_rb;
		}
	}

	return org_rb;
}

/**
 * Finds out, whether given company has all given RoadTypes available
 * @param company ID of company
 * @param rtid RoadType to test
 * @return true if company has the requested RoadType available
 */
bool HasRoadTypeAvail(const CompanyID company, RoadTypeIdentifier rtid)
{
	if (company == OWNER_DEITY || company == OWNER_TOWN || _game_mode == GM_EDITOR || _generating_world) {
		return true; // TODO
	} else {
		Company *c = Company::GetIfValid(company);
		if (c == NULL) return false;
		return HasBit(c->avail_roadtypes[rtid.basetype], rtid.subtype);
	}
}

/**
 * Validate functions for rail building.
 * @param rtid road type to check.
 * @return true if the current company may build the road.
 */
bool ValParamRoadType(RoadTypeIdentifier rtid)
{
	return rtid.IsValid() && HasRoadTypeAvail(_current_company, rtid);
}

/**
 * Add the road types that are to be introduced at the given date.
 * @param rt      Roadtype
 * @param current The currently available roadsubtypes.
 * @param date    The date for the introduction comparisons.
 * @return The road types that should be available when date
 *         introduced road types are taken into account as well.
 */
RoadSubTypes AddDateIntroducedRoadTypes(RoadType rt, RoadSubTypes current, Date date)
{
	RoadSubTypes rts = current;

	for (RoadSubType rst = ROADSUBTYPE_BEGIN; rst != ROADSUBTYPE_END; rst++) {
		const RoadtypeInfo *rti = GetRoadTypeInfo(RoadTypeIdentifier(rt, rst));
		/* Unused road type. */
		if (rti->label == 0) continue;

		/* Not date introduced. */
		if (!IsInsideMM(rti->introduction_date, 0, MAX_DAY)) continue;

		/* Not yet introduced at this date. */
		if (rti->introduction_date > date) continue;

		/* Have we introduced all required roadtypes? */
		RoadSubTypes required = rti->introduction_required_roadtypes;
		if ((rts & required) != required) continue;

		rts |= rti->introduces_roadtypes;
	}

	/* When we added roadtypes we need to run this method again; the added
	 * roadtypes might enable more rail types to become introduced. */
	return rts == current ? rts : AddDateIntroducedRoadTypes(rt, rts, date);
}

/**
 * Get the road (sub) types the given company can build.
 * @param company the company to get the roadtypes for.
 * @param rt the base road type to check
 * @return the available road sub types.
 */
RoadSubTypes GetCompanyRoadtypes(CompanyID company, RoadType rt)
{
	RoadSubTypes rst = ROADSUBTYPES_NONE;

	if (rt == ROADTYPE_ROAD) rst |= ROADSUBTYPES_NORMAL; // Road is always available. // TODO

	Engine *e;
	FOR_ALL_ENGINES_OF_TYPE(e, VEH_ROAD) {
		const EngineInfo *ei = &e->info;

		if (HasBit(ei->climates, _settings_game.game_creation.landscape) &&
				(HasBit(e->company_avail, company) || _date >= e->intro_date + DAYS_IN_YEAR) &&
				rt == (HasBit(ei->misc_flags, EF_ROAD_TRAM) ? ROADTYPE_TRAM : ROADTYPE_ROAD)) {
			rst |= GetRoadTypeInfo(e->GetRoadType())->introduces_roadtypes;
		}
	}

	return AddDateIntroducedRoadTypes(rt, rst, _date);
}

/**
 * Get the road type for a given label.
 * @param label the roadtype label.
 * @param allow_alternate_labels Search in the alternate label lists as well.
 * @return the roadtype.
 */
RoadTypeIdentifier GetRoadTypeByLabel(RoadTypeLabel label, RoadType basetype, bool allow_alternate_labels)
{
	RoadTypeIdentifier rtid;

	rtid.basetype = basetype;

	/* Loop through each road type until the label is found */
	for (RoadSubType r = ROADSUBTYPE_BEGIN; r != ROADSUBTYPE_END; r++) {
		rtid.subtype = r;
		const RoadtypeInfo *rti = GetRoadTypeInfo(rtid);
		if (rti->label == label) return rtid;
	}

	if (allow_alternate_labels) {
		/* Test if any road type defines the label as an alternate. */
		for (RoadSubType r = ROADSUBTYPE_BEGIN; r != ROADSUBTYPE_END; r++) {
			rtid.subtype = r;
			const RoadtypeInfo *rti = GetRoadTypeInfo(rtid);
			if (rti->alternate_labels.Contains(label)) return rtid;
		}
	}

	/* No matching label was found, so it is invalid */
	rtid.basetype = INVALID_ROADTYPE;
	rtid.subtype = INVALID_ROADSUBTYPE;
	return rtid;
}

uint8 RoadTypeIdentifier::Pack() const
{
	assert(this->IsValid());

	return this->basetype | (this->subtype << 1);
}

bool RoadTypeIdentifier::UnpackIfValid(uint32 data)
{
	this->basetype = (RoadType)GB(data, 0, 1);
	this->subtype = (RoadSubType)GB(data, 1, 4);

	return this->IsValid();
}

/* static */ RoadTypeIdentifier RoadTypeIdentifier::Unpack(uint32 data)
{
	RoadTypeIdentifier result;
	bool ret = result.UnpackIfValid(data);
	assert(ret);
	return result;
}

/**
 * Returns the available RoadSubTypes for the provided RoadType
 * If the given company is valid then will be returned a list of the available sub types at the current date, while passing
 * a deity company will make all the sub types available
 * @param rt the RoadType to filter
 * @param c the company ID to check the roadtype against
 * @param any_date whether to return only currently introduced roadtypes or also future ones
 * @returns the existing RoadSubTypes
 */
RoadSubTypes ExistingRoadSubTypesForRoadType(RoadType rt, CompanyID c)
{
	/* Check only players which can actually own vehicles, editor and gamescripts are considered deities */
	if (c < OWNER_END) {
		const Company *company = Company::GetIfValid(c);

		if (company != NULL) return company->avail_roadtypes[rt];
	}

	RoadSubTypes known_roadsubtypes = ROADSUBTYPES_NONE;

	/* Road is always visible and available. */
	if (rt == ROADTYPE_ROAD) known_roadsubtypes |= ROADSUBTYPES_NORMAL; // TODO

	/* Find used roadtypes */
	Engine *e;
	FOR_ALL_ENGINES_OF_TYPE(e, VEH_ROAD) {
		/* Check if the subtype can be used in the current climate */
		if (!HasBit(e->info.climates,  _settings_game.game_creation.landscape)) continue;

		/* Check whether available for all potential companies */
		if (e->company_avail != (CompanyMask)-1) continue;

		RoadTypeIdentifier rtid = e->GetRoadType();
		if (rtid.basetype != rt) continue;

		known_roadsubtypes |= GetRoadTypeInfo(rtid)->introduces_roadtypes;
	}

	/* Get the date introduced roadtypes as well. */
	known_roadsubtypes = AddDateIntroducedRoadTypes(rt, known_roadsubtypes, MAX_DAY);

	return known_roadsubtypes;
}

/**
 * Check whether we can build infrastructure for the given RoadType. This to disable building stations etc. when
 * you are not allowed/able to have the RoadType yet.
 * @param rtid the roadtype to check this for
 * @param company the company id to check this for
 * @param any_date to check only existing vehicles or if it is possible to build them in the future
 * @return true if there is any reason why you may build the infrastructure for the given roadtype
 */
bool CanBuildRoadTypeInfrastructure(RoadTypeIdentifier rtid, CompanyID company)
{
	if (_game_mode != GM_EDITOR && !Company::IsValidID(company)) return false;
	if (!_settings_client.gui.disable_unsuitable_building) return true;

	RoadSubTypes roadsubtypes = ExistingRoadSubTypesForRoadType(rtid.basetype, company);

	/* Check if the filtered subtypes does have the subtype we are checking for
	 * and if we can build new ones */
	if (_settings_game.vehicle.max_roadveh > 0 && HasBit(roadsubtypes, rtid.subtype)) {
		/* Can we actually build the vehicle type? */
		const Engine *e;
		FOR_ALL_ENGINES_OF_TYPE(e, VEH_ROAD) {
			if (HasPowerOnRoad(e->GetRoadType(), rtid) || HasPowerOnRoad(rtid, e->GetRoadType())) return true;
		}
		return false;
	}

	/* We should be able to build infrastructure when we have the actual vehicle type */
	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_ROAD && (company == OWNER_DEITY || v->owner == company) &&
			HasBit(roadsubtypes, RoadVehicle::From(v)->rtid.subtype) && HasPowerOnRoad(RoadVehicle::From(v)->rtid, rtid)) return true;
	}

	return false;
}
