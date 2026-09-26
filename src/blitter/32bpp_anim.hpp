/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file 32bpp_anim.hpp A 32 bpp blitter with animation support. */

#ifndef BLITTER_32BPP_ANIM_HPP
#define BLITTER_32BPP_ANIM_HPP

#include "32bpp_optimized.hpp"

/**
 * Converts video buffer pointer into the animation buffer pointer.
 * @param video The video buffer's pointer.
 * @param animation_buffer The animation buffer.
 * @param animation_buffer_pitch The pitch of the animation buffer.
 * @return The animation buffer pointer.
 */
template <typename T>
static inline T * ScreenToAnimationBuffer(const void *video, T *animation_buffer, int animation_buffer_pitch)
{
	ptrdiff_t raw_offset = static_cast<const uint32_t *>(video) - static_cast<const uint32_t *>(_screen.dst_ptr);
	if (_screen.pitch == animation_buffer_pitch) return &animation_buffer[raw_offset];

	ptrdiff_t lines = raw_offset / _screen.pitch;
	ptrdiff_t across = raw_offset % _screen.pitch;
	return &animation_buffer[across + (lines * animation_buffer_pitch)];
}

/** The optimised 32 bpp blitter with palette animation. */
class Blitter_32bppAnim : public Blitter_32bppOptimized {
protected:
	uint16_t *anim_buf;    ///< In this buffer we keep track of the 8bpp indexes so we can do palette animation
	std::unique_ptr<uint16_t[]> anim_alloc; ///< The raw allocated buffer, not necessarily aligned correctly
	int anim_buf_width;  ///< The width of the animation buffer.
	int anim_buf_height; ///< The height of the animation buffer.
	int anim_buf_pitch;  ///< The pitch of the animation buffer (width rounded up to 16 byte boundary).
	Palette palette;     ///< The current palette.

public:
	Blitter_32bppAnim() :
		anim_buf(nullptr),
		anim_buf_width(0),
		anim_buf_height(0),
		anim_buf_pitch(0)
	{
		this->palette = _cur_palette;
	}

	void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom) override;
	void DrawColourMappingRect(void *dst, int width, int height, PaletteID pal) override;
	void SetPixel(void *video, int x, int y, PixelColour colour) override;
	void DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, PixelColour colour, int width, int dash) override;
	void DrawRect(void *video, int width, int height, PixelColour colour) override;
	void CopyFromBuffer(void *video, const void *src, int width, int height) override;
	void CopyToBuffer(const void *video, void *dst, int width, int height) override;
	void ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y) override;
	size_t BufferSize(uint width, uint height) override;
	void PaletteAnimate(const Palette &palette) override;
	Blitter::PaletteAnimation UsePaletteAnimation() override;

	std::string_view GetName() override { return "32bpp-anim"; }
	void PostResize() override;

	/**
	 * Look up the colour in the current palette.
	 * @param index The index into the palette.
	 * @return The colour.
	 */
	inline Colour LookupColourInPalette(uint index)
	{
		return this->palette.palette[index];
	}

	/**
	 * Converts video buffer pointer into the animation buffer pointer.
	 * @param video The video buffer's pointer.
	 * @return The animation buffer pointer.
	 */
	inline uint16_t *ScreenToAnimationBuffer(const void *video)
	{
		return ::ScreenToAnimationBuffer(video, this->anim_buf, this->anim_buf_pitch);
	}

	template <BlitterMode mode> void Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom);
};

/** Factory for the 32bpp blitter with animation. */
class FBlitter_32bppAnim : public BlitterFactory {
public:
	FBlitter_32bppAnim() : BlitterFactory("32bpp-anim", "32bpp Animation Blitter (palette animation)") {}
	std::unique_ptr<Blitter> CreateInstance() override { return std::make_unique<Blitter_32bppAnim>(); }
};

#endif /* BLITTER_32BPP_ANIM_HPP */
