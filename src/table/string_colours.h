/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file string_colours.h The colour translation of GRF's strings. */

/** Colour mapping for #TextColour. */
static constexpr PixelColour _string_colourmap[17] = {
	PixelColour{150}, // TC_BLUE
	PixelColour{ 12}, // TC_SILVER
	PixelColour{189}, // TC_GOLD
	PixelColour{184}, // TC_RED
	PixelColour{174}, // TC_PURPLE
	PixelColour{ 30}, // TC_LIGHT_BROWN
	PixelColour{195}, // TC_ORANGE
	PixelColour{209}, // TC_GREEN
	PixelColour{ 68}, // TC_YELLOW
	PixelColour{ 95}, // TC_DARK_GREEN
	PixelColour{ 79}, // TC_CREAM
	PixelColour{116}, // TC_BROWN
	PixelColour{ 15}, // TC_WHITE
	PixelColour{152}, // TC_LIGHT_BLUE
	PixelColour{  6}, // TC_GREY
	PixelColour{133}, // TC_DARK_BLUE
	PixelColour{  1}, // TC_BLACK
};
