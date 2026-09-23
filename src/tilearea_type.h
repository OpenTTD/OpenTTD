/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tilearea_type.h Type for storing the 'area' of something uses on the map. */

#ifndef TILEAREA_TYPE_H
#define TILEAREA_TYPE_H

#include "tile_type.h"
#include "tilearea_orthogonal.h"
#include "tilearea_diagonal.h"
#include "tilearea_wrapper.h"

/** Wrapper for orthogonal or diagonal tile iterators. */
using OrthoDiagonalTileIterator = TileIteratorWrapper<OrthogonalTileArea::iterator, DiagonalTileArea::iterator>;

/** Wrapper for orthogonal or diagonal tile areas. */
using OrthoDiagonalTileArea = TileAreaWrapper<OrthoDiagonalTileIterator, OrthogonalTileArea, DiagonalTileArea>;

OrthoDiagonalTileArea CreateOrthoDiagonalArea(TileIndex corner1, TileIndex corner2, bool diagonal);

#endif /* TILEAREA_TYPE_H */
