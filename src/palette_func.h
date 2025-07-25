/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file palette_func.h Functions related to palettes. */

#ifndef PALETTE_FUNC_H
#define PALETTE_FUNC_H

#include "core/enum_type.hpp"
#include "gfx_type.h"
#include "strings_type.h"
#include "string_type.h"

extern Palette _cur_palette; ///< Current palette

bool CopyPalette(Palette &local_palette, bool force_copy = false);
void GfxInitPalettes();

uint8_t GetNearestColourIndex(uint8_t r, uint8_t g, uint8_t b);
uint8_t GetNearestColourReshadeIndex(uint8_t b);

inline uint8_t GetNearestColourIndex(const Colour colour)
{
	return GetNearestColourIndex(colour.r, colour.g, colour.b);
}

static constexpr int DEFAULT_BRIGHTNESS = 128;

Colour ReallyAdjustBrightness(Colour colour, int brightness);

static inline Colour AdjustBrightness(Colour colour, uint8_t brightness)
{
	/* Shortcut for normal brightness */
	if (brightness == DEFAULT_BRIGHTNESS) return colour;

	return ReallyAdjustBrightness(colour, brightness);
}

/**
 * Get the brightness of a colour.
 * This uses the maximum value of R, G or B channel, instead of perceptual brightness.
 * @param colour Colour to get the brightness of.
 * @returns Brightness of colour.
 */
static inline uint8_t GetColourBrightness(Colour colour)
{
	uint8_t rgb_max = std::max(colour.r, std::max(colour.g, colour.b));

	/* Black pixel (8bpp or old 32bpp image), so use default value */
	if (rgb_max == 0) rgb_max = DEFAULT_BRIGHTNESS;

	return rgb_max;
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

TextColour GetContrastColour(PixelColour background, uint8_t threshold = 128);

enum ColourShade : uint8_t {
	SHADE_BEGIN = 0,
	SHADE_DARKEST = SHADE_BEGIN,
	SHADE_DARKER,
	SHADE_DARK,
	SHADE_NORMAL,
	SHADE_LIGHT,
	SHADE_LIGHTER,
	SHADE_LIGHTEST,
	SHADE_LIGHTEREST,
	SHADE_END,
};
DECLARE_INCREMENT_DECREMENT_OPERATORS(ColourShade)

PixelColour GetColourGradient(Colours colour, ColourShade shade);
void SetColourGradient(Colours colour, ColourShade shade, PixelColour palette_colour);

/**
 * Return the colour for a particular greyscale level.
 * @param level Intensity, 0 = black, 15 = white
 * @return colour
 */
inline constexpr PixelColour GREY_SCALE(uint8_t level) { return PixelColour{level}; }

static constexpr PixelColour PC_BLACK              {GREY_SCALE(1)};  ///< Black palette colour.
static constexpr PixelColour PC_DARK_GREY          {GREY_SCALE(6)};  ///< Dark grey palette colour.
static constexpr PixelColour PC_GREY               {GREY_SCALE(10)}; ///< Grey palette colour.
static constexpr PixelColour PC_WHITE              {GREY_SCALE(15)}; ///< White palette colour.

static constexpr PixelColour PC_VERY_DARK_RED      {0xB2};           ///< Almost-black red palette colour.
static constexpr PixelColour PC_DARK_RED           {0xB4};           ///< Dark red palette colour.
static constexpr PixelColour PC_RED                {0xB8};           ///< Red palette colour.

static constexpr PixelColour PC_VERY_DARK_BROWN    {0x56};           ///< Almost-black brown palette colour.

static constexpr PixelColour PC_ORANGE             {0xC2};           ///< Orange palette colour.

static constexpr PixelColour PC_YELLOW             {0xBF};           ///< Yellow palette colour.
static constexpr PixelColour PC_LIGHT_YELLOW       {0x44};           ///< Light yellow palette colour.
static constexpr PixelColour PC_VERY_LIGHT_YELLOW  {0x45};           ///< Almost-white yellow palette colour.

static constexpr PixelColour PC_GREEN              {0xD0};           ///< Green palette colour.

static constexpr PixelColour PC_VERY_DARK_BLUE     {0x9A};           ///< Almost-black blue palette colour.
static constexpr PixelColour PC_DARK_BLUE          {0x9D};           ///< Dark blue palette colour.
static constexpr PixelColour PC_LIGHT_BLUE         {0x98};           ///< Light blue palette colour.

static constexpr PixelColour PC_ROUGH_LAND         {0x52};           ///< Dark green palette colour for rough land.
static constexpr PixelColour PC_GRASS_LAND         {0x54};           ///< Dark green palette colour for grass land.
static constexpr PixelColour PC_BARE_LAND          {0x37};           ///< Brown palette colour for bare land.
static constexpr PixelColour PC_RAINFOREST         {0x5C};           ///< Pale green palette colour for rainforest.
static constexpr PixelColour PC_FIELDS             {0x25};           ///< Light brown palette colour for fields.
static constexpr PixelColour PC_TREES              {0x57};           ///< Green palette colour for trees.
static constexpr PixelColour PC_WATER              {0xC9};           ///< Dark blue palette colour for water.

/**
 * Stretch TNumBits to fill 8 bits.
 * The most-significant digits are repeated as least-significant digits so that the full 8 bit range is used, e.g.:
 * 000000 -> 00000000, 111100 -> 11110011, 111111 -> 11111111
 * @param v Value of TNumBits to stretch.
 * @returns 8 bit stretched value.
 */
template <uint TNumBits>
inline constexpr uint8_t StretchBits(uint8_t v)
{
	return (v << (8 - TNumBits)) | (v >> (8 - (8 - TNumBits) * 2));
}

struct TextColourPacker
{
	TextColour &tc;

	constexpr TextColourPacker(TextColour &tc) : tc(tc) { }

	static constexpr uint8_t R_START = 12; ///< Packed start of red component
	static constexpr uint8_t R_SIZE  =  6; ///< Packed size of red component

	static constexpr uint8_t G_START = 18; ///< Packed start of green component
	static constexpr uint8_t G_SIZE  =  6; ///< Packed size of green component

	static constexpr uint8_t B_START = 24; ///< Packed start of blue component
	static constexpr uint8_t B_SIZE  =  6; ///< Packed size of blue component

	inline constexpr uint8_t GetR() const { return StretchBits<R_SIZE>(GB(this->tc, R_START, R_SIZE)); }
	inline constexpr uint8_t GetG() const { return StretchBits<G_SIZE>(GB(this->tc, G_START, G_SIZE)); }
	inline constexpr uint8_t GetB() const { return StretchBits<B_SIZE>(GB(this->tc, B_START, B_SIZE)); }

	inline constexpr void SetR(uint8_t v) { SB(this->tc, R_START, R_SIZE, v >> (8 - R_SIZE)); }
	inline constexpr void SetG(uint8_t v) { SB(this->tc, G_START, G_SIZE, v >> (8 - G_SIZE)); }
	inline constexpr void SetB(uint8_t v) { SB(this->tc, B_START, B_SIZE, v >> (8 - B_SIZE)); }

	inline constexpr Colour ToColour() const { return Colour(this->GetR(), this->GetG(), this->GetB()); }
};

#endif /* PALETTE_FUNC_H */
