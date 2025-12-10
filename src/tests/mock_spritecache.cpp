/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mock_spritecache.cpp Mock sprite cache implementation. */

#include "../stdafx.h"

#include "../blitter/factory.hpp"
#include "../spritecache.h"
#include "../spritecache_internal.h"
#include "../table/sprites.h"

#include "../safeguards.h"

static bool MockLoadNextSprite(SpriteID load_index)
{
	UniquePtrSpriteAllocator allocator;
	allocator.Allocate<Sprite>(sizeof(Sprite));

	bool is_mapgen = IsMapgenSpriteID(load_index);

	SpriteCache *sc = AllocateSpriteCache(load_index);
	sc->file = nullptr;
	sc->file_pos = 0;
	sc->ptr = std::move(allocator.data);
	sc->length = static_cast<uint32_t>(allocator.size);
	sc->lru = 0;
	sc->id = 0;
	sc->type = is_mapgen ? SpriteType::MapGen : SpriteType::Normal;
	sc->warned = false;
	sc->control_flags = {};

	/* Fill with empty sprites up until the default sprite count. */
	return load_index < SPR_OPENTTD_BASE + OPENTTD_SPRITE_COUNT;
}

void MockGfxLoadSprites()
{
	/* Force blitter 'null'. This is necessary for GfxInitSpriteMem() to function. */
	BlitterFactory::SelectBlitter("null");

	GfxInitSpriteMem();

	SpriteID load_index = 0;
	while (MockLoadNextSprite(load_index)) {
		load_index++;
	}
}
