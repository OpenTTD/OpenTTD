/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "rail_map.h"
#include "road_map.h"
#include "road_internal.h"
#include "water_map.h"

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
