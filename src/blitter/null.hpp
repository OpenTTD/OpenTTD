/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file null.hpp The blitter that doesn't blit. */

#ifndef BLITTER_NULL_HPP
#define BLITTER_NULL_HPP

#include "factory.hpp"

/** Blitter that does nothing. */
class Blitter_Null : public Blitter {
public:
	uint8_t GetScreenDepth() override { return 0; }
	void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom) override {};
	void DrawColourMappingRect(void *dst, int width, int height, PaletteID pal) override {};
	Sprite *Encode(const SpriteLoader::Sprite *sprite, AllocatorProc *allocator) override;
	void *MoveTo(void *video, int x, int y) override { return nullptr; };
	void SetPixel(void *video, int x, int y, uint8_t colour) override {};
	void DrawRect(void *video, int width, int height, uint8_t colour) override {};
	void DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8_t colour, int width, int dash) override {};
	void CopyFromBuffer(void *video, const void *src, int width, int height) override {};
	void CopyToBuffer(const void *video, void *dst, int width, int height) override {};
	void CopyImageToBuffer(const void *video, void *dst, int width, int height, int dst_pitch) override {};
	void ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y) override {};
	size_t BufferSize(uint width, uint height) override { return 0; };
	void PaletteAnimate(const Palette &palette) override { };
	Blitter::PaletteAnimation UsePaletteAnimation() override { return Blitter::PALETTE_ANIMATION_NONE; };

	const char *GetName() override { return "null"; }
	int GetBytesPerPixel() override { return 0; }
};

/** Factory for the blitter that does nothing. */
class FBlitter_Null : public BlitterFactory {
public:
	FBlitter_Null() : BlitterFactory("null", "Null Blitter (does nothing)") {}
	Blitter *CreateInstance() override { return new Blitter_Null(); }
};

#endif /* BLITTER_NULL_HPP */
