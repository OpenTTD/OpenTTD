/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 40bpp_optimized.hpp Optimized 40 bpp blitter. */

#ifndef BLITTER_40BPP_OPTIMIZED_HPP
#define BLITTER_40BPP_OPTIMIZED_HPP


#include "32bpp_optimized.hpp"
#include "../video/video_driver.hpp"

/** The optimized 40 bpp blitter (for OpenGL video driver). */
class Blitter_40bppAnim : public Blitter_32bppOptimized {
public:

	void SetPixel(void *video, int x, int y, uint8_t colour) override;
	void DrawRect(void *video, int width, int height, uint8_t colour) override;
	void DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8_t colour, int width, int dash) override;
	void CopyFromBuffer(void *video, const void *src, int width, int height) override;
	void CopyToBuffer(const void *video, void *dst, int width, int height) override;
	void CopyImageToBuffer(const void *video, void *dst, int width, int height, int dst_pitch) override;
	void ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y) override;
	void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom) override;
	void DrawColourMappingRect(void *dst, int width, int height, PaletteID pal) override;
	Sprite *Encode(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator) override;
	size_t BufferSize(uint width, uint height) override;
	Blitter::PaletteAnimation UsePaletteAnimation() override;
	bool NeedsAnimationBuffer() override;

	const char *GetName()  override { return "40bpp-anim"; }

	template <BlitterMode mode> void Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom);

protected:
	static inline Colour RealizeBlendedColour(uint8_t anim, Colour c)
	{
		return anim != 0 ? AdjustBrightness(LookupColourInPalette(anim), GetColourBrightness(c)) : c;
	}

};

/** Factory for the 40 bpp animated blitter (for OpenGL). */
class FBlitter_40bppAnim : public BlitterFactory {
protected:
	bool IsUsable() const override
	{
		return VideoDriver::GetInstance() == nullptr || VideoDriver::GetInstance()->HasAnimBuffer();
	}

public:
	FBlitter_40bppAnim() : BlitterFactory("40bpp-anim", "40bpp Animation Blitter (OpenGL)") {}
	Blitter *CreateInstance() override { return new Blitter_40bppAnim(); }
};

#endif /* BLITTER_40BPP_OPTIMIZED_HPP */
