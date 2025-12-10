/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file track_data.h Data related to rail tracks. */

/* Maps a diagonal direction to the all trackdirs that are connected to any
 * track entering in this direction (including those making 90 degree turns)
 */
extern const TrackdirBits _exitdir_reaches_trackdirs[] = {
	TRACKDIR_BIT_X_NE | TRACKDIR_BIT_LOWER_E | TRACKDIR_BIT_LEFT_N,  // DIAGDIR_NE
	TRACKDIR_BIT_Y_SE | TRACKDIR_BIT_LEFT_S  | TRACKDIR_BIT_UPPER_E, // DIAGDIR_SE
	TRACKDIR_BIT_X_SW | TRACKDIR_BIT_UPPER_W | TRACKDIR_BIT_RIGHT_S, // DIAGDIR_SW
	TRACKDIR_BIT_Y_NW | TRACKDIR_BIT_RIGHT_N | TRACKDIR_BIT_LOWER_W  // DIAGDIR_NW
};

extern const Trackdir _next_trackdir[TRACKDIR_END] = {
	TRACKDIR_X_NE,  TRACKDIR_Y_SE,  TRACKDIR_LOWER_E, TRACKDIR_UPPER_E, TRACKDIR_RIGHT_S, TRACKDIR_LEFT_S, INVALID_TRACKDIR, INVALID_TRACKDIR,
	TRACKDIR_X_SW,  TRACKDIR_Y_NW,  TRACKDIR_LOWER_W, TRACKDIR_UPPER_W, TRACKDIR_RIGHT_N, TRACKDIR_LEFT_N
};

/* Maps a trackdir to all trackdirs that make 90 deg turns with it. */
extern const TrackdirBits _track_crosses_trackdirs[TRACK_END] = {
	TRACKDIR_BIT_Y_SE     | TRACKDIR_BIT_Y_NW,                                                   // TRACK_X
	TRACKDIR_BIT_X_NE     | TRACKDIR_BIT_X_SW,                                                   // TRACK_Y
	TRACKDIR_BIT_RIGHT_N  | TRACKDIR_BIT_RIGHT_S  | TRACKDIR_BIT_LEFT_N  | TRACKDIR_BIT_LEFT_S,  // TRACK_UPPER
	TRACKDIR_BIT_RIGHT_N  | TRACKDIR_BIT_RIGHT_S  | TRACKDIR_BIT_LEFT_N  | TRACKDIR_BIT_LEFT_S,  // TRACK_LOWER
	TRACKDIR_BIT_UPPER_W  | TRACKDIR_BIT_UPPER_E  | TRACKDIR_BIT_LOWER_W | TRACKDIR_BIT_LOWER_E, // TRACK_LEFT
	TRACKDIR_BIT_UPPER_W  | TRACKDIR_BIT_UPPER_E  | TRACKDIR_BIT_LOWER_W | TRACKDIR_BIT_LOWER_E  // TRACK_RIGHT
};

/* Maps a track to all tracks that make 90 deg turns with it. */
extern const TrackBits _track_crosses_tracks[] = {
	TRACK_BIT_Y,    // TRACK_X
	TRACK_BIT_X,    // TRACK_Y
	TRACK_BIT_VERT, // TRACK_UPPER
	TRACK_BIT_VERT, // TRACK_LOWER
	TRACK_BIT_HORZ, // TRACK_LEFT
	TRACK_BIT_HORZ  // TRACK_RIGHT
};

/* Maps a trackdir to the (4-way) direction the tile is exited when following
 * that trackdir */
extern const DiagDirection _trackdir_to_exitdir[TRACKDIR_END] = {
	DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_SW, DIAGDIR_SE, DIAGDIR_NE, DIAGDIR_NE,
	DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NW, DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NE,
};

