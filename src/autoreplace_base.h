/* $Id$ */

/** @file autoreplace_base.h Base class for autoreplaces/autorenews. */

#ifndef AUTOREPLACE_BASE_H
#define AUTOREPLACE_BASE_H

#include "core/pool_type.hpp"
#include "autoreplace_type.h"

typedef uint16 EngineRenewID;

/**
 * Memory pool for engine renew elements. DO NOT USE outside of engine.c. Is
 * placed here so the only exception to this rule, the saveload code, can use
 * it.
 */
typedef Pool<EngineRenew, EngineRenewID, 16, 64000> EngineRenewPool;
extern EngineRenewPool _enginerenew_pool;

/**
 * Struct to store engine replacements. DO NOT USE outside of engine.c. Is
 * placed here so the only exception to this rule, the saveload code, can use
 * it.
 */
struct EngineRenew : EngineRenewPool::PoolItem<&_enginerenew_pool> {
	EngineID from;
	EngineID to;
	EngineRenew *next;
	GroupID group_id;

	EngineRenew(EngineID from = INVALID_ENGINE, EngineID to = INVALID_ENGINE) : from(from), to(to) {}
	~EngineRenew() {}
};

#define FOR_ALL_ENGINE_RENEWS_FROM(var, start) FOR_ALL_ITEMS_FROM(EngineRenew, enginerenew_index, var, start)
#define FOR_ALL_ENGINE_RENEWS(var) FOR_ALL_ENGINE_RENEWS_FROM(var, 0)

#endif /* AUTOREPLACE_BASE_H */
