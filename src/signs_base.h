/* $Id$ */

/** @file signs_base.h Base class for signs. */

#ifndef SIGNS_BASE_H
#define SIGNS_BASE_H

#include "signs_type.h"
#include "viewport_type.h"
#include "tile_type.h"
#include "oldpool.h"

DECLARE_OLD_POOL(Sign, Sign, 2, 16000)

struct Sign : PoolItem<Sign, SignID, &_Sign_pool> {
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

	inline bool IsValid() const { return this->owner != INVALID_OWNER; }
};

#define FOR_ALL_SIGNS_FROM(ss, start) for (ss = Sign::Get(start); ss != NULL; ss = (ss->index + 1U < Sign::GetPoolSize()) ? Sign::Get(ss->index + 1U) : NULL) if (ss->IsValid())
#define FOR_ALL_SIGNS(ss) FOR_ALL_SIGNS_FROM(ss, 0)

#endif /* SIGNS_BASE_H */
