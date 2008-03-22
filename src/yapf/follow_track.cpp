/* $Id$ */

#include "../stdafx.h"
#include "yapf.hpp"
#include "follow_track.hpp"

void FollowTrackInit(FollowTrack_t *This, const Vehicle* v)
{
	CFollowTrackWater& F = *(CFollowTrackWater*) This;
	F.Init(v, NULL);
}

bool FollowTrackWater(FollowTrack_t *This, TileIndex old_tile, Trackdir old_td)
{
	CFollowTrackWater& F = *(CFollowTrackWater*) This;
	return F.Follow(old_tile, old_td);
}

bool FollowTrackRoad(FollowTrack_t *This, TileIndex old_tile, Trackdir old_td)
{
	CFollowTrackRoad& F = *(CFollowTrackRoad*) This;
	return F.Follow(old_tile, old_td);
}

bool FollowTrackRail(FollowTrack_t *This, TileIndex old_tile, Trackdir old_td)
{
	CFollowTrackRail& F = *(CFollowTrackRail*) This;
	return F.Follow(old_tile, old_td);
}

bool FollowTrackWaterNo90(FollowTrack_t *This, TileIndex old_tile, Trackdir old_td)
{
	CFollowTrackWaterNo90& F = *(CFollowTrackWaterNo90*) This;
	return F.Follow(old_tile, old_td);
}

bool FollowTrackRoadNo90(FollowTrack_t *This, TileIndex old_tile, Trackdir old_td)
{
	CFollowTrackRoadNo90& F = *(CFollowTrackRoadNo90*) This;
	return F.Follow(old_tile, old_td);
}

bool FollowTrackRailNo90(FollowTrack_t *This, TileIndex old_tile, Trackdir old_td)
{
	CFollowTrackRailNo90& F = *(CFollowTrackRailNo90*) This;
	return F.Follow(old_tile, old_td);
}
