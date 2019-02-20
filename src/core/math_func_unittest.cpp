/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file math_func_unittest.cpp Math functions unit tests. */

#include "../stdafx.h"
#include "math_func.hpp"

#include <gtest/gtest.h>

TEST(LeastCommonMultipleTest, Zero) {
	EXPECT_EQ(0, LeastCommonMultiple(0, 0));
	EXPECT_EQ(0, LeastCommonMultiple(0, 600));
	EXPECT_EQ(0, LeastCommonMultiple(600, 0));
}

TEST(LeastCommonMultipleTest, FindLCM) {
	EXPECT_EQ(25, LeastCommonMultiple(5, 25));
	EXPECT_EQ(25, LeastCommonMultiple(25, 5));
	EXPECT_EQ(130, LeastCommonMultiple(5, 26));
	EXPECT_EQ(130, LeastCommonMultiple(26, 5));
}

TEST(GreatestCommonDivisorTest, Negative) {
	EXPECT_EQ(4, GreatestCommonDivisor(4, -52));
	EXPECT_EQ(3, GreatestCommonDivisor(-27, 6));
}

TEST(GreatestCommonDivisorTest, Zero) {
	EXPECT_EQ(27, GreatestCommonDivisor(0, 27));
	EXPECT_EQ(27, GreatestCommonDivisor(27, 0));
}

TEST(GreatestCommonDivisorTest, FindGCD) {
	EXPECT_EQ(5, GreatestCommonDivisor(5, 25));
	EXPECT_EQ(5, GreatestCommonDivisor(25, 5));
	EXPECT_EQ(1, GreatestCommonDivisor(7, 27));
	EXPECT_EQ(1, GreatestCommonDivisor(27, 7));
}

TEST(DivideApproxTest, Negative) {
	EXPECT_EQ(-2, DivideApprox(-5, 2));
	EXPECT_EQ(2, DivideApprox(-5, -2));
	EXPECT_EQ(-1, DivideApprox(-66, 80));
}

TEST(DivideApproxTest, Divide) {
	EXPECT_EQ(2, DivideApprox(5, 2));
	EXPECT_EQ(3, DivideApprox(80, 30));
	EXPECT_EQ(3, DivideApprox(8, 3));
	EXPECT_EQ(0, DivideApprox(3, 8));
}

TEST(IntSqrtTest, Zero) {
	EXPECT_EQ(0, IntSqrt(0));
}

TEST(IntSqrtTest, FindSqRt) {
	EXPECT_EQ(5, IntSqrt(25));
	EXPECT_EQ(10, IntSqrt(100));
	EXPECT_EQ(9, IntSqrt(88));
	EXPECT_EQ(1696, IntSqrt(2876278));
}
