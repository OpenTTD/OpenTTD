/* $Id$ */

#ifndef SPRITECACHE_H
#define SPRITECACHE_H

typedef struct Sprite {
	byte info;
	byte height;
	uint16 width;
	int16 x_offs;
	int16 y_offs;
	byte data[VARARRAY_SIZE];
} Sprite;

typedef struct {
	int xoffs, yoffs;
	int xsize, ysize;
} SpriteDimension;

const SpriteDimension *GetSpriteDimension(SpriteID sprite);
const void *GetRawSprite(SpriteID sprite);

static inline const Sprite *GetSprite(SpriteID sprite)
{
	return GetRawSprite(sprite);
}

static inline const byte *GetNonSprite(SpriteID sprite)
{
	return GetRawSprite(sprite);
}

void GfxLoadSprites(void);
void IncreaseSpriteLRU(void);

#endif
