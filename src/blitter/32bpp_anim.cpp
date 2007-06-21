#include "../stdafx.h"
#include "../zoom.hpp"
#include "../gfx.h"
#include "../debug.h"
#include "../table/sprites.h"
#include "32bpp_anim.hpp"

static FBlitter_32bppAnim iFBlitter_32bppAnim;

void Blitter_32bppAnim::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
	const SpriteLoader::CommonPixel *src, *src_line;
	uint32 *dst, *dst_line;
	uint8 *anim, *anim_line;

	if (_screen.width != this->anim_buf_width || _screen.height != this->anim_buf_height) {
		/* The size of the screen changed; we can assume we can wipe all data from our buffer */
		free(this->anim_buf);
		this->anim_buf = CallocT<uint8>(_screen.width * _screen.height);
		this->anim_buf_width = _screen.width;
		this->anim_buf_height = _screen.height;
	}

	/* Find where to start reading in the source sprite */
	src_line = (const SpriteLoader::CommonPixel *)bp->sprite + (bp->skip_top * bp->sprite_width + bp->skip_left) * ScaleByZoom(1, zoom);
	dst_line = (uint32 *)bp->dst + bp->top * bp->pitch + bp->left;
	anim_line = this->anim_buf + ((uint32 *)bp->dst - (uint32 *)_screen.dst_ptr) + bp->top * this->anim_buf_width + bp->left;

	for (int y = 0; y < bp->height; y++) {
		dst = dst_line;
		dst_line += bp->pitch;

		src = src_line;
		src_line += bp->sprite_width * ScaleByZoom(1, zoom);

		anim = anim_line;
		anim_line += this->anim_buf_width;

		for (int x = 0; x < bp->width; x++) {
			switch (mode) {
				case BM_COLOUR_REMAP:
					/* In case the m-channel is zero, do not remap this pixel in any way */
					if (src->m == 0) {
						if (src->a != 0) *dst = ComposeColourRGBA(src->r, src->g, src->b, src->a, *dst);
						*anim = 0;
					} else {
						if (bp->remap[src->m] != 0) {
							*dst = ComposeColourPA(this->LookupColourInPalette(bp->remap[src->m]), src->a, *dst);
							*anim = bp->remap[src->m];
						}
					}
					break;

				case BM_TRANSPARENT:
					/* TODO -- We make an assumption here that the remap in fact is transparency, not some color.
					 *  This is never a problem with the code we produce, but newgrfs can make it fail... or at least:
					 *  we produce a result the newgrf maker didn't expect ;) */

					/* Make the current color a bit more black, so it looks like this image is transparent */
					if (src->a != 0) {
						*dst = MakeTransparent(*dst, 75);
						*anim = bp->remap[*anim];
					}
					break;

				default:
					if (src->a != 0) {
						/* Above 217 is palette animation */
						if (src->m >= 217) *dst = ComposeColourPA(this->LookupColourInPalette(src->m), src->a, *dst);
						else               *dst = ComposeColourRGBA(src->r, src->g, src->b, src->a, *dst);
						*anim = src->m;
					}
					break;
			}
			dst++;
			anim++;
			src += ScaleByZoom(1, zoom);
		}
	}
}

void Blitter_32bppAnim::DrawColorMappingRect(void *dst, int width, int height, int pal)
{
	uint32 *udst = (uint32 *)dst;
	uint8 *anim;

	anim = this->anim_buf + ((uint32 *)dst - (uint32 *)_screen.dst_ptr);

	if (pal == PALETTE_TO_TRANSPARENT) {
		do {
			for (int i = 0; i != width; i++) {
				*udst = MakeTransparent(*udst, 60);
				*anim = 0;
				udst++;
				anim++;
			}
			udst = udst - width + _screen.pitch;
			anim = anim - width + this->anim_buf_width;
		} while (--height);
		return;
	}
	if (pal == PALETTE_TO_STRUCT_GREY) {
		do {
			for (int i = 0; i != width; i++) {
				*udst = MakeGrey(*udst);
				*anim = 0;
				udst++;
				anim++;
			}
			udst = udst - width + _screen.pitch;
			anim = anim - width + this->anim_buf_width;
		} while (--height);
		return;
	}

	DEBUG(misc, 0, "32bpp blitter doesn't know how to draw this color table ('%d')", pal);
}

void Blitter_32bppAnim::SetPixel(void *video, int x, int y, uint8 color)
{
	*((uint32 *)video + x + y * _screen.pitch) = LookupColourInPalette(color);
	/* Set the color in the anim-buffer too */
	this->anim_buf[((uint32 *)video - (uint32 *)_screen.dst_ptr) + x + y * this->anim_buf_width] = color;
}

void Blitter_32bppAnim::SetPixelIfEmpty(void *video, int x, int y, uint8 color)
{
	uint32 *dst = (uint32 *)video + x + y * _screen.pitch;
	if (*dst == 0) {
		*dst = LookupColourInPalette(color);
		/* Set the color in the anim-buffer too */
		this->anim_buf[((uint32 *)video - (uint32 *)_screen.dst_ptr) + x + y * this->anim_buf_width] = color;
	}
}

