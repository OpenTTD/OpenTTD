/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file spritecache_internal.h Internal functions to cache sprites in memory. */

#ifndef SPRITECACHE_INTERNAL_H
#define SPRITECACHE_INTERNAL_H

#include "stdafx.h"

#include "core/math_func.hpp"
#include "gfx_type.h"
#include "spriteloader/spriteloader.hpp"

#include "table/sprites.h"

/* These declarations are internal to spritecache but need to be exposed for unit-tests. */

struct SpriteCache {
	void *ptr;
	size_t file_pos;
	SpriteFile *file;    ///< The file the sprite in this entry can be found in.
	uint32_t id;
	int16_t lru;
	SpriteType type;     ///< In some cases a single sprite is misused by two NewGRFs. Once as real sprite and once as recolour sprite. If the recolour sprite gets into the cache it might be drawn as real sprite which causes enormous trouble.
	bool warned;         ///< True iff the user has been warned about incorrect use of this sprite
	byte control_flags;  ///< Control flags, see SpriteCacheCtrlFlags
};

static inline bool IsMapgenSpriteID(SpriteID sprite)
{
	return IsInsideMM(sprite, SPR_MAPGEN_BEGIN, SPR_MAPGEN_END);
}

void *AllocSprite(size_t mem_req);
SpriteCache *AllocateSpriteCache(uint index);

#endif /* SPRITECACHE_INTERNAL_H */
