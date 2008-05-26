/* $Id$ */

/** @file sortlist_type.h Base types for having sorted lists in GUIs. */

#ifndef SORTLIST_TYPE_H
#define SORTLIST_TYPE_H

#include "misc/smallvec.h"

enum SortListFlags {
	VL_NONE    = 0,      ///< no sort
	VL_DESC    = 1 << 0, ///< sort descending or ascending
	VL_RESORT  = 1 << 1, ///< instruct the code to resort the list in the next loop
	VL_REBUILD = 1 << 2, ///< create sort-listing to use for qsort and friends
	VL_END     = 1 << 3,
};
DECLARE_ENUM_AS_BIT_SET(SortListFlags);

struct Listing {
	bool order;    ///< Ascending/descending
	byte criteria; ///< Sorting criteria
};

template <typename T>
struct GUIList : public SmallVector<T, 32> {
	SortListFlags flags; ///< used to control sorting/resorting/etc.
	uint16 resort_timer; ///< resort list after a given amount of ticks if set
	byte sort_type;      ///< what criteria to sort on
};

#endif /* SORTLIST_TYPE_H */
