/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file rail.cpp Implementation of rail specific functions. */

#include "stdafx.h"
#include "station_map.h"
#include "tunnelbridge_map.h"
#include "date_func.h"
#include "company_func.h"
#include "company_base.h"
#include "engine_base.h"

#include "safeguards.h"

/* XXX: Below 3 tables store duplicate data. Maybe remove some? */
/* Maps a trackdir to the bit that stores its status in the map arrays, in the
 * direction along with the trackdir */
extern const byte _signal_along_trackdir[TRACKDIR_END] = {
	0x8, 0x8, 0x8, 0x2, 0x4, 0x1, 0, 0,
	0x4, 0x4, 0x4, 0x1, 0x8, 0x2
};

/* Maps a trackdir to the bit that stores its status in the map arrays, in the
 * direction against the trackdir */
extern const byte _signal_against_trackdir[TRACKDIR_END] = {
	0x4, 0x4, 0x4, 0x1, 0x8, 0x2, 0, 0,
	0x8, 0x8, 0x8, 0x2, 0x4, 0x1
};

/* Maps a Track to the bits that store the status of the two signals that can
 * be present on the given track */
extern const byte _signal_on_track[] = {
	0xC, 0xC, 0xC, 0x3, 0xC, 0x3
};

/* Maps a diagonal direction to the all trackdirs that are connected to any
 * track entering in this direction (including those making 90 degree turns)
 */
extern const TrackdirBits _exitdir_reaches_trackdirs[] = {
	TRACKDIR_BIT_X_NE | TRACKDIR_BIT_LOWER_E | TRACKDIR_BIT_LEFT_N,  // DIAGDIR_NE
	TRACKDIR_BIT_Y_SE | TRACKDIR_BIT_LEFT_S  | TRACKDIR_BIT_UPPER_E, // DIAGDIR_SE
	TRACKDIR_BIT_X_SW | TRACKDIR_BIT_UPPER_W | TRACKDIR_BIT_RIGHT_S, // DIAGDIR_SW
	TRACKDIR_BIT_Y_NW | TRACKDIR_BIT_RIGHT_N | TRACKDIR_BIT_LOWER_W  // DIAGDIR_NW
};

extern const Trackdir _next_trackdir[TRACKDIR_END] = {
	TRACKDIR_X_NE,  TRACKDIR_Y_SE,  TRACKDIR_LOWER_E, TRACKDIR_UPPER_E, TRACKDIR_RIGHT_S, TRACKDIR_LEFT_S, INVALID_TRACKDIR, INVALID_TRACKDIR,
	TRACKDIR_X_SW,  TRACKDIR_Y_NW,  TRACKDIR_LOWER_W, TRACKDIR_UPPER_W, TRACKDIR_RIGHT_N, TRACKDIR_LEFT_N
};

/* Maps a trackdir to all trackdirs that make 90 deg turns with it. */
extern const TrackdirBits _track_crosses_trackdirs[TRACK_END] = {
	TRACKDIR_BIT_Y_SE     | TRACKDIR_BIT_Y_NW,                                                   // TRACK_X
	TRACKDIR_BIT_X_NE     | TRACKDIR_BIT_X_SW,                                                   // TRACK_Y
	TRACKDIR_BIT_RIGHT_N  | TRACKDIR_BIT_RIGHT_S  | TRACKDIR_BIT_LEFT_N  | TRACKDIR_BIT_LEFT_S,  // TRACK_UPPER
	TRACKDIR_BIT_RIGHT_N  | TRACKDIR_BIT_RIGHT_S  | TRACKDIR_BIT_LEFT_N  | TRACKDIR_BIT_LEFT_S,  // TRACK_LOWER
	TRACKDIR_BIT_UPPER_W  | TRACKDIR_BIT_UPPER_E  | TRACKDIR_BIT_LOWER_W | TRACKDIR_BIT_LOWER_E, // TRACK_LEFT
	TRACKDIR_BIT_UPPER_W  | TRACKDIR_BIT_UPPER_E  | TRACKDIR_BIT_LOWER_W | TRACKDIR_BIT_LOWER_E  // TRACK_RIGHT
};

