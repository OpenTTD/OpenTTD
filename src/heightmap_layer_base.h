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

// SFTODO: This derived class should probably have its own file
struct TownLayer : HeightmapLayer {
	bool valid;		///< true iff constructor succeeded
	TownLayer(uint width, uint height, const char *file);
	~TownLayer();
};

#endif /* HEIGHTMAP_LAYER_BASE_H */
