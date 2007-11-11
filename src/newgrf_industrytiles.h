/* $Id$ */

/** @file newgrf_industrytiles.h */

#ifndef NEWGRF_INDUSTRYTILES_H
#define NEWGRF_INDUSTRYTILES_H

enum IndustryAnimationTrigger {
	IAT_CONSTRUCTION_STATE_CHANGE,
	IAT_TILELOOP,
	IAT_INDUSTRY_TICK,
	IAT_INDUSTRY_RECEIVED_CARGO,
	IAT_INDUSTRY_DISTRIBUTES_CARGO,
};

bool DrawNewIndustryTile(TileInfo *ti, Industry *i, IndustryGfx gfx, const IndustryTileSpec *inds);
uint16 GetIndustryTileCallback(CallbackID callback, uint32 param1, uint32 param2, IndustryGfx gfx_id, Industry *industry, TileIndex tile);
bool PerformIndustryTileSlopeCheck(TileIndex ind_base_tile, TileIndex ind_tile, const IndustryTileSpec *its, IndustryType type, IndustryGfx gfx, uint itspec_index);

void AnimateNewIndustryTile(TileIndex tile);
bool StartStopIndustryTileAnimation(TileIndex tile, IndustryAnimationTrigger iat, uint32 random = Random());
bool StartStopIndustryTileAnimation(const Industry *ind, IndustryAnimationTrigger iat);


enum IndustryTileTrigger {
	/* The tile of the industry has been triggered during the tileloop. */
	INDTILE_TRIGGER_TILE_LOOP       = 0x01,
	/* The industry has been triggered via it's tick. */
	INDUSTRY_TRIGGER_INDUSTRY_TICK  = 0x02,
	/* Cargo has been delivered. */
	INDUSTRY_TRIGGER_RECEIVED_CARGO = 0x04,
};
void TriggerIndustryTile(TileIndex t, IndustryTileTrigger trigger);
void TriggerIndustry(Industry *ind, IndustryTileTrigger trigger);

#endif /* NEWGRF_INDUSTRYTILES_H */
