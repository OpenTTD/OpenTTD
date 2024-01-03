/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file mock_spritecache.cpp Mock sprite cache implementation. */

#include "../stdafx.h"

#include "../blitter/factory.hpp"
#include "../core/math_func.hpp"
#include "../spritecache.h"
#include "../spritecache_internal.h"
#include "../table/sprites.h"

static bool MockLoadNextSprite(int load_index)
{
	static Sprite *sprite = (Sprite *)AllocSprite(sizeof(*sprite));

	bool is_mapgen = IsMapgenSpriteID(load_index);

	SpriteCache *sc = AllocateSpriteCache(load_index);
	sc->file = nullptr;
	sc->file_pos = 0;
	sc->ptr = sprite;
	sc->lru = 0;
	sc->id = 0;
	sc->type = is_mapgen ? SpriteType::MapGen : SpriteType::Normal;
	sc->warned = false;
	sc->control_flags = 0;

	/* Fill with empty sprites up until the default sprite count. */
	return (uint)load_index < SPR_OPENTTD_BASE + OPENTTD_SPRITE_COUNT;
}

void MockGfxLoadSprites()
{
	/* Force blitter 'null'. This is necessary for GfxInitSpriteMem() to function. */
	BlitterFactory::SelectBlitter("null");

	GfxInitSpriteMem();

	int load_index = 0;
	while (MockLoadNextSprite(load_index)) {
		load_index++;
	}
}
