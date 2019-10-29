/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file palettes.h The colour translation of the GRF palettes. */

#include "../core/endian_type.hpp"

#define M(r, g, b) Colour(r, g, b)

/** Colour palette (DOS) */
static const Palette _palette = {
	{
		/* transparent */
		Colour(0, 0, 0, 0),
		/* grey scale */
		                  M( 16,  16,  16), M( 32,  32,  32), M( 48,  48,  48),
		M( 65,  64,  65), M( 82,  80,  82), M( 98, 101,  98), M(115, 117, 115),
		/* regular colours */
		M(131, 133, 131), M(148, 149, 148), M(168, 168, 168), M(184, 184, 184),
		M(200, 200, 200), M(216, 216, 216), M(232, 232, 232), M(252, 252, 252),
		M( 52,  60,  72), M( 68,  76,  92), M( 88,  96, 112), M(108, 116, 132),
		M(132, 140, 152), M(156, 160, 172), M(176, 184, 196), M(204, 208, 220),
		M( 48,  44,   4), M( 64,  60,  12), M( 80,  76,  20), M( 96,  92,  28),
		M(120, 120,  64), M(148, 148, 100), M(176, 176, 132), M(204, 204, 168),
		M( 72,  44,   4), M( 88,  60,  20), M(104,  80,  44), M(124, 104,  72),
		M(152, 132,  92), M(184, 160, 120), M(212, 188, 148), M(244, 220, 176),
		M( 64,   0,   4), M( 88,   4,  16), M(112,  16,  32), M(136,  32,  52),
		M(160,  56,  76), M(188,  84, 108), M(204, 104, 124), M(220, 132, 144),
		M(236, 156, 164), M(252, 188, 192), M(252, 212,   0), M(252, 232,  60),
		M(252, 248, 128), M( 76,  40,   0), M( 96,  60,   8), M(116,  88,  28),
		M(136, 116,  56), M(156, 136,  80), M(176, 156, 108), M(196, 180, 136),
		M( 68,  24,   0), M( 96,  44,   4), M(128,  68,   8), M(156,  96,  16),
		M(184, 120,  24), M(212, 156,  32), M(232, 184,  16), M(252, 212,   0),
		M(252, 248, 128), M(252, 252, 192), M( 32,   4,   0), M( 64,  20,   8),
		M( 84,  28,  16), M(108,  44,  28), M(128,  56,  40), M(148,  72,  56),
		M(168,  92,  76), M(184, 108,  88), M(196, 128, 108), M(212, 148, 128),
		M(  8,  52,   0), M( 16,  64,   0), M( 32,  80,   4), M( 48,  96,   4),
		M( 64, 112,  12), M( 84, 132,  20), M(104, 148,  28), M(128, 168,  44),
		M( 28,  52,  24), M( 44,  68,  32), M( 60,  88,  48), M( 80, 104,  60),
		M(104, 124,  76), M(128, 148,  92), M(152, 176, 108), M(180, 204, 124),
		M( 16,  52,  24), M( 32,  72,  44), M( 56,  96,  72), M( 76, 116,  88),
		M( 96, 136, 108), M(120, 164, 136), M(152, 192, 168), M(184, 220, 200),
		M( 32,  24,   0), M( 56,  28,   0), M( 72,  40,   4), M( 88,  52,  12),
		M(104,  64,  24), M(124,  84,  44), M(140, 108,  64), M(160, 128,  88),
		M( 76,  40,  16), M( 96,  52,  24), M(116,  68,  40), M(136,  84,  56),
		M(164,  96,  64), M(184, 112,  80), M(204, 128,  96), M(212, 148, 112),
		M(224, 168, 128), M(236, 188, 148), M( 80,  28,   4), M(100,  40,  20),
		M(120,  56,  40), M(140,  76,  64), M(160, 100,  96), M(184, 136, 136),
		M( 36,  40,  68), M( 48,  52,  84), M( 64,  64, 100), M( 80,  80, 116),
		M(100, 100, 136), M(132, 132, 164), M(172, 172, 192), M(212, 212, 224),
		M( 40,  20, 112), M( 64,  44, 144), M( 88,  64, 172), M(104,  76, 196),
		M(120,  88, 224), M(140, 104, 252), M(160, 136, 252), M(188, 168, 252),
		M(  0,  24, 108), M(  0,  36, 132), M(  0,  52, 160), M(  0,  72, 184),
		M(  0,  96, 212), M( 24, 120, 220), M( 56, 144, 232), M( 88, 168, 240),
		M(128, 196, 252), M(188, 224, 252), M( 16,  64,  96), M( 24,  80, 108),
		M( 40,  96, 120), M( 52, 112, 132), M( 80, 140, 160), M(116, 172, 192),
		M(156, 204, 220), M(204, 240, 252), M(172,  52,  52), M(212,  52,  52),
		M(252,  52,  52), M(252, 100,  88), M(252, 144, 124), M(252, 184, 160),
		M(252, 216, 200), M(252, 244, 236), M( 72,  20, 112), M( 92,  44, 140),
		M(112,  68, 168), M(140, 100, 196), M(168, 136, 224), M(204, 180, 252),
		M(204, 180, 252), M(232, 208, 252), M( 60,   0,   0), M( 92,   0,   0),
		M(128,   0,   0), M(160,   0,   0), M(196,   0,   0), M(224,   0,   0),
		M(252,   0,   0), M(252,  80,   0), M(252, 108,   0), M(252, 136,   0),
		M(252, 164,   0), M(252, 192,   0), M(252, 220,   0), M(252, 252,   0),
		M(204, 136,   8), M(228, 144,   4), M(252, 156,   0), M(252, 176,  48),
		M(252, 196, 100), M(252, 216, 152), M(  8,  24,  88), M( 12,  36, 104),
		M( 20,  52, 124), M( 28,  68, 140), M( 40,  92, 164), M( 56, 120, 188),
		M( 72, 152, 216), M(100, 172, 224), M( 92, 156,  52), M(108, 176,  64),
		M(124, 200,  76), M(144, 224,  92), M(224, 244, 252), M(204, 240, 252),
		M(180, 220, 236), M(132, 188, 216), M( 88, 152, 172),
		/* unused pink */
		                                                      M(212,   0, 212),
		M(212,   0, 212), M(212,   0, 212), M(212,   0, 212), M(212,   0, 212),
		M(212,   0, 212), M(212,   0, 212), M(212,   0, 212), M(212,   0, 212),
		M(212,   0, 212), M(212,   0, 212), M(212,   0, 212),
		/* Palette animated colours (filled with data from #ExtraPaletteValues) */
		                                                      M(  0,   0,   0),
		M(  0,   0,   0), M(  0,   0,   0), M(  0,   0,   0), M(  0,   0,   0),
		M(  0,   0,   0), M(  0,   0,   0), M(  0,   0,   0), M(  0,   0,   0),
		M(  0,   0,   0), M(  0,   0,   0), M(  0,   0,   0), M(  0,   0,   0),
		M(  0,   0,   0), M(  0,   0,   0), M(  0,   0,   0), M(  0,   0,   0),
		M(  0,   0,   0), M(  0,   0,   0), M(  0,   0,   0), M(  0,   0,   0),
		M(  0,   0,   0), M(  0,   0,   0), M(  0,   0,   0), M(  0,   0,   0),
		M(  0,   0,   0), M(  0,   0,   0), M(  0,   0,   0),
		/* pure white */
		                                                      M(252, 252, 252)
	},
	0,  // First dirty
	256 // Dirty count
};

