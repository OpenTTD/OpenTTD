/* $Id$ */

/** @file spriteloader.hpp Base for loading sprites. */

#ifndef SPRITELOADER_HPP
#define SPRITELOADER_HPP

#include "../core/alloc_type.hpp"

class SpriteLoader {
public:
	struct CommonPixel {
		uint8 r;  ///< Red-channel
		uint8 g;  ///< Green-channel
		uint8 b;  ///< Blue-channel
		uint8 a;  ///< Alpha-channel
		uint8 m;  ///< Remap-channel
	};

	/**
	 * Structure for passing information from the sprite loader to the blitter.
	 * You can only use this struct once at a time when using AllocateData to
	 * allocate the memory as that will always return the same memory address.
	 * This to prevent thousands of malloc + frees just to load a sprite.
	 */
	struct Sprite {
		uint16 height;                   ///< Height of the sprite
		uint16 width;                    ///< Width of the sprite
		int16 x_offs;                    ///< The x-offset of where the sprite will be drawn
		int16 y_offs;                    ///< The y-offset of where the sprite will be drawn
		SpriteLoader::CommonPixel *data; ///< The sprite itself

		/**
		 * Allocate the sprite data of this sprite.
		 * @param size the minimum size of the data field.
		 */
		void AllocateData(size_t size) { this->data = Sprite::buffer.ZeroAllocate(size); }
	private:
		/** Allocated memory to pass sprite data around */
		static ReusableBuffer<SpriteLoader::CommonPixel> buffer;
	};

	/**
	 * Load a sprite from the disk and return a sprite struct which is the same for all loaders.
	 */
	virtual bool LoadSprite(SpriteLoader::Sprite *sprite, uint8 file_slot, size_t file_pos, SpriteType sprite_type) = 0;

	virtual ~SpriteLoader() { }
};

#endif /* SPRITELOADER_HPP */
