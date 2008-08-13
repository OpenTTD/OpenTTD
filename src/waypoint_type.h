/* $Id$ */

/** @file waypoint_type.h Types related to waypoints. */

#ifndef WAYPOINT_TYPE_H
#define WAYPOINT_TYPE_H

typedef uint16 WaypointID;
struct Waypoint;

enum {
	MAX_LENGTH_WAYPOINT_NAME_BYTES  =  31, ///< The maximum length of a waypoint name in bytes including '\0'
	MAX_LENGTH_WAYPOINT_NAME_PIXELS = 180, ///< The maximum length of a waypoint name in pixels
};

#endif /* WAYPOINT_TYPE_H */
