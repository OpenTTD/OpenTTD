/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "rail.h"
#include "station_map.h"
#include "tunnel_map.h"

/* XXX: Below 3 tables store duplicate data. Maybe remove some? */
/* Maps a trackdir to the bit that stores its status in the map arrays, in the
 * direction along with the trackdir */
extern const byte _signal_along_trackdir[] = {
	0x80, 0x80, 0x80, 0x20, 0x40, 0x10, 0, 0,
	0x40, 0x40, 0x40, 0x10, 0x80, 0x20
};

/* Maps a trackdir to the bit that stores its status in the map arrays, in the
 * direction against the trackdir */
extern const byte _signal_against_trackdir[] = {
	0x40, 0x40, 0x40, 0x10, 0x80, 0x20, 0, 0,
	0x80, 0x80, 0x80, 0x20, 0x40, 0x10
};

/* Maps a Track to the bits that store the status of the two signals that can
 * be present on the given track */
extern const byte _signal_on_track[] = {
	0xC0, 0xC0, 0xC0, 0x30, 0xC0, 0x30
};

/* Maps a diagonal direction to the all trackdirs that are connected to any
 * track entering in this direction (including those making 90 degree turns)
 */
extern const TrackdirBits _exitdir_reaches_trackdirs[] = {
	TRACKDIR_BIT_X_NE | TRACKDIR_BIT_LOWER_E | TRACKDIR_BIT_LEFT_N,  /* DIAGDIR_NE */
	TRACKDIR_BIT_Y_SE | TRACKDIR_BIT_LEFT_S  | TRACKDIR_BIT_UPPER_E, /* DIAGDIR_SE */
	TRACKDIR_BIT_X_SW | TRACKDIR_BIT_UPPER_W | TRACKDIR_BIT_RIGHT_S, /* DIAGDIR_SW */
	TRACKDIR_BIT_Y_NW | TRACKDIR_BIT_RIGHT_N | TRACKDIR_BIT_LOWER_W  /* DIAGDIR_NW */
};

extern const Trackdir _next_trackdir[] = {
	TRACKDIR_X_NE,  TRACKDIR_Y_SE,  TRACKDIR_LOWER_E, TRACKDIR_UPPER_E, TRACKDIR_RIGHT_S, TRACKDIR_LEFT_S, INVALID_TRACKDIR, INVALID_TRACKDIR,
	TRACKDIR_X_SW,  TRACKDIR_Y_NW,  TRACKDIR_LOWER_W, TRACKDIR_UPPER_W, TRACKDIR_RIGHT_N, TRACKDIR_LEFT_N
};

/* Maps a trackdir to all trackdirs that make 90 deg turns with it. */
extern const TrackdirBits _track_crosses_trackdirs[] = {
	TRACKDIR_BIT_Y_SE     | TRACKDIR_BIT_Y_NW,                                                   /* TRACK_X     */
	TRACKDIR_BIT_X_NE     | TRACKDIR_BIT_X_SW,                                                   /* TRACK_Y     */
	TRACKDIR_BIT_RIGHT_N  | TRACKDIR_BIT_RIGHT_S  | TRACKDIR_BIT_LEFT_N  | TRACKDIR_BIT_LEFT_S,  /* TRACK_UPPER */
	TRACKDIR_BIT_RIGHT_N  | TRACKDIR_BIT_RIGHT_S  | TRACKDIR_BIT_LEFT_N  | TRACKDIR_BIT_LEFT_S,  /* TRACK_LOWER */
	TRACKDIR_BIT_UPPER_W  | TRACKDIR_BIT_UPPER_E  | TRACKDIR_BIT_LOWER_W | TRACKDIR_BIT_LOWER_E, /* TRACK_LEFT  */
	TRACKDIR_BIT_UPPER_W  | TRACKDIR_BIT_UPPER_E  | TRACKDIR_BIT_LOWER_W | TRACKDIR_BIT_LOWER_E  /* TRACK_RIGHT */
};

/* Maps a track to all tracks that make 90 deg turns with it. */
extern const TrackBits _track_crosses_tracks[] = {
	TRACK_BIT_Y,    /* TRACK_X     */
	TRACK_BIT_X,    /* TRACK_Y     */
	TRACK_BIT_VERT, /* TRACK_UPPER */
	TRACK_BIT_VERT, /* TRACK_LOWER */
	TRACK_BIT_HORZ, /* TRACK_LEFT  */
	TRACK_BIT_HORZ  /* TRACK_RIGHT */
};

/* Maps a trackdir to the (4-way) direction the tile is exited when following
 * that trackdir */
extern const DiagDirection _trackdir_to_exitdir[] = {
	DIAGDIR_NE,DIAGDIR_SE,DIAGDIR_NE,DIAGDIR_SE,DIAGDIR_SW,DIAGDIR_SE, DIAGDIR_NE,DIAGDIR_NE,
	DIAGDIR_SW,DIAGDIR_NW,DIAGDIR_NW,DIAGDIR_SW,DIAGDIR_NW,DIAGDIR_NE,
};

