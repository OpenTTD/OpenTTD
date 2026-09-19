/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file endian_buffer.cpp Test functionality from misc/endian_buffer. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../misc/endian_buffer.hpp"
#include "../strings_func.h"
#include "../table/strings.h"
#include "../safeguards.h"

TEST_CASE("EndianBufferWriter tests")
{
	auto test_case = [&](auto input, std::vector<uint8_t> expected) {
		auto iter = expected.begin();
		for (auto byte : EndianBufferWriter<>::FromValue(input)) {
			if (iter == expected.end()) return false;
			if (byte != *iter) return false;
			++iter;
		}
		return iter == expected.end();
	};
	CHECK(test_case(true, {0x01}));
	std::string_view test_string1 = "test1";
	CHECK(test_case(test_string1, {0x74, 0x65, 0x73, 0x74, 0x31, 0x00}));
	std::string test_string2 = "test2";
	CHECK(test_case(test_string2, {0x74, 0x65, 0x73, 0x74, 0x32, 0x00}));
	EncodedString test_string3 = GetEncodedString(STR_NULL);
	CHECK(test_case(test_string3, {0xee, 0x80, 0x81, 0x30, 0x00}));
}
