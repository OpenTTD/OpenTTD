/* $Id$ */

/** @file group_type.h Types of a group. */

#ifndef GROUP_TYPE_H
#define GROUP_TYPE_H

typedef uint16 GroupID;

enum {
	ALL_GROUP     = 0xFFFD,
	DEFAULT_GROUP = 0xFFFE, ///< ungrouped vehicles are in this group.
	INVALID_GROUP = 0xFFFF,
};

struct Group;

#endif /* GROUP_TYPE_H */
