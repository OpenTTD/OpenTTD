/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sprite.h Base for drawing complex sprites. */

#ifndef SPRITE_H
#define SPRITE_H

#include "core/geometry_type.hpp"
#include "transparency.h"

#include "table/sprites.h"

struct SpriteBounds {
	Coord3D<int8_t> origin; ///< Position of northern corner within tile.
	Coord3D<uint8_t> extent; ///< Size of bounding box.
	Coord3D<int8_t> offset; ///< Relative position of sprite from bounding box.

	constexpr SpriteBounds() = default;
	constexpr SpriteBounds(const Coord3D<int8_t> &origin, const Coord3D<uint8_t> &extent, const Coord3D<int8_t> &offset) :
		origin(origin), extent(extent), offset(offset) {}
};

/* The following describes bunch of sprites to be drawn together in a single 3D
 * bounding box. Used especially for various multi-sprite buildings (like
 * depots or stations): */

/** A tile child sprite and palette to draw for stations etc, with 3D bounding box */
struct DrawTileSeqStruct : SpriteBounds {
	PalSpriteID image;

	constexpr DrawTileSeqStruct() = default;
	constexpr DrawTileSeqStruct(int8_t origin_x, int8_t origin_y, int8_t origin_z, uint8_t extent_x, uint8_t extent_y, uint8_t extent_z, PalSpriteID image) :
		SpriteBounds({origin_x, origin_y, origin_z}, {extent_x, extent_y, extent_z}, {}), image(image) {}

	/** Check whether this is a parent sprite with a boundingbox. */
	inline bool IsParentSprite() const
	{
		return static_cast<uint8_t>(this->origin.z) != 0x80;
	}
};

/**
 * Ground palette sprite of a tile, together with its sprite layout.
 * For static sprite layouts see #DrawTileSpriteSpan.
 * For allocated ones from NewGRF see #NewGRFSpriteLayout.
 */
struct DrawTileSprites {
	PalSpriteID ground{}; ///< Palette and sprite for the ground

	DrawTileSprites(PalSpriteID ground) : ground(ground) {}
	DrawTileSprites() = default;

	virtual ~DrawTileSprites() = default;
	virtual std::span<const DrawTileSeqStruct> GetSequence() const = 0;
};

/**
 * Ground palette sprite of a tile, together with its sprite layout.
 * This struct is used for static sprite layouts in the code.
 * For allocated ones from NewGRF see #NewGRFSpriteLayout.
 */
struct DrawTileSpriteSpan : DrawTileSprites {
	std::span<const DrawTileSeqStruct> seq; ///< Child sprites,

	DrawTileSpriteSpan(PalSpriteID ground, std::span<const DrawTileSeqStruct> seq) : DrawTileSprites(ground), seq(seq) {}
	DrawTileSpriteSpan(PalSpriteID ground) : DrawTileSprites(ground) {};
	DrawTileSpriteSpan() = default;

	std::span<const DrawTileSeqStruct> GetSequence() const override { return this->seq; }
};

/**
 * This structure is the same for both Industries and Houses.
 * Buildings here reference a general type of construction
 */
struct DrawBuildingsTileStruct : SpriteBounds {
	PalSpriteID ground;
	PalSpriteID building;
	uint8_t draw_proc;  // this allows to specify a special drawing procedure.
};

void DrawCommonTileSeq(const struct TileInfo *ti, const DrawTileSprites *dts, TransparencyOption to, int32_t orig_offset, uint32_t newgrf_offset, PaletteID default_palette, bool child_offset_is_unsigned);
void DrawCommonTileSeqInGUI(int x, int y, const DrawTileSprites *dts, int32_t orig_offset, uint32_t newgrf_offset, PaletteID default_palette, bool child_offset_is_unsigned);

/**
 * Draw tile sprite sequence on tile with railroad specifics.
 * @param total_offset Spriteoffset from normal rail to current railtype.
 * @param newgrf_offset Startsprite of the Action1 to use.
 */
inline void DrawRailTileSeq(const struct TileInfo *ti, const DrawTileSprites *dts, TransparencyOption to, int32_t total_offset, uint32_t newgrf_offset, PaletteID default_palette)
{
	DrawCommonTileSeq(ti, dts, to, total_offset, newgrf_offset, default_palette, false);
}

