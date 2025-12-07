/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file string_inplace.cpp Test functionality from core/string_inplace. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../core/string_inplace.hpp"
#include "../safeguards.h"

TEST_CASE("InPlaceReplacement")
{
	std::array<char, 4> buffer{1, 2, 3, 4};
	InPlaceReplacement inplace(buffer);

	CHECK(!inplace.builder.AnyBytesWritten());
	CHECK(inplace.builder.GetBytesWritten() == 0);
	CHECK(inplace.builder.GetWrittenData() == ""sv);
	CHECK(!inplace.builder.AnyBytesUnused());
	CHECK(inplace.builder.GetBytesUnused() == 0);
	CHECK(!inplace.consumer.AnyBytesRead());
	CHECK(inplace.consumer.GetBytesRead() == 0);
	CHECK(inplace.consumer.AnyBytesLeft());
	CHECK(inplace.consumer.GetBytesLeft() == 4);

	CHECK(inplace.consumer.ReadUint16LE() == 0x201);

	CHECK(inplace.builder.GetBytesWritten() == 0);
	CHECK(inplace.builder.GetBytesUnused() == 2);
	CHECK(inplace.consumer.GetBytesRead() == 2);
	CHECK(inplace.consumer.GetBytesLeft() == 2);

	inplace.builder.PutUint8(11);

	CHECK(inplace.builder.GetBytesWritten() == 1);
	CHECK(inplace.builder.GetBytesUnused() == 1);
	CHECK(inplace.consumer.GetBytesRead() == 2);
	CHECK(inplace.consumer.GetBytesLeft() == 2);

	inplace.builder.PutUint8(12);

	CHECK(inplace.builder.GetBytesWritten() == 2);
	CHECK(inplace.builder.GetBytesUnused() == 0);
	CHECK(inplace.consumer.GetBytesRead() == 2);
	CHECK(inplace.consumer.GetBytesLeft() == 2);

	CHECK(buffer[0] == 11);
	CHECK(buffer[1] == 12);
	CHECK(buffer[2] == 3);
	CHECK(buffer[3] == 4);
}
