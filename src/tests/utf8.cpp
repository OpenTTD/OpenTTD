/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file utf8.cpp Test functionality from core/utf8. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../core/utf8.hpp"

#include "../safeguards.h"

using namespace std::literals;

TEST_CASE("Utf8View - empty")
{
	Utf8View view;
	auto begin = view.begin();
	auto end = view.end();
	CHECK(begin == end);
	CHECK(begin.GetByteOffset() == 0);
}

TEST_CASE("Utf8View - invalid")
{
	Utf8View view("\u1234\x80\x80""a\xFF\x80\x80\x80\x80\x80""b\xF0");
	auto begin = view.begin();
	auto end = view.end();
	CHECK(begin < end);
	auto it = begin;
	CHECK(it == begin);
	CHECK(it.GetByteOffset() == 0);
	CHECK(*it == 0x1234);
	++it;
	CHECK(begin < it);
	CHECK(it < end);
	CHECK(it.GetByteOffset() == 5);
	CHECK(*it == 'a');
	++it;
	CHECK(begin < it);
	CHECK(it < end);
	CHECK(it.GetByteOffset() == 6);
	CHECK(*it == '?');
	++it;
	CHECK(begin < it);
	CHECK(it < end);
	CHECK(it.GetByteOffset() == 12);
	CHECK(*it == 'b');
	++it;
	CHECK(begin < it);
	CHECK(it < end);
	CHECK(it.GetByteOffset() == 13);
	CHECK(*it == '?');
	++it;
	CHECK(it.GetByteOffset() == 14);
	CHECK(begin < it);
	CHECK(it == end);
	--it;
	CHECK(begin < it);
	CHECK(it < end);
	CHECK(it.GetByteOffset() == 13);
	CHECK(*it == '?');
	--it;
	CHECK(begin < it);
	CHECK(it < end);
	CHECK(it.GetByteOffset() == 12);
	CHECK(*it == 'b');
	--it;
	CHECK(begin < it);
	CHECK(it < end);
	CHECK(it.GetByteOffset() == 6);
	CHECK(*it == '?');
	--it;
	CHECK(begin < it);
	CHECK(it < end);
	CHECK(it.GetByteOffset() == 5);
	CHECK(*it == 'a');
	--it;
	CHECK(it == begin);
	CHECK(it.GetByteOffset() == 0);
	CHECK(*it == 0x1234);
}

TEST_CASE("Utf8View - iterate")
{
	Utf8View view("\u1234a\0b\U00012345"sv);
	auto begin = view.begin();
	auto end = view.end();
	CHECK(begin < end);
	auto it = begin;
	CHECK(it == begin);
	CHECK(it.GetByteOffset() == 0);
	CHECK(std::distance(begin, it) == 0);
	CHECK(std::distance(it, end) == 5);
	CHECK(*it == 0x1234);
	CHECK(it == view.GetIterAtByte(0));
	CHECK(it == view.GetIterAtByte(1));
	CHECK(it == view.GetIterAtByte(2));
	++it;
	CHECK(begin < it);
	CHECK(it < end);
	CHECK(it.GetByteOffset() == 3);
	CHECK(std::distance(begin, it) == 1);
	CHECK(std::distance(it, end) == 4);
	CHECK(*it == 'a');
	CHECK(it == view.GetIterAtByte(3));
	++it;
	CHECK(it.GetByteOffset() == 4);
	CHECK(std::distance(begin, it) == 2);
	CHECK(std::distance(it, end) == 3);
	CHECK(*it == 0);
	CHECK(it == view.GetIterAtByte(4));
	++it;
	CHECK(it.GetByteOffset() == 5);
	CHECK(std::distance(begin, it) == 3);
	CHECK(std::distance(it, end) == 2);
	CHECK(*it == 'b');
	CHECK(it == view.GetIterAtByte(5));
	++it;
	CHECK(begin < it);
	CHECK(it < end);
	CHECK(it.GetByteOffset() == 6);
	CHECK(std::distance(begin, it) == 4);
	CHECK(std::distance(it, end) == 1);
	CHECK(*it == 0x00012345);
	CHECK(it == view.GetIterAtByte(6));
	CHECK(it == view.GetIterAtByte(7));
	CHECK(it == view.GetIterAtByte(8));
	CHECK(it == view.GetIterAtByte(9));
	++it;
	CHECK(begin < it);
	CHECK(it.GetByteOffset() == 10);
	CHECK(std::distance(begin, it) == 5);
	CHECK(std::distance(it, end) == 0);
	CHECK(it == end);
	CHECK(it == view.GetIterAtByte(10));
	--it;
	CHECK(begin < it);
	CHECK(it < end);
	CHECK(it.GetByteOffset() == 6);
	CHECK(*it == 0x00012345);
	--it;
	CHECK(begin < it);
	CHECK(it < end);
	CHECK(it.GetByteOffset() == 5);
	CHECK(*it == 'b');
}
