/* $Id$ */

/** @file grf.hpp */

#ifndef SPRITELOADER_GRF_HPP
#define SPRITELOADER_GRF_HPP

#include "spriteloader.hpp"

class SpriteLoaderGrf : public SpriteLoader {
public:
	/**
	 * Load a sprite from the disk and return a sprite struct which is the same for all loaders.
	 */
	bool LoadSprite(SpriteLoader::Sprite *sprite, uint32 file_pos);
};

#endif /* SPRITELOADER_GRF_HPP */
