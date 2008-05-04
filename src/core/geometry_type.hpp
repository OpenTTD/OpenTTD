/* $Id$ */

/** @file geometry_type.hpp All geometry types in OpenTTD. */

#ifndef GEOMETRY_TYPE_HPP
#define GEOMETRY_TYPE_HPP

#if defined(__AMIGA__)
	/* AmigaOS already has a Point declared */
	#define Point OTTD_Point
#endif /* __AMIGA__ */

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
	int width;
	int height;
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
