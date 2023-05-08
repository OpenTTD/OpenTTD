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
#include "zoom_func.h"

#include "safeguards.h"


/**
 * Draws a tile sprite sequence.
 * @param ti The tile to draw on
 * @param dts Sprite and subsprites to draw
 * @param to The transparency bit that toggles drawing of these sprites
 * @param orig_offset Sprite-Offset for original sprites
 * @param newgrf_offset Sprite-Offset for NewGRF defined sprites
 * @param default_palette The default recolour sprite to use (typically company colour)
 * @param child_offset_is_unsigned Whether child sprite offsets are interpreted signed or unsigned
 */
void DrawCommonTileSeq(const TileInfo *ti, const DrawTileSprites *dts, TransparencyOption to, int32_t orig_offset, uint32_t newgrf_offset, PaletteID default_palette, bool child_offset_is_unsigned)
{
	bool parent_sprite_encountered = false;
	const DrawTileSeqStruct *dtss;
	bool skip_childs = false;
	foreach_draw_tile_seq(dtss, dts->seq) {
		SpriteID image = dtss->image.sprite;
		PaletteID pal = dtss->image.pal;

		if (skip_childs) {
			if (!dtss->IsParentSprite()) continue;
			skip_childs = false;
		}

		/* TTD sprite 0 means no sprite */
		if ((GB(image, 0, SPRITE_WIDTH) == 0 && !HasBit(image, SPRITE_MODIFIER_CUSTOM_SPRITE)) ||
				(IsInvisibilitySet(to) && !HasBit(image, SPRITE_MODIFIER_OPAQUE))) {
			skip_childs = dtss->IsParentSprite();
			continue;
		}

		image += (HasBit(image, SPRITE_MODIFIER_CUSTOM_SPRITE) ? newgrf_offset : orig_offset);
		if (HasBit(pal, SPRITE_MODIFIER_CUSTOM_SPRITE)) pal += newgrf_offset;

		pal = SpriteLayoutPaletteTransform(image, pal, default_palette);

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
			int offs_x = child_offset_is_unsigned ? (uint8_t)dtss->delta_x : dtss->delta_x;
			int offs_y = child_offset_is_unsigned ? (uint8_t)dtss->delta_y : dtss->delta_y;
			bool transparent = !HasBit(image, SPRITE_MODIFIER_OPAQUE) && IsTransparencySet(to);
			if (parent_sprite_encountered) {
				AddChildSpriteScreen(image, pal, offs_x, offs_y, transparent);
			} else {
				if (transparent) {
					SetBit(image, PALETTE_MODIFIER_TRANSPARENT);
					pal = PALETTE_TO_TRANSPARENT;
				}
				DrawGroundSprite(image, pal, nullptr, offs_x, offs_y);
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
void DrawCommonTileSeqInGUI(int x, int y, const DrawTileSprites *dts, int32_t orig_offset, uint32_t newgrf_offset, PaletteID default_palette, bool child_offset_is_unsigned)
{
	const DrawTileSeqStruct *dtss;
	Point child_offset = {0, 0};

	bool skip_childs = false;
	foreach_draw_tile_seq(dtss, dts->seq) {
		SpriteID image = dtss->image.sprite;
		PaletteID pal = dtss->image.pal;

		if (skip_childs) {
			if (!dtss->IsParentSprite()) continue;
			skip_childs = false;
		}

		/* TTD sprite 0 means no sprite */
		if (GB(image, 0, SPRITE_WIDTH) == 0 && !HasBit(image, SPRITE_MODIFIER_CUSTOM_SPRITE)) {
			skip_childs = dtss->IsParentSprite();
			continue;
		}

		image += (HasBit(image, SPRITE_MODIFIER_CUSTOM_SPRITE) ? newgrf_offset : orig_offset);
		if (HasBit(pal, SPRITE_MODIFIER_CUSTOM_SPRITE)) pal += newgrf_offset;

		pal = SpriteLayoutPaletteTransform(image, pal, default_palette);

		if (dtss->IsParentSprite()) {
			Point pt = RemapCoords(dtss->delta_x, dtss->delta_y, dtss->delta_z);
			DrawSprite(image, pal, x + UnScaleGUI(pt.x), y + UnScaleGUI(pt.y));

			const Sprite *spr = GetSprite(image & SPRITE_MASK, SpriteType::Normal);
			child_offset.x = UnScaleGUI(pt.x + spr->x_offs);
			child_offset.y = UnScaleGUI(pt.y + spr->y_offs);
		} else {
			int offs_x = child_offset_is_unsigned ? (uint8_t)dtss->delta_x : dtss->delta_x;
			int offs_y = child_offset_is_unsigned ? (uint8_t)dtss->delta_y : dtss->delta_y;
			DrawSprite(image, pal, x + child_offset.x + ScaleSpriteTrad(offs_x), y + child_offset.y + ScaleSpriteTrad(offs_y));
		}
	}
}
