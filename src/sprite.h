/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sprite.h Base for drawing complex sprites. */

#ifndef SPRITE_H
#define SPRITE_H

#include "gfx_type.h"
#include "transparency.h"

#include "table/sprites.h"

#define GENERAL_SPRITE_COLOUR(colour) ((colour) + PALETTE_RECOLOUR_START)
#define COMPANY_SPRITE_COLOUR(owner) (GENERAL_SPRITE_COLOUR(_company_colours[owner]))

/* The following describes bunch of sprites to be drawn together in a single 3D
 * bounding box. Used especially for various multi-sprite buildings (like
 * depots or stations): */

/** A tile child sprite and palette to draw for stations etc, with 3D bounding box */
struct DrawTileSeqStruct {
	int8 delta_x; ///< \c 0x80 is sequence terminator
	int8 delta_y;
	int8 delta_z;
	byte size_x;
	byte size_y;
	byte size_z;
	PalSpriteID image;
};

/** Ground palette sprite of a tile, together with its child sprites */
struct DrawTileSprites {
	PalSpriteID ground;           ///< Palette and sprite for the ground
	const DrawTileSeqStruct *seq; ///< Array of child sprites. Terminated with a terminator entry
};

/**
 * This structure is the same for both Industries and Houses.
 * Buildings here reference a general type of construction
 */
struct DrawBuildingsTileStruct {
	PalSpriteID ground;
	PalSpriteID building;
	byte subtile_x;
	byte subtile_y;
	byte width;
	byte height;
	byte dz;
	byte draw_proc;  // this allows to specify a special drawing procedure.
};

/** Iterate through all DrawTileSeqStructs in DrawTileSprites. */
#define foreach_draw_tile_seq(idx, list) for (idx = list; ((byte) idx->delta_x) != 0x80; idx++)

void DrawCommonTileSeq(const struct TileInfo *ti, const DrawTileSprites *dts, TransparencyOption to, int32 orig_offset, uint32 newgrf_offset, SpriteID default_palette);
void DrawCommonTileSeqInGUI(int x, int y, const DrawTileSprites *dts, int32 orig_offset, uint32 newgrf_offset, SpriteID default_palette);

bool SkipSpriteData(byte type, uint16 num);

/**
 * Applies PALETTE_MODIFIER_TRANSPARENT and PALETTE_MODIFIER_COLOUR to a palette entry of a sprite layout entry
 * @Note for ground sprites use #GroundSpritePaletteTransform
 * @Note Not useable for OTTD internal spritelayouts from table/xxx_land.h as PALETTE_MODIFIER_TRANSPARENT is only set
 *       when to use the default palette.
 *
 * @param image The sprite to draw
 * @param pal The palette from the sprite layout
 * @param default_pal The default recolour sprite to use (typically company colour resp. random industry/house colour)
 * @return The palette to use
 */
static inline SpriteID SpriteLayoutPaletteTransform(SpriteID image, SpriteID pal, SpriteID default_pal)
{
	if (HasBit(image, PALETTE_MODIFIER_TRANSPARENT) || HasBit(image, PALETTE_MODIFIER_COLOUR)) {
		return (pal != 0 ? pal : default_pal);
	} else {
		return PAL_NONE;
	}
}

/**
 * Applies PALETTE_MODIFIER_COLOUR to a palette entry of a ground sprite
 * @Note Not useable for OTTD internal spritelayouts from table/xxx_land.h as PALETTE_MODIFIER_TRANSPARENT is only set
 *       when to use the default palette.
 *
 * @param image The sprite to draw
 * @param pal The palette from the sprite layout
 * @param default_pal The default recolour sprite to use (typically company colour resp. random industry/house colour)
 * @return The palette to use
 */
static inline SpriteID GroundSpritePaletteTransform(SpriteID image, SpriteID pal, SpriteID default_pal)
{
	if (HasBit(image, PALETTE_MODIFIER_COLOUR)) {
		return (pal != 0 ? pal : default_pal);
	} else {
		return PAL_NONE;
	}
}

#endif /* SPRITE_H */