void Blitter_32bppAnim::DrawRect(void *video, int width, int height, uint8 color)
{
	uint32 color32 = LookupColourInPalette(color);
	uint8 *anim_line;

	anim_line = ((uint32 *)video - (uint32 *)_screen.dst_ptr) + this->anim_buf;

	do {
		uint32 *dst = (uint32 *)video;
		uint8 *anim = anim_line;

		for (int i = width; i > 0; i--) {
			*dst = color32;
			/* Set the color in the anim-buffer too */
			*anim = color;
			dst++;
			anim++;
		}
		video = (uint32 *)video + _screen.pitch;
		anim_line += this->anim_buf_width;
	} while (--height);
}

void Blitter_32bppAnim::CopyFromBuffer(void *video, const void *src, int width, int height)
{
	assert(video >= _screen.dst_ptr && video <= (uint32 *)_screen.dst_ptr + _screen.width + _screen.height * _screen.pitch);
	uint32 *dst = (uint32 *)video;
	uint32 *usrc = (uint32 *)src;
	uint8 *anim_line;

	anim_line = ((uint32 *)video - (uint32 *)_screen.dst_ptr) + this->anim_buf;

	for (; height > 0; height--) {
		memcpy(dst, usrc, width * sizeof(uint32));
		usrc += width;
		dst += _screen.pitch;
		/* Copy back the anim-buffer */
		memcpy(anim_line, usrc, width * sizeof(uint8));
		usrc = (uint32 *)((uint8 *)usrc + width);
		anim_line += this->anim_buf_width;
	}

	/* We update the palette (or the pixels that do animation) immediatly, to avoid graphical glitches */
	this->PaletteAnimate(217, _use_dos_palette ? 38 : 28);
}

void Blitter_32bppAnim::CopyToBuffer(const void *video, void *dst, int width, int height)
{
	assert(video >= _screen.dst_ptr && video <= (uint32 *)_screen.dst_ptr + _screen.width + _screen.height * _screen.pitch);
	uint32 *udst = (uint32 *)dst;
	uint32 *src = (uint32 *)video;
	uint8 *anim_line;

	if (this->anim_buf == NULL) return;

	anim_line = ((uint32 *)video - (uint32 *)_screen.dst_ptr) + this->anim_buf;

	for (; height > 0; height--) {
		memcpy(udst, src, width * sizeof(uint32));
		src += _screen.pitch;
		udst += width;
		/* Copy the anim-buffer */
		memcpy(udst, anim_line, width * sizeof(uint8));
		udst = (uint32 *)((uint8 *)udst + width);
		anim_line += this->anim_buf_width;
	}
}

void Blitter_32bppAnim::ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y)
{
	uint8 *dst, *src;

	/* We need to scroll the anim-buffer too */
	if (scroll_y > 0) {
		dst = this->anim_buf + left + (top + height - 1) * this->anim_buf_width;
		src = dst - scroll_y * this->anim_buf_width;

		/* Adjust left & width */
		if (scroll_x >= 0) dst += scroll_x;
		else               src -= scroll_x;

		uint tw = width + (scroll_x >= 0 ? -scroll_x : scroll_x);
		uint th = height - scroll_y;
		for (; th > 0; th--) {
			memcpy(dst, src, tw * sizeof(uint8));
			src -= this->anim_buf_width;
			dst -= this->anim_buf_width;
		}
	} else {
		/* Calculate pointers */
		dst = this->anim_buf + left + top * this->anim_buf_width;
		src = dst - scroll_y * this->anim_buf_width;

		/* Adjust left & width */
		if (scroll_x >= 0) dst += scroll_x;
		else               src -= scroll_x;

		/* the y-displacement may be 0 therefore we have to use memmove,
		 * because source and destination may overlap */
		uint tw = width + (scroll_x >= 0 ? -scroll_x : scroll_x);
		uint th = height + scroll_y;
		for (; th > 0; th--) {
			memmove(dst, src, tw * sizeof(uint8));
			src += this->anim_buf_width;
			dst += this->anim_buf_width;
		}
	}

	Blitter_32bppBase::ScrollBuffer(video, left, top, width, height, scroll_x, scroll_y);
}

void Blitter_32bppAnim::PaletteAnimate(uint start, uint count)
{
	uint8 *anim = this->anim_buf;

	/* Never repaint the transparency pixel */
	if (start == 0) start++;

	/* Let's walk the anim buffer and try to find the pixels */
	for (int y = 0; y < this->anim_buf_height; y++) {
		for (int x = 0; x < this->anim_buf_width; x++) {
			if (*anim >= start && *anim <= start + count) {
				/* Update this pixel */
				this->SetPixel(_screen.dst_ptr, x, y, *anim);
			}
			anim++;
		}
	}

	/* Make sure the backend redraws the whole screen */
	_video_driver->make_dirty(0, 0, _screen.width, _screen.height);
}

Blitter::PaletteAnimation Blitter_32bppAnim::UsePaletteAnimation()
{
	return Blitter::PALETTE_ANIMATION_BLITTER;
}
