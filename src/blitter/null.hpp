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
	void Draw(Blitter::BlitterParams *, BlitterMode, ZoomLevel) override {};
	void DrawColourMappingRect(void *, int, int, PaletteID) override {};
	Sprite *Encode(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator) override;
	void *MoveTo(void *, int, int) override { return nullptr; };
	void SetPixel(void *, int, int, uint8_t) override {};
	void DrawRect(void *, int, int, uint8_t) override {};
	void DrawLine(void *, int, int, int, int, int, int, uint8_t, int, int) override {};
	void CopyFromBuffer(void *, const void *, int, int) override {};
	void CopyToBuffer(const void *, void *, int, int) override {};
	void CopyImageToBuffer(const void *, void *, int, int, int) override {};
	void ScrollBuffer(void *, int &, int &, int &, int &, int, int) override {};
	size_t BufferSize(uint, uint) override { return 0; };
	void PaletteAnimate(const Palette &) override { };
	Blitter::PaletteAnimation UsePaletteAnimation() override { return Blitter::PALETTE_ANIMATION_NONE; };

	const char *GetName() override { return "null"; }
};

/** Factory for the blitter that does nothing. */
class FBlitter_Null : public BlitterFactory {
public:
	FBlitter_Null() : BlitterFactory("null", "Null Blitter (does nothing)") {}
	Blitter *CreateInstance() override { return new Blitter_Null(); }
};

#endif /* BLITTER_NULL_HPP */
