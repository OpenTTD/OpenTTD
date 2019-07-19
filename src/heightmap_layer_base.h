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
#include <vector>
#include "town_type.h"
#include "heightmap_type.h"

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

/** This class is used to represent a town on the town layer of an extended heightmap. */
struct HeightmapTown {
	std::string name;  ///< Name of the town.
	uint posx;	   ///< Desired X position of the town on heightmap.
	uint posy;         ///< Desired Y position of the town on heightmap.
	uint radius;       ///< Radius to search for suitable position on heightmap from (posx, posy).
	TownSize size;     ///< Size of the town.
	bool city;         ///< Is this a city?
	TownLayout layout; ///< Layout of the town.

	HeightmapTown(std::string name_, uint posx_, uint posy_, uint radius_, TownSize size_, bool city_, TownLayout layout_)
	: name(name_), posx(posx_), posy(posy_), radius(radius_), size(size_), city(city_), layout(layout_) {}
};

/** This class is used to represent a town layer in an extended heightmap. */
struct TownLayer : HeightmapLayer {
	bool valid;				///< true iff constructor succeeded.
	std::vector<HeightmapTown> towns;	///< List of towns in the layer.

	TownLayer(uint width, uint height, uint default_radius, const char *file);
	~TownLayer();
};

#endif /* HEIGHTMAP_LAYER_BASE_H */
