/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file pathfinder_type.h General types related to pathfinders. */

#ifndef PATHFINDER_TYPE_H
#define PATHFINDER_TYPE_H

/**
 * Helper container to find a depot
 */
struct FindDepotData {
	TileIndex tile;   ///< The tile of the depot
	uint best_length; ///< The distance towards the depot, or UINT_MAX if not found
	bool reverse;     ///< True if reversing is necessary for the train to get to this depot

	/**
	 * Create an instance of this structure.
	 * @param tile        the tile of the depot
	 * @param best_length the distance towards the depot, or UINT_MAX if not found
	 * @param reverse     whether we need to reverse first.
	 */
	FindDepotData(TileIndex tile = INVALID_TILE, uint best_length = UINT_MAX, bool reverse = false) :
		tile(tile), best_length(best_length), reverse(reverse)
	{
	}
};

#endif /* PATHFINDER_TYPE_H */
