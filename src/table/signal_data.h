/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file signal_data.h Data related to rail signals. */

/* XXX: Below 3 tables store duplicate data. Maybe remove some? */
/* Maps a trackdir to the bit that stores its status in the map arrays, in the
 * direction along with the trackdir */
extern const uint8_t _signal_along_trackdir[TRACKDIR_END] = {
	0x8, 0x8, 0x8, 0x2, 0x4, 0x1, 0, 0,
	0x4, 0x4, 0x4, 0x1, 0x8, 0x2
};

/* Maps a trackdir to the bit that stores its status in the map arrays, in the
 * direction against the trackdir */
extern const uint8_t _signal_against_trackdir[TRACKDIR_END] = {
	0x4, 0x4, 0x4, 0x1, 0x8, 0x2, 0, 0,
	0x8, 0x8, 0x8, 0x2, 0x4, 0x1
};

/* Maps a Track to the bits that store the status of the two signals that can
 * be present on the given track */
extern const uint8_t _signal_on_track[] = {
	0xC, 0xC, 0xC, 0x3, 0xC, 0x3
};
