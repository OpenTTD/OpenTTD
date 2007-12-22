/* $Id$ */

/** @file geometry_type.hpp All geometry types in OpenTTD. */

#ifndef GEOMETRY_TYPE_HPP
#define GEOMETRY_TYPE_HPP

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
