/* $Id$ */

/** @file gfxinit.h */

#ifndef GFXINIT_H
#define GFXINIT_H

void CheckExternalFiles();
void GfxLoadSprites();
void LoadSpritesIndexed(int file_index, uint *sprite_id, const SpriteID *index_tbl);

#endif /* GFXINIT_H */
