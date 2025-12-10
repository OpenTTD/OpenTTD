/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tilearea.cpp Test functionality from tilearea_type. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../tilearea_type.h"
#include "../map_func.h"

#include "../safeguards.h"

struct TileCoord {
	uint x, y;
};

static void TestSpiralTileSequence(TileCoord center, uint diameter, std::span<TileCoord> expected)
{
	auto tile = TileXY(center.x, center.y);

	std::vector<TileIndex> result;
	for (auto ti : SpiralTileSequence(tile, diameter)) {
		result.push_back(ti);
	}
	REQUIRE(result.size() == expected.size());
	for (size_t i = 0; i < result.size(); ++i) {
		CHECK(TileX(result[i]) == expected[i].x);
		CHECK(TileY(result[i]) == expected[i].y);
	}
}

static void TestSpiralTileSequence(TileCoord start_north, uint radius, uint w, uint h, std::span<TileCoord> expected)
{
	auto tile = TileXY(start_north.x, start_north.y);

	std::vector<TileIndex> result;
	for (auto ti : SpiralTileSequence(tile, radius, w, h)) {
		result.push_back(ti);
	}
	REQUIRE(result.size() == expected.size());
	for (size_t i = 0; i < result.size(); ++i) {
		CHECK(TileX(result[i]) == expected[i].x);
		CHECK(TileY(result[i]) == expected[i].y);
	}
}

TEST_CASE("SpiralTileSequence - minimum")
{
	Map::Allocate(64, 64);

	TileCoord expected[] = {{63, 63}};
	TestSpiralTileSequence({63, 63}, 1, expected);
	TestSpiralTileSequence({63, 63}, 2, expected);
	TestSpiralTileSequence({63, 63}, 1, 0, 0, expected);
	TestSpiralTileSequence({63, 63}, 1, 2, 2, expected);
}

TEST_CASE("SpiralTileSequence - odd")
{
	Map::Allocate(64, 64);

	TileCoord expected[] = {
		{1, 1},
		{2, 0}, {1, 0}, {0, 0}, {0, 1}, {0, 2}, {1, 2}, {2, 2}, {2, 1},
		{0, 3}, {1, 3}, {2, 3}, {3, 3}, {3, 2}, {3, 1}, {3, 0},
	};
	TestSpiralTileSequence({1, 1}, 5, expected);
}

TEST_CASE("SpiralTileSequence - even")
{
	Map::Allocate(64, 64);

	TileCoord expected[] = {
		{2, 1}, {1, 1}, {1, 2}, {2, 2},
		{3, 0}, {2, 0}, {1, 0}, {0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 3}, {2, 3}, {3, 3}, {3, 2}, {3, 1},
		{0, 4}, {1, 4}, {2, 4}, {3, 4}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0},
	};
	TestSpiralTileSequence({1, 1}, 6, expected);
	TestSpiralTileSequence({1, 1}, 3, 0, 0, expected);
}

TEST_CASE("SpiralTileSequence - zero hole")
{
	Map::Allocate(64, 64);

	TileCoord expected[] = {
		{5, 2}, {4, 2}, {3, 2}, {2, 2}, {2, 3}, {3, 3}, {4, 3}, {5, 3},
		{6, 1}, {5, 1}, {4, 1}, {3, 1}, {2, 1}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4}, {6, 4}, {6, 3}, {6, 2},
	};
	TestSpiralTileSequence({2, 2}, 2, 2, 0, expected);
}

TEST_CASE("SpiralTileSequence - normal hole")
{
	Map::Allocate(64, 64);

	TileCoord expected[] = {
		{4, 2}, {3, 2}, {2, 2}, {2, 3}, {2, 4}, {2, 5}, {3, 5}, {4, 5}, {4, 4}, {4, 3},
	};
	TestSpiralTileSequence({2, 2}, 1, 1, 2, expected);
}
