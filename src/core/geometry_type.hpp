/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file geometry_type.hpp All geometry types in OpenTTD. */

#ifndef GEOMETRY_TYPE_HPP
#define GEOMETRY_TYPE_HPP

#if defined(__APPLE__)
	/* Mac OS X already has both Rect and Point declared */
	#define Rect OTTD_Rect
	#define Point OTTD_Point
#endif /* __APPLE__ */


/** Coordinates of a point in 2D */
struct Point {
	int x;
	int y;
};

/** Dimensions (a width and height) of a rectangle in 2D */
struct Dimension {
	uint width;
	uint height;

	Dimension(uint w = 0, uint h = 0) : width(w), height(h) {};

	bool operator< (const Dimension &other) const
	{
		int x = (*this).width - other.width;
		if (x != 0) return x < 0;
		return (*this).height < other.height;
	}

	bool operator== (const Dimension &other) const
	{
		return (*this).width == other.width && (*this).height == other.height;
	}
};

/** Specification of a rectangle with absolute coordinates of all edges */
struct Rect {
	int left;
	int top;
	int right;
	int bottom;
};

/**
 * Specification of a rectangle with an absolute top-left coordinate and a
 * (relative) width/height
 */
struct PointDimension {
	int x;
	int y;
	int width;
	int height;
};

/** A pair of two integers */
struct Pair {
	int a;
	int b;
};

#endif /* GEOMETRY_TYPE_HPP */
