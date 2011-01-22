/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_rail.cpp Implementation of AIRail. */

#include "../../stdafx.h"
#include "ai_rail.hpp"
#include "ai_map.hpp"
#include "ai_station.hpp"
#include "ai_industrytype.hpp"
#include "../../debug.h"
#include "../../station_base.h"
#include "../../company_func.h"
#include "../../newgrf.h"
#include "../../newgrf_generic.h"
#include "../../newgrf_station.h"
#include "../../strings_func.h"

/* static */ char *AIRail::GetName(RailType rail_type)
{
	if (!IsRailTypeAvailable(rail_type)) return NULL;

	static const int len = 64;
	char *railtype_name = MallocT<char>(len);

	::GetString(railtype_name, GetRailTypeInfo((::RailType)rail_type)->strings.menu_text, &railtype_name[len - 1]);
	return railtype_name;
}

/* static */ bool AIRail::IsRailTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return (::IsTileType(tile, MP_RAILWAY) && !::IsRailDepot(tile)) ||
			(::HasStationTileRail(tile) && !::IsStationTileBlocked(tile)) || ::IsLevelCrossingTile(tile);
}

/* static */ bool AIRail::IsLevelCrossingTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsLevelCrossingTile(tile);
}

/* static */ bool AIRail::IsRailDepotTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsRailDepotTile(tile);
}

/* static */ bool AIRail::IsRailStationTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsRailStationTile(tile);
}

/* static */ bool AIRail::IsRailWaypointTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsRailWaypointTile(tile);
}

/* static */ bool AIRail::IsRailTypeAvailable(RailType rail_type)
{
	if ((::RailType)rail_type < RAILTYPE_BEGIN || (::RailType)rail_type >= RAILTYPE_END) return false;

	return ::HasRailtypeAvail(_current_company, (::RailType)rail_type);
}

/* static */ AIRail::RailType AIRail::GetCurrentRailType()
{
	return (RailType)AIObject::GetRailType();
}

/* static */ void AIRail::SetCurrentRailType(RailType rail_type)
{
	if (!IsRailTypeAvailable(rail_type)) return;

	AIObject::SetRailType((::RailType)rail_type);
}

/* static */ bool AIRail::TrainCanRunOnRail(AIRail::RailType engine_rail_type, AIRail::RailType track_rail_type)
{
	if (!AIRail::IsRailTypeAvailable(engine_rail_type)) return false;
	if (!AIRail::IsRailTypeAvailable(track_rail_type)) return false;

	return ::IsCompatibleRail((::RailType)engine_rail_type, (::RailType)track_rail_type);
}

/* static */ bool AIRail::TrainHasPowerOnRail(AIRail::RailType engine_rail_type, AIRail::RailType track_rail_type)
{\
	if (!AIRail::IsRailTypeAvailable(engine_rail_type)) return false;
	if (!AIRail::IsRailTypeAvailable(track_rail_type)) return false;

	return ::HasPowerOnRail((::RailType)engine_rail_type, (::RailType)track_rail_type);
}

/* static */ AIRail::RailType AIRail::GetRailType(TileIndex tile)
{
	if (!AITile::HasTransportType(tile, AITile::TRANSPORT_RAIL)) return RAILTYPE_INVALID;

	return (RailType)::GetRailType(tile);
}

/* static */ bool AIRail::ConvertRailType(TileIndex start_tile, TileIndex end_tile, AIRail::RailType convert_to)
{
	EnforcePrecondition(false, ::IsValidTile(start_tile));
	EnforcePrecondition(false, ::IsValidTile(end_tile));
	EnforcePrecondition(false, IsRailTypeAvailable(convert_to));

	return AIObject::DoCommand(start_tile, end_tile, convert_to, CMD_CONVERT_RAIL);
}

/* static */ TileIndex AIRail::GetRailDepotFrontTile(TileIndex depot)
{
	if (!IsRailDepotTile(depot)) return INVALID_TILE;

	return depot + ::TileOffsByDiagDir(::GetRailDepotDirection(depot));
}

/* static */ AIRail::RailTrack AIRail::GetRailStationDirection(TileIndex tile)
{
	if (!IsRailStationTile(tile)) return RAILTRACK_INVALID;

	return (RailTrack)::GetRailStationTrackBits(tile);
}

