/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_costbase.hpp Handling of cost determination. */

#ifndef YAPF_COSTBASE_HPP
#define YAPF_COSTBASE_HPP

/** Base implementation for cost accounting. */
struct CYapfCostBase {
	/**
	 * Does the given track direction on the given tile yield an uphill penalty?
	 * @param tile The tile to check.
	 * @param td   The track direction to check.
	 * @return True if there's a slope, otherwise false.
	 */
	inline static bool stSlopeCost(TileIndex tile, Trackdir td)
	{
		if (IsDiagonalTrackdir(td)) {
			if (IsBridgeTile(tile)) {
				/* it is bridge ramp, check if we are entering the bridge */
				if (GetTunnelBridgeDirection(tile) != TrackdirToExitdir(td)) return false; // no, we are leaving it, no penalty
				/* we are entering the bridge */
				Slope tile_slope = GetTileSlope(tile);
				Axis axis = DiagDirToAxis(GetTunnelBridgeDirection(tile));
				return !HasBridgeFlatRamp(tile_slope, axis);
			} else {
				/* not bridge ramp */
				if (IsTunnelTile(tile)) return false; // tunnel entry/exit doesn't slope
				Slope tile_slope = GetTileSlope(tile);
				return IsUphillTrackdir(tile_slope, td); // slopes uphill => apply penalty
			}
		}
		return false;
	}
};

#endif /* YAPF_COSTBASE_HPP */
