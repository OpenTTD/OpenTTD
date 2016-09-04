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

#include "fileio_type.h"

/**
 * Order of these enums has to be the same as in lang/english.txt
 * Otherwise you will get inconsistent behaviour.
 */
enum HeightmapRotation {
	HM_COUNTER_CLOCKWISE, ///< Rotate the map counter clockwise 45 degrees
	HM_CLOCKWISE,         ///< Rotate the map clockwise 45 degrees
};

bool GetHeightmapDimensions(DetailedFileType dft, const char *filename, uint *x, uint *y);
void LoadHeightmap(DetailedFileType dft, const char *filename);
void FlatEmptyWorld(byte tile_height);
void FixSlopes();

#endif /* HEIGHTMAP_H */