/* static */ bool AIRail::BuildRailDepot(TileIndex tile, TileIndex front)
{
	EnforcePrecondition(false, tile != front);
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, ::IsValidTile(front));
	EnforcePrecondition(false, ::TileX(tile) == ::TileX(front) || ::TileY(tile) == ::TileY(front));
	EnforcePrecondition(false, IsRailTypeAvailable(GetCurrentRailType()));

	uint entrance_dir = (::TileX(tile) == ::TileX(front)) ? (::TileY(tile) < ::TileY(front) ? 1 : 3) : (::TileX(tile) < ::TileX(front) ? 2 : 0);

	return AIObject::DoCommand(tile, AIObject::GetRailType(), entrance_dir, CMD_BUILD_TRAIN_DEPOT);
}

/* static */ bool AIRail::BuildRailStation(TileIndex tile, RailTrack direction, uint num_platforms, uint platform_length, StationID station_id)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, direction == RAILTRACK_NW_SE || direction == RAILTRACK_NE_SW);
	EnforcePrecondition(false, num_platforms > 0 && num_platforms <= 0xFF);
	EnforcePrecondition(false, platform_length > 0 && platform_length <= 0xFF);
	EnforcePrecondition(false, IsRailTypeAvailable(GetCurrentRailType()));
	EnforcePrecondition(false, station_id == AIStation::STATION_NEW || station_id == AIStation::STATION_JOIN_ADJACENT || AIStation::IsValidStation(station_id));

	uint32 p1 = GetCurrentRailType() | (platform_length << 16) | (num_platforms << 8);
	if (direction == RAILTRACK_NW_SE) p1 |= (1 << 4);
	if (station_id != AIStation::STATION_JOIN_ADJACENT) p1 |= (1 << 24);
	return AIObject::DoCommand(tile, p1, (AIStation::IsValidStation(station_id) ? station_id : INVALID_STATION) << 16, CMD_BUILD_RAIL_STATION);
}

/* static */ bool AIRail::BuildNewGRFRailStation(TileIndex tile, RailTrack direction, uint num_platforms, uint platform_length, StationID station_id, CargoID cargo_id, IndustryType source_industry, IndustryType goal_industry, int distance, bool source_station)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, direction == RAILTRACK_NW_SE || direction == RAILTRACK_NE_SW);
	EnforcePrecondition(false, num_platforms > 0 && num_platforms <= 0xFF);
	EnforcePrecondition(false, platform_length > 0 && platform_length <= 0xFF);
	EnforcePrecondition(false, IsRailTypeAvailable(GetCurrentRailType()));
	EnforcePrecondition(false, station_id == AIStation::STATION_NEW || station_id == AIStation::STATION_JOIN_ADJACENT || AIStation::IsValidStation(station_id));
	EnforcePrecondition(false, source_industry == AIIndustryType::INDUSTRYTYPE_UNKNOWN || source_industry == AIIndustryType::INDUSTRYTYPE_TOWN || AIIndustryType::IsValidIndustryType(source_industry));
	EnforcePrecondition(false, goal_industry   == AIIndustryType::INDUSTRYTYPE_UNKNOWN || goal_industry   == AIIndustryType::INDUSTRYTYPE_TOWN || AIIndustryType::IsValidIndustryType(goal_industry));

	uint32 p1 = GetCurrentRailType() | (platform_length << 16) | (num_platforms << 8);
	if (direction == RAILTRACK_NW_SE) p1 |= 1 << 4;
	if (station_id != AIStation::STATION_JOIN_ADJACENT) p1 |= (1 << 24);

	const GRFFile *file;
	uint16 res = GetAiPurchaseCallbackResult(GSF_STATIONS, cargo_id, 0, source_industry, goal_industry, min(255, distance / 2), AICE_STATION_GET_STATION_ID, source_station ? 0 : 1, min(15, num_platforms) << 4 | min(15, platform_length), &file);
	uint32 p2 = (AIStation::IsValidStation(station_id) ? station_id : INVALID_STATION) << 16;
	if (res != CALLBACK_FAILED) {
		int index = 0;
		const StationSpec *spec = StationClass::GetByGrf(file->grfid, res, &index);
		if (spec == NULL) {
			DEBUG(grf, 1, "%s returned an invalid station ID for 'AI construction/purchase selection (18)' callback", file->filename);
		} else {
			p2 |= spec->cls_id | index << 8;
		}

	}
	return AIObject::DoCommand(tile, p1, p2, CMD_BUILD_RAIL_STATION);
}

