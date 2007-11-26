/* $Id$ */

/** @file bitmath_func.cpp */

#include "../stdafx.h"
#include "bitmath_func.hpp"

const uint8 _ffb_64[64] = {
 0,  0,  1,  0,  2,  0,  1,  0,
 3,  0,  1,  0,  2,  0,  1,  0,
 4,  0,  1,  0,  2,  0,  1,  0,
 3,  0,  1,  0,  2,  0,  1,  0,
 5,  0,  1,  0,  2,  0,  1,  0,
 3,  0,  1,  0,  2,  0,  1,  0,
 4,  0,  1,  0,  2,  0,  1,  0,
 3,  0,  1,  0,  2,  0,  1,  0,
};

/**
 * Search the first set bit in a 32 bit variable.
 *
 * This algorithm is a static implementation of a log
 * conguence search algorithm. It checks the first half
 * if there is a bit set search there further. And this
 * way further. If no bit is set return 0.
 *
 * @param x The value to search
 * @param The position of the first bit set
 */
uint8 FindFirstBit(uint32 x)
{
	if (x == 0) return 0;
	/* The macro FIND_FIRST_BIT is better to use when your x is
	  not more than 128. */

	uint8 pos = 0;

	if ((x & 0x0000ffff) == 0) { x >>= 16; pos += 16; }
	if ((x & 0x000000ff) == 0) { x >>= 8;  pos += 8;  }
	if ((x & 0x0000000f) == 0) { x >>= 4;  pos += 4;  }
	if ((x & 0x00000003) == 0) { x >>= 2;  pos += 2;  }
	if ((x & 0x00000001) == 0) { pos += 1; }

	return pos;
}
