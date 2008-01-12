/* $Id$ */

#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../debug.h"
#include "../../map_func.h"
#include "../../vehicle_base.h"
#include "../../player_base.h"
#include "trolly.h"

int AiNew_GetRailDirection(TileIndex tile_a, TileIndex tile_b, TileIndex tile_c)
{
	// 0 = vert
	// 1 = horz
	// 2 = dig up-left
	// 3 = dig down-right
	// 4 = dig down-left
	// 5 = dig up-right

	uint x1 = TileX(tile_a);
	uint x2 = TileX(tile_b);
	uint x3 = TileX(tile_c);

	uint y1 = TileY(tile_a);
	uint y2 = TileY(tile_b);
	uint y3 = TileY(tile_c);

	if (y1 == y2 && y2 == y3) return 0;
	if (x1 == x2 && x2 == x3) return 1;
	if (y2 > y1) return x2 > x3 ? 2 : 4;
	if (x2 > x1) return y2 > y3 ? 2 : 5;
	if (y1 > y2) return x2 > x3 ? 5 : 3;
	if (x1 > x2) return y2 > y3 ? 4 : 3;

	return 0;
}

int AiNew_GetRoadDirection(TileIndex tile_a, TileIndex tile_b, TileIndex tile_c)
{
	int x1, x2, x3;
	int y1, y2, y3;
	int r;

	x1 = TileX(tile_a);
	x2 = TileX(tile_b);
	x3 = TileX(tile_c);

	y1 = TileY(tile_a);
	y2 = TileY(tile_b);
	y3 = TileY(tile_c);

	r = 0;

	if (x1 < x2) r += 8;
	if (y1 < y2) r += 1;
	if (x1 > x2) r += 2;
	if (y1 > y2) r += 4;

	if (x2 < x3) r += 2;
	if (y2 < y3) r += 4;
	if (x2 > x3) r += 8;
	if (y2 > y3) r += 1;

	return r;
}

// Get's the direction between 2 tiles seen from tile_a
DiagDirection AiNew_GetDirection(TileIndex tile_a, TileIndex tile_b)
{
	if (TileY(tile_a) < TileY(tile_b)) return DIAGDIR_SE;
	if (TileY(tile_a) > TileY(tile_b)) return DIAGDIR_NW;
	if (TileX(tile_a) < TileX(tile_b)) return DIAGDIR_SW;
	return DIAGDIR_NE;
}


// This functions looks up if this vehicle is special for this AI
//  and returns his flag
uint AiNew_GetSpecialVehicleFlag(Player* p, Vehicle* v)
{
	uint i;

	for (i = 0; i < AI_MAX_SPECIAL_VEHICLES; i++) {
		if (_players_ainew[p->index].special_vehicles[i].veh_id == v->index) {
			return _players_ainew[p->index].special_vehicles[i].flag;
		}
	}

	// Not found :(
	return 0;
}


bool AiNew_SetSpecialVehicleFlag(Player* p, Vehicle* v, uint flag)
{
	int new_id = -1;
	uint i;

	for (i = 0; i < AI_MAX_SPECIAL_VEHICLES; i++) {
		if (_players_ainew[p->index].special_vehicles[i].veh_id == v->index) {
			_players_ainew[p->index].special_vehicles[i].flag |= flag;
			return true;
		}
		if (new_id == -1 &&
				_players_ainew[p->index].special_vehicles[i].veh_id == 0 &&
				_players_ainew[p->index].special_vehicles[i].flag == 0) {
			new_id = i;
		}
	}

	// Out of special_vehicle spots :s
	if (new_id == -1) {
		DEBUG(ai, 1, "special_vehicles list is too small");
		return false;
	}
	_players_ainew[p->index].special_vehicles[new_id].veh_id = v->index;
	_players_ainew[p->index].special_vehicles[new_id].flag = flag;
	return true;
}
