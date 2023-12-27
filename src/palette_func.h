/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file palette_func.h Functions related to palettes. */

#ifndef PALETTE_FUNC_H
#define PALETTE_FUNC_H

#include "core/bitmath_func.hpp"
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

/**
 * Stretch TNumBits to fill 8 bits.
 * The most-significant digits are repeated as least-significant digits so that the full 8 bit range is used, e.g.:
 * 000000 -> 00000000
 * 111100 -> 11110011
 * 111111 -> 11111111
 * @param v Value of TNumBits to stretch.
 * @returns 8 bit stretched value.
 */
template <uint TNumBits>
static inline uint8_t StretchBits(uint8_t v)
{
	return (v << (8 - TNumBits)) | (v >> (8 - (8 - TNumBits) * 2));
}

struct ColoursPacker
{
	Colours &c;

	constexpr ColoursPacker(Colours &c) : c(c) { }

	/*
	 * Constants for the bit packing used by Colours.
	 */
	static constexpr const uint I_START =  0; ///< Packed start of index component
	static constexpr const uint I_SIZE  =  4; ///< Packed size of index component

	static constexpr const uint IS_CUSTOM = 4;

	static constexpr const uint H_START =  7; ///< Packed start of red component
	static constexpr const uint H_SIZE  =  9; ///< Packed size of red component

	static constexpr const uint S_START = 16; ///< Packed start of green component
	static constexpr const uint S_SIZE  =  6; ///< Packed size of green component

	static constexpr const uint V_START = 22; ///< Packed start of blue component
	static constexpr const uint V_SIZE  =  6; ///< Packed size of blue component

	static constexpr const uint C_START = 28; ///< Packed start of contrast component
	static constexpr const uint C_SIZE  =  4; ///< Packed size of contrast component

	/* Colours is considered unused and blank if only the I component is set. */
	inline constexpr bool IsCustom() const { return HasBit(this->c, IS_CUSTOM); }

	inline constexpr uint8_t I() const { return GB(this->c, I_START, I_SIZE); }
	inline constexpr uint16_t H() const { return GB(this->c, H_START, H_SIZE) * HsvColour::HUE_MAX / (1U << 9); }
	inline constexpr uint8_t S() const { return StretchBits<S_SIZE>(GB(this->c, S_START, S_SIZE)); }
	inline constexpr uint8_t V() const { return StretchBits<V_SIZE>(GB(this->c, V_START, V_SIZE)); }
	inline constexpr uint8_t C() const { return StretchBits<C_SIZE>(GB(this->c, C_START, C_SIZE)); }

	inline void SetCustom(bool v) { SB(this->c, IS_CUSTOM, 1, v); }

	inline void SetI(uint8_t v) { SB(this->c, I_START, I_SIZE, v); }
	inline void SetH(uint16_t v) { SB(this->c, H_START, H_SIZE, v * (1U << H_SIZE) / HsvColour::HUE_MAX); }
	inline void SetS(uint8_t v) { SB(this->c, S_START, S_SIZE, v >> (8 - S_SIZE)); }
	inline void SetV(uint8_t v) { SB(this->c, V_START, V_SIZE, v >> (8 - V_SIZE)); }
	inline void SetC(uint8_t v) { SB(this->c, C_START, C_SIZE, v >> (8 - C_SIZE)); }

	inline constexpr HsvColour Hsv() const { return {this->H(), this->S(), this->V()}; }
};

struct TextColourPacker
{
	TextColour &tc;

	constexpr TextColourPacker(TextColour &tc) : tc(tc) { }

	static constexpr const uint R_START = 12; ///< Packed start of red component
	static constexpr const uint R_SIZE  =  6; ///< Packed size of red component

	static constexpr const uint G_START = 18; ///< Packed start of green component
	static constexpr const uint G_SIZE  =  6; ///< Packed size of green component

	static constexpr const uint B_START = 24; ///< Packed start of blue component
	static constexpr const uint B_SIZE  =  6; ///< Packed size of blue component

	inline constexpr uint8_t R() const { return StretchBits<R_SIZE>(GB(this->tc, R_START, R_SIZE)); }
	inline constexpr uint8_t G() const { return StretchBits<G_SIZE>(GB(this->tc, G_START, G_SIZE)); }
	inline constexpr uint8_t B() const { return StretchBits<B_SIZE>(GB(this->tc, B_START, B_SIZE)); }

	inline constexpr void SetR(uint8_t v) { SB(this->tc, R_START, R_SIZE, v >> (8 - R_SIZE)); }
	inline constexpr void SetG(uint8_t v) { SB(this->tc, G_START, G_SIZE, v >> (8 - G_SIZE)); }
	inline constexpr void SetB(uint8_t v) { SB(this->tc, B_START, B_SIZE, v >> (8 - B_SIZE)); }

	inline constexpr RgbaColour Rgba() const { return RgbaColour(this->R(), this->G(), this->B(), UINT8_MAX); }
};

TextColour GetContrastColour(RgbMColour background, uint8_t threshold = 128);

RgbMColour GetColourGradient(Colours colour, uint8_t brightness);
TextColour TextColourGradient(Colours colour, uint8_t brightness);
void SetColourGradient(Colours colour, uint8_t brightness, RgbMColour palette_colour);

RgbaColour GetCompanyColourRGB(Colours colour);
PaletteID CreateCompanyColourRemap(Colours rgb1, Colours rgb2, bool twocc, PaletteID basemap, PaletteID hint);

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
