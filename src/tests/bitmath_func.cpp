/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bitmath_func.cpp Test functionality from core/bitmath_func. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../core/bitmath_func.hpp"

TEST_CASE("SetBitIterator tests")
{
	auto test_case = [&](auto input, std::initializer_list<uint> expected) {
		auto iter = expected.begin();
		for (auto bit : SetBitIterator(input)) {
			if (iter == expected.end()) return false;
			if (bit != *iter) return false;
			++iter;
		}
		return iter == expected.end();
	};
	CHECK(test_case(0, {}));
	CHECK(test_case(1, { 0 }));
	CHECK(test_case(42, { 1, 3, 5 }));
	CHECK(test_case(0x8080FFFFU, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 23, 31 }));
	CHECK(test_case(INT32_MIN, { 31 }));
	CHECK(test_case(INT64_MIN, { 63 }));
}