/* static */ bool AIRail::BuildRailWaypoint(TileIndex tile)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsRailTile(tile));
	EnforcePrecondition(false, GetRailTracks(tile) == RAILTRACK_NE_SW || GetRailTracks(tile) == RAILTRACK_NW_SE);
	EnforcePrecondition(false, IsRailTypeAvailable(GetCurrentRailType()));

	return AIObject::DoCommand(tile, GetCurrentRailType() | (GetRailTracks(tile) == RAILTRACK_NE_SW ? AXIS_X : AXIS_Y) << 4 | 1 << 8 | 1 << 16, STAT_CLASS_WAYP | INVALID_STATION << 16, CMD_BUILD_RAIL_WAYPOINT);
}

/* static */ bool AIRail::RemoveRailWaypointTileRectangle(TileIndex tile, TileIndex tile2, bool keep_rail)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, ::IsValidTile(tile2));

	return AIObject::DoCommand(tile, tile2, keep_rail ? 1 : 0, CMD_REMOVE_FROM_RAIL_WAYPOINT);
}

/* static */ bool AIRail::RemoveRailStationTileRectangle(TileIndex tile, TileIndex tile2, bool keep_rail)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, ::IsValidTile(tile2));

	return AIObject::DoCommand(tile, tile2, keep_rail ? 1 : 0, CMD_REMOVE_FROM_RAIL_STATION);
}

/* static */ uint AIRail::GetRailTracks(TileIndex tile)
{
	if (!IsRailTile(tile)) return RAILTRACK_INVALID;

	if (IsRailStationTile(tile) || IsRailWaypointTile(tile)) return ::TrackToTrackBits(::GetRailStationTrack(tile));
	if (IsLevelCrossingTile(tile)) return ::GetCrossingRailBits(tile);
	if (IsRailDepotTile(tile)) return ::TRACK_BIT_NONE;
	return ::GetTrackBits(tile);
}

/* static */ bool AIRail::BuildRailTrack(TileIndex tile, RailTrack rail_track)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, rail_track != 0);
	EnforcePrecondition(false, (rail_track & ~::TRACK_BIT_ALL) == 0);
	EnforcePrecondition(false, KillFirstBit((uint)rail_track) == 0);
	EnforcePrecondition(false, IsRailTypeAvailable(GetCurrentRailType()));

	return AIObject::DoCommand(tile, tile, GetCurrentRailType() | (FindFirstTrack((::TrackBits)rail_track) << 4), CMD_BUILD_RAILROAD_TRACK);
}

/* static */ bool AIRail::RemoveRailTrack(TileIndex tile, RailTrack rail_track)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, ::IsPlainRailTile(tile) || ::IsLevelCrossingTile(tile));
	EnforcePrecondition(false, GetRailTracks(tile) & rail_track);
	EnforcePrecondition(false, KillFirstBit((uint)rail_track) == 0);

	return AIObject::DoCommand(tile, tile, GetCurrentRailType() | (FindFirstTrack((::TrackBits)rail_track) << 4), CMD_REMOVE_RAILROAD_TRACK);
}

/* static */ bool AIRail::AreTilesConnected(TileIndex from, TileIndex tile, TileIndex to)
{
	if (!IsRailTile(tile)) return false;
	if (from == to || AIMap::DistanceManhattan(from, tile) != 1 || AIMap::DistanceManhattan(tile, to) != 1) return false;

	if (to < from) ::Swap(from, to);

	if (tile - from == 1) {
		if (to - tile == 1) return (GetRailTracks(tile) & RAILTRACK_NE_SW) != 0;
		if (to - tile == ::MapSizeX()) return (GetRailTracks(tile) & RAILTRACK_NE_SE) != 0;
	} else if (tile - from == ::MapSizeX()) {
		if (tile - to == 1) return (GetRailTracks(tile) & RAILTRACK_NW_NE) != 0;
		if (to - tile == 1) return (GetRailTracks(tile) & RAILTRACK_NW_SW) != 0;
		if (to - tile == ::MapSizeX()) return (GetRailTracks(tile) & RAILTRACK_NW_SE) != 0;
	} else {
		return (GetRailTracks(tile) & RAILTRACK_SW_SE) != 0;
	}

	NOT_REACHED();
}

