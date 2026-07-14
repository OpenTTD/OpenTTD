/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file track_data.h Data related to rail tracks. */

/**
 * Maps a diagonal direction to the all trackdirs that are connected to any
 * track entering in this direction (including those making 90 degree turns).
 */
extern const DiagDirectionIndexArray<TrackdirBits> _exitdir_reaches_trackdirs{{{
	{Trackdir::X_NE, Trackdir::Lower_E, Trackdir::Left_N}, // DiagDirection::NE
	{Trackdir::Y_SE, Trackdir::Left_S, Trackdir::Upper_E}, // DiagDirection::SE
	{Trackdir::X_SW, Trackdir::Upper_W, Trackdir::Right_S}, // DiagDirection::SW
	{Trackdir::Y_NW, Trackdir::Right_N, Trackdir::Lower_W}, // DiagDirection::NW
}}};

/** Next trackdir to use when moving to a new tile for each current trackdir. */
extern const TrackdirIndexArray<Trackdir> _next_trackdir{
	Trackdir::X_NE, Trackdir::Y_SE, Trackdir::Lower_E, Trackdir::Upper_E, Trackdir::Right_S, Trackdir::Left_S, Trackdir::Invalid, Trackdir::Invalid,
	Trackdir::X_SW, Trackdir::Y_NW, Trackdir::Lower_W, Trackdir::Upper_W, Trackdir::Right_N, Trackdir::Left_N
};

/** Maps a trackdir to all trackdirs that make 90 deg turns with it. */
extern const TrackIndexArray<TrackdirBits> _track_crosses_trackdirs{{{
	{Trackdir::Y_SE, Trackdir::Y_NW}, // TRACK_X
	{Trackdir::X_NE, Trackdir::X_SW}, // TRACK_Y
	{Trackdir::Right_N, Trackdir::Right_S, Trackdir::Left_N, Trackdir::Left_S}, // TRACK_UPPER
	{Trackdir::Right_N, Trackdir::Right_S, Trackdir::Left_N, Trackdir::Left_S}, // TRACK_LOWER
	{Trackdir::Upper_W, Trackdir::Upper_E, Trackdir::Lower_W, Trackdir::Lower_E}, // TRACK_LEFT
	{Trackdir::Upper_W, Trackdir::Upper_E, Trackdir::Lower_W, Trackdir::Lower_E}, // TRACK_RIGHT
}}};

/** Maps a track to all tracks that make 90 deg turns with it. */
extern const TrackIndexArray<TrackBits> _track_crosses_tracks{
	Track::Y, // Track::X
	Track::X, // Track::Y
	{Track::Left, Track::Right}, // Track::Upper
	{Track::Left, Track::Right}, // Track::Lower
	{Track::Upper, Track::Lower}, // Track::Left
	{Track::Upper, Track::Lower}, // Track::Right
};

/** Maps a trackdir to the (4-way) direction the tile is exited when following that trackdir. */
extern const TrackdirIndexArray<DiagDirection> _trackdir_to_exitdir{
	DiagDirection::NE, DiagDirection::SE, DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::SE, DiagDirection::NE, DiagDirection::NE,
	DiagDirection::SW, DiagDirection::NW, DiagDirection::NW, DiagDirection::SW, DiagDirection::NW, DiagDirection::NE,
};

/** Maps a track and an (4-way) dir to the trackdir that represents the track with the exit in the given direction. */
extern const TrackIndexArray<DiagDirectionIndexArray<Trackdir>> _track_exitdir_to_trackdir{{{
	{Trackdir::X_NE,     Trackdir::Invalid,  Trackdir::X_SW,     Trackdir::Invalid},
	{Trackdir::Invalid,  Trackdir::Y_SE,     Trackdir::Invalid,  Trackdir::Y_NW},
	{Trackdir::Upper_E,  Trackdir::Invalid,  Trackdir::Invalid,  Trackdir::Upper_W},
	{Trackdir::Invalid,  Trackdir::Lower_E,  Trackdir::Lower_W,  Trackdir::Invalid},
	{Trackdir::Invalid,  Trackdir::Invalid,  Trackdir::Left_S,   Trackdir::Left_N},
	{Trackdir::Right_N,  Trackdir::Right_S,  Trackdir::Invalid,  Trackdir::Invalid}
}}};

/** Maps a track and an (4-way) dir to the trackdir that represents the track with the entry in the given direction. */
extern const TrackIndexArray<DiagDirectionIndexArray<Trackdir>> _track_enterdir_to_trackdir{{{
	{Trackdir::X_NE,     Trackdir::Invalid,  Trackdir::X_SW,     Trackdir::Invalid},
	{Trackdir::Invalid,  Trackdir::Y_SE,     Trackdir::Invalid,  Trackdir::Y_NW},
	{Trackdir::Invalid,  Trackdir::Upper_E,  Trackdir::Upper_W,  Trackdir::Invalid},
	{Trackdir::Lower_E,  Trackdir::Invalid,  Trackdir::Invalid,  Trackdir::Lower_W},
	{Trackdir::Left_N,   Trackdir::Left_S,   Trackdir::Invalid,  Trackdir::Invalid},
	{Trackdir::Invalid,  Trackdir::Invalid,  Trackdir::Right_S,  Trackdir::Right_N}
}}};

