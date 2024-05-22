/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autoreplace_base.h Base class for autoreplaces/autorenews. */

#ifndef AUTOREPLACE_BASE_H
#define AUTOREPLACE_BASE_H

#include "core/pool_type.hpp"
#include "autoreplace_type.h"
#include "engine_type.h"
#include "group_type.h"

typedef uint16_t EngineRenewID;

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
	bool replace_when_old; ///< Do replacement only when vehicle is old.

	EngineRenew(EngineID from = INVALID_ENGINE, EngineID to = INVALID_ENGINE) : from(from), to(to) {}
	~EngineRenew() {}
};

#endif /* AUTOREPLACE_BASE_H */
