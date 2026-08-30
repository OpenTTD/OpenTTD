/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tilearea.cpp Handling of tile areas. */

#include "stdafx.h"

#include "tilearea_type.h"

#include "safeguards.h"

/**
 * Create an Orthogonal/Diagonal tile area wrapper.
 * @param corner1 Tile from where to begin iterating.
 * @param corner2 Tile where to end the iterating.
 * @param diagonal Whether to create a DiagonalTileArea or OrthogonalTileArea.
 * @return The OrthoDiagonalArea.
 */
OrthoDiagonalTileArea CreateOrthoDiagonalArea(TileIndex corner1, TileIndex corner2, bool diagonal)
{
	return diagonal
		? OrthoDiagonalTileArea{DiagonalTileArea{corner1, corner2}}
		: OrthoDiagonalTileArea{OrthogonalTileArea{corner1, corner2}};
}
