/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file pool_func.cpp Implementation of PoolBase methods. */

#include "../stdafx.h"
#include "pool_type.hpp"

#include "../safeguards.h"

/**
 * Destructor removes this object from the pool vector and
 * deletes the vector itself if this was the last item removed.
 */
/* virtual */ PoolBase::~PoolBase()
{
	PoolVector *pools = PoolBase::GetPools();
	pools->erase(std::find(pools->begin(), pools->end(), this));
	if (pools->empty()) delete pools;
}

/**
 * Clean all pools of given type.
 * @param pt pool types to clean.
 */
/* static */ void PoolBase::Clean(PoolType pt)
{
	for (PoolBase *pool : *PoolBase::GetPools()) {
		if (pool->type & pt) pool->CleanPool();
	}
}
