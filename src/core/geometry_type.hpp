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


struct Point {
	int x;
	int y;
};

struct Dimension {
	int width;
	int height;
};

struct Rect {
	int left;
	int top;
	int right;
	int bottom;
};

struct PointDimension {
	int x;
	int y;
	int width;
	int height;
};

struct Pair {
	int a;
	int b;
};

#endif /* GEOMETRY_TYPE_HPP */
