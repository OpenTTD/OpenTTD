/* $Id$ */

#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../bridge_map.h"
#include "../../debug.h"
#include "../../command_func.h"
#include "trolly.h"
#include "../../depot.h"
#include "../../tunnel_map.h"
#include "../../bridge.h"
#include "../../tunnelbridge_map.h"
#include "../ai.h"
#include "../../variables.h"


#define TEST_STATION_NO_DIR 0xFF

// Tests if a station can be build on the given spot
// TODO: make it train compatible
static bool TestCanBuildStationHere(TileIndex tile, byte dir)
{
	Player *p = GetPlayer(_current_player);

	if (dir == TEST_STATION_NO_DIR) {
		CommandCost ret;
		// TODO: currently we only allow spots that can be access from al 4 directions...
		//  should be fixed!!!
		for (dir = 0; dir < 4; dir++) {
			ret = AiNew_Build_Station(p, _players_ainew[p->index].tbt, tile, 1, 1, dir, DC_QUERY_COST);
			if (CmdSucceeded(ret)) return true;
		}
		return false;
	}

	// return true if command succeeded, so the inverse of CmdFailed()
	return CmdSucceeded(AiNew_Build_Station(p, _players_ainew[p->index].tbt, tile, 1, 1, dir, DC_QUERY_COST));
}


static bool IsRoad(TileIndex tile)
{
	return
		// MP_ROAD, but not a road depot?
		(IsTileType(tile, MP_ROAD) && !IsTileDepotType(tile, TRANSPORT_ROAD)) ||
		(IsTileType(tile, MP_TUNNELBRIDGE) && GetTunnelBridgeTransportType(tile) == TRANSPORT_ROAD);
}


// Checks if a tile 'a' is between the tiles 'b' and 'c'
#define TILES_BETWEEN(a, b, c) (TileX(a) >= TileX(b) && TileX(a) <= TileX(c) && TileY(a) >= TileY(b) && TileY(a) <= TileY(c))


// Check if the current tile is in our end-area
static int32 AyStar_AiPathFinder_EndNodeCheck(AyStar *aystar, OpenListNode *current)
{
	const Ai_PathFinderInfo* PathFinderInfo = (Ai_PathFinderInfo*)aystar->user_target;

	// It is not allowed to have a station on the end of a bridge or tunnel ;)
	if (current->path.node.user_data[0] != 0) return AYSTAR_DONE;
	if (TILES_BETWEEN(current->path.node.tile, PathFinderInfo->end_tile_tl, PathFinderInfo->end_tile_br))
		if (IsTileType(current->path.node.tile, MP_CLEAR) || IsTileType(current->path.node.tile, MP_TREES))
			if (current->path.parent == NULL || TestCanBuildStationHere(current->path.node.tile, AiNew_GetDirection(current->path.parent->node.tile, current->path.node.tile)))
				return AYSTAR_FOUND_END_NODE;

	return AYSTAR_DONE;
}


// Calculates the hash
//   Currently it is a 10 bit hash, so the hash array has a max depth of 6 bits (so 64)
static uint AiPathFinder_Hash(uint key1, uint key2)
{
	return (TileX(key1) & 0x1F) + ((TileY(key1) & 0x1F) << 5);
}


// Clear the memory of all the things
static void AyStar_AiPathFinder_Free(AyStar *aystar)
{
	AyStarMain_Free(aystar);
	delete aystar;
}


static int32 AyStar_AiPathFinder_CalculateG(AyStar *aystar, AyStarNode *current, OpenListNode *parent);
static int32 AyStar_AiPathFinder_CalculateH(AyStar *aystar, AyStarNode *current, OpenListNode *parent);
static void AyStar_AiPathFinder_FoundEndNode(AyStar *aystar, OpenListNode *current);
static void AyStar_AiPathFinder_GetNeighbours(AyStar *aystar, OpenListNode *current);


