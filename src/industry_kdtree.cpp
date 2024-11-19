/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file industry_kdtree.cpp Implementation of the industry k-d tree functions */

#include "industry_kdtree.h"
#include "industry.h"

uint16_t Kdtree_IndustryXYFunc::operator()(IndustryID iid, int dim)
{
	return (dim == 0) ? TileX(Industry::Get(iid)->location.tile) : TileY(Industry::Get(iid)->location.tile);
}
