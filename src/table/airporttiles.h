/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file airporttiles.h Tables with airporttile defaults. */

#ifndef AIRPORTTILES_H
#define AIRPORTTILES_H

/** Writes all airport tile properties in the AirportTile struct */
#define AT(num_frames, anim_speed) {(1 << 8) | num_frames, anim_speed}
/** Writes an airport tile without animation in the AirportTile struct */
#define AT_NOANIM {0xFFFF, 2}

/** All default airport tiles.
 * @see AirportTiles for a list of names. */
static const AirportTileSpec _origin_airporttile_specs[] = {
	/* 0..9 */
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,

	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,

	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,

	AT_NOANIM,
	AT(12, 2), // APT_RADAR_GRASS_FENCE_SW
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT(4, 1), // APT_GRASS_FENCE_NE_FLAG

	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,

	AT_NOANIM,
	AT(12, 2), // APT_RADAR_FENCE_SW
	AT(12, 2), // APT_RADAR_FENCE_NE
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,

	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,

	AT_NOANIM,
	AT_NOANIM,
	AT_NOANIM,
	AT(4, 1), // APT_GRASS_FENCE_NE_FLAG_2
};

assert_compile(NUM_AIRPORTTILES == lengthof(_origin_airporttile_specs));

#undef AT_NOANIM
#undef AT

#endif /* AIRPORTTILES_H */
