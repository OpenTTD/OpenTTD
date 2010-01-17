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
#define AT(anim_next, anim_speed) {anim_next, anim_speed}
/** All default airport tiles.
 * @see AirportTiles for a list of names. */
static const AirportTileSpec _origin_airporttile_specs[] = {
	/* 0..9 */
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),

	/* 10..19 */
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),

	/* 20..29*/
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),

	/* 30..39*/
	AT(AIRPORTTILE_NOANIM, 2),
	AT(                32, 2),
	AT(                33, 2),
	AT(                34, 2),
	AT(                35, 2),
	AT(                36, 2),
	AT(                37, 2),
	AT(                38, 2),
	AT(                39, 2),
	AT(                40, 2),

	/* 40..49 */
	AT(                41, 2),
	AT(                42, 2),
	AT(                31, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),

	/* 50..59 */
	AT(                51, 1),
	AT(                52, 1),
	AT(                53, 1),
	AT(                50, 1),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),

	/* 60..69 */
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(                67, 2),
	AT(                68, 2),
	AT(                69, 2),
	AT(                70, 2),

	/* 70..79 */
	AT(                71, 2),
	AT(                72, 2),
	AT(                73, 2),
	AT(                74, 2),
	AT(                75, 2),
	AT(                76, 2),
	AT(                77, 2),
	AT(                66, 2),
	AT(                79, 2),
	AT(                80, 2),

	/* 80..89 */
	AT(                81, 2),
	AT(                82, 2),
	AT(                83, 2),
	AT(                84, 2),
	AT(                85, 2),
	AT(                86, 2),
	AT(                87, 2),
	AT(                88, 2),
	AT(                89, 2),
	AT(                78, 2),

	/* 90..99 */
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),

	/* 100..109 */
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),

	/* 110..119 */
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),

	/* 120..129 */
	AT(AIRPORTTILE_NOANIM, 2),
	AT(               122, 2),
	AT(               123, 2),
	AT(               124, 2),
	AT(               125, 2),
	AT(               126, 2),
	AT(               127, 2),
	AT(               128, 2),
	AT(               129, 2),
	AT(               130, 2),

	/* 130..139 */
	AT(               131, 2),
	AT(               132, 2),
	AT(               121, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),
	AT(AIRPORTTILE_NOANIM, 2),

	/* 140..143 */
	AT(               141, 1),
	AT(               142, 1),
	AT(               143, 1),
	AT(               140, 1),
};

assert_compile(NUM_AIRPORTTILES == lengthof(_origin_airporttile_specs));

#undef AT

#endif /* AIRPORTTILES_H */
