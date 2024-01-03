/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_anim.hpp A 32 bpp blitter with animation support. */

#ifndef BLITTER_32BPP_ANIM_HPP
#define BLITTER_32BPP_ANIM_HPP

#include "32bpp_optimized.hpp"

/** The optimised 32 bpp blitter with palette animation. */
class Blitter_32bppAnim : public Blitter_32bppOptimized {
protected:
	uint16_t *anim_buf;    ///< In this buffer we keep track of the 8bpp indexes so we can do palette animation
	void *anim_alloc;    ///< The raw allocated buffer, not necessarily aligned correctly
	int anim_buf_width;  ///< The width of the animation buffer.
	int anim_buf_height; ///< The height of the animation buffer.
	int anim_buf_pitch;  ///< The pitch of the animation buffer (width rounded up to 16 byte boundary).
	Palette palette;     ///< The current palette.

public:
	Blitter_32bppAnim() :
		anim_buf(nullptr),
		anim_alloc(nullptr),
		anim_buf_width(0),
		anim_buf_height(0),
		anim_buf_pitch(0)
	{
		this->palette = _cur_palette;
	}

	~Blitter_32bppAnim();

	void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom) override;
	void DrawColourMappingRect(void *dst, int width, int height, PaletteID pal) override;
	void SetPixel(void *video, int x, int y, uint8_t colour) override;
	void DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8_t colour, int width, int dash) override;
	void DrawRect(void *video, int width, int height, uint8_t colour) override;
	void CopyFromBuffer(void *video, const void *src, int width, int height) override;
	void CopyToBuffer(const void *video, void *dst, int width, int height) override;
	void ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y) override;
	size_t BufferSize(uint width, uint height) override;
	void PaletteAnimate(const Palette &palette) override;
	Blitter::PaletteAnimation UsePaletteAnimation() override;

	const char *GetName() override { return "32bpp-anim"; }
	void PostResize() override;

	/**
	 * Look up the colour in the current palette.
	 */
	inline Colour LookupColourInPalette(uint index)
	{
		return this->palette.palette[index];
	}

	inline int ScreenToAnimOffset(const uint32_t *video)
	{
		int raw_offset = video - (const uint32_t *)_screen.dst_ptr;
		if (_screen.pitch == this->anim_buf_pitch) return raw_offset;
		int lines = raw_offset / _screen.pitch;
		int across = raw_offset % _screen.pitch;
		return across + (lines * this->anim_buf_pitch);
	}

	template <BlitterMode mode> void Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom);
};

/** Factory for the 32bpp blitter with animation. */
class FBlitter_32bppAnim : public BlitterFactory {
public:
	FBlitter_32bppAnim() : BlitterFactory("32bpp-anim", "32bpp Animation Blitter (palette animation)") {}
	Blitter *CreateInstance() override { return new Blitter_32bppAnim(); }
};

#endif /* BLITTER_32BPP_ANIM_HPP */
