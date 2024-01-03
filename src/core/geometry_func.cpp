/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file geometry_func.cpp Geometry functions. */

#include "../stdafx.h"
#include "geometry_func.hpp"
#include "math_func.hpp"

#include "../safeguards.h"

/**
 * Compute bounding box of both dimensions.
 * @param d1 First dimension.
 * @param d2 Second dimension.
 * @return The bounding box of both dimensions, the smallest dimension that surrounds both arguments.
 */
Dimension maxdim(const Dimension &d1, const Dimension &d2)
{
	Dimension d;
	d.width  = std::max(d1.width,  d2.width);
	d.height = std::max(d1.height, d2.height);
	return d;
}

/**
 * Compute the bounding rectangle around two rectangles.
 * @param r1 First rectangle.
 * @param r2 Second rectangle.
 * @return The bounding rectangle, the smallest rectangle that contains both arguments.
 */
Rect BoundingRect(const Rect &r1, const Rect &r2)
{
	/* If either the first or the second is empty, return the other. */
	if (IsEmptyRect(r1)) return r2;
	if (IsEmptyRect(r2)) return r1;

	Rect r;

	r.top    = std::min(r1.top,    r2.top);
	r.bottom = std::max(r1.bottom, r2.bottom);
	r.left   = std::min(r1.left,   r2.left);
	r.right  = std::max(r1.right,  r2.right);

	return r;
}
