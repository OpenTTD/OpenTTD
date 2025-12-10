/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file 8bpp_optimized.cpp Implementation of the optimized 8 bpp blitter. */

#include "../stdafx.h"
#include "../zoom_func.h"
#include "../settings_type.h"
#include "8bpp_optimized.hpp"

#include "../safeguards.h"

/** Instantiation of the 8bpp optimised blitter factory. */
static FBlitter_8bppOptimized iFBlitter_8bppOptimized;

void Blitter_8bppOptimized::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
	/* Find the offset of this zoom-level */
	const SpriteData *sprite_src = (const SpriteData *)bp->sprite;
	uint offset = sprite_src->offset[zoom];

	/* Find where to start reading in the source sprite */
	const uint8_t *src = sprite_src->data + offset;
	uint8_t *dst_line = (uint8_t *)bp->dst + bp->top * bp->pitch + bp->left;

	/* Skip over the top lines in the source image */
	for (int y = 0; y < bp->skip_top; y++) {
		for (;;) {
			uint trans = *src++;
			uint pixels = *src++;
			if (trans == 0 && pixels == 0) break;
			src += pixels;
		}
	}

	const uint8_t *src_next = src;

	for (int y = 0; y < bp->height; y++) {
		uint8_t *dst = dst_line;
		dst_line += bp->pitch;

		uint skip_left = bp->skip_left;
		int width = bp->width;

		for (;;) {
			src = src_next;
			uint trans = *src++;
			uint pixels = *src++;
			src_next = src + pixels;
			if (trans == 0 && pixels == 0) break;
			if (width <= 0) continue;

			if (skip_left != 0) {
				if (skip_left < trans) {
					trans -= skip_left;
					skip_left = 0;
				} else {
					skip_left -= trans;
					trans = 0;
				}
				if (skip_left < pixels) {
					src += skip_left;
					pixels -= skip_left;
					skip_left = 0;
				} else {
					src += pixels;
					skip_left -= pixels;
					pixels = 0;
				}
			}
			if (skip_left != 0) continue;

			/* Skip transparent pixels */
			dst += trans;
			width -= trans;
			if (width <= 0 || pixels == 0) continue;
			pixels = std::min<uint>(pixels, width);
			width -= pixels;

			switch (mode) {
				case BlitterMode::ColourRemap:
				case BlitterMode::CrashRemap: {
					const uint8_t *remap = bp->remap;
					do {
						uint m = remap[*src];
						if (m != 0) *dst = m;
						dst++; src++;
					} while (--pixels != 0);
					break;
				}

				case BlitterMode::BlackRemap:
					std::fill_n(dst, pixels, 0);
					dst += pixels;
					break;

				case BlitterMode::Transparent:
				case BlitterMode::TransparentRemap: {
					const uint8_t *remap = bp->remap;
					src += pixels;
					do {
						*dst = remap[*dst];
						dst++;
					} while (--pixels != 0);
					break;
				}

				default:
					std::copy_n(src, pixels, dst);
					dst += pixels; src += pixels;
					break;
			}
		}
	}
}

Sprite *Blitter_8bppOptimized::Encode(SpriteType sprite_type, const SpriteLoader::SpriteCollection &sprite, SpriteAllocator &allocator)
{
	/* Make memory for all zoom-levels */
	uint memory = sizeof(SpriteData);

	ZoomLevel zoom_min;
	ZoomLevel zoom_max;

	if (sprite_type == SpriteType::Font) {
		zoom_min = ZoomLevel::Min;
		zoom_max = ZoomLevel::Min;
	} else {
		zoom_min = _settings_client.gui.zoom_min;
		zoom_max = _settings_client.gui.zoom_max;
		if (zoom_max == zoom_min) zoom_max = ZoomLevel::Max;
	}

	for (ZoomLevel i = zoom_min; i <= zoom_max; i++) {
		memory += sprite[i].width * sprite[i].height;
	}

	/* We have no idea how much memory we really need, so just guess something */
	memory *= 5;

	/* Don't allocate memory each time, but just keep some
	 * memory around as this function is called quite often
	 * and the memory usage is quite low. */
	static ReusableBuffer<uint8_t> temp_buffer;
	SpriteData *temp_dst = reinterpret_cast<SpriteData *>(temp_buffer.ZeroAllocate(memory));
	uint8_t *dst = temp_dst->data;

	/* Make the sprites per zoom-level */
	for (ZoomLevel i = zoom_min; i <= zoom_max; i++) {
		const SpriteLoader::Sprite &src_orig = sprite[i];
		/* Store the index table */
		uint offset = dst - temp_dst->data;
		temp_dst->offset[i] = offset;

		/* cache values, because compiler can't cache it */
		int scaled_height = src_orig.height;
		int scaled_width = src_orig.width;

		for (int y = 0; y < scaled_height; y++) {
			uint trans = 0;
			uint pixels = 0;
			uint last_colour = 0;
			uint8_t *count_dst = nullptr;

			/* Store the scaled image */
			const SpriteLoader::CommonPixel *src = &src_orig.data[y * src_orig.width];

			for (int x = 0; x < scaled_width; x++) {
				uint colour = src++->m;

				if (last_colour == 0 || colour == 0 || pixels == 255) {
					if (count_dst != nullptr) {
						/* Write how many non-transparent bytes we get */
						*count_dst = pixels;
						pixels = 0;
						count_dst = nullptr;
					}
					/* As long as we find transparency bytes, keep counting */
					if (colour == 0 && trans != 255) {
						last_colour = 0;
						trans++;
						continue;
					}
					/* No longer transparency, so write the amount of transparent bytes */
					*dst = trans;
					dst++;
					trans = 0;
					/* Reserve a byte for the pixel counter */
					count_dst = dst;
					dst++;
				}
				last_colour = colour;
				if (colour == 0) {
					trans++;
				} else {
					pixels++;
					*dst = colour;
					dst++;
				}
			}

			if (count_dst != nullptr) *count_dst = pixels;

			/* Write line-ending */
			*dst = 0; dst++;
			*dst = 0; dst++;
		}
	}

	uint size = dst - (uint8_t *)temp_dst;

	/* Safety check, to make sure we guessed the size correctly */
	assert(size < memory);

	/* Allocate the exact amount of memory we need */
	Sprite *dest_sprite = allocator.Allocate<Sprite>(sizeof(*dest_sprite) + size);

	const auto &root_sprite = sprite.Root();
	dest_sprite->height = root_sprite.height;
	dest_sprite->width = root_sprite.width;
	dest_sprite->x_offs = root_sprite.x_offs;
	dest_sprite->y_offs = root_sprite.y_offs;
	std::copy_n(reinterpret_cast<std::byte *>(temp_dst), size, dest_sprite->data);

	return dest_sprite;
}
