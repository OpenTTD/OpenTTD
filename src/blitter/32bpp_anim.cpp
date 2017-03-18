/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_anim.cpp Implementation of the optimized 32 bpp blitter with animation support. */

#include "../stdafx.h"
#include "../video/video_driver.hpp"
#include "32bpp_anim.hpp"

#include "../table/sprites.h"

#include "../safeguards.h"

/** Instantiation of the 32bpp with animation blitter factory. */
static FBlitter_32bppAnim iFBlitter_32bppAnim;

Blitter_32bppAnim::~Blitter_32bppAnim()
{
	free(this->anim_buf);
}

template <BlitterMode mode>
inline void Blitter_32bppAnim::Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom)
{
	const SpriteData *src = (const SpriteData *)bp->sprite;

	const Colour *src_px = (const Colour *)(src->data + src->offset[zoom][0]);
	const uint16 *src_n  = (const uint16 *)(src->data + src->offset[zoom][1]);

	for (uint i = bp->skip_top; i != 0; i--) {
		src_px = (const Colour *)((const byte *)src_px + *(const uint32 *)src_px);
		src_n  = (const uint16 *)((const byte *)src_n  + *(const uint32 *)src_n);
	}

	Colour *dst = (Colour *)bp->dst + bp->top * bp->pitch + bp->left;
	assert(_screen.pitch == this->anim_buf_pitch); // precondition for translating 'bp->dst' into an 'anim_buf' offset below.
	uint16 *anim = this->anim_buf + ((uint32 *)bp->dst - (uint32 *)_screen.dst_ptr) + bp->top * this->anim_buf_pitch + bp->left;

	const byte *remap = bp->remap; // store so we don't have to access it via bp everytime

	for (int y = 0; y < bp->height; y++) {
		Colour *dst_ln = dst + bp->pitch;
		uint16 *anim_ln = anim + this->anim_buf_pitch;

		const Colour *src_px_ln = (const Colour *)((const byte *)src_px + *(const uint32 *)src_px);
		src_px++;

		const uint16 *src_n_ln = (const uint16 *)((const byte *)src_n + *(const uint32 *)src_n);
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

					n = min<uint>(n - d, (uint)bp->width);
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
			n = min<uint>(*src_n++, (uint)(dst_end - dst));

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
								uint8 g = MakeDark(src_px->r, src_px->g, src_px->b);
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
									uint8 g = MakeDark(src_px->r, src_px->g, src_px->b);
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
					/* TODO -- We make an assumption here that the remap in fact is transparency, not some colour.
					 *  This is never a problem with the code we produce, but newgrfs can make it fail... or at least:
					 *  we produce a result the newgrf maker didn't expect ;) */

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
	assert(_screen.pitch == this->anim_buf_pitch); // precondition for translating 'dst' into an 'anim_buf' offset below.
	uint16 *anim = this->anim_buf + ((uint32 *)dst - (uint32 *)_screen.dst_ptr);

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

	DEBUG(misc, 0, "32bpp blitter doesn't know how to draw this colour table ('%d')", pal);
}

void Blitter_32bppAnim::SetPixel(void *video, int x, int y, uint8 colour)
{
	*((Colour *)video + x + y * _screen.pitch) = LookupColourInPalette(colour);

	/* Set the colour in the anim-buffer too, if we are rendering to the screen */
	if (_screen_disable_anim) return;
	assert(_screen.pitch == this->anim_buf_pitch); // precondition for translating 'video' into an 'anim_buf' offset below.
	this->anim_buf[((uint32 *)video - (uint32 *)_screen.dst_ptr) + x + y * this->anim_buf_pitch] = colour | (DEFAULT_BRIGHTNESS << 8);
}

void Blitter_32bppAnim::DrawRect(void *video, int width, int height, uint8 colour)
{
	if (_screen_disable_anim) {
		/* This means our output is not to the screen, so we can't be doing any animation stuff, so use our parent DrawRect() */
		Blitter_32bppOptimized::DrawRect(video, width, height, colour);
		return;
	}

	Colour colour32 = LookupColourInPalette(colour);
	assert(_screen.pitch == this->anim_buf_pitch); // precondition for translating 'video' into an 'anim_buf' offset below.
	uint16 *anim_line = ((uint32 *)video - (uint32 *)_screen.dst_ptr) + this->anim_buf;

	do {
		Colour *dst = (Colour *)video;
		uint16 *anim = anim_line;

		for (int i = width; i > 0; i--) {
			*dst = colour32;
			/* Set the colour in the anim-buffer too */
			*anim = colour | (DEFAULT_BRIGHTNESS << 8);
			dst++;
			anim++;
		}
		video = (uint32 *)video + _screen.pitch;
		anim_line += this->anim_buf_pitch;
	} while (--height);
}

void Blitter_32bppAnim::CopyFromBuffer(void *video, const void *src, int width, int height)
{
	assert(!_screen_disable_anim);
	assert(video >= _screen.dst_ptr && video <= (uint32 *)_screen.dst_ptr + _screen.width + _screen.height * _screen.pitch);
	Colour *dst = (Colour *)video;
	const uint32 *usrc = (const uint32 *)src;
	assert(_screen.pitch == this->anim_buf_pitch); // precondition for translating 'video' into an 'anim_buf' offset below.
	uint16 *anim_line = ((uint32 *)video - (uint32 *)_screen.dst_ptr) + this->anim_buf;

	for (; height > 0; height--) {
		/* We need to keep those for palette animation. */
		Colour *dst_pal = dst;
		uint16 *anim_pal = anim_line;

		memcpy(dst, usrc, width * sizeof(uint32));
		usrc += width;
		dst += _screen.pitch;
		/* Copy back the anim-buffer */
		memcpy(anim_line, usrc, width * sizeof(uint16));
		usrc = (const uint32 *)((const uint16 *)usrc + width);
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
	assert(video >= _screen.dst_ptr && video <= (uint32 *)_screen.dst_ptr + _screen.width + _screen.height * _screen.pitch);
	uint32 *udst = (uint32 *)dst;
	const uint32 *src = (const uint32 *)video;

	if (this->anim_buf == NULL) return;

	assert(_screen.pitch == this->anim_buf_pitch); // precondition for translating 'video' into an 'anim_buf' offset below.
	const uint16 *anim_line = ((const uint32 *)video - (uint32 *)_screen.dst_ptr) + this->anim_buf;

	for (; height > 0; height--) {
		memcpy(udst, src, width * sizeof(uint32));
		src += _screen.pitch;
		udst += width;
		/* Copy the anim-buffer */
		memcpy(udst, anim_line, width * sizeof(uint16));
		udst = (uint32 *)((uint16 *)udst + width);
		anim_line += this->anim_buf_pitch;
	}
}

void Blitter_32bppAnim::ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y)
{
	assert(!_screen_disable_anim);
	assert(video >= _screen.dst_ptr && video <= (uint32 *)_screen.dst_ptr + _screen.width + _screen.height * _screen.pitch);
	uint16 *dst, *src;

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
			memcpy(dst, src, tw * sizeof(uint16));
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
			memmove(dst, src, tw * sizeof(uint16));
			src += this->anim_buf_pitch;
			dst += this->anim_buf_pitch;
		}
	}

	Blitter_32bppBase::ScrollBuffer(video, left, top, width, height, scroll_x, scroll_y);
}

