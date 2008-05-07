/* $Id$ */

/** @file landscape_type.h Types related to the landscape. */

#ifndef LANDSCAPE_TYPE_H
#define LANDSCAPE_TYPE_H

typedef byte LandscapeID;

/* Landscape types */
enum {
	LT_TEMPERATE  = 0,
	LT_ARCTIC     = 1,
	LT_TROPIC     = 2,
	LT_TOYLAND    = 3,

	NUM_LANDSCAPE = 4,
};

#endif /* LANDSCAPE_TYPE_H */
