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
#include "tile_cmd.h"
#include "viewport_func.h"
#include "landscape.h"
#include "spritecache.h"

#include "table/sprites.h"

/**
 * Draws a tile sprite sequence.
 * @param ti The tile to draw on
 * @param dts Sprite and subsprites to draw
 * @param to The transparancy bit that toggles drawing of these sprites
 * @param orig_offset Sprite-Offset for original sprites
 * @param newgrf_offset Sprite-Offset for NewGRF defined sprites
 * @param default_palette The default recolour sprite to use (typically company colour)
 */
void DrawCommonTileSeq(const TileInfo *ti, const DrawTileSprites *dts, TransparencyOption to, int32 orig_offset, uint32 newgrf_offset, SpriteID default_palette)
{
	const DrawTileSeqStruct *dtss;
	foreach_draw_tile_seq(dtss, dts->seq) {
		SpriteID image = dtss->image.sprite;

		/* Stop drawing sprite sequence once we meet a sprite that doesn't have to be opaque */
		if (IsInvisibilitySet(to) && !HasBit(image, SPRITE_MODIFIER_OPAQUE)) return;

		if (newgrf_offset == 0 || HasBit(image, SPRITE_MODIFIER_USE_OFFSET)) {
			image += orig_offset;
		} else {
			image += newgrf_offset;
		}

		SpriteID pal = SpriteLayoutPaletteTransform(image, dtss->image.pal, default_palette);

		if ((byte)dtss->delta_z != 0x80) {
			AddSortableSpriteToDraw(
				image, pal,
				ti->x + dtss->delta_x, ti->y + dtss->delta_y,
				dtss->size_x, dtss->size_y,
				dtss->size_z, ti->z + dtss->delta_z,
				!HasBit(image, SPRITE_MODIFIER_OPAQUE) && IsTransparencySet(to)
			);
		} else {
			/* For stations and original spritelayouts delta_x and delta_y are signed */
			AddChildSpriteScreen(image, pal, dtss->delta_x, dtss->delta_y, !HasBit(image, SPRITE_MODIFIER_OPAQUE) && IsTransparencySet(to));
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
 */
void DrawCommonTileSeqInGUI(int x, int y, const DrawTileSprites *dts, int32 orig_offset, uint32 newgrf_offset, SpriteID default_palette)
{
	const DrawTileSeqStruct *dtss;
	Point child_offset = {0, 0};

	foreach_draw_tile_seq(dtss, dts->seq) {
		SpriteID image = dtss->image.sprite;
		if (newgrf_offset == 0 || HasBit(image, SPRITE_MODIFIER_USE_OFFSET)) {
			image += orig_offset;
		} else {
			image += newgrf_offset;
		}

		SpriteID pal = SpriteLayoutPaletteTransform(image, dtss->image.pal, default_palette);

		if ((byte)dtss->delta_z != 0x80) {
			Point pt = RemapCoords(dtss->delta_x, dtss->delta_y, dtss->delta_z);
			DrawSprite(image, pal, x + pt.x, y + pt.y);

			const Sprite *spr = GetSprite(image & SPRITE_MASK, ST_NORMAL);
			child_offset.x = pt.x + spr->x_offs;
			child_offset.y = pt.y + spr->y_offs;
		} else {
			/* For stations and original spritelayouts delta_x and delta_y are signed */
			DrawSprite(image, pal, x + child_offset.x + dtss->delta_x, y + child_offset.y + dtss->delta_y);
		}
	}
}
