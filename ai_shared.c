#include "stdafx.h"
#include "ttd.h"
#include "map.h"
#include "ai.h"
#include "vehicle.h"

int AiNew_GetRailDirection(uint tile_a, uint tile_b, uint tile_c) {
	// 0 = vert
	// 1 = horz
	// 2 = dig up-left
	// 3 = dig down-right
	// 4 = dig down-left
	// 5 = dig up-right

	int x1, x2, x3;
	int y1, y2, y3;

	x1 = TileX(tile_a);
	x2 = TileX(tile_b);
	x3 = TileX(tile_c);

	y1 = TileY(tile_a);
	y2 = TileY(tile_b);
	y3 = TileY(tile_c);

	if (y1 == y2 && y2 == y3) return 0;
	if (x1 == x2 && x2 == x3) return 1;
	if (y2 > y1) {
		if (x2 > x3) return 2;
		else return 4;
	}
	if (x2 > x1) {
		if (y2 > y3) return 2;
		else return 5;
	}
	if (y1 > y2) {
		if (x2 > x3) return 5;
		else return 3;
	}
	if (x1 > x2) {
		if (y2 > y3) return 4;
		else return 3;
	}

	return 0;
}

int AiNew_GetRoadDirection(uint tile_a, uint tile_b, uint tile_c) {
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
int AiNew_GetDirection(uint tile_a, uint tile_b) {
	if (TileY(tile_a) < TileY(tile_b)) return 1;
	if (TileY(tile_a) > TileY(tile_b)) return 3;
	if (TileX(tile_a) < TileX(tile_b)) return 2;
	return 0;
}

// This functions looks up if this vehicle is special for this AI
//  and returns his flag
uint AiNew_GetSpecialVehicleFlag(Player *p, Vehicle *v) {
	int i;
	for (i=0;i<AI_MAX_SPECIAL_VEHICLES;i++) {
		if (p->ainew.special_vehicles[i].veh_id == v->index) {
			return p->ainew.special_vehicles[i].flag;
		}
	}

	// Not found :(
	return 0;
}

bool AiNew_SetSpecialVehicleFlag(Player *p, Vehicle *v, uint flag) {
	int i, new_id = -1;
	for (i=0;i<AI_MAX_SPECIAL_VEHICLES;i++) {
		if (p->ainew.special_vehicles[i].veh_id == v->index) {
			p->ainew.special_vehicles[i].flag |= flag;
			return true;
		}
		if (new_id == -1 && p->ainew.special_vehicles[i].veh_id == 0 &&
			p->ainew.special_vehicles[i].flag == 0)
			new_id = i;
	}

	// Out of special_vehicle spots :s
	if (new_id == -1) {
		DEBUG(ai, 1)("special_vehicles list is too small :(");
		return false;
	}
	p->ainew.special_vehicles[new_id].veh_id = v->index;
	p->ainew.special_vehicles[new_id].flag = flag;
	return true;
}
