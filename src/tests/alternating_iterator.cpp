/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file bitmath_func.cpp Test functionality from core/bitmath_func. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../misc/alternating_iterator.hpp"

#include "../safeguards.h"

TEST_CASE("AlternatingIterator tests")
{
	auto test_case = [&](auto input, std::initializer_list<int> expected) {
		return std::ranges::equal(input, expected);
	};

	/* Sequence includes sentinel markers to detect out-of-bounds reads without relying on UB. */
	std::initializer_list<const int> raw_sequence_even = {INT_MAX, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, INT_MAX};
	const std::span<const int> sequence_even = std::span{raw_sequence_even.begin() + 1, raw_sequence_even.end() - 1};

	CHECK(test_case(AlternatingView(sequence_even, sequence_even.begin() + 0), { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }));
	CHECK(test_case(AlternatingView(sequence_even, sequence_even.begin() + 1), { 1, 0, 2, 3, 4, 5, 6, 7, 8, 9 }));
	CHECK(test_case(AlternatingView(sequence_even, sequence_even.begin() + 2), { 2, 1, 3, 0, 4, 5, 6, 7, 8, 9 }));
	CHECK(test_case(AlternatingView(sequence_even, sequence_even.begin() + 3), { 3, 2, 4, 1, 5, 0, 6, 7, 8, 9 }));
	CHECK(test_case(AlternatingView(sequence_even, sequence_even.begin() + 4), { 4, 3, 5, 2, 6, 1, 7, 0, 8, 9 }));
	CHECK(test_case(AlternatingView(sequence_even, sequence_even.begin() + 5), { 5, 4, 6, 3, 7, 2, 8, 1, 9, 0 }));
	CHECK(test_case(AlternatingView(sequence_even, sequence_even.begin() + 6), { 6, 5, 7, 4, 8, 3, 9, 2, 1, 0 }));
	CHECK(test_case(AlternatingView(sequence_even, sequence_even.begin() + 7), { 7, 6, 8, 5, 9, 4, 3, 2, 1, 0 }));
	CHECK(test_case(AlternatingView(sequence_even, sequence_even.begin() + 8), { 8, 7, 9, 6, 5, 4, 3, 2, 1, 0 }));
	CHECK(test_case(AlternatingView(sequence_even, sequence_even.begin() + 9), { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }));

	/* Sequence includes sentinel markers to detect out-of-bounds reads without relying on UB. */
	std::initializer_list<const int> raw_sequence_odd = {INT_MAX, 0, 1, 2, 3, 4, INT_MAX};
	const std::span<const int> sequence_odd = std::span{raw_sequence_odd.begin() + 1, raw_sequence_odd.end() - 1};

	CHECK(test_case(AlternatingView(sequence_odd, sequence_odd.begin() + 0), { 0, 1, 2, 3, 4 }));
	CHECK(test_case(AlternatingView(sequence_odd, sequence_odd.begin() + 1), { 1, 0, 2, 3, 4 }));
	CHECK(test_case(AlternatingView(sequence_odd, sequence_odd.begin() + 2), { 2, 1, 3, 0, 4 }));
	CHECK(test_case(AlternatingView(sequence_odd, sequence_odd.begin() + 3), { 3, 2, 4, 1, 0 }));
	CHECK(test_case(AlternatingView(sequence_odd, sequence_odd.begin() + 4), { 4, 3, 2, 1, 0 }));
}
