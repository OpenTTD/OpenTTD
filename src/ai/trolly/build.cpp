/* $Id$ */

#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../debug.h"
#include "../../functions.h"
#include "../../map.h"
#include "../../road_map.h"
#include "../../tile.h"
#include "../../vehicle.h"
#include "../../command.h"
#include "trolly.h"
#include "../../engine.h"
#include "../../station.h"
#include "../../variables.h"
#include "../../bridge.h"
#include "../ai.h"

// Build HQ
//  Params:
//    tile : tile where HQ is going to be build
bool AiNew_Build_CompanyHQ(Player *p, TileIndex tile)
{
	if (CmdFailed(AI_DoCommand(tile, 0, 0, DC_AUTO | DC_NO_WATER, CMD_BUILD_COMPANY_HQ)))
		return false;
	AI_DoCommand(tile, 0, 0, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_BUILD_COMPANY_HQ);
	return true;
}


// Build station
//  Params:
//    type : AI_TRAIN/AI_BUS/AI_TRUCK : indicates the type of station
//    tile : tile where station is going to be build
//    length : in case of AI_TRAIN: length of station
//    numtracks : in case of AI_TRAIN: tracks of station
//    direction : the direction of the station
//    flag : flag passed to DoCommand (normally 0 to get the cost or DC_EXEC to build it)
int AiNew_Build_Station(Player *p, byte type, TileIndex tile, byte length, byte numtracks, byte direction, byte flag)
{
	if (type == AI_TRAIN)
		return AI_DoCommand(tile, direction + (numtracks << 8) + (length << 16), 0, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_RAILROAD_STATION);

	if (type == AI_BUS)
		return AI_DoCommand(tile, direction, RoadStop::BUS, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD_STOP);

	return AI_DoCommand(tile, direction, RoadStop::TRUCK, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD_STOP);
}


// Builds a brdige. The second best out of the ones available for this player
//  Params:
//   tile_a : starting point
//   tile_b : end point
//   flag : flag passed to DoCommand
int AiNew_Build_Bridge(Player *p, TileIndex tile_a, TileIndex tile_b, byte flag)
{
	int bridge_type, bridge_len, type, type2;

	// Find a good bridgetype (the best money can buy)
	bridge_len = GetBridgeLength(tile_a, tile_b);
	type = type2 = 0;
	for (bridge_type = MAX_BRIDGES-1; bridge_type >= 0; bridge_type--) {
		if (CheckBridge_Stuff(bridge_type, bridge_len)) {
			type2 = type;
			type = bridge_type;
			// We found two bridges, exit
			if (type2 != 0) break;
		}
	}
	// There is only one bridge that can be built
	if (type2 == 0 && type != 0) type2 = type;

	// Now, simply, build the bridge!
	if (p->ainew.tbt == AI_TRAIN) {
		return AI_DoCommand(tile_a, tile_b, (0x00 << 8) + type2, flag | DC_AUTO, CMD_BUILD_BRIDGE);
	} else {
		return AI_DoCommand(tile_a, tile_b, (0x80 << 8) + type2, flag | DC_AUTO, CMD_BUILD_BRIDGE);
	}
}


