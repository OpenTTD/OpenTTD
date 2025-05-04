/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file string_consumer.cpp Test functionality from core/string_consumer. */

#include "../stdafx.h"

#include <ranges>

#include "../3rdparty/catch2/catch.hpp"

#include "../core/flatset_type.hpp"

#include "../safeguards.h"

TEST_CASE("FlatSet - basic")
{
	/* Sorted array of expected values. */
	const auto values = std::to_array<uint8_t>({5, 10, 15, 20, 25});

	FlatSet<uint8_t> set;

	/* Set should be empty. */
	CHECK(set.empty());

	/* Insert in a random order,. */
	set.insert(values[1]);
	set.insert(values[2]);
	set.insert(values[4]);
	set.insert(values[3]);
	set.insert(values[0]);
	CHECK(set.size() == 5);
	CHECK(set.contains(values[0]));
	CHECK(set.contains(values[1]));
	CHECK(set.contains(values[2]));
	CHECK(set.contains(values[3]));
	CHECK(set.contains(values[4]));
	CHECK(std::ranges::equal(set, values));

	/* Test inserting an existing value does not affect order. */
	set.insert(values[1]);
	CHECK(set.size() == 5);
	CHECK(set.contains(values[0]));
	CHECK(set.contains(values[1]));
	CHECK(set.contains(values[2]));
	CHECK(set.contains(values[3]));
	CHECK(set.contains(values[4]));
	CHECK(std::ranges::equal(set, values));

	/* Insert a value multiple times. */
	set.insert(0);
	set.insert(0);
	set.insert(0);
	CHECK(set.size() == 6);
	CHECK(set.contains(0));

	/* Remove a value multiple times. */
	set.erase(0);
	set.erase(0);
	set.erase(0);
	CHECK(set.size() == 5);
	CHECK(!set.contains(0));
}
