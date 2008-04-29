/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "rail_map.h"
#include "road_map.h"
#include "road_internal.h"
#include "water_map.h"
#include "genworld.h"
#include "player_func.h"
#include "player_base.h"
#include "engine_func.h"
#include "engine_base.h"
#include "settings_type.h"
#include "date_func.h"

bool IsPossibleCrossing(const TileIndex tile, Axis ax)
{
	return (IsTileType(tile, MP_RAILWAY) &&
		!HasSignals(tile) &&
		GetTrackBits(tile) == (ax == AXIS_X ?  TRACK_BIT_Y : TRACK_BIT_X) &&
		GetTileSlope(tile, NULL) == SLOPE_FLAT);
}

RoadBits CleanUpRoadBits(const TileIndex tile, RoadBits org_rb)
{
	for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
		const TileIndex neighbor_tile = TileAddByDiagDir(tile, dir);

		/* Get the Roadbit pointing to the neighbor_tile */
		const RoadBits target_rb = DiagDirToRoadBits(dir);

		/* If the roadbit is in the current plan */
		if (org_rb & target_rb) {
			bool connective = false;
			const RoadBits mirrored_rb = MirrorRoadBits(target_rb);

			switch (GetTileType(neighbor_tile)) {
				/* Allways connective ones */
				case MP_CLEAR: case MP_TREES:
					connective = true;
					break;

				/* The conditionaly connective ones */
				case MP_TUNNELBRIDGE:
				case MP_STATION:
				case MP_ROAD: {
					const RoadBits neighbor_rb = GetAnyRoadBits(neighbor_tile, ROADTYPE_ROAD) | GetAnyRoadBits(neighbor_tile, ROADTYPE_TRAM);

					/* Accept only connective tiles */
					connective = (neighbor_rb & mirrored_rb) || // Neighbor has got the fitting RoadBit
							CountBits(neighbor_rb) == 1; // Neighbor has got only one Roadbit

				} break;

				case MP_RAILWAY:
					connective = IsPossibleCrossing(neighbor_tile, DiagDirToAxis(dir));
					break;

				case MP_WATER:
					/* Check for real water tile */
					connective = !IsWater(neighbor_tile);
					break;

				/* The defentetly not connective ones */
				default: break;
			}

			/* If the neighbor tile is inconnective remove the planed road connection to it */
			if (!connective) org_rb ^= target_rb;

		}
	}

	return org_rb;
}

bool HasRoadTypesAvail(const PlayerID p, const RoadTypes rts)
{
	RoadTypes avail_roadtypes;

	if (p == OWNER_TOWN || _game_mode == GM_EDITOR || IsGeneratingWorld()) {
		avail_roadtypes = ROADTYPES_ROAD;
	} else {
		if (!IsValidPlayer(p)) return false;
		avail_roadtypes = (RoadTypes)GetPlayer(p)->avail_roadtypes | ROADTYPES_ROAD; // road is available for always for everybody
	}
	return (rts & ~avail_roadtypes) == 0;
}

bool ValParamRoadType(const RoadType rt)
{
	return HasRoadTypesAvail(_current_player, RoadTypeToRoadTypes(rt));
}

RoadTypes GetPlayerRoadtypes(PlayerID p)
{
	RoadTypes rt = ROADTYPES_NONE;

	Engine *e;
	FOR_ALL_ENGINES_OF_TYPE(e, VEH_ROAD) {
		const EngineInfo *ei = &e->info;

		if (HasBit(ei->climates, _opt.landscape) &&
				(HasBit(e->player_avail, p) || _date >= e->intro_date + 365)) {
			SetBit(rt, HasBit(ei->misc_flags, EF_ROAD_TRAM) ? ROADTYPE_TRAM : ROADTYPE_ROAD);
		}
	}

	return rt;
}