/** Maps a track and a full (8-way) direction to the trackdir that represents the track running in the given direction. */
extern const TrackIndexArray<DirectionIndexArray<Trackdir>> _track_direction_to_trackdir{{{
	{Trackdir::Invalid, Trackdir::X_NE,     Trackdir::Invalid, Trackdir::Invalid,  Trackdir::Invalid, Trackdir::X_SW,     Trackdir::Invalid, Trackdir::Invalid},
	{Trackdir::Invalid, Trackdir::Invalid,  Trackdir::Invalid, Trackdir::Y_SE,     Trackdir::Invalid, Trackdir::Invalid,  Trackdir::Invalid, Trackdir::Y_NW},
	{Trackdir::Invalid, Trackdir::Invalid,  Trackdir::Upper_E, Trackdir::Invalid,  Trackdir::Invalid, Trackdir::Invalid,  Trackdir::Upper_W, Trackdir::Invalid},
	{Trackdir::Invalid, Trackdir::Invalid,  Trackdir::Lower_E, Trackdir::Invalid,  Trackdir::Invalid, Trackdir::Invalid,  Trackdir::Lower_W, Trackdir::Invalid},
	{Trackdir::Left_N,  Trackdir::Invalid,  Trackdir::Invalid, Trackdir::Invalid,  Trackdir::Left_S,  Trackdir::Invalid,  Trackdir::Invalid, Trackdir::Invalid},
	{Trackdir::Right_N, Trackdir::Invalid,  Trackdir::Invalid, Trackdir::Invalid,  Trackdir::Right_S, Trackdir::Invalid,  Trackdir::Invalid, Trackdir::Invalid}
}}};

/** Maps a (4-way) direction to the diagonal trackdir that runs in that direction. */
extern const DiagDirectionIndexArray<Trackdir> _dir_to_diag_trackdir{
	Trackdir::X_NE, Trackdir::Y_SE, Trackdir::X_SW, Trackdir::Y_NW,
};

/** Maps a single horizontal/vertical trackbit that is in a specific tile corner. */
extern const CornerIndexArray<TrackBits> _corner_to_trackbits{
	Track::Left, Track::Lower, Track::Right, Track::Upper,
};

extern const TrackdirBits _uphill_trackdirs[] = {
	{}, // 0 SLOPE_FLAT
	{Trackdir::X_SW, Trackdir::Y_NW}, // 1 SLOPE_W   -> inclined for diagonal track
	{Trackdir::X_SW, Trackdir::Y_SE}, // 2 SLOPE_S   -> inclined for diagonal track
	{Trackdir::X_SW}, // 3 SLOPE_SW
	{Trackdir::X_NE, Trackdir::Y_SE}, // 4 SLOPE_E   -> inclined for diagonal track
	{}, // 5 SLOPE_EW
	{Trackdir::Y_SE}, // 6 SLOPE_SE
	{}, // 7 SLOPE_WSE -> leveled
	{Trackdir::X_NE, Trackdir::Y_NW}, // 8 SLOPE_N   -> inclined for diagonal track
	{Trackdir::Y_NW}, // 9 SLOPE_NW
	{}, // 10 SLOPE_NS
	{}, // 11 SLOPE_NWS -> leveled
	{Trackdir::X_NE}, // 12 SLOPE_NE
	{}, // 13 SLOPE_ENW -> leveled
	{}, // 14 SLOPE_SEN -> leveled
	{}, // 15 invalid
	{}, // 16 invalid
	{}, // 17 invalid
	{}, // 18 invalid
	{}, // 19 invalid
	{}, // 20 invalid
	{}, // 21 invalid
	{}, // 22 invalid
	{Trackdir::X_SW, Trackdir::Y_SE}, // 23 SLOPE_STEEP_S -> inclined for diagonal track
	{}, // 24 invalid
	{}, // 25 invalid
	{}, // 26 invalid
	{Trackdir::X_SW, Trackdir::Y_NW}, // 27 SLOPE_STEEP_W -> inclined for diagonal track
	{}, // 28 invalid
	{Trackdir::X_NE, Trackdir::Y_NW}, // 29 SLOPE_STEEP_N -> inclined for diagonal track
	{Trackdir::X_NE, Trackdir::Y_SE}, // 30 SLOPE_STEEP_E -> inclined for diagonal track
};
