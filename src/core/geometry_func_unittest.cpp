/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file geometry_func_unittest.cpp Geometry functions unit tests. */

#include "../stdafx.h"
#include "geometry_func.hpp"
#include "math_func.hpp"

#include <gtest/gtest.h>

TEST(MaxdimTest, Zero) {
	Dimension d1 = {0, 0};
	Dimension d2 = {120, 100};

	EXPECT_EQ(120, maxdim(d1, d2).width);
	EXPECT_EQ(100, maxdim(d1, d2).height);
}

TEST(MaxdimTest, Overlap) {
	Dimension d1 = {50, 250};
	Dimension d2 = {350, 50};

	EXPECT_EQ(350, maxdim(d1, d2).width);
	EXPECT_EQ(250, maxdim(d1, d2).height);
}
