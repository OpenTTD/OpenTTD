/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file math_func.cpp Math functions. */

#include "../stdafx.h"
#include "math_func.hpp"

#include "../safeguards.h"

/**
 * Deterministic approximate division.
 * Cancels out division errors stemming from the integer nature of the division over multiple runs.
 * @param a Dividend.
 * @param b Divisor.
 * @return a/b or (a/b)+1.
 */
int DivideApprox(int a, int b)
{
	int random_like = ((a + b) * (a - b)) % b;

	int remainder = a % b;

	int ret = a / b;
	if (abs(random_like) < abs(remainder)) {
		ret += ((a < 0) ^ (b < 0)) ? -1 : 1;
	}

	return ret;
}

/**
 * Compute the integer square root.
 * @param num Radicand.
 * @return Rounded integer square root.
 */
uint32_t IntSqrt(uint32_t num)
{
	return (uint32_t)round(sqrt(num));
}
