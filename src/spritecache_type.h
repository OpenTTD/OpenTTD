/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file spritecache_type.h Types related to the sprite cache. */

#ifndef SPRITECACHE_TYPE_H
#define SPRITECACHE_TYPE_H

#include "core/enum_type.hpp"

/** Data structure describing a sprite. */
struct Sprite {
	uint16_t height; ///< Height of the sprite.
	uint16_t width;  ///< Width of the sprite.
	int16_t x_offs;  ///< Number of pixels to shift the sprite to the right.
	int16_t y_offs;  ///< Number of pixels to shift the sprite downwards.
	std::byte data[]; ///< Sprite data.
};

enum class SpriteCacheCtrlFlag : uint8_t {
	AllowZoomMin1xPal, ///< Allow use of sprite min zoom setting at 1x in palette mode.
	AllowZoomMin1x32bpp, ///< Allow use of sprite min zoom setting at 1x in 32bpp mode.
	AllowZoomMin2xPal, ///< Allow use of sprite min zoom setting at 2x in palette mode.
	AllowZoomMin2x32bpp, ///< Allow use of sprite min zoom setting at 2x in 32bpp mode.
};

using SpriteCacheCtrlFlags = EnumBitSet<SpriteCacheCtrlFlag, uint8_t>;

#endif /* SPRITECACHE_TYPE_H */
