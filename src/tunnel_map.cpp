/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tunnel_map.cpp Map accessors for tunnels. */

#include "stdafx.h"
#include "tunnelbridge_map.h"

#include "safeguards.h"


/**
 * Gets the other end of the tunnel. Where a vehicle would reappear when it
 * enters at the given tile.
 * @param tile the tile to search from.
 * @return the tile of the other end of the tunnel.
 */
TileIndex GetOtherTunnelEnd(TileIndex tile)
{
	DiagDirection dir = GetTunnelBridgeDirection(tile);
	TileIndexDiff delta = TileOffsByDiagDir(dir);
	int z = GetTileZ(tile);

	dir = ReverseDiagDir(dir);
	do {
		tile += delta;
	} while (
		!IsTunnelTile(tile) ||
		GetTunnelBridgeDirection(tile) != dir ||
		GetTileZ(tile) != z
	);

	return tile;
}


/**
 * Is there a tunnel in the way in the given direction?
 * @param tile the tile to search from.
 * @param z    the 'z' to search on.
 * @param dir  the direction to start searching to.
 * @return true if and only if there is a tunnel.
 */
bool IsTunnelInWayDir(TileIndex tile, int z, DiagDirection dir)
{
	if (!HasTunnelFlag(tile)) return false;

	TileIndexDiff delta = TileOffsByDiagDir(dir);
	int height;

	do {
		tile -= delta;
		if (!IsValidTile(tile)) return false;
		height = GetTileZ(tile);
	} while (z < height);

	return z == height && IsTunnelTile(tile) && GetTunnelBridgeDirection(tile) == dir;
}

/**
 * Is there a tunnel in the way in any direction?
 * @param tile the tile to search from.
 * @param z the 'z' to search on.
 * @return true if and only if there is a tunnel.
 */
bool IsTunnelInWay(TileIndex tile, int z)
{
	if (!HasTunnelFlag(tile)) return false;

	return IsTunnelInWayDir(tile, z, (TileX(tile) > (Map::MaxX() / 2)) ? DIAGDIR_NE : DIAGDIR_SW) ||
			IsTunnelInWayDir(tile, z, (TileY(tile) > (Map::MaxY() / 2)) ? DIAGDIR_NW : DIAGDIR_SE);
}

/**
 * Is there a tunnel beneath the given tile along a specified axis?
 *
 * Searches diagonally from the given tile along the specified axis
 * toward the nearest map edge, looking for a tunnel that crosses beneath the tile.
 * If such a tunnel is found, the 'start' tile may be updated to point to one of the
 * tunnel's endpoints, depending on the search direction.
 *
 * @param[in, out] start The tile to search from. If a tunnel is found and the optional dir parameter
 *  is provided, this will be updated to the tunnel's entry or exit tile, depending on dir.
 * @param axis           The axis along which to search for tunnels.
 * @param dir            The diagonal direction used to traverse the axis. (optional)
 *  If provided, 'start' will be updated to the tunnel endpoint on the side of the search direction.
 * @return true if and only if a tunnel exists beneath the starting tile along the given axis.
 */
static bool FindTunnelCrossingBelow(TileIndex &start, Axis axis, DiagDirection dir = INVALID_DIAGDIR)
{
	/* Determine the search direction along the axis which is closest to the map edge. */
	DiagDirection search_dir;
	if (axis == AXIS_X) {
		search_dir = TileX(start) < (Map::MaxX() / 2) ? DIAGDIR_NE : DIAGDIR_SW;
	} else {
		search_dir = TileY(start) < (Map::MaxY() / 2) ? DIAGDIR_NW : DIAGDIR_SE;
	}

	/* The direction a tunnel has to match must be against the search direction. */
	DiagDirection tunnel_match_dir = ReverseDiagDir(search_dir);

	TileIndexDiff delta = TileOffsByDiagDir(search_dir);
	int last_height = MAX_TILE_HEIGHT;

	/* Start the search on a diagonal segment of tiles towards the map edge.
	 * The starting point of the search is skipped. */
	for (TileIndex t = start + delta; IsValidTile(t); t += delta) {
		if (last_height == 0) return false; // No further search required as it's impossible to have tunnels below height 0
		last_height = GetTileZ(t);

		if (!IsTunnelTile(t)) continue;
		if (GetTunnelBridgeDirection(t) != tunnel_match_dir) continue;

		TileIndex end = GetOtherTunnelEnd(t);
		if (end == start) continue; // Skip this tunnel. It really must go past the start point to be considered in the way

		/* Determine whether the tunnel end is past the start point. */
		uint dist_tunnel = DistanceManhattan(t, end);
		uint dist_start = DistanceManhattan(t, start);
		if (dist_tunnel <= dist_start) continue;

		/* Tunnel crosses the starting point. */
		if (dir != INVALID_DIAGDIR) start = dir == search_dir ? t : end;
		return true;
	}

	return false;
}

/**
 * Scans a diagonal tile segment to update the tunnel presence flags after a tunnel is removed.
 *
 * The 'start' and 'end' tiles define a diagonal segment that was previously marked as having a
 * tunnel below. Since the tunnel is now gone, check each tile in that segment to determine whether
 * any other tunnels still exist below. If there's no more tunnels, the tile marker is cleared.
 *
 * @param start The first tile of the diagonal segment previously marked for tunnel presence.
 * @param end   The last tile of the diagonal segment previously marked for tunnel presence.
 * @param dir   The diagonal direction used to traverse the segment and determine axis alignment.
 */
void UpdateTunnelPresenceFlags(TileIndex start, TileIndex end, DiagDirection dir)
{
	TileIndexDiff delta = TileOffsByDiagDir(dir);
	Axis axis = DiagDirToAxis(dir);
	Axis other_axis = OtherAxis(axis);

	auto [min, max] = std::minmax(start, end);
	for (TileIndex t = start + delta; t > min && t < max; t += delta) {
		assert(HasTunnelFlag(t));

		TileIndex skip_to = t;
		if (FindTunnelCrossingBelow(skip_to, axis, dir)) {
			t = skip_to - delta;
			continue;
		}
		if (FindTunnelCrossingBelow(t, other_axis)) continue;

		ClearTunnelFlag(t);
	}
}
