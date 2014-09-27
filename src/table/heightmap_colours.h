/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file heightmap_colours.h The colour tables for heightmaps.
 */

/** Height map colours for the green colour scheme, ordered by height. */
static const uint32 _green_map_heights[] = {
	MKCOLOUR_XXXX(0x5A),
	MKCOLOUR_XYXY(0x5A, 0x5B),
	MKCOLOUR_XXXX(0x5B),
	MKCOLOUR_XYXY(0x5B, 0x5C),
	MKCOLOUR_XXXX(0x5C),
	MKCOLOUR_XYXY(0x5C, 0x5D),
	MKCOLOUR_XXXX(0x5D),
	MKCOLOUR_XYXY(0x5D, 0x5E),
	MKCOLOUR_XXXX(0x5E),
	MKCOLOUR_XYXY(0x5E, 0x5F),
	MKCOLOUR_XXXX(0x5F),
	MKCOLOUR_XYXY(0x5F, 0x1F),
	MKCOLOUR_XXXX(0x1F),
	MKCOLOUR_XYXY(0x1F, 0x27),
	MKCOLOUR_XXXX(0x27),
	MKCOLOUR_XXXX(0x27),
};
assert_compile(lengthof(_green_map_heights) == MAX_TILE_HEIGHT + 1);

/** Height map colours for the dark green colour scheme, ordered by height. */
static const uint32 _dark_green_map_heights[] = {
	MKCOLOUR_XXXX(0x60),
	MKCOLOUR_XYXY(0x60, 0x61),
	MKCOLOUR_XXXX(0x61),
	MKCOLOUR_XYXY(0x61, 0x62),
	MKCOLOUR_XXXX(0x62),
	MKCOLOUR_XYXY(0x62, 0x63),
	MKCOLOUR_XXXX(0x63),
	MKCOLOUR_XYXY(0x63, 0x64),
	MKCOLOUR_XXXX(0x64),
	MKCOLOUR_XYXY(0x64, 0x65),
	MKCOLOUR_XXXX(0x65),
	MKCOLOUR_XYXY(0x65, 0x66),
	MKCOLOUR_XXXX(0x66),
	MKCOLOUR_XYXY(0x66, 0x67),
	MKCOLOUR_XXXX(0x67),
	MKCOLOUR_XXXX(0x67),
};
assert_compile(lengthof(_dark_green_map_heights) == MAX_TILE_HEIGHT + 1);

/** Height map colours for the violet colour scheme, ordered by height. */
static const uint32 _violet_map_heights[] = {
	MKCOLOUR_XXXX(0x80),
	MKCOLOUR_XYXY(0x80, 0x81),
	MKCOLOUR_XXXX(0x81),
	MKCOLOUR_XYXY(0x81, 0x82),
	MKCOLOUR_XXXX(0x82),
	MKCOLOUR_XYXY(0x82, 0x83),
	MKCOLOUR_XXXX(0x83),
	MKCOLOUR_XYXY(0x83, 0x84),
	MKCOLOUR_XXXX(0x84),
	MKCOLOUR_XYXY(0x84, 0x85),
	MKCOLOUR_XXXX(0x85),
	MKCOLOUR_XYXY(0x85, 0x86),
	MKCOLOUR_XXXX(0x86),
	MKCOLOUR_XYXY(0x86, 0x87),
	MKCOLOUR_XXXX(0x87),
	MKCOLOUR_XXXX(0x87),
};
assert_compile(lengthof(_violet_map_heights) == MAX_TILE_HEIGHT + 1);