/** Description of the length of the palette cycle animations */
static const uint EPV_CYCLES_DARK_WATER    =  5; ///< length of the dark blue water animation
static const uint EPV_CYCLES_LIGHTHOUSE    =  4; ///< length of the lighthouse/stadium animation
static const uint EPV_CYCLES_OIL_REFINERY  =  7; ///< length of the oil refinery's fire animation
static const uint EPV_CYCLES_FIZZY_DRINK   =  5; ///< length of the fizzy drinks animation
static const uint EPV_CYCLES_GLITTER_WATER = 15; ///< length of the glittery water animation

/** Description of tables for the palette animation */
struct ExtraPaletteValues {
	Colour dark_water[EPV_CYCLES_DARK_WATER];               ///< dark blue water
	Colour dark_water_toyland[EPV_CYCLES_DARK_WATER];       ///< dark blue water Toyland
	Colour lighthouse[EPV_CYCLES_LIGHTHOUSE];               ///< lighthouse & stadium
	Colour oil_refinery[EPV_CYCLES_OIL_REFINERY];           ///< oil refinery
	Colour fizzy_drink[EPV_CYCLES_FIZZY_DRINK];             ///< fizzy drinks
	Colour glitter_water[EPV_CYCLES_GLITTER_WATER];         ///< glittery water
	Colour glitter_water_toyland[EPV_CYCLES_GLITTER_WATER]; ///< glittery water Toyland
};

/** Actual palette animation tables */
static const ExtraPaletteValues _extra_palette_values = {
	/* dark blue water */
	{ M( 32,  68, 112), M( 36,  72, 116), M( 40,  76, 120), M( 44,  80, 124),
	  M( 48,  84, 128) },

	/* dark blue water Toyland */
	{ M( 28, 108, 124), M( 32, 112, 128), M( 36, 116, 132), M( 40, 120, 136),
	  M( 44, 124, 140) },

	/* lighthouse & stadium */
	{ M(240, 208,   0), M(  0,   0,   0), M(  0,   0,   0), M(  0,   0,   0) },

	/* oil refinery */
	{ M(252,  60,   0), M(252,  84,   0), M(252, 108,   0), M(252, 124,   0),
	  M(252, 148,   0), M(252, 172,   0), M(252, 196,   0) },

	/* fizzy drinks */
	{ M( 76,  24,   8), M(108,  44,  24), M(144,  72,  52), M(176, 108,  84),
	  M(212, 148, 128) },

	/* glittery water */
	{ M(216, 244, 252), M(172, 208, 224), M(132, 172, 196), M(100, 132, 168),
	  M( 72, 100, 144), M( 72, 100, 144), M( 72, 100, 144), M( 72, 100, 144),
	  M( 72, 100, 144), M( 72, 100, 144), M( 72, 100, 144), M( 72, 100, 144),
	  M(100, 132, 168), M(132, 172, 196), M(172, 208, 224) },

	/* glittery water Toyland */
	{ M(216, 244, 252), M(180, 220, 232), M(148, 200, 216), M(116, 180, 196),
	  M( 92, 164, 184), M( 92, 164, 184), M( 92, 164, 184), M( 92, 164, 184),
	  M( 92, 164, 184), M( 92, 164, 184), M( 92, 164, 184), M( 92, 164, 184),
	  M(116, 180, 196), M(148, 200, 216), M(180, 220, 232) }
};
#undef M
