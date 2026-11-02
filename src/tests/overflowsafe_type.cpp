/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file overflowsafe_type.cpp Test functionality from core/overflowsafe_type. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../core/overflowsafe_type.hpp"

#include "../safeguards.h"

TEST_CASE("OverflowSafeInt tests")
{
	/* Various initialisers. */
	CHECK( OverflowSafeInt32{INT8_MIN} == INT8_MIN );
	CHECK( OverflowSafeInt32{INT8_MAX} == INT8_MAX );
	CHECK( OverflowSafeInt32{UINT8_MAX} == UINT8_MAX );
	CHECK( OverflowSafeInt32{INT16_MIN} == INT16_MIN );
	CHECK( OverflowSafeInt32{INT16_MAX} == INT16_MAX );
	CHECK( OverflowSafeInt32{UINT16_MAX} == UINT16_MAX );
	CHECK( OverflowSafeInt32{INT32_MIN} == INT32_MIN );
	CHECK( OverflowSafeInt32{INT32_MAX} == INT32_MAX );

	/*
	 * These initialisers should not be possible:
	 *
	 * CHECK( OverflowSafeInt32{UINT32_MAX} == INT32_MAX );
	 * CHECK( OverflowSafeInt32{INT64_MIN} == INT32_MIN );
	 * CHECK( OverflowSafeInt32{INT64_MAX} == INT32_MAX );
	 * CHECK( OverflowSafeInt32{UINT64_MAX} == INT32_MAX );
	 */

	/* Positive multiplication. */
	CHECK( OverflowSafeInt32{1} * INT16_MAX == INT16_MAX );
	CHECK( OverflowSafeInt32{1} * UINT16_MAX == UINT16_MAX );
	CHECK( OverflowSafeInt32{1} * INT32_MAX == INT32_MAX );
	CHECK( OverflowSafeInt32{2} * INT32_MAX == INT32_MAX );

	/* Negative multiplication. */
	CHECK( OverflowSafeInt32{-1} * INT16_MAX == -INT16_MAX );
	CHECK( OverflowSafeInt32{-1} * UINT16_MAX == -UINT16_MAX );
	CHECK( OverflowSafeInt32{-1} * INT32_MAX == -INT32_MAX );
	CHECK( OverflowSafeInt32{-2} * INT32_MAX == INT32_MIN );

	/* Assignment. */
	OverflowSafeInt32 a{0};
	a = INT32_MAX;
	CHECK( a == INT32_MAX );

	/*
	 * These assignments should not be possible:
	 *
	 * a = UINT32_MAX;
	 * CHECK( a == INT32_MAX );
	 * a = INT64_MAX;
	 * CHECK( a == INT32_MAX );
	 * a = UINT64_MAX;
	 * CHECK( a == INT32_MAX );
	 */
}
