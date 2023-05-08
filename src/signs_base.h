/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file signs_base.h Base class for signs. */

#ifndef SIGNS_BASE_H
#define SIGNS_BASE_H

#include "signs_type.h"
#include "viewport_type.h"
#include "core/pool_type.hpp"
#include "company_type.h"

typedef Pool<Sign, SignID, 16, 64000> SignPool;
extern SignPool _sign_pool;

struct Sign : SignPool::PoolItem<&_sign_pool> {
	std::string         name;
	TrackedViewportSign sign;
	int32_t               x;
	int32_t               y;
	int32_t               z;
	Owner               owner; // placed by this company. Anyone can delete them though. OWNER_NONE for gray signs from old games.

	Sign(Owner owner = INVALID_OWNER);
	~Sign();

	void UpdateVirtCoord();
};

#endif /* SIGNS_BASE_H */
