/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file grf.hpp Base for reading sprites from (New)GRFs. */

#ifndef SPRITELOADER_GRF_HPP
#define SPRITELOADER_GRF_HPP

#include "spriteloader.hpp"

/** Sprite loader for graphics coming from a (New)GRF. */
class SpriteLoaderGrf : public SpriteLoader {
	byte container_ver;
public:
	SpriteLoaderGrf(byte container_ver) : container_ver(container_ver) {}
	uint8 LoadSprite(SpriteLoader::Sprite *sprite, uint8 file_slot, size_t file_pos, SpriteType sprite_type, bool load_32bpp);
};

#endif /* SPRITELOADER_GRF_HPP */
