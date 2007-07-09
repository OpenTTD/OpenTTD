/* $Id$ */

/** @file newgrf_industrytiles.h */

#ifndef NEWGRF_INDUSTRYTILES_H
#define NEWGRF_INDUSTRYTILES_H

bool DrawNewIndustryTile(TileInfo *ti, Industry *i, IndustryGfx gfx, const IndustryTileSpec *inds);
uint16 GetIndustryTileCallback(uint16 callback, uint32 param1, uint32 param2, IndustryGfx gfx_id, Industry *industry, TileIndex tile);
bool PerformIndustryTileSlopeCheck(TileIndex tile, const IndustryTileSpec *its, IndustryGfx gfx);

#endif /* NEWGRF_INDUSTRYTILES_H */
