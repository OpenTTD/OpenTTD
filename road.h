/* $Id$ */

#ifndef ROAD_H
#define ROAD_H

typedef enum RoadBits {
	ROAD_NW  = 1,
	ROAD_SW  = 2,
	ROAD_SE  = 4,
	ROAD_NE  = 8,
	ROAD_X   = ROAD_SW | ROAD_NE,
	ROAD_Y   = ROAD_NW | ROAD_SE,
	ROAD_ALL = ROAD_X  | ROAD_Y
} RoadBits;

static inline RoadBits ComplementRoadBits(RoadBits r)
{
	return ROAD_ALL ^ r;
}

static inline RoadBits DiagDirToRoadBits(DiagDirection d)
{
	return 1 << (3 ^ d);
}

#endif
