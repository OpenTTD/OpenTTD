/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 8bpp_debug.cpp Implementation of 8 bpp debug blitter. */

#include "../stdafx.h"
#include "../zoom_func.h"
#include "../core/random_func.hpp"
#include "8bpp_debug.hpp"

static FBlitter_8bppDebug iFBlitter_8bppDebug;

void Blitter_8bppDebug::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
	const uint8 *src, *src_line;
	uint8 *dst, *dst_line;

	/* Find where to start reading in the source sprite */
	src_line = (const uint8 *)bp->sprite + (bp->skip_top * bp->sprite_width + bp->skip_left) * ScaleByZoom(1, zoom);
	dst_line = (uint8 *)bp->dst + bp->top * bp->pitch + bp->left;

	for (int y = 0; y < bp->height; y++) {
		dst = dst_line;
		dst_line += bp->pitch;

		src = src_line;
		src_line += bp->sprite_width * ScaleByZoom(1, zoom);

		for (int x = 0; x < bp->width; x++) {
			if (*src != 0) *dst = *src;
			dst++;
			src += ScaleByZoom(1, zoom);
		}
		assert(src <= src_line);
	}
}

Sprite *Blitter_8bppDebug::Encode(SpriteLoader::Sprite *sprite, AllocatorProc *allocator)
{
	Sprite *dest_sprite;
	dest_sprite = (Sprite *)allocator(sizeof(*dest_sprite) + sprite->height * sprite->width);

	dest_sprite->height = sprite->height;
	dest_sprite->width  = sprite->width;
	dest_sprite->x_offs = sprite->x_offs;
	dest_sprite->y_offs = sprite->y_offs;

	/* Write a random colour as sprite; this makes debugging really easy */
	uint colour = InteractiveRandom() % 150 + 2;
	for (int i = 0; i < sprite->height * sprite->width; i++) {
		dest_sprite->data[i] = (sprite->data[i].m == 0) ? 0 : colour;
	}

	return dest_sprite;
}
