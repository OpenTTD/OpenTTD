#ifndef SPRITECACHE_H
#define SPRITECACHE_H

typedef struct {
	int xoffs, yoffs;
	int xsize, ysize;
} SpriteDimension;

const SpriteDimension *GetSpriteDimension(SpriteID sprite);
byte *GetSpritePtr(SpriteID sprite);

void GfxInitSpriteMem(byte *ptr, uint32 size);
void GfxLoadSprites(void);
void IncreaseSpriteLRU(void);

#endif
