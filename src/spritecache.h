/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file spritecache.h Functions to cache sprites in memory. */

#ifndef SPRITECACHE_H
#define SPRITECACHE_H

#include "gfx_type.h"
#include "spriteloader/spriteloader.hpp"

/** Data structure describing a sprite. */
struct Sprite {
	uint16_t height; ///< Height of the sprite.
	uint16_t width;  ///< Width of the sprite.
	int16_t x_offs;  ///< Number of pixels to shift the sprite to the right.
	int16_t y_offs;  ///< Number of pixels to shift the sprite downwards.
	byte data[];   ///< Sprite data.
};

enum SpriteCacheCtrlFlags {
	SCCF_ALLOW_ZOOM_MIN_1X_PAL    = 0, ///< Allow use of sprite min zoom setting at 1x in palette mode.
	SCCF_ALLOW_ZOOM_MIN_1X_32BPP  = 1, ///< Allow use of sprite min zoom setting at 1x in 32bpp mode.
	SCCF_ALLOW_ZOOM_MIN_2X_PAL    = 2, ///< Allow use of sprite min zoom setting at 2x in palette mode.
	SCCF_ALLOW_ZOOM_MIN_2X_32BPP  = 3, ///< Allow use of sprite min zoom setting at 2x in 32bpp mode.
};

extern uint _sprite_cache_size;

typedef void *AllocatorProc(size_t size);

void *SimpleSpriteAlloc(size_t size);
void *GetRawSprite(SpriteID sprite, SpriteType type, AllocatorProc *allocator = nullptr, SpriteEncoder *encoder = nullptr);
bool SpriteExists(SpriteID sprite);

SpriteType GetSpriteType(SpriteID sprite);
SpriteFile *GetOriginFile(SpriteID sprite);
uint32_t GetSpriteLocalID(SpriteID sprite);
uint GetSpriteCountForFile(const std::string &filename, SpriteID begin, SpriteID end);
uint GetMaxSpriteID();


static inline const Sprite *GetSprite(SpriteID sprite, SpriteType type)
{
	assert(type != SpriteType::Recolour);
	return (Sprite*)GetRawSprite(sprite, type);
}

static inline const byte *GetNonSprite(SpriteID sprite, SpriteType type)
{
	assert(type == SpriteType::Recolour);
	return (byte*)GetRawSprite(sprite, type);
}

void GfxInitSpriteMem();
void GfxClearSpriteCache();
void GfxClearFontSpriteCache();
void IncreaseSpriteLRU();

SpriteFile &OpenCachedSpriteFile(const std::string &filename, Subdirectory subdir, bool palette_remap);

void ReadGRFSpriteOffsets(SpriteFile &file);
size_t GetGRFSpriteOffset(uint32_t id);
bool LoadNextSprite(int load_index, SpriteFile &file, uint file_sprite_id);
bool SkipSpriteData(SpriteFile &file, byte type, uint16_t num);
void DupSprite(SpriteID old_spr, SpriteID new_spr);

#endif /* SPRITECACHE_H */
