/* $Id$ */

/** @file autoreplace_base.h Base class for autoreplaces/autorenews. */

#ifndef AUTOREPLACE_BASE_H
#define AUTOREPLACE_BASE_H

#include "oldpool.h"
#include "autoreplace_type.h"

/**
 * Memory pool for engine renew elements. DO NOT USE outside of engine.c. Is
 * placed here so the only exception to this rule, the saveload code, can use
 * it.
 */
DECLARE_OLD_POOL(EngineRenew, EngineRenew, 3, 8000)

/**
 * Struct to store engine replacements. DO NOT USE outside of engine.c. Is
 * placed here so the only exception to this rule, the saveload code, can use
 * it.
 */
struct EngineRenew : PoolItem<EngineRenew, EngineRenewID, &_EngineRenew_pool> {
	EngineID from;
	EngineID to;
	EngineRenew *next;
	GroupID group_id;

	EngineRenew(EngineID from = INVALID_ENGINE, EngineID to = INVALID_ENGINE) : from(from), to(to), next(NULL) {}
	~EngineRenew() { this->from = INVALID_ENGINE; }

	inline bool IsValid() const { return this->from != INVALID_ENGINE; }
};

#define FOR_ALL_ENGINE_RENEWS_FROM(er, start) for (er = GetEngineRenew(start); er != NULL; er = (er->index + 1U < GetEngineRenewPoolSize()) ? GetEngineRenew(er->index + 1U) : NULL) if (er->IsValid())
#define FOR_ALL_ENGINE_RENEWS(er) FOR_ALL_ENGINE_RENEWS_FROM(er, 0)

#endif /* AUTOREPLACE_BASE_H */
