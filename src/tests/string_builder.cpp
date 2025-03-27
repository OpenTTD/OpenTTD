/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file string_builder.cpp Test functionality from core/string_builder. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../core/string_builder.hpp"
#include "../safeguards.h"

using namespace std::literals;

TEST_CASE("StringBuilder - basic")
{
	std::string buffer;
	StringBuilder builder(buffer);

	CHECK(!builder.AnyBytesWritten());
	CHECK(builder.GetBytesWritten() == 0);
	CHECK(builder.GetWrittenData() == ""sv);

	builder.Put("ab");
	builder += "cdef";

	CHECK(builder.AnyBytesWritten());
	CHECK(builder.GetBytesWritten() == 6);
	CHECK(builder.GetWrittenData() == "abcdef"sv);

	CHECK(buffer == "abcdef"sv);
}

TEST_CASE("StringBuilder - binary")
{
	std::string buffer;
	StringBuilder builder(buffer);

	builder.PutUint8(1);
	builder.PutSint8(-1);
	builder.PutUint16LE(0x201);
	builder.PutSint16LE(-0x201);
	builder.PutUint32LE(0x30201);
	builder.PutSint32LE(-0x30201);
	builder.PutUint64LE(0x7060504030201);
	builder.PutSint64LE(-0x7060504030201);

	CHECK(buffer == "\x01\xFF\x01\x02\xFF\xFD\x01\x02\x03\x00\xFF\xFD\xFC\xFF\x01\x02\x03\04\x05\x06\x07\x00\xFF\xFD\xFC\xFB\xFA\xF9\xF8\xFF"sv);
}

TEST_CASE("StringBuilder - text")
{
	std::string buffer;
	StringBuilder builder(buffer);

	builder.PutChar('a');
	builder.PutUtf8(0x1234);
	builder.PutChar(' ');
	builder.PutIntegerBase<uint32_t>(1234, 10);
	builder.PutChar(' ');
	builder.PutIntegerBase<uint32_t>(0x7FFF, 16);
	builder.PutChar(' ');
	builder.PutIntegerBase<int32_t>(-1234, 10);
	builder.PutChar(' ');
	builder.PutIntegerBase<int32_t>(-0x7FFF, 16);
	builder.PutChar(' ');
	builder.PutIntegerBase<uint64_t>(1'234'567'890'123, 10);
	builder.PutChar(' ');
	builder.PutIntegerBase<uint64_t>(0x1234567890, 16);
	builder.PutChar(' ');
	builder.PutIntegerBase<int64_t>(-1'234'567'890'123, 10);
	builder.PutChar(' ');
	builder.PutIntegerBase<int64_t>(-0x1234567890, 16);

	CHECK(buffer == "a\u1234 1234 7fff -1234 -7fff 1234567890123 1234567890 -1234567890123 -1234567890"sv);
}
