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
 * @return The position of the first bit set
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

/**
 * Search the last set bit in a 64 bit variable.
 *
 * This algorithm is a static implementation of a log
 * conguence search algorithm. It checks the second half
 * if there is a bit set search there further. And this
 * way further. If no bit is set return 0.
 *
 * @param x The value to search
 * @return The position of the last bit set
 */
uint8 FindLastBit(uint64 x)
{
	if (x == 0) return 0;

	uint8 pos = 0;

	if ((x & 0xffffffff00000000ULL) != 0) { x >>= 32; pos += 32; }
	if ((x & 0x00000000ffff0000ULL) != 0) { x >>= 16; pos += 16; }
	if ((x & 0x000000000000ff00ULL) != 0) { x >>= 8;  pos += 8;  }
	if ((x & 0x00000000000000f0ULL) != 0) { x >>= 4;  pos += 4;  }
	if ((x & 0x000000000000000cULL) != 0) { x >>= 2;  pos += 2;  }
	if ((x & 0x0000000000000002ULL) != 0) { pos += 1; }

	return pos;
}
