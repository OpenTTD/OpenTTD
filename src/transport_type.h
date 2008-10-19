/* $Id$ */

/** @file transport_type.h Base types related to transport. */

#ifndef TRANSPORT_TYPE_H
#define TRANSPORT_TYPE_H

typedef uint16 UnitID;

/** Available types of transport */
enum TransportType {
	/* These constants are for now linked to the representation of bridges
	 * and tunnels, so they can be used by GetTileTrackStatus_TunnelBridge.
	 * In an ideal world, these constants would be used everywhere when
	 * accessing tunnels and bridges. For now, you should just not change
	 * the values for road and rail.
	 */
	TRANSPORT_BEGIN = 0,
	TRANSPORT_RAIL = TRANSPORT_BEGIN, ///< Transport by train
	TRANSPORT_ROAD,  ///< Transport by road vehicle
	TRANSPORT_WATER, ///< Transport over water
	TRANSPORT_AIR,   ///< Transport through air
	TRANSPORT_END,
	INVALID_TRANSPORT = 0xff,
};

#endif /* TRANSPORT_TYPE_H */
