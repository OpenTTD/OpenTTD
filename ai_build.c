#include "stdafx.h"
#include "ttd.h"
#include "command.h"
#include "ai.h"
#include "engine.h"

// Build HQ
//  Params:
//    tile : tile where HQ is going to be build
bool AiNew_Build_CompanyHQ(Player *p, uint tile) {
	if (DoCommandByTile(tile, 0, 0, DC_AUTO | DC_NO_WATER, CMD_BUILD_COMPANY_HQ) == CMD_ERROR)
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
int AiNew_Build_Station(Player *p, byte type, uint tile, byte length, byte numtracks, byte direction, byte flag) {
	if (type == AI_TRAIN)
		return DoCommandByTile(tile, direction + (numtracks << 8) + (length << 16), 0, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_RAILROAD_STATION);
	else if (type == AI_BUS)
		return DoCommandByTile(tile, direction, 0, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_BUS_STATION);
	else
		return DoCommandByTile(tile, direction, 0, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_TRUCK_STATION);
}

// Builds a brdige. The second best out of the ones available for this player
//  Params:
//   tile_a : starting point
//   tile_b : end point
//   flag : flag passed to DoCommand
int AiNew_Build_Bridge(Player *p, uint tile_a, uint tile_b, byte flag) {
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
	else
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
     		if (cost == CMD_ERROR) {
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
     		if (cost == CMD_ERROR) {
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
	     		if (res == CMD_ERROR) {
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
     		if (cost == CMD_ERROR) {
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
     		if (cost == CMD_ERROR) {
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
	     		if (!IS_TILETYPE(route[part], MP_TUNNELBRIDGE)) {
	     			// Build the tile
	     			res = DoCommandByTile(route[part], dir, 0, flag | DC_NO_WATER, CMD_BUILD_ROAD);
	     			// Currently, we ignore CMD_ERRORs!
	     			if (res == CMD_ERROR && flag == DC_EXEC && !IS_TILETYPE(route[part], MP_STREET) && !EnsureNoVehicle(route[part])) {
     					// Problem.. let's just abort it all!
     					DEBUG(ai,0)("Darn, the route could not be builded.. aborting!");
    	     			p->ainew.state = AI_STATE_NOTHING;
    	     			return 0;
    	     		} else {
    	     			if (res != CMD_ERROR)
    	     				cost += res;
    	     		}
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
        int start, count, i, r = CMD_ERROR;
        start = _cargoc.ai_roadveh_start[p->ainew.cargo];
        count = _cargoc.ai_roadveh_count[p->ainew.cargo];

        // Let's check it backwards.. we simply want to best engine available..
        for (i=start+count-1;i>=start;i--) {
        	// Is it availiable?
        	// Also, check if the reliability of the vehicle is above the AI_VEHICLE_MIN_RELIABILTY
        	if (!HASBIT(_engines[i].player_avail, _current_player) || _engines[i].reliability * 100 < AI_VEHICLE_MIN_RELIABILTY << 16) continue;
        	// Can we build it?
        	r = DoCommandByTile(0, i, 0, DC_QUERY_COST, CMD_BUILD_ROAD_VEH);
        	if (r != CMD_ERROR) break;
       	}
       	// We did not find a vehicle :(
       	if (r == CMD_ERROR) { return -1; }
       	return i;
    }
}

// Builds the best vehicle possible
int AiNew_Build_Vehicle(Player *p, uint tile, byte flag) {
	int i = AiNew_PickVehicle(p);
	if (i == -1) return CMD_ERROR;
	
	if (p->ainew.tbt == AI_TRAIN) {
		return CMD_ERROR;
	} else {
		return DoCommandByTile(tile, i, 0, flag, CMD_BUILD_ROAD_VEH);
	}
}

int AiNew_Build_Depot(Player *p, uint tile, byte direction, byte flag) {
	static const byte _roadbits_by_dir[4] = {2,1,8,4};
	int r, r2;
    if (p->ainew.tbt == AI_TRAIN) {
    	return DoCommandByTile(tile, 0, direction, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_TRAIN_DEPOT);
    } else {
    	r = DoCommandByTile(tile, direction, 0, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD_DEPOT);
    	if (r == CMD_ERROR) return r;
    	// Try to build the road from the depot
    	r2 = DoCommandByTile(tile + _tileoffs_by_dir[direction], _roadbits_by_dir[direction], 0, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD);
    	// If it fails, ignore it..
    	if (r2 == CMD_ERROR) return r;
    	return r + r2;
    }
}