// This creates the AiPathFinder
AyStar *new_AyStar_AiPathFinder(int max_tiles_around, Ai_PathFinderInfo *PathFinderInfo)
{
	PathNode start_node;
	uint x;
	uint y;
	// Create AyStar
	AyStar *result = new AyStar();
	init_AyStar(result, AiPathFinder_Hash, 1 << 10);
	// Set the function pointers
	result->CalculateG = AyStar_AiPathFinder_CalculateG;
	result->CalculateH = AyStar_AiPathFinder_CalculateH;
	result->EndNodeCheck = AyStar_AiPathFinder_EndNodeCheck;
	result->FoundEndNode = AyStar_AiPathFinder_FoundEndNode;
	result->GetNeighbours = AyStar_AiPathFinder_GetNeighbours;

	result->free = AyStar_AiPathFinder_Free;

	// Set some information
	result->loops_per_tick = AI_PATHFINDER_LOOPS_PER_TICK;
	result->max_path_cost = 0;
	result->max_search_nodes = AI_PATHFINDER_MAX_SEARCH_NODES;

	// Set the user_data to the PathFinderInfo
	result->user_target = PathFinderInfo;

	// Set the start node
	start_node.parent = NULL;
	start_node.node.direction = 0;
	start_node.node.user_data[0] = 0;

	// Now we add all the starting tiles
	for (x = TileX(PathFinderInfo->start_tile_tl); x <= TileX(PathFinderInfo->start_tile_br); x++) {
		for (y = TileY(PathFinderInfo->start_tile_tl); y <= TileY(PathFinderInfo->start_tile_br); y++) {
			start_node.node.tile = TileXY(x, y);
			result->addstart(result, &start_node.node, 0);
		}
	}

	return result;
}


// To reuse AyStar we sometimes have to clean all the memory
void clean_AyStar_AiPathFinder(AyStar *aystar, Ai_PathFinderInfo *PathFinderInfo)
{
	PathNode start_node;
	uint x;
	uint y;

	aystar->clear(aystar);

	// Set the user_data to the PathFinderInfo
	aystar->user_target = PathFinderInfo;

	// Set the start node
	start_node.parent = NULL;
	start_node.node.direction = 0;
	start_node.node.user_data[0] = 0;
	start_node.node.tile = PathFinderInfo->start_tile_tl;

	// Now we add all the starting tiles
	for (x = TileX(PathFinderInfo->start_tile_tl); x <= TileX(PathFinderInfo->start_tile_br); x++) {
		for (y = TileY(PathFinderInfo->start_tile_tl); y <= TileY(PathFinderInfo->start_tile_br); y++) {
			TileIndex tile = TileXY(x, y);

			if (!IsTileType(tile, MP_CLEAR) && !IsTileType(tile, MP_TREES)) continue;
			if (!TestCanBuildStationHere(tile, TEST_STATION_NO_DIR)) continue;
			start_node.node.tile = tile;
			aystar->addstart(aystar, &start_node.node, 0);
		}
	}
}


// The h-value, simple calculation
static int32 AyStar_AiPathFinder_CalculateH(AyStar *aystar, AyStarNode *current, OpenListNode *parent)
{
	const Ai_PathFinderInfo* PathFinderInfo = (Ai_PathFinderInfo*)aystar->user_target;
	int r, r2;

	if (PathFinderInfo->end_direction != AI_PATHFINDER_NO_DIRECTION) {
		// The station is pointing to a direction, add a tile towards that direction, so the H-value is more accurate
		r = DistanceManhattan(current->tile, PathFinderInfo->end_tile_tl + TileOffsByDiagDir(PathFinderInfo->end_direction));
		r2 = DistanceManhattan(current->tile, PathFinderInfo->end_tile_br + TileOffsByDiagDir(PathFinderInfo->end_direction));
	} else {
		// No direction, so just get the fastest route to the station
		r = DistanceManhattan(current->tile, PathFinderInfo->end_tile_tl);
		r2 = DistanceManhattan(current->tile, PathFinderInfo->end_tile_br);
	}
	// See if the bottomright is faster than the topleft..
	if (r2 < r) r = r2;
	return r * AI_PATHFINDER_H_MULTIPLER;
}


