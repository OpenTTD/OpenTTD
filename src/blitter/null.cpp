/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file null.cpp A blitter that doesn't blit. */

#include "../stdafx.h"
#include "null.hpp"

#include "../safeguards.h"

/** Instantiation of the null blitter factory. */
static FBlitter_Null iFBlitter_Null;

Sprite *Blitter_Null::Encode(SpriteType, const SpriteLoader::SpriteCollection &sprite, SpriteAllocator &allocator)
{
	Sprite *dest_sprite;
	dest_sprite = allocator.Allocate<Sprite>(sizeof(*dest_sprite));

	const auto &root_sprite = sprite.Root();
	dest_sprite->height = root_sprite.height;
	dest_sprite->width = root_sprite.width;
	dest_sprite->x_offs = root_sprite.x_offs;
	dest_sprite->y_offs = root_sprite.y_offs;

	return dest_sprite;
}
