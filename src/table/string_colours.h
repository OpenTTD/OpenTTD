/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file string_colours.h The colour translation of GRF's strings. */

/** Colour mapping for #TextColour. */
static constexpr PixelColour _string_colourmap[17] = {
	PixelColour{150}, // TextColour::Blue
	PixelColour{ 12}, // TextColour::Silver
	PixelColour{189}, // TextColour::Gold
	PixelColour{184}, // TextColour::Red
	PixelColour{174}, // TextColour::Purple
	PixelColour{ 30}, // TextColour::LightBrown
	PixelColour{195}, // TextColour::Orange
	PixelColour{209}, // TextColour::Green
	PixelColour{ 68}, // TextColour::Yellow
	PixelColour{ 95}, // TextColour::DarkGreen
	PixelColour{ 79}, // TextColour::Cream
	PixelColour{116}, // TextColour::Brown
	PixelColour{ 15}, // TextColour::White
	PixelColour{152}, // TextColour::LightBlue
	PixelColour{  6}, // TextColour::Grey
	PixelColour{133}, // TextColour::DarkBlue
	PixelColour{  1}, // TextColour::Black
};
