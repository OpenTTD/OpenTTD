/* $Id$ */

#include "../stdafx.h"

#include "yapf.hpp"
#include "follow_track.hpp"
#include "yapf_node_rail.hpp"
#include "yapf_costbase.hpp"
#include "yapf_costcache.hpp"

/** translate tileh to the bitset of up-hill trackdirs */
const TrackdirBits CYapfCostBase::c_upwards_slopes[] = {
	TRACKDIR_BIT_NONE                    , // no tileh
	TRACKDIR_BIT_X_SW | TRACKDIR_BIT_Y_NW, // 1
	TRACKDIR_BIT_X_SW | TRACKDIR_BIT_Y_SE, // 2
	TRACKDIR_BIT_X_SW                    , // 3
	TRACKDIR_BIT_X_NE | TRACKDIR_BIT_Y_SE, // 4
	TRACKDIR_BIT_NONE                    , // 5
	TRACKDIR_BIT_Y_SE                    , // 6
	TRACKDIR_BIT_NONE                    , // 7
	TRACKDIR_BIT_X_NE | TRACKDIR_BIT_Y_NW, // 8,
	TRACKDIR_BIT_Y_NW                    , // 9
	TRACKDIR_BIT_NONE                    , //10
	TRACKDIR_BIT_NONE                    , //11,
	TRACKDIR_BIT_X_NE                    , //12
	TRACKDIR_BIT_NONE                    , //13
	TRACKDIR_BIT_NONE                    , //14
	TRACKDIR_BIT_NONE                    , //15
};
