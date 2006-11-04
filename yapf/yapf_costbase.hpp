/* $Id$ */

#ifndef  YAPF_COSTBASE_HPP
#define  YAPF_COSTBASE_HPP

struct CYapfCostBase {
	static const TrackdirBits   c_upwards_slopes[16];

	FORCEINLINE static bool stSlopeCost(TileIndex tile, Trackdir td)
	{
		if (IsDiagonalTrackdir(td) && !IsTunnelTile(tile)) {
			uint tile_slope = GetTileSlope(tile, NULL) & 0x0F;
			if ((c_upwards_slopes[tile_slope] & TrackdirToTrackdirBits(td)) != 0) {
				return true;
			}
		}
		return false;
	}
};

struct CostRailSettings {
	// look-ahead signal penalty
};


#endif /* YAPF_COSTBASE_HPP */
