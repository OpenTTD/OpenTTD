/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file 32bpp_optimized.cpp Implementation of the optimized 32 bpp blitter. */

#include "../stdafx.h"
#include "../zoom_func.h"
#include "../settings_type.h"
#include "../palette_func.h"
#include "32bpp_optimized.hpp"

#include "../safeguards.h"

/** Instantiation of the optimized 32bpp blitter factory. */
static FBlitter_32bppOptimized iFBlitter_32bppOptimized;

/**
 * Draws a sprite to a (screen) buffer. It is templated to allow faster operation.
 *
 * @tparam mode blitter mode
 * @param bp further blitting parameters
 * @param zoom zoom level at which we are drawing
 */
template <BlitterMode mode, bool Tpal_to_rgb>
inline void Blitter_32bppOptimized::Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom)
{
	const SpriteData *src = (const SpriteData *)bp->sprite;

	/* src_px : each line begins with uint32_t n = 'number of bytes in this line',
	 *          then n times is the Colour struct for this line */
	const Colour *src_px = reinterpret_cast<const Colour *>(src->data + src->offset[0][zoom]);
	/* src_n  : each line begins with uint32_t n = 'number of bytes in this line',
	 *          then interleaved stream of 'm' and 'n' channels. 'm' is remap,
	 *          'n' is number of bytes with the same alpha channel class */
	const uint16_t *src_n = reinterpret_cast<const uint16_t *>(src->data + src->offset[1][zoom]);

	/* skip upper lines in src_px and src_n */
	for (uint i = bp->skip_top; i != 0; i--) {
		src_px = (const Colour *)((const uint8_t *)src_px + *(const uint32_t *)src_px);
		src_n = (const uint16_t *)((const uint8_t *)src_n + *(const uint32_t *)src_n);
	}

	/* skip lines in dst */
	Colour *dst = (Colour *)bp->dst + bp->top * bp->pitch + bp->left;

	/* store so we don't have to access it via bp every time (compiler assumes pointer aliasing) */
	const uint8_t *remap = bp->remap;

	for (int y = 0; y < bp->height; y++) {
		/* next dst line begins here */
		Colour *dst_ln = dst + bp->pitch;

		/* next src line begins here */
		const Colour *src_px_ln = (const Colour *)((const uint8_t *)src_px + *(const uint32_t *)src_px);
		src_px++;

		/* next src_n line begins here */
		const uint16_t *src_n_ln = (const uint16_t *)((const uint8_t *)src_n + *(const uint32_t *)src_n);
		src_n += 2;

		/* we will end this line when we reach this point */
		Colour *dst_end = dst + bp->skip_left;

		/* number of pixels with the same alpha channel class */
		uint n;

		while (dst < dst_end) {
			n = *src_n++;

			if (src_px->a == 0) {
				dst += n;
				src_px ++;
				src_n++;
			} else {
				if (dst + n > dst_end) {
					uint d = dst_end - dst;
					src_px += d;
					src_n += d;

					dst = dst_end - bp->skip_left;
					dst_end = dst + bp->width;

					n = std::min(n - d, (uint)bp->width);
					goto draw;
				}
				dst += n;
				src_px += n;
				src_n += n;
			}
		}

		dst -= bp->skip_left;
		dst_end -= bp->skip_left;

		dst_end += bp->width;

		while (dst < dst_end) {
			n = std::min<uint>(*src_n++, dst_end - dst);

			if (src_px->a == 0) {
				dst += n;
				src_px++;
				src_n++;
				continue;
			}

			draw:;

			switch (mode) {
				case BlitterMode::ColourRemap:
					if (src_px->a == 255) {
						do {
							uint m = *src_n;
							/* In case the m-channel is zero, do not remap this pixel in any way */
							if (m == 0) {
								*dst = src_px->data;
							} else {
								uint r = remap[GB(m, 0, 8)];
								if (r != 0) *dst = AdjustBrightness(this->LookupColourInPalette(r), GB(m, 8, 8));
							}
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					} else {
						do {
							uint m = *src_n;
							if (m == 0) {
								*dst = ComposeColourRGBANoCheck(src_px->r, src_px->g, src_px->b, src_px->a, *dst);
							} else {
								uint r = remap[GB(m, 0, 8)];
								if (r != 0) *dst = ComposeColourPANoCheck(AdjustBrightness(this->LookupColourInPalette(r), GB(m, 8, 8)), src_px->a, *dst);
							}
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					}
					break;

				case BlitterMode::CrashRemap:
					if (src_px->a == 255) {
						do {
							uint m = *src_n;
							if (m == 0) {
								uint8_t g = MakeDark(src_px->r, src_px->g, src_px->b);
								*dst = ComposeColourRGBA(g, g, g, src_px->a, *dst);
							} else {
								uint r = remap[GB(m, 0, 8)];
								if (r != 0) *dst = AdjustBrightness(this->LookupColourInPalette(r), GB(m, 8, 8));
							}
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					} else {
						do {
							uint m = *src_n;
							if (m == 0) {
								if (src_px->a != 0) {
									uint8_t g = MakeDark(src_px->r, src_px->g, src_px->b);
									*dst = ComposeColourRGBA(g, g, g, src_px->a, *dst);
								}
							} else {
								uint r = remap[GB(m, 0, 8)];
								if (r != 0) *dst = ComposeColourPANoCheck(AdjustBrightness(this->LookupColourInPalette(r), GB(m, 8, 8)), src_px->a, *dst);
							}
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					}
					break;

				case BlitterMode::BlackRemap:
					do {
						*dst = Colour(0, 0, 0);
						dst++;
						src_px++;
						src_n++;
					} while (--n != 0);
					break;

				case BlitterMode::Transparent:
					/* Make the current colour a bit more black, so it looks like this image is transparent */
					src_n += n;
					if (src_px->a == 255) {
						src_px += n;
						do {
							*dst = MakeTransparent(*dst, 3, 4);
							dst++;
						} while (--n != 0);
					} else {
						do {
							*dst = MakeTransparent(*dst, (256 * 4 - src_px->a), 256 * 4);
							dst++;
							src_px++;
						} while (--n != 0);
					}
					break;

				case BlitterMode::TransparentRemap:
					/* Apply custom transparency remap. */
					src_n += n;
					if (src_px->a != 0) {
						src_px += n;
						do {
							*dst = this->LookupColourInPalette(remap[GetNearestColourIndex(*dst)]);
							dst++;
						} while (--n != 0);
					} else {
						dst += n;
						src_px += n;
					}
					break;

				default:
					if (src_px->a == 255) {
						/* faster than memcpy(), n is usually low */
						do {
							if (Tpal_to_rgb && *src_n != 0) {
								/* Convert the mapping channel to a RGB value */
								*dst = AdjustBrightness(this->LookupColourInPalette(GB(*src_n, 0, 8)), GB(*src_n, 8, 8)).data;
							} else {
								*dst = src_px->data;
							}
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					} else {
						do {
							if (Tpal_to_rgb && *src_n != 0) {
								/* Convert the mapping channel to a RGB value */
								Colour colour = AdjustBrightness(this->LookupColourInPalette(GB(*src_n, 0, 8)), GB(*src_n, 8, 8));
								*dst = ComposeColourRGBANoCheck(colour.r, colour.g, colour.b, src_px->a, *dst);
							} else {
								*dst = ComposeColourRGBANoCheck(src_px->r, src_px->g, src_px->b, src_px->a, *dst);
							}
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					}
					break;
			}
		}

		dst = dst_ln;
		src_px = src_px_ln;
		src_n  = src_n_ln;
	}
}

template <bool Tpal_to_rgb>
void Blitter_32bppOptimized::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
	switch (mode) {
		default: NOT_REACHED();
		case BlitterMode::Normal: Draw<BlitterMode::Normal, Tpal_to_rgb>(bp, zoom); return;
		case BlitterMode::ColourRemap: Draw<BlitterMode::ColourRemap, Tpal_to_rgb>(bp, zoom); return;
		case BlitterMode::Transparent: Draw<BlitterMode::Transparent, Tpal_to_rgb>(bp, zoom); return;
		case BlitterMode::TransparentRemap: Draw<BlitterMode::TransparentRemap, Tpal_to_rgb>(bp, zoom); return;
		case BlitterMode::CrashRemap: Draw<BlitterMode::CrashRemap, Tpal_to_rgb>(bp, zoom); return;
		case BlitterMode::BlackRemap: Draw<BlitterMode::BlackRemap, Tpal_to_rgb>(bp, zoom); return;
	}
}

template void Blitter_32bppOptimized::Draw<true>(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
template void Blitter_32bppOptimized::Draw<false>(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);

/**
 * Draws a sprite to a (screen) buffer. Calls adequate templated function.
 *
 * @param bp further blitting parameters
 * @param mode blitter mode
 * @param zoom zoom level at which we are drawing
 */
void Blitter_32bppOptimized::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
	this->Draw<false>(bp, mode, zoom);
}

template <bool Tpal_to_rgb>
Sprite *Blitter_32bppOptimized::EncodeInternal(SpriteType sprite_type, const SpriteLoader::SpriteCollection &sprite, SpriteAllocator &allocator)
{
	/* streams of pixels (a, r, g, b channels)
	 *
	 * stored in separated stream so data are always aligned on 4B boundary */
	SpriteCollMap<std::unique_ptr<Colour[]>> dst_px_orig;

	/* interleaved stream of 'm' channel and 'n' channel
	 * 'n' is number of following pixels with the same alpha channel class
	 * there are 3 classes: 0, 255, others
	 *
	 * it has to be stored in one stream so fewer registers are used -
	 * x86 has problems with register allocation even with this solution */
	SpriteCollMap<std::unique_ptr<uint16_t[]>> dst_n_orig;

	/* lengths of streams */
	SpriteCollMap<uint32_t> lengths[2];

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

	for (ZoomLevel z = zoom_min; z <= zoom_max; z++) {
		const SpriteLoader::Sprite *src_orig = &sprite[z];

		uint size = src_orig->height * src_orig->width;

		dst_px_orig[z] = std::make_unique<Colour[]>(size + src_orig->height * 2);
		dst_n_orig[z]  = std::make_unique<uint16_t[]>(size * 2 + src_orig->height * 4 * 2);

		uint32_t *dst_px_ln = reinterpret_cast<uint32_t *>(dst_px_orig[z].get());
		uint32_t *dst_n_ln  = reinterpret_cast<uint32_t *>(dst_n_orig[z].get());

		const SpriteLoader::CommonPixel *src = (const SpriteLoader::CommonPixel *)src_orig->data;

		for (uint y = src_orig->height; y > 0; y--) {
			/* Index 0 of dst_px and dst_n is left as space to save the length of the row to be filled later. */
			Colour *dst_px = (Colour *)&dst_px_ln[1];
			uint16_t *dst_n = (uint16_t *)&dst_n_ln[1];

			uint16_t *dst_len = dst_n++;

			uint last = 3;
			int len = 0;

			for (uint x = src_orig->width; x > 0; x--) {
				uint8_t a = src->a;
				uint t = a > 0 && a < 255 ? 1 : a;

				if (last != t || len == 65535) {
					if (last != 3) {
						*dst_len = len;
						dst_len = dst_n++;
					}
					len = 0;
				}

				last = t;
				len++;

				if (a != 0) {
					dst_px->a = a;
					*dst_n = src->m;
					if (src->m != 0) {
						/* Get brightest value */
						uint8_t rgb_max = std::max({ src->r, src->g, src->b });

						/* Black pixel (8bpp or old 32bpp image), so use default value */
						if (rgb_max == 0) rgb_max = DEFAULT_BRIGHTNESS;
						*dst_n |= rgb_max << 8;

						if (Tpal_to_rgb) {
							/* Pre-convert the mapping channel to a RGB value */
							Colour colour = AdjustBrightness(this->LookupColourInPalette(src->m), rgb_max);
							dst_px->r = colour.r;
							dst_px->g = colour.g;
							dst_px->b = colour.b;
						} else {
							dst_px->r = src->r;
							dst_px->g = src->g;
							dst_px->b = src->b;
						}
					} else {
						dst_px->r = src->r;
						dst_px->g = src->g;
						dst_px->b = src->b;
					}
					dst_px++;
					dst_n++;
				} else if (len == 1) {
					dst_px++;
					*dst_n = src->m;
					dst_n++;
				}

				src++;
			}

			if (last != 3) {
				*dst_len = len;
			}

			dst_px = (Colour *)AlignPtr(dst_px, 4);
			dst_n  = (uint16_t *)AlignPtr(dst_n, 4);

			*dst_px_ln = (uint8_t *)dst_px - (uint8_t *)dst_px_ln;
			*dst_n_ln  = (uint8_t *)dst_n  - (uint8_t *)dst_n_ln;

			dst_px_ln = (uint32_t *)dst_px;
			dst_n_ln =  (uint32_t *)dst_n;
		}

		lengths[0][z] = reinterpret_cast<uint8_t *>(dst_px_ln) - reinterpret_cast<uint8_t *>(dst_px_orig[z].get()); // all are aligned to 4B boundary
		lengths[1][z] = reinterpret_cast<uint8_t *>(dst_n_ln) - reinterpret_cast<uint8_t *>(dst_n_orig[z].get());
	}

	uint len = 0; // total length of data
	for (ZoomLevel z = zoom_min; z <= zoom_max; z++) {
		len += lengths[0][z] + lengths[1][z];
	}

	Sprite *dest_sprite = allocator.Allocate<Sprite>(sizeof(*dest_sprite) + sizeof(SpriteData) + len);

	const auto &root_sprite = sprite.Root();
	dest_sprite->height = root_sprite.height;
	dest_sprite->width = root_sprite.width;
	dest_sprite->x_offs = root_sprite.x_offs;
	dest_sprite->y_offs = root_sprite.y_offs;

	SpriteData *dst = (SpriteData *)dest_sprite->data;

	uint32_t offset = 0;
	for (ZoomLevel z = zoom_min; z <= zoom_max; z++) {
		dst->offset[0][z] = offset;
		offset += lengths[0][z];
		dst->offset[1][z] = offset;
		offset += lengths[1][z];

		std::copy_n(reinterpret_cast<uint8_t *>(dst_px_orig[z].get()), lengths[0][z], dst->data + dst->offset[0][z]);
		std::copy_n(reinterpret_cast<uint8_t *>(dst_n_orig[z].get()), lengths[1][z], dst->data + dst->offset[1][z]);
	}

	return dest_sprite;
}

template Sprite *Blitter_32bppOptimized::EncodeInternal<true>(SpriteType sprite_type, const SpriteLoader::SpriteCollection &sprite, SpriteAllocator &allocator);
template Sprite *Blitter_32bppOptimized::EncodeInternal<false>(SpriteType sprite_type, const SpriteLoader::SpriteCollection &sprite, SpriteAllocator &allocator);

Sprite *Blitter_32bppOptimized::Encode(SpriteType sprite_type, const SpriteLoader::SpriteCollection &sprite, SpriteAllocator &allocator)
{
	return this->EncodeInternal<true>(sprite_type, sprite, allocator);
}
