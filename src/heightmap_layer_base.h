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
#include <iostream> // SFTODO TEMP
#include "heightmap_layer_type.h"
#include <vector> // SFTODO MOVE WHEN I MOVE CODE USING THIS
#include "town_type.h" // SFTODO MOVE WHEN MOVE CODE USING THIS
#include "heightmap_type.h" // SFTODO MOVE WHEN MOVE CODE USING THIS
#include "map_func.h" // SFTODO MOVE WHEN MOVE CODE USING THIS, MAYBE TEMP

/** This class is used to represent each one of the layers that can compose an extended heightmap. */
struct HeightmapLayer {
	HeightmapLayerType type; ///< Type of the layer.
	uint width;              ///< Width of the layer.
	uint height;             ///< Height of the layer.
	byte *information;       ///< Information contained in the layer.

	HeightmapLayer(HeightmapLayerType type_, uint width_ = 0, uint height_ = 0, byte *information_ = nullptr)
	: type(type_), width(width_), height(height_), information(information_) {}

	virtual ~HeightmapLayer();

	virtual void Transform(HeightmapRotation rotation, uint target_width, uint target_height) {};
};

// SFTODO: This probably needs moving to its own file, or at least the file containing TownLayer when it moves
struct HeightmapTown {
	std::string name;  ///< Name of the town.
	uint posx;	   ///< X position of the town on heightmap.
	uint posy;         ///< Y position of the town on heightmap.
	uint mapx; // SFTODO COMMENT - NO, DELETE
	uint mapy; // SFTODO COMMENT - NO, DELETE
	TownSize size;     ///< Size of the town.
	bool city;         ///< Is this a city?
	TownLayout layout; ///< Layout of the town.

	HeightmapTown(std::string name_, uint posx_, uint posy_, TownSize size_, bool city_, TownLayout layout_)
	: name(name_), posx(posx_), posy(posy_), size(size_), city(city_), layout(layout_) {}

	// SFTODO: SHOULDN'T REALLY BE INLINE, IT'S JUST CONVENIENT FOR NOW
	// SFTODO: RENAME THIS "TRANSFORM"
	// SFTODO: GET RID OF _after_rotation IN ARG NAME, REALLY ALL THE INT PARMS SHOUDL HAVE THIS SO JUST DOCUMENT THEY ARE AFTER-ROTATION NUMBERS IN A COMMENT HERE
	void Transform(HeightmapRotation rotation, uint height_after_rotation, int num_width, int num_height, int div_width, int div_height)
	{
		std::cout << "SFTODO: TOWN TRANSFOR MAPMAXX " << MapMaxX() << " MAPMAXY " << MapMaxY() << std::endl;
		std::cout << "SFTODO: TOWN " << name << " BEFORE TRANSFORM POSX " << posx << " POSY " << posy << std::endl;
		uint x = this->posx;
		uint y = this->posy;
		//SFTODO COMMENT - THIS IS ROTATION AND SCALING (NB I BELIEVE WIDTH/HEIGHT IN ALL LAYERS OF HEIGHTMAP ARE THE "UNROTATED" VALUES, IE AS IF ROTATION WERE COUNTERCLOCKWISE)
		if (rotation == HM_CLOCKWISE) {
			auto newx = y;
			y = (height_after_rotation - 1) - x; // SFTODO: CAN WE REPLACE HEIGH_AFTER_ROTATIO WITH MAPMAXY()+1 OR SIMILAR?
			x = newx;
		}
		x *= num_width;
		x /= div_width;
		y *= num_height;
		y /= div_height;
		// SFTODO: COMMENT - THIS IS TO ADJUST FOR OTTD COORDS STARTING AT TOP RIGHT NOT BOTTOM LEFT
		assert(x <= MapMaxX());
		assert(y <= MapMaxY());
		x = MapMaxX() - x;
		y = MapMaxY() - y;
		// SFTODO WE NEED TO ALLOW FOR FACT A 512X512 HEIGHTMAP GIVES A 510X510 MAP
		std::cout << "SFTODO: TOWN " << name << " AFTER TRANSFORM MAPX " << x << " MAPY " << y << std::endl;
		assert(x <= MapMaxX());
		assert(y <= MapMaxY());
		this->mapx = x;
		this->mapy = y;
	}
};

// SFTODO: This derived class should probably have its own file
struct TownLayer : HeightmapLayer {
	bool valid;				///< true iff constructor succeeded
	std::vector<HeightmapTown> towns;	///< List of towns in the layer.

	TownLayer(uint width, uint height, const char *file);
	~TownLayer();

	void Transform(HeightmapRotation rotation, uint target_width, uint target_height);
};

#endif /* HEIGHTMAP_LAYER_BASE_H */
