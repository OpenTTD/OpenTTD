/* $Id$ */

/** @file png.hpp Base for reading files from PNG. */

#ifndef SPRITELOADER_PNG_HPP
#define SPRITELOADER_PNG_HPP

#include "spriteloader.hpp"

class SpriteLoaderPNG : public SpriteLoader {
public:
	/**
	 * Load a sprite from the disk and return a sprite struct which is the same for all loaders.
	 */
	bool LoadSprite(SpriteLoader::Sprite *sprite, uint8 file_slot, size_t file_pos);
};

#endif /* SPRITELOADER_PNG_HPP */