// We found the end.. let's get the route back and put it in an array
static void AyStar_AiPathFinder_FoundEndNode(AyStar *aystar, OpenListNode *current)
{
	Ai_PathFinderInfo *PathFinderInfo = (Ai_PathFinderInfo*)aystar->user_target;
	uint i = 0;
	PathNode *parent = &current->path;

	do {
		PathFinderInfo->route_extra[i] = parent->node.user_data[0];
		PathFinderInfo->route[i++] = parent->node.tile;
		if (i > lengthof(PathFinderInfo->route)) {
			// We ran out of space for the PathFinder
			DEBUG(ai, 0, "No more space in pathfinder route[] array");
			PathFinderInfo->route_length = -1; // -1 indicates out of space
			return;
		}
		parent = parent->parent;
	} while (parent != NULL);
	PathFinderInfo->route_length = i;
	DEBUG(ai, 1, "Found route of %d nodes long in %d nodes of searching", i, Hash_Size(&aystar->ClosedListHash));
}


// What tiles are around us.
static void AyStar_AiPathFinder_GetNeighbours(AyStar *aystar, OpenListNode *current)
{
	CommandCost ret;
	int dir;

	Ai_PathFinderInfo *PathFinderInfo = (Ai_PathFinderInfo*)aystar->user_target;

	aystar->num_neighbours = 0;

	// Go through all surrounding tiles and check if they are within the limits
	for (DiagDirection i = DIAGDIR_BEGIN; i < DIAGDIR_END; i++) {
		TileIndex ctile = current->path.node.tile; // Current tile
		TileIndex atile = ctile + TileOffsByDiagDir(i); // Adjacent tile

		if (TileX(atile) > 1 && TileX(atile) < MapMaxX() - 1 &&
				TileY(atile) > 1 && TileY(atile) < MapMaxY() - 1) {
			// We also directly test if the current tile can connect to this tile..
			//  We do this simply by just building the tile!

			// If the next step is a bridge, we have to enter it the right way
			if (!PathFinderInfo->rail_or_road && IsRoad(atile)) {
				if (IsTileType(atile, MP_TUNNELBRIDGE)) {
					if (GetTunnelBridgeDirection(atile) != i) continue;
				}
			}

			if ((AI_PATHFINDER_FLAG_BRIDGE & current->path.node.user_data[0]) != 0 ||
					(AI_PATHFINDER_FLAG_TUNNEL & current->path.node.user_data[0]) != 0) {
				// We are a bridge/tunnel, how cool!!
				//  This means we can only point forward.. get the direction from the user_data
				if ((uint)i != (current->path.node.user_data[0] >> 8)) continue;
			}
			dir = 0;

			// First, check if we have a parent
			if (current->path.parent == NULL && current->path.node.user_data[0] == 0) {
				// If not, this means we are at the starting station
				if (PathFinderInfo->start_direction != AI_PATHFINDER_NO_DIRECTION) {
					// We do need a direction?
					if (AiNew_GetDirection(ctile, atile) != PathFinderInfo->start_direction) {
						// We are not pointing the right way, invalid tile
						continue;
					}
				}
			} else if (current->path.node.user_data[0] == 0) {
				if (PathFinderInfo->rail_or_road) {
					// Rail check
					dir = AiNew_GetRailDirection(current->path.parent->node.tile, ctile, atile);
					ret = AI_DoCommand(ctile, 0, dir, DC_AUTO | DC_NO_WATER, CMD_BUILD_SINGLE_RAIL);
					if (CmdFailed(ret)) continue;
#ifdef AI_PATHFINDER_NO_90DEGREES_TURN
					if (current->path.parent->parent != NULL) {
						// Check if we don't make a 90degree curve
						int dir1 = AiNew_GetRailDirection(current->path.parent->parent->node.tile, current->path.parent->node.tile, ctile);
						if (_illegal_curves[dir1] == dir || _illegal_curves[dir] == dir1) {
							continue;
						}
					}
#endif
				} else {
					// Road check
					dir = AiNew_GetRoadDirection(current->path.parent->node.tile, ctile, atile);
					if (IsRoad(ctile)) {
						if (IsTileType(ctile, MP_TUNNELBRIDGE)) {
							// We have a bridge, how nicely! We should mark it...
							dir = 0;
						} else {
							// It already has road.. check if we miss any bits!
							if ((GetAnyRoadBits(ctile, ROADTYPE_ROAD) & dir) != dir) {
								// We do miss some pieces :(
								dir &= ~GetAnyRoadBits(ctile, ROADTYPE_ROAD);
							} else {
								dir = 0;
							}
						}
					}
					// Only destruct things if it is MP_CLEAR of MP_TREES
					if (dir != 0) {
						ret = AI_DoCommand(ctile, dir, 0, DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD);
						if (CmdFailed(ret)) continue;
					}
				}
			}

			// The tile can be connected
			aystar->neighbours[aystar->num_neighbours].tile = atile;
			aystar->neighbours[aystar->num_neighbours].user_data[0] = 0;
			aystar->neighbours[aystar->num_neighbours++].direction = 0;
		}
	}

	// Next step, check for bridges and tunnels
	if (current->path.parent != NULL && current->path.node.user_data[0] == 0) {
		// First we get the dir from this tile and his parent
		DiagDirection dir = AiNew_GetDirection(current->path.parent->node.tile, current->path.node.tile);
		// It means we can only walk with the track, so the bridge has to be in the same direction
		TileIndex tile = current->path.node.tile;
		TileIndex new_tile = tile;
		Slope tileh = GetTileSlope(tile, NULL);

		// Bridges can only be build on land that is not flat
		//  And if there is a road or rail blocking
		if (tileh != SLOPE_FLAT ||
				(PathFinderInfo->rail_or_road && IsTileType(tile + TileOffsByDiagDir(dir), MP_ROAD)) ||
				(!PathFinderInfo->rail_or_road && IsTileType(tile + TileOffsByDiagDir(dir), MP_RAILWAY))) {
			for (;;) {
				new_tile += TileOffsByDiagDir(dir);

				// Precheck, is the length allowed?
				if (!CheckBridge_Stuff(0, GetBridgeLength(tile, new_tile))) break;

				// Check if we hit the station-tile.. we don't like that!
				if (TILES_BETWEEN(new_tile, PathFinderInfo->end_tile_tl, PathFinderInfo->end_tile_br)) break;

				// Try building the bridge..
				ret = AI_DoCommand(tile, new_tile, (0 << 8) + (MAX_BRIDGES / 2), DC_AUTO, CMD_BUILD_BRIDGE);
				if (CmdFailed(ret)) continue;
				// We can build a bridge here.. add him to the neighbours
				aystar->neighbours[aystar->num_neighbours].tile = new_tile;
				aystar->neighbours[aystar->num_neighbours].user_data[0] = AI_PATHFINDER_FLAG_BRIDGE + (dir << 8);
				aystar->neighbours[aystar->num_neighbours++].direction = 0;
				// We can only have 12 neighbours, and we need 1 left for tunnels
				if (aystar->num_neighbours == 11) break;
			}
		}

		// Next, check for tunnels!
		// Tunnels can only be built on slopes corresponding to the direction
		//  For now, we check both sides for this tile.. terraforming gives fuzzy result
		if ((dir == DIAGDIR_NE && tileh == SLOPE_NE) ||
				(dir == DIAGDIR_SE && tileh == SLOPE_SE) ||
				(dir == DIAGDIR_SW && tileh == SLOPE_SW) ||
				(dir == DIAGDIR_NW && tileh == SLOPE_NW)) {
			// Now simply check if a tunnel can be build
			ret = AI_DoCommand(tile, (PathFinderInfo->rail_or_road?0:0x200), 0, DC_AUTO, CMD_BUILD_TUNNEL);
			tileh = GetTileSlope(_build_tunnel_endtile, NULL);
			if (CmdSucceeded(ret) && (tileh == SLOPE_SW || tileh == SLOPE_SE || tileh == SLOPE_NW || tileh == SLOPE_NE)) {
				aystar->neighbours[aystar->num_neighbours].tile = _build_tunnel_endtile;
				aystar->neighbours[aystar->num_neighbours].user_data[0] = AI_PATHFINDER_FLAG_TUNNEL + (dir << 8);
				aystar->neighbours[aystar->num_neighbours++].direction = 0;
			}
		}
	}
}


