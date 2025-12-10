/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file spritecache_internal.h Internal functions to cache sprites in memory. */

#ifndef SPRITECACHE_INTERNAL_H
#define SPRITECACHE_INTERNAL_H

#include "core/math_func.hpp"
#include "gfx_type.h"
#include "spritecache_type.h"
#include "spriteloader/sprite_file_type.hpp"

#include "table/sprites.h"

/* These declarations are internal to spritecache but need to be exposed for unit-tests. */

struct SpriteCache {
	std::unique_ptr<std::byte[]> ptr;
	size_t file_pos = 0;
	SpriteFile *file = nullptr; ///< The file the sprite in this entry can be found in.
	uint32_t length; ///< Length of sprite data.
	uint32_t id = 0;
	uint32_t lru = 0;
	SpriteType type = SpriteType::Invalid; ///< In some cases a single sprite is misused by two NewGRFs. Once as real sprite and once as recolour sprite. If the recolour sprite gets into the cache it might be drawn as real sprite which causes enormous trouble.
	bool warned = false; ///< True iff the user has been warned about incorrect use of this sprite
	SpriteCacheCtrlFlags control_flags{}; ///< Control flags, see SpriteCacheCtrlFlags

	void ClearSpriteData();
};

inline bool IsMapgenSpriteID(SpriteID sprite)
{
	return IsInsideMM(sprite, SPR_MAPGEN_BEGIN, SPR_MAPGEN_END);
}

SpriteCache *AllocateSpriteCache(uint index);

#endif /* SPRITECACHE_INTERNAL_H */
