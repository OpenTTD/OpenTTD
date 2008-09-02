/* $Id$ */

/** @file spritecache.h Functions to cache sprites in memory. */

#ifndef SPRITECACHE_H
#define SPRITECACHE_H

#include "gfx_type.h"

struct Sprite {
	byte height;
	uint16 width;
	int16 x_offs;
	int16 y_offs;
	byte data[VARARRAY_SIZE];
};

extern uint _sprite_cache_size;

const void *GetRawSprite(SpriteID sprite, SpriteType type);
bool SpriteExists(SpriteID sprite);

static inline const Sprite *GetSprite(SpriteID sprite, SpriteType type)
{
	assert(type != ST_RECOLOUR);
	return (Sprite*)GetRawSprite(sprite, type);
}

static inline const byte *GetNonSprite(SpriteID sprite, SpriteType type)
{
	assert(type == ST_RECOLOUR);
	return (byte*)GetRawSprite(sprite, type);
}

void GfxInitSpriteMem();
void IncreaseSpriteLRU();

bool LoadNextSprite(int load_index, byte file_index, uint file_sprite_id);
void DupSprite(SpriteID old_spr, SpriteID new_spr);

#endif /* SPRITECACHE_H */
