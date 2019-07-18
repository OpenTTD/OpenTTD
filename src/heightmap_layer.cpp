/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file heightmap_layer.cpp Base implementation of heightmap layers. */

#include "stdafx.h"
#include "heightmap_layer_base.h"

HeightmapLayer::~HeightmapLayer() {
	free(this->information);
}

// SFTODO: THIS DERIVED CLASS SHOULD PROBABLY HAVE ITS OWN FILE

TownLayer::TownLayer(uint width, uint height, const char *file)
: HeightmapLayer(HLT_TOWN, width, height), valid(false)
{
	this->valid = true; // SFTODO: MAKE SURE THIS IS LAST LINE OF CTOR!
}

TownLayer::~TownLayer()
{
}
