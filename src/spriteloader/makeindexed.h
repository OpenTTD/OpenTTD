/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file makeindexed.h Base for converting sprites from another source from 32bpp RGBA to indexed 8bpp. */

#ifndef SPRITELOADER_MAKEINDEXED_H
#define SPRITELOADER_MAKEINDEXED_H

#include "spriteloader.hpp"

/** Sprite loader for converting graphics coming from another source. */
class SpriteLoaderMakeIndexed : public SpriteLoader {
	SpriteLoader &baseloader;
public:
	SpriteLoaderMakeIndexed(SpriteLoader &baseloader) : baseloader(baseloader) {}
	uint8_t LoadSprite(SpriteLoader::SpriteCollection &sprite, SpriteFile &file, size_t file_pos, SpriteType sprite_type, bool load_32bpp, uint8_t control_flags, uint8_t &avail_8bpp, uint8_t &avail_32bpp) override;
};

#endif /* SPRITELOADER_MAKEINDEXED_H */