/**
 * Prepare the second parameter for CmdBuildRailroadTrack and CmdRemoveRailroadTrack. The direction
 * depends on all three tiles. Sometimes the third tile needs to be adjusted.
 */
static uint32 SimulateDrag(TileIndex from, TileIndex tile, TileIndex *to)
{
	int diag_offset = abs(abs((int)::TileX(*to) - (int)::TileX(tile)) - abs((int)::TileY(*to) - (int)::TileY(tile)));
	uint32 p2 = AIRail::GetCurrentRailType();
	if (::TileY(from) == ::TileY(*to)) {
		p2 |= (TRACK_X << 4);
		*to -= Clamp((int)::TileX(*to) - (int)::TileX(tile), -1, 1);
	} else if (::TileX(from) == ::TileX(*to)) {
		p2 |= (TRACK_Y << 4);
		*to -= ::MapSizeX() * Clamp((int)::TileY(*to) - (int)::TileY(tile), -1, 1);
	} else if (::TileY(from) < ::TileY(tile)) {
		if (::TileX(*to) < ::TileX(tile)) {
			p2 |= (TRACK_UPPER << 4);
		} else {
			p2 |= (TRACK_LEFT << 4);
		}
		if (diag_offset) {
			*to -= Clamp((int)::TileX(*to) - (int)::TileX(tile), -1, 1);
		} else {
			*to -= ::MapSizeX() * Clamp((int)::TileY(*to) - (int)::TileY(tile), -1, 1);
		}
	} else if (::TileY(from) > ::TileY(tile)) {
		if (::TileX(*to) < ::TileX(tile)) {
			p2 |= (TRACK_RIGHT << 4);
		} else {
			p2 |= (TRACK_LOWER << 4);
		}
		if (diag_offset) {
			*to -= Clamp((int)::TileX(*to) - (int)::TileX(tile), -1, 1);
		} else {
			*to -= ::MapSizeX() * Clamp((int)::TileY(*to) - (int)::TileY(tile), -1, 1);
		}
	} else if (::TileX(from) < ::TileX(tile)) {
		if (::TileY(*to) < ::TileY(tile)) {
			p2 |= (TRACK_UPPER << 4);
		} else {
			p2 |= (TRACK_RIGHT << 4);
		}
		if (!diag_offset) {
			*to -= Clamp((int)::TileX(*to) - (int)::TileX(tile), -1, 1);
		} else {
			*to -= ::MapSizeX() * Clamp((int)::TileY(*to) - (int)::TileY(tile), -1, 1);
		}
	} else if (::TileX(from) > ::TileX(tile)) {
		if (::TileY(*to) < ::TileY(tile)) {
			p2 |= (TRACK_LEFT << 4);
		} else {
			p2 |= (TRACK_LOWER << 4);
		}
		if (!diag_offset) {
			*to -= Clamp((int)::TileX(*to) - (int)::TileX(tile), -1, 1);
		} else {
			*to -= ::MapSizeX() * Clamp((int)::TileY(*to) - (int)::TileY(tile), -1, 1);
		}
	}
	return p2;
}

/* static */ bool AIRail::BuildRail(TileIndex from, TileIndex tile, TileIndex to)
{
	EnforcePrecondition(false, ::IsValidTile(from));
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, ::IsValidTile(to));
	EnforcePrecondition(false, ::DistanceManhattan(from, tile) == 1);
	EnforcePrecondition(false, ::DistanceManhattan(tile, to) >= 1);
	EnforcePrecondition(false, IsRailTypeAvailable(GetCurrentRailType()));
	int diag_offset = abs(abs((int)::TileX(to) - (int)::TileX(tile)) - abs((int)::TileY(to) - (int)::TileY(tile)));
	EnforcePrecondition(false, diag_offset <= 1 ||
			(::TileX(from) == ::TileX(tile) && ::TileX(tile) == ::TileX(to)) ||
			(::TileY(from) == ::TileY(tile) && ::TileY(tile) == ::TileY(to)));

	uint32 p2 = SimulateDrag(from, tile, &to) | 1 << 8;
	return AIObject::DoCommand(tile, to, p2, CMD_BUILD_RAILROAD_TRACK);
}

