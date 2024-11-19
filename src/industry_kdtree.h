/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file industry_kdtree.h Declarations for accessing the k-d tree of industries */

#ifndef INDUSTRY_KDTREE_H
#define INDUSTRY_KDTREE_H

#include "core/kdtree.hpp"
#include "industry_type.h"

/* Forward declaration of Industry struct */
struct Industry;

struct Kdtree_IndustryXYFunc {
	uint16_t operator()(IndustryID iid, int dim);
};

using IndustryKdtree = Kdtree<IndustryID, Kdtree_IndustryXYFunc, uint16_t, int>;

#endif
