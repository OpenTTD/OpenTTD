/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_anim.cpp Implementation of the optimized 32 bpp blitter with animation support. */

#include "../stdafx.h"
#include "../video/video_driver.hpp"
#include "../palette_func.h"
#include "32bpp_anim.hpp"
#include "common.hpp"

#include "../table/sprites.h"

#include "../safeguards.h"

/** Instantiation of the 32bpp with animation blitter factory. */
static FBlitter_32bppAnim iFBlitter_32bppAnim;

Blitter_32bppAnim::~Blitter_32bppAnim()
{
	free(this->anim_alloc);
}

template <BlitterMode mode>
inline void Blitter_32bppAnim::Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom)
{
	const SpriteData *src = (const SpriteData *)bp->sprite;

	const Colour *src_px = (const Colour *)(src->data + src->offset[zoom][0]);
	const uint16_t *src_n  = (const uint16_t *)(src->data + src->offset[zoom][1]);

	for (uint i = bp->skip_top; i != 0; i--) {
		src_px = (const Colour *)((const byte *)src_px + *(const uint32_t *)src_px);
		src_n  = (const uint16_t *)((const byte *)src_n  + *(const uint32_t *)src_n);
	}

	Colour *dst = (Colour *)bp->dst + bp->top * bp->pitch + bp->left;
	uint16_t *anim = this->anim_buf + this->ScreenToAnimOffset((uint32_t *)bp->dst) + bp->top * this->anim_buf_pitch + bp->left;

	const byte *remap = bp->remap; // store so we don't have to access it via bp every time

	for (int y = 0; y < bp->height; y++) {
		Colour *dst_ln = dst + bp->pitch;
		uint16_t *anim_ln = anim + this->anim_buf_pitch;

		const Colour *src_px_ln = (const Colour *)((const byte *)src_px + *(const uint32_t *)src_px);
		src_px++;

		const uint16_t *src_n_ln = (const uint16_t *)((const byte *)src_n + *(const uint32_t *)src_n);
		src_n += 2;

		Colour *dst_end = dst + bp->skip_left;

		uint n;

		while (dst < dst_end) {
			n = *src_n++;

			if (src_px->a == 0) {
				dst += n;
				src_px ++;
				src_n++;

				if (dst > dst_end) anim += dst - dst_end;
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
				anim += n;
				dst += n;
				src_px++;
				src_n++;
				continue;
			}

			draw:;

			switch (mode) {
				case BM_COLOUR_REMAP:
					if (src_px->a == 255) {
						do {
							uint m = *src_n;
							/* In case the m-channel is zero, do not remap this pixel in any way */
							if (m == 0) {
								*dst = src_px->data;
								*anim = 0;
							} else {
								uint r = remap[GB(m, 0, 8)];
								*anim = r | (m & 0xFF00);
								if (r != 0) *dst = this->AdjustBrightness(this->LookupColourInPalette(r), GB(m, 8, 8));
							}
							anim++;
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					} else {
						do {
							uint m = *src_n;
							if (m == 0) {
								*dst = ComposeColourRGBANoCheck(src_px->r, src_px->g, src_px->b, src_px->a, *dst);
								*anim = 0;
							} else {
								uint r = remap[GB(m, 0, 8)];
								*anim = 0;
								if (r != 0) *dst = ComposeColourPANoCheck(this->AdjustBrightness(this->LookupColourInPalette(r), GB(m, 8, 8)), src_px->a, *dst);
							}
							anim++;
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					}
					break;

				case BM_CRASH_REMAP:
					if (src_px->a == 255) {
						do {
							uint m = *src_n;
							if (m == 0) {
								uint8_t g = MakeDark(src_px->r, src_px->g, src_px->b);
								*dst = ComposeColourRGBA(g, g, g, src_px->a, *dst);
								*anim = 0;
							} else {
								uint r = remap[GB(m, 0, 8)];
								*anim = r | (m & 0xFF00);
								if (r != 0) *dst = this->AdjustBrightness(this->LookupColourInPalette(r), GB(m, 8, 8));
							}
							anim++;
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
									*anim = 0;
								}
							} else {
								uint r = remap[GB(m, 0, 8)];
								*anim = 0;
								if (r != 0) *dst = ComposeColourPANoCheck(this->AdjustBrightness(this->LookupColourInPalette(r), GB(m, 8, 8)), src_px->a, *dst);
							}
							anim++;
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					}
					break;


				case BM_BLACK_REMAP:
					do {
						*dst++ = Colour(0, 0, 0);
						*anim++ = 0;
						src_px++;
						src_n++;
					} while (--n != 0);
					break;

				case BM_TRANSPARENT:
					/* Make the current colour a bit more black, so it looks like this image is transparent */
					src_n += n;
					if (src_px->a == 255) {
						src_px += n;
						do {
							*dst = MakeTransparent(*dst, 3, 4);
							*anim = 0;
							anim++;
							dst++;
						} while (--n != 0);
					} else {
						do {
							*dst = MakeTransparent(*dst, (256 * 4 - src_px->a), 256 * 4);
							*anim = 0;
							anim++;
							dst++;
							src_px++;
						} while (--n != 0);
					}
					break;

				case BM_TRANSPARENT_REMAP:
					/* Apply custom transparency remap. */
					src_n += n;
					if (src_px->a != 0) {
						src_px += n;
						do {
							*dst = this->LookupColourInPalette(remap[GetNearestColourIndex(*dst)]);
							*anim = 0;
							anim++;
							dst++;
						} while (--n != 0);
					} else {
						dst += n;
						anim += n;
						src_px += n;
					}
					break;

				default:
					if (src_px->a == 255) {
						do {
							/* Compiler assumes pointer aliasing, can't optimise this on its own */
							uint m = GB(*src_n, 0, 8);
							/* Above PALETTE_ANIM_START is palette animation */
							*anim++ = *src_n;
							*dst++ = (m >= PALETTE_ANIM_START) ? this->AdjustBrightness(this->LookupColourInPalette(m), GB(*src_n, 8, 8)) : src_px->data;
							src_px++;
							src_n++;
						} while (--n != 0);
					} else {
						do {
							uint m = GB(*src_n, 0, 8);
							*anim++ = 0;
							if (m >= PALETTE_ANIM_START) {
								*dst = ComposeColourPANoCheck(this->AdjustBrightness(this->LookupColourInPalette(m), GB(*src_n, 8, 8)), src_px->a, *dst);
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

		anim = anim_ln;
		dst = dst_ln;
		src_px = src_px_ln;
		src_n  = src_n_ln;
	}
}

void Blitter_32bppAnim::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
	if (_screen_disable_anim) {
		/* This means our output is not to the screen, so we can't be doing any animation stuff, so use our parent Draw() */
		Blitter_32bppOptimized::Draw(bp, mode, zoom);
		return;
	}

	switch (mode) {
		default: NOT_REACHED();
		case BM_NORMAL:       Draw<BM_NORMAL>      (bp, zoom); return;
		case BM_COLOUR_REMAP: Draw<BM_COLOUR_REMAP>(bp, zoom); return;
		case BM_TRANSPARENT:  Draw<BM_TRANSPARENT> (bp, zoom); return;
		case BM_TRANSPARENT_REMAP: Draw<BM_TRANSPARENT_REMAP>(bp, zoom); return;
		case BM_CRASH_REMAP:  Draw<BM_CRASH_REMAP> (bp, zoom); return;
		case BM_BLACK_REMAP:  Draw<BM_BLACK_REMAP> (bp, zoom); return;
	}
}

void Blitter_32bppAnim::DrawColourMappingRect(void *dst, int width, int height, PaletteID pal)
{
	if (_screen_disable_anim) {
		/* This means our output is not to the screen, so we can't be doing any animation stuff, so use our parent DrawColourMappingRect() */
		Blitter_32bppOptimized::DrawColourMappingRect(dst, width, height, pal);
		return;
	}

	Colour *udst = (Colour *)dst;
	uint16_t *anim = this->anim_buf + this->ScreenToAnimOffset((uint32_t *)dst);

	if (pal == PALETTE_TO_TRANSPARENT) {
		do {
			for (int i = 0; i != width; i++) {
				*udst = MakeTransparent(*udst, 154);
				*anim = 0;
				udst++;
				anim++;
			}
			udst = udst - width + _screen.pitch;
			anim = anim - width + this->anim_buf_pitch;
		} while (--height);
		return;
	}
	if (pal == PALETTE_NEWSPAPER) {
		do {
			for (int i = 0; i != width; i++) {
				*udst = MakeGrey(*udst);
				*anim = 0;
				udst++;
				anim++;
			}
			udst = udst - width + _screen.pitch;
			anim = anim - width + this->anim_buf_pitch;
		} while (--height);
		return;
	}

	Debug(misc, 0, "32bpp blitter doesn't know how to draw this colour table ('{}')", pal);
}

void Blitter_32bppAnim::SetPixel(void *video, int x, int y, uint8_t colour)
{
	*((Colour *)video + x + y * _screen.pitch) = LookupColourInPalette(colour);

	/* Set the colour in the anim-buffer too, if we are rendering to the screen */
	if (_screen_disable_anim) return;

	this->anim_buf[this->ScreenToAnimOffset((uint32_t *)video) + x + y * this->anim_buf_pitch] = colour | (DEFAULT_BRIGHTNESS << 8);
}

void Blitter_32bppAnim::DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8_t colour, int width, int dash)
{
	const Colour c = LookupColourInPalette(colour);

	if (_screen_disable_anim)  {
		this->DrawLineGeneric(x, y, x2, y2, screen_width, screen_height, width, dash, [&](int x, int y) {
			*((Colour *)video + x + y * _screen.pitch) = c;
		});
	} else {
		uint16_t * const offset_anim_buf = this->anim_buf + this->ScreenToAnimOffset((uint32_t *)video);
		const uint16_t anim_colour = colour | (DEFAULT_BRIGHTNESS << 8);
		this->DrawLineGeneric(x, y, x2, y2, screen_width, screen_height, width, dash, [&](int x, int y) {
			*((Colour *)video + x + y * _screen.pitch) = c;
			offset_anim_buf[x + y * this->anim_buf_pitch] = anim_colour;
		});
	}
}

void Blitter_32bppAnim::DrawRect(void *video, int width, int height, uint8_t colour)
{
	if (_screen_disable_anim) {
		/* This means our output is not to the screen, so we can't be doing any animation stuff, so use our parent DrawRect() */
		Blitter_32bppOptimized::DrawRect(video, width, height, colour);
		return;
	}

	Colour colour32 = LookupColourInPalette(colour);
	uint16_t *anim_line = this->ScreenToAnimOffset((uint32_t *)video) + this->anim_buf;

	do {
		Colour *dst = (Colour *)video;
		uint16_t *anim = anim_line;

		for (int i = width; i > 0; i--) {
			*dst = colour32;
			/* Set the colour in the anim-buffer too */
			*anim = colour | (DEFAULT_BRIGHTNESS << 8);
			dst++;
			anim++;
		}
		video = (uint32_t *)video + _screen.pitch;
		anim_line += this->anim_buf_pitch;
	} while (--height);
}

void Blitter_32bppAnim::CopyFromBuffer(void *video, const void *src, int width, int height)
{
	assert(!_screen_disable_anim);
	assert(video >= _screen.dst_ptr && video <= (uint32_t *)_screen.dst_ptr + _screen.width + _screen.height * _screen.pitch);
	Colour *dst = (Colour *)video;
	const uint32_t *usrc = (const uint32_t *)src;
	uint16_t *anim_line = this->ScreenToAnimOffset((uint32_t *)video) + this->anim_buf;

	for (; height > 0; height--) {
		/* We need to keep those for palette animation. */
		Colour *dst_pal = dst;
		uint16_t *anim_pal = anim_line;

		memcpy(static_cast<void *>(dst), usrc, width * sizeof(uint32_t));
		usrc += width;
		dst += _screen.pitch;
		/* Copy back the anim-buffer */
		memcpy(anim_line, usrc, width * sizeof(uint16_t));
		usrc = (const uint32_t *)&((const uint16_t *)usrc)[width];
		anim_line += this->anim_buf_pitch;

		/* Okay, it is *very* likely that the image we stored is using
		 * the wrong palette animated colours. There are two things we
		 * can do to fix this. The first is simply reviewing the whole
		 * screen after we copied the buffer, i.e. run PaletteAnimate,
		 * however that forces a full screen redraw which is expensive
		 * for just the cursor. This just copies the implementation of
		 * palette animation, much cheaper though slightly nastier. */
		for (int i = 0; i < width; i++) {
			uint colour = GB(*anim_pal, 0, 8);
			if (colour >= PALETTE_ANIM_START) {
				/* Update this pixel */
				*dst_pal = this->AdjustBrightness(LookupColourInPalette(colour), GB(*anim_pal, 8, 8));
			}
			dst_pal++;
			anim_pal++;
		}
	}
}

void Blitter_32bppAnim::CopyToBuffer(const void *video, void *dst, int width, int height)
{
	assert(!_screen_disable_anim);
	assert(video >= _screen.dst_ptr && video <= (uint32_t *)_screen.dst_ptr + _screen.width + _screen.height * _screen.pitch);
	uint32_t *udst = (uint32_t *)dst;
	const uint32_t *src = (const uint32_t *)video;

	if (this->anim_buf == nullptr) return;

	const uint16_t *anim_line = this->ScreenToAnimOffset((const uint32_t *)video) + this->anim_buf;

	for (; height > 0; height--) {
		memcpy(udst, src, width * sizeof(uint32_t));
		src += _screen.pitch;
		udst += width;
		/* Copy the anim-buffer */
		memcpy(udst, anim_line, width * sizeof(uint16_t));
		udst = (uint32_t *)&((uint16_t *)udst)[width];
		anim_line += this->anim_buf_pitch;
	}
}

void Blitter_32bppAnim::ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y)
{
	assert(!_screen_disable_anim);
	assert(video >= _screen.dst_ptr && video <= (uint32_t *)_screen.dst_ptr + _screen.width + _screen.height * _screen.pitch);
	uint16_t *dst, *src;

	/* We need to scroll the anim-buffer too */
	if (scroll_y > 0) {
		dst = this->anim_buf + left + (top + height - 1) * this->anim_buf_pitch;
		src = dst - scroll_y * this->anim_buf_pitch;

		/* Adjust left & width */
		if (scroll_x >= 0) {
			dst += scroll_x;
		} else {
			src -= scroll_x;
		}

		uint tw = width + (scroll_x >= 0 ? -scroll_x : scroll_x);
		uint th = height - scroll_y;
		for (; th > 0; th--) {
			memcpy(dst, src, tw * sizeof(uint16_t));
			src -= this->anim_buf_pitch;
			dst -= this->anim_buf_pitch;
		}
	} else {
		/* Calculate pointers */
		dst = this->anim_buf + left + top * this->anim_buf_pitch;
		src = dst - scroll_y * this->anim_buf_pitch;

		/* Adjust left & width */
		if (scroll_x >= 0) {
			dst += scroll_x;
		} else {
			src -= scroll_x;
		}

		/* the y-displacement may be 0 therefore we have to use memmove,
		 * because source and destination may overlap */
		uint tw = width + (scroll_x >= 0 ? -scroll_x : scroll_x);
		uint th = height + scroll_y;
		for (; th > 0; th--) {
			memmove(dst, src, tw * sizeof(uint16_t));
			src += this->anim_buf_pitch;
			dst += this->anim_buf_pitch;
		}
	}

	Blitter_32bppBase::ScrollBuffer(video, left, top, width, height, scroll_x, scroll_y);
}

size_t Blitter_32bppAnim::BufferSize(uint width, uint height)
{
	return (sizeof(uint32_t) + sizeof(uint16_t)) * width * height;
}

void Blitter_32bppAnim::PaletteAnimate(const Palette &palette)
{
	assert(!_screen_disable_anim);

	this->palette = palette;
	/* If first_dirty is 0, it is for 8bpp indication to send the new
	 *  palette. However, only the animation colours might possibly change.
	 *  Especially when going between toyland and non-toyland. */
	assert(this->palette.first_dirty == PALETTE_ANIM_START || this->palette.first_dirty == 0);

	const uint16_t *anim = this->anim_buf;
	Colour *dst = (Colour *)_screen.dst_ptr;

	/* Let's walk the anim buffer and try to find the pixels */
	const int width = this->anim_buf_width;
	const int pitch_offset = _screen.pitch - width;
	const int anim_pitch_offset = this->anim_buf_pitch - width;
	for (int y = this->anim_buf_height; y != 0 ; y--) {
		for (int x = width; x != 0 ; x--) {
			uint16_t value = *anim;
			uint8_t colour = GB(value, 0, 8);
			if (colour >= PALETTE_ANIM_START) {
				/* Update this pixel */
				*dst = this->AdjustBrightness(LookupColourInPalette(colour), GB(value, 8, 8));
			}
			dst++;
			anim++;
		}
		dst += pitch_offset;
		anim += anim_pitch_offset;
	}

	/* Make sure the backend redraws the whole screen */
	VideoDriver::GetInstance()->MakeDirty(0, 0, _screen.width, _screen.height);
}

Blitter::PaletteAnimation Blitter_32bppAnim::UsePaletteAnimation()
{
	return Blitter::PALETTE_ANIMATION_BLITTER;
}

void Blitter_32bppAnim::PostResize()
{
	if (_screen.width != this->anim_buf_width || _screen.height != this->anim_buf_height ||
			_screen.pitch != this->anim_buf_pitch) {
		/* The size of the screen changed; we can assume we can wipe all data from our buffer */
		free(this->anim_alloc);
		this->anim_buf_width = _screen.width;
		this->anim_buf_height = _screen.height;
		this->anim_buf_pitch = (_screen.width + 7) & ~7;
		this->anim_alloc = CallocT<uint16_t>(this->anim_buf_pitch * this->anim_buf_height + 8);

		/* align buffer to next 16 byte boundary */
		this->anim_buf = reinterpret_cast<uint16_t *>((reinterpret_cast<uintptr_t>(this->anim_alloc) + 0xF) & (~0xF));
	}
}