/* static */ bool AIRail::RemoveRail(TileIndex from, TileIndex tile, TileIndex to)
{
	EnforcePrecondition(false, ::IsValidTile(from));
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, ::IsValidTile(to));
	EnforcePrecondition(false, ::DistanceManhattan(from, tile) == 1);
	EnforcePrecondition(false, ::DistanceManhattan(tile, to) >= 1);
	int diag_offset = abs(abs((int)::TileX(to) - (int)::TileX(tile)) - abs((int)::TileY(to) - (int)::TileY(tile)));
	EnforcePrecondition(false, diag_offset <= 1 ||
			(::TileX(from) == ::TileX(tile) && ::TileX(tile) == ::TileX(to)) ||
			(::TileY(from) == ::TileY(tile) && ::TileY(tile) == ::TileY(to)));

	if (!IsRailTypeAvailable(GetCurrentRailType())) SetCurrentRailType(GetRailType(tile));
	uint32 p2 = SimulateDrag(from, tile, &to);
	return AIObject::DoCommand(tile, to, p2, CMD_REMOVE_RAILROAD_TRACK);
}

/**
 * Contains information about the trackdir that belongs to a track when entering
 *   from a specific direction.
 */
struct AIRailSignalData {
	Track track;        ///< The track that will be taken to travel.
	Trackdir trackdir;  ///< The Trackdir belonging to that track.
	uint signal_cycles; ///< How many times the signal should be cycled in order to build it in the correct direction.
};

static const int NUM_TRACK_DIRECTIONS = 3; ///< The number of directions you can go when entering a tile.

/**
 * List information about the trackdir and number of needed cycles for building signals when
 *   entering a track from a specific direction. The first index is the difference between the
 *   TileIndex of the previous and current tile, where (-)MapSizeX is replaced with -2 / 2 and
 *   2 it added.
 */
static const AIRailSignalData _possible_trackdirs[5][NUM_TRACK_DIRECTIONS] = {
	{{TRACK_UPPER,   TRACKDIR_UPPER_E, 0}, {TRACK_Y,       TRACKDIR_Y_SE,    0}, {TRACK_LEFT,    TRACKDIR_LEFT_S,  1}},
	{{TRACK_RIGHT,   TRACKDIR_RIGHT_S, 1}, {TRACK_X,       TRACKDIR_X_SW,    1}, {TRACK_UPPER,   TRACKDIR_UPPER_W, 1}},
	{{INVALID_TRACK, INVALID_TRACKDIR, 0}, {INVALID_TRACK, INVALID_TRACKDIR, 0}, {INVALID_TRACK, INVALID_TRACKDIR, 0}},
	{{TRACK_LOWER,   TRACKDIR_LOWER_E, 0}, {TRACK_X,       TRACKDIR_X_NE,    0}, {TRACK_LEFT,    TRACKDIR_LEFT_N,  0}},
	{{TRACK_RIGHT,   TRACKDIR_RIGHT_N, 0}, {TRACK_Y,       TRACKDIR_Y_NW,    1}, {TRACK_LOWER,   TRACKDIR_LOWER_W, 1}}
};

/* static */ AIRail::SignalType AIRail::GetSignalType(TileIndex tile, TileIndex front)
{
	if (AIMap::DistanceManhattan(tile, front) != 1) return SIGNALTYPE_NONE;
	if (!::IsTileType(tile, MP_RAILWAY) || !::HasSignals(tile)) return SIGNALTYPE_NONE;

	int data_index = 2 + (::TileX(front) - ::TileX(tile)) + 2 * (::TileY(front) - ::TileY(tile));

	for (int i = 0; i < NUM_TRACK_DIRECTIONS; i++) {
		const Track &track = _possible_trackdirs[data_index][i].track;
		if (!(::TrackToTrackBits(track) & GetRailTracks(tile))) continue;
		if (!HasSignalOnTrack(tile, track)) continue;
		if (!HasSignalOnTrackdir(tile, _possible_trackdirs[data_index][i].trackdir)) continue;
		SignalType st = (SignalType)::GetSignalType(tile, track);
		if (HasSignalOnTrackdir(tile, ::ReverseTrackdir(_possible_trackdirs[data_index][i].trackdir))) st = (SignalType)(st | SIGNALTYPE_TWOWAY);
		return st;
	}

	return SIGNALTYPE_NONE;
}