int Blitter_32bppAnim::BufferSize(int width, int height)
{
	return width * height * (sizeof(uint32) + sizeof(uint16));
}

void Blitter_32bppAnim::PaletteAnimate(const Palette &palette)
{
	assert(!_screen_disable_anim);

	this->palette = palette;
	/* If first_dirty is 0, it is for 8bpp indication to send the new
	 *  palette. However, only the animation colours might possibly change.
	 *  Especially when going between toyland and non-toyland. */
	assert(this->palette.first_dirty == PALETTE_ANIM_START || this->palette.first_dirty == 0);

	const uint16 *anim = this->anim_buf;
	Colour *dst = (Colour *)_screen.dst_ptr;

	/* Let's walk the anim buffer and try to find the pixels */
	for (int y = this->anim_buf_height; y != 0 ; y--) {
		for (int x = this->anim_buf_width; x != 0 ; x--) {
			uint colour = GB(*anim, 0, 8);
			if (colour >= PALETTE_ANIM_START) {
				/* Update this pixel */
				*dst = this->AdjustBrightness(LookupColourInPalette(colour), GB(*anim, 8, 8));
			}
			dst++;
			anim++;
		}
		dst += _screen.pitch - this->anim_buf_width;
		anim += this->anim_buf_pitch - this->anim_buf_width;
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
		free(this->anim_buf);
		this->anim_buf_width = _screen.width;
		this->anim_buf_height = _screen.height;
		this->anim_buf_pitch = _screen.pitch;
		this->anim_buf = CallocT<uint16>(this->anim_buf_height * this->anim_buf_pitch);
	}
}
