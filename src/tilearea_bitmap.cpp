/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tilearea_bitmap.cpp Handling of bitmap tile areas. */

#include "stdafx.h"

#include "tilearea_bitmap.h"

#include "safeguards.h"

/**
 * Construct this BitmapTileArea from an OrthogonalTileArea.
 * @param ta The orthogonal tile area.
 */
BitmapTileArea::BitmapTileArea(const OrthogonalTileArea &ta) : ta(ta)
{
	this->data.resize(static_cast<size_t>(this->ta.w) * this->ta.h);
}

/**
 * Test if this tile area is empty.
 * @return \c true iff the tile area is empty.
 */
bool BitmapTileArea::IsEmpty() const
{
	return this->ta.IsEmpty() || std::ranges::none_of(this->data, [](bool b) {
		return b;
	});
}

/**
 * Initialize the BitmapTileArea with the specified OrthogonalTileArea.
 * @param ta The tile area.
 */
void BitmapTileArea::Initialize(const OrthogonalTileArea &ta)
{
	this->ta.tile = ta.tile;
	this->ta.w = ta.w;
	this->ta.h = ta.h;

	this->data.clear();
	this->data.resize(static_cast<size_t>(this->ta.w) * this->ta.h);
}