extern Foundation GetRailFoundation(Slope tileh, TrackBits bits); // XXX function declaration in .c
extern Foundation GetRoadFoundation(Slope tileh, RoadBits bits); // XXX function declaration in .c
extern Foundation GetBridgeFoundation(Slope tileh, Axis); // XXX function declaration in .c
enum BridgeFoundation {
	BRIDGE_NO_FOUNDATION = 1 << 0 | 1 << 3 | 1 << 6 | 1 << 9 | 1 << 12,
};

// The most important function: it calculates the g-value
static int32 AyStar_AiPathFinder_CalculateG(AyStar *aystar, AyStarNode *current, OpenListNode *parent)
{
	Ai_PathFinderInfo *PathFinderInfo = (Ai_PathFinderInfo*)aystar->user_target;
	int res = 0;
	Slope tileh = GetTileSlope(current->tile, NULL);
	Slope parent_tileh = GetTileSlope(parent->path.node.tile, NULL);

	// Check if we hit the end-tile
	if (TILES_BETWEEN(current->tile, PathFinderInfo->end_tile_tl, PathFinderInfo->end_tile_br)) {
		// We are at the end-tile, check if we had a direction or something...
		if (PathFinderInfo->end_direction != AI_PATHFINDER_NO_DIRECTION && AiNew_GetDirection(current->tile, parent->path.node.tile) != PathFinderInfo->end_direction) {
			// We are not pointing the right way, invalid tile
			return AYSTAR_INVALID_NODE;
		}
		// If it was valid, drop out.. we don't build on the endtile
		return 0;
	}

	// Give everything a small penalty
	res += AI_PATHFINDER_PENALTY;

	if (!PathFinderInfo->rail_or_road) {
		// Road has the lovely advantage it can use other road... check if
		//  the current tile is road, and if so, give a good bonus
		if (IsRoad(current->tile)) {
			res -= AI_PATHFINDER_ROAD_ALREADY_EXISTS_BONUS;
		}
	}

	// We should give a penalty when the tile is going up or down.. this is one way to do so!
	//  Too bad we have to count it from the parent.. but that is not so bad.
	// We also dislike long routes on slopes, since they do not look too realistic
	//  when there is a flat land all around, they are more expensive to build, and
	//  especially they essentially block the ability to connect or cross the road
	//  from one side.
	if (parent_tileh != SLOPE_FLAT && parent->path.parent != NULL) {
		// Skip if the tile was from a bridge or tunnel
		if (parent->path.node.user_data[0] == 0 && current->user_data[0] == 0) {
			if (PathFinderInfo->rail_or_road) {
				Foundation f = GetRailFoundation(parent_tileh, (TrackBits)(1 << AiNew_GetRailDirection(parent->path.parent->node.tile, parent->path.node.tile, current->tile)));
				// Maybe is BRIDGE_NO_FOUNDATION a bit strange here, but it contains just the right information..
				if (IsInclinedFoundation(f) || (!IsFoundation(f) && HasBit(BRIDGE_NO_FOUNDATION, parent_tileh))) {
					res += AI_PATHFINDER_TILE_GOES_UP_PENALTY;
				} else {
					res += AI_PATHFINDER_FOUNDATION_PENALTY;
				}
			} else {
				if (!IsRoad(parent->path.node.tile) || !IsTileType(parent->path.node.tile, MP_TUNNELBRIDGE)) {
					Foundation f = GetRoadFoundation(parent_tileh, (RoadBits)AiNew_GetRoadDirection(parent->path.parent->node.tile, parent->path.node.tile, current->tile));
					if (IsInclinedFoundation(f) || (!IsFoundation(f) && HasBit(BRIDGE_NO_FOUNDATION, parent_tileh))) {
						res += AI_PATHFINDER_TILE_GOES_UP_PENALTY;
					} else {
						res += AI_PATHFINDER_FOUNDATION_PENALTY;
					}
				}
			}
		}
	}

	// Are we part of a tunnel?
	if ((AI_PATHFINDER_FLAG_TUNNEL & current->user_data[0]) != 0) {
		int r;
		// Tunnels are very expensive when build on long routes..
		// Ironicly, we are using BridgeCode here ;)
		r = AI_PATHFINDER_TUNNEL_PENALTY * GetBridgeLength(current->tile, parent->path.node.tile);
		res += r + (r >> 8);
	}

	// Are we part of a bridge?
	if ((AI_PATHFINDER_FLAG_BRIDGE & current->user_data[0]) != 0) {
		// That means for every length a penalty
		res += AI_PATHFINDER_BRIDGE_PENALTY * GetBridgeLength(current->tile, parent->path.node.tile);
		// Check if we are going up or down, first for the starting point
		// In user_data[0] is at the 8th bit the direction
		if (!HasBit(BRIDGE_NO_FOUNDATION, parent_tileh)) {
			if (IsLeveledFoundation(GetBridgeFoundation(parent_tileh, (Axis)((current->user_data[0] >> 8) & 1)))) {
				res += AI_PATHFINDER_BRIDGE_GOES_UP_PENALTY;
			}
		}
		// Second for the end point
		if (!HasBit(BRIDGE_NO_FOUNDATION, tileh)) {
			if (IsLeveledFoundation(GetBridgeFoundation(tileh, (Axis)((current->user_data[0] >> 8) & 1)))) {
				res += AI_PATHFINDER_BRIDGE_GOES_UP_PENALTY;
			}
		}
		if (parent_tileh == SLOPE_FLAT) res += AI_PATHFINDER_BRIDGE_GOES_UP_PENALTY;
		if (tileh == SLOPE_FLAT) res += AI_PATHFINDER_BRIDGE_GOES_UP_PENALTY;
	}

	//  To prevent the AI from taking the fastest way in tiles, but not the fastest way
	//    in speed, we have to give a good penalty to direction changing
	//  This way, we get almost the fastest way in tiles, and a very good speed on the track
	if (!PathFinderInfo->rail_or_road) {
		if (parent->path.parent != NULL &&
				AiNew_GetDirection(current->tile, parent->path.node.tile) != AiNew_GetDirection(parent->path.node.tile, parent->path.parent->node.tile)) {
			// When road exists, we don't like turning, but its free, so don't be to piggy about it
			if (IsRoad(parent->path.node.tile)) {
				res += AI_PATHFINDER_DIRECTION_CHANGE_ON_EXISTING_ROAD_PENALTY;
			} else {
				res += AI_PATHFINDER_DIRECTION_CHANGE_PENALTY;
			}
		}
	} else {
		// For rail we have 1 exeption: diagonal rail..
		// So we fetch 2 raildirection. That of the current one, and of the one before that
		if (parent->path.parent != NULL && parent->path.parent->parent != NULL) {
			int dir1 = AiNew_GetRailDirection(parent->path.parent->node.tile, parent->path.node.tile, current->tile);
			int dir2 = AiNew_GetRailDirection(parent->path.parent->parent->node.tile, parent->path.parent->node.tile, parent->path.node.tile);
			// First, see if we are on diagonal path, that is better than straight path
			if (dir1 > 1) res -= AI_PATHFINDER_DIAGONAL_BONUS;

			// First see if they are different
			if (dir1 != dir2) {
				// dir 2 and 3 are 1 diagonal track, and 4 and 5.
				if (!(((dir1 == 2 || dir1 == 3) && (dir2 == 2 || dir2 == 3)) || ((dir1 == 4 || dir1 == 5) && (dir2 == 4 || dir2 == 5)))) {
					// It is not, so we changed of direction
					res += AI_PATHFINDER_DIRECTION_CHANGE_PENALTY;
				}
				if (parent->path.parent->parent->parent != NULL) {
					int dir3 = AiNew_GetRailDirection(parent->path.parent->parent->parent->node.tile, parent->path.parent->parent->node.tile, parent->path.parent->node.tile);
					// Check if we changed 3 tiles of direction in 3 tiles.. bad!!!
					if ((dir1 == 0 || dir1 == 1) && dir2 > 1 && (dir3 == 0 || dir3 == 1)) {
						res += AI_PATHFINDER_CURVE_PENALTY;
					}
				}
			}
		}
	}

	return (res < 0) ? 0 : res;
}
