/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file heightmap.h Functions related to creating heightmaps from files. */

#ifndef HEIGHTMAP_H
#define HEIGHTMAP_H

/*
 * Order of these enums has to be the same as in lang/english.txt
 * Otherwise you will get inconsistent behaviour.
 */
enum {
	HM_COUNTER_CLOCKWISE, ///< Rotate the map counter clockwise 45 degrees
	HM_CLOCKWISE,         ///< Rotate the map clockwise 45 degrees
};

/**
 * Get the dimensions of a heightmap.
 * @param filename to query
 * @param x dimension x
 * @param y dimension y
 * @return Returns false if loading of the image failed.
 */
bool GetHeightmapDimensions(char *filename, uint *x, uint *y);

/**
 * Load a heightmap from file and change the map in his current dimensions
 *  to a landscape representing the heightmap.
 * It converts pixels to height. The brighter, the higher.
 * @param filename of the heighmap file to be imported
 */
void LoadHeightmap(char *filename);

/**
 * Make an empty world where all tiles are of height 'tile_height'.
 * @param tile_height of the desired new empty world
 */
void FlatEmptyWorld(byte tile_height);

/**
 * This function takes care of the fact that land in OpenTTD can never differ
 * more than 1 in height
 */
void FixSlopes();

#endif /* HEIGHTMAP_H */