/* Maps a track to all tracks that make 90 deg turns with it. */
extern const TrackBits _track_crosses_tracks[] = {
	TRACK_BIT_Y,    // TRACK_X
	TRACK_BIT_X,    // TRACK_Y
	TRACK_BIT_VERT, // TRACK_UPPER
	TRACK_BIT_VERT, // TRACK_LOWER
	TRACK_BIT_HORZ, // TRACK_LEFT
	TRACK_BIT_HORZ  // TRACK_RIGHT
};

/* Maps a trackdir to the (4-way) direction the tile is exited when following
 * that trackdir */
extern const DiagDirection _trackdir_to_exitdir[TRACKDIR_END] = {
	DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_SW, DIAGDIR_SE, DIAGDIR_NE, DIAGDIR_NE,
	DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NW, DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NE,
};

extern const Trackdir _track_exitdir_to_trackdir[][DIAGDIR_END] = {
	{TRACKDIR_X_NE,     INVALID_TRACKDIR,  TRACKDIR_X_SW,     INVALID_TRACKDIR},
	{INVALID_TRACKDIR,  TRACKDIR_Y_SE,     INVALID_TRACKDIR,  TRACKDIR_Y_NW},
	{TRACKDIR_UPPER_E,  INVALID_TRACKDIR,  INVALID_TRACKDIR,  TRACKDIR_UPPER_W},
	{INVALID_TRACKDIR,  TRACKDIR_LOWER_E,  TRACKDIR_LOWER_W,  INVALID_TRACKDIR},
	{INVALID_TRACKDIR,  INVALID_TRACKDIR,  TRACKDIR_LEFT_S,   TRACKDIR_LEFT_N},
	{TRACKDIR_RIGHT_N,  TRACKDIR_RIGHT_S,  INVALID_TRACKDIR,  INVALID_TRACKDIR}
};

extern const Trackdir _track_enterdir_to_trackdir[][DIAGDIR_END] = {
	{TRACKDIR_X_NE,     INVALID_TRACKDIR,  TRACKDIR_X_SW,     INVALID_TRACKDIR},
	{INVALID_TRACKDIR,  TRACKDIR_Y_SE,     INVALID_TRACKDIR,  TRACKDIR_Y_NW},
	{INVALID_TRACKDIR,  TRACKDIR_UPPER_E,  TRACKDIR_UPPER_W,  INVALID_TRACKDIR},
	{TRACKDIR_LOWER_E,  INVALID_TRACKDIR,  INVALID_TRACKDIR,  TRACKDIR_LOWER_W},
	{TRACKDIR_LEFT_N,   TRACKDIR_LEFT_S,   INVALID_TRACKDIR,  INVALID_TRACKDIR},
	{INVALID_TRACKDIR,  INVALID_TRACKDIR,  TRACKDIR_RIGHT_S,  TRACKDIR_RIGHT_N}
};

extern const Trackdir _track_direction_to_trackdir[][DIR_END] = {
	{INVALID_TRACKDIR, TRACKDIR_X_NE,     INVALID_TRACKDIR, INVALID_TRACKDIR,  INVALID_TRACKDIR, TRACKDIR_X_SW,     INVALID_TRACKDIR, INVALID_TRACKDIR},
	{INVALID_TRACKDIR, INVALID_TRACKDIR,  INVALID_TRACKDIR, TRACKDIR_Y_SE,     INVALID_TRACKDIR, INVALID_TRACKDIR,  INVALID_TRACKDIR, TRACKDIR_Y_NW},
	{INVALID_TRACKDIR, INVALID_TRACKDIR,  TRACKDIR_UPPER_E, INVALID_TRACKDIR,  INVALID_TRACKDIR, INVALID_TRACKDIR,  TRACKDIR_UPPER_W, INVALID_TRACKDIR},
	{INVALID_TRACKDIR, INVALID_TRACKDIR,  TRACKDIR_LOWER_E, INVALID_TRACKDIR,  INVALID_TRACKDIR, INVALID_TRACKDIR,  TRACKDIR_LOWER_W, INVALID_TRACKDIR},
	{TRACKDIR_LEFT_N,  INVALID_TRACKDIR,  INVALID_TRACKDIR, INVALID_TRACKDIR,  TRACKDIR_LEFT_S,  INVALID_TRACKDIR,  INVALID_TRACKDIR, INVALID_TRACKDIR},
	{TRACKDIR_RIGHT_N, INVALID_TRACKDIR,  INVALID_TRACKDIR, INVALID_TRACKDIR,  TRACKDIR_RIGHT_S, INVALID_TRACKDIR,  INVALID_TRACKDIR, INVALID_TRACKDIR}
};

