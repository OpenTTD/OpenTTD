/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file heightmap_layer_type.h Basic types related to heightmap layers. */

#ifndef HEIGHTMAP_LAYER_TYPE_H
#define HEIGHTMAP_LAYER_TYPE_H

/**
 * Existing layer types.
 */
enum HeightmapLayerType {
	/** Height layers are 8 bit grayscale images that describe the height of each point of the map as a number in the [0, 255] interval. */
	HLT_HEIGHTMAP,
	/** Town layers are a textual representation of the towns on the map. */
	HLT_TOWN
};

#endif /* HEIGHTMAP_LAYER_TYPE_H */
