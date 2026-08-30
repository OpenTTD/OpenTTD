/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tilearea_spiral.cpp Handling of spiral tile sequences. */

#include "stdafx.h"

#include "tilearea_spiral.h"

#include "map_func.h"

#include "safeguards.h"

/** Create the iterator. @copydoc SpiralTileSequence::SpiralTileSequence(TileIndex, uint) */
SpiralTileIterator::SpiralTileIterator(TileIndex center, uint diameter) : max_radius(diameter / 2), cur_radius(0), dir(DiagDirection::Begin)
{
	assert(diameter > 0);

	if (diameter % 2 == 1) {
		this->extent.fill(1);
		this->dir = DiagDirection::Invalid; // special case for odd diameters, see Increment()
		this->position = 0;

		this->x = TileX(center);
		this->y = TileY(center);
	} else {
		this->extent.fill(0);
		this->dir = DiagDirection::Begin;
		this->InitPosition();

		/* Start with the west corner of the center 2x2 rect */
		this->x = TileX(center) + 1;
		this->y = TileY(center);
	}
	this->SkipOutsideMap();
}

/** Create the iterator. @copydoc SpiralTileSequence::SpiralTileSequence(TileIndex, uint, uint, uint) */
SpiralTileIterator::SpiralTileIterator(TileIndex start_north, uint radius, uint w, uint h) :
	max_radius(radius), extent{w, h, w, h}, cur_radius(0), dir(DiagDirection::Begin),
	/* first tile is the west corner */
	x(TileX(start_north) + w + 1), y(TileY(start_north))
{
	assert(max_radius > 0);
	this->InitPosition();
	this->SkipOutsideMap();
}

/**
 * Advance the internal state until it reaches a valid tile or the end.
 */
void SpiralTileIterator::SkipOutsideMap()
{
	while (!this->IsEnd() && (this->x >= Map::SizeX() || this->y >= Map::SizeY())) this->Increment();
}

/**
 * Initialise "position" after "dir" was changed.
 */
void SpiralTileIterator::InitPosition()
{
	this->position = this->extent[this->dir] + this->cur_radius * 2 + 1;
}

/**
 * Advance the internal state to the next potential tile.
 * The tile may be outside the map though.
 */
void SpiralTileIterator::Increment()
{
	assert(!this->IsEnd());

	/* Special value for first tile in areas with odd diameter */
	if (this->dir == DiagDirection::Invalid) {
		const auto west = TileIndexDiffCByDir(Direction::W);
		this->x += west.x;
		this->y += west.y;
		this->dir = DiagDirection::Begin;
		this->InitPosition();
		return;
	}

	/* Step to the next 'neighbour' in the circular line */
	const auto diff = TileIndexDiffCByDiagDir(this->dir);
	this->x += diff.x;
	this->y += diff.y;
	--this->position;
	if (this->position > 0) return;

	/* Corner reached, switch direction */
	++this->dir;

	if (this->dir == DiagDirection::End) {
		/* Jump to next circle */
		const auto west = TileIndexDiffCByDir(Direction::W);
		this->x += west.x;
		this->y += west.y;
		++this->cur_radius;
		this->dir = DiagDirection::Begin;
	}

	this->InitPosition();
}
