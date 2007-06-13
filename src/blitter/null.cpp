#include "../stdafx.h"
#include "../zoom.hpp"
#include "../gfx.h"
#include "../functions.h"
#include "null.hpp"

static FBlitter_Null iFBlitter_Null;

void Blitter_Null::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
}

void Blitter_Null::DrawColorMappingRect(void *dst, int width, int height, int pal)
{
}

Sprite *Blitter_Null::Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator)
{
	Sprite *dest_sprite;
	dest_sprite = (Sprite *)allocator(sizeof(*dest_sprite));

	dest_sprite->height = sprite->height;
	dest_sprite->width  = sprite->width;
	dest_sprite->x_offs = sprite->x_offs;
	dest_sprite->y_offs = sprite->y_offs;

	return dest_sprite;
}
