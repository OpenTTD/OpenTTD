#include "rail.h"

/* Maps a trackdir to the bit that stores its status in the map arrays, in the
 * direction along with the trackdir */
const byte _signal_along_trackdir[] = {
	0x80, 0x80, 0x80, 0x20, 0x40, 0x10, 0, 0,
	0x40, 0x40, 0x40, 0x10, 0x80, 0x20
};

/* Maps a trackdir to the bit that stores its status in the map arrays, in the
 * direction against the trackdir */
const byte _signal_against_trackdir[] = {
	0x40, 0x40, 0x40, 0x10, 0x80, 0x20, 0, 0,
	0x80, 0x80, 0x80, 0x20, 0x40, 0x10
};

/* Maps a Track to the bits that store the status of the two signals that can
 * be present on the given track */
const byte _signal_on_track[] = {
	0xC0, 0xC0, 0xC0, 0x30, 0xC0, 0x30
};

/* Maps a diagonal direction to the all trackdirs that are connected to any
 * track entering in this direction (including those making 90 degree turns)
 */
const TrackdirBits _exitdir_reaches_trackdirs[] = {
	TRACKDIR_BIT_DIAG1_NE|TRACKDIR_BIT_LOWER_E|TRACKDIR_BIT_LEFT_N,  /* DIAGDIR_NE */
	TRACKDIR_BIT_DIAG2_SE|TRACKDIR_BIT_LEFT_S |TRACKDIR_BIT_UPPER_E, /* DIAGDIR_SE */
	TRACKDIR_BIT_DIAG1_SW|TRACKDIR_BIT_UPPER_W|TRACKDIR_BIT_RIGHT_S, /* DIAGDIR_SW */
	TRACKDIR_BIT_DIAG2_NW|TRACKDIR_BIT_RIGHT_N|TRACKDIR_BIT_LOWER_W  /* DIAGDIR_NW */
};

/* TODO: Remove magic numbers from tables below just like
 * _exitdir_reaches_trackdirs[] */

const Trackdir _next_trackdir[14] = {
	0,  1,  3,  2,  5,  4, 0, 0,
	8,  9,  11, 10, 13, 12
};

/* Maps a trackdir to all trackdirs that make 90 deg turns with it. */
const TrackdirBits _trackdir_crosses_trackdirs[] = {
	0x0202, 0x0101, 0x3030, 0x3030, 0x0C0C, 0x0C0C, 0, 0,
	0x0202, 0x0101, 0x3030, 0x3030, 0x0C0C, 0x0C0C
};

/* Maps a track to all tracks that make 90 deg turns with it. */
const TrackBits _track_crosses_tracks[] = {
	0x2, /* Track 1 -> Track 2 */
	0x1, /* Track 2 -> Track 1 */
	0x30, /* Upper -> Left | Right */
	0x30, /* Lower -> Left | Right */
	0x0C, /* Left -> Upper | Lower */
	0x0C, /* Right -> Upper | Lower */
};

/* Maps a trackdir to the (4-way) direction the tile is exited when following
 * that trackdir */
const DiagDirection _trackdir_to_exitdir[] = {
	0,1,0,1,2,1, 0,0,
	2,3,3,2,3,0,
};

const Trackdir _track_exitdir_to_trackdir[][DIAGDIR_END] = {
	{0,    0xff, 8,    0xff},
	{0xff, 1,    0xff, 9},
	{2,    0xff, 0xff, 10},
	{0xff, 3,    11,   0xf},
	{0xff, 0xff, 4,    12},
	{13,   5,    0xff, 0xff}
};

const Trackdir _track_direction_to_trackdir[][DIR_END] = {
	{0xff, 0,    0xff, 0xff, 0xff, 8,    0xff, 0xff},
	{0xff, 0xff, 0xff, 1,    0xff, 0xff, 0xff, 9},
	{0xff, 0xff, 2,    0xff, 0xff, 0xff, 10,   0xff},
	{0xff, 0xff, 3,    0xff, 0xff, 0xff, 11,   0xff},
	{12,   0xff, 0xff, 0xff, 4,    0xff, 0xff, 0xff},
	{13,   0xff, 0xff, 0xff, 5,    0xff, 0xff, 0xff}
};

const Trackdir _dir_to_diag_trackdir[] = {
	0, 1, 8, 9,
};

const DiagDirection _reverse_diagdir[] = {
	2, 3, 0, 1
};

const Trackdir _reverse_trackdir[] = {
	8, 9, 10, 11, 12, 13, 0xFF, 0xFF,
	0, 1, 2,  3,  4,  5
};