// Build the route part by part
// Basicly what this function do, is build that amount of parts of the route
//  that go in the same direction. It sets 'part' to the last part of the route builded.
//  The return value is the cost for the builded parts
//
//  Params:
//   PathFinderInfo : Pointer to the PathFinderInfo used for AiPathFinder
//   part : Which part we need to build
//
// TODO: skip already builded road-pieces (e.g.: cityroad)
int AiNew_Build_RoutePart(Player *p, Ai_PathFinderInfo *PathFinderInfo, byte flag)
{
	int part = PathFinderInfo->position;
	byte *route_extra = PathFinderInfo->route_extra;
	TileIndex *route = PathFinderInfo->route;
	int dir;
	int old_dir = -1;
	int cost = 0;
	int res;
	// We need to calculate the direction with the parent of the parent.. so we skip
	//  the first pieces and the last piece
	if (part < 1) part = 1;
	// When we are done, stop it
	if (part >= PathFinderInfo->route_length - 1) {
		PathFinderInfo->position = -2;
		return 0;
	}


	if (PathFinderInfo->rail_or_road) {
		// Tunnel code
		if ((AI_PATHFINDER_FLAG_TUNNEL & route_extra[part]) != 0) {
			cost += AI_DoCommand(route[part], 0, 0, flag, CMD_BUILD_TUNNEL);
			PathFinderInfo->position++;
			// TODO: problems!
			if (CmdFailed(cost)) {
				DEBUG(ai, 0, "[BuildPath] tunnel could not be built (0x%X)", route[part]);
				return 0;
			}
			return cost;
		}
		// Bridge code
		if ((AI_PATHFINDER_FLAG_BRIDGE & route_extra[part]) != 0) {
			cost += AiNew_Build_Bridge(p, route[part], route[part - 1], flag);
			PathFinderInfo->position++;
			// TODO: problems!
			if (CmdFailed(cost)) {
				DEBUG(ai, 0, "[BuildPath] bridge could not be built (0x%X, 0x%X)", route[part], route[part - 1]);
				return 0;
			}
			return cost;
		}

		// Build normal rail
		// Keep it doing till we go an other way
		if (route_extra[part - 1] == 0 && route_extra[part] == 0) {
			while (route_extra[part] == 0) {
				// Get the current direction
				dir = AiNew_GetRailDirection(route[part-1], route[part], route[part+1]);
				// Is it the same as the last one?
				if (old_dir != -1 && old_dir != dir) break;
				old_dir = dir;
				// Build the tile
				res = AI_DoCommand(route[part], 0, dir, flag, CMD_BUILD_SINGLE_RAIL);
				if (CmdFailed(res)) {
					// Problem.. let's just abort it all!
					p->ainew.state = AI_STATE_NOTHING;
					return 0;
				}
				cost += res;
				// Go to the next tile
				part++;
				// Check if it is still in range..
				if (part >= PathFinderInfo->route_length - 1) break;
			}
			part--;
		}
		// We want to return the last position, so we go back one
		PathFinderInfo->position = part;
	} else {
		// Tunnel code
		if ((AI_PATHFINDER_FLAG_TUNNEL & route_extra[part]) != 0) {
			cost += AI_DoCommand(route[part], 0x200, 0, flag, CMD_BUILD_TUNNEL);
			PathFinderInfo->position++;
			// TODO: problems!
			if (CmdFailed(cost)) {
				DEBUG(ai, 0, "[BuildPath] tunnel could not be built (0x%X)", route[part]);
				return 0;
			}
			return cost;
		}
		// Bridge code
		if ((AI_PATHFINDER_FLAG_BRIDGE & route_extra[part]) != 0) {
			cost += AiNew_Build_Bridge(p, route[part], route[part + 1], flag);
			PathFinderInfo->position++;
			// TODO: problems!
			if (CmdFailed(cost)) {
				DEBUG(ai, 0, "[BuildPath] bridge could not be built (0x%X, 0x%X)", route[part], route[part + 1]);
				return 0;
			}
			return cost;
		}

		// Build normal road
		// Keep it doing till we go an other way
		// EnsureNoVehicle makes sure we don't build on a tile where a vehicle is. This way
		//  it will wait till the vehicle is gone..
		if (route_extra[part-1] == 0 && route_extra[part] == 0 && (flag != DC_EXEC || EnsureNoVehicle(route[part]))) {
			while (route_extra[part] == 0 && (flag != DC_EXEC || EnsureNoVehicle(route[part]))) {
				// Get the current direction
				dir = AiNew_GetRoadDirection(route[part-1], route[part], route[part+1]);
				// Is it the same as the last one?
				if (old_dir != -1 && old_dir != dir) break;
				old_dir = dir;
				// There is already some road, and it is a bridge.. don't build!!!
				if (!IsTileType(route[part], MP_TUNNELBRIDGE)) {
					// Build the tile
					res = AI_DoCommand(route[part], dir, 0, flag | DC_NO_WATER, CMD_BUILD_ROAD);
					// Currently, we ignore CMD_ERRORs!
					if (CmdFailed(res) && flag == DC_EXEC && !IsTileType(route[part], MP_STREET) && !EnsureNoVehicle(route[part])) {
						// Problem.. let's just abort it all!
						DEBUG(ai, 0, "[BuidPath] route building failed at tile 0x%X, aborting", route[part]);
						p->ainew.state = AI_STATE_NOTHING;
						return 0;
					}

					if (!CmdFailed(res)) cost += res;
				}
				// Go to the next tile
				part++;
				// Check if it is still in range..
				if (part >= PathFinderInfo->route_length - 1) break;
			}
			part--;
			// We want to return the last position, so we go back one
		}
		if (!EnsureNoVehicle(route[part]) && flag == DC_EXEC) part--;
		PathFinderInfo->position = part;
	}

	return cost;
}


