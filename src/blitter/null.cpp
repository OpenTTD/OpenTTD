/* $Id$ */

/** @file null.cpp A blitter that doesn't blit. */

#include "../stdafx.h"
#include "null.hpp"

static FBlitter_Null iFBlitter_Null;

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
