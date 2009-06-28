/* $Id$ */

/** @file geometry_func.cpp Geometry functions. */

#include "../stdafx.h"
#include "geometry_func.hpp"
#include "math_func.hpp"

/**
 * Compute bounding box of both dimensions.
 * @param d1 First dimension.
 * @param d2 Second dimension.
 * @return The bounding box of both dimensions, the smallest dimension that surrounds both arguments.
 */
Dimension maxdim(const Dimension &d1, const Dimension &d2)
{
	Dimension d;
	d.width  = max(d1.width,  d2.width);
	d.height = max(d1.height, d2.height);
	return d;
}