extern const Trackdir _track_exitdir_to_trackdir[][DIAGDIR_END] = {
	{TRACKDIR_X_NE,     INVALID_TRACKDIR,  TRACKDIR_X_SW,     INVALID_TRACKDIR},
	{INVALID_TRACKDIR,  TRACKDIR_Y_SE,     INVALID_TRACKDIR,  TRACKDIR_Y_NW},
	{TRACKDIR_UPPER_E,  INVALID_TRACKDIR,  INVALID_TRACKDIR,  TRACKDIR_UPPER_W},
	{INVALID_TRACKDIR,  TRACKDIR_LOWER_E,  TRACKDIR_LOWER_W,  INVALID_TRACKDIR},
	{INVALID_TRACKDIR,  INVALID_TRACKDIR,  TRACKDIR_LEFT_S,   TRACKDIR_LEFT_N},
	{TRACKDIR_RIGHT_N,  TRACKDIR_RIGHT_S,  INVALID_TRACKDIR,  INVALID_TRACKDIR}
};

extern const Trackdir _track_enterdir_to_trackdir[][DIAGDIR_END] = {
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

extern const TrackBits _corner_to_trackbits[] = {
	TRACK_BIT_LEFT, TRACK_BIT_LOWER, TRACK_BIT_RIGHT, TRACK_BIT_UPPER,
};

extern const TrackdirBits _uphill_trackdirs[] = {
	TRACKDIR_BIT_NONE                    , ///<  0 SLOPE_FLAT
	TRACKDIR_BIT_X_SW | TRACKDIR_BIT_Y_NW, ///<  1 SLOPE_W   -> inclined for diagonal track
	TRACKDIR_BIT_X_SW | TRACKDIR_BIT_Y_SE, ///<  2 SLOPE_S   -> inclined for diagonal track
	TRACKDIR_BIT_X_SW                    , ///<  3 SLOPE_SW
	TRACKDIR_BIT_X_NE | TRACKDIR_BIT_Y_SE, ///<  4 SLOPE_E   -> inclined for diagonal track
	TRACKDIR_BIT_NONE                    , ///<  5 SLOPE_EW
	TRACKDIR_BIT_Y_SE                    , ///<  6 SLOPE_SE
	TRACKDIR_BIT_NONE                    , ///<  7 SLOPE_WSE -> leveled
	TRACKDIR_BIT_X_NE | TRACKDIR_BIT_Y_NW, ///<  8 SLOPE_N   -> inclined for diagonal track
	TRACKDIR_BIT_Y_NW                    , ///<  9 SLOPE_NW
	TRACKDIR_BIT_NONE                    , ///< 10 SLOPE_NS
	TRACKDIR_BIT_NONE                    , ///< 11 SLOPE_NWS -> leveled
	TRACKDIR_BIT_X_NE                    , ///< 12 SLOPE_NE
	TRACKDIR_BIT_NONE                    , ///< 13 SLOPE_ENW -> leveled
	TRACKDIR_BIT_NONE                    , ///< 14 SLOPE_SEN -> leveled
	TRACKDIR_BIT_NONE                    , ///< 15 invalid
	TRACKDIR_BIT_NONE                    , ///< 16 invalid
	TRACKDIR_BIT_NONE                    , ///< 17 invalid
	TRACKDIR_BIT_NONE                    , ///< 18 invalid
	TRACKDIR_BIT_NONE                    , ///< 19 invalid
	TRACKDIR_BIT_NONE                    , ///< 20 invalid
	TRACKDIR_BIT_NONE                    , ///< 21 invalid
	TRACKDIR_BIT_NONE                    , ///< 22 invalid
	TRACKDIR_BIT_X_SW | TRACKDIR_BIT_Y_SE, ///< 23 SLOPE_STEEP_S -> inclined for diagonal track
	TRACKDIR_BIT_NONE                    , ///< 24 invalid
	TRACKDIR_BIT_NONE                    , ///< 25 invalid
	TRACKDIR_BIT_NONE                    , ///< 26 invalid
	TRACKDIR_BIT_X_SW | TRACKDIR_BIT_Y_NW, ///< 27 SLOPE_STEEP_W -> inclined for diagonal track
	TRACKDIR_BIT_NONE                    , ///< 28 invalid
	TRACKDIR_BIT_X_NE | TRACKDIR_BIT_Y_NW, ///< 29 SLOPE_STEEP_N -> inclined for diagonal track
	TRACKDIR_BIT_X_NE | TRACKDIR_BIT_Y_SE, ///< 30 SLOPE_STEEP_E -> inclined for diagonal track
};
