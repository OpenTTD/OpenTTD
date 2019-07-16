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
	uint width;              ///< Width of the layer.
	uint height;             ///< Height of the layer.
	HeightmapLayerType type; ///< Type of the layer.
	byte *information;       ///< Information contained in the layer.

	~HeightmapLayer();
};

#endif /* HEIGHTMAP_LAYER_BASE_H */