extern const Trackdir _track_exitdir_to_trackdir[][DIAGDIR_END] = {
	{TRACKDIR_X_NE,     INVALID_TRACKDIR,  TRACKDIR_X_SW,     INVALID_TRACKDIR},
	{INVALID_TRACKDIR,  TRACKDIR_Y_SE,     INVALID_TRACKDIR,  TRACKDIR_Y_NW},
	{TRACKDIR_UPPER_E,  INVALID_TRACKDIR,  INVALID_TRACKDIR,  TRACKDIR_UPPER_W},
	{INVALID_TRACKDIR,  TRACKDIR_LOWER_E,  TRACKDIR_LOWER_W,  INVALID_TRACKDIR},
	{INVALID_TRACKDIR,  INVALID_TRACKDIR,  TRACKDIR_LEFT_S,   TRACKDIR_LEFT_N},
	{TRACKDIR_RIGHT_N,  TRACKDIR_RIGHT_S,  INVALID_TRACKDIR,  INVALID_TRACKDIR}
};

extern const Trackdir _track_enterdir_to_trackdir[][DIAGDIR_END] = { // TODO: replace magic with enums
	{TRACKDIR_X_NE,     INVALID_TRACKDIR,  TRACKDIR_X_SW,     INVALID_TRACKDIR},
	{INVALID_TRACKDIR,  TRACKDIR_Y_SE,     INVALID_TRACKDIR,  TRACKDIR_Y_NW},
	{INVALID_TRACKDIR,  TRACKDIR_UPPER_E,  TRACKDIR_UPPER_W,  INVALID_TRACKDIR},
	{TRACKDIR_LOWER_E,  INVALID_TRACKDIR,  INVALID_TRACKDIR,  TRACKDIR_LOWER_W},
	{TRACKDIR_LEFT_N,   TRACKDIR_LEFT_S,   INVALID_TRACKDIR,  INVALID_TRACKDIR},
	{INVALID_TRACKDIR,  INVALID_TRACKDIR,  TRACKDIR_RIGHT_S,  TRACKDIR_RIGHT_N}
};

extern const Trackdir _track_direction_to_trackdir[][DIR_END] = {
	{INVALID_TRACKDIR, TRACKDIR_X_NE,     INVALID_TRACKDIR, INVALID_TRACKDIR,  INVALID_TRACKDIR, TRACKDIR_X_SW,     INVALID_TRACKDIR, INVALID_TRACKDIR},
	{INVALID_TRACKDIR, INVALID_TRACKDIR,  INVALID_TRACKDIR, TRACKDIR_Y_SE,     INVALID_TRACKDIR, INVALID_TRACKDIR,  INVALID_TRACKDIR, TRACKDIR_Y_NW},
	{INVALID_TRACKDIR, INVALID_TRACKDIR,  TRACKDIR_UPPER_E, INVALID_TRACKDIR,  INVALID_TRACKDIR, INVALID_TRACKDIR,  TRACKDIR_UPPER_W, INVALID_TRACKDIR},
	{INVALID_TRACKDIR, INVALID_TRACKDIR,  TRACKDIR_LOWER_E, INVALID_TRACKDIR,  INVALID_TRACKDIR, INVALID_TRACKDIR,  TRACKDIR_LOWER_W, INVALID_TRACKDIR},
	{TRACKDIR_LEFT_N,  INVALID_TRACKDIR,  INVALID_TRACKDIR, INVALID_TRACKDIR,  TRACKDIR_LEFT_S,  INVALID_TRACKDIR,  INVALID_TRACKDIR, INVALID_TRACKDIR},
	{TRACKDIR_RIGHT_N, INVALID_TRACKDIR,  INVALID_TRACKDIR, INVALID_TRACKDIR,  TRACKDIR_RIGHT_S, INVALID_TRACKDIR,  INVALID_TRACKDIR, INVALID_TRACKDIR}
};

extern const Trackdir _dir_to_diag_trackdir[] = {
	TRACKDIR_X_NE, TRACKDIR_Y_SE, TRACKDIR_X_SW, TRACKDIR_Y_NW,
};


RailType GetTileRailType(TileIndex tile, Track track)
{
	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			return GetRailType(tile);

		case MP_STREET:
			/* rail/road crossing */
			if (IsLevelCrossing(tile)) return GetRailTypeCrossing(tile);
			break;

		case MP_STATION:
			if (IsRailwayStationTile(tile)) return GetRailType(tile);
			break;

		case MP_TUNNELBRIDGE:
			if (IsTunnel(tile)) {
				if (GetTunnelTransportType(tile) == TRANSPORT_RAIL) return GetRailType(tile);
			} else {
				if (GetBridgeTransportType(tile) == TRANSPORT_RAIL) return GetRailType(tile);
			}
			break;

		default:
			break;
	}
	return INVALID_RAILTYPE;
}
