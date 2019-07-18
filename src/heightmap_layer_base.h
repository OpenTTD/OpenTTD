/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file  heightmap_layer_base.h Base class for all heightmap layers. */

#ifndef HEIGHTMAP_LAYER_BASE_H
#define HEIGHTMAP_LAYER_BASE_H

#include "stdafx.h"
#include "heightmap_layer_type.h"
#include <vector> // SFTODO MOVE WHEN I MOVE CODE USING THIS

/** This class is used to represent each one of the layers that can compose an extended heightmap. */
struct HeightmapLayer {
	HeightmapLayerType type; ///< Type of the layer.
	uint width;              ///< Width of the layer.
	uint height;             ///< Height of the layer.
	byte *information;       ///< Information contained in the layer.

	HeightmapLayer(HeightmapLayerType type_, uint width_ = 0, uint height_ = 0, byte *information_ = nullptr)
	: type(type_), width(width_), height(height_), information(information_) {}

	virtual ~HeightmapLayer();
};

// SFTODO: This probably needs moving to its own file, or at least the file containing TownLayer when it moves
struct HeightmapTown {
	std::string name; ///< Name of the town.
	uint posx;	  ///< X position of the town.
	uint posy;        ///< Y position of the town.

	HeightmapTown(std::string name_, uint posx_, uint posy_) : name(name_), posx(posx_), posy(posy_) {}
};

// SFTODO: This derived class should probably have its own file
struct TownLayer : HeightmapLayer {
	bool valid;				///< true iff constructor succeeded
	std::vector<HeightmapTown> towns;	///< List of towns in the layer.

	TownLayer(uint width, uint height, const char *file);
	~TownLayer();
};

#endif /* HEIGHTMAP_LAYER_BASE_H */
