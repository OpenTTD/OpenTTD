/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file geometry_func.h Geometry functions. */

#ifndef GEOMETRY_FUNC_HPP
#define GEOMETRY_FUNC_HPP

#include "geometry_type.h"

Dimension maxdim(const Dimension &d1, const Dimension &d2);

/**
 * Check if a rectangle is empty.
 * @param r Rectangle to check.
 * @return True if and only if the rectangle doesn't define space.
 */
inline bool IsEmptyRect(const Rect &r)
{
	return (r.left | r.top | r.right | r.bottom) == 0;
}

Rect BoundingRect(const Rect &r1, const Rect &r2);

#endif /* GEOMETRY_FUNC_HPP */
