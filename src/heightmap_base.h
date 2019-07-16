/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file heightmap_base.h Base class for heightmaps and its components. */

#ifndef HEIGHTMAP_BASE_H
#define HEIGHTMAP_BASE_H

#include "core/smallmap_type.hpp"
#include "heightmap_type.h"
#include "heightmap_layer_type.h"
#include "heightmap_layer_base.h"

typedef SmallMap<HeightmapLayerType, HeightmapLayer *, 1> HeightmapLayerMap;

struct ExtendedHeightmap {
	HeightmapLayerMap layers; ///< Map of layers.

	/* Extended heightmap information. */
	char filename[64];           ///< Name of the file from which the extended heightmap was loaded.
	byte max_map_height;         ///< This property indicates the maximum height level of the values defined in the extended heightmap.
	byte min_map_desired_height; ///< Indicates the minimum height of the resulting OpenTTD map.
	byte max_map_desired_height; ///< Indicates the maximum height of the resulting OpenTTD map.
	uint width;                  ///< The extended heightmap will be scaled to this width.
	uint height;                 ///< The extended heightmap will be scaled to this height.
	HeightmapRotation rotation;  ///< Indicates the preferred orientation for the extended heightmap.
	bool freeform_edges;         ///< True if the extended heightmap should have freeform edges. This is always true except for legacy heightmaps, which will use the current setting value. This value will not be exposed to the extended heightmap property section in the metadata file.

	/* Extended heightmap load. */
	void LoadLegacyHeightmap(char *file_path, char *file_name);

	/* Map creation methods. */
	void CreateMap();
	void ApplyLayers();

	/* Layer apply methods. */
	void ApplyHeightLayer(const HeightmapLayer *height_layer);

	/* Other functions. */
	bool IsValid();
};

#endif /* HEIGHTMAP_BASE_H */
