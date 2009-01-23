/* $Id$ */

/** @file landscape_type.h Types related to the landscape. */

#ifndef LANDSCAPE_TYPE_H
#define LANDSCAPE_TYPE_H

typedef byte LandscapeID; ///< Landscape type. @see LandscapeType

/** Landscape types */
enum LandscapeType {
	LT_TEMPERATE  = 0,
	LT_ARCTIC     = 1,
	LT_TROPIC     = 2,
	LT_TOYLAND    = 3,

	NUM_LANDSCAPE = 4,
};

/**
 * For storing the water borders which shall be retained.
 */
enum Borders {
	BORDER_NE = 0,
	BORDER_SE = 1,
	BORDER_SW = 2,
	BORDER_NW = 3,
	BORDERS_RANDOM = 16,
};

#endif /* LANDSCAPE_TYPE_H */
