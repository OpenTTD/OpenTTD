/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file palette_func.h Functions related to palettes. */

#ifndef PALETTE_FUNC_H
#define PALETTE_FUNC_H

#include "gfx_type.h"
#include "strings_type.h"
#include "string_type.h"

extern Palette _cur_palette; ///< Current palette

bool CopyPalette(Palette &local_palette, bool force_copy = false);
void GfxInitPalettes();

uint8_t GetNearestColourIndex(uint8_t r, uint8_t g, uint8_t b);

inline uint8_t GetNearestColourIndex(const RgbaColour colour)
{
	return GetNearestColourIndex(colour.r, colour.g, colour.b);
}

/**
 * Checks if a Colours value is valid.
 *
 * @param colours The value to check
 * @return true if the given value is a valid Colours.
 */
inline bool IsValidColours(Colours colours)
{
	return colours < COLOUR_END;
}

HsvColour ConvertRgbToHsv(RgbaColour rgb);
RgbaColour ConvertHsvToRgb(HsvColour hsv);
HsvColour AdjustHsvColourBrightness(HsvColour hsv, int amt);

TextColour GetContrastColour(RgbMColour background, uint8_t threshold = 128);

/**
 * All 16 colour gradients
 * 8 colours per gradient from darkest (0) to lightest (7)
 */
RgbMColour GetColourGradient(Colours colour, uint8_t brightness);
void SetColourGradient(Colours colour, uint8_t brightness, RgbMColour palette_colour);

/**
 * Return the colour for a particular greyscale level.
 * @param level Intensity, 0 = black, 15 = white
 * @return colour
 */
#define GREY_SCALE(level) (level)

static constexpr const RgbMColour PC_BLACK              = GREY_SCALE(1);  ///< Black palette colour.
static constexpr const RgbMColour PC_DARK_GREY          = GREY_SCALE(6);  ///< Dark grey palette colour.
static constexpr const RgbMColour PC_GREY               = GREY_SCALE(10); ///< Grey palette colour.
static constexpr const RgbMColour PC_WHITE              = GREY_SCALE(15); ///< White palette colour.

static constexpr const RgbMColour PC_VERY_DARK_RED      = 0xB2;           ///< Almost-black red palette colour.
static constexpr const RgbMColour PC_DARK_RED           = 0xB4;           ///< Dark red palette colour.
static constexpr const RgbMColour PC_RED                = 0xB8;           ///< Red palette colour.

static constexpr const RgbMColour PC_VERY_DARK_BROWN    = 0x56;           ///< Almost-black brown palette colour.

static constexpr const RgbMColour PC_ORANGE             = 0xC2;           ///< Orange palette colour.

static constexpr const RgbMColour PC_YELLOW             = 0xBF;           ///< Yellow palette colour.
static constexpr const RgbMColour PC_LIGHT_YELLOW       = 0x44;           ///< Light yellow palette colour.
static constexpr const RgbMColour PC_VERY_LIGHT_YELLOW  = 0x45;           ///< Almost-white yellow palette colour.

static constexpr const RgbMColour PC_GREEN              = 0xD0;           ///< Green palette colour.

static constexpr const RgbMColour PC_VERY_DARK_BLUE     = 0x9A;           ///< Almost-black blue palette colour.
static constexpr const RgbMColour PC_DARK_BLUE          = 0x9D;           ///< Dark blue palette colour.
static constexpr const RgbMColour PC_LIGHT_BLUE         = 0x98;           ///< Light blue palette colour.

static constexpr const RgbMColour PC_ROUGH_LAND         = 0x52;           ///< Dark green palette colour for rough land.
static constexpr const RgbMColour PC_GRASS_LAND         = 0x54;           ///< Dark green palette colour for grass land.
static constexpr const RgbMColour PC_BARE_LAND          = 0x37;           ///< Brown palette colour for bare land.
static constexpr const RgbMColour PC_RAINFOREST         = 0x5C;           ///< Pale green palette colour for rainforest.
static constexpr const RgbMColour PC_FIELDS             = 0x25;           ///< Light brown palette colour for fields.
static constexpr const RgbMColour PC_TREES              = 0x57;           ///< Green palette colour for trees.
static constexpr const RgbMColour PC_WATER              = 0xC9;           ///< Dark blue palette colour for water.

#endif /* PALETTE_FUNC_H */
