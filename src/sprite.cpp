/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sprite.cpp Handling of sprites */

#include "stdafx.h"
#include "sprite.h"
#include "viewport_func.h"
#include "landscape.h"
#include "spritecache.h"
#include "core/alloc_func.hpp"
#include "core/mem_func.hpp"


/**
 * Draws a tile sprite sequence.
 * @param ti The tile to draw on
 * @param dts Sprite and subsprites to draw
 * @param to The transparancy bit that toggles drawing of these sprites
 * @param orig_offset Sprite-Offset for original sprites
 * @param newgrf_offset Sprite-Offset for NewGRF defined sprites
 * @param default_palette The default recolour sprite to use (typically company colour)
 * @param child_offset_is_unsigned Whether child sprite offsets are interpreted signed or unsigned
 */
void DrawCommonTileSeq(const TileInfo *ti, const DrawTileSprites *dts, TransparencyOption to, int32 orig_offset, uint32 newgrf_offset, PaletteID default_palette, bool child_offset_is_unsigned)
{
	bool parent_sprite_encountered = false;
	const DrawTileSeqStruct *dtss;
	foreach_draw_tile_seq(dtss, dts->seq) {
		SpriteID image = dtss->image.sprite;

		/* TTD sprite 0 means no sprite */
		if (GB(image, 0, SPRITE_WIDTH) == 0 && !HasBit(image, SPRITE_MODIFIER_CUSTOM_SPRITE)) continue;

		/* Stop drawing sprite sequence once we meet a sprite that doesn't have to be opaque */
		if (IsInvisibilitySet(to) && !HasBit(image, SPRITE_MODIFIER_OPAQUE)) return;

		image += (HasBit(image, SPRITE_MODIFIER_CUSTOM_SPRITE) ? newgrf_offset : orig_offset);

		PaletteID pal = SpriteLayoutPaletteTransform(image, dtss->image.pal, default_palette);

		if (dtss->IsParentSprite()) {
			parent_sprite_encountered = true;
			AddSortableSpriteToDraw(
				image, pal,
				ti->x + dtss->delta_x, ti->y + dtss->delta_y,
				dtss->size_x, dtss->size_y,
				dtss->size_z, ti->z + dtss->delta_z,
				!HasBit(image, SPRITE_MODIFIER_OPAQUE) && IsTransparencySet(to)
			);
		} else {
			int offs_x = child_offset_is_unsigned ? (uint8)dtss->delta_x : dtss->delta_x;
			int offs_y = child_offset_is_unsigned ? (uint8)dtss->delta_y : dtss->delta_y;
			bool transparent = !HasBit(image, SPRITE_MODIFIER_OPAQUE) && IsTransparencySet(to);
			if (parent_sprite_encountered) {
				AddChildSpriteScreen(image, pal, offs_x, offs_y, transparent);
			} else {
				if (transparent) {
					SetBit(image, PALETTE_MODIFIER_TRANSPARENT);
					pal = PALETTE_TO_TRANSPARENT;
				}
				DrawGroundSprite(image, pal, NULL, offs_x, offs_y);
			}
		}
	}
}

/**
 * Draws a tile sprite sequence in the GUI
 * @param x X position to draw to
 * @param y Y position to draw to
 * @param dts Sprite and subsprites to draw
 * @param orig_offset Sprite-Offset for original sprites
 * @param newgrf_offset Sprite-Offset for NewGRF defined sprites
 * @param default_palette The default recolour sprite to use (typically company colour)
 * @param child_offset_is_unsigned Whether child sprite offsets are interpreted signed or unsigned
 */
void DrawCommonTileSeqInGUI(int x, int y, const DrawTileSprites *dts, int32 orig_offset, uint32 newgrf_offset, PaletteID default_palette, bool child_offset_is_unsigned)
{
	const DrawTileSeqStruct *dtss;
	Point child_offset = {0, 0};

	foreach_draw_tile_seq(dtss, dts->seq) {
		SpriteID image = dtss->image.sprite;

		/* TTD sprite 0 means no sprite */
		if (GB(image, 0, SPRITE_WIDTH) == 0 && !HasBit(image, SPRITE_MODIFIER_CUSTOM_SPRITE)) continue;

		image += (HasBit(image, SPRITE_MODIFIER_CUSTOM_SPRITE) ? newgrf_offset : orig_offset);

		PaletteID pal = SpriteLayoutPaletteTransform(image, dtss->image.pal, default_palette);

		if (dtss->IsParentSprite()) {
			Point pt = RemapCoords(dtss->delta_x, dtss->delta_y, dtss->delta_z);
			DrawSprite(image, pal, x + pt.x, y + pt.y);

			const Sprite *spr = GetSprite(image & SPRITE_MASK, ST_NORMAL);
			child_offset.x = pt.x + spr->x_offs;
			child_offset.y = pt.y + spr->y_offs;
		} else {
			int offs_x = child_offset_is_unsigned ? (uint8)dtss->delta_x : dtss->delta_x;
			int offs_y = child_offset_is_unsigned ? (uint8)dtss->delta_y : dtss->delta_y;
			DrawSprite(image, pal, x + child_offset.x + offs_x, y + child_offset.y + offs_y);
		}
	}
}

/** Create a copy of an existing DrawTileSeqStruct array. */
const DrawTileSeqStruct *CopyDrawTileSeqStruct(const DrawTileSeqStruct *dtss)
{
	const DrawTileSeqStruct *element;

	size_t count = 1; // 1 for the terminator
	foreach_draw_tile_seq(element, dtss) count++;

	DrawTileSeqStruct *copy = MallocT<DrawTileSeqStruct>(count);
	MemCpyT(copy, dtss, count);

	return copy;
}
