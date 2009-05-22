/* $Id$ */

/** @file signs_base.h Base class for signs. */

#ifndef SIGNS_BASE_H
#define SIGNS_BASE_H

#include "signs_type.h"
#include "viewport_type.h"
#include "tile_type.h"
#include "core/pool_type.hpp"

typedef Pool<Sign, SignID, 16, 64000> SignPool;
extern SignPool _sign_pool;

struct Sign : SignPool::PoolItem<&_sign_pool> {
	char *name;
	ViewportSign sign;
	int32        x;
	int32        y;
	byte         z;
	OwnerByte    owner; // placed by this company. Anyone can delete them though. OWNER_NONE for gray signs from old games.

	/**
	 * Creates a new sign
	 */
	Sign(Owner owner = INVALID_OWNER);

	/** Destroy the sign */
	~Sign();
};

#define FOR_ALL_SIGNS_FROM(var, start) FOR_ALL_ITEMS_FROM(Sign, sign_index, var, start)
#define FOR_ALL_SIGNS(var) FOR_ALL_SIGNS_FROM(var, 0)

#endif /* SIGNS_BASE_H */