// This functions tries to find the best vehicle for this type of cargo
// It returns INVALID_ENGINE if not suitable engine is found
EngineID AiNew_PickVehicle(Player *p)
{
	if (p->ainew.tbt == AI_TRAIN) {
		// Not supported yet
		return INVALID_ENGINE;
	} else {
		EngineID best_veh_index = INVALID_ENGINE;
		int32 best_veh_rating = 0;
		EngineID start = ROAD_ENGINES_INDEX;
		EngineID end   = ROAD_ENGINES_INDEX + NUM_ROAD_ENGINES;
		EngineID i;

		/* Loop through all road vehicles */
		for (i = start; i != end; i++) {
			const RoadVehicleInfo *rvi = RoadVehInfo(i);
			const Engine* e = GetEngine(i);
			int32 rating;
			int32 ret;

			/* Skip vehicles which can't take our cargo type */
			if (rvi->cargo_type != p->ainew.cargo && !CanRefitTo(i, p->ainew.cargo)) continue;

			// Is it availiable?
			// Also, check if the reliability of the vehicle is above the AI_VEHICLE_MIN_RELIABILTY
			if (!HASBIT(e->player_avail, _current_player) || e->reliability * 100 < AI_VEHICLE_MIN_RELIABILTY << 16) continue;

			/* Rate and compare the engine by speed & capacity */
			rating = rvi->max_speed * rvi->capacity;
			if (rating <= best_veh_rating) continue;

			// Can we build it?
			ret = AI_DoCommand(0, i, 0, DC_QUERY_COST, CMD_BUILD_ROAD_VEH);
			if (CmdFailed(ret)) continue;

			best_veh_rating = rating;
			best_veh_index = i;
		}

		return best_veh_index;
	}
}


void CcAI(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	Player* p = GetPlayer(_current_player);

	if (success) {
		p->ainew.state = AI_STATE_GIVE_ORDERS;
		p->ainew.veh_id = _new_vehicle_id;

		if (GetVehicle(p->ainew.veh_id)->cargo_type != p->ainew.cargo) {
			/* Cargo type doesn't match, so refit it */
			if (CmdFailed(DoCommand(tile, p->ainew.veh_id, p->ainew.cargo, DC_EXEC, CMD_REFIT_ROAD_VEH))) {
				/* Refit failed, so sell the vehicle */
				DoCommand(tile, p->ainew.veh_id, 0, DC_EXEC, CMD_SELL_ROAD_VEH);
				p->ainew.state = AI_STATE_NOTHING;
			}
		}
	} else {
		/* XXX this should be handled more gracefully */
		p->ainew.state = AI_STATE_NOTHING;
	}
}


// Builds the best vehicle possible
int AiNew_Build_Vehicle(Player *p, TileIndex tile, byte flag)
{
	EngineID i = AiNew_PickVehicle(p);

	if (i == INVALID_ENGINE) return CMD_ERROR;
	if (p->ainew.tbt == AI_TRAIN) return CMD_ERROR;

	if (flag & DC_EXEC) {
		return AI_DoCommandCc(tile, i, 0, flag, CMD_BUILD_ROAD_VEH, CcAI);
	} else {
		return AI_DoCommand(tile, i, 0, flag, CMD_BUILD_ROAD_VEH);
	}
}

int AiNew_Build_Depot(Player* p, TileIndex tile, DiagDirection direction, byte flag)
{
	int ret, ret2;
	if (p->ainew.tbt == AI_TRAIN) {
		return AI_DoCommand(tile, 0, direction, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_TRAIN_DEPOT);
	} else {
		ret = AI_DoCommand(tile, direction, 0, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD_DEPOT);
		if (CmdFailed(ret)) return ret;
		// Try to build the road from the depot
		ret2 = AI_DoCommand(tile + TileOffsByDiagDir(direction), DiagDirToRoadBits(ReverseDiagDir(direction)), 0, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD);
		// If it fails, ignore it..
		if (CmdFailed(ret2)) return ret;
		return ret + ret2;
	}
}
