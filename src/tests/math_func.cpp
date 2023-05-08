/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file math_func.cpp Test functionality from core/math_func. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../core/math_func.hpp"

TEST_CASE("LeastCommonMultipleTest - Zero")
{
	CHECK(0 == LeastCommonMultiple(0, 0));
	CHECK(0 == LeastCommonMultiple(0, 600));
	CHECK(0 == LeastCommonMultiple(600, 0));
}

TEST_CASE("LeastCommonMultipleTest - FindLCM")
{
	CHECK(25 == LeastCommonMultiple(5, 25));
	CHECK(25 == LeastCommonMultiple(25, 5));
	CHECK(130 == LeastCommonMultiple(5, 26));
	CHECK(130 == LeastCommonMultiple(26, 5));
}

TEST_CASE("GreatestCommonDivisorTest - Negative")
{
	CHECK(4 == GreatestCommonDivisor(4, -52));
	// CHECK(3 == GreatestCommonDivisor(-27, 6)); // error - returns -3
}

TEST_CASE("GreatestCommonDivisorTest - Zero")
{
	CHECK(27 == GreatestCommonDivisor(0, 27));
	CHECK(27 == GreatestCommonDivisor(27, 0));
}

TEST_CASE("GreatestCommonDivisorTest - FindGCD")
{
	CHECK(5 == GreatestCommonDivisor(5, 25));
	CHECK(5 == GreatestCommonDivisor(25, 5));
	CHECK(1 == GreatestCommonDivisor(7, 27));
	CHECK(1 == GreatestCommonDivisor(27, 7));
}

TEST_CASE("DivideApproxTest - Negative")
{
	CHECK(-2 == DivideApprox(-5, 2));
	CHECK(2 == DivideApprox(-5, -2));
	CHECK(-1 == DivideApprox(-66, 80));
}

TEST_CASE("DivideApproxTest, Divide")
{
	CHECK(2 == DivideApprox(5, 2));
	CHECK(3 == DivideApprox(80, 30));
	CHECK(3 == DivideApprox(8, 3));
	CHECK(0 == DivideApprox(3, 8));
}

TEST_CASE("IntSqrtTest - Zero")
{
	CHECK(0 == IntSqrt(0));
}

TEST_CASE("IntSqrtTest - FindSqRt")
{
	CHECK(5 == IntSqrt(25));
	CHECK(10 == IntSqrt(100));
	CHECK(9 == IntSqrt(88));
	CHECK(1696 == IntSqrt(2876278));
}


TEST_CASE("ClampTo")
{
	CHECK(0 == ClampTo<uint8_t>(std::numeric_limits<int64_t>::lowest()));
	CHECK(0 == ClampTo<uint8_t>(-1));
	CHECK(0 == ClampTo<uint8_t>(0));
	CHECK(1 == ClampTo<uint8_t>(1));

	CHECK(255 == ClampTo<uint8_t>(std::numeric_limits<uint64_t>::max()));
	CHECK(255 == ClampTo<uint8_t>(256));
	CHECK(255 == ClampTo<uint8_t>(255));
	CHECK(254 == ClampTo<uint8_t>(254));

	CHECK(-128 == ClampTo<int8_t>(std::numeric_limits<int64_t>::lowest()));
	CHECK(-128 == ClampTo<int8_t>(-129));
	CHECK(-128 == ClampTo<int8_t>(-128));
	CHECK(-127 == ClampTo<int8_t>(-127));

	CHECK(127 == ClampTo<int8_t>(std::numeric_limits<uint64_t>::max()));
	CHECK(127 == ClampTo<int8_t>(128));
	CHECK(127 == ClampTo<int8_t>(127));
	CHECK(126 == ClampTo<int8_t>(126));

	CHECK(126 == ClampTo<int64_t>(static_cast<uint8_t>(126)));
	CHECK(126 == ClampTo<uint64_t>(static_cast<int8_t>(126)));
	CHECK(0 == ClampTo<uint64_t>(static_cast<int8_t>(-126)));
	CHECK(0 == ClampTo<uint8_t>(static_cast<int8_t>(-126)));

	/* The realm around 64 bits types is tricky as there is not one type/method that works for all. */

	/* lowest/max uint64_t does not get clamped when clamping to uint64_t. */
	CHECK(std::numeric_limits<uint64_t>::lowest() == ClampTo<uint64_t>(std::numeric_limits<uint64_t>::lowest()));
	CHECK(std::numeric_limits<uint64_t>::max() == ClampTo<uint64_t>(std::numeric_limits<uint64_t>::max()));

	/* negative int64_t get clamped to 0. */
	CHECK(0 == ClampTo<uint64_t>(std::numeric_limits<int64_t>::lowest()));
	CHECK(0 == ClampTo<uint64_t>(int64_t(-1)));
	/* positive int64_t remain the same. */
	CHECK(1 == ClampTo<uint64_t>(int64_t(1)));
	CHECK(static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) == ClampTo<uint64_t>(std::numeric_limits<int64_t>::max()));

	/* max uint64_t gets clamped to max int64_t. */
	CHECK(std::numeric_limits<int64_t>::max() == ClampTo<int64_t>(std::numeric_limits<uint64_t>::max()));
}


TEST_CASE("SoftClamp")
{
	/* Special behaviour of soft clamp returning the average of min/max when min is higher than max. */
	CHECK(1250 == SoftClamp(0, 1500, 1000));
	int million = 1000 * 1000;
	CHECK(1250 * million == SoftClamp(0, 1500 * million, 1000 * million));
	CHECK(0 == SoftClamp(0, 1500 * million, -1500 * million));
}