/**
 * Check if signal_type is a valid SignalType.
 */
static bool IsValidSignalType(int signal_type)
{
	if (signal_type < AIRail::SIGNALTYPE_NORMAL || signal_type > AIRail::SIGNALTYPE_COMBO_TWOWAY) return false;
	if (signal_type > AIRail::SIGNALTYPE_PBS_ONEWAY && signal_type < AIRail::SIGNALTYPE_NORMAL_TWOWAY) return false;
	return true;
}

/* static */ bool AIRail::BuildSignal(TileIndex tile, TileIndex front, SignalType signal)
{
	EnforcePrecondition(false, AIMap::DistanceManhattan(tile, front) == 1)
	EnforcePrecondition(false, ::IsPlainRailTile(tile));
	EnforcePrecondition(false, ::IsValidSignalType(signal));

	Track track = INVALID_TRACK;
	uint signal_cycles;

	int data_index = 2 + (::TileX(front) - ::TileX(tile)) + 2 * (::TileY(front) - ::TileY(tile));
	for (int i = 0; i < NUM_TRACK_DIRECTIONS; i++) {
		const Track &t = _possible_trackdirs[data_index][i].track;
		if (!(::TrackToTrackBits(t) & GetRailTracks(tile))) continue;
		track = t;
		signal_cycles = _possible_trackdirs[data_index][i].signal_cycles;
		break;
	}
	EnforcePrecondition(false, track != INVALID_TRACK);

	uint p1 = track;
	if (signal < SIGNALTYPE_TWOWAY) {
		if (signal != SIGNALTYPE_PBS && signal != SIGNALTYPE_PBS_ONEWAY) signal_cycles++;
		p1 |= (signal_cycles << 15);
	}
	p1 |= ((signal >= SIGNALTYPE_TWOWAY ? signal ^ SIGNALTYPE_TWOWAY : signal) << 5);

	return AIObject::DoCommand(tile, p1, 0, CMD_BUILD_SIGNALS);
}

/* static */ bool AIRail::RemoveSignal(TileIndex tile, TileIndex front)
{
	EnforcePrecondition(false, AIMap::DistanceManhattan(tile, front) == 1)
	EnforcePrecondition(false, GetSignalType(tile, front) != SIGNALTYPE_NONE);

	Track track = INVALID_TRACK;
	int data_index = 2 + (::TileX(front) - ::TileX(tile)) + 2 * (::TileY(front) - ::TileY(tile));
	for (int i = 0; i < NUM_TRACK_DIRECTIONS; i++) {
		const Track &t = _possible_trackdirs[data_index][i].track;
		if (!(::TrackToTrackBits(t) & GetRailTracks(tile))) continue;
		track = t;
		break;
	}
	EnforcePrecondition(false, track != INVALID_TRACK);

	return AIObject::DoCommand(tile, track, 0, CMD_REMOVE_SIGNALS);
}

/* static */ Money AIRail::GetBuildCost(RailType railtype, BuildType build_type)
{
	if (!AIRail::IsRailTypeAvailable(railtype)) return -1;

	switch (build_type) {
		case BT_TRACK:    return ::RailBuildCost((::RailType)railtype);
		case BT_SIGNAL:   return ::GetPrice(PR_BUILD_SIGNALS, 1, NULL);
		case BT_DEPOT:    return ::GetPrice(PR_BUILD_DEPOT_TRAIN, 1, NULL);
		case BT_STATION:  return ::GetPrice(PR_BUILD_STATION_RAIL, 1, NULL) + ::GetPrice(PR_BUILD_STATION_RAIL_LENGTH, 1, NULL);
		case BT_WAYPOINT: return ::GetPrice(PR_BUILD_WAYPOINT_RAIL, 1, NULL);
		default: return -1;
	}
}

/* static */ int32 AIRail::GetMaxSpeed(RailType railtype)
{
	if (!AIRail::IsRailTypeAvailable(railtype)) return -1;

	return ::GetRailTypeInfo((::RailType)railtype)->max_speed;
}