/**
 * Draw tile sprite sequence in GUI with railroad specifics.
 * @param total_offset Spriteoffset from normal rail to current railtype.
 * @param newgrf_offset Startsprite of the Action1 to use.
 */
inline void DrawRailTileSeqInGUI(int x, int y, const DrawTileSprites *dts, int32_t total_offset, uint32_t newgrf_offset, PaletteID default_palette)
{
	DrawCommonTileSeqInGUI(x, y, dts, total_offset, newgrf_offset, default_palette, false);
}

/**
 * Draw TTD sprite sequence on tile.
 */
inline void DrawOrigTileSeq(const struct TileInfo *ti, const DrawTileSprites *dts, TransparencyOption to, PaletteID default_palette)
{
	DrawCommonTileSeq(ti, dts, to, 0, 0, default_palette, false);
}

/**
 * Draw TTD sprite sequence in GUI.
 */
inline void DrawOrigTileSeqInGUI(int x, int y, const DrawTileSprites *dts, PaletteID default_palette)
{
	DrawCommonTileSeqInGUI(x, y, dts, 0, 0, default_palette, false);
}

/**
 * Draw NewGRF industrytile or house sprite layout
 * @param stage Sprite inside the Action1 spritesets to use, i.e. construction stage.
 */
inline void DrawNewGRFTileSeq(const struct TileInfo *ti, const DrawTileSprites *dts, TransparencyOption to, uint32_t stage, PaletteID default_palette)
{
	DrawCommonTileSeq(ti, dts, to, 0, stage, default_palette, true);
}

/**
 * Draw NewGRF object in GUI
 * @param stage Sprite inside the Action1 spritesets to use, i.e. construction stage.
 */
inline void DrawNewGRFTileSeqInGUI(int x, int y, const DrawTileSprites *dts, uint32_t stage, PaletteID default_palette)
{
	DrawCommonTileSeqInGUI(x, y, dts, 0, stage, default_palette, true);
}

/**
 * Applies PALETTE_MODIFIER_TRANSPARENT and PALETTE_MODIFIER_COLOUR to a palette entry of a sprite layout entry
 * @note for ground sprites use #GroundSpritePaletteTransform
 * @note Not usable for OTTD internal spritelayouts from table/xxx_land.h as PALETTE_MODIFIER_TRANSPARENT is only set
 *       when to use the default palette.
 *
 * @param image The sprite to draw
 * @param pal The palette from the sprite layout
 * @param default_pal The default recolour sprite to use (typically company colour resp. random industry/house colour)
 * @return The palette to use
 */
inline PaletteID SpriteLayoutPaletteTransform(SpriteID image, PaletteID pal, PaletteID default_pal)
{
	if (HasBit(image, PALETTE_MODIFIER_TRANSPARENT) || HasBit(image, PALETTE_MODIFIER_COLOUR)) {
		return (pal != 0 ? pal : default_pal);
	} else {
		return PAL_NONE;
	}
}

/**
 * Applies PALETTE_MODIFIER_COLOUR to a palette entry of a ground sprite
 * @note Not usable for OTTD internal spritelayouts from table/xxx_land.h as PALETTE_MODIFIER_TRANSPARENT is only set
 *       when to use the default palette.
 *
 * @param image The sprite to draw
 * @param pal The palette from the sprite layout
 * @param default_pal The default recolour sprite to use (typically company colour resp. random industry/house colour)
 * @return The palette to use
 */
inline PaletteID GroundSpritePaletteTransform(SpriteID image, PaletteID pal, PaletteID default_pal)
{
	if (HasBit(image, PALETTE_MODIFIER_COLOUR)) {
		return (pal != 0 ? pal : default_pal);
	} else {
		return PAL_NONE;
	}
}

/**
 * Get recolour palette for a colour.
 * @param colour Colour.
 * @return Recolour palette.
 */
static inline PaletteID GetColourPalette(Colours colour) { return PALETTE_RECOLOUR_START + colour; }

#endif /* SPRITE_H */