extern const Trackdir _dir_to_diag_trackdir[] = {
	TRACKDIR_X_NE, TRACKDIR_Y_SE, TRACKDIR_X_SW, TRACKDIR_Y_NW,
};

extern const TrackBits _corner_to_trackbits[] = {
	TRACK_BIT_LEFT, TRACK_BIT_LOWER, TRACK_BIT_RIGHT, TRACK_BIT_UPPER,
};

extern const TrackdirBits _uphill_trackdirs[] = {
	TRACKDIR_BIT_NONE                    , ///<  0 SLOPE_FLAT
	TRACKDIR_BIT_X_SW | TRACKDIR_BIT_Y_NW, ///<  1 SLOPE_W   -> inclined for diagonal track
	TRACKDIR_BIT_X_SW | TRACKDIR_BIT_Y_SE, ///<  2 SLOPE_S   -> inclined for diagonal track
	TRACKDIR_BIT_X_SW                    , ///<  3 SLOPE_SW
	TRACKDIR_BIT_X_NE | TRACKDIR_BIT_Y_SE, ///<  4 SLOPE_E   -> inclined for diagonal track
	TRACKDIR_BIT_NONE                    , ///<  5 SLOPE_EW
	TRACKDIR_BIT_Y_SE                    , ///<  6 SLOPE_SE
	TRACKDIR_BIT_NONE                    , ///<  7 SLOPE_WSE -> leveled
	TRACKDIR_BIT_X_NE | TRACKDIR_BIT_Y_NW, ///<  8 SLOPE_N   -> inclined for diagonal track
	TRACKDIR_BIT_Y_NW                    , ///<  9 SLOPE_NW
	TRACKDIR_BIT_NONE                    , ///< 10 SLOPE_NS
	TRACKDIR_BIT_NONE                    , ///< 11 SLOPE_NWS -> leveled
	TRACKDIR_BIT_X_NE                    , ///< 12 SLOPE_NE
	TRACKDIR_BIT_NONE                    , ///< 13 SLOPE_ENW -> leveled
	TRACKDIR_BIT_NONE                    , ///< 14 SLOPE_SEN -> leveled
	TRACKDIR_BIT_NONE                    , ///< 15 invalid
	TRACKDIR_BIT_NONE                    , ///< 16 invalid
	TRACKDIR_BIT_NONE                    , ///< 17 invalid
	TRACKDIR_BIT_NONE                    , ///< 18 invalid
	TRACKDIR_BIT_NONE                    , ///< 19 invalid
	TRACKDIR_BIT_NONE                    , ///< 20 invalid
	TRACKDIR_BIT_NONE                    , ///< 21 invalid
	TRACKDIR_BIT_NONE                    , ///< 22 invalid
	TRACKDIR_BIT_X_SW | TRACKDIR_BIT_Y_SE, ///< 23 SLOPE_STEEP_S -> inclined for diagonal track
	TRACKDIR_BIT_NONE                    , ///< 24 invalid
	TRACKDIR_BIT_NONE                    , ///< 25 invalid
	TRACKDIR_BIT_NONE                    , ///< 26 invalid
	TRACKDIR_BIT_X_SW | TRACKDIR_BIT_Y_NW, ///< 27 SLOPE_STEEP_W -> inclined for diagonal track
	TRACKDIR_BIT_NONE                    , ///< 28 invalid
	TRACKDIR_BIT_X_NE | TRACKDIR_BIT_Y_NW, ///< 29 SLOPE_STEEP_N -> inclined for diagonal track
	TRACKDIR_BIT_X_NE | TRACKDIR_BIT_Y_SE, ///< 30 SLOPE_STEEP_E -> inclined for diagonal track
};

/**
 * Return the rail type of tile, or INVALID_RAILTYPE if this is no rail tile.
 */
RailType GetTileRailType(TileIndex tile)
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
 * Finds out if a company has a certain railtype available
 * @param company the company in question
 * @param railtype requested RailType
 * @return true if company has requested RailType available
 */
