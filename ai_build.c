#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "map.h"
#include "tile.h"
#include "command.h"
#include "ai_new.h"
#include "engine.h"
#include "station.h"
#include "variables.h"

// Build HQ
//  Params:
//    tile : tile where HQ is going to be build
bool AiNew_Build_CompanyHQ(Player *p, TileIndex tile)
{
	if (CmdFailed(DoCommandByTile(tile, 0, 0, DC_AUTO | DC_NO_WATER, CMD_BUILD_COMPANY_HQ)))
		return false;
	DoCommandByTile(tile, 0, 0, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_BUILD_COMPANY_HQ);
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
		return DoCommandByTile(tile, direction + (numtracks << 8) + (length << 16), 0, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_RAILROAD_STATION);

	if (type == AI_BUS)
		return DoCommandByTile(tile, direction, RS_BUS, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD_STOP);

	return DoCommandByTile(tile, direction, RS_TRUCK, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD_STOP);
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
			if (type2 != 0)
				break;
		}
	}
	// There is only one bridge that can be build..
	if (type2 == 0 && type != 0) type2 = type;

	// Now, simply, build the bridge!
	if (p->ainew.tbt == AI_TRAIN)
		return DoCommandByTile(tile_a, tile_b, (0<<8) + type2, flag | DC_AUTO, CMD_BUILD_BRIDGE);

	return DoCommandByTile(tile_a, tile_b, (0x80 << 8) + type2, flag | DC_AUTO, CMD_BUILD_BRIDGE);
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
int AiNew_Build_RoutePart(Player *p, Ai_PathFinderInfo *PathFinderInfo, byte flag) {
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
	if (part >= PathFinderInfo->route_length - 1) { PathFinderInfo->position = -2; return 0; }


	if (PathFinderInfo->rail_or_road) {
		// Tunnel code
     	if ((AI_PATHFINDER_FLAG_TUNNEL & route_extra[part]) != 0) {
     		cost += DoCommandByTile(route[part], 0, 0, flag, CMD_BUILD_TUNNEL);
     		PathFinderInfo->position++;
     		// TODO: problems!
     		if (CmdFailed(cost)) {
     			DEBUG(ai,0)("[AiNew - BuildPath] We have a serious problem: tunnel could not be build!");
				return 0;
     		}
     		return cost;
     	}
     	// Bridge code
     	if ((AI_PATHFINDER_FLAG_BRIDGE & route_extra[part]) != 0) {
     		cost += AiNew_Build_Bridge(p, route[part], route[part-1], flag);
     		PathFinderInfo->position++;
     		// TODO: problems!
     		if (CmdFailed(cost)) {
     			DEBUG(ai,0)("[AiNew - BuildPath] We have a serious problem: bridge could not be build!");
				return 0;
     		}
     		return cost;
     	}

     	// Build normal rail
     	// Keep it doing till we go an other way
     	if (route_extra[part-1] == 0 && route_extra[part] == 0) {
     		while (route_extra[part] == 0) {
	     		// Get the current direction
	     		dir = AiNew_GetRailDirection(route[part-1], route[part], route[part+1]);
	     		// Is it the same as the last one?
	     		if (old_dir != -1 && old_dir != dir) break;
	     		old_dir = dir;
	     		// Build the tile
	     		res = DoCommandByTile(route[part], 0, dir, flag, CMD_BUILD_SINGLE_RAIL);
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
     		cost += DoCommandByTile(route[part], 0x200, 0, flag, CMD_BUILD_TUNNEL);
     		PathFinderInfo->position++;
     		// TODO: problems!
     		if (CmdFailed(cost)) {
     			DEBUG(ai,0)("[AiNew - BuildPath] We have a serious problem: tunnel could not be build!");
				return 0;
     		}
     		return cost;
     	}
     	// Bridge code
     	if ((AI_PATHFINDER_FLAG_BRIDGE & route_extra[part]) != 0) {
     		cost += AiNew_Build_Bridge(p, route[part], route[part+1], flag);
     		PathFinderInfo->position++;
     		// TODO: problems!
     		if (CmdFailed(cost)) {
     			DEBUG(ai,0)("[AiNew - BuildPath] We have a serious problem: bridge could not be build!");
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
	     			res = DoCommandByTile(route[part], dir, 0, flag | DC_NO_WATER, CMD_BUILD_ROAD);
	     			// Currently, we ignore CMD_ERRORs!
						if (CmdFailed(res) && flag == DC_EXEC && !IsTileType(route[part], MP_STREET) && !EnsureNoVehicle(route[part])) {
							// Problem.. let's just abort it all!
							DEBUG(ai,0)("Darn, the route could not be builded.. aborting!");
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
// It returns vehicle_id or -1 if not found
int AiNew_PickVehicle(Player *p) {
    if (p->ainew.tbt == AI_TRAIN) {
        // Not supported yet
        return -1;
    } else {
        int start, count, i, ret = CMD_ERROR;
        start = _cargoc.ai_roadveh_start[p->ainew.cargo];
        count = _cargoc.ai_roadveh_count[p->ainew.cargo];

        // Let's check it backwards.. we simply want to best engine available..
        for (i=start+count-1;i>=start;i--) {
        	// Is it availiable?
        	// Also, check if the reliability of the vehicle is above the AI_VEHICLE_MIN_RELIABILTY
        	if (!HASBIT(GetEngine(i)->player_avail, _current_player) || GetEngine(i)->reliability * 100 < AI_VEHICLE_MIN_RELIABILTY << 16) continue;
        	// Can we build it?
        	ret = DoCommandByTile(0, i, 0, DC_QUERY_COST, CMD_BUILD_ROAD_VEH);
        	if (!CmdFailed(ret)) break;
       	}
       	// We did not find a vehicle :(
       	if (CmdFailed(ret)) { return -1; }
       	return i;
    }
}

// Builds the best vehicle possible
int AiNew_Build_Vehicle(Player *p, TileIndex tile, byte flag)
{
	int i = AiNew_PickVehicle(p);
	if (i == -1) return CMD_ERROR;

	if (p->ainew.tbt == AI_TRAIN)
		return CMD_ERROR;

	return DoCommandByTile(tile, i, 0, flag, CMD_BUILD_ROAD_VEH);
}

int AiNew_Build_Depot(Player *p, TileIndex tile, byte direction, byte flag)
{
	static const byte _roadbits_by_dir[4] = {2,1,8,4};
	int ret, ret2;
    if (p->ainew.tbt == AI_TRAIN)
    	return DoCommandByTile(tile, 0, direction, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_TRAIN_DEPOT);

		ret = DoCommandByTile(tile, direction, 0, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD_DEPOT);
		if (CmdFailed(ret)) return ret;
		// Try to build the road from the depot
		ret2 = DoCommandByTile(tile + TileOffsByDir(direction), _roadbits_by_dir[direction], 0, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD);
		// If it fails, ignore it..
		if (CmdFailed(ret2)) return ret;
		return ret + ret2;
}
