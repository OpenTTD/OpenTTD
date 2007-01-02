/* $Id$ */

#ifndef ROAD_H
#define ROAD_H

typedef enum RoadBits {
	ROAD_NW  = 1U,
	ROAD_SW  = 2U,
	ROAD_SE  = 4U,
	ROAD_NE  = 8U,
	ROAD_X   = ROAD_SW | ROAD_NE,
	ROAD_Y   = ROAD_NW | ROAD_SE,
	ROAD_ALL = ROAD_X  | ROAD_Y
} RoadBits;

static inline RoadBits ComplementRoadBits(RoadBits r)
{
	return (RoadBits)(ROAD_ALL ^ r);
}

static inline RoadBits DiagDirToRoadBits(DiagDirection d)
{
	return (RoadBits)(1U << (3 ^ d));
}

#endif /* ROAD_H */