bool HasRailtypeAvail(const CompanyID company, const RailType railtype)
{
	return HasBit(Company::Get(company)->avail_railtypes, railtype);
}

/**
 * Validate functions for rail building.
 * @param rail the railtype to check.
 * @return true if the current company may build the rail.
 */
bool ValParamRailtype(const RailType rail)
{
	return rail < RAILTYPE_END && HasRailtypeAvail(_current_company, rail);
}

/**
 * Returns the "best" railtype a company can build.
 * As the AI doesn't know what the BEST one is, we have our own priority list
 * here. When adding new railtypes, modify this function
 * @param company the company "in action"
 * @return The "best" railtype a company has available
 */
RailType GetBestRailtype(const CompanyID company)
{
	if (HasRailtypeAvail(company, RAILTYPE_MAGLEV)) return RAILTYPE_MAGLEV;
	if (HasRailtypeAvail(company, RAILTYPE_MONO)) return RAILTYPE_MONO;
	if (HasRailtypeAvail(company, RAILTYPE_ELECTRIC)) return RAILTYPE_ELECTRIC;
	return RAILTYPE_RAIL;
}

/**
 * Add the rail types that are to be introduced at the given date.
 * @param current The currently available railtypes.
 * @param date    The date for the introduction comparisons.
 * @return The rail types that should be available when date
 *         introduced rail types are taken into account as well.
 */
RailTypes AddDateIntroducedRailTypes(RailTypes current, Date date)
{
	RailTypes rts = current;

	for (RailType rt = RAILTYPE_BEGIN; rt != RAILTYPE_END; rt++) {
		const RailtypeInfo *rti = GetRailTypeInfo(rt);
		/* Unused rail type. */
		if (rti->label == 0) continue;

		/* Not date introduced. */
		if (!IsInsideMM(rti->introduction_date, 0, MAX_DAY)) continue;

		/* Not yet introduced at this date. */
		if (rti->introduction_date > date) continue;

		/* Have we introduced all required railtypes? */
		RailTypes required = rti->introduction_required_railtypes;
		if ((rts & required) != required) continue;

		rts |= rti->introduces_railtypes;
	}

	/* When we added railtypes we need to run this method again; the added
	 * railtypes might enable more rail types to become introduced. */
	return rts == current ? rts : AddDateIntroducedRailTypes(rts, date);
}

/**
 * Get the rail types the given company can build.
 * @param c the company to get the rail types for.
 * @return the rail types.
 */
RailTypes GetCompanyRailtypes(CompanyID company)
{
	RailTypes rts = RAILTYPES_NONE;

	Engine *e;
	FOR_ALL_ENGINES_OF_TYPE(e, VEH_TRAIN) {
		const EngineInfo *ei = &e->info;

		if (HasBit(ei->climates, _settings_game.game_creation.landscape) &&
				(HasBit(e->company_avail, company) || _date >= e->intro_date + DAYS_IN_YEAR)) {
			const RailVehicleInfo *rvi = &e->u.rail;

			if (rvi->railveh_type != RAILVEH_WAGON) {
				assert(rvi->railtype < RAILTYPE_END);
				rts |= GetRailTypeInfo(rvi->railtype)->introduces_railtypes;
			}
		}
	}

	return AddDateIntroducedRailTypes(rts, _date);
}

/**
 * Get the rail type for a given label.
 * @param label the railtype label.
 * @param allow_alternate_labels Search in the alternate label lists as well.
 * @return the railtype.
 */
RailType GetRailTypeByLabel(RailTypeLabel label, bool allow_alternate_labels)
{
	/* Loop through each rail type until the label is found */
	for (RailType r = RAILTYPE_BEGIN; r != RAILTYPE_END; r++) {
		const RailtypeInfo *rti = GetRailTypeInfo(r);
		if (rti->label == label) return r;
	}

	if (allow_alternate_labels) {
		/* Test if any rail type defines the label as an alternate. */
		for (RailType r = RAILTYPE_BEGIN; r != RAILTYPE_END; r++) {
			const RailtypeInfo *rti = GetRailTypeInfo(r);
			if (rti->alternate_labels.Contains(label)) return r;
		}
	}

	/* No matching label was found, so it is invalid */
	return INVALID_RAILTYPE;
}
