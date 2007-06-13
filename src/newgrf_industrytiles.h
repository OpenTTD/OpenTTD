/* $Id$ */

/** @file newgrf_industrytiles.h */

#ifndef NEWGRF_INDUSTRYTILES_H
#define NEWGRF_INDUSTRYTILES_H

void DrawNewIndustryTile(TileInfo *ti, IndustryGfx gfx);
uint16 GetIndustryTileCallback(uint16 callback, uint32 param1, uint32 param2, IndustryGfx gfx_id, Industry *industry, TileIndex tile);

#endif /* NEWGRF_INDUSTRYTILES_H */
