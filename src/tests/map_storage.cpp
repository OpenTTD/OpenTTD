/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_storage.cpp Consistency tests for members of map_func.h. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"

#include "../safeguards.h"

/** Unique data that will allow to distinguish parts of map storage from each other. */
static constexpr uint64_t BASE_VALUE = 0b0000000011111110000000111111000000111110000011110000111000110010; // Needs to be 64 bits.

/** Function that performs unit test of internall low level storage. */
void Tile::RunUnitTest()
{
	TileBase &base = base_tiles[this->tile.base()];
	TileExtended &extended = extended_tiles[this->tile.base()];

	base.base = uint32_t(BASE_VALUE);
	extended.base = BASE_VALUE;

	/* Test common base. */
	CHECK(base.common.tropic_zone == GB(BASE_VALUE, 0, 2));
	CHECK(base.common.bridge_above == GB(BASE_VALUE, 2, 2));
	CHECK(base.common.type == GB(BASE_VALUE, 4, 4));
	CHECK(base.common.height == GB(BASE_VALUE, 8, 8));

	/* Test common extended. */
	CHECK(extended.common.owner == GB(BASE_VALUE, 0, 5));
	CHECK(extended.common.water_class == GB(BASE_VALUE, 5, 2));
	CHECK(extended.common.ship_docking == GB(BASE_VALUE, 7, 1));
	CHECK(extended.common.bit_offset_1 == GB(BASE_VALUE, 8, 8));
	CHECK(extended.common.bit_offset_2 == GB(BASE_VALUE, 16, 16));
	CHECK(extended.common.animation_state == GB(BASE_VALUE, 32, 2));

	/* Test old base. */
	CHECK(this->type() == GB(BASE_VALUE, 0, 8));
	CHECK(this->height() == GB(BASE_VALUE, 8, 8));
	CHECK(this->m3() == GB(BASE_VALUE, 16, 8));
	CHECK(this->m4() == GB(BASE_VALUE, 24, 8));

	/* Test old extended. */
	CHECK(this->m1() == GB(BASE_VALUE, 0, 8));
	CHECK(this->m5() == GB(BASE_VALUE, 8, 8));
	CHECK(this->m2() == GB(BASE_VALUE, 16, 16));
	CHECK(this->m6() == GB(BASE_VALUE, 32, 8));
	CHECK(this->m7() == GB(BASE_VALUE, 40, 8));
	CHECK(this->m8() == GB(BASE_VALUE, 48, 16));
}

/** Tests if internall structs and unions of Tile class are consistent. */
TEST_CASE("Map storage test.")
{
	Map::Allocate(MIN_MAP_SIZE, MIN_MAP_SIZE);
	Tile t(TileIndex{});
	t.RunUnitTest();
}
