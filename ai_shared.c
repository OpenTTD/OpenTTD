#include "stdafx.h"
#include "ttd.h"
#include "player.h"
#include "ai.h"

int AiNew_GetRailDirection(uint tile_a, uint tile_b, uint tile_c) {
	// 0 = vert
	// 1 = horz
	// 2 = dig up-left
	// 3 = dig down-right
	// 4 = dig down-left
	// 5 = dig up-right

	int x1, x2, x3;
	int y1, y2, y3;

	x1 = GET_TILE_X(tile_a);
	x2 = GET_TILE_X(tile_b);
	x3 = GET_TILE_X(tile_c);

	y1 = GET_TILE_Y(tile_a);
	y2 = GET_TILE_Y(tile_b);
	y3 = GET_TILE_Y(tile_c);

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

	x1 = GET_TILE_X(tile_a);
	x2 = GET_TILE_X(tile_b);
	x3 = GET_TILE_X(tile_c);

	y1 = GET_TILE_Y(tile_a);
	y2 = GET_TILE_Y(tile_b);
	y3 = GET_TILE_Y(tile_c);

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
	if (GET_TILE_Y(tile_a) < GET_TILE_Y(tile_b)) return 1;
	if (GET_TILE_Y(tile_a) > GET_TILE_Y(tile_b)) return 3;
	if (GET_TILE_X(tile_a) < GET_TILE_X(tile_b)) return 2;
	return 0;
}
 
