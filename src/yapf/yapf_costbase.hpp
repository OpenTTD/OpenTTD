/* $Id$ */

/** @file yapf_costbase.hpp Handling of cost determination. */

#ifndef  YAPF_COSTBASE_HPP
#define  YAPF_COSTBASE_HPP

struct CYapfCostBase {
	FORCEINLINE static bool stSlopeCost(TileIndex tile, Trackdir td)
	{
		if (IsDiagonalTrackdir(td)) {
			if (IsBridgeTile(tile)) {
				// it is bridge ramp, check if we are entering the bridge
				if (GetTunnelBridgeDirection(tile) != TrackdirToExitdir(td)) return false; // no, we are living it, no penalty
				// we are entering the bridge
				Slope tile_slope = GetTileSlope(tile, NULL);
				Axis axis = DiagDirToAxis(GetTunnelBridgeDirection(tile));
				return !HasBridgeFlatRamp(tile_slope, axis);
			} else {
				// not bridge ramp
				if (IsTunnelTile(tile)) return false; // tunnel entry/exit doesn't slope
				Slope tile_slope = GetTileSlope(tile, NULL);
				return IsUphillTrackdir(tile_slope, td); // slopes uphill => apply penalty
			}
		}
		return false;
	}
};

struct CostRailSettings {
	// look-ahead signal penalty
};


#endif /* YAPF_COSTBASE_HPP */
